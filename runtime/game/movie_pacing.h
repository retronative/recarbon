// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
// ReCarbon: implemented by game/title_hooks.cpp, which has no Carbon hooks yet.
#pragma once
#include <cstdint>

namespace game
{
    // Any thread: when the XMV player last rendered a movie frame (ns, steady
    // clock; 0: none yet). A movie (the boot logos, the intro, a cutscene)
    // plays while these keep coming.
    int64_t LastMovieFrameNs();
}
