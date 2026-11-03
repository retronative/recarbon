// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// NFS: Carbon's deferred render-callback queue (state at 0x82B8C764) is a
// lock-free single-producer, single-consumer list, as MW's was (SpeedBreaker's
// runtime/game/render_queue.cpp, 0x82909650): sub_82469448 records a
// callback (copies the entry, stores its callback at +0 and size at +4, then
// publishes: the write pointer at +20, then the count at +0 of the state) and
// the main thread plays them back in sub_824694C0 (sub_824E9720), calling
// each entry's callback. It has no barriers: it relies on the entry's stores
// becoming visible before the write pointer and count that publish it. x86
// keeps stores in order; ARM64 doesn't: the main thread called a callback
// read before its entry was written, 0x8209B4AC in .rdata ("indirect call
// ... not a recompiled function", lr 0x82469528; a 16-minute soak, ROADMAP
// M6). Mid-asm hooks (config/recarbon.toml) add a release fence before the
// publish (0x824694B0) and an acquire fence before an entry is read
// (0x82469518). The same publish is inlined five times in the frame render
// that the recording thread runs in deferred mode (sub_824E9A80 ->
// sub_824F0210: sub_824EE110 at 0x824EE334, 0x824EE39C, 0x824EE42C and
// sub_824EF830 at 0x824EF9C4; sub_824EF738 at 0x824EF80C has the same shape
// and no caller found), each fenced the same way. The other writers of the
// state (sub_824693C0, sub_82469400, sub_824E9A08, sub_824E9A80) reset it
// whole; sub_824E9A80 does so on the recording thread before it records.
//
// ReCarbon: the recording handshake. sub_824E9A80 (the recording thread)
// resets the state (+0, +4, +8, +12) and then sets the handshake word
// 0x82C651C4 to 1 (0x824E9B24); sub_824E9720 (the main thread) polls that
// word and then reads +12, +8, +0 and +4 to decide whether to play or to
// clear the handshake. Neither side has a barrier, so on ARM64 the main
// thread can see the handshake with the previous frame's +8 and counts: it
// drops the frame (+8 = 1, count == played), plays from the buffer being
// rewritten (count != played: the push race's crash), or keeps its own
// played count above the new count (both threads then wait on each other).
// RenderQueueWindow is the release before the handshake store and
// RenderQueueHandshake the acquire after the poll that sees it (0x824E9788,
// where both of the poll's exits meet). +16..+24 are stored after the
// handshake: until the first push, count == 0 and sub_824694C0 returns
// before reading them; the push's release orders them before its count, but
// the main thread's +24 load (0x824694F4) has no acquire after its count
// load (0x824E980C). That is harmful only if the new write pointer passes
// the previous frame's end within one out-of-order window: not fenced.
// Likewise the store of 1 to +8 at the window's end (0x824E9B40) has no
// release after the last push: the main thread can clear the handshake with
// the last entries unplayed (RECARBON_RQ_LOG's "not played" shows it).
//
// RECARBON_RQ_LOG=1 counts recording windows, publishes and entries played
// (game/render_queue_log.h). Off, each hook is its fence and one branch.
#include <stdafx.h>
#include <game/render_queue.h>
#include <game/render_queue_log.h>
#include <atomic>
#include <chrono>
#include <cstdlib>

namespace
{
    using namespace game::renderqueue;

    const bool s_log = [] {
        const bool on = LogEnabled(std::getenv("RECARBON_RQ_LOG"));
        if (on)
            fprintf(stderr, "[rq] RECARBON_RQ_LOG: counting render-queue recording windows, publishes and entries played\n");
        return on;
    }();

    std::atomic<uint64_t> s_windows{ 0 }, s_publishes{ 0 }, s_played{ 0 };
    const auto s_launch = std::chrono::steady_clock::now();
    int64_t s_lastLogNs = 0;  // MaybeLog's, on the recording thread only
    Counts s_lastLogged;

    int64_t SinceLaunchNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - s_launch).count();
    }

    Counts Snapshot()
    {
        return { s_windows.load(std::memory_order_relaxed), s_publishes.load(std::memory_order_relaxed),
            s_played.load(std::memory_order_relaxed) };
    }

    // Once a window, on the recording thread: the 10 s line. Lines come
    // while recordings do, each naming its own interval.
    void MaybeLog()
    {
        const int64_t now = SinceLaunchNs();
        if (now - s_lastLogNs < kLogIntervalNs)
            return;
        const Counts total = Snapshot();
        fprintf(stderr, "%s\n", IntervalLine((now - s_lastLogNs) / 1e9, total - s_lastLogged, total).c_str());
        s_lastLogNs = now;
        s_lastLogged = total;
    }
}

void RenderQueuePublish()
{
    std::atomic_thread_fence(std::memory_order_release);
    if (s_log)
        s_publishes.fetch_add(1, std::memory_order_relaxed);
}

// Before each entry's callback is read: one a played entry.
void RenderQueueAcquire()
{
    std::atomic_thread_fence(std::memory_order_acquire);
    if (s_log)
        s_played.fetch_add(1, std::memory_order_relaxed);
}

void RenderQueueWindow()
{
    std::atomic_thread_fence(std::memory_order_release);
    if (s_log)
    {
        s_windows.fetch_add(1, std::memory_order_relaxed);
        MaybeLog();
    }
}

void HandoffRenderDone();  // game/handoff_waits.cpp: the same address ends RECARBON_HANDOFF_TRACE's render wait

void RenderQueueHandshake()
{
    std::atomic_thread_fence(std::memory_order_acquire);
    HandoffRenderDone();  // one mid-asm hook per address (config/recarbon.toml)
}

void game::renderqueue::LogTotals()
{
    if (s_log)
        fprintf(stderr, "%s\n", TotalLine(SinceLaunchNs() / 1e9, Snapshot()).c_str());
}
