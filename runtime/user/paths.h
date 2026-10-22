// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "portable.h"

// Extracted disc contents (default.xex, NFS/, Movies/): where startup found
// or installed the game (install::FindGameInstall, SetGamePath); before
// that, RECARBON_GAME_DIR or the repo layout.
inline std::filesystem::path& GamePathStorage()
{
    static std::filesystem::path path;  // set once, before the game starts
    return path;
}

inline void SetGamePath(std::filesystem::path path)
{
    GamePathStorage() = std::move(path);
}

inline std::filesystem::path GetGamePath()
{
    if (!GamePathStorage().empty())
        return GamePathStorage();
    if (const char* dir = std::getenv("RECARBON_GAME_DIR"))
        return dir;
    return "game/files";
}

// The per-user folders are named for the project: ReCarbon (macOS) or
// recarbon (Linux). ReCarbon change: there are no previous releases, so
// SpeedBreaker's folder migration (nfsmw-recomp -> SpeedBreaker) is gone, and
// SpeedBreaker's own folders are never read or touched.
#ifdef __APPLE__
inline constexpr const char* kUserFolderName = "ReCarbon";
#else
inline constexpr const char* kUserFolderName = "recarbon";
#endif

// Where the per-user folders go (outside portable mode: user/portable.h).
//   macOS: ~/Library/Application Support, ~/Library/Caches
//   iOS: the same in the app's container; $XDG_DATA_HOME and
//        $XDG_CACHE_HOME when set (test runs: iOS sets HOME itself)
//   Linux: $XDG_DATA_HOME or ~/.local/share, $XDG_CACHE_HOME or ~/.cache
inline std::filesystem::path UserDataBase()
{
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? home : ".";
#ifdef __APPLE__
#if TARGET_OS_IOS
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        return xdg;
#endif
    return base / "Library/Application Support";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"))
        return xdg;
    return base / ".local/share";
#endif
}

inline std::filesystem::path CacheBase()
{
    const char* home = std::getenv("HOME");
    std::filesystem::path base = home ? home : ".";
#ifdef __APPLE__
#if TARGET_OS_IOS
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"))
        return xdg;
#endif
    return base / "Library/Caches";
#else
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"))
        return xdg;
    return base / ".cache";
#endif
}

// This computer's own data and cache folders, portable mode or not
// (install/portable_switch.h copies between them and ReCarbon's folder).
inline std::filesystem::path HomeUserPath()
{
    return UserDataBase() / kUserFolderName;
}

inline std::filesystem::path HomeCachePath()
{
    return CacheBase() / kUserFolderName;
}

// Per-user data: saves, settings, logs and (by default) the installed game.
// In portable mode, the data folder in ReCarbon's own folder.
inline std::filesystem::path GetUserPath()
{
    if (const portable::Layout& layout = portable::Current(); layout.Active())
        return layout.data;
    return HomeUserPath();
}

// Caches that can be deleted any time (compiled shaders, pipelines). In
// portable mode, the cache folder in ReCarbon's own folder.
inline std::filesystem::path GetCachePath()
{
    if (const portable::Layout& layout = portable::Current(); layout.Active())
        return layout.cache;
    return HomeCachePath();
}

inline std::filesystem::path GetSavePath(bool)
{
    return GetUserPath() / "save";
}
