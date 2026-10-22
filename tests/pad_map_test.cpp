// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// The controller's default button mapping (hid/pad_map.h) as the M5
// regression baseline: every SDL gamepad button the runtime reads, and the
// XInput bit it gives the game, as SpeedBreaker shipped it. M8's controller
// profiles must restore exactly this on "reset to default".
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime $(pkg-config --cflags sdl3) tests/pad_map_test.cpp -o build/pad_map_test
//   build/pad_map_test
#include <hid/pad_map.h>

#include <cstdio>
#include <iterator>

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

    // XInput's bits (xbox.h XAMINPUT_GAMEPAD_*), written out here so the
    // baseline doesn't follow a change to the header it checks.
    struct Expected
    {
        SDL_GamepadButton sdl;
        uint16_t xinput;
        const char* what;
    };
    constexpr Expected kBaseline[] = {
        { SDL_GAMEPAD_BUTTON_SOUTH, 0x1000, "South -> A" },
        { SDL_GAMEPAD_BUTTON_EAST, 0x2000, "East -> B" },
        { SDL_GAMEPAD_BUTTON_WEST, 0x4000, "West -> X" },
        { SDL_GAMEPAD_BUTTON_NORTH, 0x8000, "North -> Y" },
        { SDL_GAMEPAD_BUTTON_BACK, 0x0020, "Back -> BACK" },
        { SDL_GAMEPAD_BUTTON_START, 0x0010, "Start -> START" },
        { SDL_GAMEPAD_BUTTON_LEFT_STICK, 0x0040, "left stick click -> LEFT_THUMB" },
        { SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0x0080, "right stick click -> RIGHT_THUMB" },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0x0100, "left shoulder -> LEFT_SHOULDER" },
        { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0x0200, "right shoulder -> RIGHT_SHOULDER" },
        { SDL_GAMEPAD_BUTTON_DPAD_UP, 0x0001, "D-pad up -> DPAD_UP" },
        { SDL_GAMEPAD_BUTTON_DPAD_DOWN, 0x0002, "D-pad down -> DPAD_DOWN" },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, 0x0004, "D-pad left -> DPAD_LEFT" },
        { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 0x0008, "D-pad right -> DPAD_RIGHT" },
    };
}

int main()
{
    using hid::padmap::kDefault;

    Check(std::size(kDefault) == std::size(kBaseline), "the mapping has the baseline's 14 buttons");
    for (const Expected& want : kBaseline)
    {
        int found = 0;
        for (const auto& e : kDefault)
            if (e.sdl == want.sdl)
            {
                found++;
                Check(e.xinput == want.xinput, want.what);
            }
        Check(found == 1, want.what);
    }
    uint16_t all = 0;
    for (const auto& e : kDefault)
    {
        Check((all & e.xinput) == 0, "no two buttons give the same XInput bit");
        all |= e.xinput;
    }

    if (g_failures)
    {
        printf("pad_map_test: %d failed\n", g_failures);
        return 1;
    }
    printf("pad_map_test: all passed\n");
    return 0;
}
