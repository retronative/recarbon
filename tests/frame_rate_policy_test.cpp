// From SpeedBreaker v0.1.8's tests/frame_rate_policy_test.cpp
// (GPL-3.0-or-later). ReCarbon changes: the policy's rules are tested as
// they are, but with Carbon's numbers where the cases stood for the game:
// gameplay at 2250 draws a frame (upstream 4000, MW's iPhone races), the
// median of hand test 4's two long race stretches (2255 and 2297,
// game/m3/hand04.log, 2026-10-23), and its 60 fps front end at 55 draws
// (Carbon's logs: median 53-55 draws, CP 0.2 ms; its 30 fps menus likewise).
// Upstream's Steam Frame headset case is left out (not a ReCarbon platform),
// and so is NFSMW_THERMAL_SIM (ReCarbon has none). Added: WindowIsRecent
// and NoEvidenceAcrossSwitch; Mac3xRace and ThrottledWhileFair (the Frame
// Rate options' audit: what Auto does on the M5 at 3x and on the M1, where
// macOS never reports thermal serious). Frame Rate is 60 by
// default here; Auto is the player's choice. The thresholds themselves
// (frame_rate_policy.h) are SpeedBreaker's, measured on iPhones and iPads,
// and not retuned for Carbon on a Mac.
//
// Tests for video::AutoFrameRatePolicy (runtime/video/frame_rate_policy.cpp),
// Frame Rate's Auto. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/frame_rate_policy_test.cpp runtime/video/frame_rate_policy.cpp -o build/frame_rate_policy_test
//   build/frame_rate_policy_test [log[@<sec>:<state>,...] ...]
// Without arguments: the synthetic cases. With logs: also replays each one's
// [perf] lines (120 frames each, fed as four 30-frame samples as the command
// processor does) and prints the switches Auto would have made, with an
// optional thermal schedule (<sec>:<state>). At 30 the replay has no
// frames of its own to read: it uses the log's costs as a stand-in. A line
// spanning a race's end and a menu smears both into its four samples (the
// command processor's 30-frame ones would mostly be one or the other).
#include <video/frame_rate_policy.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

using platform::thermal::State;
using video::AutoFrameRatePolicy;
using video::FrameRateDecision;
using video::FrameRateReason;
using video::FrameSample;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

namespace
{
    struct Switch
    {
        double at;
        int rate;
        FrameRateReason reason;
        int backoff;
    };

    // Drives a policy with 30-frame samples every 30/fps seconds of both
    // clocks (the host's and the game's run together here), of `draws` a
    // frame (gameplay unless set lower: ReCarbon, Carbon's race median).
    struct Driver
    {
        AutoFrameRatePolicy policy;
        double t = 0;
        std::vector<Switch> switches;
        float draws = 2250;

        // `fps60`/`cp`/`gpu` while at 60; at 30 the frames come at 30 with
        // `cp30`/`gpu30` (negative: the same as at 60).
        void Run(double seconds, State thermal, double fps60, float cp, float gpu, float cp30 = -1, float gpu30 = -1)
        {
            const double end = t + seconds;
            while (t < end)
            {
                const bool at30 = policy.Rate() == 30;
                const double fps = at30 ? std::min(fps60, 30.0) : fps60;
                const double dt = 30.0 / fps;
                t += dt;
                FrameSample s{ dt, 30, at30 && cp30 >= 0 ? cp30 : cp, at30 && gpu30 >= 0 ? gpu30 : gpu, draws };
                FrameRateDecision d = policy.Update(t, thermal, s);
                if (d.changed)
                    switches.push_back({ t, d.rate, d.reason, d.backoff });
            }
        }
    };

    const char* ReasonName(FrameRateReason r)
    {
        switch (r)
        {
        case FrameRateReason::Hot: return "hot";
        case FrameRateReason::CantHold60: return "can't hold 60";
        case FrameRateReason::Headroom: return "headroom";
        case FrameRateReason::Try: return "try";
        default: return "-";
        }
    }

