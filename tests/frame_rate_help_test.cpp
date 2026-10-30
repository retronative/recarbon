// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// video/frame_rate_help.h: the Frame Rate row's help text for each of its values, word for word
// (the player-facing wording is the specification), and nothing for a value the row can't hold.
// Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/frame_rate_help_test.cpp -o build/frame_rate_help_test && build/frame_rate_help_test
#include <video/frame_rate_help.h>

#include <cstdio>
#include <cstring>

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

    bool Same(const char* a, const char* b)
    {
        return std::strcmp(a, b) == 0;
    }
}

int main()
{
    using video::FrameRateDescription;
    static_assert(settings::kFrameRateAuto != 60 && settings::kFrameRateAuto != 30, "Auto's value is its own");

    printf("---- each value's text ----\n");
    Check(Same(FrameRateDescription(settings::kFrameRateAuto),
              "Targets 60 FPS when your device can sustain it and switches to 30 FPS when needed. "
              "Switching during gameplay may cause a noticeable change in motion smoothness."),
        "Auto's text");
    Check(Same(FrameRateDescription(60),
              "If your device cannot maintain 60 FPS, you may experience uneven frame pacing, judder, or stuttering."),
        "60's text");
    Check(Same(FrameRateDescription(30),
              "Targets consistent 30 FPS for steadier frame pacing on 60 Hz displays. Motion will be less fluid than at 60 FPS."),
        "30's text");

    printf("---- other values ----\n");
    for (int32_t v : { -1, 1, 45, 59, 120, INT32_MAX })
        Check(Same(FrameRateDescription(v), ""), "a value the row can't hold says nothing");

    printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "ok", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
