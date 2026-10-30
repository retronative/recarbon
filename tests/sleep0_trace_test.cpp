// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// kernel/sleep0_trace.h: RECARBON_SLEEP0_TRACE=1's per-thread account of the game's Sleep(0)
// waits (off by default), on a fake clock: the core layout check, the duration histogram, and
// how zero delays become waits. Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/sleep0_trace_test.cpp -o build/sleep0_trace_test && build/sleep0_trace_test
#include <kernel/sleep0_trace.h>

#include <cstdio>

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

    using namespace kernel::sleep0trace;
    using kernel::sleep0::kGapNs;
    using kernel::sleep0::kSleepNs;
    constexpr int64_t us = 1000, ms = 1000 * us;

    // This M5's device tree: CPUs 0-5 E, 6-9 P.
    std::vector<CpuNode> M5()
    {
        std::vector<CpuNode> nodes;
        for (int i = 0; i < 10; i++)
            nodes.push_back({ i, i < 6 ? 'E' : 'P' });
        return nodes;
    }

    // A wait of yields every 2 us from `from` for `len`, ending at from + len.
    void YieldWait(Tracker& t, int64_t from, int64_t len, Cluster c = kPerformance)
    {
        for (int64_t at = from; at < from + len; at += 2 * us)
        {
            t.Call(at);
            t.Done(at, std::min(at + 2 * us, from + len), false, c, c);
        }
    }
}

