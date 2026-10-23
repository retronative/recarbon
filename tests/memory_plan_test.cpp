// From SpeedBreaker v0.1.8's tests/memory_plan_test.cpp (GPL-3.0-or-later).
// ReCarbon changes: the Mac's part only. A Mac has one device, Apple silicon
// through MoltenVK, so upstream's Steam Machine, Steam Deck, NVIDIA, Intel and
// Turnip memory types (and the overrides and test hook planned on them) aren't
// ported. Kept: upstream's Apple memory (its types from a MoltenVK log) and its
// plans; added: the MacBook Air M5's and M1's heaps, and the reasons, as
// ReCarbon logs them ("guest memory imported (zero-copy) (an integrated GPU
// (system memory)); upload ring in CPU-mapped VRAM, memory type 1 (CPU-mapped
// VRAM (heap 0, 24 GB) holds it)", 8.00 GB on the M1; game/m3/hand04.log and
// the M1's runs, 2026-10-23).
//
// Tests for gpu::memplan (runtime/gpu/memory_plan.h): where guest memory and
// the upload ring go. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/memory_plan_test.cpp -o build/memory_plan_test
//   build/memory_plan_test
#include <gpu/memory_plan.h>

#include <cstdio>
#include <initializer_list>
#include <string>

using namespace gpu::memplan;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

constexpr uint64_t MB = 1ull << 20, GB = 1ull << 30;
constexpr uint32_t kMoltenVk = 14;  // VK_DRIVER_ID_MOLTENVK

// "name:type,..." (import has no type; a place whose heap is too small for
// it is marked "!": tried only as a last resort).
static std::string Text(const Plan& p, bool ring)
{
    std::string s;
    for (const Candidate& c : p.order)
    {
        if (!s.empty())
            s += ",";
        s += Name(c.place, ring);
        if (c.place != Place::kImport)
            s += ":" + std::to_string(c.type);
        if (!c.fits)
            s += "!";
    }
    return s;
}

static void Expect(const char* what, const Plan& p, bool ring, const char* want)
{
    std::string got = Text(p, ring);
    printf("%-58s %s  (%s)\n", what, got.c_str(), p.why.c_str());
    CHECK(got == want, "%s: %s, want %s", what, got.c_str(), want);
}

// Apple silicon (MoltenVK): private, shared (the log: ring in type 1, flags
// F), memoryless; upstream's heap, and the M5's as logged.
static Memory Apple(uint64_t heap = 10922 * MB)
{
    return { { { 0x1, 0 }, { 0xF, 0 }, { 0x11, 0 } }, { heap } };
}

static GuestInput Input(bool importable, const char* shared = nullptr, const char* shadow = nullptr)
{
    GuestInput in;
    in.gpu = Gpu::kIntegrated;
    in.importable = importable;
    in.importRefusal = ImportRefusal(kMoltenVk);
    in.overrides = ParseOverrides(shared, shadow, nullptr);
    return in;
}

// The copy's type when the copy is used, for PlanRing (kNoType: imported).
static uint32_t CopyType(const Plan& guest)
{
    return guest.order.front().place == Place::kImport ? kNoType : guest.order.front().type;
}

int main()
{
    printf("plans:\n");
    struct Mac
    {
        const char* name;
        uint64_t heap;
        const char* ringWhy;  // as logged; nullptr: upstream's, not logged here
    };
    for (const Mac& mac : { Mac{ "Apple", 10922 * MB, nullptr }, Mac{ "M5", 24 * GB, "CPU-mapped VRAM (heap 0, 24 GB) holds it" },
             Mac{ "M1", 8 * GB, "CPU-mapped VRAM (heap 0, 8.00 GB) holds it" } })
    {
        Memory m = Apple(mac.heap);
        Plan g = PlanGuestMemory(m, Input(true));
        Expect((std::string(mac.name) + ", guest memory").c_str(), g, false, "import,copy-vram:1");
        Plan r = PlanRing(m, ~0u, CopyType(g));
        Expect((std::string(mac.name) + ", ring").c_str(), r, true, "ring-vram:1,ring-cached:1");
        if (mac.ringWhy)
        {
            CHECK(g.why == "an integrated GPU (system memory)", "%s: guest memory's reason \"%s\"", mac.name, g.why.c_str());
            CHECK(r.why == mac.ringWhy, "%s: the ring's reason \"%s\", logged \"%s\"", mac.name, r.why.c_str(), mac.ringWhy);
        }
    }
    // MoltenVK has no cached type outside device memory: the ring goes to its
    // shared type.
    Expect("Apple ring, RECARBON_RING_MEMORY=cached", PlanRing(Apple(), ~0u, kNoType, Place::kCached), true, "ring-cached:1,ring-vram:1");
    CHECK(ParseOverrides(nullptr, nullptr, "cached").ring == Place::kCached, "RECARBON_RING_MEMORY=cached");

    printf("types:\n");
    CHECK(FindType(Apple(), ~0u, Place::kCached, false) == kNoType, "no cached system memory on Apple");
    CHECK(FindType(Apple(), ~0u, Place::kCached, true) == 1, "the ring's cached type on Apple");
    CHECK(FindType(Apple(), ~0u, Place::kVram, false) == 1, "the shared type is Apple's CPU-mapped VRAM");
    CHECK(Fits(Apple(24 * GB), 1, kGuestBytes, kRingBytes), "the M5's heap holds both");
    CHECK(Size(256 * MB) == "256 MB" && Size(8573157376ull) == "7.98 GB" && Size(16621477888ull) == "15 GB", "sizes %s %s %s",
        Size(256 * MB).c_str(), Size(8573157376ull).c_str(), Size(16621477888ull).c_str());
    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
