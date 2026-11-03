// ReCarbon runtime. GPL-3.0-or-later (see COPYING).
//
// Vert+ for NFS: Carbon (game/ultrawide.h, game/vert_plus.h): on unless
// RECARBON_VERT_PLUS=0; the player's switch is Expanded View (aspect_ratio). The design is SpeedBreaker's Carbon layer
// (game/carbon/ultrawide.cpp, GPL-3.0-or-later): the game keeps rendering its
// 1280x720 frame, the presenter stretches it to a screen narrower than 16:9,
// and the player's view is made taller by the same factor, so the stretch
// doesn't distort it. ReCarbon: every address below was found in this
// executable's own code (ppc/, the decrypted image), by the evidence given
// with it; none comes from another build. With RECARBON_VERT_PLUS=0 each
// hook returns before it reads or writes anything, the override calls the
// game's function unchanged, nothing is written into guest memory, and
// HooksActive()/TallHooksActive() stay false (the frame is letterboxed at
// 16:9, as before).
//
// Two mid-asm hooks (config/recarbon.toml):
//   - VertPlusProjection, before `stfs f5,0(r29)` (word D0BD0000) at
//     0x822CEEC4 in the view setup sub_822CEBA0. r7 is the view (`mr r7,r4`
//     at entry, not written after); its id is the word at +8 (`lwz r6,8(r7)`
//     at 0x822CECB4), which the game's own 16:9 widening tests for 1, 4, 2,
//     5 and 21 (`cmpwi cr6,r6,...` at 0x822CECFC-0x822CED54, gated on the
//     widescreen byte 0x82B74658). The function builds two projections, at
//     view + 64 (r29 = r31 + 64) and view + 128 (r30 = r31 + 128), from the
//     same registers: f5 = P[0][0] = 1 / tan(half the horizontal view)
//     (`fdivs f5,f30,f0` at 0x822CEE48), stored at 0x822CEEC4 and 0x822CEF1C;
//     f11 = 1 / tan(half the vertical view) (`fdivs f11,f30,f11` at
//     0x822CEEB4), negated in place at 0x822CEED8 and stored as P[1][1] at
//     0x822CEEDC (+20 of the first) and 0x822CEF20 (+20 of the second).
//     Neither register is written between the hook and 0x822CEF20 but by
//     that fneg, so one hook scales both matrices; the frustum
//     (sub_822CE9B0, called next) follows from the stored ones. Ids, by the
//     game's name table at 0x82B73B90 (id 0 first): 1 EVIEW_PLAYER1, 2
//     EVIEW_PLAYER2, 3 EVIEW_PLAYER1_RVM, 4 and 5 the players' reflections,
//     8 EVIEW_PLAYER1_PIP, 21 EVIEW_SHADOWMATTE. Per-view targets: 0x82C721D0
//     + id * 480, the target at +464, its width and height at +16 and +20
//     (the same function's `mulli r10,r10,480`, `lwz r10,464(r10)`, `lwz
//     r10,16(r10)` / `lwz r5,20(r11)`, 640x480 without a target).
//   - VertPlusFrontEnd, at the `b 0x8250AEEC` (word 4800005C) at 0x8250AE90
//     that ends the widescreen case of the front-end (HUD and menu) matrix,
//     sub_8250AD98. With the widescreen byte 0x82B74658 set, f0 = 1.0
//     (0x8205E220, or the value a listener returns) x 0.92 (0x82064A30) is
//     the y scale and f13 = f0 x 0.75 (0x8205E444) the x scale; after the
//     branch they are stored as the matrix's [0][0] (`stfs f13,112(r1)`) and
//     [1][1] (`stfs f0,132(r1)`) and the matrix (r4 = r1 + 112) goes to the
//     upload sub_824F4FC8. That function keeps its last call's r5 at
//     0x82C65270, its matrix pointer r4 at 0x82C65274 and r3 at 0x82C65278,
//     and returns at once (to 0x824F53A0) when a call brings the same three
//     and r6 (a force byte; the front end passes 0) is 0. The front end's
//     matrix is always at the same stack slot, so when the hook changes the
//     matrix (a new window shape, the setting changed, the first frames) it
//     clears 0x82C65274 and the next call uploads it. RECARBON_FE_RESEND=0
//     leaves the cache alone (an A/B switch).
// One function override (no ppc/ regeneration):
//   - sub_82505378, the rear-view mirror's and the crew's picture-in-picture
//     rectangle: f1 = left edge, f2 = top or bottom edge (0..1 of the
//     screen, made clip space as 2v - 1), f3 = signed width, f4 = height. Its
//     only two callers (`bl` at 0x825055F0 in sub_825054D8 and 0x82505820 in
//     sub_82505720; no data word points at it but its .pdata entry) pass, in
//     widescreen, 0.638, 0.785, -0.275, 0.128 (the mirror, mirrored) and
//     0.04, 0.68, 0.191, 0.255 (the picture-in-picture). The first reads the
//     texture object at 0x82FD8250, built by the initialiser sub_82AA4020
//     from the string "RVM_MASK" (0x8209CD10); the second the one at
//     0x82FD0800, from "PIP_MASK" (0x82084D08, sub_82AA4088). Narrowed about
//     the screen's centre like the HUD, so each stays in its HUD frame.
// Movies: the XMV renderer's setup, sub_827B7760, asks XGetVideoMode and, on
// a widescreen mode (as the runtime reports), reads its display aspect,
// 1.7777778, from 0x820103C0 (`lfs f0,960(r11)` at 0x827B77FC, the word's
// only reader; 4:3 is 0x820C8634), divides it by the back buffer's aspect and
// keeps it at +140, from which sub_827B7D30 fits the movie's rectangle.
// SetAspectScale writes the aspect the frame is shown at there, so a movie
// set up after it is letterboxed to 16:9.
#include <stdafx.h>
#include "title_hooks.h"
#include "ultrawide.h"
#include "vert_plus.h"