    void HotDropsAtOnce()
    {
        Driver d;
        d.Run(0.5, State::Serious, 60, 2, 8);
        CHECK(d.switches.size() == 1 && d.switches[0].rate == 30 && d.switches[0].reason == FrameRateReason::Hot,
            "%zu switches", d.switches.size());
        Driver c;
        c.Run(0.5, State::Critical, 60, 2, 8);
        CHECK(c.switches.size() == 1 && c.switches[0].reason == FrameRateReason::Hot, "critical: %zu switches", c.switches.size());
        // Fair, nominal and no reading at all don't.
        for (State s : { State::Fair, State::Nominal, State::Unknown })
        {
            Driver n;
            n.Run(300, s, 60, 2, 8);
            CHECK(n.switches.empty(), "thermal %d: %zu switches", int(s), n.switches.size());
        }
    }

    void CantHold60()
    {
        // 50 fps at a full frame's cost: once the window holds 10 s.
        Driver d;
        d.Run(30, State::Nominal, 50, 12, 18);
        CHECK(d.switches.size() == 1 && d.switches[0].reason == FrameRateReason::CantHold60, "%zu switches", d.switches.size());
        if (!d.switches.empty())
            CHECK(d.switches[0].at >= 10 && d.switches[0].at < 11, "dropped at %.1f s, want after the first 10 s", d.switches[0].at);
        // 55 fps (above kDropFps) stays, and so does 50 fps that costs little
        // (something else holds it back, and 30 wouldn't help).
        Driver above;
        above.Run(120, State::Nominal, 55, 12, 18);
        CHECK(above.switches.empty(), "55 fps: %zu switches", above.switches.size());
        Driver cheap;
        cheap.Run(120, State::Nominal, 50, 8, 11);
        CHECK(cheap.switches.empty(), "50 fps at 11 ms: %zu switches", cheap.switches.size());
    }

    void Bursts()
    {
        // A race that holds 60 (GPU 14 ms) with a 5 s stretch at 47 fps (GPU
        // 18) every 40 s: the mean of the 10 s around it is under 54, but
        // most of it ran at 60.
        Driver d;
        for (int i = 0; i < 20; i++)
        {
            d.Run(35, State::Nominal, 60, 9, 14);
            d.Run(5, State::Nominal, 47, 12, 18);
        }
        CHECK(d.switches.empty(), "bursts: %zu switches", d.switches.size());
        // A menu at 60 whose GPU busy reads 16.7 ms (the queue's occupancy),
        // then a load: slow frames, but cheap ones.
        Driver m;
        for (int i = 0; i < 10; i++)
        {
            m.Run(8, State::Nominal, 60, 0.1f, 16.7f);
            m.Run(6.5, State::Nominal, 18, 0.1f, 8);
        }
        CHECK(m.switches.empty(), "menu, then a load: %zu switches", m.switches.size());
    }

    void GameThirtyAndHitches()
    {
        // The game's own 30 fps menus and movies: CP well under 1 ms, GPU 3-6.
        Driver menus;
        menus.Run(600, State::Nominal, 30, 0.3f, 5);
        CHECK(menus.switches.empty(), "menus: %zu switches", menus.switches.size());
        // Loading: 30 frames over 2.5 s (800 ms hitches) at little cost.
        AutoFrameRatePolicy p;
        double t = 0;
        int changes = 0;
        for (int i = 0; i < 100; i++)
        {
            t += 2.5;
            changes += p.Update(t, State::Nominal, { 2.5, 30, 0.2f, 4 }).changed;
        }
        CHECK(changes == 0, "loading: %d switches", changes);
    }

