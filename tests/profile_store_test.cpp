// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// user/profiles.cpp (ROADMAP M8 B2): the profile files' format and reading
// (round trips, skipped entries, corrupt text), the startup decision that
// keeps settings.toml's [keys] (migration, a [keys] the profiles didn't know
// of, an unusable file), the edit rule (a built-in active makes a custom copy),
// and the running store in a temporary folder: a switch writes the file and
// sets the Key options at once, an edit lands in a custom copy on the next
// Poll, and a write that fails (a read-only folder) changes nothing.
#include <user/profiles.h>
#include <user/settings.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

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

    std::string ReadFile(const std::filesystem::path& p)
    {
        std::ifstream in(p);
        std::ostringstream s;
        s << in.rdbuf();
        return s.str();
    }

    using namespace profiles;
    namespace hp = hid::profiles;
    using settings::keynames::Code;
}

int main()
{
    KeyboardMapping defaults{};
    for (size_t i = 0; i < defaults.size(); i++)
        defaults[i] = uint16_t(settings::DefaultValue(hp::IdOf(i)));
    const KeyboardMapping wasd = hp::BuiltinMapping(hp::Builtin::Wasd, defaults);
    KeyboardMapping mine = defaults;
    mine[2 * size_t(hid::keymap::Control::A)] = Code("f");

    printf("---- the files' text ----\n");
    KeyboardStore store{ "Mine \"1\"", { { "Mine \"1\"", mine }, { "Other", wasd } } };
    Parsed<KeyboardStore> back = ParseKeyboard(Serialize(store), defaults);
    Check(back.ok && back.store == store && back.skipped.empty(), "a keyboard store reads back the same (a name with quotes too)");
    PadStore pads{ "Swapped", { { "Swapped", hp::DefaultPad() } } };
    std::swap(pads.custom[0].buttons[0], pads.custom[0].buttons[1]);
    Parsed<PadStore> padBack = ParsePad(Serialize(pads));
    Check(padBack.ok && padBack.store == pads && padBack.skipped.empty(), "a controller store reads back the same");

    Parsed<KeyboardStore> partial = ParseKeyboard("active = \"P\"\n[[profile]]\nname = \"P\"\na = [\"f\", \"none\"]\n", defaults);
    Check(partial.ok && partial.store.custom.size() == 1 && partial.store.custom[0].keys == mine, "a control left out takes Default's keys");
    Parsed<KeyboardStore> odd = ParseKeyboard(
        "active = \"nobody\"\n"
        "[[profile]]\nname = \"P\"\nno_such_control = [\"f\", \"none\"]\na = [\"f\"]\nb = [\"no_such_key\", \"none\"]\n"
        "[[profile]]\nname = \"P\"\n"
        "[[profile]]\nname = \"WASD\"\n"
        "[[profile]]\nname = \"\"\n", defaults);
    Check(odd.ok && odd.store.custom.size() == 1, "one usable profile of five");
    Check(odd.skipped.size() == 7, "seven entries named as skipped, got " + std::to_string(odd.skipped.size()));
    Check(odd.store.active == "Default", "an active name that names no profile reads as Default");
    Check(ParseKeyboard("active = \"wasd\"\n", defaults).store.active == "WASD", "a built-in's name in any case is the built-in");
    Parsed<KeyboardStore> corrupt = ParseKeyboard("active = \"Custom\n[[profile\n", defaults);
    Check(!corrupt.ok && !corrupt.error.empty(), "corrupt text doesn't read: " + corrupt.error);
    Check(!ParsePad("active = [\n").ok, "a corrupt controller file doesn't read");
    Check(ParsePad("[[profile]]\nname = \"X\"\na = \"left_paddle1\"\n").skipped.size() == 1, "a paddle isn't a v1 button");

    printf("---- startup keeps [keys] ----\n");
    Startup s = DecideKeyboard(FileState::Missing, {}, defaults, defaults);
    Check(s.write && s.store.active == "Default" && s.store.custom.empty(), "first launch, default [keys]: Default");
    s = DecideKeyboard(FileState::Missing, {}, mine, defaults);
    Check(s.write && s.store.active == "Custom" && s.store.custom.size() == 1 && s.store.custom[0].keys == mine,
        "first launch, customized [keys]: kept exactly as \"Custom\"");
    s = DecideKeyboard(FileState::Missing, {}, wasd, defaults);
    Check(s.store.active == "WASD" && s.store.custom.empty(), "first launch, [keys] equal to WASD: WASD");
    KeyboardStore onFile{ "Other", { { "Other", wasd } } };
    s = DecideKeyboard(FileState::Parsed, onFile, wasd, defaults);
    Check(!s.write && s.store == onFile, "[keys] is the active profile: nothing to do");
    s = DecideKeyboard(FileState::Parsed, onFile, mine, defaults);
    Check(s.write && s.store.active == "Custom (settings.toml)" && s.store.custom.size() == 2 && s.store.custom[1].keys == mine &&
            s.store.custom[0] == onFile.custom[0],
        "a [keys] the profiles didn't know of is kept as a profile, and the others stay");
    s = DecideKeyboard(FileState::Parsed, onFile, defaults, defaults, false);
    Check(s.apply && !s.write && s.store == onFile, "settings.toml not loaded: the active profile restores the keys");
    s = DecideKeyboard(FileState::Unreadable, {}, mine, defaults);
    Check(s.disabled && !s.write, "an unusable keyboard.toml: profiles off, nothing written");

    printf("---- an edit never changes a built-in ----\n");
    KeyboardStore e = AfterEdit({ "WASD", {} }, mine, defaults);
    Check(e.active == "WASD (custom)" && e.custom.size() == 1 && e.custom[0].keys == mine, "WASD edited: a copy \"WASD (custom)\"");
    KeyboardMapping mine2 = mine;
    mine2[0] = Code("t");
    e.active = "WASD";
    e = AfterEdit(e, mine2, defaults);
    Check(e.active == "WASD (custom 2)" && e.custom.size() == 2, "a second copy has its own name");
    e = AfterEdit(e, mine, defaults);
    Check(e.active == "WASD (custom 2)" && e.custom[1].keys == mine, "a custom profile takes the edit itself");
    Check(AfterEdit(KeyboardStore{ "WASD", {} }, wasd, defaults) == KeyboardStore{ "WASD", {} }, "no change, no copy");
    PadMapping swapped = hp::DefaultPad();
    std::swap(swapped[0], swapped[1]);
    PadStore pe = AfterEdit(PadStore{}, swapped);
    Check(pe.active == "Default (custom)" && pe.custom.size() == 1, "the controller's Default edited: a copy");

    printf("---- the running store ----\n");
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("recarbon_profiles_test_" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "profiles");
    settings::Load(dir / "settings.toml");
    Init(true, dir / "profiles");
    Check(ActiveKeyboard() == "Default" && ReadFile(dir / "profiles/keyboard.toml").find("active = \"Default\"") != std::string::npos,
        "a fresh folder: Default, and keyboard.toml written");
    Check(std::filesystem::exists(dir / "profiles/controller.toml"), "controller.toml written");
    Check(SwitchKeyboard("WASD") && ActiveKeyboard() == "WASD", "switch to WASD");
    KeyboardMapping now{};
    for (size_t i = 0; i < now.size(); i++)
        now[i] = uint16_t(settings::GetStoredValue(hp::IdOf(i)));
    Check(now == wasd, "the Key options are WASD's at once (no restart)");
    Check(ReadFile(dir / "profiles/keyboard.toml").find("active = \"WASD\"") != std::string::npos, "keyboard.toml says WASD");
    settings::Set(settings::Id::KeyA, double(Code("f")));
    Poll();
    Check(ActiveKeyboard() == "WASD (custom)", "an edit with WASD active: \"WASD (custom)\", got " + ActiveKeyboard());
    Check(ReadFile(dir / "profiles/keyboard.toml").find("name = \"WASD (custom)\"") != std::string::npos, "the copy is in the file");
    Check(!SwitchKeyboard("Nobody") && ActiveKeyboard() == "WASD (custom)", "an unknown name changes nothing");

    const std::string before = ReadFile(dir / "profiles/keyboard.toml");
    chmod((dir / "profiles").c_str(), 0555);
    const bool switched = SwitchKeyboard("Arrows");
    chmod((dir / "profiles").c_str(), 0755);
    for (size_t i = 0; i < now.size(); i++)
        now[i] = uint16_t(settings::GetStoredValue(hp::IdOf(i)));
    Check(!switched && ActiveKeyboard() == "WASD (custom)", "a write that fails: no switch");
    Check(now[2 * size_t(hid::keymap::Control::A)] == Code("f") && now[0] == Code("w"), "...and the Key options are as they were");
    Check(ReadFile(dir / "profiles/keyboard.toml") == before && !LastError().empty(), "...the file too, and LastError says why: " + LastError());
    Check(SwitchKeyboard("Arrows") && ActiveKeyboard() == "Arrows", "once writable again, the switch works");

    Check(SetPadButton(0, SDL_GAMEPAD_BUTTON_EAST) && ActivePad() == "Default (custom)" && CurrentPad()[0] == SDL_GAMEPAD_BUTTON_EAST &&
            CurrentPad()[1] == SDL_GAMEPAD_BUTTON_SOUTH,
        "a controller button rebound: a custom copy (A and B swapped), in effect at once");
    Check(!SetPadButton(0, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1), "a paddle can't be bound in v1");
    Check(SwitchPad("Default") && CurrentPad() == hp::DefaultPad(), "back to the controller's Default");
    std::filesystem::remove_all(dir);

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
