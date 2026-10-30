// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/dynamic_scale.h: RECARBON_DYNAMIC_SCALE's policy (an experiment, off by default), fed fake
// 30-frame samples on a fake clock: the ladder of rungs, stepping down fast and up slowly, the
// dwells, the thermal and loading-screen guards, and a setting change. Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/dynamic_scale_test.cpp -o build/dynamic_scale_test && build/dynamic_scale_test
#include <gpu/dynamic_scale.h>

#include <cstdio>

namespace
{
    int g_failures = 0;

    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            printf("  FAIL: %s\n", what);
            g_failures++;
        }
    }

    using namespace gpu;
    using namespace gpu::dynscale;
    using platform::thermal::State;
    const RenderScale k3 = RenderScale::Whole(3, 3), k32 = RenderScale::Whole(3, 2), k2 = RenderScale::Whole(2, 2), k4 = RenderScale::Whole(4, 4);
    const RenderScale k15 = { { 3, 2 }, { 3, 2 } };

    std::string Names(const std::vector<RenderScale>& v)
    {
        std::string s;
        for (const RenderScale& r : v)
            s += (s.empty() ? "" : " ") + ScaleName(r);
        return s;
    }

    // `n` samples at `gpuMs`, 0.5 s apart from `t`, in a busy scene; the last decision.
    Decision Feed(Policy& p, double& t, int n, float gpuMs, State thermal = State::Nominal, float draws = 2400)
    {
        Decision d;
        for (int i = 0; i < n; i++)
        {
            t += 0.5;
            d = p.Update(t, thermal, { gpuMs, draws });
            if (d.changed)
                return d;
        }
        return d;
    }
}

