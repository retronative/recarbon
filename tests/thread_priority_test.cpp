// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// The QoS class a guest thread priority gets on macOS (kernel/thread_priority.h,
// KeSetBasePriorityThread). Carbon's XAudio asks for 15 (time critical) on the
// thread its 5.33 ms render callback waits for. When priorities were ignored
// on macOS, the front end's audio ran dry (ROADMAP M5).
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/thread_priority_test.cpp -o build/thread_priority_test
//   build/thread_priority_test
#include <kernel/thread_priority.h>

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
    using kernel::HostQos;
    using kernel::HostQosForPriority;

    Check(HostQosForPriority(15) == HostQos::UserInteractive, "15 (time critical, Carbon's XAudio thread) is user-interactive");
    Check(HostQosForPriority(16) == HostQos::UserInteractive, "16 (Carbon passes it; Windows clamps it to 15) is user-interactive");
    Check(HostQosForPriority(17) == HostQos::UserInteractive, "17 (Carbon passes it) is user-interactive");
    Check(HostQosForPriority(2) == HostQos::UserInitiated, "2 (highest) is user-initiated");
    Check(HostQosForPriority(1) == HostQos::UserInitiated, "1 (above normal) is user-initiated");
    Check(HostQosForPriority(0) == HostQos::Unchanged, "0 (normal) keeps the default");
    Check(HostQosForPriority(-2) == HostQos::Unchanged, "-2 (lowest) is never lowered");
    Check(HostQosForPriority(-15) == HostQos::Unchanged, "-15 (idle) is never lowered");

    if (g_failures)
    {
        printf("thread_priority_test: %d failed\n", g_failures);
        return 1;
    }
    printf("thread_priority_test: all passed\n");
    return 0;
}
