# ReCarbon website: design and implementation plan

Research and planning only (2026-10-23, at commit 7d74acf). Nothing here is built, installed,
published or pushed. Each phase below waits for the user's approval. This plan changes no
milestone scope or criterion, and no runtime code.

**Goal:** a racing game site that happens to contain excellent documentation, not a
documentation site with a gaming theme. Astro Starlight is the content engine. The landing
page, the controls screen ("Garage") and the theme are custom.

## 1. Controls: what the code really maps

Every mapping on the site comes from these tables, never typed in by hand (§2).

### 1.1 Keyboard defaults (Settings > Controls, `[keys]` in settings.toml)
Source: `runtime/user/settings.cpp:246-293` (one `KeyOption` per control, a second slot that
defaults to `none`). Resolution rules: `runtime/hid/key_map.h:56-81` and `:111-141` (buttons OR
together; an axis is its positive key minus its negative key; both keys held = centred).

| Xbox control | Default key | 2nd key | settings.cpp | Game action (evidence) |
|---|---|---|---|---|
| Right trigger | W | – | 246 | Gas (option text "in the game's default layout") |
| Left trigger | S | – | 248 | Brake and reverse (option text) |
| Left stick left / right | A / D | ← / → | 250, 252 | Steer (option text) |
| Left stick up / down | ↑ / ↓ | – | 254, 256 | Menus (option text); in driving: to verify |
| A | Space | – | 258 | Menu confirm (M5 hand test 2); in driving: to verify |
| B | Backspace | – | 260 | Menu back (hand test 2); in driving: to verify |
| X | Left Shift | – | 262 | To verify |
| Y | Left Ctrl | – | 264 | To verify |
| LB / RB | Q / E | – | 266, 268 | To verify in game. Also the ReCarbon Settings tab switch (§1.3) |
| D-pad ↑ ↓ ← → | ↑ ↓ ← → | – | 270-276 | Menu navigation (hand test 2) |
| Start | Return | – | 278 | Pause (option text) |
| Back | Escape | – | 280 | Back (option text) |
| Right stick ↑ ↓ ← → | O / L / K / ; | – | 282-288 | Camera or look: to verify |
| Left / right stick click | Z / C | – | 290, 292 | To verify |

Notes:
- The arrows drive both the D-pad and the stick (the `Up` key fires both, `tests/key_map_test.cpp:105-107`).
- Handbrake, nitrous, Speedbreaker, camera and look-back are game actions on Xbox controls, and
  the repo has **no evidence yet** of which control does which in Carbon. Hand test 5's
  per-control checklist (pending) is the planned source. Until it reports, the site shows
  these actions as "to verify" or leaves them out (open decision D4).

### 1.2 Controller defaults
Source: `runtime/hid/pad_map.h:28-38` (14 SDL buttons → XInput bits). Sticks and triggers are
passed through in `runtime/hid/input.cpp` `Update()`, with a radial stick deadzone (default
0.08) and a trigger deadzone (0.02), from `settings.cpp` Input.
- **South/East/West/North → A/B/X/Y**, positional, so a PlayStation pad's Cross is A
  (`pad_map.h:27`).
- Back, Start, LB, RB, LS, RS and the D-pad map one to one.
- **Back is held back** while it might start the Back + Start chord (`input.cpp:59-66`
  `PadBackForGame`).