int main()
{
    printf("---- the ladder ----\n");
    Check(Names(Ladder(k3, k2)) == "3x3 3x2 2x2", "3x down to 2x: 3x3, 3x2, 2x2");
    Check(Names(Ladder(k4, k2)) == "4x4 4x3 3x3 3x2 2x2", "4x down to 2x");
    Check(Names(Ladder(k2, k2)) == "2x2", "2x is its own floor");
    Check(Names(Ladder(k15, k2)) == "1.5x1.5", "1.5x stands alone (under the floor, not a whole rung)");
    Check(Names(Ladder(k32, k2)) == "3x2 2x2", "a 3x2 top");
    Check(Names(Ladder(k3, k3)) == "3x3", "a floor at the top: one rung");
    Check(Names(Ladder(RenderScale::Whole(2, 3), k2)) == "2x3 3x2 2x2", "a top that isn't a rung (2x3) heads its ladder");

    printf("---- holding ----\n");
    {
        Policy p(14, k3, k2);
        double t = 0;
        Check(!Feed(p, t, 40, 13.0f).changed && p.Current() == k3 && p.Changes() == 0, "under the budget: 3x3 stays, 20 s");
        Check(p.Ema() > 12.9f && p.Ema() < 13.1f, "the EMA settles on the samples");
    }

    {
        Policy p(14, k3, k2);
        double t = 0;
        Feed(p, t, 10, 13.0f);
        Decision d = Feed(p, t, 2, 15.0f);
        Check(!d.changed && p.Ema() > 14.4f && p.Ema() < 14.6f, "two samples of 15 after 13: the EMA (14, then 14.5) is over once: not yet");
        d = Feed(p, t, 1, 15.0f);
        Check(d.changed, "the third (EMA 14.75): the second over in a row: down");
    }
    {
        Policy p(14, k3, k2);
        double t = 0;
        Feed(p, t, 10, 13.0f);
        Decision d = Feed(p, t, 1, 20.0f);  // EMA 16.5: over once
        d = Feed(p, t, 5, 10.0f);           // the EMA falls under: the count resets
        Check(!d.changed && p.Ema() < 14.0f, "(under again)");
        d = Feed(p, t, 1, 20.0f);           // over once more: still one in a row
        Check(!d.changed, "an over-budget sample after a lull doesn't add to the earlier one");
    }

    printf("---- stepping down ----\n");
    {
        Policy p(14, k3, k2);
        double t = 0;
        Feed(p, t, 10, 13.0f);
        t += 0.5;
        Decision d = p.Update(t, State::Nominal, { 20.0f, 2400 });
        Check(!d.changed, "one sample over the budget: not yet (the EMA 16.5)");
        t += 0.5;
        d = p.Update(t, State::Nominal, { 20.0f, 2400 });
        Check(d.changed && d.scale == k32 && d.reason == Reason::OverBudget && d.rung == 1, "the second: down to 3x2");
        const double down1 = t;
        d = Feed(p, t, 9, 20.0f);  // still over: 4.5 s
        Check(!d.changed, "no second step within the 5 s dwell");
        d = Feed(p, t, 2, 20.0f);
        Check(d.changed && d.scale == k2 && t - down1 >= kDownDwellSec, "past the dwell, still over: down to 2x2");
        d = Feed(p, t, 20, 20.0f);
        Check(!d.changed && p.Current() == k2 && p.Changes() == 2, "at the floor: nowhere lower");
    }

    printf("---- stepping up ----\n");
    {
        Policy p(14, k3, k2);
        double t = 0;
        Feed(p, t, 2, 20.0f);  // down to 3x2 at t = 1
        Check(p.Current() == k32, "(down to 3x2)");
        // At 3x2, 8 ms: scaled to 3x3's pixels (x1.5) 12 ms, under 14 x 0.85 = 11.9? No: 12 > 11.9.
        Decision d = Feed(p, t, 60, 8.0f);
        Check(!d.changed, "8 ms at 3x2 estimates 12 ms at 3x3, over the margin (11.9): no step up");
        d = Feed(p, t, 60, 7.5f);  // 11.25 ms: fits
        Check(d.changed && d.scale == k3 && d.reason == Reason::Headroom && d.rung == 0, "7.5 ms at 3x2 (11.25 at 3x3): up to 3x3");
    }
    {
        Policy p(14, k3, k2);
        double t = 0;
        Feed(p, t, 2, 20.0f);
        const double down = t;
        Decision d = Feed(p, t, 30, 6.0f);  // 15 s of headroom
        Check(!d.changed && t - down < kUpDwellSec, "headroom within the 20 s dwell: no step up");
        d = Feed(p, t, 12, 6.0f);
        Check(d.changed && d.scale == k3 && t - down >= kUpDwellSec, "then up after kUpSamples past the dwell");
    }
    {
        Policy p(14, k3, k2);
        double t = 0;
        Feed(p, t, 2, 20.0f);
        Decision d = Feed(p, t, 60, 6.0f, State::Serious);
        Check(!d.changed, "a hot device never steps up");
        d = Feed(p, t, 60, 6.0f, State::Nominal, 300);
        Check(!d.changed, "a loading screen (few draws) never steps up");
        d = Feed(p, t, 9, 6.0f);
        Check(!d.changed, "nine busy samples under: not yet");
        d = Feed(p, t, 1, 6.0f);
        Check(d.changed, "the tenth: up");
        d = Feed(p, t, 9, 20.0f, State::Serious);
        Check(!d.changed, "over the budget right after the step up: the 5 s dwell first");
        d = Feed(p, t, 2, 20.0f, State::Serious);
        Check(d.changed && d.scale == k32, "then a hot device still steps down");
    }

    printf("---- a setting change ----\n");
    {
        Policy p(14, k3, k2);
        double t = 0;
        Feed(p, t, 2, 20.0f);
        Check(p.Current() == k32 && p.Changes() == 1, "(at 3x2)");
        p.SetTop(k4, k2);
        Check(p.Current() == k4 && p.Rungs() == 5, "a new top: its ladder, from the top");
        Decision d = p.Update(t + 0.5, State::Nominal, { 20.0f, 2400 });
        Check(!d.changed && d.ema == 20.0f, "the EMA starts again from the next sample");
        Check(p.Describe().find("rung 1 of 5 (4x4)") == 0, "the description");
    }

    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
