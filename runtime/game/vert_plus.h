// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Vert+'s policy for NFS: Carbon (game/carbon_ultrawide.cpp has the hooks
// and the evidence for each address). Header only and free of guest memory:
// tests/vert_plus_test.cpp checks it alone. Ported from SpeedBreaker's
// Carbon layer (game/carbon/ultrawide.cpp and game/front_end_matrix.h,
// GPL-3.0-or-later): the view ids it widens, the scaling of the hooks'
// registers, the movie aspect and the front-end matrix tracker.
// ReCarbon changes: on unless RECARBON_VERT_PLUS=0 (a developer switch;
// State(false) changes nothing, reports no hooks and asks for no
// guest-memory write; the player's switch is Settings > Display > Expanded
// View, the aspect_ratio setting: Off is the original 16:9 camera), and
// Vert+ only: HooksActive() stays false, so a screen wider than 16:9 is still
// pillarboxed (no Hor+ in this phase).
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>

namespace game::vert_plus
{
    // RECARBON_VERT_PLUS (`env`): off for exactly "0", on for anything else
    // (unset, empty, "1"). ReCarbon (Vert+ branch): on by default, the
    // user's decision; Expanded View Off keeps the original camera.
    inline bool Requested(const char* env)
    {
        return !(env && env[0] == '0' && env[1] == '\0');
    }

    // The views whose projection follows the screen's shape: EVIEW_PLAYER1
    // and 2 (ids 1, 2), their reflections (4, 5) and the shadow matte (21),
    // the ids the game's own 16:9 widening tests. The mirror (3,
    // EVIEW_PLAYER1_RVM), the crew's picture-in-picture (8), the shadow maps
    // and the environment maps keep theirs.
    inline bool WidenedViewId(uint32_t id)
    {
        return id == 1 || id == 2 || id == 4 || id == 5 || id == 21;
    }

    struct Scales
    {
        float x = 1.0f, y = 1.0f;
    };

    // The presenter's view scales as the hooks take them (x for Hor+, y for
    // Vert+, video/picture_fit's at most 4:3).
    inline Scales Clamp(float x, float y)
    {
        return { std::clamp(x, 0.1f, 1.0f), std::clamp(y, 0.5f, 1.0f) };
    }

    // A 0..1 screen edge narrowed about the screen's centre (the mirror's
    // and the picture-in-picture's rectangle).
    inline float AboutCentre(float v, float scale)
    {
        return 0.5f + (v - 0.5f) * scale;
    }

    // The display aspect the movie player fits a movie to: the aspect the
    // 16:9 frame is shown at, so a movie stays 16:9 on the screen
    // (letterboxed under Vert+, pillarboxed under Hor+).
    inline float MovieAspect(Scales s)
    {
        return (16.0f / 9.0f) * s.y / s.x;
    }

    // The front-end matrix scales the hook left last (its thread only): the
    // game's upload keeps the pointers of its last call and skips a call
    // that brings the same ones, and the front end's matrix is always on the
    // same stack slot, so a changed matrix must be sent again.
    class MatrixTracker
    {
    public:
        // Whether (x, y) differs from the last, or is the first.
        bool Changed(float x, float y)
        {
            const bool changed = !m_seen || x != m_x || y != m_y;
            m_seen = true;
            m_x = x;
            m_y = y;
            return changed;
        }

    private:
        bool m_seen = false;
        float m_x = 0.0f, m_y = 0.0f;
    };

    // The whole state, one instance (game/carbon_ultrawide.cpp). Scales:
    // any thread (the presenter sets them, the game's render thread reads
    // them). FrontEnd: the game's render thread only.
    class State
    {
    public:
        explicit State(bool on) : m_on(on) {}

        bool On() const { return m_on; }

        // The presenter's scales for the next frames. Returns whether they
        // changed (for the log). Off, they are still kept for the renderer
        // to read back, as before Vert+ (always 1, 1 there, as the presenter
        // only fills with the hooks active).
        bool SetScales(float x, float y)
        {
            const Scales next = Clamp(x, y);
            const Scales last = m_scales.exchange(next, std::memory_order_relaxed);
            return last.x != next.x || last.y != next.y;
        }
        Scales Get() const { return m_scales.load(std::memory_order_relaxed); }

        // Whether the movie player's display aspect is to be written: on only.
        bool WritesMovieAspect() const { return m_on; }

        // Hor+: not in this phase.
        bool HooksActive() const { return false; }
        // Vert+: on, once the projection hook has run.
        bool TallHooksActive() const { return m_seen.load(std::memory_order_relaxed); }

        // The projection hook, before the first store of P[0][0] (p00 = 1 /
        // tan of half the horizontal view) and of P[1][1] (p11 = 1 / tan of
        // half the vertical view, its magnitude: negated after the hook).
        // Returns whether this view's matrices were changed.
        bool Projection(uint32_t id, float& p00, float& p11)
        {
            if (!m_on)
                return false;
            if (!m_seen.load(std::memory_order_relaxed))
                m_seen.store(true, std::memory_order_relaxed);
            const Scales s = Get();
            if (!WidenedViewId(id) || (s.x == 1.0f && s.y == 1.0f))
                return false;
            p00 *= s.x;
            p11 *= s.y;
            return true;
        }

        // The front-end matrix hook (x, y its scales for the 16:9 frame).
        // Returns whether the matrix differs from the one left last, so the
        // game's cached pointer must be cleared.
        bool FrontEnd(float& x, float& y)
        {
            if (!m_on)
                return false;
            const Scales s = Get();
            x *= s.x;
            y *= s.y;
            m_frontEndX.store(x, std::memory_order_relaxed);
            m_frontEndY.store(y, std::memory_order_relaxed);
            return m_tracker.Changed(x, y);
        }
        // 0 before the first front-end frame (and always when off).
        float FrontEndX() const { return m_frontEndX.load(std::memory_order_relaxed); }
        float FrontEndY() const { return m_frontEndY.load(std::memory_order_relaxed); }

        // The mirror's and the picture-in-picture's rectangle: left edge,
        // top or bottom edge (0..1 of the screen), signed width, height.
        void Rectangle(float& left, float& top, float& width, float& height) const
        {
            if (!m_on)
                return;
            const Scales s = Get();
            if (s.x != 1.0f)
            {
                left = AboutCentre(left, s.x);
                width *= s.x;
            }
            if (s.y != 1.0f)
            {
                top = AboutCentre(top, s.y);
                height *= s.y;
            }
        }

    private:
        const bool m_on;
        std::atomic<Scales> m_scales{ Scales{} };
        std::atomic<bool> m_seen{ false };
        std::atomic<float> m_frontEndX{ 0.0f }, m_frontEndY{ 0.0f };
        MatrixTracker m_tracker;
    };
}
