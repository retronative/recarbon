// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// user/key_names.h Bindable (ROADMAP M5.15): Settings > Controls binds a key
// only if the keyboard layout names it in printable ASCII. A layout in another
// script (Myanmar, Thai) is refused, so no binding is saved under a name the
// menu can't show. The names are what SDL gives on a layout (the Myanmar ones
// as logged on the Mac, 2026-10-23). Header only: no SDL.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/key_bindable_test.cpp -o build/key_bindable_test && build/key_bindable_test
#include <user/key_names.h>

#include <cstdio>

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
static_assert(Bindable(kW, "W"));
static_assert(!Bindable(kW, "\xE1\x80\x90"));

int main()
{
    printf("---- ASCII names bind ----\n");
    Check(Bindable(kW, "W"), "W named W");
    Check(Bindable(kA, "A") && Bindable(kS, "S"), "A and S");
    Check(Bindable(kW, "Z"), "a French layout's Z for the W position (still ASCII)");

    printf("---- special keys bind ----\n");
    Check(Bindable(kSpace, "Space"), "Space");
    Check(Bindable(kLeft, "Left"), "Left arrow");
    Check(Bindable(kLeftShift, "Left Shift"), "Left Shift");
    Check(Bindable(kF5, "F5"), "F5");
    Check(Bindable(kKeypad1, "Keypad 1"), "Keypad 1");

    printf("---- non-ASCII names are refused ----\n");
    Check(!Bindable(kW, "\xE1\x80\x90"), "Myanmar W (U+1010)");
    Check(!Bindable(kS, "\xE1\x80\xBB"), "Myanmar S (U+103B)");
    Check(!Bindable(kA, "\xE1\x80\xB1"), "Myanmar A (U+1031)");
    Check(!Bindable(kW, "\xE0\xB9\x84"), "Thai (U+0E44)");
    Check(!Bindable(kW, "\xC3\x89"), "Latin-1 \xC3\x89 (U+00C9): not ASCII either");
    Check(!Bindable(kW, "W\xE1\x80\x90"), "one non-ASCII character is enough");

    printf("---- what isn't a key of the table, or no key ----\n");
    Check(!Bindable(kF1, "F1"), "F1 (opens the menu) isn't bindable");
    Check(!Bindable(settings::keynames::kNone, ""), "no key isn't a binding");
    Check(!Bindable(0x1FF, "X"), "a code outside the table");

    printf("---- names ----\n");
    Check(AsciiName(""), "an empty name passes (the cell shows the position)");
    Check(Bindable(kW, ""), "so a key the layout doesn't name binds");
    Check(!AsciiName("\t"), "a control character isn't printable");
    Check(!AsciiName("\x7F"), "DEL isn't printable");
    Check(AsciiName(" ~"), "space to tilde are");

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
