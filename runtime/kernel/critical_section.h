// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// RtlEnterCriticalSection and friends (imports_unleashed.cpp) on a guest
// XRTL_CRITICAL_SECTION. Its fields are big-endian because guest code reads
// them: NFS: Carbon's XAudio (sub_82A4E7A8) reads RecursionCount, leaves the
// section that many times to let the audio thread in, and enters it again.
// Read host-endian, a count of 1 was 0x01000000: the first leave released
// the section, the rest drove the count negative, and a thread entering in
// between could never release it, so a movie's decoder thread waited for it
// forever (ROADMAP M4: the movie-teardown hang).
// `thread` is the caller's owner value (its r13); 0 means unowned.
#pragma once
#include <atomic>
#include <cstdint>
#include <xbox.h>

namespace kernel
{
    inline std::atomic_ref<uint32_t> CriticalSectionOwner(XRTL_CRITICAL_SECTION& cs)
    {
        static_assert(sizeof(cs.OwningThread) == sizeof(uint32_t));
        return std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(&cs.OwningThread));
    }

    inline void InitializeCriticalSection(XRTL_CRITICAL_SECTION& cs)
    {
        cs.LockCount = -1;
        cs.RecursionCount = 0;
        cs.OwningThread = 0;
    }

    inline bool TryEnterCriticalSection(XRTL_CRITICAL_SECTION& cs, uint32_t thread)
    {
        const uint32_t self = ByteSwap(thread);
        uint32_t previousOwner = 0;
        if (CriticalSectionOwner(cs).compare_exchange_strong(previousOwner, self) || previousOwner == self)
        {
            cs.RecursionCount = cs.RecursionCount + 1;
            return true;
        }
        return false;
    }

    inline void EnterCriticalSection(XRTL_CRITICAL_SECTION& cs, uint32_t thread)
    {
        const uint32_t self = ByteSwap(thread);
        auto owner = CriticalSectionOwner(cs);
        while (true)
        {
            // Strong: a spurious failure would leave previousOwner 0 and
            // wait(0) on a free section.
            uint32_t previousOwner = 0;
            if (owner.compare_exchange_strong(previousOwner, self) || previousOwner == self)
            {
                cs.RecursionCount = cs.RecursionCount + 1;
                return;
            }
            owner.wait(previousOwner);
        }
    }

    inline void LeaveCriticalSection(XRTL_CRITICAL_SECTION& cs)
    {
        cs.RecursionCount = cs.RecursionCount - 1;
        if (cs.RecursionCount != 0)
            return;

        auto owner = CriticalSectionOwner(cs);
        owner.store(0);
        owner.notify_one();
    }
}
