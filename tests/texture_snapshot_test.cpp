// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/texture_snapshot.h: a texture load's untile reads a copy of its source in the upload ring
// instead of guest memory, so the write watch needn't guard that memory until the GPU runs it
// (the game streaming textures beside it then waited for the GPU: 16 KB host pages hold four
// guest pages). Which loads, where the untile reads, where its staging goes. Hand-computed.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/texture_snapshot_test.cpp -o build/texture_snapshot_test
#include <gpu/texture_snapshot.h>

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
}

int main()
{
    using namespace gpu::texsnap;
    printf("which loads copy their source\n");
    Check(Eligible(0x10000, 0x8000, false, false), "64 KB with 32 KB of mips");
    Check(Eligible(kMaxBytes, kMaxBytes, false, false), "at the limit, each range");
    Check(!Eligible(kMaxBytes + 4, 0, false, false), "a larger base level: guarded as before");
    Check(!Eligible(0x10000, kMaxBytes + 4, false, false), "larger mips: guarded as before");
    Check(!Eligible(0x10000, 0, true, false), "split guest memory (the untile's reads span parts): as before");
    Check(!Eligible(0x10000, 0, false, true), "a shadow copy of guest memory: as before");
    Check(!Eligible(0, 0, false, false), "nothing to copy");

    printf("the untile's source address in the copy\n");
    // A copy of [0x1274E000, +0x10000) at ring byte 0x2340100: a slice 0x4000 in reads 0x2344100.
    Check(SourceBase(0x2340100, 0x1274E000, 0x1274E000) == 0x2340100, "the range's start");
    Check(SourceBase(0x2340100, 0x1274E000, 0x12752000) == 0x2344100, "a slice inside it");

    printf("the staging after the copy, in one allocation\n");
    Check(CopyBytes(0x10000) == 0x10000 && CopyBytes(0x8001) == 0x8100, "the copy rounded to 256 bytes");
    Check(StagingAt(0x2340100, 0x8001) == 0x2340100 + 0x8100, "the staging starts after it");
    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
