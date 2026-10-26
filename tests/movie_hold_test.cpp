// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// game/movie_hold.h: when a movie frame's swap waits one vblank more, so NFS: Carbon's 29.97 fps
// movies show each frame for two vblanks. First each rule alone; then the swap callbacks recorded
// from Carbon's boot and attract movies (tests/data/carbon_movie_swaps.txt) replayed through the
// game's own target rule (D3D's swap callback, sub_826E3378: released + interval, or the vblank
// after the current one when that has passed): without holds the replay gives the 15.6% uneven
// frames measured on screen; with them, almost none, and no frame is held outside a movie.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/movie_hold_test.cpp -o build/movie_hold_test && build/movie_hold_test tests/data/carbon_movie_swaps.txt
#include <game/movie_hold.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

    using namespace game::movie;
    constexpr double kPeriod = 16.6667e6, kGuard = 10e6;

    // A steady movie: four frames of two vblanks behind it.
    Tracker Steady()
    {
        Tracker t;
        t.steady = kSteadyFrames;
        return t;
    }

    // A swap due one vblank after the last release (released 100, count 100: target 101), `since`
    // ms after the vblank.
    Swap Due(double sinceMs)
    {
        return { .interval = 1, .count = 100, .released = 100, .target = 101, .sinceVblankNs = sinceMs * 1e6,
            .periodNs = kPeriod, .guardNs = kGuard };
    }

    struct Replay
    {
        uint64_t spans[5] = {};
        uint64_t held = 0;
        double Uneven() const
        {
            uint64_t n = spans[1] + spans[2] + spans[3] + spans[4];
            return n ? 100.0 * double(n - spans[2]) / double(n) : 0;
        }
    };

    // The recorded callbacks through the game's rule, every one a movie frame.
    Replay Run(const std::vector<std::pair<uint32_t, uint32_t>>& swaps, double guardNs)
    {
        Replay r;
        Tracker t;
        bool first = true;
        uint32_t released = 0;
        for (auto [count, sinceUs] : swaps)
        {
            uint32_t target = !first && released + 1 > count ? released + 1 : count + 1;
            Record(t, true, released);
            Swap s{ .interval = 1, .count = count, .released = released, .target = target, .sinceVblankNs = sinceUs * 1e3,
                .periodNs = kPeriod, .guardNs = guardNs };
            if (!first && ShouldHold(t, true, s))
            {
                target++;
                r.held++;
            }
            if (!first)
                r.spans[std::min<uint32_t>(target - released, 4)]++;
            released = target;
            first = false;
        }
        return r;
    }
}

int main(int argc, char** argv)
{
    printf("---- each rule ----\n");
    Check(ShouldHold(Steady(), true, Due(12)), "a steady movie's frame due one vblank after the last, 12 ms after a vblank: held");
    Check(ShouldHold(Steady(), true, Due(6.67)), "6.67 ms after it: exactly the guard before the next, held");
    Check(!ShouldHold(Steady(), true, Due(6.6)), "6.6 ms after it: earlier than the guard, not held (the movie's own 1)");
    Check(!ShouldHold(Steady(), false, Due(12)), "not a movie frame (the game's own): never held");
    Tracker young = Steady();
    young.steady = kSteadyFrames - 1;
    Check(!ShouldHold(young, true, Due(12)), "fewer than four steady movie frames behind it: not held");
    Swap two = Due(12);
    two.target = 102;
    Check(!ShouldHold(Steady(), true, two), "already due two vblanks after the last: not held");
    Swap every = Due(12);
    every.interval = 0;
    Check(!ShouldHold(Steady(), true, every), "interval 0 (immediate): not held");
    Swap late = Due(12);
    late.count = 101;
    late.target = 102;
    late.released = 101;
    Check(ShouldHold(Steady(), true, late), "the count already at the release, target the next: held the same way");
    Swap off = Due(12);
    off.guardNs = 0;
    Check(!ShouldHold(Steady(), true, off), "guard 0: off");
    Swap slow = Due(12);
    slow.periodNs = 1e9 / 50;
    Check(!ShouldHold(Steady(), true, slow), "a vblank period far from 60 Hz (50 Hz): not held");
    Swap suspended = Due(12);
    suspended.suspended = true;
    Check(!ShouldHold(Steady(), true, suspended), "suspended, or no vblank since the resume: not held");

    printf("---- the tracker ----\n");
    Tracker t;
    Record(t, true, 10);
    Check(t.frames == 0, "the first swap: nothing released before it to count");
    Record(t, true, 12);
    Check(t.frames == 1 && t.spans[2] == 1 && t.steady == 1, "a frame's span is counted when the next swap comes");
    Record(t, true, 14);
    Check(t.spans[2] == 2 && t.steady == 2, "and the next");
    Record(t, true, 15);
    Check(t.spans[1] == 1 && t.steady == 0, "a 1 ends the steady run");
    Check(Record(t, false, 20) && t.frames == 0 && t.steady == 0, "a swap with no movie frame ends the movie: counts reset");
    Check(!Record(t, false, 21), "...once");

    printf("---- Carbon's recorded movie swaps ----\n");
    std::vector<std::pair<uint32_t, uint32_t>> swaps;
    std::ifstream in(argc > 1 ? argv[1] : "tests/data/carbon_movie_swaps.txt");
    for (std::string line; std::getline(in, line);)
    {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream fields(line);
        uint32_t count, since;
        if (fields >> count >> since)
            swaps.push_back({ count, since });
    }
    Check(swaps.size() == 2825, "2825 recorded swaps");
    Replay before = Run(swaps, 0), after = Run(swaps, kGuard);
    printf("  without holds: 1:%llu 2:%llu 3:%llu 4+:%llu (%.1f%% uneven); with: 1:%llu 2:%llu 3:%llu 4+:%llu (%.1f%%), %llu held\n",
        (unsigned long long)before.spans[1], (unsigned long long)before.spans[2], (unsigned long long)before.spans[3],
        (unsigned long long)before.spans[4], before.Uneven(), (unsigned long long)after.spans[1], (unsigned long long)after.spans[2],
        (unsigned long long)after.spans[3], (unsigned long long)after.spans[4], after.Uneven(), (unsigned long long)after.held);
    Check(before.Uneven() > 15 && before.Uneven() < 16.5, "the replay without holds gives what was measured (15.6%)");
    Check(after.Uneven() < 1.5, "with holds: under 1.5% uneven");
    Check(after.spans[1] * 10 < before.spans[1], "a tenth of the 1s at most");
    Check(after.spans[4] <= before.spans[4], "no frame left longer on screen than before");

    printf("---- %d failed checks ----\n", g_failures);
    return g_failures ? 1 : 0;
}
