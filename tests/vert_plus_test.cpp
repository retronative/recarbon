// ReCarbon: tests for Vert+'s policy (runtime/game/vert_plus.h) and the
// band clip's parked rule (runtime/gpu/edge_bars.h Parked, from SpeedBreaker's
// Carbon layer, GPL-3.0-or-later). Every expected value is worked by hand.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/vert_plus_test.cpp -o build/vert_plus_test
//   build/vert_plus_test
#include <game/vert_plus.h>
#include <gpu/edge_bars.h>

#include <cmath>
#include <cstdio>

using game::vert_plus::State;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool Near(float a, float b)
{
    return std::fabs(a - b) <= 1e-5f;
}

static void Gate()
{
    printf("RECARBON_VERT_PLUS:\n");
    using game::vert_plus::Requested;
    CHECK(!Requested("0"), "\"0\" is on");
    for (const char* v : { (const char*)nullptr, "", "1", "true", "00", "01", " 0" })
        CHECK(Requested(v), "\"%s\" is off", v ? v : "(unset)");
}

// Off: nothing the game hands the hooks changes, and nothing reports hooks
// or asks for a guest-memory write, whatever scales the presenter sets.
static void Off()
{
    printf("Vert+ off:\n");
    State s(false);
    s.SetScales(1.0f, 0.75f);
    float p00 = 1.25f, p11 = 2.0f;
    CHECK(!s.Projection(1, p00, p11) && p00 == 1.25f && p11 == 2.0f, "projection changed: %f %f", p00, p11);
    CHECK(!s.HooksActive() && !s.TallHooksActive(), "hooks reported after the projection hook");
    float x = 0.69f, y = 0.92f;
    CHECK(!s.FrontEnd(x, y) && x == 0.69f && y == 0.92f, "front end changed or resent: %f %f", x, y);
    CHECK(s.FrontEndX() == 0.0f && s.FrontEndY() == 0.0f, "front-end scales %f %f, want 0 0", s.FrontEndX(), s.FrontEndY());
    float l = 0.638f, t = 0.785f, w = -0.275f, h = 0.128f;
    s.Rectangle(l, t, w, h);
    CHECK(l == 0.638f && t == 0.785f && w == -0.275f && h == 0.128f, "mirror moved: %f %f %f %f", l, t, w, h);
    CHECK(!s.WritesMovieAspect(), "movie aspect written");
    // The renderer still reads the presenter's scales back (1, 1 there).
    CHECK(s.Get().y == 0.75f, "scales not kept: %f", s.Get().y);
}

static void Views()
{
    printf("Widened views:\n");
    for (uint32_t id = 0; id < 64; id++)
    {
        const bool want = id == 1 || id == 2 || id == 4 || id == 5 || id == 21;
        CHECK(game::vert_plus::WidenedViewId(id) == want, "view %u: %d", id, !want);
    }
    State s(true);
    float p00 = 1.25f, p11 = 2.0f;
    // At 1, 1 (16:9, or Aspect Ratio on 16:9) nothing changes, but the hooks
    // are reported.
    CHECK(!s.Projection(1, p00, p11) && p00 == 1.25f && p11 == 2.0f, "1, 1 changed the view");
    CHECK(s.TallHooksActive() && !s.HooksActive(), "after the hook: Hor+ %d Vert+ %d", s.HooksActive(), s.TallHooksActive());
    // Vert+ 0.75 (4:3): |P[1][1]| 2 -> 1.5, P[0][0] kept.
    s.SetScales(1.0f, 0.75f);
    CHECK(s.Projection(4, p00, p11) && p00 == 1.25f && p11 == 1.5f, "view 4: %f %f, want 1.25 1.5", p00, p11);
    // The mirror (3) and the picture-in-picture (8) keep theirs.
    for (uint32_t id : { 3u, 8u, 0u, 16u })
    {
        float a = 1.25f, b = 2.0f;
        CHECK(!s.Projection(id, a, b) && a == 1.25f && b == 2.0f, "view %u widened: %f %f", id, a, b);
    }
    // The presenter's scales are clamped to 0.5..1 in y, 0.1..1 in x.
    s.SetScales(1.0f, 0.25f);
    CHECK(s.Get().y == 0.5f && s.Get().x == 1.0f, "clamp: %f %f", s.Get().x, s.Get().y);
    s.SetScales(1.5f, 1.5f);
    CHECK(s.Get().y == 1.0f && s.Get().x == 1.0f, "clamp: %f %f", s.Get().x, s.Get().y);
}

static void FrontEnd()
{
    printf("Front-end matrix:\n");
    State s(true);
    // The first frame at 1, 1: unchanged, sent once.
    float x = 0.69f, y = 0.92f;
    CHECK(s.FrontEnd(x, y) && x == 0.69f && y == 0.92f, "first frame: %f %f, not resent", x, y);
    x = 0.69f, y = 0.92f;
    CHECK(!s.FrontEnd(x, y), "the same matrix resent");
    // Vert+ 0.75: y 0.92 -> 0.69, x kept; sent once, then not.
    s.SetScales(1.0f, 0.75f);
    x = 0.69f, y = 0.92f;
    CHECK(s.FrontEnd(x, y) && x == 0.69f && Near(y, 0.69f), "Vert+: %f %f, want 0.69 0.69, resent", x, y);
    CHECK(Near(s.FrontEndX(), 0.69f) && Near(s.FrontEndY(), 0.69f), "kept %f %f", s.FrontEndX(), s.FrontEndY());
    x = 0.69f, y = 0.92f;
    CHECK(!s.FrontEnd(x, y), "the same Vert+ matrix resent");
    // Back to 16:9: changed again.
    s.SetScales(1.0f, 1.0f);
    x = 0.69f, y = 0.92f;
    CHECK(s.FrontEnd(x, y) && y == 0.92f, "back to 1, 1: %f, not resent", y);
}