    void Dwell()
    {
        // Thermal flipping every 5 s between critical and nominal never
        // cools down long enough to come back.
        Driver flip;
        for (int i = 0; i < 200; i++)
            flip.Run(5, i % 2 ? State::Nominal : State::Critical, 60, 2, 8);
        CHECK(flip.switches.size() == 1, "flipping: %zu switches", flip.switches.size());
        // Hot whenever it's at 60, cool whenever it's at 30, cheap frames: it
        // switches as often as it may. Each drop waits out the dwell.
        Driver d;
        for (int i = 0; i < 20000; i++)
            d.Run(0.5, d.policy.Rate() == 60 ? State::Critical : State::Nominal, 60, 2, 8);
        CHECK(d.switches.size() >= 6, "only %zu switches", d.switches.size());
        bool dwellBound = false;
        for (size_t i = 1; i < d.switches.size(); i++)
        {
            double gap = d.switches[i].at - d.switches[i - 1].at;
            CHECK(gap >= AutoFrameRatePolicy::kDwellSec - 1e-9, "switches at %.1f and %.1f s", d.switches[i - 1].at, d.switches[i].at);
            dwellBound |= d.switches[i].rate == 30 && gap < AutoFrameRatePolicy::kDwellSec + 1;
        }
        CHECK(dwellBound, "no drop waited out the dwell");
    }

    void Cooldown()
    {
        // Serious for 10 s, then fair: 30 at once, 60 again 60 s after the
        // last hot sample (not before), by headroom (cheap frames at 30).
        Driver d;
        d.Run(10, State::Serious, 60, 4, 10);
        d.Run(100, State::Fair, 60, 4, 10);
        CHECK(d.switches.size() == 2, "%zu switches", d.switches.size());
        if (d.switches.size() == 2)
        {
            CHECK(d.switches[1].rate == 60 && d.switches[1].reason == FrameRateReason::Headroom, "rise %s",
                ReasonName(d.switches[1].reason));
            CHECK(d.switches[1].at >= 10 + AutoFrameRatePolicy::kCoolSec - 1 && d.switches[1].at < 10 + AutoFrameRatePolicy::kCoolSec + 2,
                "rose at %.1f s", d.switches[1].at);
        }
        // A return to serious restarts that clock: fair 10-50 s, serious
        // again until 60 s: back to 60 no sooner than 120 s.
        Driver r;
        r.Run(10, State::Serious, 60, 4, 10);
        r.Run(40, State::Fair, 60, 4, 10);
        r.Run(10, State::Serious, 60, 4, 10);
        r.Run(100, State::Nominal, 60, 4, 10);
        CHECK(r.switches.size() == 2, "%zu switches", r.switches.size());
        if (r.switches.size() == 2)
            CHECK(r.switches[1].at >= 60 + AutoFrameRatePolicy::kCoolSec - 1, "rose at %.1f s", r.switches[1].at);
        // Hot throughout: never back.
        Driver h;
        h.Run(1000, State::Serious, 60, 4, 10);
        CHECK(h.switches.size() == 1, "hot: %zu switches", h.switches.size());
    }

