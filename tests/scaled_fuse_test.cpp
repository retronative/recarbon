// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/scaled_fuse.h (SpeedBreaker v0.1.9's SCALED_FUSE, write-through only): which cached scaled
// texture a scaled resolve may also store its samples into, and where its destination sits in it.
// The scaled memory stays the truth, so the texture may take the resolve only if it is exactly what
// its untile from that memory would give before (current) and after (the whole rectangle lands in
// it): loaded, storable, the same tiled 32-bit layout and pitch, the destination whole 4 KB tiles
// in, the resolve's range inside it, its rectangle inside it, at a whole scale. And the index the
// renderer keeps them in (by first guest byte), found by address and forgotten when a texture
// goes. Header only: no Vulkan.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/scaled_fuse_test.cpp -o build/scaled_fuse_test && build/scaled_fuse_test
#include <gpu/scaled_fuse.h>

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

    using namespace gpu::scaledfuse;

    // A 1280x720 scaled texture at 0x00400000, pitch 1280 (40 tiles a row), loaded and current.
    Texture Tex()
    {
        return { .uploaded = true, .scaled = true, .storable = true, .tiled = true, .current = true, .bpbLog2 = 2,
            .guestStart = 0x00400000, .guestSize = 1280 * 736 * 4, .pitch = 1280, .guestWidth = 1280, .guestHeight = 720 };
    }

    // The whole surface resolved into it, at its own base.
    Resolve Whole()
    {
        return { .destBase = 0x00400000, .destPitch = 1280, .rangeBase = 0x00400000, .rangeBytes = 1280 * 736 * 4, .x0 = 0, .y0 = 0,
            .width = 1280, .height = 720, .wholeScale = true };
    }

    bool Refused(const Texture& t, const Resolve& r, const char* why)
    {
        Place p;
        const char* got = Refusal(t, r, p);
        return got && strstr(got, why);
    }
}

int main()
{
    printf("---- taken ----\n");
    Place p{ 99, 99 };
    Check(Refusal(Tex(), Whole(), p) == nullptr && p.hostX == 0 && p.hostY == 0, "the whole surface at its own base: texels from 0,0");
    // The second of three 256-row strips, through a base 8 tile rows in (8 x 40 tiles).
    Resolve strip = Whole();
    strip.destBase = 0x00400000 + 8 * 40 * 4096;
    strip.rangeBase = strip.destBase;
    strip.rangeBytes = 8 * 40 * 4096;
    strip.height = 256;
    Check(Refusal(Tex(), strip, p) == nullptr && p.hostX == 0 && p.hostY == 256, "a strip 8 tile rows in: texels from 0,256");
    // Half a row of tiles in (20 tiles): x offset 640.
    Resolve right = Whole();
    right.destBase = 0x00400000 + 20 * 4096;
    right.rangeBase = right.destBase;
    right.rangeBytes = 4096;
    right.width = 640;
    right.height = 32;
    Check(Refusal(Tex(), right, p) == nullptr && p.hostX == 640 && p.hostY == 0, "20 tiles in: texels from 640,0");
    Resolve inner = Whole();
    inner.x0 = 64;
    inner.y0 = 32;
    inner.width = 128;
    inner.height = 64;
    Check(Refusal(Tex(), inner, p) == nullptr, "a rectangle inside it, at its own base");

    printf("---- the texture ----\n");
    Texture t = Tex();
    t.uploaded = false;
    Check(Refused(t, Whole(), "not loaded"), "not loaded yet: its untile hasn't run");
    t = Tex();
    t.scaled = false;
    Check(Refused(t, Whole(), "not scaled"), "a 1x texture (the 1x fused path's)");
    t = Tex();
    t.storable = false;
    Check(Refused(t, Whole(), "no store view"), "no R32_UINT store view");
    t = Tex();
    t.tiled = false;
    Check(Refused(t, Whole(), "layout"), "linear");
    t = Tex();
    t.bpbLog2 = 1;
    Check(Refused(t, Whole(), "layout"), "not 32-bit");
    t = Tex();
    t.current = false;
    Check(Refused(t, Whole(), "not current"), "written since its load, or a page without a current scaled copy");

    printf("---- the resolve ----\n");
    Resolve r = Whole();
    r.wholeScale = false;
    Check(Refused(Tex(), r, "whole scale"), "1.5x: not taken (write-through is whole scales only)");
    r = Whole();
    r.destPitch = 640;
    Check(Refused(Tex(), r, "pitch"), "another pitch");
    r = Whole();
    r.destBase += 2048;
    r.rangeBase += 2048;
    r.rangeBytes -= 4096;
    Check(Refused(Tex(), r, "whole tiles"), "the destination half a tile in");
    r = Whole();
    r.destBase -= 4096;
    r.rangeBase -= 4096;
    Check(Refused(Tex(), r, "before the texture"), "the destination before the texture");
    r = Whole();
    r.rangeBytes += 4096;
    Check(Refused(Tex(), r, "range"), "the range past the texture's end");
    r = Whole();
    r.width = 1312;
    Check(Refused(Tex(), r, "rectangle"), "the rectangle wider than the texture (a texel past its right edge would change the pool alone)");
    r = right;
    r.width = 672;  // 640 + 672 = 1312 > 1280
    Check(Refused(Tex(), r, "rectangle"), "20 tiles in, a rectangle reaching past its right edge");
    r = Whole();
    r.y0 = 700;
    r.height = 32;
    Check(Refused(Tex(), r, "rectangle"), "a rectangle below its last row");
    r = Whole();
    r.x0 = -8;
    Check(Refused(Tex(), r, "rectangle"), "a rectangle starting left of it");

    printf("---- the index ----\n");
    Index index;
    int a = 1, b = 2, c = 3;
    index.Add(0x00400000, 0x00100000, &a);
    index.Add(0x00800000, 0x00080000, &b);
    index.Add(0x00820000, 0x00010000, &c);  // a smaller one inside b
    Check(index.Find(0x00400000) == std::vector<void*>{ &a }, "a destination at a texture's start");
    Check(index.Find(0x004FF000) == std::vector<void*>{ &a }, "one at its last page");
    Check(index.Find(0x00500000).empty(), "one just past its end");
    Check(index.Find(0x003FF000).empty(), "one before it");
    Check((index.Find(0x00821000) == std::vector<void*>{ &c, &b }), "inside two: the nearest start first");
    Check(index.Find(0x00890000).empty(), "past two textures' ends, within the largest's reach: neither");
    index.Erase(0x00820000, &c);
    Check(index.Find(0x00821000) == std::vector<void*>{ &b }, "a texture that went is never found again");
    index.Erase(0x00800000, &a);
    Check(index.Find(0x00800000) == std::vector<void*>{ &b }, "erasing another texture's start keeps it (only its own entry goes)");
    index.Add(0x00800000, 0x00080000, &c);
    Check(index.Find(0x00800000) == std::vector<void*>{ &c }, "a new texture at the same start takes the entry");
    index.Erase(0x00800000, &b);
    Check(index.Find(0x00800000) == std::vector<void*>{ &c }, "and the old one going doesn't drop it");
    index.Clear();
    Check(index.Find(0x00400000).empty(), "cleared (a scale change drops every scaled texture)");

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
