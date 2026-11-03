// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
// ReCarbon: implemented by game/game_time.cpp, which counts them only while
// RECARBON_GAME_TIME_LOG is on (0 otherwise, as before), and never gives a
// frame whole vblanks (snapped stays 0).
//
// The game's frame statistics for the command processor's [perf] line. In
// SpeedBreaker MW's main-loop hooks counted them.
#pragma once
#include <cstdint>

namespace game
{
    // Since launch, for the [perf] line: game frames (main loop iterations),
    // those whose world ran no step or two and more, and those given whole
    // vblanks of time.
    struct FrameTimeStats
    {
        uint64_t frames, noStep, multiStep, snapped;
    };
    FrameTimeStats GetFrameTimeStats();
}
