// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// kernel/sleep0_schedule.h: RECARBON_SLEEP0_BACKOFF_AT=<s>,<s>,... (a diagnostic, off by default)
// switches the macOS Sleep(0) back-off on or off at those seconds in one session. Parser,
// boundaries, the transitions a thread sees, and one log line per switch under racing threads.
// Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/sleep0_schedule_test.cpp -o build/sleep0_schedule_test && build/sleep0_schedule_test
#include <kernel/sleep0_schedule.h>

#include <cstdio>
#include <string>
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

    using namespace kernel::sleep0schedule;

    bool Is(const char* v, std::vector<double> want)
    {
        return ParseTimes(v) == want;
    }
}

int main()
{
    printf("---- RECARBON_SLEEP0_BACKOFF_AT ----\n");
    Check(Is(nullptr, {}) && Is("", {}), "unset or empty: no schedule");
    Check(Is("240", { 240 }), "one time");
    Check(Is("0", { 0 }), "0: from the first poll");
    Check(Is("240,270.5,300.25", { 240, 270.5, 300.25 }), "several, with fractions");
    Check(Is("1.05", { 1.05 }), "two fraction digits, exactly as strtod reads them");
    Check(Is("270,240", {}) && Is("240,240", {}), "descending or repeated: none");
    Check(Is("240,", {}) && Is(",240", {}) && Is("240,,270", {}), "an empty item: none");
    Check(Is(" 240", {}) && Is("240 ", {}) && Is("240s", {}) && Is("240;270", {}) && Is("240 270", {}), "spaces, junk or another separator: none");
    Check(Is("-5", {}) && Is("+5", {}) && Is(".5", {}) && Is("5.", {}) && Is("1e3", {}), "signs, bare points, exponents: none");
    Check(Is("999999", { 999999 }) && Is("1000000", {}), "six digits at most");
    {
        std::string many;
        for (size_t i = 1; i <= kMaxSwitches; i++)
            many += (i > 1 ? "," : "") + std::to_string(i);
        Check(ParseTimes(many.c_str()).size() == kMaxSwitches, "kMaxSwitches times");
        many += "," + std::to_string(kMaxSwitches + 1);
        Check(ParseTimes(many.c_str()).empty(), "one more: none");
    }

    printf("---- phases ----\n");
    const std::vector<double> times = { 240, 270, 300 };
    Check(PhaseAt({}, 1e9) == 0, "no schedule: phase 0 throughout");
    Check(PhaseAt(times, 239.999) == 0 && PhaseAt(times, 240) == 1, "a switch takes effect at its time exactly");
    Check(PhaseAt(times, 269.9) == 1 && PhaseAt(times, 270) == 2 && PhaseAt(times, 1e9) == 3, "each time reached adds one");
    Check(!OnIn(false, 0) && OnIn(false, 1) && !OnIn(false, 2) && OnIn(false, 3), "from off: on in odd phases");
    Check(OnIn(true, 0) && !OnIn(true, 1) && OnIn(true, 2), "from RECARBON_SLEEP0_BACKOFF=1: the other way round");

    printf("---- a thread's transitions ----\n");
    {
        ThreadPhase t;
        Check(!t.Changed(0), "the first look isn't a change");
        Check(!t.Changed(0), "the same phase: no change");
        Check(t.Changed(1), "a new phase: a change");
        Check(!t.Changed(1), "once");
        Check(t.Changed(3), "phases skipped while the thread didn't poll: one change");
    }
    {
        ThreadPhase t;
        Check(!t.Changed(2), "a thread that first polls in a later phase: no change");
    }

    printf("---- one poll's step ----\n");
    {
        const std::vector<double> at = { 240, 270 };
        ThreadPhase seen;
        std::atomic<int> logged{ 0 };
        Step s = StepAt(at, false, 100, seen, logged);
        Check(s.phase == 0 && !s.backOff && !s.newPhase && s.closedPhase == -1 && !s.logSwitch, "before the first switch: off, nothing to do");
        s = StepAt(at, false, 239.9, seen, logged);
        Check(!s.newPhase && !s.logSwitch, "still phase 0");
        s = StepAt(at, false, 240, seen, logged);
        Check(s.phase == 1 && s.backOff && s.newPhase && s.closedPhase == 0 && !s.closedBackOff && s.logSwitch,
            "at 240 s: on; restart the back-off state; close phase 0 (off); log the switch");
        s = StepAt(at, false, 241, seen, logged);
        Check(s.backOff && !s.newPhase && s.closedPhase == -1 && !s.logSwitch, "later in phase 1: on, nothing more");
        ThreadPhase other;
        s = StepAt(at, false, 242, other, logged);
        Check(s.backOff && !s.newPhase && !s.logSwitch, "another thread's first poll in phase 1: no window to close, the switch already logged");
        s = StepAt(at, false, 275, seen, logged);
        Check(s.phase == 2 && !s.backOff && s.closedPhase == 1 && s.closedBackOff && s.logSwitch, "at 270 s: off again; close phase 1 (on)");
        s = StepAt(at, false, 280, other, logged);
        Check(s.newPhase && s.closedPhase == 1 && !s.logSwitch, "the other thread closes its own phase-1 window, without a second switch line");
    }
    {
        ThreadPhase seen;
        std::atomic<int> logged{ 0 };
        Step s = StepAt({ 240 }, true, 300, seen, logged);
        Check(s.phase == 1 && !s.backOff && !s.newPhase && s.logSwitch, "from RECARBON_SLEEP0_BACKOFF=1: off after the switch; a first poll late still logs it");
    }

    printf("---- one log line per switch, threads racing ----\n");
    {
        std::atomic<int> logged{ 0 };
        Check(!ClaimSwitchLog(logged, 0), "phase 0 is never a switch");
        Check(ClaimSwitchLog(logged, 1) && !ClaimSwitchLog(logged, 1), "phase 1: one claim");
        Check(!ClaimSwitchLog(logged, 0), "an older phase seen late: no claim");
        Check(ClaimSwitchLog(logged, 3) && logged.load() == 3, "a jump: one claim, at the newest");
    }
    {
        constexpr int kThreads = 8, kPhases = 40;
        std::atomic<int> logged{ 0 };
        std::vector<std::atomic<int>> perPhase(kPhases + 1);
        std::vector<std::thread> threads;
        std::atomic<bool> go{ false };
        std::vector<int> badOrder(kThreads, 0), changes(kThreads, 0);
        std::vector<double> schedule;
        for (int s = 1; s <= kPhases; s++)
            schedule.push_back(s);
        for (int k = 0; k < kThreads; k++)
            threads.emplace_back([&, k] {
                while (!go.load())
                {
                }
                ThreadPhase mine;
                int last = -1;
                for (int i = 0; i <= kPhases * 50; i++)
                {
                    const int phase = PhaseAt(schedule, double(i) / 50.0);
                    if (phase < last)
                        badOrder[k]++;
                    last = phase;
                    if (mine.Changed(phase))
                        changes[k]++;
                    if (ClaimSwitchLog(logged, phase))
                        perPhase[phase].fetch_add(1);
                }
            });
        go.store(true);
        for (auto& t : threads)
            t.join();
        int total = 0, bad = 0;
        for (int p = 1; p <= kPhases; p++)
        {
            total += perPhase[p].load();
            bad += perPhase[p].load() > 1;
        }
        Check(bad == 0 && total == kPhases && logged.load() == kPhases, "racing threads: exactly one claim for each of the 40 switches");
        bool order = true, every = true;
        for (int k = 0; k < kThreads; k++)
        {
            order &= badOrder[k] == 0;
            every &= changes[k] == kPhases;
        }
        Check(order, "each thread sees phases in order");
        Check(every, "each thread sees each of the 40 switches once");
    }

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
