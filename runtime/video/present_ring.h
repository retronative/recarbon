// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// The present times of the last N new game frames, for the performance
// report's frame-time percentiles (report/perf_report.h). One writer (the
// presenter, under its queue lock) does a relaxed store and a release store
// per frame; a reader (the report's thread, once per export) copies it and
// drops what the writer may have overwritten meanwhile. Header only:
// tests/perf_report_test.cpp checks the copy and times Push.
#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace video
{
    template <size_t N>
    class PresentRing
    {
        static_assert(N > 1 && (N & (N - 1)) == 0, "a power of two");

    public:
        // The writer only.
        void Push(int64_t ns)
        {
            const uint64_t i = m_count.load(std::memory_order_relaxed);
            m_ns[i & (N - 1)].store(ns, std::memory_order_relaxed);
            m_count.store(i + 1, std::memory_order_release);
        }

        // Any thread: frames pushed so far.
        uint64_t Count() const { return m_count.load(std::memory_order_acquire); }

        // Any thread: the newest min(Count(), N) times, oldest first, without
        // any the writer overwrote while they were copied.
        std::vector<int64_t> Copy() const
        {
            const uint64_t end = m_count.load(std::memory_order_acquire);
            const uint64_t begin = end > N ? end - N : 0;
            std::vector<int64_t> out;
            out.reserve(size_t(end - begin));
            for (uint64_t i = begin; i < end; i++)
                out.push_back(m_ns[i & (N - 1)].load(std::memory_order_relaxed));
            std::atomic_thread_fence(std::memory_order_acquire);
            // Frames up to `now - N` may be overwritten by now (frame `now`'s
            // store can be under way before the count says so).
            const uint64_t now = m_count.load(std::memory_order_relaxed);
            const uint64_t stale = now + 1 > N ? std::min(now + 1 - N, end) : 0;
            if (stale > begin)
                out.erase(out.begin(), out.begin() + ptrdiff_t(stale - begin));
            return out;
        }

    private:
        std::atomic<int64_t> m_ns[N] = {};
        std::atomic<uint64_t> m_count{ 0 };
    };
}
