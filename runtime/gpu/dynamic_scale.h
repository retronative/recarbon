// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// RECARBON_DYNAMIC_SCALE=<budget ms> (an experiment, off by default): the
// internal resolution follows the GPU's time. The Internal Resolution setting
// is the top rung; the renderer (gpu/renderer.cpp) steps down a rung when the
// GPU's per-frame time (an EMA of the command processor's 30-frame averages,
// video/frame_rate.h's cadence) stays over the budget, and back up when the
// time scaled by the next rung's pixels would fit with a margin. Each step is
// a live resolution change (ApplyRenderScale: every scaled target replaced,
// a visible hitch), so steps down come fast, steps up slowly, and never two
// within a dwell. The same shape as Frame Rate's Auto (video/frame_rate_policy.h);
// when both are on, Auto's 30 fps judges the costs the rung leaves, so the
// resolution gives first. Pure, for tests/dynamic_scale_test.cpp.
#pragma once
#include "render_scale.h"
#include <platform/thermal.h>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace gpu::dynscale
{
    constexpr float kEmaAlpha = 0.5f;        // per 30-frame sample (about 0.5 s at 60)
    constexpr int kDownSamples = 2;          // over the budget this many samples in a row: down
    constexpr int kUpSamples = 10;           // under it (scaled to the next rung) this many: up
    constexpr float kUpMargin = 0.85f;       // ... with the estimate under budget x this
    constexpr double kDownDwellSec = 5;      // no step down within this of the last change
    constexpr double kUpDwellSec = 20;       // no step up within this of the last change
    constexpr float kGameplayDraws = 1100;   // a step up needs a scene this busy (not a loading screen)

    // The rungs from `top` down to `floor`: the whole scales with the long
    // axis at most one step above the short one (3x3, 3x2, 2x2; 4x4, 4x3,
    // ...), from `top`'s pixels down to `floor`'s, most pixels first. A top
    // that is none of them (1.5x, or under the floor) stands alone.
    inline std::vector<RenderScale> Ladder(const RenderScale& top, const RenderScale& floor)
    {
        std::vector<RenderScale> rungs;
        const uint64_t most = top.HostPixels(1280, 720), least = floor.HostPixels(1280, 720);
        for (uint32_t sx = 4; sx >= 2; sx--)
            for (uint32_t sy = sx; sy + 1 >= sx && sy >= 2; sy--)
            {
                const RenderScale r = RenderScale::Whole(sx, sy);
                const uint64_t px = r.HostPixels(1280, 720);
                if (px <= most && px >= least)
                    rungs.push_back(r);
            }
        if (rungs.empty() || !(rungs.front() == top))
            rungs.insert(rungs.begin(), top);
        return rungs;
    }

    struct Sample
    {
        float gpuMs = 0;  // the GPU's time a frame over the last 30 frames
        float draws = 0;  // draws a frame
    };

    enum class Reason : uint8_t { None, OverBudget, Headroom, NewTop };

    struct Decision
    {
        bool changed = false;
        RenderScale scale;        // the rung after this update
        Reason reason = Reason::None;
        float ema = 0;            // the GPU time the update judged
        int rung = 0;             // 0: the top
    };

    class Policy
    {
    public:
        Policy(float budgetMs, const RenderScale& top, const RenderScale& floor)
            : m_budget(budgetMs), m_rungs(Ladder(top, floor))
        {
        }

        // The setting changed: a new ladder, from its top.
        void SetTop(const RenderScale& top, const RenderScale& floor)
        {
            m_rungs = Ladder(top, floor);
            m_rung = 0;
            m_over = m_under = 0;
            m_ema = 0;
            m_haveEma = false;
        }

        const RenderScale& Current() const { return m_rungs[size_t(m_rung)]; }
        int Changes() const { return m_changes; }
        float Ema() const { return m_ema; }
        size_t Rungs() const { return m_rungs.size(); }

        Decision Update(double seconds, platform::thermal::State thermal, const Sample& s)
        {
            m_ema = m_haveEma ? m_ema + kEmaAlpha * (s.gpuMs - m_ema) : s.gpuMs;
            m_haveEma = true;
            Decision d;
            d.scale = Current();
            d.rung = m_rung;
            d.ema = m_ema;
            const bool over = m_ema > m_budget;
            m_over = over ? m_over + 1 : 0;
            const bool canDown = m_rung + 1 < int(m_rungs.size()) && (m_changes == 0 || seconds - m_lastChange >= kDownDwellSec);
            if (over && m_over >= kDownSamples && canDown)
            {
                m_rung++;
                return Changed(d, seconds, Reason::OverBudget);
            }
            // Up: the next rung's estimate (this time scaled by its pixels,
            // which overstates fixed costs: the cautious side) under the
            // budget with a margin, in a busy scene, on a device that isn't hot.
            bool fits = false;
            if (m_rung > 0)
            {
                const RenderScale& next = m_rungs[size_t(m_rung - 1)];
                const double ratio = double(next.HostPixels(1280, 720)) / double(Current().HostPixels(1280, 720));
                fits = m_ema * ratio <= m_budget * kUpMargin;
            }
            const bool hot = thermal == platform::thermal::State::Serious || thermal == platform::thermal::State::Critical;
            m_under = fits && s.draws >= kGameplayDraws && !hot ? m_under + 1 : 0;
            if (m_rung > 0 && m_under >= kUpSamples && seconds - m_lastChange >= kUpDwellSec)
            {
                m_rung--;
                return Changed(d, seconds, Reason::Headroom);
            }
            return d;
        }

        std::string Describe() const
        {
            return std::format("rung {} of {} ({}), GPU {:.1f} ms against {:.0f}, {} changes", m_rung + 1, m_rungs.size(), ScaleName(Current()), m_ema,
                m_budget, m_changes);
        }

    private:
        Decision Changed(Decision& d, double seconds, Reason reason)
        {
            m_changes++;
            m_lastChange = seconds;
            d.changed = true;
            d.scale = Current();
            d.rung = m_rung;
            d.reason = reason;
            return d;
        }

        float m_budget;
        std::vector<RenderScale> m_rungs;
        int m_rung = 0;
        float m_ema = 0;
        bool m_haveEma = false;
        int m_over = 0, m_under = 0, m_changes = 0;
        double m_lastChange = 0;
    };
}