    void TryAndBackoff()
    {
        // Can't hold 60, and at 30 the GPU still reads above the rise's 13
        // ms (CP fine): no headroom, so a try after kTrySec at 30. It fails
        // (the scene still can't hold 60) and drops again after the dwell,
        // soon after the rise: the next cooldown and try wait twice as long.
        Driver d;
        d.Run(2000, State::Nominal, 50, 10, 18, 10, 16);
        CHECK(d.switches.size() >= 5, "%zu switches", d.switches.size());
        if (d.switches.size() >= 5)
        {
            CHECK(d.switches[0].rate == 30 && d.switches[0].reason == FrameRateReason::CantHold60, "first %s",
                ReasonName(d.switches[0].reason));
            double firstTry = d.switches[1].at - d.switches[0].at;
            CHECK(d.switches[1].reason == FrameRateReason::Try && firstTry >= AutoFrameRatePolicy::kTrySec - 1 &&
                    firstTry < AutoFrameRatePolicy::kTrySec + 2, "first try after %.1f s (%s)", firstTry, ReasonName(d.switches[1].reason));
            double redrop = d.switches[2].at - d.switches[1].at;
            CHECK(redrop >= AutoFrameRatePolicy::kDwellSec && redrop < AutoFrameRatePolicy::kDwellSec + 2 && d.switches[2].backoff == 1,
                "dropped %.1f s after the try, back-off %d", redrop, d.switches[2].backoff);
            double secondTry = d.switches[3].at - d.switches[2].at;
            CHECK(secondTry >= 2 * AutoFrameRatePolicy::kTrySec - 1 && secondTry < 2 * AutoFrameRatePolicy::kTrySec + 2,
                "second try after %.1f s", secondTry);
            for (const Switch& s : d.switches)
                CHECK(s.backoff <= AutoFrameRatePolicy::kBackoffMax, "back-off %d", s.backoff);
        }
        // CP over the rise's 12 ms at 30: no try either.
        Driver cp;
        cp.Run(2000, State::Nominal, 45, 14, 18, 14, 18);
        CHECK(cp.switches.size() == 1, "CP-bound: %zu switches", cp.switches.size());
    }

    void BackoffResets()
    {
        // A failed return (k = 1), then a scene that holds 60 for 10 minutes:
        // the back-off is forgotten, so the next drop's cooldown is 60 s.
        Driver d;
        d.Run(75, State::Nominal, 50, 10, 18, 4, 10);   // drop at 10 s, rise by headroom at 70 s
        d.Run(40, State::Nominal, 50, 10, 18, 4, 10);   // drop again 30 s later: k = 1
        CHECK(d.switches.size() == 3 && d.switches[2].backoff == 1, "%zu switches, back-off %d", d.switches.size(),
            d.switches.empty() ? -1 : d.switches.back().backoff);
        d.Run(200, State::Nominal, 60, 4, 10);          // k = 1: back to 60 after 120 s cool
        size_t n = d.switches.size();
        CHECK(n == 4 && d.switches[3].rate == 60 && d.switches[3].at - d.switches[2].at >= 2 * AutoFrameRatePolicy::kCoolSec - 1,
            "%zu switches; rose %.1f s after", n, n >= 4 ? d.switches[3].at - d.switches[2].at : 0.0);
        d.Run(AutoFrameRatePolicy::kSettleSec + 10, State::Nominal, 60, 4, 10);
        d.Run(10, State::Serious, 60, 4, 10);
        d.Run(100, State::Fair, 60, 4, 10);
        CHECK(d.switches.size() == 6 && d.switches[4].backoff == 0 && d.switches[5].at - d.switches[4].at < 10 + AutoFrameRatePolicy::kCoolSec + 2,
            "%zu switches; back-off %d", d.switches.size(), d.switches.size() > 4 ? d.switches[4].backoff : -1);
    }

    void MenusAreNoEvidence()
    {
        // Races that can't hold 60 (48 fps; GPU busy 19 ms at 60, 16 at 30:
        // no headroom, but a try may come) between cheap menus at 60 (55
        // draws, CP 0.2 ms) and loads (8 draws, 30 fps): a menu or a load
        // at 30 doesn't read as room for 60, so 60 comes back only by tries,
        // each one after twice the last's wait.
        Driver d;
        for (int i = 0; i < 18; i++)  // an hour
        {
            d.draws = 2250;
            d.Run(150, State::Nominal, 48, 11, 19, 11, 16);
            d.draws = 55;
            d.Run(25, State::Nominal, 60, 0.2f, 5);
            d.draws = 8;
            d.Run(15, State::Nominal, 30, 0.1f, 4);
        }
        int headroom = 0;
        for (const Switch& s : d.switches)
            headroom += s.reason == FrameRateReason::Headroom;
        CHECK(headroom == 0 && d.switches.size() <= 9, "%zu switches, %d by headroom", d.switches.size(), headroom);
        // Gameplay at 30 that has room for 60 still brings it back by
        // headroom: 5 s of it, menus (no evidence: no rise once cool), then
        // the 5 s more that fill the window.
        Driver r;
        r.Run(5, State::Serious, 60, 4, 10);
        r.draws = 55;
        r.Run(100, State::Fair, 60, 0.2f, 5);
        CHECK(r.switches.size() == 1, "menus: %zu switches", r.switches.size());
        r.draws = 2250;
        r.Run(15, State::Fair, 60, 4, 10);
        CHECK(r.switches.size() == 2 && r.switches[1].reason == FrameRateReason::Headroom, "%zu switches", r.switches.size());
    }

