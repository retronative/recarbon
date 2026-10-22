// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// Title-lifetime kernel calls NFS: Carbon makes that NFS: Most Wanted didn't.
// Reference: Xenia (BSD-3-Clause) at 95a5c3ee.
#include <stdafx.h>
#include "function.h"
#include <mutex.h>

#include <algorithm>
#include <vector>

namespace
{
    struct TerminateNotification
    {
        uint32_t routine, priority;
    };
    Mutex s_terminateLock;
    std::vector<TerminateNotification> s_terminateNotifications;
}

// Xenia ExRegisterTitleTerminateNotification_entry (xboxkrnl_modules.cc)
// with X_EX_TITLE_TERMINATE_REGISTRATION (xbox.h): routine at +0, priority at
// +4. create != 0 adds the routine, 0 removes it. The runtime records them;
// like Xenia, which keeps the call in KernelState::TerminateTitle commented
// out, it doesn't run them when the game exits.
void ExRegisterTitleTerminateNotification(const be<uint32_t>* registration, uint32_t create)
{
    const uint32_t routine = registration[0], priority = registration[1];
    std::lock_guard lock(s_terminateLock);
    if (create)
        s_terminateNotifications.push_back({ routine, priority });
    else
        std::erase_if(s_terminateNotifications, [&](const TerminateNotification& n) { return n.routine == routine; });
    fprintf(stderr, "[kernel] title terminate notification %s: routine %08X priority %08X (%zu registered; not run at exit)\n",
        create ? "added" : "removed", routine, priority, s_terminateNotifications.size());
}

GUEST_FUNCTION_HOOK(__imp__ExRegisterTitleTerminateNotification, ExRegisterTitleTerminateNotification);