- **Back paddles and extra buttons are not mapped.** `pad_map.h` has no
  `SDL_GAMEPAD_BUTTON_*_PADDLE*` or `MISC*` entry, and nothing in `runtime/` reads them.
  Whether an 8BitDo Ultimate 2's paddles or L4/R4 reach SDL at all, or are remapped inside the
  pad (8BitDo's own software), is device-specific: **unverified**. The diagram draws the
  paddles greyed out, labelled "not mapped by ReCarbon (yet)". If the pad's firmware remaps
  them to other buttons, they act as those buttons.

### 1.3 ReCarbon's own UI (Settings overlay, ImGui)
- **Open/close:** F1; on macOS also Cmd+, (`runtime/ui/ui.cpp:665-667`). The comment there says
  F1 is the brightness key on Mac keyboards, so it needs Fn+F1. Controller: Back + Start
  (`ui.cpp:117-122`). A three-finger tap on touch screens (`ui.cpp:124`) applies to iOS and
  Steam Deck only, so the site leaves it out.
- **Tabs:** Q / E or LB / RB (`runtime/ui/settings_menu.cpp:74-82`). The tabs are Display,
  Graphics, Audio, Input, Controls and Advanced (`runtime/user/settings.h:104`).
- **Navigation:** arrows / D-pad / left stick. Close or back: Escape or B
  (`settings_menu.cpp:553`, `:918`).
- **On the Controls page:** ←/→ picks the slot. Backspace or Y clears the slot; Delete or X
  resets the control (`settings_menu.cpp:412-423`).
- **Quit Game** is the last footer button, with a confirmation (`settings_menu.cpp:10`).

### 1.4 Verification status (shown on the site as badges)
| Claim | Status | Evidence |
|---|---|---|
| Keyboard default table, and how it resolves | **Tested** | `tests/key_map_test.cpp` (24 controls, roles, bits), `tests/settings_keys_test.cpp` (defaults, `[keys]` names) |
| Key names in settings.toml (`"w"`, `"left_shift"`, `"semicolon"`, `"none"`) | **Tested** | `tests/key_names_test.cpp`, `runtime/user/key_names.h` |
| Controller default table | **Tested** | `tests/pad_map_test.cpp:36-56` (14-button baseline) |
| Keyboard in menus, `[keys]` rebinding surviving a restart | **Hand-verified** | M5 hand test 2 (ROADMAP M5) |
| Keyboard controls in a race (each action) | **Pending** | M7 hand test 5, not yet run |
| Physical controller | **Pending** | M5, an 8BitDo is deferred (ROADMAP, 2026-10-23) |
| Virtual controller (menus, hot-unplug) | **Automated** | `RECARBON_VIRTUAL_PAD` runs; DETACH/ATTACH at 7d74acf |
| Back paddles / extra buttons | **Not mapped** | `pad_map.h` |

The site never upgrades a badge without a commit that does it in `controls.json` (§2).

## 2. Data: one source, no drift

- **Generator:** `scripts/gen_site_controls.py` (new, Python 3.11, no dependencies) parses
  `settings.cpp` `KeyOption(...)` lines, `key_map.h` `kEntries`, `pad_map.h` `kDefault` and
  `settings_menu.cpp`'s `PageDelta`. It writes `site/src/data/controls.generated.json`.
- **Hand-written overlay:** `site/src/data/controls.actions.json` holds per-mode game actions,
  callout text and verification status, with an evidence field (commit, test or hand-test
  name) for each. It may only name controls that exist in the generated file. The build fails
  on an unknown id.
- **Schema (TypeScript):**
  `Control { id: "RightTrigger"…; xinput: "RT"|"A"|…; keys: [scancodeName, scancodeName|null];
  pad: SdlButton|SdlAxis; modes: { driving?: Action; menu?: Action; settings?: Action } }`
  and `Action { label; detail?; status: "tested"|"hand-verified"|"pending"|"unverified"|"not-mapped"; evidence }`.
  Plus a `ui` block for F1 / Cmd+, / Back+Start / Q·E, and a `layout` block for the 75%
  keyboard's key geometry (site-only).
- **Drift check (CI):** the site workflow re-runs the generator and fails if the committed JSON
  differs. A second check asserts the generated defaults equal `tests/pad_map_test.cpp`'s
  baseline, so the C++ tests stay the authority.
- **Built-ins vs customized:** the site shows ReCarbon's built-in keyboard presets (WASD,
  Arrows; generated from `runtime/hid/profiles.h` and checked against `tests/profiles_test.cpp`)
  and the default controller profile, never a user's. A "Presets and your own profiles" panel
  says custom profiles are made in game (Settings > Controls, Settings > Controller). An optional
  local-only viewer could read a pasted profile in the browser, never uploaded (decision D6).

## 3. Platform

- **Astro + Starlight** for `/docs/**`: Markdown/MDX, search (Pagefind), sidebar, i18n-ready.
  Override `Header`, `SiteTitle`, `PageFrame`, `Sidebar`, `Footer`, `Hero`, `Banner` and
  `ThemeSelect` via `starlight({ components: {...} })`. Theme via Starlight's CSS custom
  properties (`--sl-color-*`, `--sl-font`) plus our own token file. Starlight's light theme is
  kept for readability (decision D7), but the default is dark.
- **Custom pages outside the docs layout:** `src/pages/index.astro` (landing) and
  `src/pages/garage.astro` (controls), on our own `RaceLayout.astro` that shares the token
  file and header. Docs pages that embed a diagram use `<StarlightPage>` or MDX components.