    void ReheatBacksOff()
    {
        // A device that heats up again H s after each return to 60 (46 fps,
        // CP 14, GPU 20 while hot) and cools 40 s after each drop (CP 6, GPU
        // 12 at 30): a re-drop within kSettleSec of a return backs off, so
        // a 2-4 minute cycle settles near the back-off's floor (13 and 12
        // switches an hour; it was 28 and 20 when only drops within 120 s
        // backed off), as a fast one does (15-17: the back-off's climb).
        for (double H : { 45.0, 90.0, 150.0, 240.0 })
        {
            Driver d;
            double since60 = 0, since30 = 0;
            while (d.t < 3600)
            {
                const bool at30 = d.policy.Rate() == 30;
                const State th = at30 ? (d.t - since30 >= 40 ? State::Fair : State::Serious)
                                      : (d.t - since60 >= H ? State::Serious : State::Fair);
                const size_t before = d.switches.size();
                if (th == State::Serious && !at30)
                    d.Run(0.5, th, 46, 14, 20);
                else
                    d.Run(0.5, th, 60, 9, 13, 6, 12);
                if (d.switches.size() > before)
                    (d.policy.Rate() == 30 ? since30 : since60) = d.switches.back().at;
            }
            CHECK(d.switches.size() <= (H < 120 ? 17u : 13u), "re-heat after %.0f s: %zu switches an hour", H, d.switches.size());
        }
    }

    // ReCarbon: two rules upstream's cases didn't pin (each planted wrong,
    // every case above still passed).
    void WindowIsRecent()
    {
        // Five minutes that hold 60, then a scene that can't: judged on the
        // last kWindowSec, so it drops once the slow frames are most of those
        // (about 6 s in), not once they are most of all the time since the
        // last switch.
        Driver d;
        d.Run(300, State::Nominal, 60, 4, 10);
        d.Run(60, State::Nominal, 50, 12, 18);
        CHECK(d.switches.size() == 1 && d.switches[0].at >= 305 && d.switches[0].at < 311, "%zu switches, the first at %.1f s",
            d.switches.size(), d.switches.empty() ? 0.0 : d.switches[0].at);
    }

    void NoEvidenceAcrossSwitch()
    {
        // A cheap race at 60 fills the window; then a menu while the device
        // turns hot (dropped at once), and only menus at 30 once it's fair:
        // the race's frames at 60 aren't evidence of room for 60 at 30, so
        // nothing brings it back.
        Driver d;
        d.Run(15, State::Nominal, 60, 4, 10);
        d.draws = 55;
        d.Run(1, State::Serious, 30, 0.2f, 5);
        d.Run(300, State::Fair, 30, 0.2f, 5);
        CHECK(d.switches.size() == 1 && d.switches[0].rate == 30, "%zu switches", d.switches.size());
    }

