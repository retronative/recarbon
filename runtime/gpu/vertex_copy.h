// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Vertex data the guest may reuse before the GPU runs a draw (eager writes, gpu/renderer.cpp):
// a range up to the threshold is copied into the upload ring with the draw and the shader reads
// the copy; a larger one is guarded by the write watch instead (a CPU write to its pages waits
// for the GPU). ReCarbon: the threshold is 128 KB (SpeedBreaker's is 64 KB). In NFS: Carbon,
// D3D's set-stream-source writes the previous vertex buffer's header fence a few hundred bytes in
// front of its data, in the same page; for the 66-75 KB buffers a guard made that write wait for
// the GPU about once a frame (2.7 ms at 2x, 17 ms at 3x on a MacBook Air M5). Copying them costs
// about 0.7 MB a frame more (measured on a MacBook Air M5, October 2026). RECARBON_VERTEX_COPY_128=0: 64 KB, as
// before. Vulkan-free: tests/vertex_copy_test.cpp checks it alone.
#pragma once
#include <cstdint>

namespace gpu::vertexcopy
{
    constexpr uint32_t kThreshold = 128 * 1024, kLegacyThreshold = 64 * 1024;
    constexpr uint32_t kCopiesPerDraw = 4;  // copies a draw's ring allocation holds

    inline uint32_t Threshold(const char* env)
    {
        return env && env[0] == '0' ? kLegacyThreshold : kThreshold;
    }

    // Whether a vertex range is copied with its draw (false: guarded): it fits the threshold, the
    // draw has a copy left, and the GPU neither writes it (a resolve into vertex memory: the CPU's
    // bytes are stale) nor owns it.
    inline bool Copy(uint32_t size, uint32_t threshold, uint32_t copiesUsed, bool gpuWritePending, bool gpuOwned)
    {
        return size != 0 && size <= threshold && copiesUsed < kCopiesPerDraw && !gpuWritePending && !gpuOwned;
    }

    // Ring bytes a copy takes (256-byte aligned, as the draw's other data).
    inline uint32_t RingBytes(uint32_t size)
    {
        return (size + 255) & ~255u;
    }
}