- **Interactivity:** Astro islands only where needed. The diagrams are SVG generated at build
  time from the JSON, plus about 6 KB of vanilla TypeScript for mode switching, focus and
  touch. No React or Vue runtime. The page works without JS: all callouts are rendered and
  the mode tabs become anchors.
- **Versioning:** docs per release with the `starlight-versions` plugin (archives `/docs/` as
  `/docs/0.1/` at each release tag). Until 0.1 ships there is one "alpha" version, with a
  banner. A version switcher lives in the header.
- **Deploy:** GitHub Actions `withastro/action`, then `actions/deploy-pages`. **`recarbon` is
  private and stays private.** Pages from a private repo needs a paid plan, and the site would
  still be public. Recommended: a separate public repo (e.g. `retronative/recarbon-site`), fed
  by the generator's output, and no game data, logs or code ever (decision D1). A custom domain
  goes in `public/CNAME` with `base` removed; otherwise `base: '/recarbon-site'` (D2).
- **Budgets:**
  - Landing page: ≤ 150 KB transferred before the hero video (HTML+CSS+JS+fonts), LCP ≤ 2.0 s
    on a 4G profile, CLS < 0.05.
  - Docs pages: ≤ 80 KB.
  - Fonts: two families, subset, `font-display: swap`, self-hosted. The OFL files come from
    Google Fonts but are served locally, so there are no third-party requests.

## 4. Visual direction (original identity)

- **Mood:** night city, wet asphalt, carbon fibre, light trails. ReCarbon's identity, not EA's:
  no NFS logo, typeface, UI chrome or artwork, and no copied "Carbon" lettering.
- **Palette (tokens):**
  - `--rn-asphalt #0B0D10`, `--rn-carbon #15181D`, `--rn-graphite #232831`
  - `--rn-neon #19E3FF` (primary accent), `--rn-ember #FF4D2E` (secondary, warnings),
    `--rn-volt #C6FF3D` (verified badge)
  - `--rn-text #E9EEF5`, `--rn-muted #8A94A6`
  - Every text pair checked ≥ 4.5:1. Neon is used for accents only, never for long text.
  - RetroNative's own brand marks, if any exist, override these (D3).
- **Type (OFL):** display "Chakra Petch" or "Orbitron" (choose in the mockup); UI/body "Inter"
  or "Barlow"; mono "JetBrains Mono" for commands. Italic-slanted display headings (CSS
  `skewX(-8deg)` on blocks, not text) give the speed feel.
- **Motifs:**
  - speed lines as CSS gradients on section edges;
  - HUD frames (corner brackets, tick-mark rulers, a tachometer-style progress arc for
    install steps);
  - diagonal cut cards (`clip-path`);
  - subtle parallax on the hero only;
  - "lap time" style counters for stats (builds, fps).
- **Motion rules:** transitions are 150–300 ms, transform and opacity only. Parallax and
  looping motion are off under `prefers-reduced-motion: reduce`, which also swaps the video
  for its poster. Nothing flashes more than three times a second, and nothing autoplays with
  sound.
- **Navigation:** a top "race HUD" bar: logo, Play (Download), Garage (Controls), Docs,
  Known Issues, GitHub (when public), version chip. In docs, the Starlight sidebar is restyled
  as a "track map" with section numbers. Search stays prominent (Cmd+K).

