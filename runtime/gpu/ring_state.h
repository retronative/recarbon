// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// The primary ring buffer's base, size and read position, shared by the guest
// thread that (re)initializes it (VdInitializeRingBuffer) and the command
// processor's thread that consumes it (command_processor.cpp).
//
// SpeedBreaker kept these in plain variables: a re-initialization reset the
// read index to 0, but a batch of the old ring still executing on the command
// processor then stored its end (the old ring's write index) over that reset.
// NFS: Carbon re-initializes the ring right after its first kicks (32 KB at
// 0x05B51000, then 64 KB at 0x05B49000); when the old batch was still running,
// the new ring's packets up to that stale index were never read, D3D's fences
// stopped advancing and its fence wait (sub_826DBD78) spun forever (about one
// launch in two, ROADMAP M3). A batch now advances the read position only if
// the ring wasn't re-initialized while it ran; one that raced a reset is
// dropped, as a reset of the hardware's ring drops what it had in flight.
//
// The write pointer (CP_RB_WPTR) restarts at 0 with the ring too: Carbon's
// D3D device setup (sub_826DC488) sets its own copy of it (device+10824) to 0
// at 0x826DC6F8 without writing the register, and its next kick (through
// sub_826DB3B0) writes the new one. Left at the old ring's 31, the register
// had the command processor read the new ring's first 31 dwords before D3D
// wrote them, and then skip them. Taking a snapshot reads the register under
// the same lock as the reset, so the two never mix rings.
#pragma once
#include <cstdint>
#include <mutex>

namespace gpu
{
    class RingState
    {
    public:
        struct Snapshot
        {
            uint32_t primary = 0;    // physical address, 0 = not initialized
            uint32_t size = 0;       // bytes
            uint32_t readIndex = 0;  // dwords
            uint64_t generation = 0;
        };

        // VdInitializeRingBuffer: a new ring, read from its start, and the
        // write pointer restarted at 0 (resetWritePointer(), under the lock).
        template<typename ResetWritePointer>
        void Initialize(uint32_t primary, uint32_t size, ResetWritePointer resetWritePointer)
        {
            std::lock_guard lock(m_mutex);
            resetWritePointer();
            m_state.primary = primary;
            m_state.size = size;
            m_state.readIndex = 0;
            m_state.generation++;
        }

        Snapshot Take() const
        {
            std::lock_guard lock(m_mutex);
            return m_state;
        }

        // The ring and the write pointer (readWritePointer(), under the lock)
        // as one consistent pair.
        template<typename ReadWritePointer>
        Snapshot Take(ReadWritePointer readWritePointer, uint32_t& writeIndex) const
        {
            std::lock_guard lock(m_mutex);
            writeIndex = readWritePointer();
            return m_state;
        }

        // After a batch read from `taken` up to `writeIndex`: the new read
        // position, unless the ring was re-initialized since `taken` (then
        // false, and the reset position stands).
        bool Advance(const Snapshot& taken, uint32_t writeIndex)
        {
            std::lock_guard lock(m_mutex);
            if (taken.generation != m_state.generation)
                return false;
            m_state.readIndex = writeIndex;
            return true;
        }

    private:
        mutable std::mutex m_mutex;
        Snapshot m_state;
    };
}
