// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// A texture load's untile (gpu/renderer.cpp's texture upload: compute passes reading the source in
// guest memory) reads a copy of that source taken into the upload ring when the load is recorded,
// in the same ring allocation as its staging (no submit can fall between them). Guest memory then
// needn't be guarded until the GPU runs the untile. That guard works in host pages (16 KB on
// Apple silicon: four guest pages), and the game streams texture data into the memory beside a
// texture still to be untiled: each such write waited for the GPU (M1 Air, 1080p HDR race,
// 2026-11-12: sub_826D91C0 writing just past a texture's mips, 22-35 ms each). The texels are the
// same: the guard made a CPU write wait until after the untile, so the untile always saw the
// memory as it was when the load was recorded, which is what the copy holds.
// Vulkan-free: tests/texture_snapshot_test.cpp.
#pragma once
#include <cstdint>

namespace gpu::texsnap
{
    // The largest base level or mip range copied (each); larger textures stay guarded. The copy
    // is made once per untile dispatch (a slice, a mip level).
    constexpr uint32_t kMaxBytes = 256 * 1024;

    // A load whose untiles can read copies: not split guest memory (the untile then reads its
    // parts by address) nor a shadow copy (discrete GPUs: guest memory is the GPU's own copy).
    inline bool Eligible(uint32_t size, uint32_t mipSize, bool splitMemory, bool shadowMode, uint32_t limit = kMaxBytes)
    {
        return !splitMemory && !shadowMode && size != 0 && size <= limit && mipSize <= limit;
    }

    // The untile's source address for guest `address` inside the range copied from `rangeBase`
    // to ring byte `ringOffset` (the untile's addresses are its source base plus an offset).
    inline uint32_t SourceBase(uint64_t ringOffset, uint32_t rangeBase, uint32_t address)
    {
        return uint32_t(ringOffset) + (address - rangeBase);
    }

    // The copy's bytes in the allocation (rounded to the ring's 256), and where the staging after it starts.
    inline uint64_t CopyBytes(uint32_t size) { return (uint64_t(size) + 255) & ~uint64_t(255); }
    inline uint64_t StagingAt(uint64_t block, uint32_t size) { return block + CopyBytes(size); }
}
