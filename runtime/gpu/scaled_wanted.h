// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Which scaled resolves keep their host-resolution samples (gpu/renderer.cpp), from SpeedBreaker
// v0.2.0's ScaledWanted (GPL-3.0-or-later). A resolve from a scaled target keeps its samples only
// where a scaled reader asked for that memory in the last kWantFrames frames: a scaled texture's
// check (ScaledTextureValid) or the front buffer. Elsewhere it writes the 1x words alone, as at 1x,
// and those pages then read as 1x, as after any 1x write. Upstream's case: the scene's depth strips
// were stored at host resolution every frame for no reader. A reader that starts asking reads 1x
// for the frame its memory takes to be resolved again.
// ReCarbon: on by default (RECARBON_SCALED_WANTED=0 turns it off: every scaled resolve keeps its
// samples, as before). Verified on a MacBook Air M5 at 3x with SCALED_FUSE (2026-10-27): the same
// picture (pixel-identical in a still scene) for 1.0-1.6 ms less GPU time a frame, 31-63 MB fewer
// resolved a frame. Vulkan-free: tests/scaled_wanted_test.cpp checks it alone.
#pragma once
#include <cstdint>
#include <utility>

namespace gpu::scaledwanted
{
    constexpr uint64_t kWantFrames = 120;

    // On unless RECARBON_SCALED_WANTED (`env`) starts with 0.
    inline bool On(const char* env) { return !env || env[0] != '0'; }

    // A reader's range and when it last asked: frame + 1 (0: never).
    struct Want
    {
        uint32_t base = 0, size = 0;
        uint64_t wanted = 0;
    };

    inline const Want& Entry(const Want& w) { return w; }
    template <typename K, typename V>
    const V& Entry(const std::pair<const K, V>& e) { return e.second; }

    // Did a reader overlapping [base, base + size) ask in the last kWantFrames frames?
    template <typename Readers>
    bool Wanted(const Readers& readers, uint32_t base, uint32_t size, uint64_t frame)
    {
        for (const auto& r : readers)
        {
            const auto& w = Entry(r);
            if (!w.wanted || w.wanted + kWantFrames <= frame + 1)
                continue;
            const uint64_t b = w.base, e = b + w.size;
            if (b < uint64_t(base) + size && uint64_t(base) < e)
                return true;
        }
        return false;
    }

    // Whether a scaled resolve of [base, base + size) keeps its samples.
    template <typename Readers>
    bool Keep(bool on, const Readers& readers, uint32_t base, uint32_t size, uint64_t frame)
    {
        return !on || Wanted(readers, base, size, frame);
    }
}
