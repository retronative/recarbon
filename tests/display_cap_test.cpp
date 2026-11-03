// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// video/display_cap.h: the pixels of the window's display (the panel's
// native mode, the largest one-pixel-per-point mode, the backing store, and
// never more than the backing store) and the 4K cap on the internal
// resolution, on fake displays: a 2940x1912 Retina, 4K, 5K, 1080p, unknown,
// and macOS's scaled "More Space" and "Larger Text" modes; and the saved
// window size within the display at launch. Expected values
// are worked out from the rule, not from the code's output. Header only.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/display_cap_test.cpp -o build/display_cap_test && build/display_cap_test
#include <video/display_cap.h>

#include <cstdio>
#include <utility>
#include <vector>

using gpu::RenderScale;
using video::ModeSize;
using video::PixelSize;
using video::PixelSource;

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

    const RenderScale k1 = RenderScale::Whole(1, 1), k2 = RenderScale::Whole(2, 2), k3 = RenderScale::Whole(3, 3);
    const RenderScale k15 = { gpu::AxisFromHalves(3), gpu::AxisFromHalves(3) };

    video::DisplayPixelsFound Pixels(PixelSize native, std::vector<ModeSize> modes, ModeSize current)
    {
        return video::PhysicalPixels(native, modes, current);
    }

    // The display's pixels, each way they can be known.
    void Physical()
    {
        printf("---- the display's pixels ----\n");
        // MacBook Air M5 15": the log's 1470x956 points at 2x; its panel's
        // native mode as macOS flags it.
        auto air = Pixels({ 2940, 1912 }, { { 1470, 956, 2.0f }, { 1710, 1112, 2.0f } }, { 1470, 956, 2.0f });
        Check(air.pixels == PixelSize{ 2940, 1912 } && air.source == PixelSource::Panel, "Retina: the panel's 2940x1912");
        // A 4K monitor at its default (looks like 1920x1080 at 2x).
        auto uhd = Pixels({ 3840, 2160 }, {}, { 1920, 1080, 2.0f });
        Check(uhd.pixels == PixelSize{ 3840, 2160 }, "4K monitor at 1920x1080 points x2: 3840x2160");
        // A 5K display (Studio Display) at its default 2560x1440 at 2x.
        auto fiveK = Pixels({ 5120, 2880 }, {}, { 2560, 1440, 2.0f });
        Check(fiveK.pixels == PixelSize{ 5120, 2880 }, "5K: 5120x2880");
        // A 1080p monitor, no native flag (Linux): its largest mode.
        auto hd = Pixels({}, { { 1280, 720, 1.0f }, { 1920, 1080, 1.0f }, { 1600, 900, 1.0f } }, { 1920, 1080, 1.0f });
        Check(hd.pixels == PixelSize{ 1920, 1080 } && hd.source == PixelSource::Modes, "1080p: the largest 1x mode");
        // Nothing known (no display for the window, or between an unplug and the next one).
        auto none = Pixels({}, {}, {});
        Check(!none.pixels.Known() && none.source == PixelSource::Unknown, "unknown: 0x0");
        Check(!Pixels({}, {}, { 1920, 0, 1.0f }).pixels.Known() && !Pixels({}, {}, { 1920, 1080, 0.0f }).pixels.Known(),
            "a mode of no height or no density is unknown");
        // MacBook Pro 16" at More Space: 2056x1329 points at 2x is a
        // 4112x2658 backing store on a 3456x2234 panel.
        auto moreSpace = Pixels({ 3456, 2234 }, { { 2056, 1329, 2.0f } }, { 2056, 1329, 2.0f });
        Check(moreSpace.pixels == PixelSize{ 3456, 2234 }, "More Space on a MacBook Pro 16: the panel, not the 4112x2658 backing");
        // The same without the native flag: the 1x modes still list the panel,
        // the scaled HiDPI modes don't count.
        auto moreSpaceModes = Pixels({}, { { 3456, 2234, 1.0f }, { 2056, 1329, 2.0f }, { 1728, 1117, 2.0f } }, { 2056, 1329, 2.0f });
        Check(moreSpaceModes.pixels == PixelSize{ 3456, 2234 } && moreSpaceModes.source == PixelSource::Modes,
            "More Space, no native flag: the largest 1x mode, the HiDPI ones skipped");
        // Only the backing store known: it is taken as it is (the
        // documented limit: an over-large backing store is believed).
        auto backingOnly = Pixels({}, { { 2056, 1329, 2.0f } }, { 2056, 1329, 2.0f });
        Check(backingOnly.pixels == PixelSize{ 4112, 2658 } && backingOnly.source == PixelSource::Backing, "backing store only");
        // A 4K panel at Larger Text (1280x720 points at 2x): the window can
        // show 2560x1440 at most.
        auto larger = Pixels({ 3840, 2160 }, {}, { 1280, 720, 2.0f });
        Check(larger.pixels == PixelSize{ 2560, 1440 }, "4K panel at Larger Text: no more than the 2560x1440 backing store");
        // A 4K panel at More Space (2560x1440 points at 2x, 5120x2880 backing).
        auto uhdMore = Pixels({ 3840, 2160 }, {}, { 2560, 1440, 2.0f });
        Check(uhdMore.pixels == PixelSize{ 3840, 2160 }, "4K panel at More Space: the panel's 3840x2160");
        // A fractional density (a scaled mode SDL reports at 1.5).
        Check(video::ModePixels({ 1707, 960, 1.5f }) == PixelSize{ 2561, 1440 }, "1707x960 at 1.5: 2561x1440 (rounded)");
    }

    // The cap.
    void Cap()
    {
        printf("---- the 4K cap ----\n");
        using video::CapForDisplay;
        const PixelSize retina{ 2940, 1912 }, uhd{ 3840, 2160 }, fiveK{ 5120, 2880 }, hd{ 1920, 1080 }, unknown{};
        Check(CapForDisplay(k3, retina) == k2, "3x on a 2940x1912 Retina: 2x");
        Check(CapForDisplay(k3, uhd) == k3, "3x on 3840x2160: 3x");
        Check(CapForDisplay(k3, fiveK) == k3, "3x on 5K: 3x");
        Check(CapForDisplay(k3, hd) == k2, "3x on 1080p: 2x");
        Check(CapForDisplay(k3, unknown) == k3, "3x on an unknown display: 3x (no cap)");
        Check(CapForDisplay(k3, { 3840, 2159 }) == k2, "3x on 3840x2159: 2x");
        Check(CapForDisplay(k3, { 3839, 2160 }) == k2, "3x on 3839x2160: 2x");
        Check(CapForDisplay(k3, { 5120, 2160 }) == k3, "3x on a 5120x2160 ultrawide: 3x");
        Check(CapForDisplay(k3, { 3456, 2234 }) == k2, "3x on a MacBook Pro 16's panel: 2x");
        Check(CapForDisplay(k3, { 3840, 2400 }) == k3, "3x on 3840x2400 (16:10 4K): 3x");
        for (const PixelSize d : { retina, uhd, fiveK, hd, unknown })
            Check(CapForDisplay(k2, d) == k2 && CapForDisplay(k15, d) == k15 && CapForDisplay(k1, d) == k1, "2x, 1.5x, 1x unchanged");
        Check(CapForDisplay(RenderScale::Whole(3, 2), { 3440, 1440 }) == k2, "Auto's 3x2 on a 3440x1440 ultrawide: 2x2");
        Check(CapForDisplay(RenderScale::Whole(4, 4), retina) == k2, "4x below 4K: 2x");
        Check(CapForDisplay({ gpu::AxisFromHalves(3), { 3, 1 } }, retina) == RenderScale{ gpu::AxisFromHalves(3), { 2, 1 } },
            "1.5x3: only the axis above 2x");
        Check(video::Below4K(retina) && !video::Below4K(uhd) && !video::Below4K(unknown), "Below4K");
        // The More Space MacBook Pro 16 end to end: capped.
        Check(CapForDisplay(k3, Pixels({ 3456, 2234 }, {}, { 2056, 1329, 2.0f }).pixels) == k2, "More Space on a MacBook Pro 16: capped");
        Check(CapForDisplay(k3, Pixels({}, {}, {}).pixels) == k3, "nothing known: not capped");
    }

    // The saved window size at launch.
    void WindowSize()
    {
        printf("---- the saved window size at launch ----\n");
        auto clamp = [](int w, int h, int uw, int uh) {
            video::ClampWindowSize(w, h, uw, uh);
            return std::pair{ w, h };
        };
        // Saved on a 5K monitor (2560x1415 points usable), opened on the
        // M5's panel (1470x924 points usable under the menu bar).
        Check(clamp(2400, 1350, 1470, 924) == std::pair{ 1470, 924 }, "a 5K monitor's size on the laptop: the laptop's usable bounds");
        Check(clamp(1280, 720, 1470, 924) == std::pair{ 1280, 720 }, "a size that fits: kept");
        Check(clamp(1600, 720, 1470, 924) == std::pair{ 1470, 720 }, "only the width too large: only the width");
        Check(clamp(1280, 1000, 1470, 924) == std::pair{ 1280, 924 }, "only the height too large: only the height");
        Check(clamp(1470, 924, 1470, 924) == std::pair{ 1470, 924 }, "exactly the usable bounds: kept");
        Check(clamp(2400, 1350, 0, 0) == std::pair{ 2400, 1350 }, "unknown bounds: kept");
    }
}