#include <kernel/function.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" PPC_FUNC(__imp__sub_82505378);  // the mirror's and picture-in-picture's rectangle

namespace
{
    constexpr uint32_t kMovieDisplayAspect = 0x820103C0;
    constexpr uint32_t kViewTable = 0x82C721D0, kViewStride = 480, kViewTarget = 464;
    constexpr uint32_t kFrontEndMatrixPointer = 0x82C65274;  // sub_824F4FC8's cached matrix pointer

    game::vert_plus::State& Vp()
    {
        static game::vert_plus::State s{ game::vert_plus::Requested(std::getenv("RECARBON_VERT_PLUS")) };
        return s;
    }

    const bool s_frontEndResend = [] {
        const char* v = std::getenv("RECARBON_FE_RESEND");
        return !(v && v[0] == '0');
    }();

    uint32_t GuestWord(uint32_t address)
    {
        return ByteSwap(*static_cast<const uint32_t*>(g_memory.Translate(address)));
    }

    void LogView(uint32_t id, bool widened)
    {
        static std::atomic<uint64_t> seen{ 0 };
        if (id >= 64 || (seen.fetch_or(1ull << id) & (1ull << id)))
            return;
        const uint32_t target = GuestWord(kViewTable + id * kViewStride + kViewTarget);
        const uint32_t w = target ? GuestWord(target + 16) : 0, h = target ? GuestWord(target + 20) : 0;
        fprintf(stderr, "[uw] view %u: target %ux%u%s\n", id, w, h, widened ? ", widened" : "");
    }
}

namespace game
{
    void LogTitleHooks()
    {
        if (Vp().On())
            fprintf(stderr, "[title] NFS: Carbon: movie pacing on; Vert+ on (RECARBON_VERT_PLUS=0 turns it off): Expanded View "
                "On makes the view taller on a screen narrower than 16:9, Off letterboxes it at 16:9\n");
        else
            fprintf(stderr, "[title] NFS: Carbon: movie pacing on; Vert+ off (RECARBON_VERT_PLUS=0): "
                "the frame is shown at 16:9\n");
    }

    void SetAspectScale(float scaleX, float scaleY)
    {
        game::vert_plus::State& vp = Vp();
        const bool changed = vp.SetScales(scaleX, scaleY);
        if (!vp.WritesMovieAspect())
            return;
        const game::vert_plus::Scales s = vp.Get();
        if (changed)
            fprintf(stderr, "[uw] view scale x %.4f y %.4f\n", s.x, s.y);
        const float aspect = game::vert_plus::MovieAspect(s);
        uint32_t bits;
        memcpy(&bits, &aspect, sizeof(bits));
        *static_cast<uint32_t*>(g_memory.Translate(kMovieDisplayAspect)) = ByteSwap(bits);
    }

    float AspectScaleX()
    {
        return Vp().Get().x;
    }

    float AspectScaleY()
    {
        return Vp().Get().y;
    }

    bool HooksActive()
    {
        return Vp().HooksActive();
    }

    bool TallHooksActive()
    {
        return Vp().TallHooksActive();
    }

    float FrontEndScaleX()
    {
        return Vp().FrontEndX();
    }

    float FrontEndScaleY()
    {
        return Vp().FrontEndY();
    }

    bool FrontEndParksByCentre()
    {
        return true;
    }
}

void VertPlusProjection(PPCRegister& r7, PPCRegister& f5, PPCRegister& f11)
{
    game::vert_plus::State& vp = Vp();
    if (!vp.On())
        return;
    const uint32_t id = GuestWord(r7.u32 + 8);
    float p00 = float(f5.f64), p11 = float(f11.f64);
    const bool widened = vp.Projection(id, p00, p11);
    LogView(id, game::vert_plus::WidenedViewId(id));
    if (!widened)
        return;
    f5.f64 = double(p00);
    f11.f64 = double(p11);  // its magnitude: negated into P[1][1] after this
}

void VertPlusFrontEnd(PPCRegister& f13, PPCRegister& f0)
{
    game::vert_plus::State& vp = Vp();
    if (!vp.On())
        return;
    float x = float(f13.f64), y = float(f0.f64);
    const bool changed = vp.FrontEnd(x, y);
    f13.f64 = double(x);
    f0.f64 = double(y);
    if (!changed || !s_frontEndResend)
        return;
    *static_cast<uint32_t*>(g_memory.Translate(kFrontEndMatrixPointer)) = 0;
    static uint32_t logged = 0;
    if (++logged <= 16)
        fprintf(stderr, "[uw] front-end matrix x %.4f y %.4f: sent to the game again%s\n", x, y,
            logged == 16 ? " (no more of these lines)" : "");
}

PPC_FUNC(sub_82505378)
{
    game::vert_plus::State& vp = Vp();
    if (vp.On())
    {
        float left = float(ctx.f1.f64), top = float(ctx.f2.f64), width = float(ctx.f3.f64), height = float(ctx.f4.f64);
        vp.Rectangle(left, top, width, height);
        ctx.f1.f64 = double(left);
        ctx.f2.f64 = double(top);
        ctx.f3.f64 = double(width);
        ctx.f4.f64 = double(height);
    }
    __imp__sub_82505378(ctx, base);
}