    // ReCarbon (Frame Rate options): what Auto does on a MacBook Air M5 at
    // 3x, the resolution the 30 fps mode is for. The race is GPU-bound at
    // 45-50 fps (GPU 22-25 ms a frame, CP 8: measured on a MacBook Air M5, October 2026),
    // macOS reports thermal "fair" throughout (never serious, so Hot never
    // fires), and at 30 the GPU still reads over the rise's 13 ms. So: a
    // drop by CantHold60 once the window holds 10 s; 60 comes back only by
    // tries, each a 60 the scene can't hold for the window plus the dwell
    // (30-45 s of shaky 60) before the next drop; the waits double up to
    // 8x (180, 360, 720, 1440, 1440 s) and never two switches within the
    // dwell. The inherited (iOS-measured) numbers, pinned as they stand.
    void Mac3xRace()
    {
        Driver d;
        d.Run(5000, State::Fair, 48, 8, 24, 8, 24);
        CHECK(d.switches.size() >= 10, "%zu switches", d.switches.size());
        if (d.switches.size() < 10)
            return;
        CHECK(d.switches[0].rate == 30 && d.switches[0].reason == FrameRateReason::CantHold60 && d.switches[0].at >= 10 &&
                d.switches[0].at < 11, "first %s at %.1f s", ReasonName(d.switches[0].reason), d.switches[0].at);
        const double tryGap[] = { 180, 360, 720, 1440, 1440 };
        for (size_t i = 1; i < 10; i++)
        {
            const Switch& s = d.switches[i];
            const double gap = s.at - d.switches[i - 1].at;
            CHECK(gap >= AutoFrameRatePolicy::kDwellSec - 1e-9, "switches at %.1f and %.1f s", d.switches[i - 1].at, s.at);
            if (s.rate == 60)
            {
                CHECK(s.reason == FrameRateReason::Try, "rise %zu by %s", i, ReasonName(s.reason));
                const double want = tryGap[(i - 1) / 2];
                CHECK(gap >= want - 1 && gap < want + 2, "try %zu after %.1f s at 30, want %.0f", (i + 1) / 2, gap, want);
            }
            else
            {
                CHECK(s.reason == FrameRateReason::CantHold60, "drop %zu by %s", i, ReasonName(s.reason));
                CHECK(gap >= AutoFrameRatePolicy::kDwellSec && gap < 45, "the try lasted %.1f s", gap);
                CHECK(s.backoff == std::min(int((i + 1) / 2), AutoFrameRatePolicy::kBackoffMax), "drop %zu: back-off %d", i, s.backoff);
            }
        }
    }

    // ReCarbon: a MacBook Air M1's long race (docs/performance.md): 30 fps
    // at first, then slower as the GPU throttles, while macOS says "fair"
    // the whole time. The drop comes from the frames, not the thermal
    // state, within the window of the slowdown; and 30 stays while the
    // GPU reads over the rise's 13 ms.
    void ThrottledWhileFair()
    {
        Driver d;
        d.Run(120, State::Fair, 60, 6, 13);
        CHECK(d.switches.empty(), "60 fps at thermal fair: %zu switches", d.switches.size());
        d.Run(60, State::Fair, 40, 8, 25, 8, 25);
        CHECK(d.switches.size() == 1 && d.switches[0].reason == FrameRateReason::CantHold60 && d.switches[0].at >= 125 &&
                d.switches[0].at < 131, "%zu switches, the first at %.1f s", d.switches.size(), d.switches.empty() ? 0.0 : d.switches[0].at);
        d.Run(100, State::Fair, 40, 8, 25, 8, 25);
        CHECK(d.switches.size() == 1, "%zu switches: 60 came back with the GPU at 25 ms", d.switches.size());
    }

    // --- Replays ---

    State StateNamed(const std::string& name)
    {
        if (name == "nominal") return State::Nominal;
        if (name == "fair") return State::Fair;
        if (name == "serious") return State::Serious;
        if (name == "critical") return State::Critical;
        return State::Unknown;
    }

