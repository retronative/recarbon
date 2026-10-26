// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Movies on whole pairs of vblanks (game/movie_hold.h has the rule and the evidence). NFS:
// Carbon's XMV player renders a movie frame in sub_827CA028 (the player's vtable 0x82010C18, slot
// 19: 2826 calls over 94 s of movie, none outside), and D3D's swap callback, sub_826E3378, chooses
// the vblank that releases each swap: from the device (*[0x82000988]) it reads the vblank count
// (+16172) and the last release (+16180), stores the target at +16180 and pushes it into a ring
// of 16 {front buffer +16196, target +16200} entries at its tail (+16328), which the vblank
// interrupt (sub_826E3278) releases when the count reaches the target. A held frame's ring entry
// and the last release move one vblank on, before that vblank comes (the count is still one short
// of the target): the vblank interrupt then lets it go one vblank later, and the next swap's
// callback starts from that release. The addresses were read from Carbon's own code and checked by
// a probe of every callback (2026-10-26); SpeedBreaker's Most Wanted hook holds a countdown
// instead, which Carbon's D3D doesn't have.
// RECARBON_MOVIE_CADENCE=0 turns it off, RECARBON_MOVIE_CADENCE=<ms> sets the guard (default 10,
// at most 12). Each movie leaves a [movie] line: its frames by vblanks shown, and those held.
#include <stdafx.h>
#include "movie_hold.h"

#include <cpu/guest_time.h>
#include <kernel/function.h>
#include <video/presenter.h>
#include <video/vblank_lock.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>

extern "C" PPC_FUNC(__imp__sub_827CA028);  // XMV player: render the due movie frame
extern "C" PPC_FUNC(__imp__sub_826E3378);  // D3D: the swap's interrupt callback

namespace
{
    constexpr uint32_t kDevicePointer = 0x82000988;
    constexpr uint32_t kVblanks = 16172, kReleased = 16180, kRingTarget = 16200, kRingTail = 16328;

    const double s_guardMs = [] {
        const char* v = std::getenv("RECARBON_MOVIE_CADENCE");
        double ms = v ? std::atof(v) : 10.0;
        if (v && ms <= 0)
            fprintf(stderr, "[movie] cadence off (RECARBON_MOVIE_CADENCE=0): movie frames go out at the first vblank after their swap\n");
        return std::clamp(ms, 0.0, 12.0);
    }();

    std::atomic<uint64_t> s_movieRenders{ 0 };

    // The swap callback's thread only (the command processor's interrupts, one at a time).
    game::movie::Tracker s_tracker;
    uint64_t s_lastRenders = 0;

    int64_t NowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void LogMovie(const game::movie::Tracker& t)
    {
        if (t.frames)
            fprintf(stderr, "[movie] %llu frames: %llu on 1 vblank, %llu on 2, %llu on 3, %llu on 4+; %llu held a vblank (%.1f ms guard%s)\n",
                (unsigned long long)t.frames, (unsigned long long)t.spans[1], (unsigned long long)t.spans[2],
                (unsigned long long)t.spans[3], (unsigned long long)t.spans[4], (unsigned long long)t.held, s_guardMs,
                s_guardMs > 0 ? "" : ": off");
    }

    std::atomic_ref<uint32_t> GuestWord(uint8_t* base, uint32_t address)
    {
        return std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(base + address));
    }
}

PPC_FUNC(sub_827CA028)
{
    __imp__sub_827CA028(ctx, base);
    s_movieRenders.fetch_add(1, std::memory_order_release);
}

PPC_FUNC(sub_826E3378)
{
    const uint32_t arg = ctx.r3.u32;
    const uint32_t device = PPC_LOAD_U32(PPC_LOAD_U32(kDevicePointer));
    const uint32_t released = PPC_LOAD_U32(device + kReleased), count = PPC_LOAD_U32(device + kVblanks);
    const uint32_t tail = PPC_LOAD_U32(device + kRingTail);
    const int64_t now = NowNs();
    __imp__sub_826E3378(ctx, base);

    // A movie frame: the XMV player rendered since the last swap (the game waits for that swap
    // before it renders the next frame).
    const uint64_t renders = s_movieRenders.load(std::memory_order_acquire);
    const bool movie = renders != s_lastRenders;
    s_lastRenders = renders;
    game::movie::Tracker before = s_tracker;
    if (game::movie::Record(s_tracker, movie, released))
        LogMovie(before);

    // Only a swap the callback pushed into the ring (a new target).
    const uint32_t target = PPC_LOAD_U32(device + kReleased);
    if (!movie || PPC_LOAD_U32(device + kRingTail) != tail + 1)
        return;
    const video::VblankLock& vblank = video::GuestVblank();
    const int64_t lastVblank = video::LastGuestVblank();
    game::movie::Swap swap{ .interval = (arg >> 8) & 15, .count = count, .released = released, .target = target,
        .sinceVblankNs = double(now - lastVblank), .periodNs = vblank.PeriodNs(), .guardNs = s_guardMs * 1e6,
        .suspended = guesttime::Suspended() || lastVblank < guesttime::LastResumeNs() };
    if (!game::movie::ShouldHold(s_tracker, movie, swap))
        return;
    // Not if the vblank came meanwhile (the interrupt may have let it go): the count must still
    // be one short of the target, and the entry still hold it.
    if (PPC_LOAD_U32(device + kVblanks) != target - 1)
        return;
    uint32_t expected = ByteSwap(target);
    if (!GuestWord(base, device + kRingTarget + (tail & 15) * 8).compare_exchange_strong(expected, ByteSwap(target + 1)))
        return;
    PPC_STORE_U32(device + kReleased, target + 1);
    s_tracker.held++;
}
