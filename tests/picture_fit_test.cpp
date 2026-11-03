// From SpeedBreaker v0.1.8's tests/picture_fit_test.cpp (GPL-3.0-or-later).
// ReCarbon changes: Carbon's view-scaling hooks are Vert+ only and opt-in
// (RECARBON_VERT_PLUS=1, game/vert_plus.h, whose State the presenter's
// game::HooksActive()/TallHooksActive() report). The presenter's new-frame
// fit is FitPicture(..., fillMode && game::HooksActive(), fillMode &&
// game::TallHooksActive()). Off (the default), both are false and the 16:9
// frame is letterboxed or pillarboxed on every screen, whatever Aspect Ratio
// says, as before Vert+. On, once the projection hook has run, a screen
// narrower than 16:9 is filled (Vert+; the expected scales and picture sizes
// below are worked by hand from screen aspect / (16/9), clamped at 4:3) and a
// wider one is still pillarboxed (no Hor+). Upstream's Hor+ cases and its
// 4:3 frame redrawn aren't ported. Kept: the letterbox cases, the 1, 1
// redraw, and Auto internal resolution at the budgets a Mac gets
// (renderer.cpp: 4 on Apple's integrated GPUs, 9 on an M3 or later Max or
// Ultra; upstream's 6 for a discrete GPU and the iOS budgets don't occur),
// plus the picture sizes and choices logged on the MacBook Air M5 and M1
// (2026-10-23, reference/FEASIBILITY.md). (At those budgets, which axis Auto
// cuts first when both are over doesn't change its answer.) Not covered: the
// budget's choice itself, in renderer.cpp, and presenter.cpp's FrameRegion,
// which sizes Auto for a filled picture (FitPicture with fillMode for both).
//
// Tests for video::FitPicture, PlacePicture and AutoRenderScale
// (runtime/video/picture_fit.cpp). From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/picture_fit_test.cpp runtime/video/picture_fit.cpp -o build/picture_fit_test
//   build/picture_fit_test
#include <game/vert_plus.h>
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

// A filled picture: the view scales and the picture's size, worked by hand.
static void Filled(uint32_t w, uint32_t h, const PictureFit& f, float wantY, int32_t wantW, int32_t wantH)
{
    CHECK(f.scaleX == 1.0f && std::fabs(f.scaleY - wantY) <= 1e-4f, "Vert+ %ux%u: scale x %.4f y %.5f, want 1 and %.5f", w, h, f.scaleX, f.scaleY, wantY);
    CHECK(f.width == wantW && f.height == wantH, "Vert+ %ux%u: picture %dx%d, want %dx%d", w, h, f.width, f.height, wantW, wantH);
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
    // ReCarbon: Vert+ off (RECARBON_VERT_PLUS unset): the hooks report
    // nothing, even after the game ran them.
    game::vert_plus::State off(false);
    float p00 = 1.0f, p11 = 1.0f;
    off.Projection(1, p00, p11);
    printf("Vert+ off:\n");
    CHECK(!off.HooksActive() && !off.TallHooksActive(), "Vert+ off reports hooks active: Fill Screen would stretch the frame");

    printf("A new frame as the presenter fits it, Vert+ off, Aspect Ratio on Auto and on 16:9: letterboxed:\n");
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
                FitPicture(s[0], s[1], 1280.0f, 720.0f, fillMode && off.HooksActive(), fillMode && off.TallHooksActive()), what);
            // The frame's shape, not its size, decides: a 2x2 or 3x2 image.
            Letterbox(s[0], s[1], 2560.0f, 1440.0f,
                FitPicture(s[0], s[1], 2560.0f, 1440.0f, fillMode && off.HooksActive(), fillMode && off.TallHooksActive()), what);
            Letterbox(s[0], s[1], 3840.0f, 1440.0f, FitPicture(s[0], s[1], 3840.0f, 1440.0f, false, false), "16:9 (3x2 image)");
        }

    // ReCarbon: Vert+ on (RECARBON_VERT_PLUS=1): no hooks before the
    // projection hook has run (the boot movies), Vert+ after it, never Hor+.
    game::vert_plus::State on(true);
    printf("Vert+ on:\n");
    CHECK(!on.HooksActive() && !on.TallHooksActive(), "Vert+ on reports hooks before the projection hook ran");
    on.Projection(1, p00, p11);
    CHECK(!on.HooksActive() && on.TallHooksActive(), "Vert+ on, after the projection hook: Hor+ %d Vert+ %d, want 0 and 1",
        on.HooksActive(), on.TallHooksActive());

    printf("A new frame, Vert+ on, Aspect Ratio on Auto: filled below 16:9, down to 4:3:\n");
    // screen aspect / (16/9): 2940/1846 = 1.592633 -> 0.895856; 16:10 ->
    // 0.9; 3:2 -> 0.84375; 4:3 -> 0.75; 5:4 (1.25) and portrait (0.5625)
    // are clamped to 4:3 (0.75): a 1280x960 and a 720x540 picture.
    const struct
    {
        uint32_t w, h;
        float scaleY;
        int32_t pw, ph;
    } tall[] = { { 2940, 1846, 0.895856f, 2940, 1846 }, { 2880, 1800, 0.9f, 2880, 1800 }, { 2560, 1600, 0.9f, 2560, 1600 },
        { 3024, 1890, 0.9f, 3024, 1890 }, { 2160, 1440, 0.84375f, 2160, 1440 }, { 1024, 768, 0.75f, 1024, 768 },
        { 1280, 1024, 0.75f, 1280, 960 }, { 720, 1280, 0.75f, 720, 540 } };
    for (const auto& t : tall)
    {
        Filled(t.w, t.h, FitPicture(t.w, t.h, 1280.0f, 720.0f, on.HooksActive(), on.TallHooksActive()), t.scaleY, t.pw, t.ph);
        Filled(t.w, t.h, FitPicture(t.w, t.h, 2560.0f, 1440.0f, on.HooksActive(), on.TallHooksActive()), t.scaleY, t.pw, t.ph);
        // Aspect Ratio on 16:9: letterboxed.
        Letterbox(t.w, t.h, 1280.0f, 720.0f, FitPicture(t.w, t.h, 1280.0f, 720.0f, false, false), "Vert+ on, 16:9");
    }
    // 16:9, within 1% of it, and wider screens (no Hor+): as before.
    const uint32_t notTall[][2] = { { 1920, 1080 }, { 1366, 768 }, { 1360, 768 }, { 2560, 1440 }, { 2560, 1080 }, { 3440, 1440 } };
    for (const auto& s : notTall)
        Letterbox(s[0], s[1], 1280.0f, 720.0f, FitPicture(s[0], s[1], 1280.0f, 720.0f, on.HooksActive(), on.TallHooksActive()), "Vert+ on, Auto");

    printf("Redraws:\n");
    for (const auto& s : screens)
        Letterbox(s[0], s[1], 1280.0f, 720.0f, PlacePicture(s[0], s[1], 1280.0f, 720.0f, 1.0f, 1.0f), "redraw 1, 1");
    // A frame rendered for 2940x1846 redrawn after the window became
    // 2940x1653 (16:9): the frame keeps its shape (aspect 1.592633), as tall
    // as the screen: 1653 x 1.592633 = 2632.6 -> 2633x1653.
    Filled(2940, 1653, PlacePicture(2940, 1653, 1280.0f, 720.0f, 1.0f, 0.895856f), 0.895856f, 2633, 1653);

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
