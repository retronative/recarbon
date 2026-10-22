# ReCarbon — Roadmap (Xbox 360 → Apple Silicon)

Every milestone is gated. Don't start the next one until the acceptance criteria are met **and
observed**, not inferred. The "Evidence" line in each milestone is what goes into the milestone's PR description.

## M0 — Correct input binary *(done 2026-10-22)*
- **Evidence:** FEASIBILITY §1.
  - Xbox 360 XEX2, title 0x454107EC, SHA-256 `947f95c7…c721e1`.
  - `xexsurvey` produced a 17,629,184 B image.
  - All 8 helper addresses found.
  - 369 imports, 11 of them new versus MW.
- **Done locally (git-ignored, M1 can reuse):**
  - XenonRecomp at `ddd128bc` plus SpeedBreaker patches 0001–0008, built in `tools/XenonRecomp`.
  - `xexsurvey` built in `tools/survey/`.
- **Do:**
  - Obtain your own Xbox 360 NFS: Carbon (USA) disc image.
  - Extract it to `../rom_extracted/` (or `game/files`).
  - Run XEX inspection, using SpeedBreaker's `survey/xexsurvey.cpp` built against XenonRecomp.
- **Accept:**
  - `default.xex` is present.
  - Header parsed: title ID, version, base, image size, sections, import libraries and counts, TLS, stack size.
  - Hashes recorded in `reference/FEASIBILITY.md`.
  - The XEX decrypts and decompresses via `Xex2LoadImage` from the player's own disc.

## M1 — Toolchain + recompile-only PoC *(done 2026-10-22)*
- **Evidence** (FEASIBILITY §3):
  - `scripts/setup_tools.sh` then `scripts/survey.sh`, run from scratch twice. The config and
    `ppc/` were byte-identical across runs.
  - **Output:** 72,864 functions, 289 files, 181 MB.
  - **Recompiler log:** 0 errors, warnings or unrecognized instructions.
  - **Structural validation** (`survey/validate_recomp.py`): all HARD checks pass.
    - All 30,984 `.pdata` functions are present.
    - Mapping and definitions match one-to-one.
    - 0 failure markers.
    - Every non-zero code byte is translated except 576 B of SEH data headers.
  - **Instruction tests** (`tests/run_instr_tests.sh`): 242 tests, 0 failures. That includes
    15 hand-computed ReCarbon tests for `eqv`, `bdnzt` and `dcbst`, and a mutation check
    proved failures are caught.
- **Do:**
  - Import SpeedBreaker's `scripts/setup_tools.sh` and `patches/xenonrecomp/`.
  - Build XenonRecomp and XenonAnalyse.
  - Parametrize the `survey/` scripts from the XEX section table.
  - Generate `config/recarbon.toml` and the switch tables.
  - Run XenonRecomp.
- **Accept:**
  - XenonRecomp completes on the Carbon XEX with **zero** unhandled instructions.
  - Counts are recorded for: functions, switch tables, "switch case outside function" errors, and secondary entries.
  - Structural validation passes.
  - `ppc/` is git-ignored and `git status` is clean of generated code.

## M2 — Native compilation and link
- **Do:**
  - Compile `ppc/*.cpp` on arm64 with Homebrew clang.
  - Link against an unmodified copy of SpeedBreaker's runtime with the game layer removed.
  - Generate `import_stubs.cpp`.
- **Accept:**
  - A `ReCarbon` arm64 Mach-O builds with `-Werror` settings unchanged.
  - The build time and peak RSS on the target Mac are recorded.
  - The list of unimplemented imports is recorded.

## M3 — Runtime initialization
- **Do:**
  - Load the XEX image into the 4 GB guest space.
  - Wire the imports.
  - Run the guest entry point with `STUB_CONTINUE` off.
- **Accept:**
  - Guest code reaches `main`, opens files through the VFS (logged), and spawns guest threads.
  - The first unimplemented import or crash is diagnosed, with no silent stubs.
  - No host crash for at least 60 s, or a root-caused failure.

## M4 — First frame (rendering)
- **Do:**
  - Enable the PM4/Xenos path at 1x, 16:9, with the MW-only renderer features disabled.
  - Add logging for unknown texture formats and dropped memexport draws.
- **Accept:**
  - The EA/Black Box logos or the first front-end frame are presented through MoltenVK.
  - A screenshot is captured locally (never committed).
  - Unknown-format and dropped-draw counts are recorded.

## M5 — Audio, movies, input
- **Do:**
  - Enable XMA → FFmpeg → SDL3.
  - Verify the movie format (VP6/XMV) and add pacing hooks.
  - Port the controller and keyboard input with a Carbon key map.
- **Accept:**
  - Front-end music plays without stalls for 5 minutes.
  - The intro movie plays at the correct cadence.
  - Menus are navigable with a controller and with the keyboard.

## M6 — First boot to free-roam / race load
- **Do:**
  - Implement the missing kernel/XAM calls: content, saves and profile, scoped per title.
  - Find Carbon's render-queue and frame-time equivalents and add the ARM fence hooks.
- **Accept:**
  - A race loads from a new profile, and the save is created and reloaded across restarts.
  - No deadlock for 10 minutes in free-roam.

## M7 — Playable racing
- **Accept:**
  - One full career race completes at a stable 30 fps or better on the MacBook Air M5, measured with frame-time logs over 10 minutes and thermal state noted.
  - No missing world geometry.
  - Audio stays in sync.

## M8 — Polish and distribution
- **Do:**
  - Parametrize the installer: Carbon manifest, title ID, ReCarbon names.
  - Package `ReCarbon.app` (bundle ID `io.github.thixpin.recarbon`, via `make_app.py`) with license notices and LGPL sources.
  - Optional: render scaling and widescreen hooks, reintroduced per Carbon render target.
- **Accept:**
  - A clean install from a disc image on a fresh macOS user account.
  - `check_app.py` passes.
  - The unit tests run in `ctest`.

## Not planned
- Any original-Xbox (XBE) port. That is out of scope unless re-approved (see ARCHITECTURE appendix).
- Wine/CrossOver or runtime CPU emulation.
