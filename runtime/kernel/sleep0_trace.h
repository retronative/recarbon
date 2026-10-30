// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_SLEEP0_TRACE=1 (a diagnostic, off by default; macOS): how the game's
// Sleep(0) polls (KeDelayExecutionThread with no time, kernel/imports_sync.cpp)
// wait, with RECARBON_SLEEP0_BACKOFF on or off. Per thread: each wait (a run of
// zero delays, each under kernel/sleep0_backoff.h's kGapNs after the last
// returned), its length, whether it ended after a yield or a sleep and that
// sleep's length (what the back-off can add to a handoff), each sleep's overrun,
// the time between waits' ends, and on which core cluster a wait ended and how
// often a sleep moved the thread between clusters. One line per thread per
// kLogEveryNs, from the thread itself. Pure, for tests/sleep0_trace_test.cpp.
#pragma once
#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include "sleep0_backoff.h"

namespace kernel::sleep0trace
{
    constexpr int64_t kLogEveryNs = 10'000'000'000;
    constexpr size_t kMaxCpus = 64;

    enum Cluster : int8_t { kUnknown = -1, kEfficiency = 0, kPerformance = 1 };

    // Which cluster each CPU number (pthread_cpu_number_np) is on, from the
    // device tree's cpu nodes (logical-cpu-id, cluster-type "E"/"P").
    struct CpuLayout
    {
        std::array<int8_t, kMaxCpus> cluster;
        bool verified = false;

        CpuLayout() { cluster.fill(kUnknown); }
        // All unknown unless BuildLayout verified it.
        Cluster Of(int cpu) const { return cpu >= 0 && size_t(cpu) < kMaxCpus ? Cluster(cluster[cpu]) : kUnknown; }
    };

    struct CpuNode { int id; char type; };

    // Verified only if every CPU 0..ncpu-1 appears once as E or P and the
    // counts match hw.perflevel1 (efficiency) and hw.perflevel0 (performance).
    // Otherwise unverified: every CPU reads as unknown.
    inline CpuLayout BuildLayout(const std::vector<CpuNode>& nodes, int ncpu, int efficiency, int performance)
    {
        CpuLayout layout;
        if (ncpu <= 0 || size_t(ncpu) > kMaxCpus || int(nodes.size()) != ncpu)
            return layout;
        int counts[2] = {};
        for (const CpuNode& n : nodes)
        {
            if (n.id < 0 || n.id >= ncpu || layout.cluster[n.id] != kUnknown || (n.type != 'E' && n.type != 'P'))
                return CpuLayout();
            layout.cluster[n.id] = n.type == 'E' ? kEfficiency : kPerformance;
            counts[layout.cluster[n.id]]++;
        }
        if (counts[kEfficiency] != efficiency || counts[kPerformance] != performance)
            return CpuLayout();
        layout.verified = true;
        return layout;
    }

    // Durations in quarter-octave buckets of microseconds (each about 19% wide).
    struct Histogram
    {
        static constexpr int kBuckets = 112;  // up to 2^28 us
        std::array<uint32_t, kBuckets> counts{};
        uint64_t n = 0;
        int64_t maxNs = 0;

        static int BucketOf(int64_t ns)
        {
            const double us = double(ns) / 1000.0;
            if (us < 1.0)
                return 0;
            int b = 0;
            for (double edge = 1.0; b < kBuckets - 1 && us >= edge * 1.189207115; b++)
                edge *= 1.189207115;  // 2^(1/4)
            return b;
        }
        // A bucket's upper edge, in ns.
        static int64_t UpperNs(int b)
        {
            double edge = 1.0;
            for (int i = 0; i <= b; i++)
                edge *= 1.189207115;
            return int64_t(edge * 1000.0);
        }
        void Add(int64_t ns)
        {
            counts[BucketOf(ns)]++;
            n++;
            if (ns > maxNs)
                maxNs = ns;
        }
        // The upper edge of the bucket holding the p-th percentile (0 if empty).
        int64_t PercentileNs(double p) const
        {
            if (n == 0)
                return 0;
            const uint64_t rank = uint64_t(p / 100.0 * double(n - 1));
            uint64_t seen = 0;
            for (int b = 0; b < kBuckets; b++)
            {
                seen += counts[b];
                if (seen > rank)
                    return std::min(UpperNs(b), maxNs);
            }
            return maxNs;
        }
    };

