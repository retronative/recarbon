// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// Tests for runtime/gpu/edram_format.h: which EDRAM colour formats share
// their pixels (one host render target) and which take 64 bits a sample, as
// Xenia's xenos.h defines them. NFS: Carbon's movie player draws a frame as
// k_2_10_10_10_FLOAT (3) and blends its overlay on the same EDRAM as
// k_2_10_10_10_FLOAT_AS_16_16_16_16 (12): separate targets made every movie
// frame black (ROADMAP M4). Header only; from the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/edram_format_test.cpp -o build/edram_format_test
//   build/edram_format_test
#include <gpu/edram_format.h>

#include <cstdio>
#include <initializer_list>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main()
{
    using gpu::IsColorFormat64bpp;
    using gpu::StorageColorFormat;

    // The movie's two views of EDRAM base 0 are one target.
    CHECK(StorageColorFormat(12) == 3, "2_10_10_10_FLOAT_AS_16_16_16_16 is stored as 2_10_10_10_FLOAT (got %u)", StorageColorFormat(12));
    CHECK(StorageColorFormat(12) == StorageColorFormat(3), "formats 3 and 12 share a target");
    CHECK(StorageColorFormat(10) == 2, "2_10_10_10_AS_10_10_10_10 is stored as 2_10_10_10 (got %u)", StorageColorFormat(10));

    // Every other format is its own storage (Xenia's default case).
    for (uint32_t f : { 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 14u, 15u })
        CHECK(StorageColorFormat(f) == f, "format %u is its own storage (got %u)", f, StorageColorFormat(f));
    // Storage formats are fixed points: normalizing twice changes nothing.
    for (uint32_t f = 0; f < 16; f++)
        CHECK(StorageColorFormat(StorageColorFormat(f)) == StorageColorFormat(f), "format %u normalizes once", f);

    // 64 bpp: exactly Xenia's three; the _AS_16_16_16_16 variant is 32.
    for (uint32_t f = 0; f < 16; f++)
    {
        const bool expected = f == 5 || f == 7 || f == 15;
        CHECK(IsColorFormat64bpp(f) == expected, "format %u: 64 bpp %d, want %d", f, int(IsColorFormat64bpp(f)), int(expected));
    }
    CHECK(!IsColorFormat64bpp(12), "2_10_10_10_FLOAT_AS_16_16_16_16 takes 32 bits a sample");
    // A shared target can't have two sample sizes.
    for (uint32_t f = 0; f < 16; f++)
        CHECK(IsColorFormat64bpp(f) == IsColorFormat64bpp(StorageColorFormat(f)), "format %u and its storage agree on size", f);

    if (g_failures)
        printf("edram_format_test: %d failure(s)\n", g_failures);
    else
        printf("edram_format_test: all passed\n");
    return g_failures ? 1 : 0;
}
