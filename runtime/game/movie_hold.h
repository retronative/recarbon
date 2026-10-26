// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Movies on whole pairs of vblanks: which movie frames wait one vblank more (game/movie_pacing.cpp
// applies it; tests/movie_hold_test.cpp checks it). Vulkan- and guest-free.
//
// NFS: Carbon's movies (29.97 fps) swap with interval 1, and D3D's swap callback (sub_826E3378)
// releases each frame at the vblank after the one current when the callback runs (target = count
// + 1: all 1281 callbacks of a probe, 2026-10-26). The callbacks come +-4.4 ms (p5..p95) about
// their ideal times, so while the movie's phase is near a vblank, frames fall either side of it
// and show for 1 and 3 vblanks instead of 2 and 2: 15.6% of movie frames on a MacBook Air M5, 18.5%
// on screen in another run. A frame due one vblank after the last release, whose callback came
// within the guard (10 ms) before that vblank, is held to the next one: such an early callback is
// the scatter, not the movie running ahead. A late frame can't be helped, so a 3 stays a 3. Only
// movie frames (the XMV player rendered since the last swap), after four of two vblanks or more in
// a row, at a vblank period near 60 Hz, and not while suspended: the game's own frames are never
// touched. SpeedBreaker's runtime/game/movie_pacing.cpp does the same for Most Wanted's countdown.
#pragma once
#include <algorithm>
#include <cstdint>

namespace game::movie
{
    constexpr uint32_t kSteadyFrames = 4;  // movie frames of 2+ vblanks before any hold
    constexpr double kMinPeriodNs = 1e9 / 61.5, kMaxPeriodNs = 1e9 / 58.5;  // RefreshFit's range

    struct Tracker
    {
        uint32_t lastReleased = 0;  // the release vblank before the previous swap's
        bool lastWasMovie = false;  // the previous swap was a movie frame
        uint32_t steady = 0;        // consecutive movie frames of 2+ vblanks
        uint64_t frames = 0, spans[5] = {}, held = 0;
    };

    // At each swap callback, before the hold: `released` is the vblank that released the previous
    // swap, so the frame before it showed for released - lastReleased vblanks. True when this swap
    // ends a movie (its counts are then reset, after the caller logs them).
    inline bool Record(Tracker& t, bool movie, uint32_t released)
    {
        if (t.lastWasMovie && t.lastReleased)
        {
            uint32_t span = released - t.lastReleased;
            t.frames++;
            t.spans[std::min<uint32_t>(span, 4)]++;
            t.steady = span >= 2 ? t.steady + 1 : 0;
        }
        bool ended = !movie && t.lastWasMovie;
        t.lastReleased = released;
        t.lastWasMovie = movie;
        if (ended)
        {
            t.frames = t.held = 0;
            std::fill(std::begin(t.spans), std::end(t.spans), 0);
            t.steady = 0;
        }
        return ended;
    }

    struct Swap
    {
        uint32_t interval;      // the callback's (argument >> 8) & 15
        uint32_t count;         // the device's vblank count when it ran
        uint32_t released;      // the vblank that released the previous swap
        uint32_t target;        // the vblank it chose for this one
        double sinceVblankNs;   // time since the last guest vblank
        double periodNs;        // the guest vblank period
        double guardNs;         // 0: off
        bool suspended = false; // or no vblank since the resume
    };

    inline bool ShouldHold(const Tracker& t, bool movie, const Swap& s)
    {
        return movie && s.guardNs > 0 && s.interval == 1 && t.steady >= kSteadyFrames && !s.suspended
            && s.target == s.count + 1 && s.target == s.released + 1
            && s.periodNs >= kMinPeriodNs && s.periodNs <= kMaxPeriodNs && s.sinceVblankNs >= s.periodNs - s.guardNs;
    }
}
