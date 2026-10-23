// From SpeedBreaker v0.1.8's tests/picture_fit_test.cpp (GPL-3.0-or-later).
// ReCarbon changes: Carbon has no view-scaling hooks (game/title_hooks.cpp,
// linked here: HooksActive() and TallHooksActive() are false), so the
// presenter's new-frame fit (presenter.cpp: FitPicture(..., fillMode &&
// game::HooksActive(), fillMode && game::TallHooksActive())) letterboxes or
// pillarboxes the 16:9 frame on every screen, whatever Aspect Ratio says.
// Upstream's Fill Screen cases (Hor+, Vert+, a 4:3 frame redrawn) need MW's
// hooks and aren't ported. Kept: the letterbox cases, the 1, 1 redraw, and
// Auto internal resolution at the budgets a Mac gets (renderer.cpp: 4 on
// Apple's integrated GPUs, 9 on an M3 or later Max or Ultra; upstream's 6 for
// a discrete GPU and the iOS budgets don't occur), plus the picture sizes and
// choices logged on the MacBook Air M5 and M1 (2026-10-23,
// reference/FEASIBILITY.md). (At those budgets, which axis Auto cuts first
// when both are over doesn't change its answer.) Not covered: the budget's
// choice itself, in renderer.cpp, and
// presenter.cpp's FrameRegion, which sizes Auto for a filled picture
// (FitPicture with fillMode for both) even though Carbon's is letterboxed.
//
// Tests for video::FitPicture, PlacePicture and AutoRenderScale
// (runtime/video/picture_fit.cpp). From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/picture_fit_test.cpp runtime/video/picture_fit.cpp runtime/game/title_hooks.cpp -o build/picture_fit_test
//   build/picture_fit_test
#include <game/ultrawide.h>
#include <video/picture_fit.h>

#include <cmath>
#include <cstdio>
#include <initializer_list>

using video::AutoRenderScale;
using video::FitPicture;
using video::PictureFit;
using video::PlacePicture;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

// The letterbox from before Vert+, exactly.
static void Letterbox(uint32_t w, uint32_t h, float frameW, float frameH, const PictureFit& f, const char* what)
{
    float scale = std::fmin(float(w) / frameW, float(h) / frameH);
    int32_t pw = int32_t(frameW * scale), ph = int32_t(frameH * scale);
    CHECK(f.scaleX == 1.0f && f.scaleY == 1.0f, "%s %ux%u: scale x %.4f y %.4f", what, w, h, f.scaleX, f.scaleY);
    CHECK(f.width == pw && f.height == ph, "%s %ux%u: picture %dx%d, want %dx%d", what, w, h, f.width, f.height, pw, ph);
}

static void Scale(uint32_t regionW, uint32_t regionH, uint32_t budget, uint32_t wantX, uint32_t wantY)
{
    uint32_t sx = 0, sy = 0;
    AutoRenderScale(regionW, regionH, budget, sx, sy);
    printf("render scale for %ux%u, budget %u: %ux%u\n", regionW, regionH, budget, sx, sy);
    CHECK(sx == wantX && sy == wantY, "%ux%u budget %u: %ux%u, want %ux%u", regionW, regionH, budget, sx, sy, wantX, wantY);
}

int main()
{
    // ReCarbon: the premise of everything below.
    printf("Carbon's view-scaling hooks:\n");
    CHECK(!game::HooksActive() && !game::TallHooksActive(), "title_hooks.cpp reports hooks active: Fill Screen would stretch the frame");

    printf("A new frame as the presenter fits it, Aspect Ratio on Auto and on 16:9: letterboxed:\n");
    // 16:9, within 1% of it, 16:10 (the Macs' built-in panels, fullscreen and
    // windowed as logged, and the M5's in points, whose box is 826.875 rows:
    // cut, not rounded), ultrawide, 3:2, 4:3, 5:4, portrait.
    const uint32_t screens[][2] = { { 1280, 720 }, { 1366, 768 }, { 1360, 768 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 },
        { 2940, 1846 }, { 2940, 1782 }, { 2880, 1800 }, { 2880, 1676 }, { 1470, 956 }, { 3024, 1890 }, { 2560, 1600 }, { 2560, 1080 },
        { 3440, 1440 }, { 2160, 1440 }, { 1024, 768 }, { 1280, 1024 }, { 720, 1280 } };
    for (const auto& s : screens)
        for (bool fillMode : { true, false })
        {
            const char* what = fillMode ? "Auto" : "16:9";
            Letterbox(s[0], s[1], 1280.0f, 720.0f,
                FitPicture(s[0], s[1], 1280.0f, 720.0f, fillMode && game::HooksActive(), fillMode && game::TallHooksActive()), what);
            // The frame's shape, not its size, decides: a 2x2 or 3x2 image.
            Letterbox(s[0], s[1], 2560.0f, 1440.0f,
                FitPicture(s[0], s[1], 2560.0f, 1440.0f, fillMode && game::HooksActive(), fillMode && game::TallHooksActive()), what);
            Letterbox(s[0], s[1], 3840.0f, 1440.0f, FitPicture(s[0], s[1], 3840.0f, 1440.0f, false, false), "16:9 (3x2 image)");
        }

    printf("Redraws (no hook ever sets a view scale, so always 1, 1):\n");
    for (const auto& s : screens)
        Letterbox(s[0], s[1], 1280.0f, 720.0f, PlacePicture(s[0], s[1], 1280.0f, 720.0f, 1.0f, 1.0f), "redraw 1, 1");

    printf("Auto internal resolution:\n");
    Scale(1280, 720, 4, 1, 1);
    Scale(1280, 720, 9, 1, 1);
    Scale(1024, 768, 4, 1, 1);
    Scale(3024, 1890, 4, 2, 2);   // a filled 16:10 picture: 3x3 cut to 2x2
    Scale(3024, 1701, 4, 2, 2);   // its 16:9 box
    Scale(3440, 1440, 4, 2, 2);   // a filled ultrawide picture: 3x2 cut to 2x2
    Scale(3440, 1440, 9, 3, 2);   // the same on an M3 or later Max
    Scale(3840, 2160, 4, 2, 2);   // 4K: 3x3 cut to 2x2
    Scale(3840, 2160, 9, 3, 3);   // 4K on an M3 or later Max
    Scale(1365, 768, 4, 2, 2);    // a letterboxed 16:9 box, a pixel narrower than 16:9
    Scale(1280, 960, 4, 1, 2);    // a 4:3 region
    Scale(2560, 1600, 4, 2, 2);   // a filled 16:10 picture: as its 16:9 box, not 2x3
    // ReCarbon: as logged ("auto for a WxH picture", budget 4 on both Macs).
    Scale(2560, 1440, 4, 2, 2);   // M5 and M1, a 2560x1440 window
    Scale(2940, 1846, 4, 2, 2);   // M5 fullscreen
    Scale(2940, 1782, 4, 2, 2);   // M5, a window as large as the screen allows
    Scale(2880, 1782, 4, 2, 2);   // M5, a 1440x900 window request
    Scale(2880, 1800, 4, 2, 2);   // M1 fullscreen

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
