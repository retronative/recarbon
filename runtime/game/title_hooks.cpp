// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// NFS: Carbon's title layer. SpeedBreaker's game/ held MW's hooks: mid-asm
// hooks and function overrides at MW addresses (render-queue fences,
// Hor+/Vert+ view scaling, sun glare, nitrous trails, frame pacing, movie
// pacing) and stores into MW's data. Those addresses mean nothing in
// Carbon's image, so none of that code is built here
// (reference/ARCHITECTURE.md). Carbon's own: the render-queue fences
// (game/render_queue.cpp, ROADMAP M6), movie pacing (game/movie_pacing.cpp,
// v0.1.1, with its own state), the frame-time stats (game/game_time.cpp,
// RECARBON_GAME_TIME_LOG, measured only) and Vert+ (game/carbon_ultrawide.cpp,
// RECARBON_VERT_PLUS=1, which also logs the title hooks at startup). This
// file reports what movie_pacing.h defines without a hook for it:
//   - LastMovieFrameNs(): 0, "none yet" (the presenter's movie-quiet timing;
//     the movie pacing hook doesn't feed it, so that stays as in v0.1.0).
#include "movie_pacing.h"

namespace game
{
    int64_t LastMovieFrameNs()
    {
        return 0;
    }
}