    // One thread's zero delays.
    struct Tracker
    {
        bool inWait = false;
        int64_t waitStart = 0, lastReturn = 0, lastWaitEnd = 0;
        bool lastSlept = false;
        int64_t lastActionNs = 0;
        Cluster lastCluster = kUnknown;

        Histogram waitLen, finalSleep, overrun, betweenEnds;
        uint64_t waits = 0, endedAfterSleep = 0, endedAfterYield = 0, sleeps = 0, yields = 0, sleepMoves = 0;
        uint64_t endedOn[3] = {};  // unknown, efficiency, performance
        int64_t windowStart = 0;

        // A zero delay begins at `now`: a pause over kGapNs since the last one
        // returned ended the wait before it.
        void Call(int64_t now)
        {
            if (windowStart == 0)
                windowStart = now;
            if (inWait && now - lastReturn > sleep0::kGapNs)
                EndWait();
            if (!inWait)
            {
                inWait = true;
                waitStart = now;
            }
        }

        // It yielded or slept from `start` to `end`, on `before` then `after` (clusters).
        // (`askedNs`: the sleep's asked length, for its overrun.)
        void Done(int64_t start, int64_t end, bool slept, Cluster before, Cluster after, int64_t askedNs = sleep0::kSleepNs)
        {
            lastReturn = end;
            lastSlept = slept;
            lastActionNs = end - start;
            lastCluster = after;
            if (slept)
            {
                sleeps++;
                overrun.Add(std::max<int64_t>(0, end - start - askedNs));
                if (before != kUnknown && after != kUnknown && before != after)
                    sleepMoves++;
            }
            else
                yields++;
        }

        void EndWait()
        {
            inWait = false;
            waits++;
            waitLen.Add(lastReturn - waitStart);
            if (lastSlept)
            {
                endedAfterSleep++;
                finalSleep.Add(lastActionNs);
            }
            else
                endedAfterYield++;
            endedOn[lastCluster + 1]++;
            if (lastWaitEnd)
                betweenEnds.Add(lastReturn - lastWaitEnd);
            lastWaitEnd = lastReturn;
        }

        // At a RECARBON_SLEEP0_BACKOFF_AT switch (kernel/sleep0_schedule.h): an
        // open wait ends at its last return, in this window, so a window's
        // line holds one phase only.
        void CloseWait()
        {
            if (inWait)
                EndWait();
        }

        // Every kLogEveryNs: a line for `name`, and the counts start again
        // (a wait still open is counted in the next window).
        bool Due(int64_t now) const { return windowStart != 0 && now - windowStart >= kLogEveryNs; }

        std::string Report(const char* name, int64_t now) const
        {
            auto us = [](int64_t ns) { return double(ns) / 1000.0; };
            const uint64_t ended = endedOn[0] + endedOn[1] + endedOn[2];
            auto share = [&](uint64_t n) { return ended ? 100.0 * double(n) / double(ended) : 0.0; };
            return std::format(
                "[sleep0] {} {:.1f} s: {} waits (us p50 {:.0f} p90 {:.0f} max {:.0f}); ended after a yield {}, after a sleep {} (that sleep us p50 "
                "{:.0f} p90 {:.0f} max {:.0f}); {} yields, {} sleeps (overrun us p50 {:.0f} p90 {:.0f} max {:.0f}, {} moved cluster); ended on "
                "P {:.0f}% E {:.0f}% ? {:.0f}%; between wait ends us p50 {:.0f} p90 {:.0f}",
                name, double(now - windowStart) / 1e9, waits, us(waitLen.PercentileNs(50)), us(waitLen.PercentileNs(90)), us(waitLen.maxNs),
                endedAfterYield, endedAfterSleep, us(finalSleep.PercentileNs(50)), us(finalSleep.PercentileNs(90)), us(finalSleep.maxNs), yields,
                sleeps, us(overrun.PercentileNs(50)), us(overrun.PercentileNs(90)), us(overrun.maxNs), sleepMoves, share(endedOn[kPerformance + 1]),
                share(endedOn[kEfficiency + 1]), share(endedOn[0]), us(betweenEnds.PercentileNs(50)), us(betweenEnds.PercentileNs(90)));
        }

        void StartWindow(int64_t now)
        {
            Tracker next;
            next.inWait = inWait;
            next.waitStart = waitStart;
            next.lastReturn = lastReturn;
            next.lastWaitEnd = lastWaitEnd;
            next.lastSlept = lastSlept;
            next.lastActionNs = lastActionNs;
            next.lastCluster = lastCluster;
            next.windowStart = now;
            *this = next;
        }
    };
}
