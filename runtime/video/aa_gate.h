// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Whether the presenter's FXAA-style pass (video/presenter.cpp, kAaCs) runs
// for a frame. Pure functions, for tests/aa_gate_test.cpp.
//
// The pass runs at the frame image's size, before the scaling pass draws the
// picture into the window. When the picture is shrunk to two thirds of the
// image or less along both axes (3x Internal Resolution, 3840x2160, on a
// 1440p screen), the bicubic downscale already averages several source
// pixels into each screen pixel, and the AA pass costs about 0.5 ms a frame
// on an M5 at full clocks (measured on a MacBook Air M5, October 2026). The gate skips it
// then. A frame shown at its own size (3x on a 4K screen), enlarged, or shrunk
// less than 1.5x along either axis (2x on a 1080p or the MacBook Air's
// screen, 3x on the MacBook Air's) keeps it.
//
// RECARBON_AA_DOWNSCALE_GATE=1 turns the gate on; it is off by default, and
// then the pass runs whenever Settings > Graphics > Anti-Aliasing is on, as
// before. With Anti-Aliasing off, the pass never runs, gate or not.
#pragma once
#include <cstdint>

namespace video::aagate
{
    // The srcW x srcH image drawn at dstW x dstH is shrunk by 1.5x or more
    // along both axes (dst * 3 <= src * 2; exactly 1.5x counts). A size of
    // zero or less is not.
    inline bool ShrunkEnough(uint32_t srcW, uint32_t srcH, int32_t dstW, int32_t dstH)
    {
        if (!srcW || !srcH || dstW <= 0 || dstH <= 0)
            return false;
        return uint64_t(dstW) * 3 <= uint64_t(srcW) * 2 && uint64_t(dstH) * 3 <= uint64_t(srcH) * 2;
    }

    // The AA pass runs for this frame.
    inline bool RunAa(bool antiAliasing, bool gate, uint32_t srcW, uint32_t srcH, int32_t dstW, int32_t dstH)
    {
        return antiAliasing && !(gate && ShrunkEnough(srcW, srcH, dstW, dstH));
    }
}
