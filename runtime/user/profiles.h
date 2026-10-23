// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Input profiles in the user folder (ROADMAP M8, reference/M8_PLAN.md B2):
// profiles/keyboard.toml and profiles/controller.toml, each the active
// profile's name and the custom profiles. The mappings are hid/profiles.h's.
//   - The built-in profiles (Default, WASD, Arrows; the controller's Default)
//     are never stored or edited: an edit while one is active makes a custom
//     copy, "<name> (custom)", and makes it active.
//   - The keyboard's bindings in effect are always the Key options, settings.toml's
//     [keys] (the input code reads those): switching profiles sets them, and
//     an edit to them (Settings > Controls) updates the active custom profile.
//   - Startup never changes [keys], so no binding is lost. With no
//     keyboard.toml (the first launch of a build with profiles), [keys] becomes
//     the active profile: Default if it equals the defaults, else a custom
//     "Custom". With a keyboard.toml whose active profile isn't [keys] (keys
//     changed by a build without profiles, or by hand), [keys] is kept as the
//     custom "Custom (settings.toml)" and made active. A keyboard.toml that
//     can't be read or doesn't parse is left untouched, the game runs on [keys],
//     and profiles are off for the session (Settings > Controls says why).
//   - Every write is validated (written text parsed back and compared) and
//     atomic (user/atomic_file.h); a failed one leaves the file and the
//     in-memory profiles as they were, and LastError says why.
//   - Controller: the active mapping replaces hid/pad_map.h's default in the
//     input code; buttons only (v1).
#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <hid/profiles.h>

namespace profiles
{
    using hid::profiles::KeyboardMapping;
    using hid::profiles::PadMapping;

    struct KeyboardProfile
    {
        std::string name;
        KeyboardMapping keys{};
        bool operator==(const KeyboardProfile&) const = default;
    };
    struct PadProfile
    {
        std::string name;
        PadMapping buttons{};
        bool operator==(const PadProfile&) const = default;
    };
    template <class Profile>
    struct Store
    {
        std::string active = "Default";
        std::vector<Profile> custom;
        bool operator==(const Store&) const = default;
    };
    using KeyboardStore = Store<KeyboardProfile>;
    using PadStore = Store<PadProfile>;

    // A custom profile's name: 1 to 32 printable ASCII characters, not a
    // built-in's ("Default", "WASD", "Arrows").
    bool ValidName(std::string_view name);
    bool IsBuiltin(std::string_view name, bool keyboard);

    // The files' text.
    std::string Serialize(const KeyboardStore& store);
    std::string Serialize(const PadStore& store);
    template <class S>
    struct Parsed
    {
        bool ok = false;
        S store;
        std::string error;                 // why it isn't ok
        std::vector<std::string> skipped;  // entries left out, for the log
    };
    // A control missing from a profile takes `defaults`' keys; an unknown or
    // unusable entry is skipped (and named), as is a profile with a bad or
    // repeated name. An active name that names no profile reads as Default.
    Parsed<KeyboardStore> ParseKeyboard(std::string_view text, const KeyboardMapping& defaults);
    Parsed<PadStore> ParsePad(std::string_view text);

    // The mapping a name stands for in `store`, or nullopt.
    std::optional<KeyboardMapping> Find(const KeyboardStore& store, std::string_view name, const KeyboardMapping& defaults);
    std::optional<PadMapping> Find(const PadStore& store, std::string_view name);

    // Startup, decided from what was found (pure; Init does the I/O).
    enum class FileState : uint8_t { Missing, Parsed, Unreadable };
    struct Startup
    {
        KeyboardStore store;
        bool write = false;     // store the result
        bool apply = false;     // set the Key options to the active profile's keys
        bool disabled = false;  // the file can't be used: run on [keys], write nothing
        std::string log;        // what was decided, for the log
    };
    // `keysFromFile`: settings.toml was found and loaded, so the Key options
    // are the player's [keys]. Without it (no settings.toml, or one that
    // didn't parse and was set aside) they are only the defaults, and a
    // usable keyboard.toml's active profile restores them.
    Startup DecideKeyboard(FileState state, const KeyboardStore& parsed, const KeyboardMapping& keys, const KeyboardMapping& defaults,
        bool keysFromFile = true);

    // After the bindings in effect changed to `keys`: the active custom
    // profile takes them; with a built-in active, a custom copy named
    // "<name> (custom)" (or "<name> (custom 2)"...) does, and becomes active.
    KeyboardStore AfterEdit(KeyboardStore store, const KeyboardMapping& keys, const KeyboardMapping& defaults);
    PadStore AfterEdit(PadStore store, const PadMapping& buttons);

    // ---- The running game's profiles (main thread) ----

    // After settings::Load: reads or creates the files (GetUserPath()/profiles,
    // or `folder`), and follows the Key options from then on.
    // `keysFromFile` as for DecideKeyboard.
    void Init(bool keysFromFile, const std::filesystem::path& folder = {});
    // Once a frame: writes what the Key options' changes since the last
    // call amount to (one write for a whole Reset page).
    void Poll();
    bool KeyboardEnabled();  // false: keyboard.toml couldn't be used this session
    std::string LastError();  // why the last write or the startup failed ("" if fine)

    // The keyboard profiles in the order the menu shows them (the built-ins,
    // then the custom ones), and the active one.
    std::vector<std::string> KeyboardNames();
    std::string ActiveKeyboard();
    // Makes `name` active and sets the Key options to its keys (at once, no
    // restart). False (nothing changed) for an unknown name, or when the write
    // fails (LastError).
    bool SwitchKeyboard(std::string_view name);

    std::vector<std::string> PadNames();
    std::string ActivePad();
    bool SwitchPad(std::string_view name);
    // The active controller mapping (any thread: a copy under a lock).
    PadMapping CurrentPad();
    // Binds XInput button `index` (hid/pad_map.h's order) to `button`; a
    // built-in active makes a custom copy first. False if the write fails.
    bool SetPadButton(size_t index, SDL_GamepadButton button);
}