int main()
{
    printf("---- the core layout ----\n");
    {
        CpuLayout l = BuildLayout(M5(), 10, 6, 4);
        Check(l.verified, "the M5's 6 E + 4 P: verified");
        Check(l.Of(0) == kEfficiency && l.Of(5) == kEfficiency && l.Of(6) == kPerformance && l.Of(9) == kPerformance, "CPUs 0-5 E, 6-9 P");
        Check(l.Of(10) == kUnknown && l.Of(-1) == kUnknown, "outside 0..ncpu-1: unknown");
    }
    {
        Check(!BuildLayout(M5(), 10, 4, 6).verified, "counts that don't match hw.perflevel: unverified");
        auto dup = M5();
        dup[3].id = 4;
        Check(!BuildLayout(dup, 10, 6, 4).verified, "a CPU listed twice: unverified");
        auto missing = M5();
        missing.pop_back();
        Check(!BuildLayout(missing, 10, 6, 4).verified, "a CPU missing: unverified");
        auto odd = M5();
        odd[7].type = 'S';
        Check(!BuildLayout(odd, 10, 6, 4).verified, "a type other than E or P: unverified");
        auto high = M5();
        high[9].id = 10;
        Check(!BuildLayout(high, 10, 6, 4).verified, "an id past ncpu: unverified");
        Check(BuildLayout(dup, 10, 6, 4).Of(0) == kUnknown, "unverified: every CPU reads unknown");
        Check(!BuildLayout({}, 0, 0, 0).verified, "no CPUs: unverified");
        auto nine = M5();
        nine.pop_back();
        Check(!BuildLayout(nine, 10, 6, 3).verified, "9 CPUs listed of hw.ncpu 10, even with counts to match them: unverified");
    }

    printf("---- the histogram ----\n");
    {
        Histogram h;
        Check(h.PercentileNs(50) == 0, "empty: 0");
        for (int i = 0; i < 90; i++)
            h.Add(100 * us);
        for (int i = 0; i < 10; i++)
            h.Add(10 * ms);
        const int64_t p50 = h.PercentileNs(50), p90 = h.PercentileNs(90), p95 = h.PercentileNs(95);
        Check(p50 >= 100 * us && p50 < 120 * us, "p50 at 100 us, within a bucket (19%)");
        Check(p90 >= 100 * us && p90 < 120 * us, "p90 still the 100 us bucket (the 90th of 100)");
        Check(p95 == 10 * ms, "p95 in the 10 ms bucket, capped at the max");
        Check(h.maxNs == 10 * ms && h.n == 100, "max and count");
        Histogram tiny;
        tiny.Add(0);
        tiny.Add(500);
        Check(tiny.counts[0] == 2, "under 1 us: the first bucket");
        Histogram two;
        two.Add(100 * us);
        two.Add(10 * ms);
        Check(two.PercentileNs(0) < 120 * us && two.PercentileNs(100) == 10 * ms, "p0 the smaller, p100 the larger");
    }

    printf("---- waits ----\n");
    {
        Tracker t;
        YieldWait(t, 1 * ms, 300 * us);
        YieldWait(t, 1 * ms + 300 * us + kGapNs, 200 * us);  // exactly kGapNs later: the same wait
        Check(t.waits == 0 && t.inWait, "a pause of exactly kGapNs keeps the wait open");
        YieldWait(t, 20 * ms, 500 * us);  // much later: the first wait ended
        Check(t.waits == 1, "a pause over kGapNs ends the wait");
        Check(t.waitLen.maxNs == 300 * us + kGapNs + 200 * us, "its length: first call to last return");
        Check(t.endedAfterYield == 1 && t.endedAfterSleep == 0, "it ended after a yield");
        Check(t.endedOn[kPerformance + 1] == 1, "on a performance core");
    }
    {
        Tracker t;
        int64_t at = 0;
        YieldWait(t, at, 1 * ms);
        at = 1 * ms;
        t.Call(at);
        t.Done(at, at + 140 * us, true, kPerformance, kEfficiency);  // a back-off sleep, 40 us over, onto an E core
        t.Call(at + 150 * us);
        t.Done(at + 150 * us, at + 400 * us, true, kEfficiency, kEfficiency);  // 150 us over
        t.Call(5 * ms);  // the wait ended at the last sleep's end
        Check(t.waits == 1 && t.endedAfterSleep == 1, "a wait that ended after a sleep");
        Check(t.finalSleep.maxNs == 250 * us, "the final sleep's length (the most it added to the handoff)");
        Check(t.sleeps == 2 && t.overrun.maxNs == 250 * us - kSleepNs, "two sleeps; overrun past kSleepNs");
        Check(t.sleepMoves == 1, "one sleep moved the thread from P to E");
        Check(t.endedOn[kEfficiency + 1] == 1, "the wait ended on an efficiency core");
        Check(t.waitLen.maxNs == 1 * ms + 400 * us, "the wait's length includes its sleeps");
    }
    {
        Tracker t;
        t.Call(1 * ms);
        t.Done(1 * ms, 1 * ms + 120 * us, true, kPerformance, kEfficiency);
        t.Call(9 * ms);
        Check(t.endedOn[kEfficiency + 1] == 1 && t.sleepMoves == 1, "a wait whose last sleep moved it P -> E ended on E");
    }
    {
        Tracker t;
        for (int frame = 0; frame < 5; frame++)
            YieldWait(t, frame * 16667 * us, 3 * ms);
        t.Call(100 * ms);
        Check(t.waits == 5 && t.betweenEnds.n == 4, "five waits: four gaps between their ends");
        const int64_t p50 = t.betweenEnds.PercentileNs(50);
        Check(p50 >= 16667 * us && p50 < 20 * ms, "a frame apart (16.7 ms, within a bucket)");
    }
    {
        Tracker t;
        t.Call(5 * ms);
        t.Done(5 * ms, 5 * ms + kSleepNs - 10 * us, true, kUnknown, kPerformance);
        Check(t.overrun.maxNs == 0, "a sleep shorter than asked: no overrun");
        Check(t.sleepMoves == 0, "an unknown cluster isn't a move");
    }

    {
        Tracker t;
        t.Call(5 * ms);
        t.Done(5 * ms, 5 * ms + 1050 * us, true, kPerformance, kPerformance, 1 * ms);  // a 1 ms chunk, 50 us over
        Check(t.overrun.maxNs == 50 * us, "a long chunk's overrun counts against its asked length");
    }

    printf("---- the log window ----\n");
    {
        Tracker t;
        YieldWait(t, 1 * ms, 100 * us);
        Check(!t.Due(1 * ms + kLogEveryNs - 1), "not before kLogEveryNs");
        Check(t.Due(1 * ms + kLogEveryNs), "at kLogEveryNs");
        YieldWait(t, 50 * ms, 100 * us);
        const std::string line = t.Report("g2", 1 * ms + kLogEveryNs);
        Check(line.find("[sleep0] g2 10.0 s: 1 waits") == 0, "the line: thread, window, waits");
        Check(line.find("ended on P 100% E 0% ? 0%") != std::string::npos, "the cluster shares");
        t.StartWindow(1 * ms + kLogEveryNs);
        Check(t.waits == 0 && t.yields == 0 && t.waitLen.n == 0, "a new window starts from zero");
        Check(t.inWait && t.waitStart == 50 * ms, "an open wait carries over");
        t.Call(1 * ms + kLogEveryNs + 1 * ms);
        Check(t.waits == 1 && t.betweenEnds.n == 1 && t.betweenEnds.maxNs == 49 * ms,
            "and ends in the new window, timed from the previous window's last end");
    }

    printf("---- a phase switch ----\n");
    {
        Tracker t;
        YieldWait(t, 1 * ms, 100 * us);
        YieldWait(t, 5 * ms, 300 * us);  // open when the phase switches
        t.CloseWait();
        Check(t.waits == 2 && !t.inWait, "the open wait ends in the old window");
        Check(t.waitLen.maxNs == 300 * us, "at its last return, not at the switch");
        t.CloseWait();
        Check(t.waits == 2, "closing again changes nothing");
        t.StartWindow(6 * ms);
        Check(!t.inWait && t.waits == 0, "the new window carries no wait over");
        YieldWait(t, 7 * ms, 100 * us);
        t.Call(9 * ms);
        Check(t.waits == 1 && t.betweenEnds.maxNs == 7 * ms + 100 * us - (5 * ms + 300 * us),
            "its first wait is timed from the old window's last end");
    }

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