namespace
{
    // The GPU's image limits: render targets and the swapchain.
    void Limits()
    {
        printf("---- the GPU's image limits ----\n");
        using video::FitImageLimit;
        const RenderScale k4 = RenderScale::Whole(4, 4);
        Check(FitImageLimit(k4, 2048, 16384) == k4, "4x on Apple's 16384: 8192 rows, kept");
        Check(FitImageLimit(k4, 2048, 8192) == k4, "4x at exactly 8192: kept");
        Check(FitImageLimit(k4, 2048, 4096) == k2, "4x on a 4096 GPU: 2x (8192 and 6144 rows don't fit)");
        Check(FitImageLimit(k3, 2048, 4096) == k2, "3x on a 4096 GPU: 2x");
        Check(FitImageLimit(k15, 2048, 4096) == k15, "1.5x on a 4096 GPU: 3072 rows, kept");
        Check(FitImageLimit(RenderScale::Whole(3, 2), 2048, 5000) == k2, "3x2 under 5000: 2x2, each axis on its own");
        Check(FitImageLimit({ { 3, 1 }, gpu::AxisFromHalves(3) }, 2048, 5000) == RenderScale{ { 2, 1 }, gpu::AxisFromHalves(3) },
            "3x1.5 under 5000: 2x1.5");
        Check(FitImageLimit(k4, 2048, 7000) == k3, "4x under 7000: 3x (6144 rows)");
        Check(FitImageLimit(k2, 2048, 3500) == k15, "2x under 3500: 1.5x (3072 rows)");
        Check(FitImageLimit(k3, 2048, 2000) == k1, "nothing fits: 1x");
        Check(FitImageLimit(k4, 2048, 0) == k4, "an unknown limit: kept");
        Check(FitImageLimit(k4, 4096, 16384) == k4 && FitImageLimit(k4, 4097, 16384) == k3, "a wider guest surface counts");

        using video::ClampExtent;
        const PixelSize lo{ 1, 1 }, hi{ 16384, 16384 };
        Check(ClampExtent({ 2940, 1846 }, lo, hi) == PixelSize{ 2940, 1846 }, "the M5's fullscreen extent: kept");
        Check(ClampExtent({ 20000, 1000 }, lo, hi) == PixelSize{ 16384, 1000 }, "wider than the GPU's limit: clamped");
        Check(ClampExtent({ 1000, 20000 }, lo, hi) == PixelSize{ 1000, 16384 }, "taller than the limit: clamped");
        Check(ClampExtent({ 0, 0 }, lo, hi) == PixelSize{} && ClampExtent({ 0, 900 }, lo, hi) == PixelSize{}, "0 (minimised): no extent");
        Check(ClampExtent({ 20000, 1000 }, {}, {}) == PixelSize{ 20000, 1000 }, "unknown limits: kept");
        Check(ClampExtent({ 100, 100 }, { 200, 200 }, hi) == PixelSize{ 200, 200 }, "below the minimum: the minimum");
    }
}

int main()
{
    Physical();
    Cap();
    WindowSize();
    Limits();
    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
