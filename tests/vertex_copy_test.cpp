// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/vertex_copy.h: which vertex ranges a draw copies into the upload ring (and which the write
// watch guards instead). The threshold's boundaries at 128 KB and, with RECARBON_VERTEX_COPY_128=0,
// 64 KB; ranges the GPU writes or owns, or past the draw's copy slots, are never copied; the ring
// bytes a copy takes, and that a draw's copies fit one ring slot. Header only: no Vulkan.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/vertex_copy_test.cpp -o build/vertex_copy_test && build/vertex_copy_test
#include <gpu/vertex_copy.h>

#include <cstdio>

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

    using namespace gpu::vertexcopy;
}

int main()
{
    printf("---- the threshold ----\n");
    Check(Threshold(nullptr) == 128 * 1024, "unset: 128 KB");
    Check(Threshold("1") == 128 * 1024 && Threshold("") == 128 * 1024, "=1 or empty: 128 KB");
    Check(Threshold("0") == 64 * 1024, "=0: 64 KB, as before");

    printf("---- its boundaries ----\n");
    const uint32_t t = Threshold(nullptr), legacy = Threshold("0");
    Check(Copy(128 * 1024, t, 0, false, false), "exactly 128 KB: copied");
    Check(!Copy(128 * 1024 + 4, t, 0, false, false), "past 128 KB: guarded");
    Check(Copy(0x12750, t, 0, false, false) && Copy(0x101A0, t, 0, false, false), "Carbon's 66-75 KB buffers: copied");
    Check(!Copy(0x4EE40, t, 0, false, false), "its 323 KB one: still guarded");
    Check(!Copy(0x101A0, legacy, 0, false, false) && Copy(64 * 1024, legacy, 0, false, false), "with 64 KB: as before");
    Check(!Copy(0, t, 0, false, false), "an empty range: nothing to copy");

    printf("---- never copied ----\n");
    Check(!Copy(4096, t, 0, true, false), "a range the GPU writes (a resolve into vertex memory)");
    Check(!Copy(4096, t, 0, false, true), "a range the GPU owns");
    Check(Copy(4096, t, kCopiesPerDraw - 1, false, false) && !Copy(4096, t, kCopiesPerDraw, false, false),
        "the draw's last copy slot, and none past it");

    printf("---- the ring ----\n");
    Check(RingBytes(1) == 256 && RingBytes(256) == 256 && RingBytes(257) == 512, "256-byte aligned");
    Check(RingBytes(t) == t, "128 KB takes 128 KB");
    Check(uint64_t(RingBytes(t)) * kCopiesPerDraw <= 16ull << 20, "a draw's copies fit one 16 MB ring slot");

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
