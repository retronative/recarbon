// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// kernel/sleep0_backoff.h: the macOS back-off for the game's Sleep(0) polls (off by default,
// RECARBON_SLEEP0_BACKOFF=1), on a fake clock. Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/sleep0_backoff_test.cpp -o build/sleep0_backoff_test && build/sleep0_backoff_test
#include <kernel/sleep0_backoff.h>

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

    using namespace kernel::sleep0;
    constexpr int64_t us = 1000, ms = 1000 * us;

    // Polls every `step` from `from` until `to`; the first poll that sleeps, or -1.
    int64_t FirstSleep(State& s, int64_t from, int64_t to, int64_t step)
    {
        for (int64_t t = from; t <= to; t += step)
            if (Poll(s, t) == Action::Sleep)
                return t;
        return -1;
    }
}

int main()
{
    printf("---- a run of zero delays ----\n");
    {
        State s;
        Check(Poll(s, 5 * ms) == Action::Yield, "the first poll yields");
        Check(FirstSleep(s, 5 * ms + 10 * us, 5 * ms + kSpinNs, 10 * us) == -1, "polls within the first kSpinNs of a run yield");
        Check(Poll(s, 5 * ms + kSpinNs + 10 * us) == Action::Sleep, "past kSpinNs of back-to-back polls: sleep");
    }
    {
        State s;
        const int64_t t0 = 3600'000 * ms;  // a steady clock an hour in
        Check(Poll(s, t0) == Action::Yield && FirstSleep(s, t0 + 10 * us, t0 + 2 * ms, 10 * us) == t0 + kSpinNs + 10 * us,
            "an hour in: the first poll past kSpinNs sleeps");
    }
    {
        State s;
        FirstSleep(s, 0, 500 * us, 100 * us);
        Check(Poll(s, 500 * us + kGapNs) == Action::Yield, "a pause of exactly kGapNs keeps the run (still under kSpinNs)");
        Check(Poll(s, kSpinNs + 10 * us) == Action::Sleep, "the kept run passes kSpinNs: sleep");
    }
    {
        State s;
        FirstSleep(s, 0, 900 * us, 100 * us);
        const int64_t t = 900 * us + kGapNs + 1;  // just over kGapNs after the last poll (900 us)
        Check(Poll(s, t) == Action::Yield, "a pause over kGapNs starts a new run");
        Check(FirstSleep(s, t + 100 * us, t + kSpinNs, 100 * us) == -1, "the new run yields for its own kSpinNs");
        Check(Poll(s, t + kSpinNs + 1) == Action::Sleep, "then sleeps");
    }

    printf("---- after a sleep ----\n");
    {
        State s;
        const int64_t t = FirstSleep(s, 0, 2 * ms, 10 * us);
        Check(!Slept(s, t, t + kSleepNs + 20 * us), "a sleep on time isn't late");
        Check(Poll(s, t + kSleepNs + 20 * us + 50 * us) == Action::Sleep, "the run goes on from the sleep's end: sleep again");
        Check(Poll(s, t + kSleepNs + 20 * us + 50 * us + kGapNs + 1) == Action::Yield, "a pause after it still starts a new run");
    }
    {
        State s;
        const int64_t t = FirstSleep(s, 0, 2 * ms, 10 * us);
        Check(!Slept(s, t, t + kSleepNs + 1500 * us), "a slow sleep under kLateNs late isn't late");
        Check(Poll(s, t + kSleepNs + 1500 * us + 50 * us) == Action::Sleep, "a poll soon after a slow sleep continues the run");
    }
    {
        State s;
        const int64_t t = FirstSleep(s, 0, 2 * ms, 10 * us);
        Check(!Slept(s, t, t + kSleepNs + kLateNs), "kLateNs late exactly: not late");
    }
    {
        State s;
        const int64_t t = FirstSleep(s, 0, 2 * ms, 10 * us);
        const int64_t end = t + kSleepNs + kLateNs + 1;
        Check(Slept(s, t, end), "more than kLateNs late: late");
        Check(Poll(s, end + 10 * us) == Action::Yield, "after a late sleep the run yields");
        Check(FirstSleep(s, end + 20 * us, end + kYieldAfterLateNs - 1, 100 * us) == -1, "for kYieldAfterLateNs");
        Check(Poll(s, end + kYieldAfterLateNs) == Action::Sleep, "then sleeps again");
    }
    {
        State s;
        const int64_t t = FirstSleep(s, 0, 2 * ms, 10 * us);
        Slept(s, t, t + 10 * ms);  // a stretched sleep
        const int64_t later = t + 10 * ms + 300 * ms;
        Check(Poll(s, later) == Action::Yield, "a new run inside the late window yields");
        Check(FirstSleep(s, later + 10 * us, later + 2 * ms, 10 * us) == -1, "even past its kSpinNs");
    }

    printf("---- frame threads ----\n");
    Check(AppliesTo(true, false, false) && AppliesTo(true, true, false), "by default every thread backs off");
    Check(AppliesTo(true, false, true) && !AppliesTo(true, true, true), "with frame threads excluded: only the others");
    Check(!AppliesTo(false, false, true) && !AppliesTo(false, true, false), "off is off");

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
