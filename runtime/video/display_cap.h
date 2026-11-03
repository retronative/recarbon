// ReCarbon (v0.1.4). GPL-3.0-or-later (see COPYING).
//
// The pixels of the display the game's window is on, and the internal
// resolution they allow. Pure functions (tests/display_cap_test.cpp).
//
// The display's pixels: its panel's native size (macOS's native mode flag,
// else the largest mode of one pixel per point), no more than its current
// mode's backing store (points x pixel density). So a "More Space" mode,
// whose backing store is larger than the panel (a MacBook Pro 16's 2056x1329
// points at 2x: 4112x2658 on a 3456x2234 panel), doesn't count as more pixels
// than the panel has, and a "Larger Text" mode on a 4K panel (1280x720 points
// at 2x: 2560x1440) counts as what the window can show. Nothing known: 0x0.
//
// The 4K cap: on a display below 3840x2160 pixels (either axis), an axis
// above 2x renders at 2x. 4K UHD (3x) there would be downscaled to the
// display, so it costs the GPU about twice QHD's time for pixels thrown
// away. The stored setting is kept (it applies again on a 4K display), an
// unknown display is not capped, and RECARBON_RENDER_SCALE (a developer
// override) never goes through here.
#pragma once
#include <gpu/render_scale.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace video
{
    struct PixelSize
    {
        uint32_t width = 0, height = 0;

        bool operator==(const PixelSize&) const = default;
        bool Known() const { return width > 0 && height > 0; }
    };

    // An SDL_DisplayMode's size: points, and pixels per point.
    struct ModeSize
    {
        int w = 0, h = 0;
        float density = 1.0f;
    };

    inline PixelSize ModePixels(const ModeSize& m)
    {
        if (m.w <= 0 || m.h <= 0 || !(m.density > 0.0f))
            return {};
        return { uint32_t(std::lround(double(m.w) * m.density)), uint32_t(std::lround(double(m.h) * m.density)) };
    }

    // Where the display's pixels came from, for the log.
    enum class PixelSource
    {
        Unknown,
        Panel,    // the panel's native mode (capped by the backing store when that is smaller)
        Modes,    // the largest mode of one pixel per point (capped likewise)
        Backing,  // the current mode's backing store alone
    };

    inline const char* PixelSourceName(PixelSource s)
    {
        switch (s)
        {
        case PixelSource::Panel: return "the panel's native mode";
        case PixelSource::Modes: return "the largest display mode";
        case PixelSource::Backing: return "the current display mode";
        default: return "unknown";
        }
    }

    struct DisplayPixelsFound
    {
        PixelSize pixels;
        PixelSource source = PixelSource::Unknown;
    };

    // `native`: the panel's native pixels where the system says (macOS),
    // else 0x0. `modes`: the display's modes (SDL_GetFullscreenDisplayModes).
    // `current`: its current mode (SDL_GetCurrentDisplayMode), w = 0 if none.
    inline DisplayPixelsFound PhysicalPixels(PixelSize native, std::span<const ModeSize> modes, ModeSize current)
    {
        PixelSize panel = native;
        PixelSource source = PixelSource::Panel;
        if (!panel.Known())
        {
            source = PixelSource::Modes;
            for (const ModeSize& m : modes)
            {
                if (std::fabs(m.density - 1.0f) > 0.01f)
                    continue;
                const PixelSize p = ModePixels(m);
                if (p.Known() && uint64_t(p.width) * p.height > uint64_t(panel.width) * panel.height)
                    panel = p;
            }
        }
        const PixelSize backing = ModePixels(current);
        if (!panel.Known())
            return backing.Known() ? DisplayPixelsFound{ backing, PixelSource::Backing } : DisplayPixelsFound{};
        if (backing.Known())
            panel = { std::min(panel.width, backing.width), std::min(panel.height, backing.height) };
        return { panel, source };
    }

    constexpr PixelSize k4KPixels{ 3840, 2160 };

    // A known display below 4K UHD on either axis.
    inline bool Below4K(PixelSize display)
    {
        return display.Known() && (display.width < k4KPixels.width || display.height < k4KPixels.height);
    }

    // The internal resolution on that display: above 2x on an axis is 2x
    // below 4K (3x: QHD's 2560x1440), as asked elsewhere.
    inline gpu::RenderScale CapForDisplay(const gpu::RenderScale& scale, PixelSize display)
    {
        if (!Below4K(display))
            return scale;
        auto axis = [](gpu::AxisScale a) { return a.num > 2 * a.den ? gpu::AxisScale{ 2, 1 } : a; };
        return { axis(scale.x), axis(scale.y) };
    }

    // The GPU's largest 2D image (maxImageDimension2D; 0 unknown) for a
    // target of up to `guestMax` guest pixels on an axis (2048 rows): an
    // axis whose host size would be larger steps down the scales the
    // renderer takes (4, 3, 2, 1.5, 1) until it fits. Apple's GPUs take
    // 16384 (4x is 8192): a guard for a smaller GPU, and RECARBON_RENDER_SCALE.
    inline gpu::RenderScale FitImageLimit(const gpu::RenderScale& scale, uint32_t guestMax, uint32_t maxDimension)
    {
        if (maxDimension == 0)
            return scale;
        auto axis = [&](gpu::AxisScale a) {
            if (a.Size(guestMax) <= maxDimension)
                return a;
            for (uint32_t halves : { 8u, 6u, 4u, 3u })
            {
                const gpu::AxisScale step = gpu::AxisFromHalves(halves);
                if (step.Value() < a.Value() && step.Size(guestMax) <= maxDimension)
                    return step;
            }
            return gpu::AxisScale{ 1, 1 };
        };
        return { axis(scale.x), axis(scale.y) };
    }

    // A swapchain's extent within the surface's limits
    // (VkSurfaceCapabilitiesKHR's minImageExtent and maxImageExtent): the
    // window's pixels, which Wayland's surfaces give, aren't bound by them.
    // 0x0 stays 0x0 (minimised: no swapchain).
    inline PixelSize ClampExtent(PixelSize extent, PixelSize lo, PixelSize hi)
    {
        if (!extent.Known())
            return {};
        auto axis = [](uint32_t v, uint32_t l, uint32_t h) { return h >= l && h > 0 ? std::clamp(v, l, h) : v; };
        return { axis(extent.width, lo.width, hi.width), axis(extent.height, lo.height, hi.height) };
    }

    // The window's saved size at launch (points), within the usable bounds
    // of the display it opens on (points; 0: unknown, the size is kept), so
    // a size saved on a larger monitor doesn't open past a laptop's edges.
    inline void ClampWindowSize(int& w, int& h, int usableW, int usableH)
    {
        if (usableW > 0 && w > usableW)
            w = usableW;
        if (usableH > 0 && h > usableH)
            h = usableH;
    }
}