    void Replay(const std::string& arg)
    {
        std::string path = arg;
        std::vector<std::pair<double, State>> schedule;
        if (size_t at = arg.find('@'); at != std::string::npos)
        {
            path = arg.substr(0, at);
            std::string rest = arg.substr(at + 1);
            for (size_t pos = 0; pos <= rest.size();)
            {
                size_t comma = rest.find(',', pos);
                std::string item = rest.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                if (size_t colon = item.find(':'); colon != std::string::npos)
                    schedule.push_back({ std::atof(item.c_str()), StateNamed(item.substr(colon + 1)) });
                if (comma == std::string::npos)
                    break;
                pos = comma + 1;
            }
        }
        std::ifstream in(path);
        if (!in)
        {
            printf("== %s: can't read\n", path.c_str());
            g_failures++;
            return;
        }
        static const std::regex stamp(R"(^\[\s*([0-9.]+)\] \[perf\])"), fpsRe(R"(\[perf\] ([0-9.]+) fps)"),
            cpRe(R"(CP busy ([0-9.]+) ms/frame)"), gpuRe(R"(GPU busy ([0-9.]+) ms \(executing ([0-9.]+))"),
            drawsRe(R"(per frame: ([0-9.]+) draws)");
        AutoFrameRatePolicy policy;
        std::string line;
        double t = 0, last = 0;
        int lines = 0;
        printf("== %s%s%s\n", path.c_str(), schedule.empty() ? "" : " @", schedule.empty() ? "" : arg.substr(arg.find('@') + 1).c_str());
        while (std::getline(in, line))
        {
            std::smatch m;
            if (line.find("[perf]") == std::string::npos || !std::regex_search(line, m, fpsRe))
                continue;
            double fps = std::atof(m[1].str().c_str());
            if (fps <= 0)
                continue;
            float cp = std::regex_search(line, m, cpRe) ? float(std::atof(m[1].str().c_str())) : 0.0f;
            float gpu = 0;
            if (std::regex_search(line, m, gpuRe))
                gpu = std::atof(m[2].str().c_str()) > 0 ? float(std::atof(m[2].str().c_str())) : float(std::atof(m[1].str().c_str()));
            float draws = std::regex_search(line, m, drawsRe) ? float(std::atof(m[1].str().c_str())) : 0.0f;
            double end = std::regex_search(line, m, stamp) ? std::atof(m[1].str().c_str()) : last + 120.0 / fps;
            lines++;
            for (int k = 0; k < 4; k++)
            {
                t = last + (end - last) * (k + 1) / 4.0;
                State thermal = State::Unknown;
                for (auto& [when, state] : schedule)
                    if (t >= when)
                        thermal = state;
                const bool at30 = policy.Rate() == 30;
                FrameSample s{ 30.0 / (at30 ? 30.0 : fps), 30, cp, gpu, draws };
                FrameRateDecision d = policy.Update(t, thermal, s);
                if (d.changed)
                    printf("   %7.1f s  %d -> %d  %-13s %5.1f fps (median %5.1f) over %4.1f s, CP %4.1f, GPU %4.1f ms (slow ones %4.1f, %4.1f)%s\n",
                        t, 90 - d.rate, d.rate, ReasonName(d.reason), d.fps, d.medianFps, d.windowSeconds, d.cpMs, d.gpuMs, d.slowCpMs,
                        d.slowGpuMs, d.backoff ? (" back-off " + std::to_string(d.backoff)).c_str() : "");
            }
            last = end;
        }
        printf("   (%d [perf] lines, %.0f s)\n", lines, t);
    }
}

int main(int argc, char** argv)
{
    HotDropsAtOnce();
    CantHold60();
    Bursts();
    GameThirtyAndHitches();
    Dwell();
    Cooldown();
    TryAndBackoff();
    BackoffResets();
    MenusAreNoEvidence();
    ReheatBacksOff();
    WindowIsRecent();
    NoEvidenceAcrossSwitch();
    Mac3xRace();
    ThrottledWhileFair();
    for (int i = 1; i < argc; i++)
        Replay(argv[i]);
    printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "ok", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
