// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// game/wait_span.h: a polling loop's wait from its first poll to its exit (the hitch report's
// "D3D waits", game/gpu_waits.cpp) and RECARBON_HANDOFF_TRACE's 10 s window of them
// (game/handoff_waits.cpp), on a fake clock. Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/wait_span_test.cpp -o build/wait_span_test && build/wait_span_test
// `build/wait_span_test --bench` also times, on the real clock, what a wait costs the game
// thread (not part of ctest: a wall-clock measure, never a check).
#include <game/wait_span.h>

#include <chrono>
#include <cstdio>
#include <cstring>

namespace
{
    int g_failures = 0;

    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            printf("  FAIL: %s\n", what);
            g_failures++;
        }
    }

    using namespace game::waitspan;
    constexpr int64_t us = 1000, ms = 1000 * us, s = 1000 * ms;

    // A wait of `polls` polls from `from`, one every `every`, as a hook sees it: the clock is read
    // only at the first poll; it ends at `end`.
    int64_t Wait(Span& span, int64_t from, int polls, int64_t every, int64_t end)
    {
        for (int i = 0; i < polls; i++)
        {
            if (!span.open)
                span.Begin(from + i * every);
            span.Poll();
        }
        return span.End(end);
    }

    int64_t NowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // What gpu_waits.cpp adds to a GPU wait: a clock read at the first poll, a branch at the later
    // ones, a clock read and an add at the end.
    void Bench()
    {
        constexpr int kWaits = 2'000'000;
        for (int polls : { 1, 10 })
        {
            Span span;
            uint64_t total = 0;
            const auto begin = std::chrono::steady_clock::now();
            for (int w = 0; w < kWaits; w++)
            {
                for (int p = 0; p < polls; p++)
                    if (!span.open)
                        span.Begin(NowNs());
                if (span.open)
                    total += uint64_t(span.End(NowNs()));
            }
            const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count() / kWaits;
            printf("bench: %.1f ns a wait of %d poll%s (measured total %llu ns)\n", ns, polls, polls > 1 ? "s" : "", (unsigned long long)total);
        }
        // An exit with no poll (the loop found the GPU done): one branch.
        {
            Span span;
            uint64_t total = 0;
            const auto begin = std::chrono::steady_clock::now();
            for (int w = 0; w < kWaits * 10; w++)
            {
                if (span.open)
                    total += uint64_t(span.End(NowNs()));
                asm volatile("" ::: "memory");
            }
            const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count() / (kWaits * 10);
            printf("bench: %.2f ns an exit without a wait (%llu)\n", ns, (unsigned long long)total);
        }
    }
}

int main(int argc, char** argv)
{
    printf("---- a span ----\n");
    {
        Span span;
        Check(span.End(5 * us) == -1, "an exit with no poll: no wait (-1)");
        Check(Wait(span, 100 * us, 1, 0, 130 * us) == 30 * us, "one poll: first poll to exit");
        Check(!span.open, "closed at the exit");
        Check(Wait(span, 1 * ms, 50, 2 * us, 1 * ms + 250 * us) == 250 * us, "50 polls: timed from the first, not the last");
        Check(span.polls == 50, "its polls counted");
        Check(span.End(2 * ms) == -1, "a second exit adds nothing");
        Check(Wait(span, 10 * ms, 3, 1 * us, 9 * ms) == 0, "a clock that went back: 0, never negative");
    }
    {
        Span span;
        span.Begin(10 * us);
        span.Poll();
        span.Begin(40 * us);  // later polls don't move the start
        span.Poll();
        Check(span.End(50 * us) == 40 * us && span.polls == 0 + 2, "the start is the first poll's");
        span.Begin(60 * us);
        Check(span.polls == 0, "a new wait counts its own polls");
        span.Poll();
        Check(span.End(61 * us) == 1 * us && span.polls == 1, "a new wait, its own start");
    }

    printf("---- a window ----\n");
    {
        Window w;
        Check(!w.Due(100 * s), "empty: never due");
        w.Add(1 * s, 2 * ms, 10);
        w.Add(2 * s, 4 * ms, 20);
        w.Add(3 * s, 100 * us, 1);
        Check(w.waits == 3 && w.polls == 31 && w.totalNs == 6 * ms + 100 * us, "counts, polls and total");
        Check(w.length.maxNs == 4 * ms, "the longest");
        Check(!w.Due(1 * s + 9999 * ms), "under 10 s after its first wait: not due");
        Check(w.Due(11 * s), "10 s after: due");
        const std::string line = w.Report("WaitPlayed", 11 * s);
        Check(line.starts_with("[handoff] WaitPlayed 10.0 s: 3 waits, 6.1 ms in all (0.1%)"), line.c_str());
        Check(line.find(", 31 polls") != std::string::npos, "the line names the polls");
        Check(line.find("max 4000") != std::string::npos, "the line names the longest, in us");
        w.StartWindow(11 * s);
        Check(w.waits == 0 && w.polls == 0 && w.totalNs == 0 && w.length.n == 0 && w.windowStart == 11 * s, "a new window starts empty, at now");
    }
    {
        Window w;
        for (int i = 0; i < 99; i++)
            w.Add(1 * s + i, 100 * us, 1);
        w.Add(2 * s, 8 * ms, 1);
        const std::string line = w.Report("render", 11 * s);
        const int64_t p50 = w.length.PercentileNs(50), p99 = w.length.PercentileNs(99);
        Check(p50 >= 100 * us && p50 < 120 * us, "p50: the usual wait");
        Check(p99 >= 100 * us && p99 < 120 * us, "p99 of 100: the 99th, not the one long wait");
        Check(line.find("p50 1") != std::string::npos && line.find("p99 1") != std::string::npos, line.c_str());
        w.Add(3 * s, 8 * ms, 1);
        Check(w.length.PercentileNs(99) == 8 * ms, "two long of 101: p99 is long");
        const std::string longer = w.Report("render", 11 * s);
        Check(longer.find("p99 8000") != std::string::npos && longer.find("p50 1") != std::string::npos, longer.c_str());
    }

    if (argc > 1 && strcmp(argv[1], "--bench") == 0)
        Bench();
    printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
