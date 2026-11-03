// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// How long a game thread waits in a polling loop, measured from its first poll
// to the loop's exit: a Span per thread and loop (two clock reads a wait: none
// for the polls after the first), and a Window of those waits for a summary
// line every kLogEveryNs (count, total, p50/p99/max, polls). Used by
// game/gpu_waits.cpp (Direct3D's GPU waits, into the hitch report's
// "D3D waits") and game/handoff_waits.cpp (RECARBON_HANDOFF_TRACE). Pure, for
// tests/wait_span_test.cpp.
#pragma once
#include <kernel/sleep0_trace.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>

namespace game::waitspan
{
    constexpr int64_t kLogEveryNs = 10'000'000'000;

    struct Span
    {
        int64_t start = 0;
        uint64_t polls = 0;
        bool open = false;

        // A poll at `now`: the first opens the wait. `now` is read only when
        // the caller sees !open (so the later polls cost no clock read).
        void Begin(int64_t now)
        {
            if (!open)
            {
                open = true;
                start = now;
                polls = 0;
            }
        }
        void Poll() { polls++; }
        // The loop's exit at `now`: the wait's length, or -1 if no poll
        // opened one (the loop found what it waits for at once).
        int64_t End(int64_t now)
        {
            if (!open)
                return -1;
            open = false;
            return std::max<int64_t>(0, now - start);
        }
    };

    struct Window
    {
        kernel::sleep0trace::Histogram length;
        uint64_t waits = 0, polls = 0;
        int64_t totalNs = 0, windowStart = 0;

        void Add(int64_t now, int64_t lengthNs, uint64_t spanPolls)
        {
            if (windowStart == 0)
                windowStart = now;
            waits++;
            polls += spanPolls;
            totalNs += lengthNs;
            length.Add(lengthNs);
        }
        bool Due(int64_t now) const { return windowStart != 0 && now - windowStart >= kLogEveryNs; }

        std::string Report(const char* name, int64_t now) const
        {
            const double seconds = double(now - windowStart) / 1e9;
            return std::format("[handoff] {} {:.1f} s: {} waits, {:.1f} ms in all ({:.1f}%), us p50 {:.0f} p99 {:.0f} max {:.0f}, {} polls", name,
                seconds, waits, double(totalNs) / 1e6, seconds > 0 ? 100.0 * double(totalNs) / 1e9 / seconds : 0.0,
                double(length.PercentileNs(50)) / 1000.0, double(length.PercentileNs(99)) / 1000.0, double(length.maxNs) / 1000.0, polls);
        }
        void StartWindow(int64_t now) { *this = Window{ .windowStart = now }; }
    };
}
