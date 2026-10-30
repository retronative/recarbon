// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// video/aa_gate.h: the presenter's AA pass is skipped only with
// RECARBON_AA_DOWNSCALE_GATE=1 and a picture shrunk by 1.5x or more along
// both axes; native size, enlarged and lesser shrinks keep it; with the gate
// off it runs exactly when Anti-Aliasing is on. Picture sizes are the 16:9
// frame fitted into the window (letterboxed or pillarboxed), as the presenter
// draws them. Header only: no Vulkan.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/aa_gate_test.cpp -o build/aa_gate_test && build/aa_gate_test
#include <video/aa_gate.h>

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

    using video::aagate::RunAa;
    using video::aagate::ShrunkEnough;

    // With the gate on and Anti-Aliasing on.
    bool Runs(uint32_t w, uint32_t h, int32_t dw, int32_t dh) { return RunAa(true, true, w, h, dw, dh); }
}

int main()
{
    printf("---- 3x (3840x2160) ----\n");
    Check(!Runs(3840, 2160, 2560, 1440), "on a 1440p screen (exactly 1.5x): skipped");
    Check(!Runs(3840, 2160, 1920, 1080), "on a 1080p screen (2x): skipped");
    Check(Runs(3840, 2160, 3840, 2160), "on a 4K screen (1:1): kept");
    Check(Runs(3840, 2160, 2940, 1654), "on the MacBook Air M5's screen (1.31x): kept");
    Check(Runs(3840, 2160, 3024, 1701), "on a 14-inch MacBook Pro's screen (1.27x): kept");
    Check(Runs(3840, 2160, 5120, 2880), "on a 5K screen (enlarged): kept");

    printf("---- 2x (2560x1440) and 1x (1280x720) ----\n");
    Check(Runs(2560, 1440, 2560, 1440), "2x on a 1440p screen (1:1): kept");
    Check(Runs(2560, 1440, 1920, 1080), "2x on a 1080p screen (1.33x): kept");
    Check(Runs(2560, 1440, 2940, 1654), "2x on the MacBook Air M5's screen (enlarged): kept");
    Check(Runs(2560, 1440, 3840, 2160), "2x on a 4K screen (enlarged): kept");
    Check(Runs(1280, 720, 1280, 720), "1x in a 1280x720 window: kept");
    Check(Runs(1280, 720, 2940, 1654), "1x on the MacBook Air M5's screen: kept");
    Check(Runs(1280, 720, 3840, 2160), "1x on a 4K screen: kept");

    printf("---- the 1.5x boundary ----\n");
    Check(ShrunkEnough(3840, 2160, 2560, 1440), "exactly 1.5x along both axes");
    Check(!ShrunkEnough(3840, 2160, 2561, 1440), "a pixel wider: 1.4994x across");
    Check(!ShrunkEnough(3840, 2160, 2560, 1441), "a pixel taller: 1.4990x down");
    Check(ShrunkEnough(3840, 2160, 2559, 1439), "a pixel smaller along both");
    Check(ShrunkEnough(3, 3, 2, 2), "3 to 2: exactly 1.5x");
    Check(!ShrunkEnough(3, 3, 3, 2), "3 to 3 across: not shrunk");
    Check(!ShrunkEnough(2560, 1440, 1707, 960), "2x into 1707x960: 1.4997x across");
    Check(ShrunkEnough(2560, 1440, 1706, 960), "2x into 1706x960: just past 1.5x");

    printf("---- aspect ratios ----\n");
    Check(!Runs(3840, 2160, 2560, 1440), "3x pillarboxed on a 3440x1440 ultrawide (2560x1440): skipped");
    Check(Runs(3840, 2160, 3440, 1935), "3x letterboxed in a 3440x2160 window (1.12x): kept");
    Check(!Runs(3840, 2160, 2560, 1440), "3x letterboxed on a 2560x1600 screen (2560x1440): skipped");
    Check(Runs(3840, 2160, 2560, 1600), "stretched to 2560x1600 (1.35x down): kept");
    Check(Runs(3840, 2160, 2000, 1500), "1.92x across, 1.44x down: kept");
    Check(Runs(3840, 2160, 2600, 1000), "1.48x across, 2.16x down: kept");
    Check(!Runs(3840, 2160, 2000, 1400), "1.92x across, 1.54x down: skipped");
    Check(!Runs(3840, 2400, 2560, 1600), "a 16:10 frame at 1.5x: skipped");

    printf("---- resolution changes (one decision per frame, no state) ----\n");
    {
        struct Frame { uint32_t w, h; int32_t dw, dh; bool runs; const char* what; };
        const Frame frames[] = {
            { 3840, 2160, 2560, 1440, false, "3x at 1440p" },
            { 3840, 2160, 3840, 2160, true, "the window moved to a 4K screen" },
            { 3840, 2160, 2560, 1440, false, "and back" },
            { 2560, 1440, 2560, 1440, true, "Internal Resolution 2x" },
            { 3840, 2160, 2560, 1440, false, "3x again" },
            { 3840, 2160, 1280, 720, false, "a small window" },
            { 3840, 2160, 2940, 1654, true, "resized to the MacBook Air M5's screen" },
            { 1280, 720, 1280, 720, true, "1x (a movie, the front end)" },
        };
        for (const Frame& f : frames)
            Check(Runs(f.w, f.h, f.dw, f.dh) == f.runs, f.what);
    }

    printf("---- degenerate sizes: the pass runs as before ----\n");
    Check(Runs(0, 2160, 2560, 1440), "no image width");
    Check(Runs(3840, 0, 2560, 1440), "no image height");
    Check(Runs(3840, 2160, 0, 1440), "no picture width (a minimised window)");
    Check(Runs(3840, 2160, 2560, 0), "no picture height");
    Check(Runs(3840, 2160, -2560, 1440), "a negative picture width");
    Check(!ShrunkEnough(0, 0, 0, 0), "nothing at all");
    Check(ShrunkEnough(0xFFFFFFFFu, 0xFFFFFFFFu, 0x7FFFFFFF, 0x7FFFFFFF), "the largest sizes, 2x: no overflow");
    Check(ShrunkEnough(0x7FFFFFFFu, 0x7FFFFFFFu, 1431655764, 1431655764), "2147483647 to 1431655764: 1.5x, past 32 bits");
    Check(!ShrunkEnough(0x7FFFFFFFu, 0x7FFFFFFFu, 1431655765, 1431655765), "one more: under 1.5x");
    Check(!ShrunkEnough(0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFF, 0x7FFFFFFF), "the largest picture at 1:1 (3x its size wraps 32 bits)");

    printf("---- opt-out: the gate off, or Anti-Aliasing off ----\n");
    {
        const uint32_t sizes[][4] = {
            { 3840, 2160, 2560, 1440 }, { 3840, 2160, 1920, 1080 }, { 3840, 2160, 3840, 2160 },
            { 2560, 1440, 1920, 1080 }, { 1280, 720, 3840, 2160 }, { 3840, 2160, 0, 0 },
        };
        bool gateOffRuns = true, aaOffSkips = true;
        for (const auto& s : sizes)
        {
            gateOffRuns = gateOffRuns && RunAa(true, false, s[0], s[1], int32_t(s[2]), int32_t(s[3]));
            aaOffSkips = aaOffSkips && !RunAa(false, false, s[0], s[1], int32_t(s[2]), int32_t(s[3]))
                && !RunAa(false, true, s[0], s[1], int32_t(s[2]), int32_t(s[3]));
        }
        Check(gateOffRuns, "the gate off: the pass runs at every size, as before");
        Check(aaOffSkips, "Anti-Aliasing off: the pass never runs, gate or not");
    }

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
