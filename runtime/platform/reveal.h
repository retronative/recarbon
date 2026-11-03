// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// macOS: shows a file in Finder, selected (Settings > Advanced > Reveal
// Performance Report). Elsewhere there is none.
#pragma once
#include <filesystem>

namespace platform
{
    // True where Reveal exists (macOS).
    bool CanReveal();
    // Opens a Finder window on the file's folder with the file selected.
    // Any thread. False: not on this system, or no such file.
    bool RevealInFinder(const std::filesystem::path& path);
}
