// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_SLEEP0_BACKOFF=2 (an experiment, off by default; macOS): the Sleep(0)
// back-off (kernel/sleep0_backoff.h) with a predicted wake. A thread whose long
// waits end at a steady period (Carbon's g2: once a frame, about 16 ms each, in
// WaitPlayed) sleeps only until kMarginNs before the predicted end and yields
// from there, so the handoff isn't delayed by a final sleep (about 0.1 ms a frame
// with the fixed back-off, measured on a MacBook Air M5, October 2026).
//
// Built on the fixed policy, never instead of it: its Poll still decides each
// poll first. Its yields (a run's first kSpinNs, the second after a late sleep)
// stay yields, and without a confident prediction every decision is its own.
// The prediction only shortens or skips its sleeps near the predicted end.
//  - Learns only from waits of kLongWaitNs or more (a short periodic wait, such
//    as the render thread's 25-100 us handoff, never becomes confident).
//  - Confident with kMinGood of the last kHistory intervals between long-wait
//    ends within kToleranceNs of their median; an interval that is a whole
//    multiple of it (a dropped frame) counts as that many periods.
//  - A wait's predicted end is fixed when it starts: the first multiple of the
//    period after the last long wait's end that lies past the wait's start. A
//    last end that didn't fit a whole number of periods (a late or odd frame)
//    predicts nothing: the next wait takes the fixed policy, and the first end
//    that fits again re-anchors the phase.
//  - A wait that outlasts its prediction by kLateBudgetNs goes back to the fixed
//    policy's sleeps: at most kSpinNs + kMarginNs + kLateBudgetNs of yielding.
//  - RECARBON_SLEEP0_BACKOFF=3 (`longChunks`): while more than kLongChunkFromNs
//    remain before the predicted end, sleeps are up to kLongChunkNs instead of
//    100 us (still never into the yield window), so a 15 ms wait wakes about 16
//    times instead of about 90. Near the end, unconfident or after a miss: 100 us.
//    An early end can then cost up to a long chunk.
// Known limit: waits are the fixed policy's runs, so two waits less than
// kGapNs apart are one; the second then misses (the fixed policy's delay, no
// worse) and the prediction carries on from the next.
// State is per thread (no shared state). Pure, for tests/sleep0_predict_test.cpp.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <string>

#include "sleep0_backoff.h"

namespace kernel::sleep0predict
{
    constexpr int64_t kLongWaitNs = 2'000'000;
    constexpr int64_t kMarginNs = 1'500'000;     // yield from this long before the predicted end
    constexpr int64_t kLateBudgetNs = 1'000'000; // ... and this long after it, at most
    constexpr int64_t kToleranceNs = 1'000'000;
    constexpr int64_t kMinSleepNs = 20'000;      // a shorter sleep than this: yield instead
    constexpr int kHistory = 8, kMinGood = 6;
    constexpr int64_t kLongChunkNs = 1'000'000;      // =3: a sleep while far from the end
    constexpr int64_t kLongChunkFromNs = 2'000'000;  // ... far: more than this before the predicted end

    // Intervals between the ends of a thread's long waits.
    struct Predictor
    {
        int64_t lastEnd = 0;
        bool anchored = false;  // lastEnd fits the period: a phase to predict from
        std::array<int64_t, kHistory> intervals{};
        int count = 0, next = 0;

        int64_t Median() const
        {
            std::array<int64_t, kHistory> sorted = intervals;
            std::sort(sorted.begin(), sorted.begin() + count);
            return count ? sorted[count / 2] : 0;
        }

        // The period, or 0 when not confident.
        int64_t Period() const
        {
            const int64_t median = Median();
            int good = 0;
            for (int i = 0; i < count; i++)
                good += intervals[i] >= median - kToleranceNs && intervals[i] <= median + kToleranceNs;
            return good >= kMinGood ? median : 0;
        }

        void Push(int64_t interval)
        {
            intervals[next] = interval;
            next = (next + 1) % kHistory;
            count = std::min(count + 1, kHistory);
        }