### Page structure
1. **Landing** (`/`): video hero (§5) → "Native on Apple Silicon" stat strip (measured
   figures only, with dates: e.g. 58.5 fps averaged in hand test 4's race) → what works now
   (cards with status badges) → Garage teaser (mini keyboard/controller) → Get started in 3
   steps (disc image → `--install` → play) → known issues → credits strip → legal footer.
2. **Garage** (`/garage`): the controls screen (§6).
3. **Docs** (`/docs/...`): §7.

## 5. Cinematic video hero

### 5.1 What ReCarbon can really show today (repo evidence)
| Scene | Evidence | Status |
|---|---|---|
| Front end / main menu (Career … Options) | FEASIBILITY §6c "Quick Race" | Captured before |
| Quick Race, race running at ~58–60 fps | FEASIBILITY §6c, WORKLOG `qr01`, A/B runs | Runs (scripted car can't steer) |
| Career race, Zone Heat 1 (Circuit), AI racing, 1st place, results screen | ROADMAP M7, hand test 4 | Ran by hand |
| Prologue chase, free-roam in the city at night, traffic | FEASIBILITY §6b hand test 3 ("prologue chase", "white traffic car") | Ran by hand |
| Cops / pursuits in free-roam | Prologue chase only. No pursuit footage evidence | **Capture to be confirmed** |
| Drift events | none | **Capture to be confirmed** |
| Canyon duel | movie frames only (WORKLOG: "canyon frames" in frame dumps) | **Capture to be confirmed** |
| In-game movies (FMV) | M5, but EA's cinematic footage | **Do not use** (pure EA content) |

### 5.2 Storyboard
**Loop (14 s, seamless, muted).** Cut on motion, with the end frame matched back to the start:
1. 0.0–2.5 s: low chase cam, night street, car accelerating away (Quick Race start, or free-roam).
2. 2.5–5.0 s: side-on pass of a traffic car, light trails (free-roam, prologue).
3. 5.0–8.0 s: corner exit, AI cars in frame (career Circuit, hand-driven).
4. 8.0–11.0 s: bumper cam, straight, city lights streaming (any race).
5. 11.0–14.0 s: chase cam settling into the same framing as shot 1. A crossfade over the last
   0.5 s makes the loop seamless.

**Trailer (60–90 s, optional, user-triggered):**
1. Title card (ReCarbon wordmark, "native on Apple Silicon").
2. Front-end navigation.
3. Quick Race start countdown.
4. Free-roam montage.
5. Career race with AI and results screen.
6. Settings overlay (F1, the tabs) as a ReCarbon feature beat.
7. End card with the Download CTA and the disclaimer.

Pursuit, drift and canyon beats are added only after they're captured and confirmed working.

### 5.3 Capture requirements
- **Machine and build:** a MacBook Air M5, a committed build (SHA in the file's metadata), and
  an **isolated HOME** (`scripts/run_attempt.sh`'s default) with a capture profile created for
  the purpose, never the user's real saves or settings.
- **Video:** Internal Resolution 2x (1440p) or 1.5x (1080p), and a 1920×1080 or
  2560×1440 window or fullscreen. 60 fps, Performance Overlay off, no debug env
  (`RECARBON_DUMP_FRAMES` off: it stalls 57 ms every 60 frames, FEASIBILITY §6c).
- **HUD:** capture each shot twice, with the game's HUD on and (if the game's options allow it)
  off. Whether Carbon has a HUD toggle is to be checked. The loop prefers HUD-off, or cropping
  that keeps EA's HUD and logos out of frame.
- **Camera:** the in-game chase and bumper cameras. There is no free camera, and none is
  planned.
- **Tool:** macOS screen recording (Cmd+Shift+5) or a ScreenCaptureKit recorder (e.g. OBS
  with the macOS ScreenCaptureKit source) at 60 fps, lossless or high-bitrate HEVC master.
  Record known 1 s stalls honestly; don't splice footage to hide them in a "performance" claim
  (cutting between shots is fine).
- **Audio:** game music and SFX are EA's copyright. The loop is **muted with no audio track**.
  A sound version needs a licensing decision (D5): original music, or the trailer without
  sound.

### 5.4 Encoding and loading
- **Sources:** `<video muted loop playsinline autoplay preload="none" poster=…>` with
  `<source>` AV1 (`video/mp4; codecs=av01…`) → HEVC (`hvc1`) → H.264 (`avc1`) fallbacks.
- **Targets:**
  - loop: 1920×1080 at 30 or 60 fps, about 2.5 Mb/s AV1, **≤ 4 MB** for 14 s;
  - mobile variant: 1280×720, about 1.5 Mb/s, ≤ 2.5 MB, chosen by a `media` attribute or JS;
  - trailer: 1080p60 at 6–8 Mb/s, loaded only when the user presses play.
- **Poster:** the shot 1 frame in AVIF, with a WebP fallback, ≤ 120 KB, responsive `srcset`.
  The poster is the LCP element, and it is preloaded.
- **Gating:** the video is attached only after load, by JS, when none of these hold:
  `prefers-reduced-motion`, `navigator.connection.saveData`, or an effective type of 2g/3g.
  Otherwise only the poster shows. An IntersectionObserver pauses the video offscreen, and so
  does `visibilitychange`.
- **Controls:** a visible pause/play button (WCAG 2.2.2) with an aria-label. The trailer has
  standard controls and captions (text cards). A "Watch trailer" button opens a dialog.
- **Overlay:** the logo, a tagline (e.g. "Carbon. Native. Apple Silicon." — copy to be
  approved), and Download and Get Started CTAs. The scrim gradient keeps text ≥ 4.5:1 on any
  frame.

### 5.5 Placeholder until footage exists
An animated abstract hero: layered CSS speed lines and a slow neon gradient sweep over a
carbon-weave SVG pattern, with the wordmark and CTAs. No invented gameplay, renders, or AI
imagery of cars or cities posing as the game. It is static under reduced motion.

### 5.6 Legal note (decision D8)
- Only authentic ReCarbon captures, labelled "captured in ReCarbon <version>, MacBook Air M5".
- Whether to show gameplay footage at all is the maintainer's decision (D8).
- The README's disclaimer appears in the footer of every page.

## 6. Garage: the controls screen

- **Feel:** an in-game control configuration screen. A dark carbon panel, the device on the
  left with neon traces running to callout plates on the right, and a mode selector styled as
  a gear selector: **DRIVE / MENU / SETTINGS**. A device switch: **KEYBOARD | CONTROLLER**.
- **Keyboard (75%):** an original SVG with an F-row, the number row, QWERTY, macOS modifiers
  (fn, control, option, command), arrows and a nav column (Home/End/PgUp/PgDn/Del).
  - Unused keys are dimmed to 25%.
  - Active keys glow in the mode colour, with leader lines (SVG paths with an arrowhead
    marker) to callouts.
  - In DRIVE: W S A D, the arrows, Space, Backspace, Shift, Ctrl, Q/E, O K L ;, Z/C, Return,
    Esc.
  - In SETTINGS: F1 (+ "Fn+F1 on Mac keyboards", ⌘,), Q/E tabs, the arrows, Esc,
    Backspace/Delete on the Controls page.
  - In MENU: the arrows, Space, Backspace, Return, Esc.
  - Actions still to be verified show a hatched outline and a "to verify" badge.
- **Controller:** an original, generic Xbox-layout drawing (not a trace of an 8BitDo or
  Microsoft product photo). It shows both sticks, the D-pad, ABXY, the bumpers, both triggers,
  View/Menu (Back/Start) and the paddles, which are greyed with a "not mapped" note.
  - A "8BitDo Ultimate 2 notes" disclosure: what ReCarbon sees through SDL3, and what is
    device-specific. It says "pending hand test" until M5's controller test passes.
  - Glyph names: Xbox letters with positional names in parentheses (South…), so PlayStation
    users can follow.
- **Interaction:**
  - hover or focus on a key or callout highlights the pair;
  - click or tap pins it and opens the detail sheet: the control, the default keys, the
    settings.toml name, the status badge and its evidence;
  - on touch, tap targets are ≥ 44 px (the keyboard zooms, see below).
- **Accessibility:**
  - each key and button is a `<button>` inside the SVG `<foreignObject>`, or the SVG uses
    `role="img"` with an equivalent HTML list beside it;
  - focus order follows the callout list, not the keyboard geometry;
  - `aria-pressed` on the mode tabs, and an `aria-live` region announcing the selection;
  - a full "Text table" toggle shows the §1 table for each mode;
  - colour is never the only cue (icons and badges too).
- **Mobile:** the keyboard shows by clusters, with a tab strip "Left hand / Arrows / Right
  hand / F-row". Each cluster is zoomed to fit the width, so there is no sideways scrolling of
  the whole board. A mini-map shows where the cluster sits. The controller fits at 360 px
  wide, with callouts as a list below it.

## 7. Documentation sections and sources

| Section | Source in repo | Gap |
|---|---|---|
| Installation (disc image → `ReCarbon --install <iso> --dest`, app first-run installer) | `runtime/ui/installer_screen.cpp`, FEASIBILITY "Packaging and memory" (install check b063ffa), `scripts/gen_install_manifest.py` | No end-user guide exists yet. M8 fresh-account install still pending |
| Download | `scripts/macos/make_app.py`, `check_app.py` | No release yet. Check 3 (`/opt/homebrew` strings in libvulkan) is a known issue. macOS 27+ only |
| Supported hardware | FEASIBILITY (MacBook Air M5 measured; M1 Air run: 57 fps mean, CPU ~216%) | Other Macs untested; state "measured on" only |
| Controls | §1, Garage | Game actions pending hand test 5 |
| Settings | `runtime/user/settings.cpp` option table (labels and descriptions) | Generate a settings reference from the same table (same generator approach) |
| Performance | FEASIBILITY §6c, ROADMAP M7 frame pacing | Only measured figures with dates. M7 10-minute criterion still pending |
| Troubleshooting | logs folder (`~/Library/Application Support/ReCarbon/logs`), Save Bug Report (`runtime/report/internal.h:16`) | Needs writing |
| Known issues | ROADMAP M7 "Known issues for the alpha" (1 s stalls, GPU-idle stalls, controller untested, check 3) | Keep in sync per release |
| FAQ | README disclaimer, AGENTS.md | Needs writing (legal copy, "why no download of the game") |
| Development | AGENTS.md (commands), ARCHITECTURE.md, ROADMAP.md | Public-facing version needed, without internal process notes |
| Credits | NOTICE, THIRD_PARTY_NOTICES.md, SpeedBreaker, XenonRecomp, Xenia (BSD), SDL3, MoltenVK, FFmpeg (LGPL) | Contributor list |
| Licenses | COPYING (GPL-3.0-or-later), `LICENSES/` (24 files) | Render them as pages, plus the LGPL source offer for FFmpeg |

- README "Status" is stale ("Nothing runs yet", M3). The site's status must come from ROADMAP,
  and the README should be refreshed separately (not part of this plan).
- Nothing on the site may link to or describe obtaining game files beyond "your own disc".

## 8. Repository layout (proposal)

`site/` in a public `recarbon-site` repo (D1):
- `src/pages/index.astro`, `garage.astro`
- `src/content/docs/**`
- `src/components/{Hero,Garage,Keyboard,Controller,Callout,StatusBadge}.astro`
- `src/data/*.json`, `src/styles/tokens.css`, `public/video/`, `public/posters/`
- `astro.config.mjs`, `.github/workflows/deploy.yml`

The generator stays in `recarbon/scripts/` and its JSON is copied over by a documented manual
step or a release script. The private repo never pushes to the public one automatically.

## 9. Phases and approval gates

| Phase | Output | Effort | Gate |
|---|---|---|---|
| 0. Decisions | D1–D8 answered | – | **User** |
| 1. Design mockup | Static HTML/SVG mockups of the landing page, the Garage (both devices, 3 modes) and a docs page, desktop and mobile, as local files (not published) | 1–2 days | **User approves the look** |
| 2. Controls prototype | Generator + JSON + drift check; Garage island working offline; a11y pass (keyboard-only, VoiceOver, axe) | 2–3 days | **User approves** |
| 3. Content | Docs sections from §7, settings reference generated, known issues synced | 3–4 days | **User reviews text** (legal copy especially) |
| 4. Footage | Capture session (isolated HOME), loop + poster encoded to §5.4 budgets | 1 day + capture | **User approves footage and D8** |
| 5. Deploy | Public site repo, Pages via Actions, domain | 0.5 day | **User approves publishing**, after M7/M8 allow |
| 6. Release versioning | `starlight-versions` snapshot at the first tagged alpha | 0.5 day/release | With each release |

None of this runs in parallel with a human acceptance test, and none changes runtime code.

## 10. Open decisions for the user

- **D1 Hosting:** a separate public `retronative/recarbon-site` repo (recommended), or Pages
  from a private repo on a paid plan (the site is still public). `recarbon` itself stays
  private.
- **D2 Domain:** a custom domain (e.g. under a RetroNative domain) or `*.github.io`.
- **D3 Branding:** existing RetroNative logo, colours and fonts, or this plan's palette.
- **D4 Unverified mappings:** show the game actions marked "to verify", or hide them until hand
  test 5 and the controller test report.
- **D5 Audio:** a muted loop only (recommended), or a sound trailer with licensed or original
  music.
- **D6 Custom bindings viewer:** paste a `[keys]` block to view it locally (client-side only),
  or defaults only.
- **D7 Light theme:** keep a light mode for docs readability, or dark only.
- **D8 Footage and legal:** whether to show gameplay footage at all (EA's copyrighted game),
  and the disclaimer wording.
- **Release versioning:** which version the first public docs carry (0.1 alpha?) and when the
  site goes live relative to M7/M8.

## Sources
- Starlight overrides reference: https://starlight.astro.build/reference/overrides/
- Starlight custom pages / `StarlightPage`: https://starlight.astro.build/guides/pages/
- Overriding components: https://starlight.astro.build/guides/overriding-components/
- Starlight plugins (starlight-versions): https://starlight.astro.build/resources/plugins/
- Astro on GitHub Pages: https://docs.astro.build/en/guides/deploy/github/
- Pages from private repos (community answer; check against GitHub's docs before deciding):
  https://github.com/orgs/community/discussions/22817
