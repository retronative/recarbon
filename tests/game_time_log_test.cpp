// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// game/game_time_log.h: RECARBON_GAME_TIME_LOG's CSV line per game frame and its 10 s
// [gametime] summary of the world's steps (off by default), on a fake clock. Header only.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/game_time_log_test.cpp -o build/game_time_log_test && build/game_time_log_test
#include <game/game_time_log.h>

#include <cstdio>
#include <cstring>
#include <string>

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

    using namespace game::timelog;
    constexpr int64_t ms = 1'000'000, s = 1000 * ms;

    std::string Row(const Frame& f)
    {
        char out[128];
        return std::string(out, FormatRow(f, out, sizeof(out)));
    }

    Frame World(int64_t at, int steps, int ticks = 60)
    {
        return Frame{ .hostNs = at, .measuredMs = 16.667, .frameMs = ticks / 3.6, .accMs = 20, .ticks = ticks, .steps = steps, .rawSteps = steps };
    }
}

int main()
{
    printf("---- the step count ----\n");
    Check(ClampSteps(0) == 1, "a raw 0 (acc a hair under a step after the float product): 1, as the game raises it");
    Check(ClampSteps(-3) == 1, "negative: 1");
    Check(ClampSteps(1) == 1 && ClampSteps(2) == 2 && ClampSteps(8) == 8, "1 to 8 kept");
    Check(ClampSteps(9) == 8 && ClampSteps(1000) == 8, "over 8: 8");

    printf("---- the CSV ----\n");
    Check(std::string(kHeader) == "host_ns,measured_ms,ticks,frame_ms,steps,raw_steps,acc_ms\n", "the header");
    {
        Frame f{ .hostNs = 123456789012345, .measuredMs = 16.6667, .frameMs = 16.3889, .accMs = 33.0556, .ticks = 59, .steps = 2, .rawSteps = 1 };
        Check(Row(f) == "123456789012345,16.667,59,16.389,2,1,33.056\n", Row(f).c_str());
    }
    {
        Frame f{ .hostNs = 5, .measuredMs = 100, .ticks = 360 };
        Check(Row(f) == "5,100.000,360,0.000,-1,-1,-1.000\n", "a frame without the world: steps, raw and acc -1");
    }
    {
        Frame f = World(1, 0);
        f.rawSteps = 0;
        Check(Row(f).find(",0,0,20.000\n") != std::string::npos, "below one step: 0 and 0");
    }
    {
        char small[16];
        Frame f{ .hostNs = 123456789012345 };
        const size_t n = FormatRow(f, small, sizeof(small));
        Check(n == sizeof(small) - 1 && std::strlen(small) == n, "a short buffer: cut, never overrun");
    }

    printf("---- the summary ----\n");
    {
        Summary sum;
        Check(!sum.Due(100 * s), "empty: never due");
        int64_t t = 1 * s;
        const int steps[] = { 1, 1, 0, 2, 1, 3, 5, -1 };
        for (int k : steps)
        {
            sum.Add(World(t, k, k < 0 ? 0 : 60));
            t += 16 * ms;
        }
        Check(sum.frames == 8 && sum.noWorld == 1, "frames, and those without the world");
        Check(sum.steps[0] == 1 && sum.steps[1] == 3 && sum.steps[2] == 1 && sum.steps[3] == 2, "0, 1, 2 and 3+ (3 and 5) steps");
        Check(sum.ticks == 7 * 60, "ticks summed");
        Check(!sum.Due(1 * s + 9999 * ms) && sum.Due(11 * s), "due 10 s after its first frame");
        const std::string line = sum.Report(11 * s);
        Check(line == "[gametime] 10.0 s: 8 frames (1 without the world): 1 with no step, 3 with 1, 1 with 2, 2 with 3+; mean 52.50 ticks, 16.667 ms measured",
            line.c_str());
        sum.StartWindow(11 * s);
        Check(sum.frames == 0 && sum.noWorld == 0 && sum.steps[0] + sum.steps[1] + sum.steps[2] + sum.steps[3] == 0 && sum.ticks == 0 &&
                  sum.measuredMs == 0 && sum.windowStart == 11 * s,
            "a new window starts empty, at now");
        Check(!sum.Due(20 * s) && sum.Due(21 * s), "and is due 10 s after that");
    }
    {
        Summary sum;
        Check(sum.Report(0).find("mean 0.00 ticks") != std::string::npos, "an empty window: no division by zero");
    }

    printf(g_failures ? "%d FAILED\n" : "all passed\n", g_failures);
    return g_failures ? 1 : 0;
}
