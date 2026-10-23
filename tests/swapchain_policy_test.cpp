// From SpeedBreaker v0.1.8's tests/swapchain_policy_test.cpp
// (GPL-3.0-or-later). ReCarbon changes: the Mac's part only. On macOS the
// presenter never suspends (SDL sends no background events there), reads no
// interface orientation (Unknown) and is never landscape-only, and its view
// always fills its window (presenter.cpp CheckSwapchain), so upstream's
// iPhone, iPad, inactive and resume scenarios, and the planted rules only
// they catch (holdWhileInactive, refusePortrait, repairView, repairEvery,
// repairPatience), aren't ported. Kept: the periodic check, SUBOPTIMAL at the
// same and a new size, OUT_OF_DATE, desktop resizes and a tall window, and
// the UI's display size on Retina windows. Added: entering fullscreen as the
// M5 logged it, a size event and a rebuild each starting the watch, and the
// UI sizes logged on the M5 and M1.
//
// Tests for video::SwapchainPolicy, ChooseExtent and FitUiDisplay
// (runtime/video/swapchain_policy.cpp): when the presenter builds its
// swapchain again, at what size, and the size the UI lays out at. Each
// scenario drives the policy as presenter.cpp does (the surface read when it
// asks, a rebuild at the size it decides, MoltenVK's SUBOPTIMAL after a
// present while the layer and the swapchain disagree); then every planted
// wrong rule must fail at least one scenario.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/swapchain_policy_test.cpp runtime/video/swapchain_policy.cpp -o build/swapchain_policy_test
//   build/swapchain_policy_test
#include <video/swapchain_policy.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace video;

