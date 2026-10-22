// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// NFS: Carbon's deferred render-callback queue (state at 0x82B8C764) is a
// lock-free single-producer, single-consumer list, as MW's was (SpeedBreaker's
// runtime/game/render_queue.cpp, 0x82909650): sub_82469448 records a
// callback (copies the entry, stores its callback at +0 and size at +4, then
// publishes: the write pointer at +20, then the count at +0 of the state) and
// the main thread plays them back in sub_824694C0 (sub_824E9720), calling
// each entry's callback. It has no barriers: it relies on the entry's stores
// becoming visible before the write pointer and count that publish it. x86
// keeps stores in order; ARM64 doesn't: the main thread called a callback
// read before its entry was written, 0x8209B4AC in .rdata ("indirect call
// ... not a recompiled function", lr 0x82469528; a 16-minute soak, ROADMAP
// M6). Mid-asm hooks (config/recarbon.toml) add a release fence before the
// publish (0x824694B0) and an acquire fence before an entry is read
// (0x82469518). The other writers of the state (sub_824693C0, sub_82469400,
// sub_824E9A08, sub_824E9A80) reset it on the consumer's own thread.
#include <stdafx.h>
#include <atomic>

void RenderQueuePublish()
{
    std::atomic_thread_fence(std::memory_order_release);
}

void RenderQueueAcquire()
{
    std::atomic_thread_fence(std::memory_order_acquire);
}
