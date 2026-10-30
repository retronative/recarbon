// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// kernel/sleep0_predict.h: RECARBON_SLEEP0_BACKOFF=2's predicted wake (an experiment, off by
// default), on a fake clock: a simulated thread waits for a flag that flips at scripted times,
// yielding (2 us each) or sleeping as the policy says (each sleep overrunning by a set amount).
// Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/sleep0_predict_test.cpp -o build/sleep0_predict_test && build/sleep0_predict_test
#include <kernel/sleep0_predict.h>

#include <cstdio>
#include <thread>
#include <vector>

namespace
{
    int g_failures = 0;

    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            printf("  FAIL: %s\n", what);
            g_failures++;
        }
    }

    using namespace kernel;
    using namespace kernel::sleep0predict;
    constexpr int64_t us = 1000, ms = 1000 * us;
    constexpr int64_t kPeriod = 16'667 * us;
    constexpr int64_t kYieldNs = 2 * us;

    struct WaitResult
    {
        int64_t end = 0;        // the poll that saw the flag returned
        int64_t delay = 0;      // end - the flag's time: what the policy added to the handoff
        int64_t yieldedNs = 0;  // time spent yielding
        int sleeps = 0;
        bool sleptPastSpin = false;  // a sleep was asked that ends after the yield window starts
        std::vector<int64_t> decisions;  // 0: yield, else the asked sleep
    };

    // One wait from `start` until the flag flips at `flag`, each sleep overrunning by `overrun`.
    // `fixedOnly`: the fixed policy alone, on `fixed`.
    template <bool kFixedOnly>
    WaitResult Wait(State& s, sleep0::State& fixed, int64_t start, int64_t flag, int64_t overrun = 30 * us, bool longChunks = false)
    {
        WaitResult r;
        int64_t t = start;
        for (;;)
        {
            int64_t asked = 0;
            if constexpr (kFixedOnly)
                asked = sleep0::Poll(fixed, t) == sleep0::Action::Sleep ? sleep0::kSleepNs : 0;
            else
            {
                const Decision d = Poll(s, t, longChunks);
                asked = d.action == sleep0::Action::Sleep ? d.sleepNs : 0;
                if (asked && s.predictedEnd && !s.missed && t + asked > s.predictedEnd - kMarginNs)
                    r.sleptPastSpin = true;
            }
            r.decisions.push_back(asked);
            const int64_t from = t;
            if (asked)
            {
                t += asked + overrun;
                if (longChunks)
                    sleep0::SleptAsked(s.fixed, from, t, asked);
                else
                    sleep0::Slept(kFixedOnly ? fixed : s.fixed, from, t);
                r.sleeps++;
            }
            else
            {
                t += kYieldNs;
                r.yieldedNs += kYieldNs;
            }
            if (t >= flag)
                break;
        }
        r.end = t;
        r.delay = t - flag;
        return r;
    }

    // Frames: a wait from `flag - length` to `flag` each, flags kPeriod apart from `t0`.
    struct Frames
    {
        int64_t t0 = 1000 * ms;
        int64_t length = 15 * ms;
        int64_t Flag(int i) const { return t0 + i * kPeriod; }
    };
}

