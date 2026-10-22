// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// vmem::ResetHostRange (kernel/vmem.h), as PageHeap uses it to return freed
// or decommitted guest pages to zero: on this host's real page size (16 KB
// on Apple silicon) and real mmap. A 4 KB-page heap range that starts on a
// host page and ends inside one must be zeroed without touching the guest
// pages after it (another allocation's). The old code remapped the range
// as is and mmap rounded its length up to whole host pages. Port of
// SpeedBreaker v0.1.8's fix (86cfaa4); this test is ReCarbon's own.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/vmem_host_pages_test.cpp -o build/vmem_host_pages_test
//   build/vmem_host_pages_test
#include <kernel/vmem.h>

#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

namespace
{
    int g_failures = 0;
    size_t g_hostPage = 0;
    bool g_remapAligned = true;
    std::vector<std::pair<uintptr_t, size_t>> g_remaps;

    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            printf("  FAIL: %s\n", what);
            g_failures++;
        }
    }

    // The real remap, recording what it was asked to do.
    bool RecordingRemap(void* host, size_t bytes)
    {
        g_remaps.emplace_back(uintptr_t(host), bytes);
        if (uintptr_t(host) % g_hostPage || bytes % g_hostPage)
            g_remapAligned = false;
        return vmem::RemapAnon(host, bytes);
    }

    // Region of `pages` host pages filled with 0xAA; reset [offset, offset+bytes)
    // and check that exactly that range is zero.
    void Case(const char* what, size_t pages, size_t offset, size_t bytes)
    {
        // The host pages wholly inside [offset, offset+bytes) of a host-page-aligned region.
        const size_t first = (offset + g_hostPage - 1) / g_hostPage, last = (offset + bytes) / g_hostPage;
        const size_t expectRemapBytes = last > first ? (last - first) * g_hostPage : 0;
        const size_t size = pages * g_hostPage;
        auto* region = static_cast<uint8_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0));
        if (region == MAP_FAILED)
        {
            printf("  FAIL: %s: mmap failed\n", what);
            g_failures++;
            return;
        }
        memset(region, 0xAA, size);
        g_remaps.clear();
        g_remapAligned = true;
        vmem::ResetHostRange(region + offset, bytes, g_hostPage, RecordingRemap);
        size_t zeroInside = 0, keptOutside = 0;
        for (size_t i = 0; i < size; i++)
        {
            bool inside = i >= offset && i < offset + bytes;
            if (inside && region[i] == 0)
                zeroInside++;
            if (!inside && region[i] == 0xAA)
                keptOutside++;
        }
        printf("%s\n", what);
        Check(zeroInside == bytes, "  every byte of the range is zero");
        Check(keptOutside == size - bytes, "  no byte outside the range changed");
        Check(g_remapAligned, "  remaps only whole, aligned host pages");
        size_t remapped = 0;
        for (auto& [a, n] : g_remaps)
        {
            remapped += n;
            Check(a >= uintptr_t(region + offset) && a + n <= uintptr_t(region + offset + bytes), "  remaps nothing outside the range");
        }
        Check(remapped == expectRemapBytes, "  remaps the whole host pages inside the range, and only those");
        munmap(region, size);
    }
}

int main()
{
    g_hostPage = size_t(sysconf(_SC_PAGESIZE));
    const size_t guest = 0x1000;  // the Virtual4K heap's page
    printf("vmem_host_pages_test: host page %zu bytes\n", g_hostPage);
    if (g_hostPage <= guest)
        printf("  (host pages are no larger than guest pages here: the cases still must hold)\n");

    // The bug's case: starts on a host page, ends inside one (5 guest pages).
    Case("starts on a host page, ends inside one", 4, g_hostPage, 5 * guest);
    // Inside one host page: no remap at all.
    Case("inside one host page", 4, g_hostPage + guest, guest);
    // Partial head, whole pages, partial tail.
    Case("partial head, whole host pages, partial tail", 6, guest, 3 * g_hostPage);
    // Whole host pages exactly.
    Case("whole host pages exactly", 4, g_hostPage, 2 * g_hostPage);

    if (g_failures)
    {
        printf("vmem_host_pages_test: %d failed\n", g_failures);
        return 1;
    }
    printf("vmem_host_pages_test: all passed\n");
    return 0;
}
