// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_SLEEP0_BACKOFF_AT=<s>,<s>,... (a diagnostic, off by default; macOS):
// the Sleep(0) back-off (kernel/sleep0_backoff.h) switches on or off at those
// seconds since launch, starting from RECARBON_SLEEP0_BACKOFF's state, so one
// session can compare both under the same heat and display phase.
//
// The phase is a function of the time alone (the switch times are fixed at
// startup), so every thread works it out for itself without a lock. A thread
// that sees a new phase restarts its back-off state and closes its trace window
// (kernel/sleep0_trace.h), so each [sleep0] line belongs to one phase; one
// thread, whichever claims it first, logs each switch. Pure, for
// tests/sleep0_schedule_test.cpp.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace kernel::sleep0schedule
{
    constexpr size_t kMaxSwitches = 64;

    // Up to kMaxSwitches times in seconds, each plain digits (at most 6) with an
    // optional fraction ("240", "270.5"), strictly ascending and comma-separated.
    // Anything else: none (no schedule).
    inline std::vector<double> ParseTimes(const char* v)
    {
        std::vector<double> times;
        if (!v || !*v)
            return times;
        for (const char* p = v;;)
        {
            const char* start = p;
            int digits = 0, fraction = -1;
            for (; *p >= '0' && *p <= '9'; p++)
                digits++;
            if (*p == '.')
                for (p++, fraction = 0; *p >= '0' && *p <= '9'; p++)
                    fraction++;
            if (digits == 0 || digits > 6 || fraction == 0 || times.size() == kMaxSwitches)
                return {};
            const double t = std::strtod(start, nullptr);  // the digits just checked
            if (!times.empty() && t <= times.back())
                return {};
            times.push_back(t);
            if (*p == '\0')
                return times;
            if (*p != ',')
                return {};
            p++;
        }
    }

    // The phase at `seconds`: how many switch times have been reached.
    inline int PhaseAt(const std::vector<double>& times, double seconds)
    {
        int reached = 0;
        while (size_t(reached) < times.size() && times[reached] <= seconds)
            reached++;
        return reached;
    }

    // Whether the back-off is on in `phase`, starting from `initial`.
    inline bool OnIn(bool initial, int phase)
    {
        return initial != (phase % 2 == 1);
    }

    // One thread's view: true the first time it sees a phase after another
    // (not on its first look).
    struct ThreadPhase
    {
        int seen = -1;

        bool Changed(int phase)
        {
            const bool changed = seen >= 0 && phase != seen;
            seen = phase;
            return changed;
        }
    };

    // True for exactly one caller per phase past `logged` (whichever thread
    // gets there first): that one logs the switch.
    inline bool ClaimSwitchLog(std::atomic<int>& logged, int phase)
    {
        int last = logged.load(std::memory_order_relaxed);
        while (phase > last)
            if (logged.compare_exchange_weak(last, phase, std::memory_order_relaxed))
                return true;
        return false;
    }

    // What one zero delay at `seconds` does, for its thread (`seen`) and the
    // process (`logged`).
    struct Step
    {
        int phase = 0;
        bool backOff = false;     // the back-off is on for this poll
        bool newPhase = false;    // this thread's first poll in a new phase: restart its back-off state
        int closedPhase = -1;     // ... and close its trace window for this phase (-1: none)
        bool closedBackOff = false;
        bool logSwitch = false;   // this poll logs the switch into `phase` (once in the process)
    };

    inline Step StepAt(const std::vector<double>& times, bool initial, double seconds, ThreadPhase& seen, std::atomic<int>& logged)
    {
        Step step;
        step.phase = PhaseAt(times, seconds);
        step.backOff = OnIn(initial, step.phase);
        const int previous = seen.seen;
        if (seen.Changed(step.phase))
        {
            step.newPhase = true;
            step.closedPhase = previous;
            step.closedBackOff = OnIn(initial, previous);
        }
        step.logSwitch = ClaimSwitchLog(logged, step.phase);
        return step;
    }
}
