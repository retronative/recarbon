// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// kernel/gpu_wait_backoff.h: the back-off for Direct3D's GPU-wait polls (off by default,
// RECARBON_GPU_WAIT_BACKOFF=1), on a fake clock. Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/gpu_wait_backoff_test.cpp -o build/gpu_wait_backoff_test && build/gpu_wait_backoff_test
#include <kernel/gpu_wait_backoff.h>

#include <cstdio>

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

    using namespace kernel::gpuwait;
    constexpr int64_t us = 1000, ms = 1000 * us;

    // Polls every `step` from `from` until `to`; the first poll that doesn't spin, or -1.
    int64_t FirstNonSpin(State& s, int64_t from, int64_t to, int64_t step)
    {
        for (int64_t t = from; t <= to; t += step)
            if (Poll(s, t) != Action::Spin)
                return t;
        return -1;
    }
}

int main()
{
    printf("---- a run of polls ----\n");
    {
        State s;
        Check(Poll(s, 5 * ms) == Action::Spin, "the first poll spins");
        Check(FirstNonSpin(s, 5 * ms + 1 * us, 5 * ms + kSpinNs, 1 * us) == -1, "polls within the first kSpinNs of a run spin");
        Check(Poll(s, 5 * ms + kSpinNs + 1 * us) == Action::Sleep, "past kSpinNs of back-to-back polls: sleep");
    }
    {
        State s;
        const int64_t t0 = 3600'000 * ms;  // a steady clock an hour in
        Check(Poll(s, t0) == Action::Spin && FirstNonSpin(s, t0 + 1 * us, t0 + 1 * ms, 1 * us) == t0 + kSpinNs + 1 * us,
            "an hour in: the first poll past kSpinNs sleeps");
    }
    {
        State s;
        FirstNonSpin(s, 0, 10 * us, 2 * us);
        Check(Poll(s, 10 * us + kGapNs) == Action::Sleep, "a pause of exactly kGapNs keeps the run (now past kSpinNs): sleep");
    }
    {
        State s;
        FirstNonSpin(s, 0, 10 * us, 2 * us);
        const int64_t t = 10 * us + kGapNs + 1;  // just over kGapNs after the last poll
        Check(Poll(s, t) == Action::Spin, "a pause over kGapNs starts a new run");
        Check(FirstNonSpin(s, t + 1 * us, t + kSpinNs, 1 * us) == -1, "the new run spins for its own kSpinNs");
        Check(Poll(s, t + kSpinNs + 1) == Action::Sleep, "then sleeps");
    }
    {
        State s;
        Check(Poll(s, 0) == Action::Spin && Poll(s, kSpinNs) == Action::Spin, "a poll exactly kSpinNs into the run still spins");
        Check(Poll(s, kSpinNs + 1) == Action::Sleep, "one ns later it sleeps");
    }

    printf("---- after a sleep ----\n");
    {
        State s;
        const int64_t t = FirstNonSpin(s, 0, 1 * ms, 1 * us);
        Check(!Slept(s, t, t + kSleepNs + 20 * us), "a sleep on time isn't late");
        Check(Poll(s, t + kSleepNs + 20 * us + 10 * us) == Action::Sleep, "the run goes on from the sleep's end: sleep again");
        Check(Poll(s, t + kSleepNs + 20 * us + 10 * us + kGapNs + 1) == Action::Spin, "a pause after it still starts a new run");
    }
    {
        State s;
        const int64_t t = FirstNonSpin(s, 0, 1 * ms, 1 * us);
        Check(!Slept(s, t, t + kSleepNs + 1500 * us), "a slow sleep under kLateNs late isn't late");
        Check(Poll(s, t + kSleepNs + 1500 * us + 10 * us) == Action::Sleep, "a poll soon after a slow sleep continues the run");
    }
    {
        State s;
        const int64_t t = FirstNonSpin(s, 0, 1 * ms, 1 * us);
        Check(!Slept(s, t, t + kSleepNs + kLateNs), "kLateNs late exactly: not late");
    }
    {
        State s;
        const int64_t t = FirstNonSpin(s, 0, 1 * ms, 1 * us);
        const int64_t end = t + kSleepNs + kLateNs + 1;
        Check(Slept(s, t, end), "more than kLateNs late: late");
        Check(Poll(s, end + 1 * us) == Action::Yield, "after a late sleep the run yields (not spins, not sleeps)");
        for (int64_t at = end + 2 * us; at < end + kYieldAfterLateNs; at += 50 * ms)
            if (Poll(s, at) != Action::Yield)
            {
                Check(false, "yields for kYieldAfterLateNs, even across new runs");
                break;
            }
        Check(Poll(s, end + kYieldAfterLateNs) == Action::Spin, "then the window is over: a new run (50 ms since the last poll) spins again");
    }
    {
        State s;
        const int64_t t = FirstNonSpin(s, 0, 1 * ms, 1 * us);
        Slept(s, t, t + 10 * ms);  // a stretched sleep
        const int64_t later = t + 10 * ms + 300 * ms;
        Check(Poll(s, later) == Action::Yield, "a new run inside the late window yields from its first poll");
    }

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
