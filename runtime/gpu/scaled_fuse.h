// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// A scaled resolve also stored straight into a cached scaled texture (gpu/renderer.cpp), from
// SpeedBreaker v0.1.9's SCALED_FUSE (GPL-3.0-or-later), its write-through half only (no HOLD).
// Each host sample the resolve writes to the scaled memory also goes into the texture, where its
// untile would put it (gpu/render_scale.h sbReaderHost), and the texture is then current without
// that untile: the scaled memory stays the truth, and the texture is a copy kept in step.
// So a texture may take a resolve only if it is exactly its untile's before (current: loaded, not
// written since, every page with a current scaled copy) and after: the same tiled 32-bit layout and
// pitch, the destination whole 4 KB tiles in (32-bit texels make a 4 KB tile, so the destination's
// texel x, y is the texture's x + hostX, y + hostY), the resolve's range and its whole rectangle
// inside it (a texel past its edge would change the scaled memory alone), at a whole scale.
// On by default; RECARBON_SCALED_FUSE=0 turns it off, and every scaled texture is untiled as before.
// Vulkan-free: tests/scaled_fuse_test.cpp checks it alone.
#pragma once
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <map>
#include <vector>

namespace gpu::scaledfuse
{
    // On unless RECARBON_SCALED_FUSE (`env`) starts with 0.
    inline bool On(const char* env) { return !env || env[0] != '0'; }

    struct Texture
    {
        bool uploaded = false;  // its untile ran
        bool scaled = false;
        bool storable = false;  // has an R32_UINT store view
        bool tiled = false;
        bool current = false;   // not written since its load, and every page has a current scaled copy
        uint32_t bpbLog2 = 0;
        uint32_t guestStart = 0, guestSize = 0;
        uint32_t pitch = 0;     // texels, aligned (as a resolve's destination pitch)
        uint32_t guestWidth = 0, guestHeight = 0;
    };

    struct Resolve
    {
        uint32_t destBase = 0, destPitch = 0;
        uint32_t rangeBase = 0, rangeBytes = 0;  // the guest memory it writes
        int32_t x0 = 0, y0 = 0;                  // its rectangle in the destination, guest texels
        uint32_t width = 0, height = 0;
        bool wholeScale = false;
    };

    struct Place
    {
        uint32_t hostX = 0, hostY = 0;  // the destination's texel 0,0 in the texture
    };

    // Why `t` can't take `r` (nullptr: it can, at `place`).
    inline const char* Refusal(const Texture& t, const Resolve& r, Place& place)
    {
        if (!t.uploaded)
            return "texture not loaded";
        if (!t.scaled)
            return "texture not scaled";
        if (!t.storable)
            return "texture has no store view";
        if (!t.tiled || t.bpbLog2 != 2)
            return "texture layout";
        if (!t.current)
            return "texture not current";
        if (!r.wholeScale)
            return "not a whole scale";
        if (t.pitch != r.destPitch)
            return "pitch differs";
        if (r.destBase < t.guestStart)
            return "destination before the texture";
        if ((r.destBase - t.guestStart) % 4096 != 0)
            return "destination not whole tiles in";
        if (r.rangeBase < t.guestStart || uint64_t(r.rangeBase) + r.rangeBytes > uint64_t(t.guestStart) + t.guestSize)
            return "range beyond the texture";
        const uint32_t tile = (r.destBase - t.guestStart) >> 12, tilesPerRow = t.pitch >> 5 ? t.pitch >> 5 : 1;
        const uint32_t hostX = tile % tilesPerRow * 32, hostY = tile / tilesPerRow * 32;
        if (r.x0 < 0 || r.y0 < 0 || uint64_t(hostX) + uint32_t(r.x0) + r.width > t.guestWidth ||
            uint64_t(hostY) + uint32_t(r.y0) + r.height > t.guestHeight)
            return "rectangle beyond the texture";
        place = { hostX, hostY };
        return nullptr;
    }

    // The scaled textures by first guest byte (the latest made there) and the largest one's size,
    // so a resolve finds every texture holding its destination: the nearest start first.
    template <typename T>
    class BasicIndex
    {
    public:
        void Add(uint32_t start, uint32_t size, T* texture)
        {
            m_entries[start] = { size, texture };
            m_maxSize = std::max(m_maxSize, size);
        }
        // Forgets `texture` (going), not a newer one made at the same start since.
        void Erase(uint32_t start, const T* texture)
        {
            if (auto it = m_entries.find(start); it != m_entries.end() && it->second.texture == texture)
                m_entries.erase(it);
        }
        void Clear()
        {
            m_entries.clear();
            m_maxSize = 0;
        }
        std::vector<T*> Find(uint32_t address) const
        {
            std::vector<T*> found;
            for (auto it = m_entries.upper_bound(address); it != m_entries.begin();)
            {
                --it;
                if (address - it->first >= m_maxSize)
                    break;  // none starting earlier reaches it
                if (address - it->first < it->second.size)
                    found.push_back(it->second.texture);
            }
            return found;
        }

    private:
        struct Entry
        {
            uint32_t size;
            T* texture;
        };
        std::map<uint32_t, Entry> m_entries;
        uint32_t m_maxSize = 0;
    };
    using Index = BasicIndex<void>;
}
