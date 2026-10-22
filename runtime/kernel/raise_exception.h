// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// RtlRaiseException (imports_rtl.cpp). The runtime has no SEH dispatch, so
// only the exception a title raises to name a thread is handled, as Xenia
// does (xboxkrnl_debug.cc, RtlRaiseException_entry: 0x406D1388 ->
// HandleSetThreadName, then return). The title raises it inside
// __try/__except(EXCEPTION_EXECUTE_HANDLER) (NFS: Carbon: sub_82949890, the
// XAPI RaiseException, called from 0x826D3748 and 0x82924F20), so returning
// resumes it where its handler would. Any other code is fatal.
#pragma once
#include <cstdint>

namespace kernel
{
    inline constexpr uint32_t kSetThreadNameException = 0x406D1388;
    // THREADNAME_INFO.dwType.
    inline constexpr uint32_t kThreadNameInfoType = 0x1000;

    // X_EXCEPTION_RECORD (big-endian, guest memory), as Xenia defines it.
    struct ExceptionRecordView
    {
        uint32_t code, flags, record, address, numberParameters;
        const uint32_t* information;  // numberParameters entries, host order
    };

    // THREADNAME_INFO, the exception's four parameters.
    struct ThreadNameRequest
    {
        uint32_t namePointer;  // guest address of a C string
        uint32_t threadId;     // 0xFFFFFFFF: the calling thread
    };

    // The thread-name request this exception carries, or false when it isn't
    // one (another code, or parameters that aren't a THREADNAME_INFO).
    inline bool ParseThreadName(const ExceptionRecordView& record, ThreadNameRequest& out)
    {
        if (record.code != kSetThreadNameException || record.numberParameters < 3 ||
            record.information[0] != kThreadNameInfoType)
            return false;
        out = { record.information[1], record.information[2] };
        return true;
    }
}
