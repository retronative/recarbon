// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// The back-off for Carbon's Direct3D waits on the GPU: on by default on
// macOS since v0.1.3 (RECARBON_GPU_WAIT_BACKOFF=0 turns it off; =1 elsewhere). The fence, ring-space and idle waits
// (ten call sites) poll a counter in busy loops that call the GPU hang check
// once per poll and never sleep or yield (the hang check itself only reads a
// thread pointer, the clock and the counter), so the game's Sleep(0) back-off
// (kernel/sleep0_backoff.h) cannot reach them and the main thread holds a
// core for as long as the GPU lags, which at 3x is most of the frame
// (measured on a MacBook Air M5, October 2026). game/gpu_waits.cpp overrides the hang
// check and asks this policy first.
//
// The policy, per thread, on a steady clock in ns: a run of polls (gaps under
// kGapNs) spins for its first kSpinNs, then sleeps kSleepNs before each poll.
// A sleep that ends more than kLateNs past its length (macOS timer
// coalescing on a loaded machine) makes the thread yield instead of sleeping
// for kYieldAfterLateNs. The constants follow SpeedBreaker Carbon v0.1.0's
// GPU-wait back-off (GPL-3.0-or-later): a 20 us spin then 50 us sleeps, a
// 200 us gap starting a new wait. Pure, for tests/gpu_wait_backoff_test.cpp.
#pragma once
#include <cstdint>

namespace kernel::gpuwait
{
    constexpr int64_t kGapNs = 200'000;                // a pause longer than this ends a run of polls
    constexpr int64_t kSpinNs = 20'000;                // a run spins this long before it sleeps
    constexpr int64_t kSleepNs = 50'000;               // each poll's sleep after that
    constexpr int64_t kLateNs = 2'000'000;             // a sleep this much past kSleepNs is stretched
    constexpr int64_t kYieldAfterLateNs = 1'000'000'000;

    enum class Action { Spin, Yield, Sleep };

    struct State
    {
        int64_t first = 0, last = 0;  // the run's first and latest poll
        int64_t yieldUntil = 0;       // after a late sleep: yield until then
    };

    // Before a poll at `now`: spin (return to the loop at once), yield, or
    // sleep kSleepNs.
    inline Action Poll(State& s, int64_t now)
    {
        if (now - s.last > kGapNs)  // (a steady clock is far past kGapNs at a thread's first poll)
            s.first = now;
        s.last = now;
        if (now < s.yieldUntil)
            return Action::Yield;
        if (now - s.first <= kSpinNs)
            return Action::Spin;
        return Action::Sleep;
    }

    // After a sleep that began at `start` and ended at `end`: late or not.
    inline bool Slept(State& s, int64_t start, int64_t end)
    {
        s.last = end;  // the run goes on from the sleep's end
        const bool late = end - start > kSleepNs + kLateNs;
        if (late)
            s.yieldUntil = end + kYieldAfterLateNs;
        return late;
    }
}
