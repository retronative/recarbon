// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/texture_eviction.h: which cached textures the renderer may destroy at a
// frame's end to keep the cache under its budget. Never one the GPU may still
// use (its last submission not completed), one used recently, or a scaled one;
// stale ones (their guest memory rewritten since they were loaded) first, then
// the longest unused; no more than asked for per frame; the same choice every
// time. Header only: no Vulkan.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/texture_eviction_test.cpp -o build/texture_eviction_test && build/texture_eviction_test
#include <gpu/texture_eviction.h>

#include <cstdio>
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

    using gpu::eviction::Candidate;
    using gpu::eviction::Policy;
    using gpu::eviction::Select;

    constexpr uint64_t MB = 1ull << 20;

    // 10 MB each, last used at frame 0 in submission 1 unless set otherwise.
    Candidate C(uint64_t key, uint64_t lastFrame = 0, uint64_t lastSubmission = 1, bool scaled = false, bool stale = false,
        uint64_t bytes = 10 * MB)
    {
        return { key, bytes, lastFrame, lastSubmission, scaled, stale };
    }

    // Frame 1000, submissions up to 100 completed, at most 64 a frame, unused
    // for 600 frames at least.
    Policy P(uint64_t used, uint64_t budget)
    {
        return { .frame = 1000, .completedSubmission = 100, .usedBytes = used, .budgetBytes = budget, .minIdleFrames = 600,
            .maxPerFrame = 64 };
    }

    std::vector<uint64_t> Sel(const std::vector<Candidate>& c, const Policy& p) { return Select(c, p); }

    bool Same(const std::vector<uint64_t>& got, const std::vector<uint64_t>& want) { return got == want; }
}

int main()
{
    printf("---- under the budget ----\n");
    std::vector<Candidate> three = { C(1), C(2), C(3) };
    Check(Sel(three, P(30 * MB, 30 * MB)).empty(), "at the budget: nothing goes");
    Check(Sel(three, P(30 * MB, 64 * MB)).empty(), "under it: nothing goes");
    Check(Sel(three, P(30 * MB, 0)).empty(), "a budget of 0 is no eviction at all (off)");
    Check(Sel({}, P(30 * MB, 10 * MB)).empty(), "no candidates");

    printf("---- over the budget ----\n");
    Check(Same(Sel(three, P(30 * MB, 15 * MB)), { 1, 2 }), "until under it, and no further (30 -> 10 MB)");
    Check(Same(Sel(three, P(30 * MB, 20 * MB)), { 1 }), "exactly at it after one");
    Check(Same(Sel({ C(7, 300), C(8, 100), C(9, 200) }, P(30 * MB, 10 * MB)), { 8, 9 }), "the longest unused first");

    printf("---- stale first ----\n");
    Check(Same(Sel({ C(1, 0), C(2, 300, 1, false, true), C(3, 100) }, P(30 * MB, 20 * MB)), { 2 }),
        "a stale texture goes before an older valid one");

    printf("---- never ----\n");
    Check(Sel({ C(1, 0, 101), C(2, 0, 150) }, P(20 * MB, 0 + 1)).empty(), "one whose submission hasn't completed");
    Check(Same(Sel({ C(1, 0, 100) }, P(10 * MB, 1)), { 1 }), "its submission completed exactly: it may go");
    Check(Sel({ C(1, 401), C(2, 999) }, P(20 * MB, 1)).empty(), "one used in the last 600 frames, even over the budget");
    Check(Same(Sel({ C(1, 400) }, P(10 * MB, 1)), { 1 }), "600 frames unused exactly: it may go");
    Check(Sel({ C(1, 0, 1, true) }, P(10 * MB, 1)).empty(), "a scaled one");
    Check(Sel({ C(1, 2000) }, P(10 * MB, 1)).empty(), "one used after this frame (no wrap-around)");

    printf("---- per frame, and the same every time ----\n");
    std::vector<Candidate> many;
    for (uint64_t k = 0; k < 200; k++)
        many.push_back(C(1000 - k));
    Check(Sel(many, P(2000 * MB, 1)).size() == 64, "at most maxPerFrame");
    Check(Same(Sel({ C(5), C(3), C(4) }, P(30 * MB, 15 * MB)), { 3, 4 }), "ties by key");
    Check(Same(Sel({ C(4), C(5), C(3) }, P(30 * MB, 15 * MB)), Sel({ C(5), C(3), C(4) }, P(30 * MB, 15 * MB))),
        "the order given doesn't matter");

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
