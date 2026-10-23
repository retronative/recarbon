# M8 plan: packaging (B1) and input profiles (B2)

Planning only (2026-10-23). M8 is **Pending**: nothing here starts until the user approves it,
and no milestone scope or criterion changes. ROADMAP M8 is the scope; this says how.

## B1. Packaging: source-built libraries, `check_app.py` passing

### Where it stands
- **Groundwork:**
  - `scripts/macos/make_app.py` builds `ReCarbon.app`, bundling Homebrew's builds of SDL3, the
    Vulkan loader, glslang, SPIRV-Tools and MoltenVK (eda0ab1).
  - On a second Mac the app loads only its own libraries (417e0b8).
- **`check_app` check 3 fails** only on the Vulkan loader's compiled-in Homebrew search folders.
  Measured: they are implicit-layer folders, and nothing loads from them (3507f5b).
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
  - The LGPL source obligation for binaries (FFmpeg, libmspack) applies to any binary release.
- **Recommendation:** pin the versions in use now (column 2), the ones every ReCarbon test ran
  against, rather than SpeedBreaker's older ones.
  - Take each tarball from its project's official release URL.
  - Check its SHA-256 against Homebrew's formula, as a second source.

### Decisions needed
- **D-B1a, deployment target:** 27.0 (provisional alpha target, the user's decision).
  `build_deps.sh` makes a lower one possible later, but it can't be tested here: both Macs run
  macOS 27.
- **D-B1b, versions:** current (recommended) or SpeedBreaker's pins.
- **D-B1c, MoltenVK:** keep Homebrew's (already macOS 12, as SpeedBreaker does) or build it from
  source.
- **D-B1d, network:** `build_deps.sh` downloads about 60 MB of source tarballs.
- **Approval to start M8** with B1 first.

## B2. Input profiles: design note

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

### Decisions needed
- **D-B2a, preset tables:**
  - **WASD:** W gas, S brake/reverse, A/D steer, arrows on the D-pad.
  - **Arrows:** Up gas, Down brake, Left/Right steer, W/A/S/D free. The action keys stay as
    today (Space A, Backspace B, Left Shift X, Left Ctrl Y, Q/E LB/RB, Return Start, Esc Back).

  Which game actions sit on which Xbox controls (handbrake, nitrous, camera) is still
  unverified: hand test 5.
- **D-B2b, storage:** separate `profiles/*.toml` files (proposed), or tables inside
  `settings.toml`.
- **D-B2c, controller rebinding scope in v1:** buttons only (proposed), or axes too.
- **D-B2d, `[keys]` after migration:** a mirror of the active profile (proposed, safe to
  downgrade), or frozen.

### Order and size (if approved)
1. **B1:** about a day, mostly build-script porting and verification.
2. **B2:**
   - data types and baseline tests first (behaviour-preserving);
   - then storage and migration with their tests;
   - then the menu;
   - then presets and controller rebinding.

   Each step is its own commit with its tests. About 3–4 days.
