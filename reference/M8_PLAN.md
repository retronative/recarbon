# M8 plan: packaging (B1) and input profiles (B2)

**Status (2026-10-23):**
- **M8 started** by the user's approval.
- **B1 done:**
  - `build_deps.sh` (77c0bcc);
  - the MoltenVK pin (d780e34);
  - the release app passes `check_app` on both Macs (ROADMAP M8 evidence).
- **B2 implemented** (2026-10-24) with the approved tables: bddd7b3, 16a1f8e, 812c921, ab576ad,
  577a259 (ROADMAP M8 evidence).
  - **Refinement 1:** with `settings.toml` missing or unreadable, the active profile restores the
    keys (otherwise its keys would be replaced by the defaults).
  - **Refinement 2:** rebinding a controller button swaps it with the button that had it.

Planning only (2026-10-23). M8 is **Pending**: nothing here starts until the user approves it,
and no milestone scope or criterion changes. ROADMAP M8 is the scope; this says how.

## B1. Packaging: source-built libraries, `check_app.py` passing

### Where it stands
- **Groundwork:**
  - `scripts/macos/make_app.py` builds `ReCarbon.app`, bundling Homebrew's builds of SDL3, the
    Vulkan loader, glslang, SPIRV-Tools and MoltenVK (e3173ce).
  - On a second Mac the app loads only its own libraries (b063ffa).
- **`check_app` check 3 fails** only on the Vulkan loader's compiled-in Homebrew search folders.
  Measured: they are implicit-layer folders, and nothing loads from them (c0d9604).
- **Minimum macOS of what the app ships** (`otool -l`, minos): ReCarbon and every Homebrew
  library are 27.0; MoltenVK is 12.0.

### Plan
1. **Port SpeedBreaker's `scripts/macos/build_deps.sh`** (v0.1.8, 363 lines) with ReCarbon's
   names.
   - It downloads pinned source tarballs, checks each SHA-256 and builds with Apple's clang
     (`xcrun`) for one deployment target, into a prefix outside the repository. It fails on any
     unguarded-availability warning.
   - The Vulkan loader is configured with `CMAKE_INSTALL_SYSCONFDIR=/usr/local/etc`, so no
     `/opt/homebrew` folder is compiled in. That is check 3's fix.
   - It refuses a non-empty prefix it didn't make.
2. **`make_app.py --deps <prefix>`:** take the libraries from that prefix instead of Homebrew,
   and record each one's version, source URL and SHA-256 in `Contents/Resources/licenses/`.
3. **`check_app.py` unchanged:** check 3 stays strict and must pass on its own merits.
4. **Verify:**
   - `check_app` passes;
   - the second-Mac run, with `DYLD_PRINT_LIBRARIES` showing only bundle-internal images;
   - `VK_LOADER_DEBUG` showing no `/opt/homebrew` folder;
   - `scripts/install_check.sh` 21/21;
   - one `run_attempt.sh` race regression against today's Homebrew build;
   - ctest.
5. **Not in B1:** the M8.4 install on a fresh macOS account (the user creates the account), and
   any change of deployment target.

### Dependency versions (verified 2026-10-23)

