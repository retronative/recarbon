// ReCarbon (v0.1.4). GPL-3.0-or-later (see COPYING).
//
// Settings > Graphics > Internal Resolution's text, in players' terms: each
// entry names its 16:9 standard (HD 720p, Full HD 1080p, QHD 1440p, 4K UHD
// 2160p), and the row says what is drawn now when that differs, from the
// renderer's own choice (gpu::renderer::InternalResolutionScale: Auto's
// pick, the 4K cap of video/display_cap.h, the GPU's size limit). A standard
// name is shown only for its exact size; any other size is shown in pixels.
// The scale factors (1x, 2x) stay in settings.toml, the environment and the
// logs. Pure functions (tests/resolution_label_test.cpp).
#pragma once
#include <gpu/render_scale.h>
#include <video/display_cap.h>

#include <string>
#include <string_view>

namespace video
{
    // The standard a uniform scale draws exactly: "HD" (1280x720), "Full HD"
    // (1920x1080), "QHD" (2560x1440), "4K UHD" (3840x2160); nullptr for any
    // other.
    inline const char* PresetName(const gpu::RenderScale& s)
    {
        if (!(s.x == s.y))
            return nullptr;
        if (s.x == gpu::AxisScale{ 1, 1 })
            return "HD";
        if (s.x == gpu::AxisScale{ 3, 2 })
            return "Full HD";
        if (s.x == gpu::AxisScale{ 2, 1 })
            return "QHD";
        if (s.x == gpu::AxisScale{ 3, 1 })
            return "4K UHD";
        return nullptr;
    }

    // "2560x1440": what a scale draws of the game's 1280x720 frame.
    inline std::string SizeText(const gpu::RenderScale& s)
    {
        return std::to_string(s.x.Size(1280)) + "x" + std::to_string(s.y.Size(720));
    }

    // The standard's name, else the size: "QHD", "3840x1440".
    inline std::string NameOrSize(const gpu::RenderScale& s)
    {
        const char* name = PresetName(s);
        return name ? name : SizeText(s);
    }

    // The row's value. `label` the entry's ("QHD (1440p)", "Auto"); `entry`
    // the scale the entry names (unused for Auto); `rendering` what is drawn
    // now (nullptr while not known: Auto before there is a picture).
    //   "QHD (1440p)"                     drawn as named
    //   "4K UHD (QHD on this display)"    held below its name (the 4K cap)
    //   "Auto (QHD, 1440p)", "Auto (3840x1440)"
    inline std::string ResolutionRowText(std::string_view label, bool isAuto, const gpu::RenderScale& entry, const gpu::RenderScale* rendering)
    {
        if (!rendering)
            return std::string(label);
        if (isAuto)
        {
            if (PresetName(*rendering))
                return "Auto (" + std::string(PresetName(*rendering)) + ", " + std::to_string(rendering->y.Size(720)) + "p)";
            return "Auto (" + SizeText(*rendering) + ")";
        }
        if (*rendering == entry)
            return std::string(label);
        return NameOrSize(entry) + " (" + NameOrSize(*rendering) + " on this display)";
    }

    // The row's value while a developer override fixes the size for the run.
    inline std::string FixedRowText(const gpu::RenderScale& rendering)
    {
        return SizeText(rendering) + " (set for this run)";
    }

    // The help line under the row while the 4K cap holds an entry down
    // (`asked` the entry's scale, `applied` what is drawn), else "".
    inline std::string CapNote(const gpu::RenderScale& asked, const gpu::RenderScale& applied, PixelSize display)
    {
        if (asked == applied)
            return {};
        return "This display has " + std::to_string(display.width) + "x" + std::to_string(display.height) + " pixels, fewer than " +
            NameOrSize(asked) + "'s " + SizeText(asked) + ", so the game draws " + NameOrSize(applied) + " (" + SizeText(applied) +
            "). Your choice is kept and applies on a 4K display.";
    }
}
