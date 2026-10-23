// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Input profiles (ROADMAP M8, reference/M8_PLAN.md B2): a keyboard mapping and
// a controller mapping as plain values, the built-in keyboard profiles, and
// the rules a mapping must meet. Header only, no SDL calls (SDL's button enum
// only), for tests/profiles_test.cpp. user/profiles.cpp stores them.
//   - A keyboard mapping is the Key options' values (scancodes, 0: no key), in
//     settings::Id order: control i's two keys at [2i] and [2i + 1]. So the
//     active profile is applied by setting those options, and settings.toml's
//     [keys] always holds the active profile's keys.
//   - A controller mapping is one SDL gamepad button per XInput button of
//     hid/pad_map.h, in its order (buttons only in v1: the triggers, sticks,
//     deadzones and paddles stay as they are).
//   - Built-in keyboard profiles: Default (the settings' defaults, M5's
//     baseline, unchanged), WASD and Arrows (the user's tables, 2026-10-23:
//     Default with the movement and D-pad rows replaced, so that no key drives
//     two controls). They are never stored and never edited: an edit makes a
//     custom copy.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>

#include <SDL3/SDL_gamepad.h>

#include <user/key_names.h>

#include "key_map.h"
#include "pad_map.h"

namespace hid::profiles
{
    constexpr size_t kKeySlots = 2 * keymap::kControls;
    using KeyboardMapping = std::array<uint16_t, kKeySlots>;

    constexpr size_t SlotOf(settings::Id id) { return size_t(id) - size_t(settings::Id::KeyRightTrigger); }
    constexpr settings::Id IdOf(size_t slot) { return settings::Id(size_t(settings::Id::KeyRightTrigger) + slot); }

    enum class Builtin : uint8_t { Default, Wasd, Arrows, Count };
    inline constexpr std::string_view kBuiltinNames[] = { "Default", "WASD", "Arrows" };
    static_assert(std::size(kBuiltinNames) == size_t(Builtin::Count));

    // A preset's rows that differ from Default: a control and its two keys.
    struct Row
    {
        keymap::Control control;
        std::string_view first, second;  // tokens of user/key_names.h
    };
    using C = keymap::Control;
    inline constexpr Row kWasdRows[] = {
        { C::RightTrigger, "w", "none" }, { C::LeftTrigger, "s", "none" },
        { C::LeftStickLeft, "a", "none" }, { C::LeftStickRight, "d", "none" },
        { C::LeftStickUp, "none", "none" }, { C::LeftStickDown, "none", "none" },
        { C::DpadUp, "up", "none" }, { C::DpadDown, "down", "none" },
        { C::DpadLeft, "left", "none" }, { C::DpadRight, "right", "none" },
    };
    inline constexpr Row kArrowsRows[] = {
        { C::RightTrigger, "up", "none" }, { C::LeftTrigger, "down", "none" },
        { C::LeftStickLeft, "left", "none" }, { C::LeftStickRight, "right", "none" },
        { C::LeftStickUp, "none", "none" }, { C::LeftStickDown, "none", "none" },
        { C::DpadUp, "w", "none" }, { C::DpadDown, "s", "none" },
        { C::DpadLeft, "a", "none" }, { C::DpadRight, "d", "none" },
    };

    // A built-in profile's mapping: `defaults` (the settings' defaults, which
    // is Default) with the preset's rows replaced.
    inline KeyboardMapping BuiltinMapping(Builtin builtin, const KeyboardMapping& defaults)
    {
        KeyboardMapping m = defaults;
        auto apply = [&](const auto& rows) {
            for (const Row& r : rows)
            {
                m[2 * size_t(r.control)] = settings::keynames::Code(r.first);
                m[2 * size_t(r.control) + 1] = settings::keynames::Code(r.second);
            }
        };
        if (builtin == Builtin::Wasd)
            apply(kWasdRows);
        else if (builtin == Builtin::Arrows)
            apply(kArrowsRows);
        return m;
    }

    // Every value is no key or a key of the table (user/key_names.h).
    inline bool Valid(const KeyboardMapping& m)
    {
        for (uint16_t code : m)
            if (code == settings::keynames::kUnknown || !settings::keynames::Known(code))
                return false;
        return true;
    }

    // A key bound to more than one control (or twice to one), 0 if none: the
    // presets' rule (Default has some by design: Up is the left stick and the
    // D-pad there).
    inline uint16_t RepeatedKey(const KeyboardMapping& m)
    {
        for (size_t i = 0; i < m.size(); i++)
            for (size_t j = i + 1; m[i] != settings::keynames::kNone && j < m.size(); j++)
                if (m[i] == m[j])
                    return m[i];
        return 0;
    }

    // Controller mappings.
    constexpr size_t kPadButtons = std::size(padmap::kDefault);
    using PadMapping = std::array<SDL_GamepadButton, kPadButtons>;

    inline PadMapping DefaultPad()
    {
        PadMapping m{};
        for (size_t i = 0; i < kPadButtons; i++)
            m[i] = padmap::kDefault[i].sdl;
        return m;
    }

    // The names in controller.toml: XInput's buttons (as the [keys] rows name
    // them), and the SDL buttons a v1 profile may bind (positional: south is A
    // on an Xbox pad, Cross on a PlayStation one). The paddles, Guide and the
    // misc buttons aren't bindable in v1.
    inline constexpr std::string_view kPadButtonNames[kPadButtons] = {
        "a", "b", "x", "y", "back", "start", "left_stick_click", "right_stick_click", "lb", "rb",
        "dpad_up", "dpad_down", "dpad_left", "dpad_right",
    };
    struct SdlName
    {
        SDL_GamepadButton button;
        std::string_view name;
    };
    inline constexpr SdlName kSdlButtons[] = {
        { SDL_GAMEPAD_BUTTON_SOUTH, "south" }, { SDL_GAMEPAD_BUTTON_EAST, "east" },
        { SDL_GAMEPAD_BUTTON_WEST, "west" }, { SDL_GAMEPAD_BUTTON_NORTH, "north" },
        { SDL_GAMEPAD_BUTTON_BACK, "back" }, { SDL_GAMEPAD_BUTTON_START, "start" },
        { SDL_GAMEPAD_BUTTON_LEFT_STICK, "left_stick" }, { SDL_GAMEPAD_BUTTON_RIGHT_STICK, "right_stick" },
        { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "left_shoulder" }, { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "right_shoulder" },
        { SDL_GAMEPAD_BUTTON_DPAD_UP, "dpad_up" }, { SDL_GAMEPAD_BUTTON_DPAD_DOWN, "dpad_down" },
        { SDL_GAMEPAD_BUTTON_DPAD_LEFT, "dpad_left" }, { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "dpad_right" },
    };

    inline const SdlName* FindSdlButton(SDL_GamepadButton b)
    {
        for (const SdlName& s : kSdlButtons)
            if (s.button == b)
                return &s;
        return nullptr;
    }
    inline const SdlName* FindSdlButton(std::string_view name)
    {
        for (const SdlName& s : kSdlButtons)
            if (s.name == name)
                return &s;
        return nullptr;
    }

    // Every XInput button is driven by a bindable SDL button.
    inline bool ValidPad(const PadMapping& m)
    {
        for (SDL_GamepadButton b : m)
            if (!FindSdlButton(b))
                return false;
        return true;
    }
}
