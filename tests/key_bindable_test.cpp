// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// user/key_names.h Bindable: Settings > Controls binds any key of the table, by
// its position (scancode), whatever the keyboard layout names it: v0.1.1 binds
// keys under a layout in another script (Myanmar, Thai) too, which v0.1.0
// refused (ROADMAP M5.15). And Label: what a Controls cell shows, the key's
// position when the layout's name isn't ASCII. The names are what SDL gives on
// a layout (the Myanmar ones as logged on the Mac, 2026-10-23). Header only: no SDL.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/key_bindable_test.cpp -o build/key_bindable_test && build/key_bindable_test
#include <user/key_names.h>

#include <cstdio>
#include <string_view>

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

    constexpr uint16_t kW = 26, kS = 22, kA = 4, kSpace = 44, kLeft = 80, kLeftShift = 225, kF5 = 62, kKeypad1 = 89;
    constexpr uint16_t kF1 = 58;  // opens the menu: not in the table
}

using settings::keynames::AsciiName;
using settings::keynames::Bindable;

// The same rules hold at compile time (constexpr).
static_assert(Bindable(kW));
static_assert(!Bindable(kF1));

int main()
{
    printf("---- keys of the table bind, whatever the layout calls them ----\n");
    // A binding is the key's position (its scancode): the layout's name for
    // it only labels the cell (Label, below). Under Myanmar, W's position is
    // named U+1010; it binds as "w", as on any other layout.
    Check(Bindable(kW) && Bindable(kA) && Bindable(kS), "W, A and S");
    Check(Bindable(kSpace) && Bindable(kLeft) && Bindable(kLeftShift), "Space, Left, Left Shift");
    Check(Bindable(kF5) && Bindable(kKeypad1), "F5, Keypad 1");
    // What settings.toml keeps for it is the position's name, as before.
    Check(std::string_view(settings::keynames::Token(kW)) == "w" && settings::keynames::Code("w") == kW,
        "W's position is saved as \"w\" and read back as the same key");
    Check(std::string_view(settings::keynames::Token(kLeftShift)) == "left_shift", "Left Shift as \"left_shift\"");

    printf("---- what isn't a key of the table, or no key ----\n");
    Check(!Bindable(kF1), "F1 (opens the menu) isn't bindable");
    Check(!Bindable(settings::keynames::kNone), "no key isn't a binding");
    Check(!Bindable(0x1FF), "a code outside the table");

    printf("---- names ----\n");
    Check(AsciiName(""), "an empty name is ASCII");
    Check(!AsciiName("\xE1\x80\x90"), "Myanmar W (U+1010) isn't");
    Check(!AsciiName("\xE0\xB9\x84"), "Thai (U+0E44) isn't");
    Check(!AsciiName("\xC3\x89"), "Latin-1 (U+00C9) isn't");
    Check(!AsciiName("\t") && !AsciiName("\x7F"), "control characters aren't printable");
    Check(AsciiName(" ~"), "space to tilde are");

    printf("---- labels (Settings > Controls cells) ----\n");
    using settings::keynames::Label;
    auto same = [](const char* a, const char* b) { return std::string_view(a ? a : "") == b; };
    Check(same(Label(kW, "W", "W"), "W"), "the layout's name when it is ASCII");
    Check(same(Label(kW, "Z", "W"), "Z"), "a French layout's Z for the W position");
    Check(same(Label(kW, "\xE1\x80\x90", "W"), "W"), "Myanmar W: the key's position, not a '?'");
    Check(same(Label(kW, "\xE0\xB9\x84", "W"), "W"), "Thai: the position too");
    Check(same(Label(kW, "", "W"), "W"), "no layout name: the position");
    Check(same(Label(kW, "\xE1\x80\x90", ""), "w"), "no ASCII name at all: the settings file's token");
    Check(same(Label(kW, "\xE1\x80\x90", "\xC3\x89"), "w"), "a non-ASCII position name isn't used either");
    Check(same(Label(0x1FF, "", ""), "?"), "nothing to show: '?'");

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
