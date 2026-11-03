// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See ../thermal.h.
//
// macOS and iOS: NSProcessInfo's thermal state (Foundation only, so it
// builds for both; UIKit glue is platform/*.mm, iOS only). Manual reference
// counting like the rest of the runtime, though nothing here is allocated:
// processInfo is the shared instance.
#include <platform/thermal.h>

#import <Foundation/Foundation.h>
#include <notify.h>

#include <mutex>

namespace platform::thermal::detail
{
    State ReadSystem()
    {
        @autoreleasepool
        {
            switch ([NSProcessInfo processInfo].thermalState)
            {
            case NSProcessInfoThermalStateNominal: return State::Nominal;
            case NSProcessInfoThermalStateFair: return State::Fair;
            case NSProcessInfoThermalStateSerious: return State::Serious;
            case NSProcessInfoThermalStateCritical: return State::Critical;
            }
            return State::Unknown;
        }
    }

    // ReCarbon: the pressure level macOS posts for its thermal notifications,
    // readable without privileges (the token is registered once).
    bool ReadPressure(uint64_t& level)
    {
        static int token = -1;
        static std::once_flag once;
        std::call_once(once, [] {
            if (notify_register_check("com.apple.system.thermalpressurelevel", &token) != NOTIFY_STATUS_OK)
                token = -1;
        });
        uint64_t state = 0;
        if (token < 0 || notify_get_state(token, &state) != NOTIFY_STATUS_OK)
            return false;
        level = state;
        return true;
    }
}
