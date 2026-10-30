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

    // The class a frame thread is raised to: USER_INITIATED, or with
    // RECARBON_FRAME_THREAD_QOS=interactive (an experiment, off by default)
    // USER_INTERACTIVE, so the render thread competes for the performance
    // cores like the runtime's own presenter and command processor (it ended
    // 18-25% of its waits on efficiency cores at user-initiated:
    // measured on a MacBook Air M5, October 2026).
    enum class FrameThreadQos { Initiated, Interactive };

    inline FrameThreadQos ParseFrameThreadQos(const char* v)
    {
        return v && v[0] == 'i' && v[1] == 'n' && v[2] == 't' && v[3] == 'e' && v[4] == 'r' && v[5] == 'a' && v[6] == 'c' && v[7] == 't' &&
                       v[8] == 'i' && v[9] == 'v' && v[10] == 'e' && v[11] == '\0'
                   ? FrameThreadQos::Interactive
                   : FrameThreadQos::Initiated;
    }

#if defined(__APPLE__)
    // Whether a frame thread at `current` is raised to `target`. Never
    // lowered: one already at the target or higher keeps its class.
    constexpr bool RaisesFrameThreadQos(qos_class_t current, FrameThreadQos target = FrameThreadQos::Initiated)
    {
        switch (current)
        {
        case QOS_CLASS_UNSPECIFIED:
        case QOS_CLASS_BACKGROUND:
        case QOS_CLASS_UTILITY:
        case QOS_CLASS_DEFAULT:
            return true;
        case QOS_CLASS_USER_INITIATED:
            return target == FrameThreadQos::Interactive;
        case QOS_CLASS_USER_INTERACTIVE:
        default:
            return false;
        }
    }

    // RECARBON_FRAME_THREAD_QOS_AT=<s>,<s>,... (a diagnostic, off by default):
    // the frame threads' class switches between RECARBON_FRAME_THREAD_QOS's
    // and the other at those seconds since launch (kernel/sleep0_schedule.h's
    // times), applied by each frame thread at its next present or poll, so one
    // session compares both classes.
    constexpr FrameThreadQos FrameThreadQosInPhase(FrameThreadQos initial, int phase)
    {
        const bool other = phase % 2 == 1;
        return (initial == FrameThreadQos::Interactive) != other ? FrameThreadQos::Interactive : FrameThreadQos::Initiated;
    }

    constexpr qos_class_t QosClassOf(FrameThreadQos target)
    {
        return target == FrameThreadQos::Interactive ? QOS_CLASS_USER_INTERACTIVE : QOS_CLASS_USER_INITIATED;
    }

    // The class a switch sets: `target`'s, but never below the thread's own
    // original class (a user-interactive thread stays user-interactive).
    constexpr qos_class_t SwitchedQosClass(qos_class_t original, FrameThreadQos target)
    {
        return original == QOS_CLASS_USER_INTERACTIVE ? QOS_CLASS_USER_INTERACTIVE : QosClassOf(target);
    }
#endif

    // At the top of VdSwap and XamInputGetState: the calling thread's first
    // call raises it as RaisesFrameThreadQos says (to RECARBON_FRAME_THREAD_QOS's
    // class) and logs it once ("[qos] frame thread <name> ..."); every later
    // call is a thread_local check. A no-op off Apple.
    void RaiseFrameThreadQos();
    // The same, to `target` (for tests; RaiseFrameThreadQos picks the target once).
    void RaiseFrameThreadQosTo(FrameThreadQos target);

    // Whether the calling thread has been through RaiseFrameThreadQos: a
    // thread that presents or polls the controller (on every platform).
    bool IsFrameThread();
}
