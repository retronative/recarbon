// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// KeSetBasePriorityThread (imports_unleashed.cpp) on macOS. The priority is
// read as a Win32 THREAD_PRIORITY_* value, as the runtime's Windows path
// already does (15 time critical, 2 highest, 1 above normal, 0 normal, below
// 0 lower), and raised to a QoS class. It's never lowered: a thread the game
// asks to run lower keeps the default.
// NFS: Carbon's XAudio asks for 15 on its mixing thread (sub_82A4FCC8, lr
// 0x82A503AC). The render callback (apu/audio.cpp, USER_INTERACTIVE) waits
// for that thread every 5.33 ms. At the default QoS it was often late
// (renders of 15-103 ms), and the output ran dry in the front end
// (ROADMAP M5).
#pragma once

namespace kernel
{
    enum class HostQos
    {
        Unchanged,      // the thread keeps the default
        UserInitiated,  // QOS_CLASS_USER_INITIATED
        UserInteractive // QOS_CLASS_USER_INTERACTIVE
    };

    constexpr HostQos HostQosForPriority(int priority)
    {
        if (priority >= 15)
            return HostQos::UserInteractive;
        if (priority >= 1)
            return HostQos::UserInitiated;
        return HostQos::Unchanged;
    }
}