static int g_failures = 0;
static bool g_quiet = false;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; if (!g_quiet) { printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static const Extent kWindow{ 2560, 1440 };  // ReCarbon: a 1280x720-point window on a Retina Mac, as logged

// The presenter, the layer and the view, around one policy.
struct Screen
{
    SwapchainPolicy policy;
    Extent swapchain = kWindow;
    Extent surface = kWindow;      // the layer's size (MoltenVK's currentExtent)
    Extent windowPixels = kWindow;  // what a fitted-back view measures
    bool active = true;
    Orientation interface = Orientation::Unknown;  // ReCarbon: as presenter.cpp's InterfaceNow off iOS
    bool landscapeOnly = false;
    bool viewFits = true;
    bool repairWorks = true;       // fitting the view back takes
    int frames = 0;
    int recreates = 0, repairs = 0;
    std::vector<Extent> built;

    explicit Screen(SwapchainRules rules) : policy(rules) {}

    // One RunFrame's check.
    SwapDecision Frame()
    {
        frames++;
        SurfaceState s;
        s.active = active;
        s.swapchain = swapchain;
        // Read whenever asked, active or not: holding off is the policy's.
        if (policy.WantsSurface())
        {
            s.surfaceRead = true;
            s.surface = surface;
            s.interface = interface;
            s.landscapeOnly = landscapeOnly;
            s.viewFitsWindow = viewFits;
        }
        SwapDecision d = policy.Decide(s);
        if (d.action == SwapAction::Recreate)
        {
            recreates++;
            swapchain = d.extent;
            built.push_back(d.extent);
            if (!swapchain.Empty())
                policy.Created(swapchain);
        }
        else if (d.action == SwapAction::RepairView)
        {
            repairs++;
            if (repairWorks)
            {
                // SDL lays the view out again: its drawables and resize events.
                viewFits = true;
                surface = windowPixels;
                policy.SizeChanged();
            }
        }
        return d;
    }

    // The frame's present (none while inactive): MoltenVK says SUBOPTIMAL
    // while the layer and the swapchain disagree, for the next frame's check.
    void Present()
    {
        if (active && !swapchain.Empty() && surface != swapchain)
            policy.Reported(SurfaceResult::Suboptimal);
    }

    void Run(int n)
    {
        for (int i = 0; i < n; i++)
        {
            Frame();
            Present();
        }
    }

    bool BuiltPortrait() const
    {
        for (const Extent& e : built)
            if (e.Portrait())
                return true;
        return false;
    }
};

// Long after anything happened, the layer moves with no event and no
// SUBOPTIMAL (a driver that doesn't say): caught within a second.
static void PeriodicCheck(const SwapchainRules& rules)
{
    Screen s(rules);
    for (int i = 0; i < 1000; i++)
        s.Frame();
    s.surface = { 2400, 1350 };
    int caughtAfter = -1;
    for (int i = 0; i < 200 && caughtAfter < 0; i++)
        if (s.Frame().action == SwapAction::Recreate)
            caughtAfter = i;
    CHECK(caughtAfter >= 0 && caughtAfter < 61, "an unannounced size: caught after %d frames (want within 60)", caughtAfter);
}

// MoltenVK's SUBOPTIMAL at the size the swapchain already has (drawables
// rounded another way): rebuilt, but not every frame.
static void SuboptimalSameSize(const SwapchainRules& rules)
{
    Screen s(rules);
    s.Run(200);
    for (int i = 0; i < 90; i++)
    {
        s.policy.Reported(SurfaceResult::Suboptimal);
        s.Frame();
    }
    CHECK(s.recreates >= 2 && s.recreates <= 4, "SUBOPTIMAL at the same size for 90 frames: %d rebuilds (want 2-4)", s.recreates);
}

// SUBOPTIMAL from a new size, no event, between checks: rebuilt at once.
static void SuboptimalNewSize(const SwapchainRules& rules)
{
    Screen s(rules);
    s.Run(500);
    s.surface = { 2400, 1320 };
    s.policy.Reported(SurfaceResult::Suboptimal);
    SwapDecision d = s.Frame();
    CHECK(d.action == SwapAction::Recreate && s.swapchain == Extent(2400, 1320), "SUBOPTIMAL at a new size: %s, swapchain %ux%u",
        SwapActionName(d.action), s.swapchain.width, s.swapchain.height);
}

// OUT_OF_DATE: rebuilt whatever the size.
static void OutOfDate(const SwapchainRules& rules)
{
    Screen s(rules);
    s.Run(500);
    s.policy.Reported(SurfaceResult::OutOfDate);
    SwapDecision d = s.Frame();
    CHECK(d.action == SwapAction::Recreate && s.swapchain == kWindow, "OUT_OF_DATE, same size: %s", SwapActionName(d.action));
}

// The Mac: window resizes, a fullscreen toggle, a Space change
// (each a pixel-size event, built that frame); an event with no change
// builds nothing; minimised tears down and comes back.
static void Desktop(const SwapchainRules& rules)
{
    Screen s(rules);
    s.swapchain = s.surface = { 2560, 1440 };
    s.Run(100);
    const Extent sizes[] = { { 2000, 1200 }, { 1800, 1100 }, { 3456, 2234 }, { 2560, 1440 }, { 3024, 1964 } };
    for (const Extent& e : sizes)
    {
        s.surface = e;
        s.policy.SizeChanged();
        SwapDecision d = s.Frame();
        CHECK(d.action == SwapAction::Recreate && s.swapchain == e, "desktop resize to %ux%u: %s, swapchain %ux%u", e.width, e.height,
            SwapActionName(d.action), s.swapchain.width, s.swapchain.height);
        s.Run(3);
    }
    const int before = s.recreates;
    s.policy.SizeChanged();  // focus, a move: the same size
    s.Run(10);
    CHECK(s.recreates == before, "a size event at the same size: %d rebuilds", s.recreates - before);
    // Minimised: the presenter's wait for a size, as before.
    s.surface = {};
    s.policy.SizeChanged();
    SwapDecision d = s.Frame();
    CHECK(d.action == SwapAction::Recreate && s.swapchain.Empty(), "minimised: %s, swapchain %ux%u", SwapActionName(d.action),
        s.swapchain.width, s.swapchain.height);
    s.surface = { 3024, 1964 };
    s.policy.SizeChanged();
    d = s.Frame();
    CHECK(d.action == SwapAction::Recreate && s.swapchain == Extent(3024, 1964), "restored: %s, swapchain %ux%u",
        SwapActionName(d.action), s.swapchain.width, s.swapchain.height);
    CHECK(!s.BuiltPortrait(), "desktop: a portrait swapchain");
}

// A tall desktop window is portrait, and that's fine there.
static void DesktopPortraitWindow(const SwapchainRules& rules)
{
    Screen s(rules);
    s.swapchain = s.surface = { 1280, 720 };
    s.Run(10);
    s.surface = { 800, 1200 };
    s.policy.SizeChanged();
    s.Frame();
    CHECK(s.swapchain == Extent(800, 1200), "a tall desktop window: swapchain %ux%u", s.swapchain.width, s.swapchain.height);
}

// ReCarbon: entering fullscreen on the MacBook Air M5 (2026-10-23 log:
// "swapchain 2940x1846 (was 2940x1782; the surface's size moved)"). The
// window's size event builds at the first size, then the layer grows to the
// screen's with no event of its own while the animation ends: caught on the
// frame it moves, inside the watch a size event and a new swapchain start
// (here 30 frames on), not up to a second later by the periodic check.
static void FullscreenSettlesLate(const SwapchainRules& rules)
{
    Screen s(rules);
    s.swapchain = s.surface = { 2560, 1440 };
    s.Run(500);
    s.surface = { 2940, 1782 };
    s.policy.SizeChanged();
    s.Frame();
    CHECK(s.swapchain == Extent(2940, 1782), "fullscreen: first swapchain %ux%u", s.swapchain.width, s.swapchain.height);
    for (int i = 0; i < 29; i++)
        s.Frame();
    s.surface = { 2940, 1846 };
    SwapDecision d = s.Frame();
    CHECK(d.action == SwapAction::Recreate && s.swapchain == Extent(2940, 1846), "fullscreen, the layer moved 30 frames later: %s, swapchain %ux%u",
        SwapActionName(d.action), s.swapchain.width, s.swapchain.height);
}

// ReCarbon: each of the two starts the watch on its own. A size event at an
// unchanged size (a Space change, a move to another display of the same
// scale), and a rebuild for SUBOPTIMAL at a new size with no event: a layer
// that moves 30 frames later is caught on that frame.
static void EachStartsTheWatch(const SwapchainRules& rules)
{
    Screen s(rules);
    s.Run(500);
    s.policy.SizeChanged();
    s.Run(30);
    s.surface = { 2400, 1350 };
    SwapDecision d = s.Frame();
    CHECK(d.action == SwapAction::Recreate, "30 frames after a size event at the same size, the layer moved: %s", SwapActionName(d.action));

    Screen r(rules);
    r.Run(500);
    r.surface = { 2400, 1350 };
    r.policy.Reported(SurfaceResult::Suboptimal);
    r.Frame();
    CHECK(r.swapchain == Extent(2400, 1350), "SUBOPTIMAL at a new size: swapchain %ux%u", r.swapchain.width, r.swapchain.height);
    for (int i = 0; i < 30; i++)
        r.Frame();
    r.surface = { 2560, 1440 };
    d = r.Frame();
    CHECK(d.action == SwapAction::Recreate, "30 frames after a rebuild with no event, the layer moved: %s", SwapActionName(d.action));
}

static const std::vector<std::pair<const char*, void (*)(const SwapchainRules&)>> kScenarios = {
    { "the periodic check", PeriodicCheck },
    { "SUBOPTIMAL, same size", SuboptimalSameSize },
    { "SUBOPTIMAL, new size", SuboptimalNewSize },
    { "OUT_OF_DATE", OutOfDate },
    { "desktop", Desktop },
    { "a tall desktop window", DesktopPortraitWindow },
    { "fullscreen settles late", FullscreenSettlesLate },
    { "each starts the watch", EachStartsTheWatch },
};

static int RunScenarios(const SwapchainRules& rules)
{
    int failed = 0;
    for (const auto& [name, run] : kScenarios)
    {
        const int before = g_failures;
        run(rules);
        if (g_failures != before)
        {
            failed++;
            if (!g_quiet)
                printf("  scenario \"%s\" failed\n", name);
        }
    }
    return failed;
}

// The same scenarios under a wrong rule must fail at least one.
static void Planted(const char* what, const std::function<void(SwapchainRules&)>& plant)
{
    SwapchainRules wrong;
    plant(wrong);
    const int saved = g_failures;
    g_quiet = true;
    const int failed = RunScenarios(wrong);
    g_quiet = false;
    g_failures = saved;
    printf("planted %-36s %d scenario(s) fail\n", what, failed);
    CHECK(failed > 0, "planted %s: every scenario still passes", what);
}

static void Choose(Extent surface, Orientation interface, bool landscapeOnly, Extent want, bool refused)
{
    ExtentChoice c = ChooseExtent(surface, interface, landscapeOnly);
    CHECK(c.extent == want && c.refused == refused, "ChooseExtent(%ux%u, %s, %s): %ux%u%s, want %ux%u%s", surface.width, surface.height,
        OrientationName(interface), landscapeOnly ? "landscape-only" : "any", c.extent.width, c.extent.height, c.refused ? " refused" : "",
        want.width, want.height, refused ? " refused" : "");
}

static void Ui(float ww, float wh, Extent fb, float wantW, float wantH, float wantScale, bool fromSwapchain)
{
    UiDisplay u = FitUiDisplay(ww, wh, fb);
    printf("UI for a %.0fx%.0f window, swapchain %ux%u: %.1fx%.1f points, scale %.3f x %.3f%s\n", ww, wh, fb.width, fb.height, u.width,
        u.height, u.scaleX, u.scaleY, u.fromSwapchain ? " (the swapchain's)" : "");
    CHECK(std::fabs(u.width - wantW) < 0.6f && std::fabs(u.height - wantH) < 0.6f && u.fromSwapchain == fromSwapchain,
        "%.0fx%.0f window, %ux%u: %.1fx%.1f%s", ww, wh, fb.width, fb.height, u.width, u.height, u.fromSwapchain ? " from the swapchain" : "");
    CHECK(std::fabs(u.scaleX - wantScale) < 0.01f && std::fabs(u.scaleY - wantScale) < 0.01f, "scale %.3f x %.3f, want %.3f", u.scaleX,
        u.scaleY, wantScale);
    // The UI's own scale follows the display's height (ui.cpp UpdateScale).
}

int main()
{
    printf("ChooseExtent (never landscape-only on a Mac):\n");
    Choose(kWindow, Orientation::Unknown, false, kWindow, false);
    Choose({ 800, 1200 }, Orientation::Unknown, false, { 800, 1200 }, false);
    Choose({ 2940, 1846 }, Orientation::Unknown, false, { 2940, 1846 }, false);
    Choose({}, Orientation::Unknown, false, {}, false);

    printf("\nscenarios:\n");
    const int failed = RunScenarios(SwapchainRules{});
    printf("%zu scenarios, %d failed\n", kScenarios.size(), failed);

    printf("\nplanted (each must fail a scenario):\n");
    Planted("followSuboptimal off", [](SwapchainRules& r) { r.followSuboptimal = false; });
    Planted("settleFrames 0", [](SwapchainRules& r) { r.settleFrames = 0; });
    Planted("settleFrames 20", [](SwapchainRules& r) { r.settleFrames = 20; });
    Planted("periodFrames 0 (never)", [](SwapchainRules& r) { r.periodFrames = 0; });
    Planted("periodFrames 120", [](SwapchainRules& r) { r.periodFrames = 120; });
    Planted("suboptimalBackoff 0", [](SwapchainRules& r) { r.suboptimalBackoff = 0; });
    Planted("suboptimalBackoff 100", [](SwapchainRules& r) { r.suboptimalBackoff = 100; });

    printf("\nUI display:\n");
    Ui(1512, 982, { 3024, 1964 }, 1512, 982, 2.0f, false);     // a MacBook Pro's Retina window
    Ui(1512, 982, { 3020, 1964 }, 1512, 982, 1.997f, false);   // mid-resize, a few pixels behind: the window's size stands
    Ui(1512, 982, {}, 1512, 982, 1.0f, false);                 // no swapchain yet
    // ReCarbon: as logged on the M5 and M1 (2026-10-23).
    Ui(1280, 720, { 2560, 1440 }, 1280, 720, 2.0f, false);     // a 1280x720-point window
    Ui(1470, 923, { 2940, 1846 }, 1470, 923, 2.0f, false);     // M5 fullscreen
    Ui(1440, 900, { 2880, 1800 }, 1440, 900, 2.0f, false);     // M1 fullscreen
    Ui(1470, 891, { 2940, 1846 }, 1470, 923, 2.0f, true);      // M5, fullscreen's swapchain before the window's size caught up

    printf(g_failures ? "\n%d FAILED\n" : "\nall passed\n", g_failures);
    return g_failures ? 1 : 0;
}
