// ReCarbon runtime tests. GPL-3.0-or-later (see COPYING).
//
// gpu/resolution_settle.h: stepping through Internal Resolution's entries in
// the open menu rebuilds the render targets once, for the value the player
// stops at, 250 ms after the last step or when the menu closes; a change
// with the menu closed applies at once. On a fake clock, as renderer.cpp's
// ApplyLiveSettings calls it once a swap (60 a second here) while the
// setting differs from the one applied, and Reset() when it is back to it.
// Header only. From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/resolution_settle_test.cpp -o build/resolution_settle_test && build/resolution_settle_test
#include <gpu/resolution_settle.h>

#include <cstdio>
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

    // The renderer's side: the value applied, and each rebuild.
    struct Renderer
    {
        gpu::ResolutionSettle settle;
        int32_t applied = 0;  // Auto
        std::vector<int32_t> rebuilds;
        double clock = 0.0;

        // One swap at `clock` with the setting at `setting`.
        void Swap(int32_t setting, bool menuOpen)
        {
            if (setting == applied)
                settle.Reset();
            else if (settle.Ready(setting, clock, menuOpen))
            {
                applied = setting;
                rebuilds.push_back(setting);
            }
            clock += 1.0 / 60.0;
        }
        // Swaps for `seconds` at that setting.
        void Hold(int32_t setting, bool menuOpen, double seconds)
        {
            for (double end = clock + seconds; clock < end - 1e-9;)
                Swap(setting, menuOpen);
        }
    };

    void Stepping()
    {
        printf("---- stepping in the open menu ----\n");
        Renderer r;
        // From Auto: HD, FHD, QHD, 4K UHD, a step every 100 ms, then the player stops.
        for (int32_t v : { 2, 3, 4, 6 })
            r.Hold(v, true, 0.1);
        Check(r.rebuilds.empty(), "no rebuild while the steps come faster than 250 ms");
        r.Hold(6, true, 0.2);
        Check(r.rebuilds.size() == 1 && r.applied == 6, "one rebuild, for 4K UHD, once it has stood 250 ms");
        // And not before 250 ms.
        Renderer s;
        s.Hold(6, true, 0.2);
        Check(s.rebuilds.empty(), "200 ms after the step: still waiting");
        s.Hold(6, true, 0.07);
        Check(s.rebuilds.size() == 1, "after 250 ms: applied");
    }

    void MenuCloses()
    {
        printf("---- the menu closes ----\n");
        Renderer r;
        r.Hold(2, true, 0.05);
        r.Swap(2, false);
        Check(r.rebuilds.size() == 1 && r.applied == 2, "the menu closes: applied at once");
        Renderer c;
        c.Swap(6, false);
        Check(c.rebuilds.size() == 1, "a change with the menu closed: at once");
    }

    void BackToApplied()
    {
        printf("---- stepped away and back ----\n");
        Renderer r;
        r.Hold(6, true, 0.1);
        r.Hold(0, true, 1.0);  // back to the applied Auto
        Check(r.rebuilds.empty(), "back to the applied value: no rebuild");
        r.Hold(6, true, 0.1);
        Check(r.rebuilds.empty(), "the same step again waits its own 250 ms, not the first one's");
        r.Hold(6, true, 0.2);
        Check(r.rebuilds.size() == 1, "and then applies");
    }
}

int main()
{
    Stepping();
    MenuCloses();
    BackToApplied();
    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
