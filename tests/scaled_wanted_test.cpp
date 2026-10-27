// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/scaled_wanted.h (from SpeedBreaker v0.2.0's ScaledWanted): a scaled resolve keeps its
// host-resolution samples only where a scaled reader (a scaled texture's check, the front buffer)
// asked for that memory in the last 120 frames. Overlap, not containment; a stamp is s_frame + 1
// when asked (0: never); it lapses 120 frames on; with the feature off every resolve keeps its
// samples, as before. Header only: no Vulkan.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/scaled_wanted_test.cpp -o build/scaled_wanted_test && build/scaled_wanted_test
#include <gpu/scaled_wanted.h>

#include <cstdio>
#include <unordered_map>
#include <vector>

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

    using gpu::scaledwanted::Wanted;
    using gpu::scaledwanted::Want;
    using gpu::scaledwanted::kWantFrames;
}

int main()
{
    // A reader of [0x100000, 0x110000) asked at frame 50 (stamp 51).
    std::vector<Want> one = { { 0x100000, 0x10000, 51 } };

    printf("---- overlap ----\n");
    Check(Wanted(one, 0x100000, 0x10000, 50), "the same range, the same frame");
    Check(Wanted(one, 0x108000, 0x1000, 60), "a resolve inside it");
    Check(Wanted(one, 0x0F0000, 0x20000, 60), "a resolve covering it");
    Check(Wanted(one, 0x10F000, 0x2000, 60), "a resolve overlapping its end");
    Check(Wanted(one, 0x0FF000, 0x2000, 60), "a resolve overlapping its start");
    Check(!Wanted(one, 0x110000, 0x1000, 60), "a resolve starting at its end: no overlap");
    Check(!Wanted(one, 0x0FF000, 0x1000, 60), "a resolve ending at its start: no overlap");
    Check(!Wanted(one, 0x200000, 0x1000, 60), "elsewhere");

    printf("---- the 120-frame window ----\n");
    Check(Wanted(one, 0x100000, 0x1000, 50 + kWantFrames - 1), "119 frames on: still wanted");
    Check(!Wanted(one, 0x100000, 0x1000, 50 + kWantFrames), "120 frames on: lapsed");
    std::vector<Want> never = { { 0x100000, 0x10000, 0 } };
    Check(!Wanted(never, 0x100000, 0x10000, 0), "a check never asked for (stamp 0)");
    Check(!Wanted(std::vector<Want>{}, 0x100000, 0x10000, 10), "no readers at all");

    printf("---- several readers ----\n");
    std::vector<Want> many = { { 0x100000, 0x1000, 5 }, { 0x300000, 0x1000, 400 } };
    Check(Wanted(many, 0x300000, 0x1000, 450), "the recent one counts");
    Check(!Wanted(many, 0x100000, 0x1000, 450), "the lapsed one doesn't");

    printf("---- the renderer's map ----\n");
    std::unordered_map<uint64_t, Want> map = { { 1, { 0x100000, 0x10000, 51 } } };
    Check(Wanted(map, 0x100000, 0x1000, 60), "map entries are read the same way");

    printf("---- the switch ----\n");
    Check(gpu::scaledwanted::Keep(false, never, 0x100000, 0x10000, 1000), "off: every resolve keeps its samples, as before");
    Check(!gpu::scaledwanted::Keep(true, never, 0x100000, 0x10000, 1000), "on: an unread resolve writes 1x only");
    Check(gpu::scaledwanted::Keep(true, one, 0x100000, 0x10000, 60), "on: a read one keeps them");

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
