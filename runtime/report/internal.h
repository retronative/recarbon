// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Shared by the report module's files. Everything here marked
// async-signal-safe may run in the crash handler: no allocation, no locks,
// no stdio; write(2) to descriptors opened at startup.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace report::detail
{
    // How a toast tells the player to reach Save Bug Report.
#ifdef __APPLE__
    constexpr const char* kSaveBugReportHint = "Settings (F1, Cmd+, or Back + Start) > Advanced > Save Bug Report";
#else
    constexpr const char* kSaveBugReportHint = "Settings (F1, or Back + Start) > Advanced > Save Bug Report";
#endif

    // Appends text and numbers to a fixed buffer, dropping what doesn't fit.
    // Async-signal-safe.
    struct SafeBuf
    {
        char* data;
        size_t capacity;
        size_t size = 0;

        SafeBuf& Str(const char* s);
        SafeBuf& Str(const char* s, size_t n);
        SafeBuf& Char(char c);
        SafeBuf& Dec(int64_t v);
        SafeBuf& Hex(uint64_t v, int digits = 0, bool upper = false);  // no prefix; at least `digits` digits
    };

    // write(2) until done (EINTR, short writes). Async-signal-safe.
    void WriteAll(int fd, const void* data, size_t size);

    // The original stderr and this session's log file (-1: none), for
    // writes that must not wait for the log thread (the crash handler).
    int TerminalFd();
    int LogFileFd();
    // Writes to both. Async-signal-safe.
    void WriteDirect(const char* data, size_t size);
    // The last `maxLines` lines of stderr from the in-memory tail.
    // Async-signal-safe.
    void WriteRecentOutput(int fd, size_t maxLines);

    // This session's log file reached its size cap (RECARBON_LOG_MAX_KB).
    bool LogCapped();

    // The system summary as last logged (for crash reports); "" before.
    // Async-signal-safe.
    const char* LoggedSystemSummary();

    // Host code addresses as names (crash.cpp): on Linux from the
    // executable's .symtab, read by a thread at startup (dladdr sees no
    // symbols of an executable there); else the recompiled functions from
    // the generated table (PPCFuncMappings: guest address -> host function)
    // and the rest through dladdr. Built once at startup (BuildSymbols).
    void BuildSymbols();
    // "0x0000555555a0b3c4  sub_825A5B20+0x64  (ReCarbon+0xa0b3c4)".
    // Async-signal-safe (dladdr aside, which glibc and macOS implement
    // without allocating). A caller's frame holds a return address, which
    // can be past the end of a function that ends in a call that doesn't
    // return (abort): `returnAddress` names the byte before it. `demangle`
    // (not in a signal handler) turns C++ names readable.
    void FormatFrame(SafeBuf& out, const void* pc, bool returnAddress, bool demangle = false);

    // The interrupted code's address from a signal handler's context
    // (nullptr where unknown). Async-signal-safe.
    void* ContextPc(void* context);
    // How many of a backtrace() taken in a signal handler are the handler's
    // own: up to and including the kernel's signal trampoline (1 if it
    // isn't found). Async-signal-safe (dladdr aside, as FormatFrame).
    int SignalFrames(void* const* frames, int count);

    // The calling thread's registered name, or "" (async-signal-safe).
    const char* CurrentThreadName();

    // Deletes all but the newest `keep` files in `dir` whose names start
    // with `prefix` and end with `suffix` (names sort by time).
    void KeepNewest(const std::string& dir, const char* prefix, const char* suffix, size_t keep);
    // "2026-09-28_14-03-22" for now, local time.
    std::string TimeStamp();

    // ReCarbon: HOME and its real path (when it differs), without a trailing
    // "/" (bug_report.cpp): what ScrubHome writes as "~".
    std::vector<std::string> HomeFolders();

    // ReCarbon: where a report goes and how it is written, shared by the bug
    // report and the performance report (bug_report.cpp). The folder:
    // $<envDir> when set, else on iOS the Files app's, else the Desktop when
    // there is one we may write, else the data folder.
    std::filesystem::path ReportFolder(const char* envDir);
    // Where a saved report is, for the player: on iOS the Files app's name
    // for it; elsewhere the path with the home folder as "~".
    std::string DescribeReportPath(const std::filesystem::path& path);
    // Writes `data` to ReportFolder(envDir)/name through a .partial file and
    // a rename. The Desktop refusing it (full, or macOS's privacy prompt said
    // no), it goes to the data folder instead, unless $<envDir> is set.
    // `error` empty: saved at `path`; else why not, `folder` the one tried.
    struct SavedFile
    {
        std::filesystem::path path, folder;
        std::string error;
    };
    SavedFile SaveReportFile(const char* envDir, const char* logTag, const std::string& name, const void* data, size_t size);

    // ReCarbon: this computer, for the performance report (system_info.cpp,
    // the same readings as the system summary's). Empty or < 0: unknown.
    struct HostFacts
    {
        std::string machine, chip, os;
        int perfCores = -1, effCores = -1;
        double memoryGb = -1;
    };
    HostFacts ReadHostFacts();
    // A SetSystemItem value ("GPU"), or "".
    std::string SystemItem(const char* key);
}
