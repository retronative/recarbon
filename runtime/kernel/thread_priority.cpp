// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// ReCarbon: the frame threads' QoS (thread_priority.h). No stdafx.h, so the
// unit test links this file on its own.
#include "thread_priority.h"

#if defined(__APPLE__)
#include <pthread.h>
#include <cstdio>
#endif

namespace kernel
{
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

    void RaiseFrameThreadQos()
    {
        thread_local bool s_done = false;
        if (s_done) [[likely]]
            return;
        s_done = true;

        // The thread's own requested class. An override another thread holds
        // on it (KeSetBasePriorityThread) isn't included, and isn't lowered
        // either: the thread runs at the higher of the two.
        qos_class_t current = QOS_CLASS_UNSPECIFIED;
        pthread_get_qos_class_np(pthread_self(), &current, nullptr);
        char name[64] = {};
        pthread_getname_np(pthread_self(), name, sizeof(name));
        if (RaisesFrameThreadQos(current) && pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0) == 0)
            fprintf(stderr, "[qos] frame thread %s raised to user-initiated (was %s)\n", name, QosName(current));
        else
            fprintf(stderr, "[qos] frame thread %s kept %s\n", name, QosName(current));
    }
#else
    void RaiseFrameThreadQos() {}
#endif
}