int main()
{
    printf("---- learning ----\n");
    {
        State s;
        sleep0::State unused;
        Frames f;
        int firstConfident = -1;
        for (int i = 0; i < 12; i++)
        {
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
            if (firstConfident < 0 && s.predictor.Period())
                firstConfident = i;
        }
        Check(firstConfident == 7, "confident once 7 long waits have ended (a wait ends at the next one's first poll): the 8th is the first predicted");
        const int64_t p = s.predictor.Period();
        Check(p >= kPeriod - 100 * us && p <= kPeriod + 100 * us, "the learned period: a frame (within the 30 us sleep overrun)");
    }
    {
        State s;
        sleep0::State unused;
        for (int i = 0; i < 40; i++)
        {
            const int64_t flag = 1000 * ms + i * kPeriod;
            Wait<false>(s, unused, flag - 60 * us, flag);  // the render thread's short handoff wait
        }
        Check(s.predictor.Period() == 0 && s.predictor.count == 0, "short periodic waits (60 us) never teach it anything");
    }

    {
        Predictor p;
        const int64_t odd[] = { 16'200, 17'100, 16'400, 16'900, 16'600, 16'300, 17'000, 16'700 };  // us
        p.lastEnd = 1;
        for (int64_t v : odd)
            p.Push(v * us);
        Check(p.Period() == 16'700 * us, "the period is the median of uneven intervals (16.2-17.1 ms: 16.7)");
        Predictor few;
        few.lastEnd = 1;
        for (int i = 0; i < 5; i++)
            few.Push(kPeriod);
        Check(few.Period() == 0, "5 intervals: not yet confident");
        few.Push(kPeriod);
        Check(few.Period() == kPeriod, "6: confident");
    }

    printf("---- a predicted wait ----\n");
    {
        State s;
        sleep0::State fixed;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, fixed, f.Flag(i) - f.length, f.Flag(i));
        const WaitResult r = Wait<false>(s, fixed, f.Flag(10) - f.length, f.Flag(10));
        Check(s.predictedEnd != 0, "a prediction for this wait");
        Check(r.delay <= kYieldNs, "the handoff costs at most a yield (it yields across the end)");
        Check(!r.sleptPastSpin, "no sleep ends past the start of the yield window");
        Check(r.yieldedNs >= kMarginNs - 200 * us && r.yieldedNs <= sleep0::kSpinNs + kMarginNs + 200 * us,
            "yields: the first kSpinNs and the margin, no more");
        sleep0::State alone;
        Frames g;
        WaitResult fr;
        for (int i = 0; i <= 10; i++)
            fr = Wait<true>(s, alone, g.Flag(i) - g.length, g.Flag(i));
        Check(fr.delay > 20 * us, "the fixed policy alone: its final sleep delays the handoff");
        Check(r.sleeps >= fr.sleeps - 20 && r.sleeps < fr.sleeps, "fewer sleeps than the fixed policy, by about the margin's");
    }

    printf("---- the stats ----\n");
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 20; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        Poll(s, f.Flag(20));  // the next wait's first poll ends the 20th
        Check(s.stats.confident == 13 && s.stats.hits == 13 && s.stats.early == 0 && s.stats.misses == 0, "13 predicted waits (8th to 20th), all hits");
        Check(s.stats.spinMaxNs <= kMarginNs + 100 * us && s.stats.spinSumNs / int64_t(s.stats.hits) >= kMarginNs - 100 * us,
            "yielded before the end: about the margin");
    }

    printf("---- prediction errors ----\n");
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        const WaitResult early = Wait<false>(s, unused, f.Flag(10) - f.length, f.Flag(10) - 3 * ms);  // the flag 3 ms early
        Check(early.delay <= sleep0::kSleepNs + 30 * us, "an early end costs at most one sleep, as the fixed policy");
        Poll(s, f.Flag(11) - f.length);
        Check(s.stats.early == 1, "counted as early");
        Check(s.predictor.lastEnd == early.end, "the next prediction starts from the actual end");
    }
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        const WaitResult late = Wait<false>(s, unused, f.Flag(10) - f.length, f.Flag(10) + 10 * ms);  // the flag 10 ms late
        Check(late.yieldedNs <= sleep0::kSpinNs + kMarginNs + kLateBudgetNs + 100 * us,
            "a wait 10 ms past its prediction yields no more than kSpinNs + kMarginNs + kLateBudgetNs");
        Check(late.delay <= sleep0::kSleepNs + 30 * us, "and then sleeps as the fixed policy (one sleep's delay at most)");
        Poll(s, f.Flag(11) + 9 * ms);
        Check(s.stats.misses == 1, "counted as a miss");
        Wait<false>(s, unused, f.Flag(12) - f.length, f.Flag(12));
        Check(s.predictedEnd == 0, "after a late end, no prediction from it: the fixed policy");
        Wait<false>(s, unused, f.Flag(13) - f.length, f.Flag(13));
        Check(s.predictedEnd == 0, "nor from an end that is still out of phase with it");
        const uint64_t hitsBefore = s.stats.hits;
        const WaitResult back = Wait<false>(s, unused, f.Flag(14) - f.length, f.Flag(14));
        Poll(s, f.Flag(15) - f.length);
        Check(s.stats.misses == 1 && s.stats.hits == hitsBefore + 1 && back.delay <= kYieldNs,
            "re-anchored by an end in phase: a hit again, no second miss");
    }
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        // A dropped frame: no wait at frame 10; the next ends at frame 11.
        Wait<false>(s, unused, f.Flag(11) - f.length, f.Flag(11));
        Check(s.predictor.Period() != 0, "one dropped frame: still confident");
        // Two more dropped frames within the history (frames 13 and 15): without counting a
        // double interval as two periods, 3 of 8 would be off and confidence lost.
        Wait<false>(s, unused, f.Flag(12) - f.length, f.Flag(12));
        Wait<false>(s, unused, f.Flag(14) - f.length, f.Flag(14));
        Wait<false>(s, unused, f.Flag(16) - f.length, f.Flag(16));
        Poll(s, f.Flag(17) - f.length);
        Check(s.predictor.Period() != 0, "three dropped frames in 8 intervals: still confident (each a double period)");
        const WaitResult r = Wait<false>(s, unused, f.Flag(12) - f.length, f.Flag(12));
        Check(r.delay <= kYieldNs, "the next wait is predicted from the right frame");
    }
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        // This wait starts later than a period after the last end (a long frame): the prediction is the next frame.
        const WaitResult r = Wait<false>(s, unused, f.Flag(11) - 5 * ms, f.Flag(11));
        Check(r.delay <= kYieldNs && s.predictedEnd > f.Flag(10), "a wait that starts past the next end predicts the frame after");
    }

    {
        // Two waits with under 300 us of work between them merge into one run (the fixed
        // policy's run definition): the second is a miss, no worse than the fixed policy,
        // and the next wait is predicted again.
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        const WaitResult first = Wait<false>(s, unused, f.Flag(10) - f.length, f.Flag(10));
        const WaitResult merged = Wait<false>(s, unused, first.end + 100 * us, f.Flag(11));  // 100 us of work: the same run
        Check(merged.delay <= sleep0::kSleepNs + 30 * us, "a merged second wait: at most one sleep's delay, as the fixed policy");
        Check(merged.yieldedNs <= sleep0::kSpinNs + kMarginNs + kLateBudgetNs + 100 * us, "and no more yielding than the worst case");
        Wait<false>(s, unused, f.Flag(12) - f.length, f.Flag(12));
        const WaitResult again = Wait<false>(s, unused, f.Flag(13) - f.length, f.Flag(13));
        Check(again.delay <= kYieldNs, "recovers: a later wait is a hit again");
    }

    printf("---- irregular waits: the fixed policy exactly ----\n");
    {
        State s;
        sleep0::State fixed;
        uint32_t seed = 12345;
        int64_t t = 1000 * ms;
        bool same = true;
        for (int i = 0; i < 60; i++)
        {
            seed = seed * 1664525u + 1013904223u;
            const int64_t gap = 8 * ms + int64_t(seed % 22000) * us;  // 8-30 ms apart
            const int64_t length = 3 * ms + int64_t((seed >> 8) % 9000) * us;
            t += gap;
            State copy = s;
            sleep0::State fixedCopy = fixed;
            const WaitResult a = Wait<false>(copy, fixedCopy, t - length, t);
            const WaitResult b = Wait<true>(copy, fixed, t - length, t);
            s = copy;
            same &= a.decisions == b.decisions;
        }
        Check(s.predictor.Period() == 0, "irregular intervals: never confident");
        Check(same, "decision for decision the fixed policy's");
    }

    printf("---- sleeps, oversleep and the clamp ----\n");
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        // A 2 ms oversleep that lands inside the yield window: the next poll yields.
        const WaitResult r = Wait<false>(s, unused, f.Flag(10) - f.length, f.Flag(10), 2 * ms);
        Check(r.delay <= kYieldNs + 2 * ms + 100 * us && s.stats.misses == 0, "an oversleep into the window: yields after it");
    }
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        const int64_t start = f.Flag(10) - f.length;
        Poll(s, start);  // the wait's first poll: a prediction
        const int64_t spinFrom = s.predictedEnd - kMarginNs;
        State a = s, b = s;
        a.fixed.lastZero = spinFrom - 60 * us;  // as if polling ever since (no gap: the same run)
        b.fixed.lastZero = spinFrom - 20 * us;
        const Decision near = Poll(a, spinFrom - 50 * us);
        Check(near.action == sleep0::Action::Sleep && near.sleepNs == 50 * us, "50 us before the window: a 50 us sleep, not 100");
        const Decision nearer = Poll(b, spinFrom - 10 * us);
        Check(nearer.action == sleep0::Action::Yield, "10 us before it: a yield (under kMinSleepNs)");
    }

    printf("---- the fixed policy's own yields stay ----\n");
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        int64_t t = f.Flag(10) - f.length;
        for (; t < f.Flag(10) - f.length + 2 * ms; t += kYieldNs)
            Poll(s, t);  // polling back to back (no sleeps taken)
        Check(Poll(s, t).action == sleep0::Action::Sleep, "past the run's first kSpinNs: a sleep");
        sleep0::Slept(s.fixed, t, t + 3 * ms);  // a late sleep
        Check(Poll(s, t + 3 * ms + 10 * us).action == sleep0::Action::Yield, "after a late sleep the fixed policy yields, predicted or not");
    }

    printf("---- a switch ----\n");
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i));
        s = {};  // RECARBON_SLEEP0_BACKOFF_AT restarts the thread's state
        Check(s.predictor.Period() == 0 && s.predictedEnd == 0 && !s.started, "after a reset: nothing predicted, learning again");
    }

    printf("---- =3: long chunks ----\n");
    {
        // The same learning, then one predicted wait with and without long chunks.
        auto Learned = [] {
            State s;
            sleep0::State unused;
            Frames f;
            for (int i = 0; i < 10; i++)
                Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i), 30 * us, true);
            return s;
        };
        State a = Learned(), b = Learned();
        sleep0::State unused;
        Frames f;
        const WaitResult shortChunks = Wait<false>(a, unused, f.Flag(10) - f.length, f.Flag(10), 30 * us, false);
        const WaitResult longChunks = Wait<false>(b, unused, f.Flag(10) - f.length, f.Flag(10), 30 * us, true);
        Check(longChunks.delay <= kYieldNs && !longChunks.sleptPastSpin, "a long-chunk wait: the same hit, no sleep into the yield window");
        Check(longChunks.sleeps * 4 <= shortChunks.sleeps, "at least 4x fewer sleeps (wakeups) than 100 us chunks");
        Check(longChunks.sleeps >= 12 && longChunks.sleeps <= 20, "about 16 wakeups for a 15 ms wait");
        Check(longChunks.yieldedNs <= shortChunks.yieldedNs + 50 * us, "and no more yielding");
        Check(b.stats.longSleeps >= 10, "counted as long sleeps");
    }
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i), 30 * us, true);
        const int64_t start = f.Flag(10) - f.length;
        Poll(s, start, true);
        const int64_t end = s.predictedEnd, spinFrom = end - kMarginNs;
        auto At = [&](int64_t t) {
            State c = s;
            c.fixed.lastZero = t - 10 * us;  // polling ever since: the same run
            return Poll(c, t, true);
        };
        Check(At(end - 5 * ms).sleepNs == kLongChunkNs, "5 ms before the end: a 1 ms sleep");
        Check(At(end - kLongChunkFromNs - 1).sleepNs == std::min(kLongChunkNs, spinFrom - (end - kLongChunkFromNs - 1)),
            "just over 2 ms before: a long sleep, clamped to the yield window (0.5 ms)");
        Check(At(end - kLongChunkFromNs).sleepNs == sleep0::kSleepNs, "exactly 2 ms before: back to 100 us");
        Check(At(end - 1700 * us).sleepNs == sleep0::kSleepNs, "1.7 ms before (200 us to the window): a 100 us sleep");
        Check(At(end - 1550 * us).sleepNs == 50 * us, "1.55 ms before: only the 50 us to the window");
        Check(At(spinFrom + 10 * us).action == sleep0::Action::Yield, "inside the window: yield");
        Check(At(end - 2600 * us).sleepNs == kLongChunkNs - 0 && end - 2600 * us + kLongChunkNs <= spinFrom, "2.6 ms before: a full 1 ms sleep still ends before the window");
    }
    {
        // Unconfident: the same decisions as =2 and the fixed policy (100 us), never long.
        State a, b;
        sleep0::State fa, fb;
        uint32_t seed = 777;
        int64_t t = 1000 * ms;
        bool same = true;
        for (int i = 0; i < 40; i++)
        {
            seed = seed * 1664525u + 1013904223u;
            t += 8 * ms + int64_t(seed % 22000) * us;
            const int64_t length = 3 * ms + int64_t((seed >> 8) % 9000) * us;
            const WaitResult x = Wait<false>(a, fa, t - length, t, 30 * us, true);
            const WaitResult y = Wait<false>(b, fb, t - length, t, 30 * us, false);
            same &= x.decisions == y.decisions;
        }
        Check(same && a.stats.longSleeps == 0, "irregular waits: =3 decides exactly as =2 (100 us sleeps), no long sleeps");
    }
    {
        // An early handoff inside a long chunk: at most the chunk plus its overrun.
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i), 30 * us, true);
        const WaitResult early = Wait<false>(s, unused, f.Flag(10) - f.length, f.Flag(10) - 6 * ms, 30 * us, true);
        Check(early.delay <= kLongChunkNs + 30 * us, "an early end 6 ms before the prediction: at most one long chunk late");
        Poll(s, f.Flag(11) - f.length, true);
        Check(s.stats.early == 1, "counted as early");
    }
    {
        // A miss: after the prediction plus kLateBudgetNs, 100 us sleeps (the fixed policy), not long ones.
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i), 30 * us, true);
        const uint64_t longBefore = s.stats.longSleeps;
        Poll(s, f.Flag(10) - f.length, true);
        const int64_t past = s.predictedEnd + kLateBudgetNs + 1 * us;
        State c = s;
        c.fixed.lastZero = past - 10 * us;
        const Decision d = Poll(c, past, true);
        Check(d.action == sleep0::Action::Sleep && d.sleepNs == sleep0::kSleepNs && c.missed, "past the late budget: a 100 us sleep and a miss");
        c.fixed.lastZero = past + 5 * ms - 10 * us;  // still polling: the same wait
        const Decision later = Poll(c, past + 5 * ms, true);
        Check(later.sleepNs == sleep0::kSleepNs && c.stats.longSleeps == longBefore + 0, "and stays 100 us for the rest of the wait");
    }
    {
        // A long chunk that oversleeps into the window: yields after it; lateness against the asked 1 ms.
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i), 30 * us, true);
        const WaitResult r = Wait<false>(s, unused, f.Flag(10) - f.length, f.Flag(10), 900 * us, true);  // each sleep 0.9 ms over
        Check(r.delay <= 900 * us + kLongChunkNs && s.stats.misses == 0, "oversleeping chunks: still within one chunk of the end, no miss");
        sleep0::State st;
        Check(!sleep0::SleptAsked(st, 0, kLongChunkNs + sleep0::kLateNs, kLongChunkNs), "a 1 ms sleep up to kLateNs over: not late");
        Check(sleep0::SleptAsked(st, 0, kLongChunkNs + sleep0::kLateNs + 1, kLongChunkNs) && st.yieldUntil > 0, "more: late, the 1 s yield fallback");
        Check(!sleep0::SleptAsked(st, 5 * ms, 5 * ms + 1 * ms, 1 * ms) && st.lastZero == 6 * ms, "and the run goes on from its end");
    }
    {
        State s;
        sleep0::State unused;
        Frames f;
        for (int i = 0; i < 10; i++)
            Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i), 30 * us, true);
        s = {};
        Check(s.predictor.Period() == 0 && s.stats.longSleeps == 0, "a switch's reset clears =3's state too");
    }

    printf("---- threads (no shared state) ----\n");
    {
        constexpr int kThreads = 4;
        std::vector<std::vector<int64_t>> traces(kThreads);
        std::vector<std::thread> threads;
        for (int k = 0; k < kThreads; k++)
            threads.emplace_back([&, k] {
                State s;
                sleep0::State unused;
                Frames f;
                for (int i = 0; i < 30; i++)
                {
                    const WaitResult r = Wait<false>(s, unused, f.Flag(i) - f.length, f.Flag(i) + (i == 20 ? 4 * ms : 0));
                    traces[k].insert(traces[k].end(), r.decisions.begin(), r.decisions.end());
                }
            });
        for (auto& t : threads)
            t.join();
        bool same = true;
        for (int k = 1; k < kThreads; k++)
            same &= traces[k] == traces[0];
        Check(same && !traces[0].empty(), "four threads, the same script, independent states: identical decisions");
    }

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
