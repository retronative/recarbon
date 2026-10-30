// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// The macOS back-off for the game's zero delays (Sleep(0): KeDelayExecutionThread
// with no time, in kernel/imports_sync.cpp). On the 360 a hardware thread that
// polled a flag cost nothing; here sched_yield returns at once when nothing else
// is runnable, so each poll keeps a host core at full clock. Carbon's frame
// handshake polls this way on both sides (the main loop sub_824E9720 while the
// word at 0x82C651C4 is 0, WaitPlayed sub_824E9948 while it is 1).
//
// SpeedBreaker v0.2.0's policy (cpu/backoff.cpp): a run of zero delays, each
// under kGapNs after the last, yields for its first kSpinNs, then sleeps kSleepNs
// per poll. A sleep that ends more than kLateNs late (macOS stretches the timers
// of some threads, App Nap) makes that thread yield again for kYieldAfterLateNs.
// RECARBON_SLEEP0_BACKOFF=1 turns it on on macOS (off by default there; on Linux
// the older back-off is on unless =0). Pure, for tests/sleep0_backoff_test.cpp.
#pragma once
#include <cstdint>

namespace kernel::sleep0
{
    constexpr int64_t kGapNs = 300'000;                // a pause longer than this ends a run of polls
    constexpr int64_t kSpinNs = 1'000'000;             // a run yields this long before it sleeps
    constexpr int64_t kSleepNs = 100'000;              // each poll's sleep after that
    constexpr int64_t kLateNs = 2'000'000;             // a sleep this much past kSleepNs is stretched
    constexpr int64_t kYieldAfterLateNs = 1'000'000'000;

    enum class Action { Yield, Sleep };

    // One thread's polls (times on a steady clock, in ns).
    struct State
    {
        int64_t firstZero = 0, lastZero = 0;
        int64_t yieldUntil = 0;  // after a late sleep: yield until then
    };

    // A zero delay at `now`: yield, or sleep kSleepNs (then call Slept).
    inline Action Poll(State& s, int64_t now)
    {
        if (now - s.lastZero > kGapNs)  // (a steady clock is far past kGapNs at a thread's first poll)
            s.firstZero = now;
        s.lastZero = now;
        if (now - s.firstZero <= kSpinNs || now < s.yieldUntil)
            return Action::Yield;
        return Action::Sleep;
    }

    // The sleep Poll asked for ran from `start` to `end`. True if it was late.
    inline bool Slept(State& s, int64_t start, int64_t end)
    {
        s.lastZero = end;  // the run goes on from the sleep's end
        const bool late = end - start > kSleepNs + kLateNs;
        if (late)
            s.yieldUntil = end + kYieldAfterLateNs;
        return late;
    }

    // RECARBON_SLEEP0_BACKOFF_FRAME_THREADS=0 (an experiment, off by default):
    // a frame thread (kernel/thread_priority.h: one that presents or polls
    // the controller) keeps yielding, so the back-off is confined to the
    // menus' polls (the render thread's, about 0.93 core measured) and never
    // touches a thread on the frame's critical path in gameplay.
    inline bool AppliesTo(bool on, bool frameThread, bool excludeFrameThreads)
    {
        return on && !(frameThread && excludeFrameThreads);
    }

    // As Slept, for a sleep of `askedNs` rather than kSleepNs (the predicted
    // wake's chunks, kernel/sleep0_predict.h): late past askedNs + kLateNs.
    inline bool SleptAsked(State& s, int64_t start, int64_t end, int64_t askedNs)
    {
        s.lastZero = end;
        const bool late = end - start > askedNs + kLateNs;
        if (late)
            s.yieldUntil = end + kYieldAfterLateNs;
        return late;
    }
}
