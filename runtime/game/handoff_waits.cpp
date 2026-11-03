// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_HANDOFF_TRACE=1 (a diagnostic, off by default): how long the game's
// threads wait on the frame handshake word (0x82C651C4) that hands a frame from
// the main thread to the render thread and back. The render thread's loop
// (sub_824E9720) polls it with Sleep(0) while it is 0; WaitPlayed
// (sub_824E9948) polls it while it is 1. Read-only mid-asm hooks at each
// loop's poll and exit (config/recarbon.toml) time each wait from its first
// poll to the exit (game/wait_span.h: two clock reads a wait, none for the
// later polls) and, every 10 s, the waiting thread prints a [handoff] line:
// its waits, their total, p50/p99/max and polls. The Sleep(0) polls
// themselves are untouched (kernel/imports_sync.cpp, RECARBON_SLEEP0_TRACE has
// their per-thread view across every polling loop). Off, each hook returns at
// its first branch.
#include <stdafx.h>
#include "wait_span.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>

namespace
{
    namespace waitspan = game::waitspan;

    const bool s_on = [] {
        const char* v = std::getenv("RECARBON_HANDOFF_TRACE");
        const bool on = v && v[0] == '1';
        if (on)
            fprintf(stderr, "[handoff] RECARBON_HANDOFF_TRACE=1: the frame handshake's waits, a line per waiting thread every 10 s\n");
        return on;
    }();

    int64_t NowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    struct Site
    {
        waitspan::Span span;
        waitspan::Window window;
    };
    thread_local Site t_render, t_played;

    void Poll(Site& site)
    {
        if (!site.span.open)
            site.span.Begin(NowNs());
        site.span.Poll();
    }

    void Done(Site& site, const char* name)
    {
        if (!site.span.open)
            return;
        const int64_t now = NowNs();
        const uint64_t polls = site.span.polls;
        site.window.Add(now, site.span.End(now), polls);
        if (!site.window.Due(now))
            return;
        char thread[64] = {};
        pthread_getname_np(pthread_self(), thread, sizeof(thread));
        fprintf(stderr, "%s (%s)\n", site.window.Report(name, now).c_str(), thread[0] ? thread : "?");
        site.window.StartWindow(now);
    }
}

void HandoffRenderPoll()
{
    if (!s_on) [[likely]]
        return;
    Poll(t_render);
}

void HandoffRenderDone()
{
    if (!s_on) [[likely]]
        return;
    Done(t_render, "render loop's wait for a frame");
}

void HandoffPlayedPoll()
{
    if (!s_on) [[likely]]
        return;
    Poll(t_played);
}

void HandoffPlayedDone()
{
    if (!s_on) [[likely]]
        return;
    Done(t_played, "WaitPlayed");
}
