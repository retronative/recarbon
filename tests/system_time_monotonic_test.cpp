// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// Guest system time (guesttime::SystemTime100ns, runtime/cpu/guest_time.h)
// never goes back, with several threads reading it as the game's do
// (KeQuerySystemTime, the time-stamp bundle, absolute waits). It was read
// from macOS's system_clock on every call: 1 us resolution, and it steps
// back (by 1 us when threads read it close together, dozens of times a
// second with 4-7 readers; by ms to s when the host's time is adjusted).
// guest_time_test's "system time went back ... up to 10 (1.00 us)" failed
// about 1 run in 8 from it (ROADMAP M4). This reproduces it without
// suspensions, so a failure is the host clock, not the suspension logic.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/system_time_monotonic_test.cpp runtime/cpu/guest_time.cpp -o build/system_time_monotonic_test
//   build/system_time_monotonic_test
#include <cpu/guest_time.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

int main()
{
    constexpr int kReaders = 8;
    constexpr auto kDuration = std::chrono::seconds(5);
    std::atomic<bool> stop{ false };
    struct alignas(128) Counts
    {
        uint64_t reads = 0, back = 0;
        int64_t worst = 0;
    };
    std::vector<Counts> counts(kReaders);
    std::vector<std::thread> readers;
    for (int i = 0; i < kReaders; i++)
        readers.emplace_back([&, i] {
            Counts& c = counts[i];
            int64_t last = guesttime::SystemTime100ns();
            while (!stop.load(std::memory_order_relaxed))
            {
                const int64_t now = guesttime::SystemTime100ns();
                c.reads++;
                if (now < last)
                {
                    c.back++;
                    if (last - now > c.worst)
                        c.worst = last - now;
                }
                last = now;
            }
        });
    std::this_thread::sleep_for(kDuration);
    stop = true;
    for (std::thread& t : readers)
        t.join();

    Counts total;
    for (const Counts& c : counts)
    {
        total.reads += c.reads;
        total.back += c.back;
        if (c.worst > total.worst)
            total.worst = c.worst;
    }
    printf("system_time_monotonic_test: %d readers, %llu readings, went back %llu times (worst %lld x 100 ns)\n", kReaders,
        (unsigned long long)total.reads, (unsigned long long)total.back, (long long)total.worst);
    if (total.reads < 1000)
    {
        printf("  FAIL: too few readings to mean anything\n");
        return 1;
    }
    if (total.back)
    {
        printf("  FAIL: guest system time went back\n");
        return 1;
    }
    printf("system_time_monotonic_test: all passed\n");
    return 0;
}
