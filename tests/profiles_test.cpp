// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// hid/profiles.h (ROADMAP M8 B2): the built-in keyboard profiles against the
// tables the user approved (2026-10-23; the WASD preset merged into Default,
// which is shown as "WASD", 2026-10-24), written out here in full for every
// control, independently of the header's rows; Default against the settings'
// defaults (M5's baseline); no key on two controls in Arrows; the
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
    Check(size_t(Builtin::Count) == 2 && kBuiltinNames[0] == "Default" && kBuiltinLabels[0] == "WASD" && kBuiltinNames[1] == "Arrows" &&
            kBuiltinLabels[1] == "Arrows",
        "the built-ins are Default (shown as WASD) and Arrows");

    printf("---- Arrows is the approved table ----\n");
    const KeyboardMapping arrows = BuiltinMapping(Builtin::Arrows, defaults);
    for (size_t i = 0; i < kKeySlots; i++)
        Check(arrows[i] == FromTable(kArrowsTable)[i], "Arrows slot " + std::to_string(i) + ": " + Name(arrows[i]));

    printf("---- no key on two controls in Arrows ----\n");
    Check(RepeatedKey(arrows) == 0, "Arrows repeats " + Name(RepeatedKey(arrows)));
    Check(RepeatedKey(defaults) == Code("left"), "Default shares Left (steering and the D-pad), by design");
    size_t bound = 0;
    for (uint16_t c : arrows)
        bound += c != 0;
    Check(bound == 22, "Arrows binds 22 keys, got " + std::to_string(bound));

    printf("---- what one key does in each preset (the input code's resolution) ----\n");
    // As hid/input.cpp does: a control is pressed when either of its keys is
    // held, then keymap::Resolve. So the D-pad keys reach the game as D-pad
    // buttons only, and the driving keys as the triggers and steering only.
    auto press = [](const KeyboardMapping& m, const char* key) {
        bool pressed[hid::keymap::kControls] = {};
        for (size_t i = 0; i < hid::keymap::kControls; i++)
            pressed[i] = m[2 * i] == Code(key) || m[2 * i + 1] == Code(key);
        return hid::keymap::Resolve(pressed);
    };
    namespace btn = hid::keymap::button;
    constexpr size_t LX = size_t(hid::keymap::Axis::LX), LY = size_t(hid::keymap::Axis::LY);
    auto onlyButton = [](const hid::keymap::Resolved& r, uint16_t b) {
        return r.buttons == b && !r.leftTrigger && !r.rightTrigger && !r.axisSet[0] && !r.axisSet[1] && !r.axisSet[2] && !r.axisSet[3];
    };
    Check(onlyButton(press(arrows, "w"), btn::DpadUp) && onlyButton(press(arrows, "s"), btn::DpadDown) &&
            onlyButton(press(arrows, "a"), btn::DpadLeft) && onlyButton(press(arrows, "d"), btn::DpadRight),
        "Arrows: W A S D are the D-pad only");
    auto r = press(arrows, "up");
    Check(r.rightTrigger && !r.leftTrigger && r.buttons == 0 && !r.axisSet[LY], "Arrows: Up is the throttle only (no D-pad, no stick)");
    r = press(arrows, "down");
    Check(r.leftTrigger && !r.rightTrigger && r.buttons == 0 && !r.axisSet[LY], "Arrows: Down is the brake only");
    r = press(arrows, "left");
    Check(r.axisSet[LX] && r.axis[LX] < 0 && r.buttons == 0 && !r.leftTrigger, "Arrows: Left is steering only");
    r = press(defaults, "w");
    Check(r.rightTrigger && r.buttons == 0 && !r.axisSet[LY], "Default: W is the throttle only");
    r = press(defaults, "d");
    Check(r.axisSet[LX] && r.axis[LX] > 0 && r.buttons == 0, "Default: D is steering only");
    r = press(defaults, "up");
    Check(r.buttons == btn::DpadUp && r.axisSet[LY], "Default (unchanged): Up is the D-pad and the left stick, as in M5");

    printf("---- validity ----\n");
    Check(Valid(defaults) && Valid(arrows), "both are valid");
    KeyboardMapping bad = defaults;
    bad[0] = settings::keynames::kUnknown;
    Check(!Valid(bad), "an unknown code isn't valid");

    printf("---- controller ----\n");
    const PadMapping pad = DefaultPad();
    for (size_t i = 0; i < kPadButtons; i++)
        Check(pad[i] == hid::padmap::kDefault[i].sdl, "the default pad's button " + std::to_string(i));
    Check(ValidPad(pad), "the default pad is valid");
    const PadMapping rebound = Rebind(pad, 0, SDL_GAMEPAD_BUTTON_EAST);
    Check(rebound[0] == SDL_GAMEPAD_BUTTON_EAST && rebound[1] == SDL_GAMEPAD_BUTTON_SOUTH, "A on East: B swaps to South");
    for (size_t i = 2; i < kPadButtons; i++)
        Check(rebound[i] == pad[i], "a swap leaves the other buttons, " + std::to_string(i));
    Check(Rebind(pad, 3, pad[3]) == pad, "binding a button to its own button changes nothing");
    PadMapping paddle = pad;
    paddle[0] = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1;
    Check(!ValidPad(paddle), "a paddle isn't bindable in v1");
    for (const SdlName& s : kSdlButtons)
        Check(FindSdlButton(s.name) == &s && FindSdlButton(s.button) == &s, std::string("the SDL name ") + std::string(s.name));

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
