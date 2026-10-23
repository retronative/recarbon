// ReCarbon runtime. GPL-3.0-or-later (see COPYING). See profiles.h.
#include "profiles.h"

#include <user/atomic_file.h>
#include <user/key_names.h>
#include <user/paths.h>
#include <user/settings.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <format>
#include <fstream>
#include <mutex>
#include <sstream>

// toml++, as user/settings.cpp includes it.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#if __has_include(<toml++/toml.hpp>)
#include <toml++/toml.hpp>
#else
#include "../../tools/XenonRecomp/thirdparty/tomlplusplus/include/toml++/toml.hpp"
#endif
#pragma clang diagnostic pop

namespace profiles
{
    namespace
    {
        namespace hp = hid::profiles;
        constexpr size_t kMaxFileSize = 256 * 1024;

        bool SameIgnoringCase(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
            });
        }

        std::string Quote(std::string_view s)
        {
            std::string out = "\"";
            for (char c : s)
            {
                if (c == '"' || c == '\\')
                    out += '\\';
                out += c;
            }
            return out + "\"";
        }

        // The control a Key option pair belongs to is named like the first
        // option's key in settings.toml: "right_trigger".
        std::string_view ControlName(size_t control)
        {
            return settings::Info(hp::IdOf(2 * control)).key;
        }

        std::string Token(uint16_t code)
        {
            const char* t = settings::keynames::Token(code);
            return t ? t : "none";
        }

        template <class S>
        bool ParseRoot(std::string_view text, toml::table& root, Parsed<S>& out)
        {
#if TOML_EXCEPTIONS
            try
            {
                root = toml::parse(text);
            }
            catch (const toml::parse_error& e)
            {
                out.error = std::format("doesn't parse: {} (line {})", e.description(), e.source().begin.line);
                return false;
            }
#else
            toml::parse_result result = toml::parse(text);
            if (!result)
            {
                out.error = std::format("doesn't parse: {} (line {})", result.error().description(), result.error().source().begin.line);
                return false;
            }
            root = std::move(result).table();
#endif
            return true;
        }

        // The profiles' tables, with a valid name each (others skipped).
        template <class S, class Each>
        void ForEachProfile(const toml::table& root, Parsed<S>& out, bool keyboard, Each each)
        {
            const toml::array* list = root["profile"].as_array();
            if (!list)
                return;
            for (const toml::node& node : *list)
            {
                const toml::table* t = node.as_table();
                const toml::value<std::string>* n = t ? t->get_as<std::string>("name") : nullptr;
                const std::optional<std::string> name = n ? std::optional<std::string>(n->get()) : std::nullopt;
                if (!name || !ValidName(*name) || IsBuiltin(*name, keyboard))
                {
                    out.skipped.push_back(std::format("a profile without a usable name ({})", name ? *name : "none"));
                    continue;
                }
                if (std::any_of(out.store.custom.begin(), out.store.custom.end(), [&](const auto& p) { return p.name == *name; }))
                {
                    out.skipped.push_back(std::format("a second profile named \"{}\"", *name));
                    continue;
                }
                each(*t, *name);
            }
        }

        template <class S>
        void ReadActive(const toml::table& root, Parsed<S>& out, bool keyboard)
        {
            if (auto a = root.get_as<std::string>("active"))
                out.store.active = a->get();
            const bool known = IsBuiltin(out.store.active, keyboard) ||
                std::any_of(out.store.custom.begin(), out.store.custom.end(), [&](const auto& p) { return p.name == out.store.active; });
            if (!known)
            {
                out.skipped.push_back(std::format("the active profile \"{}\" (none has that name): Default instead", out.store.active));
                out.store.active = "Default";
            }
            else if (IsBuiltin(out.store.active, keyboard))
            {
                // The built-in's own spelling ("wasd" in a hand-edited file is WASD).
                if (keyboard)
                {
                    for (std::string_view b : hp::kBuiltinNames)
                        if (SameIgnoringCase(b, out.store.active))
                            out.store.active = std::string(b);
                }
                else
                    out.store.active = "Default";
            }
        }

        template <class P>
        std::string UniqueCopyName(const std::vector<P>& custom, std::string_view base)
        {
            for (int n = 1;; n++)
            {
                std::string name = n == 1 ? std::format("{} (custom)", base) : std::format("{} (custom {})", base, n);
                if (std::none_of(custom.begin(), custom.end(), [&](const P& p) { return p.name == name; }))
                    return name;
            }
        }

        enum class Read : uint8_t { Missing, Ok, Failed };
        Read ReadText(const std::filesystem::path& path, std::string& text, std::string& error)
        {
            std::error_code ec;
            const auto status = std::filesystem::status(path, ec);
            if (ec || status.type() == std::filesystem::file_type::not_found)
                return ec && ec != std::errc::no_such_file_or_directory ? (error = ec.message(), Read::Failed) : Read::Missing;
            if (status.type() != std::filesystem::file_type::regular)
                return error = "it isn't a regular file", Read::Failed;
            if (std::filesystem::file_size(path, ec) > kMaxFileSize)
                return error = "it's too big for a profiles file", Read::Failed;
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return error = "it can't be opened", Read::Failed;
            std::ostringstream s;
            s << in.rdbuf();
            text = s.str();
            return Read::Ok;
        }
    }

    bool IsBuiltin(std::string_view name, bool keyboard)
    {
        if (!keyboard)
            return SameIgnoringCase(name, "Default");
        return std::any_of(std::begin(hp::kBuiltinNames), std::end(hp::kBuiltinNames), [&](std::string_view b) { return SameIgnoringCase(b, name); });
    }

    bool ValidName(std::string_view name)
    {
        if (name.empty() || name.size() > 32)
            return false;
        return std::all_of(name.begin(), name.end(), [](char c) { return c >= 0x20 && c <= 0x7E; });
    }

    std::string Serialize(const KeyboardStore& store)
    {
        std::string out = "# ReCarbon keyboard profiles (Settings > Controls), written by the game. The built-in\n"
                          "# profiles (Default, WASD, Arrows) aren't stored. Each control: its first and second key,\n"
                          "# named as in settings.toml's [keys]. A control left out takes Default's keys.\n";
        out += std::format("active = {}\n", Quote(store.active));
        for (const KeyboardProfile& p : store.custom)
        {
            out += std::format("\n[[profile]]\nname = {}\n", Quote(p.name));
            for (size_t c = 0; c < hid::keymap::kControls; c++)
                out += std::format("{} = [\"{}\", \"{}\"]\n", ControlName(c), Token(p.keys[2 * c]), Token(p.keys[2 * c + 1]));
        }
        return out;
    }

    std::string Serialize(const PadStore& store)
    {
        std::string out = "# ReCarbon controller profiles (Settings > Controller), written by the game. The built-in\n"
                          "# Default isn't stored. Each Xbox button: the controller button that drives it, by position\n"
                          "# (south is A on an Xbox pad, Cross on a PlayStation one). Buttons only: the sticks and\n"
                          "# triggers aren't remapped.\n";
        out += std::format("active = {}\n", Quote(store.active));
        for (const PadProfile& p : store.custom)
        {
            out += std::format("\n[[profile]]\nname = {}\n", Quote(p.name));
            for (size_t i = 0; i < hp::kPadButtons; i++)
                out += std::format("{} = \"{}\"\n", hp::kPadButtonNames[i], hp::FindSdlButton(p.buttons[i])->name);
        }
        return out;
    }

    Parsed<KeyboardStore> ParseKeyboard(std::string_view text, const KeyboardMapping& defaults)
    {
        Parsed<KeyboardStore> out;
        toml::table root;
        if (!ParseRoot(text, root, out))
            return out;
        ForEachProfile(root, out, true, [&](const toml::table& t, const std::string& name) {
            KeyboardProfile p{ name, defaults };
            for (auto&& [key, node] : t)
            {
                if (key.str() == "name")
                    continue;
                size_t control = hid::keymap::kControls;
                for (size_t c = 0; c < hid::keymap::kControls; c++)
                    if (ControlName(c) == key.str())
                        control = c;
                const toml::array* pair = node.as_array();
                uint16_t codes[2] = { settings::keynames::kUnknown, settings::keynames::kUnknown };
                if (pair && pair->size() == 2)
                    for (size_t i = 0; i < 2; i++)
                        if (auto token = pair->get_as<std::string>(i))
                            codes[i] = settings::keynames::Code(token->get());
                if (control == hid::keymap::kControls || codes[0] == settings::keynames::kUnknown || codes[1] == settings::keynames::kUnknown)
                {
                    out.skipped.push_back(std::format("{}.{}", name, key.str()));
                    continue;
                }
                p.keys[2 * control] = codes[0];
                p.keys[2 * control + 1] = codes[1];
            }
            out.store.custom.push_back(std::move(p));
        });
        ReadActive(root, out, true);
        out.ok = true;
        return out;
    }

    Parsed<PadStore> ParsePad(std::string_view text)
    {
        Parsed<PadStore> out;
        toml::table root;
        if (!ParseRoot(text, root, out))
            return out;
        ForEachProfile(root, out, false, [&](const toml::table& t, const std::string& name) {
            PadProfile p{ name, hp::DefaultPad() };
            for (auto&& [key, node] : t)
            {
                if (key.str() == "name")
                    continue;
                size_t index = hp::kPadButtons;
                for (size_t i = 0; i < hp::kPadButtons; i++)
                    if (hp::kPadButtonNames[i] == key.str())
                        index = i;
                const auto value = node.value<std::string>();
                const hp::SdlName* button = value ? hp::FindSdlButton(*value) : nullptr;
                if (index == hp::kPadButtons || !button)
                {
                    out.skipped.push_back(std::format("{}.{}", name, key.str()));
                    continue;
                }
                p.buttons[index] = button->button;
            }
            out.store.custom.push_back(std::move(p));
        });
        ReadActive(root, out, false);
        out.ok = true;
        return out;
    }

    std::optional<KeyboardMapping> Find(const KeyboardStore& store, std::string_view name, const KeyboardMapping& defaults)
    {
        for (size_t b = 0; b < size_t(hp::Builtin::Count); b++)
            if (SameIgnoringCase(hp::kBuiltinNames[b], name))
                return hp::BuiltinMapping(hp::Builtin(b), defaults);
        for (const KeyboardProfile& p : store.custom)
            if (p.name == name)
                return p.keys;
        return std::nullopt;
    }

    std::optional<PadMapping> Find(const PadStore& store, std::string_view name)
    {
        if (IsBuiltin(name, false))
            return hp::DefaultPad();
        for (const PadProfile& p : store.custom)
            if (p.name == name)
                return p.buttons;
        return std::nullopt;
    }

    Startup DecideKeyboard(FileState state, const KeyboardStore& parsed, const KeyboardMapping& keys, const KeyboardMapping& defaults)
    {
        Startup s;
        if (state == FileState::Unreadable)
        {
            s.disabled = true;
            s.log = "keyboard.toml can't be used: the game runs on settings.toml's [keys], and profiles are off this session";
            return s;
        }
        if (state == FileState::Missing)
        {
            s.write = true;
            for (size_t b = 0; b < size_t(hp::Builtin::Count); b++)
                if (hp::BuiltinMapping(hp::Builtin(b), defaults) == keys)
                {
                    s.store.active = std::string(hp::kBuiltinNames[b]);
                    s.log = std::format("no keyboard.toml yet: [keys] is {}, which is active", s.store.active);
                    return s;
                }
            s.store.custom.push_back({ "Custom", keys });
            s.store.active = "Custom";
            s.log = "no keyboard.toml yet: [keys] is kept as the profile \"Custom\", which is active";
            return s;
        }
        s.store = parsed;
        if (Find(parsed, parsed.active, defaults) == keys)
        {
            s.log = std::format("keyboard profile \"{}\"", parsed.active);
            return s;
        }
        // [keys] changed without the profiles knowing: keep it, as a profile.
        constexpr std::string_view kAdopted = "Custom (settings.toml)";
        auto it = std::find_if(s.store.custom.begin(), s.store.custom.end(), [&](const KeyboardProfile& p) { return p.name == kAdopted; });
        if (it != s.store.custom.end())
            it->keys = keys;
        else
            s.store.custom.push_back({ std::string(kAdopted), keys });
        s.store.active = std::string(kAdopted);
        s.write = true;
        s.log = std::format("settings.toml's [keys] isn't the active profile \"{}\": kept as \"{}\", which is active", parsed.active, kAdopted);
        return s;
    }

    KeyboardStore AfterEdit(KeyboardStore store, const KeyboardMapping& keys, const KeyboardMapping& defaults)
    {
        if (Find(store, store.active, defaults) == keys)
            return store;
        for (KeyboardProfile& p : store.custom)
            if (p.name == store.active)
            {
                p.keys = keys;
                return store;
            }
        std::string base = IsBuiltin(store.active, true) ? store.active : "Default";
        store.active = UniqueCopyName(store.custom, base);
        store.custom.push_back({ store.active, keys });
        return store;
    }

    PadStore AfterEdit(PadStore store, const PadMapping& buttons)
    {
        if (Find(store, store.active) == buttons)
            return store;
        for (PadProfile& p : store.custom)
            if (p.name == store.active)
            {
                p.buttons = buttons;
                return store;
            }
        store.active = UniqueCopyName(store.custom, "Default");
        store.custom.push_back({ store.active, buttons });
        return store;
    }

    // ---- The running game's profiles ----
    namespace
    {
        std::filesystem::path s_folder;
        KeyboardMapping s_defaults{};
        KeyboardStore s_keyboard;
        PadStore s_pad;
        bool s_keyboardEnabled = false, s_padEnabled = false;
        bool s_applying = false;    // SwitchKeyboard is setting the Key options
        bool s_keysChanged = false;  // since the last Poll
        std::string s_error;
        std::mutex s_padMutex;      // s_padMapping: read by the input code
        PadMapping s_padMapping = hp::DefaultPad();

        KeyboardMapping KeysInEffect()
        {
            KeyboardMapping m{};
            for (size_t i = 0; i < m.size(); i++)
                m[i] = uint16_t(settings::GetStoredValue(hp::IdOf(i)));
            return m;
        }

        // Validated, atomic: the text must parse back to `store` before it's
        // written. On failure nothing changes but s_error.
        template <class S, class ParseFn>
        bool WriteStore(const char* file, const S& store, ParseFn parse)
        {
            const std::string text = Serialize(store);
            auto back = parse(text);
            if (!back.ok || !(back.store == store) || !back.skipped.empty())
            {
                s_error = std::format("{}: what would be written doesn't read back the same; not written", file);
                fprintf(stderr, "[profiles] %s\n", s_error.c_str());
                return false;
            }
            if (std::string failed = atomicfile::Write(s_folder / file, text, "profiles"); !failed.empty())
            {
                s_error = std::format("{}: {}", file, failed);
                fprintf(stderr, "[profiles] couldn't save: %s\n", s_error.c_str());
                return false;
            }
            s_error.clear();
            return true;
        }
        bool WriteKeyboard(const KeyboardStore& store)
        {
            return WriteStore("keyboard.toml", store, [](std::string_view t) { return ParseKeyboard(t, s_defaults); });
        }
        bool WritePad(const PadStore& store)
        {
            return WriteStore("controller.toml", store, [](std::string_view t) { return ParsePad(t); });
        }

        void PublishPad()
        {
            PadMapping m = Find(s_pad, s_pad.active).value_or(hp::DefaultPad());
            std::lock_guard lock(s_padMutex);
            s_padMapping = m;
        }
    }

    void Init(const std::filesystem::path& folder)
    {
        s_folder = folder.empty() ? GetUserPath() / "profiles" : folder;
        for (size_t i = 0; i < s_defaults.size(); i++)
            s_defaults[i] = uint16_t(settings::DefaultValue(hp::IdOf(i)));

        std::string text, error;
        Read r = ReadText(s_folder / "keyboard.toml", text, error);
        Parsed<KeyboardStore> parsed;
        FileState state = r == Read::Missing ? FileState::Missing : FileState::Unreadable;
        if (r == Read::Ok)
        {
            parsed = ParseKeyboard(text, s_defaults);
            if (parsed.ok)
                state = FileState::Parsed;
            else
                error = parsed.error;
            for (const std::string& s : parsed.skipped)
                fprintf(stderr, "[profiles] keyboard.toml: skipped %s\n", s.c_str());
        }
        Startup start = DecideKeyboard(state, parsed.store, KeysInEffect(), s_defaults);
        fprintf(stderr, "[profiles] %s%s%s\n", start.log.c_str(), error.empty() ? "" : ": ", error.c_str());
        s_keyboardEnabled = !start.disabled;
        if (start.disabled)
            s_error = std::format("keyboard.toml: {}", error);
        s_keyboard = start.store;
        if (start.write && !WriteKeyboard(start.store))
            fprintf(stderr, "[profiles] keyboard profiles not saved this time; [keys] is unchanged\n");

        text.clear();
        error.clear();
        r = ReadText(s_folder / "controller.toml", text, error);
        if (r == Read::Ok)
        {
            Parsed<PadStore> pad = ParsePad(text);
            for (const std::string& s : pad.skipped)
                fprintf(stderr, "[profiles] controller.toml: skipped %s\n", s.c_str());
            if (pad.ok)
                s_pad = pad.store, s_padEnabled = true;
            else
                error = pad.error;
        }
        if (r == Read::Missing)
        {
            s_padEnabled = true;
            if (!WritePad(s_pad))
                fprintf(stderr, "[profiles] controller profiles not saved this time\n");
        }
        if (!s_padEnabled)
        {
            fprintf(stderr, "[profiles] controller.toml can't be used (%s): the default mapping this session\n", error.c_str());
            if (s_error.empty())
                s_error = std::format("controller.toml: {}", error);
        }
        PublishPad();
        fprintf(stderr, "[profiles] controller profile \"%s\"\n", s_pad.active.c_str());

        settings::Subscribe([](settings::Id id) {
            if (!s_applying && id >= settings::Id::KeyRightTrigger && hp::SlotOf(id) < hp::kKeySlots)
                s_keysChanged = true;
        });
    }

    void Poll()
    {
        if (!s_keysChanged)
            return;
        s_keysChanged = false;
        if (!s_keyboardEnabled)
            return;
        KeyboardStore next = AfterEdit(s_keyboard, KeysInEffect(), s_defaults);
        if (next == s_keyboard)
            return;
        if (WriteKeyboard(next))
        {
            if (next.active != s_keyboard.active)
                fprintf(stderr, "[profiles] keys changed with \"%s\" active (built in): saved as \"%s\", which is active\n",
                    s_keyboard.active.c_str(), next.active.c_str());
            s_keyboard = std::move(next);
        }
    }

    bool KeyboardEnabled() { return s_keyboardEnabled; }
    std::string LastError() { return s_error; }

    std::vector<std::string> KeyboardNames()
    {
        std::vector<std::string> names(std::begin(hp::kBuiltinNames), std::end(hp::kBuiltinNames));
        for (const KeyboardProfile& p : s_keyboard.custom)
            names.push_back(p.name);
        return names;
    }
    std::string ActiveKeyboard() { return s_keyboard.active; }

    bool SwitchKeyboard(std::string_view name)
    {
        std::optional<KeyboardMapping> m = s_keyboardEnabled ? Find(s_keyboard, name, s_defaults) : std::nullopt;
        if (!m)
            return false;
        KeyboardStore next = s_keyboard;
        next.active = std::string(name);
        if (!WriteKeyboard(next))
            return false;
        s_keyboard = std::move(next);
        s_applying = true;
        for (size_t i = 0; i < m->size(); i++)
            if (uint16_t(settings::GetStoredValue(hp::IdOf(i))) != (*m)[i])
                settings::Set(hp::IdOf(i), double((*m)[i]));
        s_applying = false;
        fprintf(stderr, "[profiles] keyboard profile \"%s\"\n", s_keyboard.active.c_str());
        return true;
    }

    std::vector<std::string> PadNames()
    {
        std::vector<std::string> names{ "Default" };
        for (const PadProfile& p : s_pad.custom)
            names.push_back(p.name);
        return names;
    }
    std::string ActivePad() { return s_pad.active; }

    bool SwitchPad(std::string_view name)
    {
        if (!s_padEnabled || !Find(s_pad, name))
            return false;
        PadStore next = s_pad;
        next.active = std::string(name);
        if (!WritePad(next))
            return false;
        s_pad = std::move(next);
        PublishPad();
        fprintf(stderr, "[profiles] controller profile \"%s\"\n", s_pad.active.c_str());
        return true;
    }

    PadMapping CurrentPad()
    {
        std::lock_guard lock(s_padMutex);
        return s_padMapping;
    }

    bool SetPadButton(size_t index, SDL_GamepadButton button)
    {
        if (!s_padEnabled || index >= hp::kPadButtons || !hp::FindSdlButton(button))
            return false;
        PadMapping m = Find(s_pad, s_pad.active).value_or(hp::DefaultPad());
        m[index] = button;
        PadStore next = AfterEdit(s_pad, m);
        if (next == s_pad)
            return true;
        if (!WritePad(next))
            return false;
        s_pad = std::move(next);
        PublishPad();
        fprintf(stderr, "[profiles] controller: %.*s = %.*s in \"%s\"\n", int(hp::kPadButtonNames[index].size()), hp::kPadButtonNames[index].data(),
            int(hp::FindSdlButton(button)->name.size()), hp::FindSdlButton(button)->name.data(), s_pad.active.c_str());
        return true;
    }
}
