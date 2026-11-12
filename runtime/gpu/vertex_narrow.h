// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Large dynamic vertex ranges as each draw's vertices only, from SpeedBreaker v0.2.0's
// NFSMW_NARROW_VERTICES (GPL-3.0-or-later; renderer.cpp's draw guard loop, there on with Smooth
// Motion). A draw's vertex fetch constant spans a whole dynamic vertex buffer (Carbon: 323 KB at
// 0FDF6C00 in a race), and the write watch guards all of it, a page at a time: the game writing
// the next draws' vertices beside it then waits for the GPU (16 KB host pages hold four guest
// pages). Guarded or copied as the draw's own records, the buffer's other slots stay writable.
// M1 Air, 1080p HDR race (2026-11-12, RECARBON_LOG_GUARD): every stall was a write outside the
// guarded range, most by sub_825C3980 into that buffer, ~17-20 ms each.
// Vulkan-free: tests/vertex_narrow_test.cpp checks it with the translator's per-constant fields.
#pragma once
#include <algorithm>
#include <cstdint>

namespace gpu::vertexnarrow
{
    // The records of vertices [vertexMin, vertexMax] (`stride` bytes apart, each read up to
    // `extent` bytes from its start) inside [base, base + size). True when that is smaller than
    // the range, which then becomes it; false (the range unchanged) otherwise.
    inline bool Narrow(uint32_t& base, uint32_t& size, uint64_t vertexMin, uint64_t vertexMax, uint32_t stride, uint32_t extent)
    {
        if (stride == 0 || extent == 0 || vertexMax < vertexMin)
            return false;
        const uint64_t lo = vertexMin * stride, hi = vertexMax * stride + std::max(stride, extent);
        if (lo >= size)
            return false;
        const uint64_t end = std::min<uint64_t>(hi, size);
        if (end <= lo || end - lo >= size)
            return false;
        base += uint32_t(lo);
        size = uint32_t(end - lo);
        return true;
    }

    // Large vertex ranges the CPU has written since a draw last read them (dynamic buffers;
    // once seen, kept): a small direct-mapped cache by base address. `writtenSince`: the
    // write watch's answer for the range since the entry's last look; `now` its sequence.
    class DynamicRanges
    {
    public:
        bool Dynamic(uint32_t base, uint32_t size, uint64_t now, bool writtenSince)
        {
            Entry& e = m_cache[(base >> 12) & 255];
            if (e.base != base || e.size != size)
            {
                e = { base, size, now, false };
                return false;
            }
            if (!e.dynamic && writtenSince)
                e.dynamic = true;
            e.seen = now;
            return e.dynamic;
        }
        // The sequence the cache last looked at `base` (0: never), for the write watch's question.
        uint64_t Seen(uint32_t base, uint32_t size) const
        {
            const Entry& e = m_cache[(base >> 12) & 255];
            return e.base == base && e.size == size ? e.seen : 0;
        }

    private:
        struct Entry
        {
            uint32_t base = 0, size = 0;
            uint64_t seen = 0;
            bool dynamic = false;
        };
        Entry m_cache[256];
    };
}
