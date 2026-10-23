// ReCarbon runtime (based on SpeedBreaker's settings.cpp). GPL-3.0-or-later (see COPYING).
//
// Writing a small file so that it's always either the old one or the whole
// new one: a temporary file of this process's own in the same directory,
// flushed to disk (F_FULLFSYNC on macOS), renamed over the file, then the
// directory synced. Through a symlink, the file it points at is replaced.
// Temporary files a crashed write left (a minute old) are removed first and
// logged with `tag` ("settings", "profiles"). Moved unchanged out of
// user/settings.cpp (ReCarbon M8) so settings.toml and the input profiles
// share it.
#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace atomicfile
{
    // Returns "" or why it failed (the old file is then untouched).
    std::string Write(const std::filesystem::path& path, std::string_view text, const char* tag);
}
