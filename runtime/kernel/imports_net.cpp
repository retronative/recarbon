// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// XAM networking (NetDll_*), modelled as a console whose network stack
// starts but has no link: the title's offline path. Only the calls NFS:
// Carbon makes are implemented, each when it was first reached (ROADMAP M3);
// the rest stay logging stubs (import_stubs.cpp). Reference: Xenia,
// src/xenia/kernel/xam/xam_net.cc (BSD-3-Clause), at 95a5c3ee.
#include <stdafx.h>
#include "function.h"
#include <cpu/guest_thread.h>

#include <cstring>

namespace
{
    // XNetStartupParams: 13 bytes of configuration, kept for XNetGetOpt(1).
    uint8_t s_startupParams[13]{};
}

// Xenia NetDll_XNetStartup_entry: keeps the parameters, returns 0 (started).
uint32_t NetDll_XNetStartup(uint32_t caller, const uint8_t* params)
{
    if (params)
    {
        const uint8_t size = params[0];  // cfgSizeOfStruct
        memcpy(s_startupParams, params, std::min<size_t>(size ? size : sizeof(s_startupParams), sizeof(s_startupParams)));
    }
    fprintf(stderr, "[net] XNetStartup (caller %u%s): started, no network link\n", caller, params ? ", with parameters" : "");
    return 0;
}

// Xenia NetDll_WSAStartup_entry (its non-Windows path): fills X_WSADATA with
// the requested version, empty description and status, and Xenia's own
// guessed limits (100 sockets, 1024-byte datagrams); returns 0. Also sets
// wHighVersion, as WSAStartup does. Offsets: X_WSADATA in xam_net.cc.
uint32_t NetDll_WSAStartup(uint32_t caller, uint32_t version, uint8_t* data)
{
    if (data)
    {
        *reinterpret_cast<be<uint16_t>*>(data + 0) = uint16_t(version);    // version
        *reinterpret_cast<be<uint16_t>*>(data + 2) = uint16_t(version);    // version_high
        data[4] = 0;                                                       // description[257]
        data[261] = 0;                                                     // system_status[129]
        *reinterpret_cast<be<uint16_t>*>(data + 390) = uint16_t(100);      // max_sockets
        *reinterpret_cast<be<uint16_t>*>(data + 392) = uint16_t(1024);     // max_udpdg
    }
    fprintf(stderr, "[net] WSAStartup(%u.%u, caller %u)\n", version & 0xFF, (version >> 8) & 0xFF, caller);
    return 0;
}

uint32_t NtWaitForMultipleObjectsEx(uint32_t count, be<uint32_t>* handles, uint32_t waitType, uint32_t waitMode,
    uint32_t alertable, be<int64_t>* timeout);
uint32_t RtlNtStatusToDosError(uint32_t status);

// WSAWaitForMultipleEvents (Xenia: the same wait, NtWaitForMultipleObjectsEx).
// Unlike Xenia, which passes the millisecond timeout as a positive (NT:
// absolute) time and returns 0 for any event, this keeps the WSA contract: a
// relative timeout in ms (0xFFFFFFFF: none), WSA_WAIT_EVENT_0 + index,
// WSA_WAIT_TIMEOUT (0x102) or WSA_WAIT_IO_COMPLETION (0xC0) as the NT
// statuses of the same values, and WSA_WAIT_FAILED with the last error set.
uint32_t NetDll_WSAWaitForMultipleEvents(uint32_t count, be<uint32_t>* events, uint32_t waitAll, uint32_t timeoutMs,
    uint32_t alertable)
{
    constexpr uint32_t kWaitFailed = 0xFFFFFFFF;
    if (count == 0 || count > 64)
    {
        GuestThread::SetLastError(87);  // ERROR_INVALID_PARAMETER
        return kWaitFailed;
    }
    be<int64_t> relative = -int64_t(timeoutMs) * 10000;  // 100 ns units, negative: relative
    const uint32_t status = NtWaitForMultipleObjectsEx(count, events, waitAll ? 0 : 1, 1, alertable,
        timeoutMs == 0xFFFFFFFF ? nullptr : &relative);
    if (status < count || status == STATUS_TIMEOUT || status == 0xC0)
        return status;
    GuestThread::SetLastError(RtlNtStatusToDosError(status));
    return kWaitFailed;
}

// Xenia NetDll_XNetGetTitleXnAddr_entry: a loopback address, not online,
// MAC CC:CC:CC:CC:CC:CC, XNET_GET_XNADDR_STATIC (4). Xenia gives a nonzero MAC
// because titles build "random" ids from it (its note on RakNet/58411436).
// XNADDR (xam_net.cc): ina +0, inaOnline +4 (network byte order),
// wPortOnline +8, abEnet[6] +10, abOnline[20] +16.
uint32_t NetDll_XNetGetTitleXnAddr(uint32_t caller, uint8_t* address)
{
    constexpr uint32_t kXnAddrStatic = 0x00000004;
    if (address)
    {
        const uint8_t loopback[4] = { 127, 0, 0, 1 };
        memcpy(address + 0, loopback, 4);
        memset(address + 4, 0, 6);        // inaOnline, wPortOnline
        memset(address + 10, 0xCC, 6);    // abEnet
        memset(address + 16, 0, 20);      // abOnline
    }
    return kXnAddrStatic;
}

GUEST_FUNCTION_HOOK(__imp__NetDll_XNetStartup, NetDll_XNetStartup);
GUEST_FUNCTION_HOOK(__imp__NetDll_WSAStartup, NetDll_WSAStartup);
GUEST_FUNCTION_HOOK(__imp__NetDll_WSAWaitForMultipleEvents, NetDll_WSAWaitForMultipleEvents);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetGetTitleXnAddr, NetDll_XNetGetTitleXnAddr);
