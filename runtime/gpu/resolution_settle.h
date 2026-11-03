// ReCarbon (v0.1.4). GPL-3.0-or-later (see COPYING).
//
// When a new Internal Resolution setting is applied. Each change rebuilds
// every scaled render target and drops the scaled textures (renderer.cpp's
// ApplyRenderScale: a hitch), and stepping through the entries in the menu
// (Auto, HD, FHD, QHD, 4K UHD) used to rebuild at every step: a v0.1.2 log
// has two hitches of about 480 ms while a player stepped through them. So,
// while the menu is open, a new value is applied once it has stood for
// kQuietSeconds; when the menu closes (or a change comes from elsewhere),
// at once. The row shows the new value at once either way. A change of
// what the same setting asks for (the picture's size, the window's display)
// keeps the renderer's own 30-frame settle. Header only
// (tests/resolution_settle_test.cpp); the command processor's thread.
#pragma once
#include <cstdint>

namespace gpu
{
    class ResolutionSettle
    {
    public:
        static constexpr double kQuietSeconds = 0.25;

        // Each swap while `setting` (the value now) differs from the one
        // applied: true when it is to be applied now. `now` in seconds,
        // `menuOpen` the settings menu (or a dialog over the game) is open.
        bool Ready(int32_t setting, double now, bool menuOpen)
        {
            if (!m_waiting || setting != m_value)
            {
                m_waiting = true;
                m_value = setting;
                m_since = now;
            }
            if (menuOpen && now - m_since < kQuietSeconds)
                return false;
            m_waiting = false;
            return true;
        }

        // The setting is back to the one applied (stepped away and back):
        // nothing waits.
        void Reset() { m_waiting = false; }

    private:
        bool m_waiting = false;
        int32_t m_value = 0;
        double m_since = 0.0;
    };
}
