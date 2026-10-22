// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// The controller's default button mapping: which SDL gamepad button drives
// which XInput button the game reads. hid/input.cpp reads it each frame
// (Back goes through the Back + Start settings chord there).
// tests/pad_map_test.cpp pins it as the regression baseline (ROADMAP M5).
// SpeedBreaker had it inline in input.cpp; moved unchanged so a test can
// read it, and so M8's controller profiles can replace it as a value
// (reference/ARCHITECTURE_REVIEW.md R2).
#pragma once
#include <cstdint>

#include <SDL3/SDL_gamepad.h>

#include "key_map.h"

namespace hid::padmap
{
    namespace button = keymap::button;  // XInput's bits, as the game reads them

    struct Entry
    {
        SDL_GamepadButton sdl;
        uint16_t xinput;
    };

    // Positional names: SOUTH is A on an Xbox pad, Cross on a PlayStation one.
    inline constexpr Entry kDefault[] = {
        { SDL_GAMEPAD_BUTTON_SOUTH, button::A }, { SDL_GAMEPAD_BUTTON_EAST, button::B },
        { SDL_GAMEPAD_BUTTON_WEST, button::X }, { SDL_GAMEPAD_BUTTON_NORTH, button::Y },
        { SDL_GAMEPAD_BUTTON_BACK, button::Back }, { SDL_GAMEPAD_BUTTON_START, button::Start },
        { SDL_GAMEPAD_BUTTON_LEFT_STICK, button::LeftThumb },
        { SDL_GAMEPAD_BUTTON_RIGHT_STICK, button::RightThumb },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, button::LeftShoulder },
        { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, button::RightShoulder },
        { SDL_GAMEPAD_BUTTON_DPAD_UP, button::DpadUp }, { SDL_GAMEPAD_BUTTON_DPAD_DOWN, button::DpadDown },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, button::DpadLeft }, { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, button::DpadRight },
    };
}
