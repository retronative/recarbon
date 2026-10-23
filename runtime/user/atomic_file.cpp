// ReCarbon runtime (based on SpeedBreaker's settings.cpp). GPL-3.0-or-later (see COPYING).
// See atomic_file.h. ReCarbon change: moved unchanged out of user/settings.cpp
// for the input profiles (user/profiles.cpp); the log tag is a parameter.
#include "atomic_file.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <format>

#include <fcntl.h>
#include <unistd.h>

namespace atomicfile
{
    namespace
    {
        // Where a save of `path` goes: through a symlinked settings.toml
        // (dotfiles kept elsewhere) to the file it points at, since a rename
        // over the link would replace it with a plain file. A relative link
        // is relative to its own directory, as the kernel reads it; a
        // dangling one still names where the file belongs.
        std::filesystem::path SaveTarget(const std::filesystem::path& path)
        {
            std::filesystem::path p = path;
            for (int hops = 0; hops < 8; hops++)  // a loop of links: the rename replaces the last one
            {
                std::error_code ec;
                if (!std::filesystem::is_symlink(std::filesystem::symlink_status(p, ec)))
                    break;
                std::filesystem::path link = std::filesystem::read_symlink(p, ec);
                if (ec || link.empty())
                    break;
                p = link.is_absolute() ? link : p.parent_path() / link;
            }
            return p;
        }

        // Removes what crashed saves left next to `target`: settings.toml.tmp
        // (the name before per-process ones) and any settings.toml.<pid>.tmp
        // a minute old. A save takes milliseconds, so a younger one may be
        // another instance's save in progress.
        void RemoveStaleTemps(const std::filesystem::path& target, const char* tag)
        {
            std::string prefix = target.filename().string() + ".";
            std::filesystem::path dir = target.parent_path().empty() ? std::filesystem::path(".") : target.parent_path();
            auto now = std::filesystem::file_time_type::clock::now();
            std::error_code ec;
            for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            {
                std::string file = it->path().filename().string();
                if (file != prefix + "tmp")
                {
                    if (file.size() <= prefix.size() + 4 || !file.starts_with(prefix) || !file.ends_with(".tmp"))
                        continue;
                    std::string_view pid = std::string_view(file).substr(prefix.size(), file.size() - prefix.size() - 4);
                    if (!std::all_of(pid.begin(), pid.end(), [](char c) { return c >= '0' && c <= '9'; }))
                        continue;
                    std::error_code timeError;
                    auto written = std::filesystem::last_write_time(it->path(), timeError);
                    if (timeError || now - written < std::chrono::minutes(1))
                        continue;
                }
                std::error_code removeError;
                if (std::filesystem::remove(it->path(), removeError))
                    fprintf(stderr, "[%s] removed %s, left by a save that didn't finish\n", tag, file.c_str());
            }
        }
    }

    // Writes `path` so that it's always either the old file or the whole
    // new one: a temporary file in the same directory (rename is atomic
    // only within one file system), flushed to disk before the rename
    // (else a crash can leave the renamed file empty), then the
    // directory, so the rename itself survives a power cut. The
    // temporary file is this process's own (settings.toml.<pid>.tmp,
    // created exclusively): with one shared name, two instances of the
    // game saving at once write into the same file, and one renames the
    // other's half-written text into place. Returns "" or why it failed.
    std::string Write(const std::filesystem::path& path, std::string_view text, const char* tag)
    {
        std::filesystem::path target = SaveTarget(path);
        std::error_code ec;
        std::filesystem::path dir = target.parent_path();
        if (!dir.empty())
        {
            std::filesystem::create_directories(dir, ec);
            if (ec)
                return std::format("can't create {}: {}", dir.string(), ec.message());
        }
        RemoveStaleTemps(target, tag);
        std::filesystem::path temp = target;
        temp += std::format(".{}.tmp", long(getpid()));
        int fd = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
        if (fd < 0 && errno == EEXIST)
        {
            // Saves within a process are serialised, so this was left by
            // a crashed process that had the same pid.
            unlink(temp.c_str());
            fd = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
        }
        if (fd < 0)
            return std::format("can't write {}: {}", target.string(), strerror(errno));
        int error = 0;
        for (size_t done = 0; !error && done < text.size();)
        {
            ssize_t n = write(fd, text.data() + done, text.size() - done);
            if (n > 0)
                done += size_t(n);
            else if (n < 0 && errno == EINTR)
                continue;
            else
                error = n == 0 ? EIO : errno;
        }
#ifdef __APPLE__
        // fsync on macOS doesn't flush the drive's cache; F_FULLFSYNC does
        // (not every file system supports it).
        if (!error && fcntl(fd, F_FULLFSYNC) != 0 && fsync(fd) != 0)
            error = errno;
#else
        if (!error && fsync(fd) != 0)
            error = errno;
#endif
        if (close(fd) != 0 && !error)
            error = errno;
        if (error)
        {
            unlink(temp.c_str());
            return std::format("can't write {}: {}", target.string(), strerror(error));
        }
        if (rename(temp.c_str(), target.c_str()) != 0)
        {
            error = errno;
            unlink(temp.c_str());
            return std::format("can't replace {}: {}", target.string(), strerror(error));
        }
        if (int dirFd = open(dir.empty() ? "." : dir.c_str(), O_RDONLY | O_CLOEXEC); dirFd >= 0)
        {
            fsync(dirFd);  // best effort: some file systems refuse it on directories
            close(dirFd);
        }
        return "";
    }
}
