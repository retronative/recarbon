// ReCarbon runtime (based on SpeedBreaker). GPL-3.0-or-later (see COPYING).
//
// NFS: Carbon's title layer. SpeedBreaker's game/ held MW's hooks: mid-asm
// hooks and function overrides at MW addresses (render-queue fences,
// Hor+/Vert+ view scaling, sun glare, nitrous trails, frame pacing, movie
// pacing) and stores into MW's data (the movie aspect at 0x8200FAC0). Those
// addresses mean nothing in Carbon's image, so none of that code is built
// here (reference/ARCHITECTURE.md). Carbon's own so far: the render-queue
// fences (game/render_queue.cpp, ROADMAP M6). Until its view, frame-time and
// movie equivalents are found this file reports exactly what the interface
// headers define for a ppc/ without those hooks:
//   - HooksActive()/TallHooksActive(): false, so the presenter shows the
//     16:9 frame letterboxed or pillarboxed instead of stretching it
//     (video/picture_fit.h), and the renderer's front-end detection, which
//     needs a view scale below 1, never runs.
//   - FrontEndScaleX/Y(): 0, "before the first front-end frame".
//   - LastMovieFrameNs(): 0, "none yet": no movie pacing.
//   - GetFrameTimeStats(): zeros, as before the first game frame.
// LogTitleHooks() says so in the log at startup.
#include "frame_time.h"
#include "movie_pacing.h"
#include "title_hooks.h"
#include "ultrawide.h"

#include <algorithm>
#include <atomic>
#include <cstdio>

namespace
{
    struct Scales
    {
        float x, y;
    };
    std::atomic<Scales> s_scales{ Scales{ 1.0f, 1.0f } };
}

namespace game
{
    void LogTitleHooks()
    {
        fprintf(stderr, "[title] NFS: Carbon: no title hooks yet (view scaling, movie pacing and frame-time stats "
            "are inactive; the frame is shown at 16:9)\n");
    }

    void SetAspectScale(float scaleX, float scaleY)
    {
        // Kept for the renderer to read back; no hook consumes it, and
        // nothing is written into guest memory.
        s_scales.store(Scales{ std::clamp(scaleX, 0.1f, 1.0f), std::clamp(scaleY, 0.5f, 1.0f) }, std::memory_order_relaxed);
    }

    float AspectScaleX()
    {
        return s_scales.load(std::memory_order_relaxed).x;
    }

    float AspectScaleY()
    {
        return s_scales.load(std::memory_order_relaxed).y;
    }

    bool HooksActive()
    {
        return false;
    }

    bool TallHooksActive()
    {
        return false;
    }

    float FrontEndScaleX()
    {
        return 0.0f;
    }

    float FrontEndScaleY()
    {
        return 0.0f;
    }

    FrameTimeStats GetFrameTimeStats()
    {
        return {};
    }

    int64_t LastMovieFrameNs()
    {
        return 0;
    }
}
