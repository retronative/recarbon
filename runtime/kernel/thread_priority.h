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
//
// ReCarbon: the frame threads. A guest thread that presents a frame (VdSwap)
// or polls the controller (XamInputGetState) drives the game's frames: in
// Carbon, the main game thread in the menus and the entry thread
// (recarbon-main) presenting in a race. On an M1 Air under a 6-busy-loop CPU
// load, raising
// both to USER_INITIATED cut frame gaps of 100 ms or more in a Quick Race from
// 5.3 to 0.3 per run (3 runs each; idle and audio unchanged).
#pragma once

#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

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

#if defined(__APPLE__)
    // Whether a frame thread at `current` is raised to USER_INITIATED. Never
    // lowered: one already at USER_INITIATED or higher keeps its class.
    constexpr bool RaisesFrameThreadQos(qos_class_t current)
    {
        switch (current)
        {
        case QOS_CLASS_UNSPECIFIED:
        case QOS_CLASS_BACKGROUND:
        case QOS_CLASS_UTILITY:
        case QOS_CLASS_DEFAULT:
            return true;
        case QOS_CLASS_USER_INITIATED:
        case QOS_CLASS_USER_INTERACTIVE:
        default:
            return false;
        }
    }
#endif

    // At the top of VdSwap and XamInputGetState: the calling thread's first
    // call raises it as RaisesFrameThreadQos says and logs it once ("[qos]
    // frame thread <name> ..."); every later call is a thread_local check. A
    // no-op off Apple.
    void RaiseFrameThreadQos();
}
