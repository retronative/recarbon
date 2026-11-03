// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_GAME_TIME_LOG=<file> (a diagnostic, off by default): one CSV line
// per game frame with what Carbon's frame did with its time
// (game/game_time.cpp has the hooks; config/recarbon.toml the sites), and a
// [gametime] summary every kLogEveryNs. The columns:
//   host_ns      the frame's start, the host's steady clock in ns (the clock
//                of RECARBON_FRAME_LOG's `presented`, so the two merge)
//   measured_ms  the time the main loop measured and handed the frame
//   ticks        the quantiser's whole 1/3600 s ticks for the frame
//   frame_ms     the frame's seconds after the quantiser (what the world gets)
//   steps        the world steps of 1/60 s the frame ran: -1 the world didn't
//                run (front end, loading, paused), 0 below one step, else
//                1-8 (raw_steps raised to 1 and capped at 8, as the game does)
//   raw_steps    the stepper's truncated count before that (-1 / 0 as steps)
//   acc_ms       the stepper's accumulator with this frame's time added,
//                before the steps are taken off (-1: the world didn't run)
// Pure, for tests/game_time_log_test.cpp.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <format>
#include <string>

namespace game::timelog
{
    constexpr int64_t kLogEveryNs = 10'000'000'000;
    constexpr const char* kHeader = "host_ns,measured_ms,ticks,frame_ms,steps,raw_steps,acc_ms\n";

    struct Frame
    {
        int64_t hostNs = 0;
        double measuredMs = 0, frameMs = 0, accMs = -1;
        int32_t ticks = 0;
        int steps = -1, rawSteps = -1;
    };

    // The game's own rule after its `cmpwi cr6,r11,1`: at least 1, at most 8.
    inline int ClampSteps(int raw)
    {
        return std::clamp(raw, 1, 8);
    }

    // The frame's line, with its newline; `out` holds at least 128 bytes.
    inline size_t FormatRow(const Frame& f, char* out, size_t size)
    {
        int n = snprintf(out, size, "%lld,%.3f,%d,%.3f,%d,%d,%.3f\n", (long long)f.hostNs, f.measuredMs, f.ticks, f.frameMs, f.steps, f.rawSteps,
            f.accMs);
        return n < 0 ? 0 : std::min(size_t(n), size - 1);
    }

    // The frames of a window: the world's steps per frame and the mean ticks.
    struct Summary
    {
        uint64_t frames = 0, noWorld = 0, steps[4] = {};  // 0, 1, 2, 3+ steps
        int64_t ticks = 0, windowStart = 0;
        double measuredMs = 0;

        void Add(const Frame& f)
        {
            if (windowStart == 0)
                windowStart = f.hostNs;
            frames++;
            ticks += f.ticks;
            measuredMs += f.measuredMs;
            if (f.steps < 0)
                noWorld++;
            else
                steps[std::min(f.steps, 3)]++;
        }
        bool Due(int64_t now) const { return windowStart != 0 && now - windowStart >= kLogEveryNs; }

        std::string Report(int64_t now) const
        {
            const double n = double(std::max<uint64_t>(frames, 1));
            return std::format("[gametime] {:.1f} s: {} frames ({} without the world): {} with no step, {} with 1, {} with 2, {} with 3+; "
                               "mean {:.2f} ticks, {:.3f} ms measured",
                double(now - windowStart) / 1e9, frames, noWorld, steps[0], steps[1], steps[2], steps[3], double(ticks) / n, measuredMs / n);
        }
        void StartWindow(int64_t now) { *this = Summary{ .windowStart = now }; }
    };
}
