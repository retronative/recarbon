// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Carbon's Direct3D waits on the GPU back off instead of spinning: on by
// default on macOS since ReCarbon v0.1.3 (RECARBON_GPU_WAIT_BACKOFF=0 turns
// it off; =1 turns it on elsewhere).
//
// Carbon's D3D waits for the GPU in busy loops: the fence wait
// (sub_826DBD78: the device's fence counter at +10768 against the last issued
// fence at +10780), ring-space and idle waits, ten call sites in all. Each
// loop fills a small record on its stack (the device at +0, the last-seen
// counter at +8, the clock of the last progress at +12) and calls the GPU
// hang check (sub_826E0250) with it once per poll; the check compares the
// counter with the record, refreshes the record when the GPU made progress,
// and returns 1 to keep polling or 0 after the hang handler ran. Nothing in
// the loop or the check sleeps or yields (the check's only call reads the
// current thread from the KPCR), so these polls bypass the Sleep(0) back-off
// and hold a core for as long as the GPU lags. Found by shape in our image,
// as SpeedBreaker's Carbon release describes its own (a different executable;
// the addresses here are ours).
//
// The override asks kernel/gpu_wait_backoff.h what to do before each poll:
// spin for the first 20 us of a wait, then sleep 50 us a poll; yield for a
// second after a sleep that ran 2 ms late. The hang check then runs as before,
// so the hang detection and its timeout are untouched; a poll is at most
// about a sleep later than it would have been. Per-thread state; a summary
// line every 10 s (polls, sleeps and their measured lengths, late sleeps).
//
// RECARBON_GPU_WAIT_BACKOFF_AT=<s>,<s>,... (seconds since launch, ascending)
// switches the back-off off<->on at those times for a same-session A/B, as
// RECARBON_SLEEP0_BACKOFF_AT does for the Sleep(0) back-off (the same
// kernel/sleep0_schedule.h machinery): the initial state is the switch's,
// and each switch is logged once.
#include <stdafx.h>

#include <kernel/function.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#include <kernel/gpu_wait_backoff.h>
#include <kernel/sleep0_schedule.h>
#include <kernel/sleep0_trace.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <sched.h>

extern "C" PPC_FUNC(__imp__sub_826E0250);  // D3D: the GPU hang check, called once per poll of a GPU wait

namespace
{
    namespace schedule = kernel::sleep0schedule;

    // ReCarbon v0.1.3: on by default on macOS; RECARBON_GPU_WAIT_BACKOFF=0 turns it off.
    const bool s_on = [] {
        const char* v = std::getenv("RECARBON_GPU_WAIT_BACKOFF");
#if defined(__APPLE__) && !TARGET_OS_IOS
        const bool on = !v || std::strcmp(v, "0") != 0;
#else
        const bool on = v && std::strcmp(v, "1") == 0;
#endif
        if (on)
            fprintf(stderr, "[gpuwait] D3D's GPU waits back off%s: spin 20 us, then sleep 50 us a poll\n", v ? "" : " (the default; RECARBON_GPU_WAIT_BACKOFF=0 turns it off)");
        return on;
    }();

    const std::vector<double> s_at = [] {
        const char* v = std::getenv("RECARBON_GPU_WAIT_BACKOFF_AT");
        std::vector<double> times = schedule::ParseTimes(v);
        if (v && times.empty())
            fprintf(stderr, "[gpuwait] RECARBON_GPU_WAIT_BACKOFF_AT=%s ignored: ascending seconds since launch, such as 240,270.5\n", v);
        else if (v)
            fprintf(stderr, "[gpuwait] RECARBON_GPU_WAIT_BACKOFF_AT: %zu switches of the GPU-wait back-off, the first at %.1f s (%s until then)\n",
                times.size(), times[0], s_on ? "on" : "off");
        return times;
    }();

    const auto s_launch = std::chrono::steady_clock::now();
    std::atomic<int> s_switchLogged{ 0 };

    int64_t NowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::atomic<uint64_t> s_polls{ 0 }, s_sleeps{ 0 }, s_late{ 0 }, s_yields{ 0 };
    std::atomic<int64_t> s_nextLog{ 0 };
    std::mutex s_histogramMutex;
    kernel::sleep0trace::Histogram s_sleepLength;  // the measured length of each sleep, this window

    void BackOff(kernel::gpuwait::State& state, int phase)
    {
        const int64_t now = NowNs();
        s_polls.fetch_add(1, std::memory_order_relaxed);
        switch (kernel::gpuwait::Poll(state, now))
        {
        case kernel::gpuwait::Action::Spin:
            break;
        case kernel::gpuwait::Action::Yield:
            s_yields.fetch_add(1, std::memory_order_relaxed);
            sched_yield();
            break;
        case kernel::gpuwait::Action::Sleep:
        {
            const timespec request = { 0, long(kernel::gpuwait::kSleepNs) };
            nanosleep(&request, nullptr);
            const int64_t end = NowNs();
            s_sleeps.fetch_add(1, std::memory_order_relaxed);
            if (kernel::gpuwait::Slept(state, now, end))
                s_late.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(s_histogramMutex);
                s_sleepLength.Add(end - now);
            }
            break;
        }
        }
        int64_t next = s_nextLog.load(std::memory_order_relaxed);
        if (now >= next && s_nextLog.compare_exchange_strong(next, now + 10'000'000'000, std::memory_order_relaxed))
        {
            if (next != 0)  // (the first window only arms the clock)
            {
                kernel::sleep0trace::Histogram h;
                {
                    std::lock_guard<std::mutex> lock(s_histogramMutex);
                    h = s_sleepLength;
                    s_sleepLength = {};
                }
                fprintf(stderr, "[gpuwait] 10 s%s: %llu polls, %llu sleeps (%llu late; length p50 %lld us, p90 %lld us, max %lld us), %llu yields\n",
                    phase >= 0 ? (phase % 2 == (s_on ? 0 : 1) ? " (on)" : " (off)") : "", (unsigned long long)s_polls.exchange(0),
                    (unsigned long long)s_sleeps.exchange(0), (unsigned long long)s_late.exchange(0), (long long)(h.PercentileNs(50) / 1000),
                    (long long)(h.PercentileNs(90) / 1000), (long long)(h.maxNs / 1000), (unsigned long long)s_yields.exchange(0));
            }
        }
    }
}

PPC_FUNC(sub_826E0250)
{
    thread_local kernel::gpuwait::State state;
    bool on = s_on;
    int phase = -1;
    if (!s_at.empty())
    {
        thread_local schedule::ThreadPhase seen;
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - s_launch).count();
        const schedule::Step step = schedule::StepAt(s_at, s_on, seconds, seen, s_switchLogged);
        on = step.backOff;
        phase = step.phase;
        if (step.newPhase)
            state = {};  // a new phase's polls start their own runs
        if (step.logSwitch)
            fprintf(stderr, "[gpuwait] switch %d at %.1f s: the GPU-wait back-off %s\n", step.phase, seconds, on ? "on" : "off");
    }
    if (on)
        BackOff(state, phase);
    else
        s_polls.fetch_add(1, std::memory_order_relaxed);
    __imp__sub_826E0250(ctx, base);
}
