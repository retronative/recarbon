// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Settings > Display > Frame Rate: what each choice does, in the words the
// menu's help line shows under the row for the value it displays
// (ui/settings_menu.cpp HelpLine; the note on what Auto runs at now comes
// after it). The mechanisms are video/frame_rate.h's: Auto is
// video/frame_rate_policy.h's 60-or-30, 30 is one guest vblank every other
// refresh (the game's own accumulator then steps its world twice a frame, so
// the game keeps its speed). Pure: tests/frame_rate_help_test.cpp pins the
// texts.
#pragma once
#include <user/settings.h>

#include <cstdint>

namespace video
{
    // The setting's value (settings::kFrameRateAuto, 60 or 30); "" for any
    // other (nothing to say: the row shows the value as it is).
    constexpr const char* FrameRateDescription(int32_t setting)
    {
        switch (setting)
        {
        case settings::kFrameRateAuto:
            return "Targets 60 FPS when your device can sustain it and switches to 30 FPS when needed. "
                   "Switching during gameplay may cause a noticeable change in motion smoothness.";
        case 60:
            return "If your device cannot maintain 60 FPS, you may experience uneven frame pacing, judder, or stuttering.";
        case 30:
            return "Targets consistent 30 FPS for steadier frame pacing on 60 Hz displays. Motion will be less fluid than at 60 FPS.";
        default:
            return "";
        }
    }
}
