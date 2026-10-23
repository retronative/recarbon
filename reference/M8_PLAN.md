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

#### D-B2a, preset tables
- **Rows and names:** each row is a `keymap::Control` with its two keys (slot 1, slot 2), named by
  the `settings.toml` tokens of `user/key_names.h`.
- **Default:** today's table (`settings.cpp`), pinned by `settings_keys_test`. Rows not listed
  below are the same as Default in both presets:
  - A space, B backspace, X left_shift, Y left_ctrl, LB q, RB e, Start return, Back escape;
  - the right stick o, l, k, semicolon;
  - the stick clicks z, c.

| Control | Default (today) | WASD | Arrows |
|---|---|---|---|
| Right Trigger | w, none | w, none | up, none |
| Left Trigger | s, none | s, none | down, none |
| Left Stick Left | a, left | a, none | left, none |
| Left Stick Right | d, right | d, none | right, none |
| Left Stick Up | up, none | up, none | up, none |
| Left Stick Down | down, none | down, none | down, none |
| D-pad Up / Down / Left / Right | up / down / left / right | up / down / left / right | up / down / left / right |

- **WASD:** Default with the arrows taken off steering. Arrows are then menu and D-pad keys only.
- **Arrows:** gas and brake on Up and Down, steering on Left and Right; W, A, S and D are free.
  - Up and Down still move the left stick and the D-pad too, as Left and Right already do in
    Default (several controls on one key are allowed, and the help line says which).
  - **Unverified:** what Carbon does with D-pad Up or Down, or a pushed left stick, while
    driving. Hand test 5 maps the game's actions. The Arrows preset can't be called verified
    before that.
- Both presets are fixed tables, pinned by a test like `settings_keys_test`.

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

### Order and size (if approved)
1. **B1:** about a day, mostly build-script porting and verification.
2. **B2:**
   - data types and baseline tests first (behaviour-preserving);
   - then storage and migration with their tests;
   - then the menu;
   - then presets and controller rebinding.

   Each step is its own commit with its tests. About 3–4 days.
