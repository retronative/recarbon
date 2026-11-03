// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_GAME_TIME_LOG=<file> (a diagnostic, off by default): how Carbon's
// frame turns its measured time into world steps, measured and never changed.
// The frame hands the measured seconds to a quantiser that rounds them to
// whole 1/3600 s ticks, and the world stepper adds the result to an
// accumulator and runs (int)(acc x 60) steps of 1/60 s, at least 1 and at
// most 8, or none below one step. So a steady 60 fps can still give frames
// with no step and frames with two. Four read-only mid-asm hooks
// (config/recarbon.toml has each site and its evidence) fill one
// game::timelog::Frame per frame (game/game_time_log.h has the columns), on
// the game's main thread. The line goes out when the next frame starts, into a
// buffer that a writer thread writes to the file once a second (the main
// thread makes no system call for it); the last second before an exit may be
// missing. Every 10 s a [gametime] line sums the steps (the writer thread
// prints it). With the log on, the [perf] line's `world:` counts are real too
// (GetFrameTimeStats); off, every hook returns at its first branch and the
// counts stay 0, as in v0.1.3.
#include <stdafx.h>
#include "frame_time.h"
#include "game_time_log.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

namespace
{
    namespace timelog = game::timelog;

    constexpr uint32_t kFrameTicks = 0x82C64F88;    // the frame's 1/3600 s ticks (int)
    constexpr uint32_t kFrameSeconds = 0x82C7ABCC;  // the frame's seconds (float)
    constexpr int kRowsPerHandoff = 60;

    FILE* const s_file = [] {
        const char* path = std::getenv("RECARBON_GAME_TIME_LOG");
        if (!path || !*path)
            return (FILE*)nullptr;
        FILE* f = fopen(path, "w");
        if (f)
        {
            fputs(timelog::kHeader, f);
            fprintf(stderr, "[gametime] RECARBON_GAME_TIME_LOG: a line per game frame to %s, a summary every 10 s\n", path);
        }
        else
            fprintf(stderr, "[gametime] RECARBON_GAME_TIME_LOG=%s: can't write it: off\n", path);
        return f;
    }();
    const bool s_on = s_file != nullptr;

    std::atomic<uint64_t> s_frames{ 0 }, s_noStep{ 0 }, s_multiStep{ 0 };

    // The main thread's (the hooks run on it alone).
    timelog::Frame s_frame;
    bool s_pending = false;
    timelog::Summary s_summary;
    std::string s_rows;
    int s_rowCount = 0;

    // Handed to the writer thread.
    std::mutex s_mutex;
    std::condition_variable s_ready;
    std::string s_toWrite, s_toPrint;

    void Writer()
    {
        std::string rows, lines;
        for (;;)
        {
            {
                std::unique_lock lock(s_mutex);
                s_ready.wait(lock, [] { return !s_toWrite.empty() || !s_toPrint.empty(); });
                rows.swap(s_toWrite);
                lines.swap(s_toPrint);
            }
            if (!rows.empty())
            {
                fwrite(rows.data(), 1, rows.size(), s_file);
                fflush(s_file);
                rows.clear();
            }
            if (!lines.empty())
            {
                fputs(lines.c_str(), stderr);
                lines.clear();
            }
        }
    }

    void HandOff(const std::string& line)
    {
        static std::once_flag started;
        std::call_once(started, [] { std::thread(Writer).detach(); });
        {
            std::lock_guard lock(s_mutex);
            s_toWrite += s_rows;
            s_toPrint += line;
        }
        s_ready.notify_one();
        s_rows.clear();
        s_rowCount = 0;
    }

    // The frame under way is done: its line, its counts.
    void Finish(int64_t now)
    {
        s_frames.fetch_add(1, std::memory_order_relaxed);
        if (s_frame.steps == 0)
            s_noStep.fetch_add(1, std::memory_order_relaxed);
        else if (s_frame.steps >= 2)
            s_multiStep.fetch_add(1, std::memory_order_relaxed);
        char row[128];
        s_rows.append(row, timelog::FormatRow(s_frame, row, sizeof(row)));
        s_rowCount++;
        s_summary.Add(s_frame);
        std::string line;
        if (s_summary.Due(now))
        {
            line = s_summary.Report(now) + "\n";
            s_summary.StartWindow(now);
        }
        if (s_rowCount >= kRowsPerHandoff || !line.empty())
            HandOff(line);
    }

    int64_t NowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    uint32_t GuestWord(uint32_t address)
    {
        return ByteSwap(*static_cast<const uint32_t*>(g_memory.Translate(address)));
    }
}

namespace game
{
    FrameTimeStats GetFrameTimeStats()
    {
        return { s_frames.load(std::memory_order_relaxed), s_noStep.load(std::memory_order_relaxed),
            s_multiStep.load(std::memory_order_relaxed), 0 };
    }
}

// The frame calls the quantiser: a new frame, f1 = its measured seconds.
void GameTimeFrame(PPCRegister& f1)
{
    if (!s_on) [[likely]]
        return;
    const int64_t now = NowNs();
    if (s_pending)
        Finish(now);
    s_frame = timelog::Frame{ .hostNs = now, .measuredMs = f1.f64 * 1000.0 };
    s_pending = true;
}

// The quantiser returned: the frame's ticks and seconds.
void GameTimeQuantised()
{
    if (!s_on) [[likely]]
        return;
    s_frame.ticks = int32_t(GuestWord(kFrameTicks));
    s_frame.frameMs = double(std::bit_cast<float>(GuestWord(kFrameSeconds))) * 1000.0;
}

// The stepper's below-one-step return: f0 = the accumulator.
void GameTimeNoStep(PPCRegister& f0)
{
    if (!s_on) [[likely]]
        return;
    s_frame.steps = s_frame.rawSteps = 0;
    s_frame.accMs = f0.f64 * 1000.0;
}

// The stepper's step count: r11 = k as truncated, f0 = the accumulator.
void GameTimeSteps(PPCRegister& r11, PPCRegister& f0)
{
    if (!s_on) [[likely]]
        return;
    s_frame.rawSteps = r11.s32;
    s_frame.steps = timelog::ClampSteps(r11.s32);
    s_frame.accMs = f0.f64 * 1000.0;
}