static void Rectangles()
{
    printf("Mirror and picture-in-picture:\n");
    State s(true);
    s.SetScales(1.0f, 0.75f);
    // The mirror: top 0.785 -> 0.5 + 0.285 x 0.75 = 0.71375, height 0.128 ->
    // 0.096; left and the (mirrored) width kept.
    float l = 0.638f, t = 0.785f, w = -0.275f, h = 0.128f;
    s.Rectangle(l, t, w, h);
    CHECK(l == 0.638f && w == -0.275f && Near(t, 0.71375f) && Near(h, 0.096f), "mirror %f %f %f %f", l, t, w, h);
    // The picture-in-picture: top 0.68 -> 0.635, height 0.255 -> 0.19125.
    l = 0.04f, t = 0.68f, w = 0.191f, h = 0.255f;
    s.Rectangle(l, t, w, h);
    CHECK(l == 0.04f && w == 0.191f && Near(t, 0.635f) && Near(h, 0.19125f), "PiP %f %f %f %f", l, t, w, h);
    // The centre stays: a rectangle 0.4..0.6 tall -> 0.425..0.575.
    l = 0.0f, t = 0.4f, w = 1.0f, h = 0.2f;
    s.Rectangle(l, t, w, h);
    CHECK(Near(t, 0.425f) && Near(t + h, 0.575f), "about the centre: %f..%f", t, t + h);
    // A Hor+ scale (not set by this phase's presenter, kept by the policy):
    // left 0.638 at x 0.5 -> 0.569, width -0.275 -> -0.1375.
    s.SetScales(0.5f, 1.0f);
    l = 0.638f, t = 0.785f, w = -0.275f, h = 0.128f;
    s.Rectangle(l, t, w, h);
    CHECK(Near(l, 0.569f) && Near(w, -0.1375f) && t == 0.785f && h == 0.128f, "x 0.5: %f %f %f %f", l, t, w, h);
}

static void Movies()
{
    printf("Movie display aspect:\n");
    using game::vert_plus::MovieAspect;
    CHECK(Near(MovieAspect({ 1.0f, 1.0f }), 16.0f / 9.0f), "16:9: %f", MovieAspect({ 1.0f, 1.0f }));
    // Vert+ 0.75: 4:3, the aspect the 16:9 frame is shown at.
    CHECK(Near(MovieAspect({ 1.0f, 0.75f }), 4.0f / 3.0f), "4:3: %f", MovieAspect({ 1.0f, 0.75f }));
    // 2940x1846: the screen's own aspect, 1.592633.
    CHECK(std::fabs(MovieAspect({ 1.0f, 0.895856f }) - 1.592633f) <= 1e-4f, "2940x1846: %f", MovieAspect({ 1.0f, 0.895856f }));
    // Hor+ 0.75: 64/27 = 2.370370.
    CHECK(std::fabs(MovieAspect({ 0.75f, 1.0f }) - 2.370370f) <= 1e-4f, "Hor+: %f", MovieAspect({ 0.75f, 1.0f }));
    State on(true);
    CHECK(on.WritesMovieAspect(), "on: no movie aspect written");
}

static void ParkedRule()
{
    printf("Parked front-end primitives (band -0.9..0.9):\n");
    using gpu::bars::Parked;
    // Wholly below or above the band: parked in either rule.
    CHECK(Parked(0.95f, 1.05f, -0.9f, 0.9f, false) && Parked(0.95f, 1.05f, -0.9f, 0.9f, true), "below the band");
    CHECK(Parked(-1.05f, -0.95f, -0.9f, 0.9f, false) && Parked(-1.05f, -0.95f, -0.9f, 0.9f, true), "above the band");
    // Pulsing across the edge, centre 0.955 past it: parked by centre only.
    CHECK(!Parked(0.88f, 1.03f, -0.9f, 0.9f, false), "edge crosser parked without byCentre");
    CHECK(Parked(0.88f, 1.03f, -0.9f, 0.9f, true), "edge crosser centred past the band not parked");
    CHECK(Parked(-1.03f, -0.88f, -0.9f, 0.9f, true), "edge crosser centred above the band not parked");
    // Centre 0.725 inside the band: not parked.
    CHECK(!Parked(0.5f, 0.95f, -0.9f, 0.9f, true), "centred inside parked");
    CHECK(!Parked(-0.5f, 0.5f, -0.9f, 0.9f, true), "inside parked");
    // Touching the edge exactly is not past it.
    CHECK(!Parked(0.9f, 0.9f, -0.9f, 0.9f, true), "on the edge parked");
}

int main()
{
    Gate();
    Off();
    Views();
    FrontEnd();
    Rectangles();
    Movies();
    ParkedRule();
    if (g_failures)
    {
        printf("%d FAILED\n", g_failures);
        return 1;
    }
    printf("all passed\n");
    return 0;
}
