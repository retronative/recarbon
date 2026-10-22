// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// Tests for gpu::RingState (runtime/gpu/ring_state.h): a batch of the old ring
// that ends after a re-initialization must not move the new ring's read
// position. NFS: Carbon re-initializes its ring right after its first kicks;
// before this fix the old batch's end (index 31) overwrote the reset and the
// command processor skipped the new ring's first packets, so D3D's fence wait
// spun forever (ROADMAP M3). Header only; from the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/ring_state_test.cpp -o build/ring_state_test
//   build/ring_state_test
#include <gpu/ring_state.h>

#include <atomic>
#include <cstdio>
#include <thread>

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main()
{
    using gpu::RingState;
    uint32_t wptr = 0;  // CP_RB_WPTR, as the game and the reset leave it
    auto readWptr = [&] { return wptr; };

    // Not initialized: no ring to read.
    {
        RingState ring;
        CHECK(ring.Take().primary == 0, "an uninitialized ring has no base");
    }

    // A batch on an unchanged ring advances the read position.
    {
        RingState ring;
        ring.Initialize(0x05B51000, 32768, [&] { wptr = 0; });
        RingState::Snapshot taken = ring.Take();
        CHECK(taken.readIndex == 0, "a new ring is read from its start (read %u)", taken.readIndex);
        CHECK(ring.Advance(taken, 31), "a batch on the same ring advances");
        CHECK(ring.Take().readIndex == 31, "read index %u, want 31", ring.Take().readIndex);
        CHECK(ring.Advance(ring.Take(), 40) && ring.Take().readIndex == 40, "a second batch advances too");
    }

    // The NFS: Carbon sequence: a batch of the first ring (32 KB at
    // 0x05B51000, 31 dwords) is still executing when the game re-initializes
    // the ring (64 KB at 0x05B49000). Its end must not land on the new ring.
    {
        RingState ring;
        ring.Initialize(0x05B51000, 32768, [&] { wptr = 0; });
        RingState::Snapshot oldBatch = ring.Take();
        ring.Initialize(0x05B49000, 65536, [&] { wptr = 0; });  // the guest thread, mid-batch
        CHECK(!ring.Advance(oldBatch, 31), "a batch that raced a re-initialization is dropped");
        RingState::Snapshot now = ring.Take();
        CHECK(now.primary == 0x05B49000 && now.size == 65536, "the new ring stands (%08X, %u)", now.primary, now.size);
        CHECK(now.readIndex == 0, "the new ring is read from 0, not the old batch's end (read %u)", now.readIndex);
        // The game then writes packets 0..30 and sets the write pointer to 31:
        // the command processor sees work (31 != 0) and reads it.
        CHECK(now.readIndex != 31, "the new ring's first 31 dwords would be skipped");
        CHECK(ring.Advance(now, 31) && ring.Take().readIndex == 31, "the new ring's batch advances");
    }

    // Re-initializing to the same base and size still restarts the ring.
    {
        RingState ring;
        ring.Initialize(0x05B49000, 65536, [&] { wptr = 0; });
        RingState::Snapshot taken = ring.Take();
        ring.Initialize(0x05B49000, 65536, [&] { wptr = 0; });
        CHECK(!ring.Advance(taken, 12) && ring.Take().readIndex == 0, "the same ring, re-initialized, restarts at 0");
    }

    // Concurrently: a consumer advancing from snapshots while another thread
    // re-initializes never leaves a read index from before the last reset.
    {
        RingState ring;
        ring.Initialize(0x05B51000, 32768, [&] { wptr = 0; });
        std::atomic<bool> stop{ false };
        std::thread consumer([&] {
            uint32_t w = 0;
            while (!stop.load())
                ring.Advance(ring.Take(), (w = (w + 1) % 8000) + 1);
        });
        for (int i = 0; i < 20000; i++)
        {
            ring.Initialize(0x05B49000, 65536, [&] { wptr = 0; });
            RingState::Snapshot s = ring.Take();
            CHECK(s.generation > 0, "generation counts");
        }
        stop = true;
        consumer.join();
        RingState::Snapshot stale = ring.Take();
        ring.Initialize(0x05B49000, 65536, [&] { wptr = 0; });
        CHECK(!ring.Advance(stale, 99) && ring.Take().readIndex == 0, "after the last reset, a stale batch can't move it");
    }

    // The second half of the race: the old ring's write pointer (31) still in
    // the register when the new ring starts. The reset restarts it, so a
    // snapshot right after it sees no work until D3D writes its next kick's.
    {
        RingState ring;
        ring.Initialize(0x05B51000, 32768, [&] { wptr = 0; });
        wptr = 31;                                            // D3D's last kick on the first ring
        uint32_t w = 0;
        RingState::Snapshot s = ring.Take(readWptr, w);
        CHECK(w == 31 && ring.Advance(s, w), "the first ring's batch runs");
        ring.Initialize(0x05B49000, 65536, [&] { wptr = 0; });  // VdInitializeRingBuffer again
        s = ring.Take(readWptr, w);
        CHECK(s.primary == 0x05B49000 && s.readIndex == 0 && w == 0,
            "after the reset: new ring, read 0, write pointer 0 (read %u, write %u)", s.readIndex, w);
        CHECK(w == s.readIndex, "no work on the new ring before D3D writes to it");
        wptr = 31;                                            // D3D writes packets 0..30, then the pointer
        s = ring.Take(readWptr, w);
        CHECK(w == 31 && s.readIndex == 0, "D3D's packets are new work (read %u, write %u)", s.readIndex, w);
    }

    if (g_failures)
        printf("ring_state_test: %d failure(s)\n", g_failures);
    else
        printf("ring_state_test: all passed\n");
    return g_failures ? 1 : 0;
}