| Library | In use now (Homebrew) | SpeedBreaker v0.1.8 pin | License | Source SHA-256 (Homebrew formula) |
|---|---|---|---|---|
| SDL3 | 3.4.18 | 3.4.16 | Zlib (HIDAPI, yuv2rgb: BSD-3-Clause) | `9c75cf16330322c2…` |
| Vulkan-Loader | 1.4.363.0 | 1.4.357.0 | Apache-2.0 (cJSON: MIT) | `941eff558fc74f24…` |
| Vulkan-Headers | 1.4.363.0 | 1.4.357.0 | Apache-2.0 or MIT | `4a078be12bef21cf…` |
| SPIRV-Tools | 1.4.363.0 | 1.4.357.0 | Apache-2.0 | `e6c83a215538fbfd…` |
| SPIRV-Headers | 1.4.363.0 | 1.4.357.0 | MIT | `5b3a0d099f5c7649…` |
| glslang | 16.6.0 | 16.6.0 (same SHA-256) | BSD-3-Clause, MIT, GPL-3.0-or-later parts, and others (its list) | `9c09b901149c729d…` |
| MoltenVK | 1.4.2 (Homebrew, minos 12.0) | not built (Homebrew's) | Apache-2.0 (cereal: BSD-3-Clause) | `6864db532f1dbbdb…` |
| FFmpeg | 7.1.1 (`setup_tools.sh` already builds it) | 7.1.1 | LGPL-2.1-or-later | in `setup_tools.sh` |

- **Licensing:**
  - All of these are compatible with ReCarbon's GPL-3.0-or-later. Each is already listed in
    `THIRD_PARTY_NOTICES.md` with its text in `LICENSES/`.
  - glslang's GPL-3.0 parts are compatible.
  - Source builds add no new component.
  - The LGPL source obligation (FFmpeg, libmspack) applies to any binary release; none is
    published.
- **Recommendation:** pin the versions in use now (column 2), the ones every ReCarbon test ran
  against, rather than SpeedBreaker's older ones.
  - Take each tarball from its project's official release URL.
  - Check its SHA-256 against Homebrew's formula, as a second source.

### Decisions (provisional, the user's, 2026-10-23)
- **D-B1a, deployment target:** macOS 27 or later.
- **D-B1b, versions:** Vulkan SDK 1.4.363 (Loader, Headers, SPIRV-Tools, SPIRV-Headers),
  SDL3 3.4.18 and glslang 16.6.0, the ones every ReCarbon test ran against.
- **D-B1d, sources:** release dependencies are built from source and checksum-verified where
  possible. About 60 MB of downloads is approved, from the verified upstream release URLs only.
- **Still open:**
  - **D-B1c, MoltenVK** (below);
  - **approval to start M8**, with B1 first.

### D-B1c: is a source-built MoltenVK needed? (checked 2026-10-23)
- **The bundled MoltenVK is already self-contained:**
  - `libMoltenVK.dylib` 1.4.2 is arm64 only, 5.3 MB, minos 12.0.
  - It links only system frameworks and libraries: Metal, QuartzCore, AppKit, Foundation,
    CoreGraphics, IOSurface, IOKit, CoreFoundation, libc++, libSystem, libobjc.
  - Nothing comes from Homebrew, so `check_app` has nothing to flag in it: check 3's failure is
    the Vulkan loader's alone.
- **Where Homebrew's comes from:**
  - The `molten-vk` formula builds the v1.4.2 tag with Xcode's `xcodebuild`, plus six
    dependencies at pinned git revisions: SPIRV-Cross, SPIRV-Headers, SPIRV-Tools,
    Vulkan-Headers, Vulkan-Tools and cereal.
  - The bottle (`arm64_golden_gate`, SHA-256 `0de19dbda466d4da…`) comes from Homebrew's CI.
- **What a source build would take:**
  - **Full Xcode:** neither Mac has it, only the Command Line Tools. Installing it is a large
    download and is the user's action.
  - **Git sources:** six repositories fetched at pinned revisions, not release tarballs. That's
    more than the approved 60 MB, and verified by commit rather than by a tarball SHA-256.
  - **Time:** about 10–20 minutes of `xcodebuild`.
  - **Not bit-for-bit reproducible:** the output also depends on the Xcode version.
- **What it would gain:** source provenance for one more library, and the deployment target set
  to 27 like the others. Neither is needed for a self-contained app or for `check_app`.
  - SpeedBreaker v0.1.8 makes the same choice: MoltenVK is the one library it takes from
    Homebrew.
  - Its license (Apache-2.0, cereal BSD-3-Clause) needs only the notices the app already
    carries; there's no source obligation.
- **Recommendation:** keep Homebrew's MoltenVK for the alpha.
  - `make_app.py` records the bottle's SHA-256 and the formula's version and dependency
    revisions in `Contents/Resources/licenses/README.txt`.
  - It refuses a MoltenVK whose SHA-256 differs from the recorded one, until the pin is updated
    on purpose.
  - Build it from source later, if the deployment target changes or Xcode becomes part of the
    toolchain.

## B2. Input profiles: design note

> **Changed 2026-10-24 (user decision):** the WASD preset is merged into Default, which keeps
> its keys (the column "Default (today)" below) and its name in code and in `keyboard.toml`,
> and is shown as **WASD** in Settings and on the website. A new player starts on it. The
> former WASD column is gone; Arrows is unchanged. "WASD" in a file names Default and stays
> reserved; a copy of Default is "WASD (custom)". A player whose `[keys]` were the former WASD
> keeps them as the custom profile "Custom (settings.toml)". The note below is the original
> design.

### Constraints
- **ROADMAP M8 scope:**
  - separate persistent keyboard and controller profiles in the user folder;
  - user-defined mappings, so the controller gains rebinding;
  - WASD and Arrows keyboard presets;
  - manual switching that takes effect without a restart;
  - reset to the M5 baseline;
  - migration of `[keys]` into the keyboard profile.
- **ARCHITECTURE §3.6:** a mapping is data, keyboard and controller mappings are separate types,
  there's no game logic in `hid/`, the defaults are the regression baseline, and today's
  rebinding keeps working.
- **M5.15:** bindings are scancodes, and rebinding refuses non-ASCII key names.
- **Physical controller: Deferred — not verified.** Controller rebinding can only be tested with
  the virtual pad, which is not proof of physical compatibility.

### Design (minimal)
- **Data:**
  - `hid::KeyboardMapping`, the 24 `keymap::Control`s × 2 scancodes, which is what `[keys]`
    holds today.
  - `hid::PadMapping`, the 14 `padmap::Entry` pairs, SDL button → XInput, with the
    trigger/stick axes fixed in v1.
  - Both are plain values compared with `==`, so they're testable without SDL.
- **Storage:**
  - Files: `<user folder>/profiles/keyboard.toml` and `controller.toml`, each holding a list of
    named profiles and the active one's name.
  - Built-in profiles ("Default", "WASD", "Arrows") are never written: they're constants, and
    "Default" is today's tables.
  - The files are read at startup and written on change, the way `settings.cpp` writes
    `settings.toml` today.
- **Migration:**
  - The first launch with no `keyboard.toml` creates it with one profile, "Custom", from the
    current `[keys]` table, byte-for-byte the same scancodes.
  - It's active only if `[keys]` differs from the defaults; otherwise "Default" is active.
  - `[keys]` stays in `settings.toml` and is never deleted. After migration the keyboard profile
    is the source and `[keys]` mirrors the active profile, so a downgrade still finds its keys.
- **Switching:**
  - Settings > Controls gains a profile row (choose, duplicate, reset).
  - The binding layer reads the active mapping as a value each frame, the seam §3.6 calls for:
    no restart.
  - The game's input path otherwise stays the same.
- **Rebinding:**
  - Edits always go to a user profile. Editing a built-in one first duplicates it as
    "<name> (custom)".
  - Controller rebinding is per button, through the same capture (SDL gamepad button down), with
    Back+Start and B kept as menu keys.
  - Keyboard capture keeps M5.15's ASCII rule.
- **Reset to default:** restores `padmap::kDefault` and the settings key defaults. The baseline
  tests (`pad_map_test`, `settings_keys_test`, `key_map_test`) must pass unchanged.

### Tests (all unit tests, no SDL, plus scripted runs)
- **Profile files:** load, save, round-trip, malformed files (named in the log, then defaults),
  unknown keys ignored.
- **Migration:**
  - a customized `[keys]` gives the same bindings;
  - the default `[keys]` gives "Default" active;
  - a second launch doesn't migrate again.
- **Presets:** each preset is a fixed table, pinned by tests.
- **Switching without a restart:** the binding layer gets the new mapping on the next frame.
- **Reset:** equals the M5 baseline.
- **Scripted:**
  - `RECARBON_UI_KEYS` drives switching and rebinding, with screenshots;
  - a restart shows persistence, in isolated homes;
  - the virtual pad covers controller rebinding.

### Proposals for approval (exact)

#### D-B2a, preset tables (revised 2026-10-23, for approval)
- **The rule:** within a preset no key drives two controls, so gas/brake, steering and the D-pad
  never share a key. Nothing here assumes what Carbon does with the D-pad or the left stick's
  up/down while driving (unverified until hand test 5).
- **Default** stays today's table (`settings.cpp`, pinned by `settings_keys_test`), unchanged:
  it is M5's baseline, and in it Up/Down and Left/Right drive two controls each.
- **Notation:** each cell is slot 1. Slot 2 is "none" in both presets.
- **Same in all three tables:**
  - A space, B backspace, X left_shift, Y left_ctrl, LB q, RB e, Start return, Back escape;
  - the right stick: up o, down l, left k, right semicolon;
  - the stick clicks: left z, right c.

| Control | Default (today) | WASD | Arrows |
|---|---|---|---|
| Right Trigger (gas) | w | w | up |
| Left Trigger (brake/reverse) | s | s | down |
| Left Stick Left (steer) | a, left | a | left |
| Left Stick Right (steer) | d, right | d | right |
| Left Stick Up | up | none | none |
| Left Stick Down | down | none | none |
| D-pad Up | up | up | w |
| D-pad Down | down | down | s |
| D-pad Left | left | left | a |
| D-pad Right | right | right | d |

- **Checked mechanically:** each preset binds 22 keys to 24 controls, and no key twice.
- **Menus:** in both presets the menus answer the D-pad (WASD: the arrows; Arrows: W A S D) and,
  sideways, the steering keys. The left stick's up/down is unbound; whether some Carbon screen
  needs it is to be seen in testing.
- **Tests:** pinned by a test like `settings_keys_test`, including the no-duplicate rule.

#### D-B2b, profile storage
- **Files:** two TOML files in the user folder, next to `settings.toml`:
  `profiles/keyboard.toml` and `profiles/controller.toml`.

  ```toml
  # profiles/keyboard.toml: written by ReCarbon. The built-in profiles (Default, WASD,
  # Arrows) aren't stored.
  active = "Custom"

  [[profile]]
  name = "Custom"
  right_trigger = ["w", "none"]      # [slot 1, slot 2], the [keys] tokens
  left_stick_left = ["a", "left"]
  # ... one line per control; a control left out takes Default's keys

  # profiles/controller.toml
  active = "Default"

  [[profile]]
  name = "Swapped A/B"
  a = "east"                         # Xbox control = SDL gamepad button
  b = "south"
  ```

- **Names:**
  - Profile names are 1–32 printable ASCII characters.
  - "Default", "WASD" and "Arrows" are reserved.
  - A built-in profile is edited by duplicating it first, as "<name> (custom)".
- **Bad entries:** unknown keys and malformed entries are named in the log and skipped (Default's
  keys for that control).
- **A file that can't be read** at all is left untouched, and the game runs on `[keys]`. The
  Controls page says so.

#### D-B2d, migration (keeps every existing binding)
1. **First launch of the M8 build, with no `profiles/keyboard.toml`:**
   - If `[keys]` equals Default, write `active = "Default"` with no profiles.
   - Otherwise write one profile, "Custom", holding `[keys]` exactly: both slots of every
     control, the same scancodes. Then `active = "Custom"`.
   - Log a line either way.
2. **`[keys]` is never deleted.** From then on it mirrors the active keyboard profile whenever
   that changes, so an older build still finds the keys in use. `settings.toml`'s other settings
   are untouched.
3. **Once `keyboard.toml` exists it is the source.** A hand edit to `[keys]` is then logged and
   overwritten by the mirror.
4. **No controller migration:** there is no controller setting to migrate. `controller.toml`
   starts with `active = "Default"`.
5. **Tests:**
   - a customized `[keys]` gives the same bindings;
   - the default `[keys]` gives "Default";
   - a second launch doesn't migrate again;
   - an unreadable `keyboard.toml` leaves `[keys]` in charge;
   - every case runs on copies in isolated homes.

#### D-B2c, controller rebinding scope (v1)
- **Buttons only:** the 14 XInput buttons of `padmap::kDefault` (A, B, X, Y, LB, RB, Back,
  Start, the stick clicks, the D-pad's four), each bound to one SDL gamepad button.
- **Fixed in v1:** the triggers, the sticks, the deadzones (Settings > Input) and the paddles,
  which SDL reads but `pad_map.h` doesn't map.
- **Menu rules stay physical:** opening the menu (Back + Start) and closing it (B) read the real
  buttons whatever the mapping, so no profile can lock the player out.
- **Testing:** by the virtual pad only, while the physical controller is Deferred — not
  verified. Nothing about controller rebinding is called verified on hardware.

### Control actions: evidence and labels (audit, 2026-10-24)
ReCarbon's input layer maps keys and buttons to the Xbox 360 controls. What the game does with
each control is Carbon's own code, so an action counts as known only with evidence from play.

| Player action | Xbox control (`[keys]` row) | Default keys | Evidence | Label in Settings |
|---|---|---|---|---|
| Throttle | Right Trigger (`right_trigger`) | W | Driven by the user on the keyboard (hand test 1), and in free-roam and a won career race (hand tests 3, 4) | **Throttle** |
| Brake | Left Trigger (`left_trigger`) | S | As above, for braking. Reversing isn't recorded. | **Brake** (not "Brake / Reverse" until reversing is seen) |
| Steering | Left Stick left/right (`left_stick_left`, `left_stick_right`) | A / D, ← / → | As above | **Steering Left**, **Steering Right** |
| Handbrake, Nitrous, Speedbreaker, Change Camera, Look Back | not known | — | None: no source, test or play evidence ties them to a control. SpeedBreaker's or NFSU2's layouts aren't evidence for Carbon. | Not used; the rows keep their Xbox names |

- **Changed** (0dc8bf1): only the labels, with the Xbox name and config key moved to the
  detail line. No binding changed, and the M5 key defaults in `settings_keys_test` are
  unchanged; only its label column was updated.
- **Website (Garage):** its `controls.actions.json` should follow the same rule:
  - "Gas" becomes "Throttle";
  - "Brake / reverse" becomes "Brake";
  - "Steer left/right" becomes "Steering Left/Right";
  - every other action stays unverified until hand test 5 maps them.

  The website repository isn't changed from here; its session applies this.

### Control actions: virtual-controller gameplay check (2026-10-24)
Scripted runs of the unchanged 8ad356e build (b5993d0 before the history rewrite, as its logs
name it) on the M1 worker, each in its own isolated home (fresh profile, no save), with SDL's
virtual Xbox 360 controller (`RECARBON_VIRTUAL_PAD`, `RECARBON_INPUT_LOG=1`).
- **Route:** title, "Create a new alias?" No, the Xbox Live notice, Main Menu, Quick Race
  (Circuit, default track), stock RX-8, Manual transmission, race.
- **Method:** every button and stick was swept blind; nothing was taken from another game's
  layout. An action counts only when a screenshot shows it against a frame of the same run just
  before the input. With no input after GO the chase camera stayed for at least 9 s, so the
  camera changes below aren't an idle camera.
- **Two tiers:**
  - Delivery: the step log shows the XInput state the game got, for every button and axis
    except BACK. BACK is held back 250 ms for the Back + Start chord, so its step lines read
    `0x0000`.
  - Gameplay: Carbon visibly did the action.

| Player action | Xbox control | Observed (paired frames) | Status |
|---|---|---|---|
| Change Camera | RB | Each tap moves to the next camera, kept after release: near chase, far chase, bumper, hood, near chase | Automated |
| Look Back | LB (held) | View faces backward while held, normal on release | Automated |
| Gear Up | Right stick up | HUD gear 1 > 2, and 1 > 2 > 3 (manual transmission) | Automated |
| Gear Down | Right stick down | HUD gear 2 > 1, 3 > 2, 1 > N (manual transmission) | Automated |
| Reverse | Left Trigger (held at a standstill) | HUD gear R, speed 0 > 7 > 25 mph, reverse lights | Automated (braking: hand test) |
| Pause | Start (in a race) | PAUSE menu opens | Automated |
| Menus | A, B, D-pad left/right/down | A accepts each route step; B closes PAUSE; RIGHT and LEFT move the main menu and PAUSE highlight; DOWN moves Automatic to Manual | Automated |
| Nitrous | — | The nitrous gauge was empty in this race, so nothing could show it | Unverified |
| Speedbreaker | — | Its gauge stayed full while every button and stick was held | Unverified |
| Handbrake | — | No input slowed a 1 s coast from about 18 mph more than coasting alone (right stick left and BACK tried twice) | Unverified |

- **No visible effect in a race:** Y, X, A, B, the stick clicks, the D-pad, BACK, the right
  stick's left and right, and the left stick's up and down. They stay unverified for a hand test.
- **Not a hand test:** none of this is a physical controller, and the conditions were one Quick
  Race below about 35 mph.
- **Not changed:** no binding and no runtime code.
- **Evidence:** screenshots and logs are in the website session's handoff
  (`recarbon-handoff/evidence/`), not in this repository; they show EA's game.
- **An observation, rechecked and not reproduced:** "Yes" at the boot alias prompt left the game
  on "Please don't turn off…" for over 70 s in 2 runs, with no save written. The evidence wasn't
  kept. The recheck (2026-10-24) used a fresh isolated home and the virtual controller, on the
  M5 (this build) and on the M1 (the 8ad356e build; the runtime is unchanged since). It took
  "Yes", A on the Autosave notice and on "Please don't turn off your Xbox 360 console while the
  Autosave icon is displayed", a name, START (Done), then "Yes" to "Do you want to enable
  Autosave, and save your alias now?". "Saving AA…" wrote `rmcsave:\ALIAS_AA` (286,764 B plus
  308 B `.xcontent`, as in 6b), and the main menu and Career followed on both Macs.
  - Only A dismisses the notices: START did nothing there. "No" to the Autosave question saves
    nothing ("The Autosave feature has been disabled").
  - The original runs' cause isn't known. One recheck run shows the same symptom when the taps
    never reach the game: a zsh-built script (`$t:A` is zsh's absolute-path modifier) sent no A
    after the first, and the notice stayed up.

### Check for Updates (optional polish, not a release gate; 73b4c26)
- **When it runs:** only when the player presses Settings > Advanced > Check for Updates
  (`update_dialog.cpp`). No polling, no download, no install.
- **What it sends:** one GET to GitHub's release list
  (`/repos/retronative/recarbon/releases?per_page=30`), with a User-Agent and an Accept header,
  and nothing about the player or the device.
- **What it offers:** the newest non-draft release by semver. A pre-release (ReCarbon's alphas)
  is offered only to a pre-release or 0.x build, so a 1.x build gets releases only.
  `/releases/latest` was dropped because it never lists a pre-release.
- **What it shows:** the version, the release notes, and a link to the release's page on GitHub
  (checked to be this project's).
- **Errors:** offline, a timeout, TLS failures, rate limits (with the reset time) and API errors
  each give one line.
  - A 404 (the repository is private) or an empty list says "ReCarbon has no public releases
    yet." rather than reporting a failure.
- **Tests:** SpeedBreaker's `update_check_test`, ported and extended (list parsing, the
  pre-release rule, no releases). Both mutants of the rule are caught.

### UI design reference (user, 2026-10-23; a reference, not an implementation gate)
- **Website reference:** the website's Garage keyboard and controller layouts are the visual
  reference for the profile and rebinding UI. Reuse the concepts:
  - a generic controller diagram;
  - clear button labels and short callouts;
  - the keyboard mapping layout.

  The website repository (on the M1) is only read, never changed.
- **Style:**
  - Dear ImGui drawing only: no web technology embedded.
  - The website's racing look, kept consistent with the in-game menu.
  - Controller artwork brand-neutral: no 8BitDo branding.
  - Xbox/PlayStation label switching is optional polish, not a compatibility claim.
- **Source of truth:** the runtime's mapping definitions (`settings.cpp`, `pad_map.h`,
  `key_map.h`), with no duplicated mapping logic. No runtime mapping changes to match the
  website.
- **Order:** B2's function, migration safety, presets and tests come before any visual polish.
  This adds no M8 acceptance criterion.

### Order and size (if approved)
1. **B1:** about a day, mostly build-script porting and verification.
2. **B2:**
   - data types and baseline tests first (behaviour-preserving);
   - then storage and migration with their tests;
   - then the menu;
   - then presets and controller rebinding.

   Each step is its own commit with its tests. About 3–4 days.