        void OnWaitEnd(int64_t end, int64_t length)
        {
            if (length < kLongWaitNs)
                return;
            anchored = false;
            if (lastEnd)
            {
                int64_t interval = end - lastEnd;
                const int64_t period = Period();
                if (period)
                {
                    const int64_t k = (interval + period / 2) / period;
                    if (k >= 1 && interval >= k * period - kToleranceNs && interval <= k * period + kToleranceNs)
                    {
                        anchored = true;
                        interval /= k;  // k > 1: dropped frames, k periods
                    }
                }
                Push(interval);
                if (!period && Period())
                    anchored = true;  // the push that made it confident: in phase by definition
            }
            lastEnd = end;
        }

        // The predicted end of a wait starting at `start`, or 0 when not confident.
        int64_t PredictEnd(int64_t start) const
        {
            const int64_t period = Period();
            if (!period || !anchored)  // (a period implies a last end)
                return 0;
            const int64_t k = start > lastEnd ? (start - lastEnd) / period + 1 : 1;
            return lastEnd + k * period;
        }
    };

    // For RECARBON_SLEEP0_TRACE's line: how the predictions went.
    struct Stats
    {
        uint64_t confident = 0;  // waits with a prediction
        uint64_t hits = 0;       // ... that ended while yielding, inside the window
        uint64_t early = 0;      // ... that ended before it (asleep: a sleep's delay at most)
        uint64_t misses = 0;     // ... that outlasted it by kLateBudgetNs
        uint64_t longSleeps = 0; // =3: sleeps longer than 100 us
        int64_t spinSumNs = 0, spinMaxNs = 0;  // hits: yielding before the end

        std::string Report() const
        {
            return std::format("; predicted {} (hit {}, early {}, missed {}; yielded before the end avg {:.0f} us max {:.0f} us; long sleeps {})", confident,
                hits, early, misses, hits ? double(spinSumNs) / double(hits) / 1000.0 : 0.0, double(spinMaxNs) / 1000.0, longSleeps);
        }
    };

    struct State
    {
        sleep0::State fixed;  // the fixed policy's own state, as it would be alone
        Predictor predictor;
        bool started = false;
        int64_t predictedEnd = 0;  // this wait's, 0: none
        bool missed = false;
        Stats stats;
    };

    struct Decision
    {
        sleep0::Action action = sleep0::Action::Yield;
        int64_t sleepNs = 0;
    };

    inline void EndWait(State& s, int64_t end, int64_t length)
    {
        if (s.predictedEnd)
        {
            s.stats.confident++;
            const int64_t spinFrom = s.predictedEnd - kMarginNs;
            if (s.missed)
                s.stats.misses++;
            else if (end < spinFrom)
                s.stats.early++;
            else
            {
                s.stats.hits++;
                s.stats.spinSumNs += end - spinFrom;
                s.stats.spinMaxNs = std::max(s.stats.spinMaxNs, end - spinFrom);
            }
        }
        s.predictor.OnWaitEnd(end, length);
    }

    // A zero delay at `now`. Sleeps for Decision::sleepNs (then sleep0::Slept on
    // s.fixed, as the fixed policy does; with `longChunks`, sleep0::SleptAsked),
    // or yields.
    inline Decision Poll(State& s, int64_t now, bool longChunks = false)
    {
        const int64_t first = s.fixed.firstZero, last = s.fixed.lastZero;
        const sleep0::Action fixed = sleep0::Poll(s.fixed, now);
        if (!s.started || s.fixed.firstZero != first)
        {
            if (s.started)
                EndWait(s, last, last - first);
            s.started = true;
            s.predictedEnd = s.predictor.PredictEnd(now);
            s.missed = false;
        }
        if (fixed == sleep0::Action::Yield)
            return {};
        if (!s.predictedEnd)
            return { sleep0::Action::Sleep, sleep0::kSleepNs };
        if (now > s.predictedEnd + kLateBudgetNs)
        {
            s.missed = true;  // outlasted: the fixed policy's sleeps from here
            return { sleep0::Action::Sleep, sleep0::kSleepNs };
        }
        // Never past the yield window's start; inside it (or under kMinSleepNs from it), yield.
        const int64_t chunk = longChunks && s.predictedEnd - now > kLongChunkFromNs ? kLongChunkNs : sleep0::kSleepNs;
        const int64_t sleepNs = std::min(chunk, s.predictedEnd - kMarginNs - now);
        if (sleepNs < kMinSleepNs)
            return {};
        if (sleepNs > sleep0::kSleepNs)
            s.stats.longSleeps++;
        return { sleep0::Action::Sleep, sleepNs };
    }
}
