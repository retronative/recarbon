// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// The privacy pass of a performance report (report/perf_report.h), header
// only so tests/perf_report_test.cpp can include it without the runtime.
//
//   Scrub (applied to the whole text before it is written):
//     1. the home folder (and its real path) as "~", whole folders only:
//        the bug report's ScrubHome (bug_report.cpp) calls ReplaceFolders too;
//     2. a path under the home folder or another user's ("~/...", "/Users/...",
//        "/home/...", "/Volumes/...", "/private/...", "/var/folders/...")
//        as "<path>": "~/Desktop" is still a path;
//     3. a guest address ("0x8" and 7 hex digits; "sub_" and 8 hex digits)
//        as "<guest-address>";
//     4. the login and host names, and the account's full name and each
//        of its words of three letters or more, as whole words in any case,
//        as "<user>" (not the host name's words: "Pro" in "Alices-MacBook-Pro"
//        is also the chip's "M1 Pro").
//   Forbidden (FindForbidden, on the scrubbed text; fails closed: a hit and
//   nothing is written): any path left (a "/" starting a token and followed
//   by a name and another "/"), "~/", a guest address, any of the names
//   above, and save, profile and game data names (".sav", "settings.toml",
//   "profiles/", ".xex", ".iso").
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace report::perfscrub
{
    struct Inputs
    {
        std::vector<std::string> homes;  // HOME and its real path
        std::vector<std::string> names;  // the account's full name: each word of 3+ characters too
        std::vector<std::string> whole;  // the login and host names: only as a whole
    };

    inline bool PathChar(char c) { return std::isalnum(uint8_t(c)) || c == '.' || c == '_' || c == '-'; }
    inline bool WordChar(char c) { return std::isalnum(uint8_t(c)) || c == '_'; }
    inline bool HexChar(char c) { return std::isxdigit(uint8_t(c)) != 0; }

    // Each folder in `folders` (longest first) as `with`, where the match is
    // the whole folder (/home/deck, not /home/deck2) and not the tail of a
    // longer path (/mnt/home/deck). From SpeedBreaker's bug report scrub.
    inline std::string ReplaceFolders(std::string text, std::vector<std::string> folders, std::string_view with)
    {
        for (std::string& f : folders)
            while (f.size() > 1 && f.back() == '/')
                f.pop_back();
        std::erase_if(folders, [](const std::string& f) { return f.size() <= 1; });
        std::sort(folders.begin(), folders.end(), [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
        for (const std::string& folder : folders)
        {
            std::string out;
            out.reserve(text.size());
            size_t from = 0;
            for (size_t at; (at = text.find(folder, from)) != std::string::npos;)
            {
                size_t end = at + folder.size();
                bool whole = (end == text.size() || !PathChar(text[end])) && (at == 0 || !PathChar(text[at - 1]));
                out.append(text, from, at - from);
                out += whole ? std::string(with) : folder;
                from = end;
            }
            out.append(text, from, std::string::npos);
            text.swap(out);
        }
        return text;
    }

    // Where a path token may start: the text's start or after a space, a
    // quote, "=", "(", "[", ":" or ",".
    inline bool TokenStart(const std::string& text, size_t at)
    {
        if (at == 0)
            return true;
        const char c = text[at - 1];
        return c == ' ' || c == '\t' || c == '\n' || c == '"' || c == '\'' || c == '=' || c == '(' || c == '[' || c == ':' || c == ',';
    }
    // A path runs to a space, a quote, a comma or a bracket.
    inline size_t PathEnd(const std::string& text, size_t at)
    {
        while (at < text.size() && !std::strchr(" \t\n\"',)]}", text[at]))
            at++;
        return at;
    }

    inline constexpr std::string_view kPrivatePrefixes[] = { "~/", "/Users/", "/home/", "/Volumes/", "/private/", "/var/folders/" };

    inline std::string ReplacePrivatePaths(const std::string& text)
    {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size();)
        {
            bool hit = false;
            if (TokenStart(text, i))
                for (std::string_view p : kPrivatePrefixes)
                    if (std::string_view(text).substr(i, p.size()) == p)
                    {
                        hit = true;
                        break;
                    }
            if (hit)
            {
                out += "<path>";
                i = PathEnd(text, i);
            }
            else
                out += text[i++];
        }
        return out;
    }

    // The length of a guest address at `at` (0: none): 0x8 and 7 hex digits,
    // or sub_ and 8, not inside a longer word or number.
    inline size_t GuestAddressAt(const std::string& text, size_t at)
    {
        if (at > 0 && WordChar(text[at - 1]))
            return 0;
        size_t prefix, digits;
        if (text.compare(at, 3, "0x8") == 0 || text.compare(at, 3, "0X8") == 0)
            prefix = 3, digits = 7;
        else if (text.compare(at, 4, "sub_") == 0)
            prefix = 4, digits = 8;
        else
            return 0;
        size_t end = at + prefix;
        for (size_t k = 0; k < digits; k++, end++)
            if (end >= text.size() || !HexChar(text[end]))
                return 0;
        return end < text.size() && WordChar(text[end]) ? 0 : end - at;
    }

    inline std::string ReplaceGuestAddresses(const std::string& text)
    {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size();)
            if (size_t n = GuestAddressAt(text, i))
            {
                out += "<guest-address>";
                i += n;
            }
            else
                out += text[i++];
        return out;
    }

    // The names to look for: each as given, and each word of three or more
    // characters in it (a full name's first and last name). Shorter ones
    // would eat "M5" or "GB".
    inline std::vector<std::string> NameWords(const std::vector<std::string>& names, const std::vector<std::string>& whole = {})
    {
        std::vector<std::string> words;
        for (const std::string& name : whole)
            if (name.size() >= 3)
                words.push_back(name);
        for (const std::string& name : names)
        {
            if (name.size() >= 3)
                words.push_back(name);
            std::string word;
            for (size_t i = 0; i <= name.size(); i++)
                if (i < name.size() && WordChar(name[i]))
                    word += name[i];
                else
                {
                    if (word.size() >= 3)
                        words.push_back(word);
                    word.clear();
                }
        }
        std::sort(words.begin(), words.end(), [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
        return words;
    }

    inline bool EqualNoCase(std::string_view a, std::string_view b)
    {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::tolower(uint8_t(x)) == std::tolower(uint8_t(y)); });
    }

    // Where `word` is in `text` as a whole word, any case, from `from`; npos: nowhere.
    inline size_t FindWord(const std::string& text, std::string_view word, size_t from = 0)
    {
        for (size_t i = from; i + word.size() <= text.size(); i++)
            if (EqualNoCase(std::string_view(text).substr(i, word.size()), word) && (i == 0 || !WordChar(text[i - 1]))
                && (i + word.size() == text.size() || !WordChar(text[i + word.size()])))
                return i;
        return std::string::npos;
    }

    inline std::string ReplaceNames(std::string text, const Inputs& in)
    {
        for (const std::string& word : NameWords(in.names, in.whole))
            for (size_t at = 0; (at = FindWord(text, word, at)) != std::string::npos; at += 6)
                text.replace(at, word.size(), "<user>");
        return text;
    }

    inline std::string Scrub(std::string text, const Inputs& in)
    {
        text = ReplaceFolders(std::move(text), in.homes, "~");
        text = ReplacePrivatePaths(text);
        text = ReplaceGuestAddresses(text);
        return ReplaceNames(std::move(text), in);
    }

    // What the denylist found first in `text`, or "" when nothing.
    inline std::string FindForbidden(const std::string& text, const Inputs& in)
    {
        for (size_t i = 0; i < text.size(); i++)
        {
            if (text[i] == '/' && TokenStart(text, i))
            {
                size_t j = i + 1;
                while (j < text.size() && PathChar(text[j]))
                    j++;
                if (j > i + 1 && j < text.size() && text[j] == '/')
                    return "a path";  // its kind only: the text stays out of the log and the toast
            }
            if (text.compare(i, 2, "~/") == 0)
                return "a home-folder path";
            if (GuestAddressAt(text, i))
                return "a guest address";
        }
        for (const std::string& home : in.homes)
            if (home.size() > 1 && text.find(home) != std::string::npos)
                return "the home folder";
        for (const std::string& word : NameWords(in.names, in.whole))
            if (FindWord(text, word) != std::string::npos)
                return "a user or host name";
        for (std::string_view bad : { ".sav", "settings.toml", "profiles/", ".xex", ".iso" })
            if (text.find(bad) != std::string::npos)
                return "a file name of saves, settings or game data (" + std::string(bad) + ")";
        return {};
    }
}
