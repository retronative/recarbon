// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// hid/profiles.h (ROADMAP M8 B2): the built-in keyboard profiles against the
// tables the user approved (2026-10-23), written out here in full for every
// control, independently of the header's rows; Default against the settings'
// defaults (M5's baseline); no key on two controls in WASD and Arrows; the
// controller's default mapping against hid/pad_map.h. Links user/settings.cpp
// for the defaults.
#include <hid/profiles.h>
#include <user/settings.h>

#include <cstdio>
#include <string>

namespace
{
    int g_failures = 0;

    void Check(bool ok, const std::string& what)
    {
        if (!ok)
        {
            printf("  FAIL: %s\n", what.c_str());
            g_failures++;
        }
    }

    using hid::profiles::KeyboardMapping;
    using settings::keynames::Code;

    // One row per control, in keymap::Control order: its two keys.
    struct Keys
    {
        const char* first;
        const char* second;
    };
    KeyboardMapping FromTable(const Keys (&rows)[hid::keymap::kControls])
    {
        KeyboardMapping m{};
        for (size_t i = 0; i < hid::keymap::kControls; i++)
        {
            m[2 * i] = Code(rows[i].first);
            m[2 * i + 1] = Code(rows[i].second);
        }
        return m;
    }

    // RightTrigger, LeftTrigger, LS Left, LS Right, LS Up, LS Down, A, B, X, Y, LB, RB,
    // D-pad Up, Down, Left, Right, Start, Back, RS Up, Down, Left, Right, LS click, RS click.
    const Keys kDefaultTable[] = {
        { "w", "none" }, { "s", "none" }, { "a", "left" }, { "d", "right" }, { "up", "none" }, { "down", "none" },
        { "space", "none" }, { "backspace", "none" }, { "left_shift", "none" }, { "left_ctrl", "none" }, { "q", "none" }, { "e", "none" },
        { "up", "none" }, { "down", "none" }, { "left", "none" }, { "right", "none" }, { "return", "none" }, { "escape", "none" },
        { "o", "none" }, { "l", "none" }, { "k", "none" }, { "semicolon", "none" }, { "z", "none" }, { "c", "none" },
    };
    const Keys kWasdTable[] = {
        { "w", "none" }, { "s", "none" }, { "a", "none" }, { "d", "none" }, { "none", "none" }, { "none", "none" },
        { "space", "none" }, { "backspace", "none" }, { "left_shift", "none" }, { "left_ctrl", "none" }, { "q", "none" }, { "e", "none" },
        { "up", "none" }, { "down", "none" }, { "left", "none" }, { "right", "none" }, { "return", "none" }, { "escape", "none" },
        { "o", "none" }, { "l", "none" }, { "k", "none" }, { "semicolon", "none" }, { "z", "none" }, { "c", "none" },
    };
    const Keys kArrowsTable[] = {
        { "up", "none" }, { "down", "none" }, { "left", "none" }, { "right", "none" }, { "none", "none" }, { "none", "none" },
        { "space", "none" }, { "backspace", "none" }, { "left_shift", "none" }, { "left_ctrl", "none" }, { "q", "none" }, { "e", "none" },
        { "w", "none" }, { "s", "none" }, { "a", "none" }, { "d", "none" }, { "return", "none" }, { "escape", "none" },
        { "o", "none" }, { "l", "none" }, { "k", "none" }, { "semicolon", "none" }, { "z", "none" }, { "c", "none" },
    };

    std::string Name(uint16_t code)
    {
        const char* t = settings::keynames::Token(code);
        return t ? t : std::to_string(code);
    }
}

int main()
{
    using namespace hid::profiles;
    KeyboardMapping defaults{};
    for (size_t i = 0; i < kKeySlots; i++)
        defaults[i] = uint16_t(settings::DefaultValue(IdOf(i)));

    printf("---- Default is the settings' defaults, M5's baseline ----\n");
    Check(defaults == FromTable(kDefaultTable), "the settings' key defaults are the table above");
    Check(BuiltinMapping(Builtin::Default, defaults) == defaults, "Default is the defaults, unchanged");

    printf("---- WASD and Arrows are the approved tables ----\n");
    const KeyboardMapping wasd = BuiltinMapping(Builtin::Wasd, defaults);
    const KeyboardMapping arrows = BuiltinMapping(Builtin::Arrows, defaults);
    for (size_t i = 0; i < kKeySlots; i++)
    {
        Check(wasd[i] == FromTable(kWasdTable)[i], "WASD slot " + std::to_string(i) + ": " + Name(wasd[i]));
        Check(arrows[i] == FromTable(kArrowsTable)[i], "Arrows slot " + std::to_string(i) + ": " + Name(arrows[i]));
    }

    printf("---- no key on two controls in the presets ----\n");
    Check(RepeatedKey(wasd) == 0, "WASD repeats " + Name(RepeatedKey(wasd)));
    Check(RepeatedKey(arrows) == 0, "Arrows repeats " + Name(RepeatedKey(arrows)));
    Check(RepeatedKey(defaults) == Code("left"), "Default shares Left (steering and the D-pad), by design");
    size_t bound = 0;
    for (uint16_t c : wasd)
        bound += c != 0;
    Check(bound == 22, "WASD binds 22 keys, got " + std::to_string(bound));
    bound = 0;
    for (uint16_t c : arrows)
        bound += c != 0;
    Check(bound == 22, "Arrows binds 22 keys, got " + std::to_string(bound));

    printf("---- validity ----\n");
    Check(Valid(defaults) && Valid(wasd) && Valid(arrows), "the three are valid");
    KeyboardMapping bad = defaults;
    bad[0] = settings::keynames::kUnknown;
    Check(!Valid(bad), "an unknown code isn't valid");

    printf("---- controller ----\n");
    const PadMapping pad = DefaultPad();
    for (size_t i = 0; i < kPadButtons; i++)
        Check(pad[i] == hid::padmap::kDefault[i].sdl, "the default pad's button " + std::to_string(i));
    Check(ValidPad(pad), "the default pad is valid");
    PadMapping paddle = pad;
    paddle[0] = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1;
    Check(!ValidPad(paddle), "a paddle isn't bindable in v1");
    for (const SdlName& s : kSdlButtons)
        Check(FindSdlButton(s.name) == &s && FindSdlButton(s.button) == &s, std::string("the SDL name ") + std::string(s.name));

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
