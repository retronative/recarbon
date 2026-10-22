// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// A guest critical section (kernel/critical_section.h) as guest code sees
// it. NFS: Carbon's XAudio (sub_82A4E7A8) reads RecursionCount with lwz
// (big-endian), leaves the section that many times so the audio thread can
// take it, and enters it again as many times. With the fields host-endian
// it read 1 as 0x01000000: a thread that entered in between kept the
// section forever, and a movie's teardown waited for it (ROADMAP M4.8).
// Sequential, with one helper thread joined before anything can block.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime -Itools/XenonRecomp/XenonUtils tests/critical_section_test.cpp -o build/critical_section_test
//   build/critical_section_test
#include <kernel/critical_section.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

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

    // What lwz at this field's offset gives the guest.
    uint32_t GuestWord(const XRTL_CRITICAL_SECTION& cs, size_t offset)
    {
        uint8_t b[4];
        memcpy(b, reinterpret_cast<const uint8_t*>(&cs) + offset, 4);
        return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
    }

    uint32_t GuestRecursionCount(const XRTL_CRITICAL_SECTION& cs)
    {
        return GuestWord(cs, offsetof(XRTL_CRITICAL_SECTION, RecursionCount));
    }

    uint32_t GuestOwner(const XRTL_CRITICAL_SECTION& cs)
    {
        return GuestWord(cs, offsetof(XRTL_CRITICAL_SECTION, OwningThread));
    }
}

int main()
{
    constexpr uint32_t kAudio = 0x82B60100;  // owner values: guest r13
    constexpr uint32_t kDecoder = 0x716002A0;

    static_assert(offsetof(XRTL_CRITICAL_SECTION, LockCount) == 16);
    static_assert(offsetof(XRTL_CRITICAL_SECTION, RecursionCount) == 20);
    static_assert(offsetof(XRTL_CRITICAL_SECTION, OwningThread) == 24);

    XRTL_CRITICAL_SECTION cs{};
    kernel::InitializeCriticalSection(cs);
    Check(GuestWord(cs, offsetof(XRTL_CRITICAL_SECTION, LockCount)) == 0xFFFFFFFF, "LockCount starts at -1");
    Check(GuestRecursionCount(cs) == 0 && GuestOwner(cs) == 0, "a new section is free");

    kernel::EnterCriticalSection(cs, kDecoder);
    kernel::EnterCriticalSection(cs, kDecoder);
    Check(GuestRecursionCount(cs) == 2, "the guest reads RecursionCount 2 after two enters");
    Check(GuestOwner(cs) == kDecoder, "the guest reads the owner it entered with");
    kernel::LeaveCriticalSection(cs);
    Check(GuestRecursionCount(cs) == 1, "the guest reads RecursionCount 1 after one enter");

    // sub_82A4E7A8: leave as many times as the guest reads, let another
    // thread in and out, enter as many times again.
    const uint32_t n = GuestRecursionCount(cs);
    Check(n == 1, "the yield loop leaves once");
    if (n == 1)
    {
        for (uint32_t i = 0; i < n; i++)
            kernel::LeaveCriticalSection(cs);
        Check(GuestOwner(cs) == 0, "the section is free after the yield loop's leaves");

        bool audioEntered = false;
        std::thread audio([&] {
            audioEntered = kernel::TryEnterCriticalSection(cs, kAudio);
            if (audioEntered)
                kernel::LeaveCriticalSection(cs);
        });
        audio.join();
        Check(audioEntered, "the audio thread gets the section in between");
        // Checked before entering again: under the bug this is where the
        // decoder thread would have waited forever.
        Check(GuestOwner(cs) == 0, "the audio thread releases the section");

        if (GuestOwner(cs) == 0)
        {
            for (uint32_t i = 0; i < n; i++)
                kernel::EnterCriticalSection(cs, kDecoder);
            Check(GuestOwner(cs) == kDecoder && GuestRecursionCount(cs) == 1, "the yield loop gets the section back once");
            kernel::LeaveCriticalSection(cs);
            Check(GuestOwner(cs) == 0 && GuestRecursionCount(cs) == 0, "and leaves it free");
        }
    }

    Check(kernel::TryEnterCriticalSection(cs, kAudio), "a free section can be entered");
    Check(!kernel::TryEnterCriticalSection(cs, kDecoder), "an owned section can't be entered by another thread");
    kernel::LeaveCriticalSection(cs);

    if (g_failures)
    {
        printf("critical_section_test: %d failed\n", g_failures);
        return 1;
    }
    printf("critical_section_test: all passed\n");
    return 0;
}
