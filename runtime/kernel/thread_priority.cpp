// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// ReCarbon: the frame threads' QoS (thread_priority.h). No stdafx.h, so the
// unit test links this file on its own.
#include "thread_priority.h"
#include "sleep0_schedule.h"

#include <chrono>
#include <cstdlib>
#include <vector>
#if defined(__APPLE__)
#include <pthread.h>
#include <cstdio>
#endif

namespace kernel
{
    namespace
    {
        thread_local bool s_frameThread = false;
#if defined(__APPLE__)
        thread_local qos_class_t s_original = QOS_CLASS_UNSPECIFIED;  // the frame thread's class before its raise
#endif
    }

    bool IsFrameThread()
    {
        return s_frameThread;
    }

#if defined(__APPLE__)
    namespace
    {
        const char* QosName(qos_class_t qos)
        {
            switch (qos)
            {
            case QOS_CLASS_USER_INTERACTIVE: return "user-interactive";
            case QOS_CLASS_USER_INITIATED: return "user-initiated";
            case QOS_CLASS_DEFAULT: return "default";
            case QOS_CLASS_UTILITY: return "utility";
            case QOS_CLASS_BACKGROUND: return "background";
            case QOS_CLASS_UNSPECIFIED: return "unspecified";
            default: return "unknown";
            }
        }
    }

    namespace
    {
        const auto s_launch = std::chrono::steady_clock::now();
        const std::vector<double> s_qosAt = [] {
            const char* v = std::getenv("RECARBON_FRAME_THREAD_QOS_AT");
            std::vector<double> times = sleep0schedule::ParseTimes(v);
            if (v && times.empty())
                fprintf(stderr, "[qos] RECARBON_FRAME_THREAD_QOS_AT=%s ignored: ascending seconds since launch\n", v);
            else if (v)
                fprintf(stderr, "[qos] RECARBON_FRAME_THREAD_QOS_AT: %zu switches of the frame threads' class, the first at %.1f s\n", times.size(), times[0]);
            return times;
        }();

        // A frame thread, every call: the class its phase asks for, set once per phase.
        void FollowQosSchedule(FrameThreadQos initial)
        {
            thread_local sleep0schedule::ThreadPhase seen;
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - s_launch).count();
            const int phase = sleep0schedule::PhaseAt(s_qosAt, seconds);
            if (!seen.Changed(phase))
                return;
            const qos_class_t cls = SwitchedQosClass(s_original, FrameThreadQosInPhase(initial, phase));
            char name[64] = {};
            pthread_getname_np(pthread_self(), name, sizeof(name));
            if (pthread_set_qos_class_self_np(cls, 0) == 0)
                fprintf(stderr, "[qos] RECARBON_FRAME_THREAD_QOS_AT: phase %d at %.1f s: frame thread %s now %s\n", phase, seconds, name, QosName(cls));
        }
    }

    void RaiseFrameThreadQos()
    {
        static const FrameThreadQos target = [] {
            const char* v = std::getenv("RECARBON_FRAME_THREAD_QOS");
            const FrameThreadQos t = ParseFrameThreadQos(v);
            if (t == FrameThreadQos::Interactive)
                fprintf(stderr, "[qos] RECARBON_FRAME_THREAD_QOS=interactive: frame threads raised to user-interactive\n");
            else if (v)
                fprintf(stderr, "[qos] RECARBON_FRAME_THREAD_QOS=%s ignored (only \"interactive\"): user-initiated\n", v);
            return t;
        }();
        if (s_frameThread) [[likely]]
        {
            if (!s_qosAt.empty())
                FollowQosSchedule(target);
            return;
        }
        RaiseFrameThreadQosTo(target);
    }

    void RaiseFrameThreadQosTo(FrameThreadQos target)
    {
        if (s_frameThread)
            return;
        s_frameThread = true;

        // The thread's own requested class. An override another thread holds
        // on it (KeSetBasePriorityThread) isn't included, and isn't lowered
        // either: the thread runs at the higher of the two.
        qos_class_t current = QOS_CLASS_UNSPECIFIED;
        pthread_get_qos_class_np(pthread_self(), &current, nullptr);
        s_original = current;
        char name[64] = {};
        pthread_getname_np(pthread_self(), name, sizeof(name));
        if (RaisesFrameThreadQos(current, target) && pthread_set_qos_class_self_np(QosClassOf(target), 0) == 0)
            fprintf(stderr, "[qos] frame thread %s raised to %s (was %s)\n", name, QosName(QosClassOf(target)), QosName(current));
        else
            fprintf(stderr, "[qos] frame thread %s kept %s\n", name, QosName(current));
    }
#else
    void RaiseFrameThreadQos()
    {
        s_frameThread = true;
    }
    void RaiseFrameThreadQosTo(FrameThreadQos) { RaiseFrameThreadQos(); }
#endif
}
