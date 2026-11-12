// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// HDR Output's float copy of a resolve (gpu/renderer.cpp's HdrSide, a vkCmdCopyImage from the
// resolve's render target) waits until something could change that target, and the present reads
// the target itself when the front buffer's copy is still waiting: the copy is then never made.
// Only passes write a render target's image (BeginPass: draws, clears; a new target is a new image),
// so a waiting copy is recorded before the next pass, before its target is destroyed, and before
// the next side copy (keeping their order); a reset of HDR Output's images drops it. In Carbon the
// front buffer's resolve is the frame's last, so every present reads the target (M1 Air, HDR on:
// 1201 of 1201 presents; the target still held the copy's texels at 4302 of 4302 checks; -0.75 ms
// GPU a frame at QHD and at 1080p, 2026-11-10). Vulkan-free: tests/hdr_side_defer_test.cpp.
#pragma once
#include <optional>
#include <utility>

namespace gpu::hdrsidedefer
{
    // Copy: the copy's description, with its target `rt` and its side `side` (pointers).
    template <class Copy>
    class Pending
    {
    public:
        bool Waiting() const { return m_copy.has_value(); }

        // A new copy waits; the one waiting before it (if any) is the caller's to record first.
        std::optional<Copy> Defer(const Copy& copy) { return std::exchange(m_copy, copy); }

        // A pass, or anything else that must see the side: the waiting copy, to record now.
        std::optional<Copy> Take() { return std::exchange(m_copy, std::nullopt); }

        // A target about to be destroyed: its waiting copy, to record now.
        std::optional<Copy> TakeFor(const void* rt)
        {
            if (m_copy && m_copy->rt == rt)
                return Take();
            return std::nullopt;
        }

        // The present, reading `side`: its copy if that waits (read its target instead; the copy
        // is dropped). Otherwise nothing: another side's copy keeps waiting.
        std::optional<Copy> ReadAtPresent(const void* side)
        {
            if (side && m_copy && m_copy->side == side)
                return Take();
            return std::nullopt;
        }

        // HDR Output's images are being destroyed.
        void Drop() { m_copy.reset(); }

    private:
        std::optional<Copy> m_copy;
    };
}
