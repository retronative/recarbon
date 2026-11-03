// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_RQ_LOG=1: the render queue's exposure counters (game/render_queue.cpp counts,
// tests/render_queue_log_test.cpp checks the lines). Guest- and Vulkan-free.
//
// The ARM64 races of the deferred render queue (render_queue.cpp) can only happen while the
// recording thread records a frame and the main thread plays it back: a recording window. A soak
// is measured in those windows, not in minutes, so this counts them (each handshake set at the
// start of a recording), the entries published and the entries played, and logs one line every
// 10 s and the totals at exit. Published minus played is what was recorded and never played (a
// window cut short) plus what is in flight at that moment. Off by default.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace game::renderqueue
{
    constexpr int64_t kLogIntervalNs = 10'000'000'000;

    struct Counts
    {
        uint64_t windows = 0;    // recordings started (the handshake set)
        uint64_t publishes = 0;  // entries published into the queue
        uint64_t played = 0;     // entries the main thread played back
    };

    inline Counts operator-(const Counts& a, const Counts& b)
    {
        return { a.windows - b.windows, a.publishes - b.publishes, a.played - b.played };
    }

    // On for any value but empty and "0", as the other RECARBON_*_LOG switches.
    inline bool LogEnabled(const char* value)
    {
        return value && *value && std::strcmp(value, "0") != 0;
    }

    inline std::string Describe(const Counts& c)
    {
        char s[160];
        // Played can run ahead of publishes by what was read between the two loads.
        long long unplayed = (long long)(c.publishes - c.played);
        snprintf(s, sizeof s, "%llu recording windows, %llu publishes, %llu entries played (%lld not played)",
            (unsigned long long)c.windows, (unsigned long long)c.publishes, (unsigned long long)c.played, unplayed);
        return s;
    }

    // The line for the interval since the last one: its counts, then the totals since launch.
    inline std::string IntervalLine(double seconds, const Counts& interval, const Counts& total)
    {
        return "[rq] " + std::to_string(int(seconds + 0.5)) + " s: " + Describe(interval) + "; since launch " + Describe(total);
    }

    inline std::string TotalLine(double seconds, const Counts& total)
    {
        return "[rq] total over " + std::to_string(int(seconds + 0.5)) + " s: " + Describe(total);
    }
}
