# ReCarbon — Feasibility of a Native Apple Silicon Port of Need for Speed: Carbon

Date: 2026-10-22. Host: arm64, macOS 27.0.1. Reference: SpeedBreaker v0.1.7 (`31849f5`).

## TL;DR

| Question | Answer |
|---|---|
| Is the input the Xbox 360 release? | **Yes.** `default.xex` is XEX2, title **0x454107EC**, PowerPC, retail-encrypted and LZX-compressed. It decrypts and decompresses with XenonRecomp's loader (§1). |
| Does it fit SpeedBreaker's pipeline? | **Yes, with no blockers found in M0.** All 8 compiler-helper patterns match, and every code section is covered by `.pdata`. 195 of 369 imports are already handled; 163 are imports MW also stubs, and only 11 are new. The original PE name is `NfsMWRelease.pe`, the same codebase lineage as MW. |
| Feasibility | **Conditionally feasible, medium risk.** Carbon has about 1.37× MW's code. The game-specific layer (hooks, widescreen, render queue) must be re-found. |
| Recompilation (M1) | **Complete.** The whole executable translates: 72,864 functions, 0 recompiler errors or warnings, 0 failure markers, all structural checks pass, and the output is reproducible byte for byte (§3). |
| Native build (M2) | **Complete.** All generated code compiles for arm64 and links with the adapted SpeedBreaker runtime into a 59.6 MB `ReCarbon` executable, with 0 unresolved symbols, in 98 s from clean on the MacBook Air M5 (§4). **Not run yet.** |
| Runtime (M3) | **Passed** through C5: the native executable boots Carbon, runs its kernel/CRT init, opens its data, brings up the GPU and presents frames: **10/10** consecutive windowed launches on the committed code (final1–10), after 10/10 on the ring-fix build (w01–w10). The game's own **loading screen** renders, verified by frame dumps in 3 of 3 runs. Movie video is still black (§5). |
| Rendering (M4) | **In progress.** Movies and the EA logos now render (EDRAM format aliasing fixed), verified in game-only frame dumps. Guest system time is monotonic (flaky test root-caused). Open: an intermittent movie-teardown hang, about 1 launch in 50 (§6). |

## 1. Disc and executable (evidence)

The player stated that this image is a backup of their own disc. An earlier image turned out to be the original-Xbox release; it is recorded in Appendix A.

### ISO
| Property | Value |
|---|---|
| File | `../Need for Speed - Carbon (USA) (En,Es).iso`, 7,834,892,288 bytes. Read-only: left unmodified. |
| SHA-256 | `6e5fbdca502880af48310e905c34dd4418a7950f4da252c10f12dd358ebf0b5a` |
| MD5 | `a2579fe908c6bdfb6613c37c497aed65` |
| Layout | Redump-style **XGD2**: a UDF 1.5 video partition labelled `XBOX360` at offset 0 (`file`), and XDVDFS `MICROSOFT*XBOX*MEDIA` at **0xFD90000 + 0x10000**. |

### Extraction
- Tool: `XboxDev/extract-xiso` @ `3f5b62c` (`tools/`, git-ignored).
- Command: `extract-xiso -d ../rom_extracted -x <iso>`.
- Result: "79 files … total 5741677952 bytes".
- Contents:
  - `default.xex` (4,861,952 B);
  - `$SystemUpdate/su20076000_00000000` (3,100,672 B, a console update, not needed);
  - `Movies/` with 65 `.wmv` files (English NTSC/PAL, Spanish EA bumpers, EA Sports trailers);
  - `NFS/ZDIR.BIN` plus `NFS/ZZDATA0…10.BIN`. `ZZDATA10.BIN` is 0 bytes on the disc.

### Executable: `default.xex`
| Field | Value |
|---|---|
| SHA-256 | `947f95c7306eabf5bc72972938df57a922798bafbc02edf1a9107f0462c721e1` |
| MD5 | `289fd5fb0483c24b65cc406cc8ebaa96` |
| Format | `XEX2`, module flags 0x1 (title). Encryption: normal (retail AES). Compression: normal (LZX). |
| Title ID | **0x454107EC** (EA-2028). MW is 0x454107D9. |
| Media ID / version / disc | 0x51D2B53F / 0.0.0.1 (base 0.0.0.1) / disc 1 of 1 |
| Region / allowed media | 0x000000FF (all regions) / 0x4 |
| Original PE name / timestamp | **`NfsMWRelease.pe`** / 2006-10-07 23:50:35 UTC |
| Image base / size / entry | 0x82000000 / 0x010D0000 (17.0 MB) / **0x829446E0** |
| Stack / TLS | 256 KB default stack / 64 TLS slots, 8 B TLS data at 0x8301E000 |
| XDK (static libraries) | **2.0.4025**: `D3DX9 XGRAPHC LIBCMT XMP XMEDIA XONLINE XFFB XAPILIB XBOXKRNL XAUD XHV D3D9LTCG` |
| Imports | `xboxkrnl.exe` 182 functions + 14 data; `xam.xex` 173 functions. 369 unique names. |

#### Sections (from XenonRecomp's loader, `xexsurvey`)
| Section | Range | Size | Kind |
|---|---|---|---|
| `.rdata` | 0x82000600–0x820C927C | 803 KB | data |
| `.pdata` | 0x820C9400–0x82105C40 | 242 KB, 30,984 entries | data |
| `.text` | 0x82110000–0x82AADB04 | 9.62 MB | code |
| 8 × `.embsec_*` | 0x82AADC00–0x82B35C64 | 545 KB total | code |
| `.data` | 0x82B40000–0x8301DDF0 | 4.87 MB | data |
| `.XBMOVIE`, `.tls`, `.idata`, `.XBLD`, `.reloc` | 0x8301DE00–0x830CAF3C | — | data |

#### Verification for M0
- **Decrypt and decompress:** `xexsurvey` (SpeedBreaker `survey/xexsurvey.cpp`, built against patched XenonRecomp) wrote `game/survey/image.bin`. It is 17,629,184 B, exactly the image size. It is git-ignored.
- **Compiler helpers:** all 8 found, one byte-pattern hit each:

  | Helper | Address |
  |---|---|
  | `savegprlr_14` | 0x8279C170 |
  | `restgprlr_14` | 0x8279C1C0 |
  | `savefpr_14` | 0x8279C790 |
  | `restfpr_14` | 0x8279C7DC |
  | `savevmx_14` | 0x8279E340 |
  | `restvmx_14` | 0x8279E5D8 |
  | `savevmx_64` | 0x8279E3D4 |
  | `restvmx_64` | 0x8279E66C |

- **`.embsec_*` sections** (names contain non-ASCII bytes) are **plain code**, not encrypted:
  - entropy is 6.0–6.7 bits/byte, against 6.44 for `.text`;
  - they contain normal `mflr r12`/`blr` prologues and returns;
  - `embsec2` is 42 % VMX instructions.

  `.pdata` covers 86–100 % of each section (256 functions in total). XenonRecomp translates every section flagged as code (`XenonRecomp/recompiler.cpp:198`). Their origin is not established; XDK library code is the guess (inferred).
- **`.pdata` coverage of `.text`:** 30,729 functions, 83.5 % of bytes. For MW, SpeedBreaker's own `.pdata` range (`survey/classify_switches.py:12`) gives about 22,680 entries, and MW's `.text` is 7.9 MB.
- **Import coverage** against SpeedBreaker's runtime. Real implementations are 202 names (`runtime/**`, excluding the generated `import_stubs.cpp`), plus data exports in `kernel/module.cpp`.
  - 195 of 369 Carbon imports are handled.
  - 163 are unhandled but MW imports them too; SpeedBreaker ships them as logged stubs (mostly `NetDll_*`/XNet/XOnline, crypto, UI).
  - **11 are new:** `ExRegisterTitleTerminateNotification`, `ObCreateSymbolicLink`, `ObDeleteSymbolicLink`, `XCustomSetAction`, `XamContentResolve`, `XboxKrnlVersion` (data), `XexGetModuleSection`, `XexGetProcedureAddress`, `XexLoadImage`, `XexUnloadImage`, `XexUnloadImageAndExitThread`.

  `XexLoadImage`/`XexGetProcedureAddress` suggest a dynamic module load. No other `.xex` is on the disc, so this may be an XDK path that is never taken (to verify).

Scripts: `reference/analysis/xexinfo.py` (plaintext header fields, no decryption) and `reference/analysis/xbeinfo.py`.

### Data formats
- `ZDIR.BIN` + `ZZDATA*.BIN` form EA's packed-file archive, as on MW's disc.
- Movies are **WMV**, played by the XDK's XMV player (`XMEDIA` is linked, and there is a `.XBMOVIE` section). This matches MW, whose movies SpeedBreaker plays with only host-side pacing hooks.

## 2. SpeedBreaker audit (source evidence)

Paths are relative to `../speedbreaker/`.

### 2.1 Recompilation
- **Toolchain:** `scripts/setup_tools.sh` pins XenonRecomp `ddd128bc` and applies 8 patches. These patches are generic:
  - extra PPC/VMX ops;
  - arm64 FPCR width;
  - `secondary_entries`;
  - u32 switch index;
  - barriers become fences;
  - `vpkd3d128`/`vupkd3d128`/`vcmpbfp` semantics;
  - a 49.875 MHz `mftb`.
- **Per-binary config** (`config/nfsmw.toml`): CRT save/restore helper addresses, `setjmp`/`longjmp`, `secondary_entries`, leaf `functions`, 13 `[[midasm_hook]]` entries, and the switch-table TOMLs.
- **Regeneration:** most of it is regenerable by `survey/` scripts (`xexsurvey.cpp`, `fix_switch_tables.py`, `find_missing_tables.py`, `leaf_switch_bounds.py`, `find_entry_points.py`). Two things are not:
  - MW `.text`/`.pdata` ranges are hard-coded in `survey/classify_switches.py:4-15` and `fix_switch_tables.py:33`;
  - the midasm hooks are hand reverse engineering.
- **Imports:** `scripts/gen_import_stubs.py` produces logged stubs for every unimplemented import. That is 168 for MW, out of 353 imports (`runtime/kernel/import_stubs.cpp`).

### 2.2 Kernel, memory, threads (generic, Unleashed/Xenia-derived)
- **Guest memory:** a 4 GB guest reservation (`kernel/memory.cpp:124`).
- **Physical memory:** 512 MB mirrored at 0xA/0xC/0xE0000000 via `mach_make_memory_entry_64` on Apple. Handles 16K pages (`memory.cpp:59-68`).
- **Heaps:** Xenia-style heaps (`kernel/vmem.cpp:207-209`) and o1heap.
- **Write-watch:** physical-memory write-watch for GPU invalidation (`kernel/write_watch.*`).
- **Threads:** pthread per guest thread, with PCR/TLS/TEB (`cpu/guest_thread.cpp`). A single dispatcher for waitables.
- **Files:** VFS maps `game:`/`d:` read-only.
- **Single-title assumptions:**
  - English/USA hard-coded (`kernel/imports_unleashed.cpp:94,147`);
  - fixed XUID;
  - unscoped save paths (`user/paths.h`);
  - `NFSMW_*` env vars.

### 2.3 GPU / video (generic Xenos emulation, Xenia-style)
- **Pipeline:** PM4 command processor (`gpu/command_processor.*`); ucode → GLSL 4.60 → SPIR-V via glslang. EDRAM targets and compute resolves.
- **Occlusion and present:** ZPD occlusion queries. Vulkan 1.2 on MoltenVK 1.4.2 with Metal-specific workarounds (`video/presenter.cpp`, `gpu/shader_translator.cpp:106-109`).
- **MW-specific parts:**
  - ultrawide/Vert+ front-end detection (`gpu/renderer.cpp:7815-8450`, fetch const 95, guest 0x82445DC8);
  - `gpu/edge_bars.h`;
  - render-scale pitch window 640–1280 (`renderer.cpp:1849-1869`);
  - a 640×720 resolve fix (`renderer.cpp:4700-4715`).
- **Gaps that matter for a new title:**
  - texture formats CTX1, DXT3A/5A and `_AS_16`/`_EXPAND` are missing (`renderer.cpp:2988-3013`);
  - memexport-only draws are dropped (`renderer.cpp:7088`);
  - no EDRAM format aliasing;
  - no MSAA.

### 2.4 Audio, input, movies
- **Audio:** XMA at kernel-export level (`apu/xma.cpp`) → patched FFmpeg XMA decoder → SDL3 5.1 float stream. The EA stream-player ring semantics are already handled (`apu/xma.cpp:24-36`).
- **Input:** `hid/` is generic SDL3 XInput emulation with rumble and a key map.
- **Movies:** MW uses XMV, with only pacing hooks host-side (`game/movie_pacing.cpp`).

### 2.5 Game layer (must be redone for any other title)
- **`config/nfsmw.toml:183-256`, the midasm hooks:**
  - render-queue fences (7 sites + state at 0x82909650);
  - ultrawide projection/front-end/mirror;
  - frame time;
  - world-step time/count.
- **Function overrides:** `game/glare.cpp`, `light_trails.cpp`, `movie_pacing.cpp`, `debug/d3d_waits.cpp`, `debug/timeline.cpp`. XDK-library hooks are likely to be byte-pattern findable in another title (inferred).
- **Installer:** `runtime/install/manifest.inc` (title 0x454107D9, 49 files, XEX SHA-256) and `installer.cpp:32-41` constants.

### 2.6 Build / licensing
- **Mac build:** Homebrew LLVM + lld + cmake + ninja. macOS 15 target, `-Werror=unguarded-availability-new`, MoltenVK bundled, ad hoc codesign (`scripts/macos/make_app.py`).
- **License:** GPL-3.0-or-later (Unleashed-derived runtime). Third-party: XenonRecomp MIT, Xenia BSD-3, FFmpeg LGPL-2.1, SDL3 zlib, MoltenVK Apache-2.0 (`THIRD_PARTY_NOTICES.md`).

## 3. M1 results: recompilation proof of concept (2026-10-22)

### Toolchain (exact)
| Item | Version |
|---|---|
| Host | MacBook Air, Apple M5, 24 GB, macOS 27.0.1 |
| Compiler for tools | Homebrew clang 23.1.4 (`/opt/homebrew/opt/llvm`). Apple clang 21.0.0 is present but unused. |
| Build | cmake 4.4.4, ninja 1.13.2 |
| Python | 3.15.0 (Homebrew), for `tomllib`. `/usr/bin/python3` (3.9) can't run the survey scripts. |
| XenonRecomp | hedge-dev `ddd128bcca99fe8bfbb99bea583c972351fa6ace` + `patches/xenonrecomp/0001–0009`. Submodules: fmt 11.0.2-306, libmspack v1.11, simde 0.8.4-rc1-8, tiny-AES-c 1.0.0-34, tomlplusplus 3.4.0-15, xxHash 0.7.4-810. |
| extract-xiso | XboxDev `3f5b62cfe68f000b0e3c8a30104973f3a297948e` |

### Commands
```sh
scripts/setup_tools.sh        # 65 s from an empty tools/ (clone, patch, build)
scripts/survey.sh             # 16.8 s wall clock; peak RSS of XenonRecomp about 60 MB
tests/run_instr_tests.sh      # 242 instruction tests
```
`survey.sh` resets its generated config, then runs:
1. `xexsurvey`;
2. XenonAnalyse;
3. `fix_switch_tables.py`;
4. XenonRecomp, for the function map;
5. `find_missing_tables.py`;
6. `fix_switch_tables.py` again;
7. `leaf_switch_bounds.py --write` and `find_entry_points.py --write`;
8. XenonRecomp;
9. `validate_recomp.py`.

Two runs from scratch produced **byte-identical** `config/*.toml` and `ppc/`.

### Results
| Measure | Value |
|---|---|
| Generated output | 289 files (`ppc_recomp.0–284.cpp` + 4 headers/mapping), 181 MB, 6,689,322 lines |
| Functions | 72,864 recompiled (`PPC_FUNC_IMPL` = `PPC_WEAK_FUNC`), 73,219 mapping entries (incl. 355 import thunks), 26 secondary entries |
| `.pdata` | 30,984 / 30,984 function starts present |
| Switch tables | 752: 750 from XenonAnalyse (3 misread tables dropped by the borrowed-bounds rule), 34 inline absolute tables, 2 hand-decoded byte-offset tables |
| Leaf function bounds | 153 jump-table leaf functions + 27 `b +4` tail-call stubs |
| Recompiler log | 0 errors, 0 warnings, 0 unrecognized instructions (37 progress lines only) |
| Failure markers in code | 0 (`// ERROR`, switch `ERROR:`, `.long`, `__builtin_debugtrap`, dropped instructions) |
| Coverage | 98.69 % of code-section bytes. Every untranslated non-zero byte (576 B) is one of the 72 SEH data headers; the rest is zero padding. |
| Instruction tests | 242 / 242 pass. A deliberate wrong expectation in an `eqv` test and in a `bdnzt` test was caught. |

### What it took (first pass → final)
| Problem (first pass) | Cause | Fix |
|---|---|---|
| 3,157 "switch case outside function" (143 tables) | Leaf functions without `.pdata`: XenonRecomp guessed sizes that stop before their targets | SpeedBreaker's `leaf_switch_bounds.py` (153 bounds) + `find_missing_tables.py` (34 inline tables) |
| 29 remaining, in 3 `.pdata` functions | **`b +4` tail-call stubs** before a `.pdata` function swallowed it, inline table included (27 such stubs in Carbon) | `leaf_switch_bounds.py` now finds and bounds them all (MW listed 2 by hand) |
| 42 `b` to non-function | **SEH headers** (`__C_specific_handler` thunk + scope table, 8 B before 72 `.pdata` functions) decoded as code; plus one byte-offset dispatch | `invalid_instructions = { data = 0x82AAC4F4, size = 8 }` (all 72 occurrences are headers) |
| 2 "jump table without switch entry" | `lbzx`-byte-offset dispatches (`ctr = base + byte[table + i]`) XenonAnalyse doesn't model | `byte_offset_table()` in `fix_switch_tables.py` (0x8268C09C: 32 cases; 0x826F890C: 16) |
| 79 unrecognized: `eqv` ×75, `dcbst` ×2, `bdnzt` ×2 | Not in XenonRecomp (MW never used them) | Patch **0009**: `eqv` = ~(rS ^ rB) (+ cr0 for `eqv.`); `dcbst` = no-op like `dcbf`; `bdnzt` = --CTR, branch if CTR.low32 ≠ 0 and CR[BI] (Xenia `bcx`). 15 hand-computed tests in `tests/instr/carbon_*.s`. |
| `setjmp`/`longjmp` | Found by hand | `longjmp` 0x8279B3B0 (restores f14–31, r13–31, lr, cr, r1; RtlUnwind path); `setjmp` 0x8279D060 (hook check, then saves) |

The test harness itself (`tests/run_instr_tests.sh`, from SpeedBreaker) silently skipped any
test with an instruction XenonRecomp can't translate, and returned 0 even on failures. It now
fails in both cases (ReCarbon change).

### Limitations (not M1 criteria)
- **Translation is not execution.** Nothing is compiled natively or run yet (M2/M3). Semantics
  are verified only for the instructions under test.
- **Two byte-offset tables have no bounds check** (0x8268C09C, 0x826F890C). Their length is
  inferred from the table bytes. An index beyond it would reach `__builtin_unreachable()`.
  Runtime use should confirm the index range (M3).
- **The 47 leaf bounds flagged "accepted, check"** by `leaf_switch_bounds.py` were accepted on
  the script's rule. All produce zero errors, but they haven't been individually reviewed.
- **The 26 secondary entries** come from heuristics (`find_entry_points.py`). Wrong or missing
  ones only show up at runtime as indirect-call aborts.
- **The 11 new imports have static call sites** (`XexLoadImage` 2, `XexGetProcedureAddress` 4,
  `XexGetModuleSection` 1, `ExRegisterTitleTerminateNotification` 6, `XamContentResolve` 2,
  `Ob*SymbolicLink` 1+1, `XCustomSetAction` 1). The only module names in the image are
  `xam.xex` and `default.xex`, so the Xex* calls look like XAM export lookups (inferred). They
  need runtime support or logged stubs in M3.
- **The XenonRecomp build** prints one pre-existing warning (nodiscard `std::exchange`,
  `recompiler.cpp:34`).

## 4. M2 results: native arm64 compile and link (2026-10-22)

### What was reused, and what was isolated
SpeedBreaker v0.1.7's `runtime/`, `recomp/` and CMake build were imported. Before reuse, every
runtime reference to a guest address was audited:
- **`grep` for `sub_8xxxxxxx`/`0x8xxxxxxx`:** outside `game/` and two `debug/` files, every hit is
  an NTSTATUS code, a constant, a memory-map base or a comment.
- **MW game layer removed:** all of `runtime/game/*.cpp`/`.h` except the three interface headers.
  It held 13 mid-asm hook targets and function overrides at MW addresses (glare, light trails,
  movie pacing, render-queue fences, Hor+/Vert+ view scaling, frame/step time) and MW structure
  offsets. MW's `SetAspectScale` also stored a float into guest memory at **0x8200FAC0** (MW's
  movie aspect) on every presented frame; in Carbon's image that is unrelated `.rdata`.
- **MW D3D hooks removed:** `debug/d3d_waits.cpp` (`sub_825A5B20`) and the two in
  `debug/timeline.cpp` (`sub_82597498`, `sub_82593A58`). The timeline API stays; its game-wait and
  present counters read 0, as documented in `timeline.h`.
- **Override check:** no runtime object defines any `sub_…` symbol (`nm` over all runtime
  objects), so nothing overrides a recompiled Carbon function.
- **Replacement:** `runtime/game/title_hooks.cpp` implements the interface with the states
  SpeedBreaker's own headers define for a `ppc/` without hooks. `HooksActive()` is false, so the
  picture stays 16:9 letterboxed. `FrontEndScale*` = 0, so the renderer's front-end and edge-bar
  logic never runs. `LastMovieFrameNs` = 0, and the frame stats are zeros. It logs this at startup
  (`[title] NFS: Carbon: no title hooks yet …`). These are explicit, logged states, not silent
  fakes.
- **Identity:**
  - executable `ReCarbon`, user folder `ReCarbon` (SpeedBreaker's folder migration removed, so a
    real SpeedBreaker install is never touched), env prefix `RECARBON_`, thread names
    `recarbon-*`;
  - installer for "Need for Speed: Carbon", title 0x454107EC, with a regenerated manifest:
    78 files, 5,738,577,280 B, default.xex `947f95c7…`, `$SystemUpdate` excluded;
  - the update check points at `retronative/recarbon`. That repository is **assumed** and returns
    "no release" until it exists.
- **Imports:** `kernel/import_stubs.cpp` was regenerated from Carbon's `ppc/` with SpeedBreaker's
  generator. A stub logs the name and registers and **exits** (code 3); it continues only with the
  developer switch `RECARBON_STUB_CONTINUE=1`.

### Toolchain and inputs (exact)
| Item | Version |
|---|---|
| Compiler | Homebrew clang 23.1.4, Release (`-O3`), C++20, `-UNDEBUG` (asserts on), `-Werror=unguarded-availability-new` |
| Build | cmake 4.4.4, ninja 1.13.2 |
| Libraries (Homebrew, dynamic) | SDL3 3.4.18, vulkan-loader 1.4.363, vulkan-headers 1.4.363.0, glslang 16.6.0, SPIRV-Tools 1.4.363.0, MoltenVK 1.4.2 |
| Static | FFmpeg 7.1.1 XMA-only (`scripts/build_ffmpeg_xma.sh` + `patches/ffmpeg`), XenonUtils (XEX loader), o1heap, Dear ImGui, qrcodegen, FSR1 headers |
| Deployment target | macOS 27.0, the host's, to match Homebrew's bottles (minos 27.0). A macOS 15 release build needs source-built deps (packaging, M8). |

```sh
scripts/setup_tools.sh && scripts/survey.sh       # tools (incl. FFmpeg-XMA, 65 s) and ppc/
cmake -S . -B build/main -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
ninja -C build/main -j8
```

### Metrics (MacBook Air, Apple M5 4P+6E, 24 GB, macOS 27.0.1)
| Measure | Value |
|---|---|
| Clean configure + build, `-j8` | **97.9 s** wall, 662 s user, 38.6 s sys; 395 steps |
| Generated code alone (`ReCarbonRecompLib`, 285 TUs) | 66.0 s wall, 465 s user; archive 64 MB, 287 arm64 objects |
| Peak memory | 2.56 GB combined compiler RSS (sampled every 1 s); system free memory ≥ 37 % throughout. The largest TU (`ppc_recomp.283.cpp`) takes 6.3 s and 683 MB alone. `-j8` needs no limit on this machine. |
| Output | `build/main/runtime/ReCarbon`: Mach-O arm64 executable, **59,561,176 B** (unstripped) |
| Diagnostics | 0 errors, 3 warnings, all in files byte-identical to SpeedBreaker's: the intentional `PPC_CALL_INDIRECT_FUNC` override (`cpu/indirect.h`) and two `uint32_t != NULL` asserts (`kernel/imports_unleashed.cpp:281,430`) |

### Link validation
- **No undefined-symbol tolerance:** the link uses no `-undefined dynamic_lookup`, `-U` or
  flat namespace (`build.ninja`).
- **Every undefined symbol is bound to a named library** (`nm -m -u`): libSystem 209, libc++
  210, libvulkan 119, libSDL3 95, glslang 16, Foundation/AppKit/CoreFoundation/CoreGraphics/libobjc
  34. Unbound: **0**.
- **All 72,864 recompiled bodies are present:** exact name diff of `PPC_FUNC_IMPL` in `ppc/`
  against `nm` of the binary, 0 missing.
- **All 355 function imports are defined:** 182 real implementations, 173 logging stubs.
- **Runtime overrides of `sub_…`:** 0.

### Why 30,984 function-table entries become 72,864 functions
`.pdata` is the exception/unwind table. The compiler emits an entry only for functions that need
unwind data: those with a stack frame or saved registers. A **leaf function with no frame has no
`.pdata` entry.** XenonRecomp (`recompiler.cpp:176-250`) builds its function list from:
1. `.pdata`;
2. every `bl` target;
3. a scan of all remaining code;
4. the config: helper addresses, `functions`, `secondary_entries`.

It then translates every code byte. Classifying all 72,864 generated functions by origin:

| Origin | Functions |
|---|---|
| `.pdata` entries (30,984; 6 also carry CRT helper names) | 30,978 |
| CRT register save/restore entry points: save + restore × (18 GPR r14–31 + 18 FPR f14–31 + 18 VMX v14–31 + 64 VMX v64–127), from the helper addresses (each entry is a separate function) | 236 |
| Config: leaf jump-table bounds and `b +4` stubs | 180 |
| Config: secondary entry points | 26 |
| Leaf functions called by `bl` (no `.pdata`) | 3,480 |
| Leaf functions referenced from data, e.g. vtables or callbacks (no `.pdata`) | 6,738 |
| Found only by the code scan | 31,226 |
| **Total** | **72,864** |

The last group breaks down by what precedes each function:
- 26,283 start right after a `blr` or padding;
- 3,121 follow an unconditional `b`;
- 1,324 have addresses built in code (`lis`+`addi`);
- 109 are `b` targets;
- 389 fall through from the previous piece.

They are small: median 16 B, 95th percentile 80 B, about 0.9 MB in total. Samples are complete
tiny leaf functions such as `lwz r3,12(r3); blr` getters, field-pointer accessors and copy
helpers. None is referenced by a direct branch or a data word, so they are presumably
unreferenced out-of-line copies, or reached in ways a static scan doesn't see (inferred).

**How each count was validated:**
- **30,984:** parsed independently from the decrypted image's `.pdata` (8-byte entries, nonzero
  begin). XenonRecomp reads the same table, and `validate_recomp.py` HARD-checks that every one
  is a mapped, defined function.
- **72,864:** the number of `PPC_FUNC_IMPL` bodies in `ppc/`. It equals the `PPC_WEAK_FUNC`
  wrappers and the mapping entries minus 355 import thunks. It's one-to-one with no duplicates
  (validator), and all of them are present in the linked binary (name diff, 0 missing).
- **The breakdown:** computed from the image (`.pdata`, `bl` targets, data words, `lis`+`addi`
  pairs) and the config. The categories are disjoint and sum exactly to 72,864.
- **Coverage:** 98.69 % of code bytes. The only untranslated non-zero bytes are the 72 SEH
  headers, i.e. data (§3).

### Runtime limitations (known; M3+)
- **Nothing has been executed.** M2 proves the code compiles and links, not that it is correct.
- **173 imports are stubs that exit.** Among them are the 11 new to Carbon (`XexLoadImage`,
  `XexGetProcedureAddress`, `XexGetModuleSection`, `XamContentResolve`, …), all `NetDll_*`/XNet,
  crypto, and most `XamShow*UI`. The first one reached at runtime stops the game with its name in
  the log.
- **The data import `XboxKrnlVersion`** has no initialiser in `kernel/module.cpp`. It is zeroed
  and logged (`main.cpp:157`), but it's a version structure the game may read.
- **No Carbon title hooks.** These are inactive and logged: wide/tall screen fill (the Aspect
  Ratio setting behaves as 16:9), movie pacing, frame-time stats, and the D3D wait/present
  timeline counters. Carbon's lock-free render queue, if it is like MW's, has **no ARM fence
  hooks yet** (risk R4).
- **MW-tuned GPU heuristics still active** (audit of `gpu/`, `video/`):
  - the render-scale pitch window 640–1280 with Internal Resolution "Auto" (the biggest risk:
    Carbon's blur/bloom targets may be scaled);
  - the 1280-wide untiled MSAA rule;
  - MW-measured scale budgets and the Auto frame-rate draw thresholds.

  Recommended for M3: start with Internal Resolution 1x and log Carbon's targets.
- **Not distributable yet:**
  - deployment target 27.0 (Homebrew deps);
  - no app bundle, MoltenVK bundling or signing (M8);
  - SpeedBreaker's unit tests (`tests/*.cpp`) not ported; `-DRECARBON_TESTS=ON` stops configure
    by design;
  - iOS/SteamOS paths inherited but untested.

## 5. M3 results: runtime initialization (2026-10-22)

**Method.** Each launch used `scripts/run_attempt.sh`: 1x internal resolution, audio muted,
strict stubs (`RECARBON_STUB_CONTINUE` never set), hang reports after 20 s, and a deadline
ending in SIGTERM.
- Every stop was root-caused from the disassembly before any change.
- Imports were implemented only when reached, with their semantics cited from Xenia (BSD-3)
  at 95a5c3ee.
- About 50 launches in all. The attempt table is in the local, untracked `WORKLOG.local.md`.

### Checkpoints
| | Checkpoint | State | Evidence |
|---|---|---|---|
| C1 | Image loaded, imports wired | **Passed** | `[loader] wired 355 function import slots, 14 data imports` every run |
| C2 | Entry point, CRT, guest threads | **Passed** | `calling entry 829446E0`; `MainThread` and further threads created and named |
| C3 | Data through the VFS | **Passed** | `D:\NFS\ZDIR.BIN`, `ZZDATA0–10.BIN` opened (`ZZDATA11` probed: not on the disc) |
| C4 | GPU bring-up | **Passed** | interrupt callback `826DC070`, ring buffers, PM4 batches processed, renderer ready at 1x (`internal resolution 1x1`) |
| C5 | First `VdSwap` | **Passed** | `[gpu] frame 1`, then on to frame 900 within 40 s, with no hang, stub or crash. Run groups: w01–w10 **10/10** on the ring-fix build; a30 1/1 after `XboxKrnlVersion` and the memexport log; final1–10 **10/10** on the committed code (final1–5 before `ppc/` was regenerated byte-identically, final6–10 after) |
| C6 | First presented frame of the game's own UI | **Reached** (M4's goal) | frame dumps: "NEED FOR SPEED CARBON — LOADING..." from frame 2880 in 3/3 runs on the ring-fix build (dump1–3), and 1/1 on the final code (dump4) |

**Deepest verified point:** the game's own loading screen, rendered and presented through
MoltenVK. A 240 s run went on to about 8,700 frames at 50–60 fps (about 1,220 draws per
frame) with no stop, after the boot movies and the attract movie four times. That the game
then sits at its title/attract loop is *inferred* from the files it opened, not seen: the
movie frames are black.

**Function count.** The survey fix (0x827C1A18 is now a function start) makes the pipeline
produce **72,865** functions. The M1/M2 figures (72,864) describe the earlier output, and the
validator's checks hold for both.

### Fixes (each with its evidence)
| Stop | Root cause | Fix | Test |
|---|---|---|---|
| `RtlRaiseException` (stub) | XAPI `RaiseException(0x406D1388)`, MSVC's SetThreadName (callers 0x826D3748, 0x82924F20), raised under `__try/__except` | Name the thread and return, as Xenia does (`xboxkrnl_debug.cc`). Any other code is fatal, with its code logged (no SEH dispatch). | `raise_exception_test` |
| `NetDll_XNetStartup`, `WSAStartup`, `WSAWaitForMultipleEvents`, `XNetGetTitleXnAddr` | Network init. The game's wrapper (0x829164F4) falls back to the import when `XexGetModuleHandle("xam.xex")` fails. | Xenia's semantics: a stack that starts, with no link. WSA waits keep the WSA contract (relative ms, `WSA_WAIT_EVENT_0+i`/`TIMEOUT`); Xenia passes a positive (NT: absolute) timeout, which isn't copied. | launches |
| `ExRegisterTitleTerminateNotification` | title-lifetime registration | Recorded and removed as in Xenia. **Not run at exit** (Xenia doesn't either). | launches |
| `XMsgStartIORequestEx` (XMP 0x0007001A) | the Ex entry point of an already implemented XMP message | Same as `XMsgStartIORequest`; the 6th argument is ignored, as in Xenia | launches |
| `XboxKrnlVersion` zeroed | data import new in Carbon | Xenia's initializer (2.0xFFFF.0xFFFF, 0x80) | launches |
| Abort: indirect call to `0x827C1A18` (movie player) | `leaf_switch_bounds.py` walked a leaf bound back past a `blr`, over a function whose pointer is built with `lis/addi` (0x827C2748). `find_entry_points.py` judged against the pass-1 function map. | Leaf bounds stop at code-built pointers; `survey.sh` reruns entry points to a fixed point | new HARD check in `validate_recomp.py` (2,642 pointers, all function starts); mutation: the old bound fails it |
| **Intermittent stall before frame 1** (about 1 launch in 2) | **Two races in the command processor's ring handling**, found with lldb (fence completed 5, issued 13, target 11; ring read = write = 31) and a temporary kick/wait trace. Carbon calls `VdInitializeRingBuffer` twice (32 KB at 0x05B51000, then 64 KB at 0x05B49000). (1) An old-ring batch finishing after the re-initialization stored its end (31) over the read-index reset. (2) D3D's device init resets its own write-pointer copy (`device+10824` at 0x826DC6F8) without writing `CP_RB_WPTR`, which kept the old ring's 31. Either way the CP skipped the new ring's first packets, and D3D's fence wait (`sub_826DBD78`) spun forever. | `gpu/ring_state.h`: a generation-checked read advance, and `CP_RB_WPTR` reset and read under the same lock as the ring. This is upstream SpeedBreaker code, presumably latent for MW (inferred). | `ring_state_test` (both halves; each mutant fails it). Launches: 5/10 stalled with the first half fixed, 0/10 with both. |

New tooling:
- **`ctest` harness:** 8 tests, 6 from SpeedBreaker plus ReCarbon's `raise_exception` and
  `ring_state`. SpeedBreaker's `guest_time_test` is **flaky**: 3/25 standalone runs and 1/5
  with the game running fail at `guest_time_test.cpp:487` ("system time went back … 1.00 us"
  across a simulated suspension). `cpu/guest_time.cpp` is unchanged from upstream. Not
  investigated in M3.
- **Hang reports** now print every guest thread's r0–r31.
- **Memexport-only draws** are logged once when dropped; previously they were silent.
- **`scripts/run_attempt.sh`:** the launch harness.

### Audits
- **Memory:**
  - guest base 0x7000000000 (lldb);
  - the 0xA/0xC/0xE physical windows show identical data for the same physical address
    (0x05B48000), with the 0xE window unshifted on 16K-page macOS, as SpeedBreaker records;
  - guest memory is imported into Vulkan zero-copy.
- **Threads:** every guest thread starts and names itself (`MainThread`,
  `RWAudioCore Dac`). Threads 4, 5 and 15 name themselves "" (not checked whether that is the
  game's string).
- **Synchronization:**
  - D3D and its interrupt callback share state only under spinlocks
    (`KfAcquireSpinLock`/`KeAcquireSpinLockAtRaisedIrql`, real atomics here). No D3D function
    relies on IRQL alone, which is a no-op here.
  - The ring race above was a host-side data race, not a PowerPC ordering issue.
  - Carbon's own lock-free queues (MW needed 7 fence hooks) show **no symptom yet**; still
    open (R4).
- **GPU heuristics:**
  - 1x is exact (`internal resolution 1x1`), so the 640–1280 pitch window is moot.
  - Unknown texture formats are logged (`texture format %u not implemented`), and none
    occurred in a 240 s run.
  - Memexport-only drops are now logged, and none occurred.
  - The MW-only front-end and edge-bar code stays inert (no hooks).

**What covers what:**
- `ctest` covers the RtlRaiseException fix and both ring races.
- `validate_recomp.py` covers the survey fix, and a mutation check proved it catches the bug.
- The networking, title-notification, XMsgEx and XboxKrnlVersion changes are covered only by
  the launches.

### Remaining blockers and limitations (not M3 criteria)
- **Movie video is black.** Every boot movie opens and its four decoder threads start (entry
  0x827C8B78…), but the presented frames are black for the 47 s of playback. Audio is
  muted, so it isn't verified. M4/M5.
- **Not exercised:** input, saves and profiles (166 imports are still logging stubs, among
  them XamShow*UI, content, crypto and the rest of networking), and gameplay.
- **Title terminate notifications** are recorded but not run at exit.
- **The D3D hang check's clock**, `*(*(r13+256)+88)`, is unexamined.
- **Assumed rather than verified:** `XexGetModuleHandle` failing for `xam.xex` and
  `xboxkrnl.exe` (the game falls back to imports) is equivalent to the console's success
  path.

## 6. M4 results: rendering (Passed 2026-10-22)

### Black movie video: fixed (commit 487cf65)
Evidence chain:
1. **Decoder:** fine. The Y plane read from guest memory mid-movie (lldb) is a real frame.
2. **Movie draw:** the YUV→RGB pixel shader is correct. The movie is drawn into EDRAM base 0 as
   `k_2_10_10_10_FLOAT` (format 3) and resolved.
3. **Overlay:** the movie player then blends its overlay onto the **same EDRAM** as
   `k_2_10_10_10_FLOAT_AS_16_16_16_16` (format 12), and resolves that to the front buffer.
4. **Root cause:** the renderer keyed host render targets by the raw format, so format 12 was a
   separate, never-written image. It also counted format 12 as 64 bpp.
5. **Reference:** Xenia's `GetStorageColorFormat` (12 → 3, 10 → 2: "same packing and meaning of
   values") and its 64-bpp list.

**Fix:** `gpu/edram_format.h`; targets are keyed by storage format.

**Verified:**
- `edram_format_test` (the old behaviour fails 4 checks);
- an isolated build at HEAD plus the fix: 10/10 launches;
- frame dumps:
  - the **EA logo** (frame 60) and **EA HD** (180);
  - the Nikki PSA and movie scenes;
  - the loading screen, unchanged from M3;
  - frames 60–2760 all have content (all were black before).

### Flaky `guest_time_test`: root-caused and fixed (commit 124cf31)
`guesttime::RawSystem` read macOS `system_clock` on every call. It has 1 µs resolution and
steps back: by 1 µs when several threads read it close together, and by milliseconds to
seconds when the host time is adjusted. The guest's system time inherited that through
`KeQuerySystemTime`, the time-stamp bundle and absolute-wait deadlines. The test's assertion
was right.

**Fix:** the wall clock is read once and advanced by the steady clock (no NTP tracking within
a session).

**Verified:**
- new `system_time_monotonic_test`: the old code fails 6/6, the fix passes 6/6;
- `guest_time_test` 0/25, previously 3/25;
- `ctest` 10/10.

### Counts and stability
- **Unknown texture formats and memexport drops:** 0 of each over 25 windowed runs.
- **Launches:**
  - committed code without the clock fix: 25/25 clean;
  - with it: 24/25.

### Intermittent movie-teardown hang: root-caused and fixed (commit 7038a47)
- **Symptom (about 1 in 50 launches):** the main thread waits (`KeWaitForSingleObject`) in
  the movie player's teardown (`sub_827C9B30`) on decoder thread 3's done event (0x40084AD4).
- **Ruled out:** a lost wakeup when a guest event header is re-initialized inline. An event
  trace over 9 runs showed 16 re-binds per run, all with no waiters.
- **Root cause:** the hang report shows decoder thread 3 alive, blocked in
  `RtlEnterCriticalSection` from XAudio's voice yield (`sub_82A4E7A8`). That code reads the
  critical section's `RecursionCount` with `lwz`, leaves the section that many times and
  re-enters. `XRTL_CRITICAL_SECTION` stored its counters host-endian, so a count of 1 read as
  0x01000000 (the thread's r30 in the report). The first leave released the section, and the
  rest drove the count negative. A thread entering in between could never release it, so
  thread 3 never set its done event.
- **Fix:** big-endian `LockCount`, `RecursionCount` and `OwningThread` (XenonRecomp patch
  0010, as Xenia has them), with the logic in `runtime/kernel/critical_section.h`.
  `critical_section_test` passes. With the fields host-endian it fails 5 checks, without
  deadlocking. `ctest` 11/11.
- **Verification:**
  - 58/58 launches clean: frame 900 and 8 movie opens each, no hang, no strict stub. 100 were
    planned; the run was stopped at 58 by decision.
  - 3/3 unmuted 60 s launches clean (MacBook Air speakers, 0 gaps over 50 ms).
- **Remaining uncertainty:** the yield path wasn't hit in 28 instrumented launches. The fix is
  shown by the hang report and the test, not observed in the game. At the old rate, 0/58 would
  still happen about 31% of the time without the fix, so the launches are a regression check.

## 6a. M5 progress: audio, movies, input (in progress, 2026-10-23)

### Unmuted baseline (no audio code changed)
- 180 s through the boot movies and the title: the device opens (MacBook Air speakers, 5.1
  float, 48 kHz), 0 gaps over 50 ms, sound throughout.
- Front end reached by script: START on the title, then A on "Create a new alias?" → No.
  Five strict stubs stood in the way. They're implemented from Xenia's `xam_net.cc` (commit
  752537e): `XNetUnregisterKey`, `WSACleanup`, `XNetCleanup`, `WSASetEvent` and
  `WSAResetEvent`. The offline path then shows "Cannot connect to Xbox Live!".

### Front-end audio underruns: root-caused and fixed (commit 79573bb)
- **Symptom:** in the menus, device-empty frames and gaps of 50–165 ms; none during boot.
- **Evidence:**
  - The guest render callback took 15–103 ms, not 5.33 ms. Late wakes were only catch-up.
  - No critical-section or spinlock wait over 5 ms on the audio thread.
  - A stack sample showed the callback setting an event and then waiting for XAudio's mixing
    thread (`sub_82A4FCC8`).
  - XAudio sets that thread to priority 15 (time critical), but `KeSetBasePriorityThread` was
    compiled for Windows only. The thread ran at the default QoS while the waiting callback ran
    at user-interactive: a priority inversion.
- **Fix:** `kernel/thread_priority.h` reads the priority as Win32's `THREAD_PRIORITY_*`, as the
  runtime's Windows path does. 15 and above map to user-interactive, 1–2 to user-initiated,
  and nothing is lowered. `thread_priority_test` passes; with priorities ignored, it fails 5
  checks.
- **Verification:** 3 unmuted 420 s runs with over 5 minutes of menu music each, 0 gaps over
  50 ms and 1 device-empty frame at start-up. Boot regression 5/5. `ctest` 12/12.

### Movies
- **Speed (passes for M5):** a temporary probe compared each presented picture with the last.
  - Nikki PSA: 11.38 s against 11.40 s in its ASF header. EAHD bumper: 3.97 s against 4.00 s.
  - Attract: 2,224 picture changes in its 75.58 s; the file has about 2,265 frames.
  - The XMV player times frames against audio played (MW's disassembly; the same XDK
    component, assumed for Carbon), so the audio clock runs at the right rate too.
- **Known limitation, not smooth:** 72–80% of movie frames show for 2 vblanks and 20–28% for 1
  or 3. That's MW's judder before SpeedBreaker's pacing hooks (7–10% there). Carbon has no
  pacing hooks. The probe flushes the GPU and reads back every frame, which may inflate the
  number. Smoothing is deferred to polish unless it causes gameplay issues.
- Lip-sync passes by eye and ear (hand test 2, below).

### See-through road in gameplay: root-caused and fixed (commit 557d9b5)
- **Symptom** (hand test): in gameplay the road was see-through, with buildings, terrain and
  lights visible through it. In game-only frame dumps the road was flat black with no texture
  or headlight light, and city lights showed on it. Road surfaces in the intro cutscene were
  fine.
- **Ruled out:** depth-format aliasing (every gameplay depth target is D24S8), and an
  uncleared scene depth buffer. The game clears depth with a 4x-MSAA rectangle at half pitch,
  which lands in a different host image than the 1x scene. Clearing the 1x image as well
  changed nothing visible. That split is recorded, but it isn't this defect.
- **Root cause:** the road's pixel shader uses `getGradients` (texture fetch opcode 18), which
  the translator didn't implement. The shader failed to translate, so every road draw was
  skipped, about 28 per frame (104,985 skipped draws in gameplay against 24 in the front end).
- **Fix:** `getGradients` returns (ddx(x), ddy(x), ddx(y), ddy(y)) of its first two source
  components with coarse derivatives, in pixel shaders only, as Xenia's SPIR-V translator does.
  `shader_gradients_test` (hand-assembled microcode, no game bytes) passes; against the old
  translator it fails 3 checks with the game's exact error. `ctest` 14/14.
- **Verification:** gameplay frames show asphalt, centre lines and headlight light, with
  nothing through the road. 0 translation failures, and skipped draws back to 24. Boot
  regression 3/3.

### XMA and input
- **XMA:** 3,749 frames decoded with the XMA1 decoder, 0 skipped, across five runs (menu sound
  effects). The front-end music isn't XMA. XMA1 against XMA2 isn't settled: the decoder's
  synthetic packets work for both.
- **Input:** the controller's default button mapping moved unchanged to `hid/pad_map.h` and is
  pinned by `pad_map_test` (commit af3aeea). An SDL virtual controller reaches the main menu.
  Keyboard rebinding is unchanged, and `settings_keys_test` and `key_map_test` pass.
- **Physical controller: Deferred — not verified.** By the user's decision (2026-10-23) it is no longer an
  alpha gate for M5 or M7, and is to be tested once the 8BitDo is available. The virtual-controller
  tests (menus, hot-unplug: commit 05607a2) are automated evidence for the input path only. The
  game never sees an unplug, because the keyboard stays controller 0. They don't show that a
  physical controller works.

- **M5 closed as accepted** (user decision, 2026-10-23), with three open, non-blocking
  limitations, none recorded as passed:
  - the physical controller (deferred, above);
  - movie judder (pacing hooks deferred to polish);
  - the '?' glyph on the Controls page: root-caused (a non-Latin keyboard layout) and handled
    since (33bf340, below).

  M6 stays open until M6.6, M7 is the current milestone, and M8 is Pending, not started (ROADMAP).

### Hand test 2 (2026-10-23, keyboard, by the user)
Two sessions in the player's own user folder (`scripts/run_attempt.sh` with
`RECARBON_REAL_HOME=1 RECARBON_MUTE=0`), 426 s and 1,226 s. The user reports all five checks
passed:
- movie lip-sync;
- menu navigation with the keyboard;
- rebinding in Settings > Controls: `right_trigger_2` and `left_trigger_2` set to Up and Down,
  saved to `settings.toml`, and still bound and working after a restart;
- audio quality (music, effects, engine);
- audio latency on the built-in speakers.

The logs show no crash, hang report or stub. Audio had 1 device-empty frame at start-up in
each session and one gap of 258 ms in the second, at 457.5 s (10:41:41–42). Both sessions
opened the output on the WH-CH720N headphones (the system default). The user switched to the
built-in speakers during the second session. The macOS log (`coreaudiod`) puts that switch in
the same second as the gap:
- 10:41:41.995 and 10:41:42.099: the headphones' two Bluetooth audio devices disconnect.
- 10:41:42.121: macOS picks `BuiltInSpeakerDevice` as the default output.

The first session has no such event. The latency check on the speakers therefore ran in the
second session after 10:41:42 (about 770 s on the speakers). The output stream opens the
default device and follows it, so the gap coincides with the device change, not with steady
playback. How long the switch itself takes isn't measured: the runtime doesn't log a change of
device.

Keyboard players can't see that Q and E turn the settings pages: the footer named only LB
and RB. It now shows "LB RB / Q E: page" (commit 4f6e0f5).

**The '?' glyph on the Controls page: root-caused and handled (2026-10-23, commit 33bf340).**
- **Symptom:** in automated screenshots the Controls page drew the letter keys W, S, A and D as
  the replacement glyph (U+FFFD), while named keys (Up, Space) drew correctly.
- **Cause (verified):** the M5's active keyboard layout was Myanmar. The cell names a key on the
  active layout, and on Myanmar SDL gives W as U+1010, S U+103B, A U+1031 and D U+102D (logged
  bytes `E1 80 90` and so on). The menu's font, Roboto, has no Myanmar glyphs.
  - The earlier "heisenbug" was the user switching between Myanmar and U.S. between runs: probe
    builds happened to run under U.S.
  - Separating build from machine showed it: the M1-built binary drew the diamonds on the M5
    (2 of 2), the unmodified M5 build drew them 3 of 3 under Myanmar, and the M1 (U.S.) drew
    letters 3 of 3.
- **Handling** (the user's spec):
  - ReCarbon supports English layouts for rebinding. When a key's name on the active layout
    isn't printable ASCII, the Controls page refuses to bind it. The help line says
    "Unsupported key character. Switch to an English keyboard layout.", nothing is saved, and
    the wait goes on (Esc or B cancels).
  - The check runs on capture and again just before the save.
  - Scancodes stay the binding identity. Gameplay input and saved bindings are unchanged.
  - Labels aren't substituted: under a non-Latin layout, existing letter bindings still draw as
    U+FFFD.
- **Verified:**
  - **Unit test:** `key_bindable_test` covers ASCII, special keys, Myanmar, Thai, Latin-1 and
    control characters. Mutating the rule fails it.
  - **M5, Myanmar active:** W was refused and nothing saved; F5 was saved and still bound after
    a restart (unchanged `settings.toml`).
  - **In-game mutant:** with the checks removed, a Myanmar-named T was saved.
  - **M1, U.S. active:** T was saved and still bound after a restart.

## 6b. M6 progress: profile, saves, stability (2026-10-23)

### Profile and save
- **Flow (fresh user folder):** title → "Cannot connect to Xbox Live!" → main menu. An alias is
  only demanded at Career > Start Career, which offers the game's own Enter Name screen, with
  START for Done. The save `rmcsave:\<alias>` is written just before the career intro: 286,764 B
  plus a 308 B `.xcontent`.
- **Reload:** on the next launch, `[xam] found save`. The game opens and reads the save and
  goes straight to the main menu with no alias prompt. The save's SHA-256 is unchanged.
- Automated runs use an isolated user folder (`scripts/run_attempt.sh`, commit 30e6ebf).

### Deferred render-callback queue: root-caused and fixed (commit e40e9ec)
- **Symptom:** in a 16-minute soak (alias, career intro, prologue restarts), SIGABRT after
  158 s: `indirect call to 8209B4AC, which is not a recompiled function`, in `sub_824694C0` on
  the main thread. `0x8209B4AC` is in `.rdata`: a bad pointer, not a missed function.
- **Root cause:** `sub_824694C0` plays back a lock-free single-producer, single-consumer queue
  (state `0x82B8C764`) that `sub_82469448` fills from another thread with no barriers. On ARM64
  the publish (write pointer, then count) can become visible before the entry's stores. This is
  MW's race, which SpeedBreaker fixed the same way, and which FEASIBILITY R4 and
  ARCHITECTURE_REVIEW F8 anticipated.
- **Fix:** mid-asm hooks put a release fence before the publish (`0x824694B0`) and an acquire
  fence before the consumer reads an entry (`0x82469518`). `validate_recomp.py` now
  hard-checks that every configured hook is emitted at its address: it failed before the
  regeneration and passes after.
- **Verification:** 2 further 16-minute soaks clean, 54,900 frames each. The crash was seen once
  in one soak, so the launches are a regression check; the root cause rests on the mechanism.

### Packaging and memory (M8 work done early)
- `scripts/macos/make_app.py` and `check_app.py` are ported (commit eda0ab1). The app carries
  bundle ID `org.retronative.recarbon`, signs ad hoc and runs from its bundle. `check_app` check
  3 fails: Homebrew's Vulkan loader names `/opt/homebrew`, so a release needs libraries built
  from source.
- **Alpha packaging check (2026-10-23, build 184d87f):**
  - **Build and checks:** `make_app.py` built a 72 MB app for macOS 27.0+, bundling 7 libraries
    and MoltenVK 1.4.2. `check_app` passed checks 1, 2, 4 and 5. Check 3 failed only because
    the bundled Vulkan loader has three search folders compiled in: `/opt/homebrew/etc/xdg`,
    `/opt/homebrew/share` and `/opt/homebrew/etc`. The app points the loader at its own
    MoltenVK either way. On a Mac that has Homebrew, Vulkan layers installed there could still
    be picked up.
    - **Measured** (`VK_LOADER_DEBUG=layer,driver`, 2026-10-23): the loader looks for implicit
      layers in `/opt/homebrew/etc/xdg`, `/opt/homebrew/etc` and `/opt/homebrew/share` (each
      `/vulkan/implicit_layer.d`), next to the standard `/etc`, `/usr/local/share`,
      `/usr/share` and the user's `~/.config` and `~/.local/share`. It found no layer files.
      So the strings don't make the app load anything from Homebrew. Their one effect is that
      an implicit layer installed under Homebrew's prefix would load into the game, as one
      under `/usr/local/share` would. Without Homebrew those folders don't exist.
    - The options are M8's: build the loader from source with the standard folders only
      (`build_deps.sh`, done in M8: 825ae48, 74fa6e1), or accept the folders and change check 3. Either is a decision for the
      user. Nothing has been changed.
  - **On a second Mac:** the MacBook Air (M1, macOS 27.0.1) has Homebrew's copies of the same
    libraries installed. The app ran there for 60 s with `DYLD_PRINT_LIBRARIES=1` and a
    cleared environment.
    - Every library outside the system loaded from inside `ReCarbon.app`, and nothing named
      `/opt/homebrew`.
    - The signature verified. It reached frame 900, exited cleanly, and hit no strict stubs.
  - **Installing from the disc image:** `ReCarbon --install <the user's .iso> --dest <folder>`
    copied and verified the 78 manifest files (5.3 GB) in 4 s, in an isolated user folder.
    - The install was then moved to the default `~/Library/Application Support/ReCarbon/game`,
      and the app launched from outside the repository with a cleared environment.
    - The app reported that the recorded install folder had gone, found the game in the
      default folder, and reached frame 900. It exited cleanly.
  - **Limits:**
    - The app needs macOS 27 or newer, because Homebrew's bottles are built for 27.0.
    - It wasn't tried on a Mac without Homebrew, or under a new macOS user account.
    - The release build of the libraries was ported later (M8, `build_deps.sh` 825ae48). A
      build against them passes `check_app` on all checks (74fa6e1, ROADMAP M8).
- The 16 KB host-page release fix is ported from SpeedBreaker v0.1.8 with ReCarbon's own test
  and a mutation check (commit 84df8f3). It was latent: never observed in play.

### Free-roam stability: verified by hand test 3 (2026-10-23)
Free-roam opens only after a player escapes the prologue chase, which a blind input script can't
drive, so the user played it. The session ran in the user's own user folder (`scripts/run_attempt.sh
hand03 3600` with `RECARBON_REAL_HOME=1 RECARBON_MUTE=0`), on the keyboard, at build 184d87f.
- **User's report:** the prologue chase was won at the first attempt. Free-roam ran from 11:45 to
  12:00, 15 minutes in a row, with nothing unusual seen and the Mac at a normal temperature.
- **Log correlation:** the session started at 11:36:32, so free-roam ran from about 508 s to
  1408 s. The game ran until 1427 s and quit cleanly (`window closed`, exit 0).
  - No new crash or hang report in the user folder; the last ones date from 2026-10-22.
  - No strict-stub hit, no "not a recompiled function", no watchdog report.
  - 457 performance samples in that window: 59.7 fps presented on average, the lowest 34.8,
    thermal state "fair" throughout. No audio gap over 50 ms.
  - The longest frame was 333 ms, at 1305 s, in a burst of 125–333 ms frames. The command
    processor was idle for most of each of those frames (311 ms of the 333), so the game
    submitted nothing then. The cause isn't investigated, and the user didn't notice a stall.
  - The career save was rewritten at 980 s: same size, new hash. Whether it reloads is part of
    M7's race-completion check, not verified here.
  - The user opened Settings > Controls between 1202 s and 1265 s and put every binding back as
    it was: `settings.toml` is identical to its backup from before the test.
  - The game looked for `D:\sound\NISRevData\introcareer06ng.bin`, `introcareer07ng.bin`,
    `introcareer08ng.bin`, `endingcareer22.bin` and `D:\NFS\ZZDATA11.BIN`. None of them is on
    this disc. The game carried on each time; it was also seen in earlier sessions.
- **Result:** the M6 stability criterion, no deadlock or crash for 10 minutes in free-roam, is
  met. The two 16-minute automated soaks, after the render-queue fix, are the regression check.

## 6c. M7 exploration: racing (in progress, 2026-10-23)

### Quick Race
- Main menu, in order: Career, My Cars, Challenge Series, Quick Race, Custom Match, Quick Match,
  Reward Cards, Statistics, Options.
- A scripted Quick Race loads, and the race runs with no crash, hang or strict stub in 8
  minutes. Without steering, the script's car pins itself against the first corner.
- **Frame pacing in that race:** 57.6 fps on average, no interval under 30 fps (the lowest
  42.8). A run without frame dumps (soak) averages 59.5 fps. Frame dumps (`RECARBON_DUMP_FRAMES`)
  add a ~57 ms stall every 60 frames, because the PPM is written synchronously, so they must be
  off for any frame-time measurement.
- **Open: a stall in free-roam (hand test 3, cause unconfirmed).**
  - **Timing:** session `2026-10-23_11-36-32` (started 11:36:32, build 184d87f), 1304.6–1305.8 s,
    about 11:58:16–17. The worst frame was 333.2 ms, frame 71043 at 1305.4 s, inside a burst of
    150.2, 151.5 and 125.5 ms frames.
  - **User's observation** (from their recording): a slight stutter around 11:58:17, possibly as
    they swerved to avoid a white traffic car entering an intersection. They've occasionally
    seen similar stutters in SpeedBreaker; whether the cause is the same isn't known.
  - **What the log records for frame 71043 and the other 11 slow frames in 1303–1306 s:**
    - The command processor was idle 311.1 of the 333.2 ms (busy 7.9, waits 14.2, all of them
      register/memory waits). No translated frame was ready to process for most of the time.
    - No shader or pipeline was created, and no image or texture was created or uploaded.
      Nothing was read from files (0 KB), and no `[pipelines]` line was logged in that window.
    - 19 write-watch faults, 0 stalls; 7,003 KB of snapshots.
    - "game: longest frame 0.0 ms": Carbon's frame-time hooks don't exist yet, so this log
      can't measure the game thread's own frame time.
  - **To investigate later** (no hypothesis is preferred yet):
    - shader and pipeline compilation, including guest-side shader setup the log doesn't show;
    - asset streaming, from memory as well as from files;
    - CPU-side simulation on the game's threads (traffic AI, physics, collision);
    - GPU and CPU synchronization.

    The game thread's frame time and a host stack sample, both taken during a stall, would
    separate these.

### Hand test 4: a career race, a restart, progress kept (2026-10-23)
- **The run:** build 184d87f, the real user folder, 720p internal (`RECARBON_RESOLUTION`; the
  saved setting is Auto). Both sessions were put in fullscreen by the system.
  - Session 1: `2026-10-23_13-31-57`, 13:31:57–13:47:42.
  - Session 2: `2026-10-23_13-48-17`, 13:48:17–13:56:14.
  - The times come from the log file names, the relative timestamps and the files' modification
    times, which agree. The times the user noted (save 13:42, session 1 ending 13:44, session 2
    13:51–13:54) are 2–5 minutes earlier; the log times are used here.
- **User's results:**
  - Profile ALIAS_TESTER. Race Zone Heat 1 (Circuit): 1st place, no retries.
  - Cash went from $5,000 to $10,200.
  - All four criteria passed: the race finished and the results screen showed; the AI opponents
    moved, steered and finished; session 1 quit cleanly; the cash and career progress were still
    there after the restart.
- **The log agrees:**
  - Both sessions exited with code 0. Session 1 quit through the Quit Game menu at 944.8 s, and
    session 2 by closing its window at 477.0 s.
  - Neither session wrote a hang or crash report, and the watchdog reported nothing; its only
    log line is the threshold it announces at startup. There were no strict stubs and no
    "not a recompiled function" errors.
  - The only failed opens were the disc files that don't exist (`sound\NISRevData\*.bin`), which
    the game skips as before.
  - Audio: no gaps longer than 50 ms. `settings.toml` is unchanged (hash `4bb7535e…`).
  - Session 1's race window, inferred from the cutscene files (the log has no race events):
    - the `intro04ng02` cutscene was requested at 595.6 s (13:41:53);
    - the `endingcareer11ng` cutscene was requested at 845.0 s (13:46:02);
    - the `Boss_Tuner_Kenji` movie followed at 887.5 s.
- **Save evidence, and where it stops:**
  - Session 1 opened the save at 23.4 s and 81.4 s. It opened it again at 916.8 s (13:47:14),
    after the race and the movie and 28 s before quitting.
  - The `[file] open` line doesn't record whether a file was opened for writing, so the log can't
    prove that session 1 wrote the save.
  - No hash was taken between the two sessions. The save's last write time, 13:54:26, matches
    session 2's open at 369.5 s, so its hash now (`287ce0f8…`, against `c6c4a362…` before the
    test) is session 2's write and says nothing about session 1.
  - Session 2 found the save at 1.8 s and opened it at 18.2 s.
  - Persistence therefore rests on the user seeing the race's cash and progress after the
    restart. Session 1 is the only place they could have come from: no other session ran in
    between.
  - Session 2 also played the `intro05ng02` cutscene (175.2 s), changed render targets
    (191.6 s and 357.2 s) and saved at 369.5 s. The user's report doesn't cover this part.
- Next time, take the save's hash between sessions. The route repeats for M7.7 will do that.
- **Open: frame spikes in the race (hand test 4, cause unconfirmed).**
  - **User's observation:** one brief stutter while actively driving in the race, not during
    loading or the results screen. They didn't note a time.
  - **The race window as a whole** (595–845 s, about 4 minutes, so not the 10 minutes M7.5
    asks for): 118 two-second intervals averaging 58.5 fps. The lowest interval was 29.6 fps, at
    610.7 s, just after the race's opening cutscene.
  - **Two kinds of spike, apart from loading and cutscenes.** Which one the user felt can't be
    pinned down without a time:
    - **Kind 1, submit-bound:** most of the frame was spent in the command processor's
      submission (`submits … ms`).
      - Frames of about 1 s: 607.8 and 608.9 s (right after the cutscene), then 802.3, 809.6
        and 811.3 s (each 1016.5–1033.3 ms, with 1003–1014 ms in submissions).
      - A run of 67–117 ms frames at 778.2–780.0 s and 800.9–801.0 s, with 48–98 ms in
        submissions.
      - None of these frames created a shader, a pipeline or an image. File reads were 0 KB,
        or 128 KB in three of the 1 s frames.
    - **Kind 2, command processor idle:** one 116.6 ms frame at 796.2 s, idle 100.1 ms, with
      4.1 ms in submissions. This is the same pattern as hand test 3's 333 ms free-roam stall
      (idle 311 ms); kind 1 is not.
  - **The 1 s frames elsewhere:** 7 in session 1 and 7 in session 2. Session 2's include 23.7
    and 27.1 s while loading the profile, and frames at 193.5, 194.8 and 365.7 s.
    - Earlier logs from today, same build and also fullscreen by the system: one in session
      `10-34-04`, none in `10-57-39` and none in hand test 3's `11-36-32`.
    - Two of session 2's frames (174.5 and 176.2 s, around a cutscene) are the other way round:
      idle and short of draws, with ~1 s in submissions.
    - The durations cluster just above 1.0 s (1003–1050 ms).
  - **What the code says about "submits ms"** (`gpu/renderer.cpp`, the submit path): it counts
    waiting for the Vulkan queue mutex plus `vkQueueSubmit` itself. On macOS the presenter holds
    that mutex across `vkQueuePresentKHR`, which is where MoltenVK takes the CAMetalLayer
    drawable (comment in `video/presenter.cpp`, swapchain setup).
    - A present waiting about 1 s for a drawable, with the command processor queued behind it,
      would fit kind 1. That is a candidate, not a finding: the hitch line doesn't split the
      lock wait from `vkQueueSubmit`, and no present duration was logged.
    - `RECARBON_LOG_LATENCY=1` already logs the split ("queue lock" vs "vkQueueSubmit" ms/s).
      A run with it, plus the present's duration, would test this without a code change to the
      game path.
  - The other candidates from hand test 3 (shader and pipeline compilation, asset streaming,
    CPU simulation, GPU sync) also stay open, kind 2 especially. No fix has been attempted.
  - **Measured: kind 1 reproduced in isolation (lat01b, lat01c, lat01d; 2026-10-23).** Setup:
    the scripted Quick Race in the isolated user folder, `RECARBON_LOG_LATENCY=1`,
    `RECARBON_VBLANK_LOG`, and macOS `sample` taking host stacks.
    - **Every run had kind 1, and not only in the race.** Lat01c had 419–1034 ms frames on the
      title screen at 53 draws per frame.
    - **It's waiting for the lock, not a slow submit:** in each such frame, its submission time
      matches the latency log's queue-lock time for the same second.
      - lat01b 150.0 s: 1050 ms frame, lock 1038.8 ms/s, `vkQueueSubmit` 25.6 ms/s.
      - lat01d 143.9 s: 1318 ms frame, lock 1060.6 ms/s.
      - Each has a gap of the same length between presents in the present log.
    - **Who holds the lock** (lat01d, sampled over the whole run at 8.55 ms per sample):
      - The presenter thread spent 282 samples, about 2.41 s, blocked in
        `-[CAMetalLayer nextDrawable]`, waiting on a semaphore with a timeout. The path is
        `RunFrame` → its own `vkQueueSubmit`, made while holding the queue mutex → MoltenVK's
        `getCAMetalDrawable` → `nextDrawable`.
      - The command processor spent 287 samples, about 2.45 s, waiting for the mutex in
        `SubmitRecorded`.
      - The latency log's lock waits for the run total 2.78 s.
      - So MoltenVK takes the drawable when the command buffer first uses the swapchain image,
        inside `vkQueueSubmit`, not in `vkQueuePresentKHR` as the comment in `presenter.cpp`
        said (since corrected).
    - **What this establishes:** kind 1 is the presenter waiting for a free CAMetalLayer
      drawable while holding the queue lock, with the command processor queued behind it.
      `sample` only gives totals, so the match is between totals; individual spikes weren't
      caught one by one.
    - **Still unknown:**
      - Why no drawable is free for about a second.
      - Whether `nextDrawable` gives up at its timeout. The spikes cluster at 1.00–1.07 s, and
        nothing about it is logged.
      - Whether hand test 4's 1 s frames are the same thing. They have the same signature, but
        that run had no latency split.
    - **Kind 2** (command processor idle, no lock wait) happened in every run too, and its cause
      is still unknown.
    - `vkQueueSubmit` itself (MoltenVK encodes at submit) took 300–390 ms per second during the
      race. That cost is spread across frames and isn't the cause of a spike.
    - No runtime change was made.
  - **MoltenVK diagnostics, and an A/B on them (2026-10-23).**
    - **Setting it up:**
      - `MVK_CONFIG_PERFORMANCE_TRACKING=1` with
        `MVK_CONFIG_ACTIVITY_PERFORMANCE_LOGGING_STYLE=1` logs nothing by itself. Its lines are
        info level, so `MVK_CONFIG_LOG_LEVEL=3` is needed too.
      - With all three set, MoltenVK writes a "Retrieve a CAMetalDrawable" line for every fetch,
        about 760 lines a second in all. That fills the session log's 16 MB cap in about 20 s.
      - `…_LOGGING_STYLE=0` with `MVK_CONFIG_PERFORMANCE_LOGGING_FRAME_COUNT=60` writes about 10
        lines a second, with cumulative averages and counts.
      - MoltenVK confirms 3 swapchain images (2560×1440).
    - **Each configuration was checked in the running process's environment**, all on the same
      route, build, display and isolated user folder:

      | MoltenVK | Runs with a ~1 s lock stall |
      |---|---|
      | Tracking off (lat01b–e, ab_log1 with info logging) | 5 of 5 |
      | Tracking on (lat02 and lat02b logging per event or every 60 frames; ab_trk1 and ab_trk2 logging off) | 1 of 3 |

      - ab_trk2, with tracking on, had 10 seconds with at least 500 ms of lock wait, and
        29.3 s of lock wait in all.
      - Total lock wait varied from 0.35 s to 33.7 s on identical settings.
    - **What that shows:**
      - Neither tracking nor logging prevents the stall, so MoltenVK tracking is no fix.
      - That the runs without a stall all had tracking on is a weak association; with 9 runs
        it can't be told apart from chance, and tracking overhead can't be separated from
        logging overhead either.
      - MoltenVK's timing of each fetch was never captured during a stall. Across about 67,000
        fetches with tracking on, none took over 44.4 ms.
    - **A logging gap:** the slow-frame log has holes; lat01e has no line for its 1,084 ms frame
      at 378–383 s. Stalls are therefore counted from the latency log, the gaps between
      presents and each interval's worst frame.
  - **Still open, and on the alpha's known-issues list (ROADMAP M7):**
    - why no drawable is free for about a second;
    - the GPU-idle stalls, which happen in every configuration and are a separate issue.

    Both are to be revisited before a public release, or sooner if they become frequent.

### EA audio CPU budget (SpeedBreaker v0.1.8's fix for MW)
- Carbon's sound engine is RenderWare Audio Core (`rw::audio::core::System`, `Snd9Service`,
  thread "RWAudioCore Dac"), not MW's EA SND. MW's addresses and code don't apply.
- **Black-box test** (temporary probe): the RWAudioCore Dac thread was held for 20, 50, 100 and
  300 ms during the title music and during a race with the engine at steady revs.
  - The mix's level stayed in its normal range after every stall: race RMS 0.10–0.22 before and
    after.
  - MW's symptom, a persistent 4.5–8 dB drop until the next gear change, wasn't seen.
- **Status:** no evidence of voice expulsion in Carbon (hypothesis level). Caveats: one
  scenario; the whole-mix level could hide a single dropped voice; the thread a budget would
  measure might not be the one held. It stays open for human gameplay testing.

### Needs a human
- Steering, so: controls, physics, AI racing and finishing, a full career race with progress
  saved, and 10 minutes of racing frame times. The user has already driven the car with the
  keyboard (hand test 1).

## 6d. Display and rendering resolution audit (2026-10-24, no rendering change)

**Method:**
- Each case is an isolated launch, with its own HOME and the audio muted, running 75 s
  in the front end. The figures are medians of the `[perf]` samples after the first five.
- A preflight refused to start while any other ReCarbon, build, test or CPU load was running,
  and waited for the 1-minute load average to fall under 2.5. Thermal state and power source
  were recorded.
- Every game was checked gone after its run.

**Machines:**
- **MacBook Air M5** (2026-10-24, two repetitions): on AC power, no thermal warning. The
  first M5 measurements (2026-10-23) were discarded: three orphaned `install_check.sh` games
  were running at 86–91% CPU (ROADMAP M8, "Test tooling").
- **M1:** measured 2026-10-23 with no orphans, so its numbers stand.

**Results.** The M5 figures are the two repetitions' medians, each from 13 samples. The
front end runs at the game's own 30 fps in every case.

| Case | Window / swapchain (pixels) | Internal resolution | M5 GPU busy | Memory | M1 GPU busy |
|---|---|---|---|---|---|
| 1x | 2560x1440 | 1280x720 | 2.2 / 2.3 ms | 109 MB | 5.0 ms |
| 1.5x | 2560x1440 | 1920x1080 | 5.2 / 5.3 ms | 726 MB | — |
| 2x | 2560x1440 | 2560x1440 | 6.0 / 6.1 ms | 868 MB | 10.6 ms |
| 3x | 2560x1440 | 3840x2160 | 11.1 / 10.1 ms (9.2–14.5) | 1274 MB | 13.3 ms |
| Auto | 2560x1440 | 2x2 | 5.9 / 6.0 ms | 868 MB | 9.4 ms |
| Window request 1920x1080 | 2940x1782 (clamped to the screen) | Auto 2x2 | 5.6 / 5.2 ms | 868 MB | — |
| Window request 1440x900 | 2880x1782 | Auto 2x2 | 5.7 / 5.7 ms | 868 MB | — |
| Fullscreen, Auto | 2940x1782, then 2940x1846 (1 rebuild) | Auto 2x2 | 5.6 / 5.2 ms | 868 MB | 10.5 ms (2880x1800) |

**Verified support:**
- **Internal resolutions 1x, 1.5x, 2x and 3x** all work: no stubs, no Vulkan errors, no
  fallbacks.
- **Auto** picks 2x2 on both Macs (budget 4, an integrated GPU), whatever the window or
  fullscreen size measured.
- **The swapchain is native Retina pixels.** Windows are clamped to the screen: a 1920x1080
  request gives a 2940x1782 window on the M5's 1470x956-point panel. Fullscreen is borderless
  desktop fullscreen: 2940x1846 on the M5, 2880x1800 on the M1.
- **The 16:9 picture is letterboxed or pillarboxed** on any other shape, a 16:10 panel
  included. Aspect Ratio's "Fill Screen" can't fill, because Carbon has no view-scaling hooks.
- **The UI scales with the window's height** (height/720, clamped 0.75–4).

**Corrected from 2026-10-23:**
- The M5's figures above replace that day's, which were inflated by the orphaned games
  (1x 3.9 ms then, 2.2 ms now).
- The 1920x1080 window request's four swapchain rebuilds and 26.3 fps were seen only in the
  contaminated session. Both clean runs had 0 rebuilds and 30 fps.

**Not verified:**
- **Ultrawide:** no ultrawide hardware, and a window larger than the screen is clamped.
- **Scaled 3D:** only the intro movie (720p video) was captured, so the visual correctness of
  scaled 3D rendering in gameplay is unchecked.
- **Racing at a higher internal resolution:** every hand test ran at 1x (`run_attempt.sh`
  forces it), so no racing frame times exist above 1x.

**Proposed enhancements** (none made; they are rendering changes, outside today's scope):
- **Auto's picture area:** size it for the letterboxed picture (presenter.cpp FrameRegion
  assumes a filled one). On a Mac this changes nothing at 16:9 or 16:10; it would on
  ultrawide and 4:3.
- **Aspect Ratio's description:** it promises a wider view on an ultrawide, which Carbon
  can't give yet.
- **The scaled-pitch window** (640–1280, tuned on MW): establish it per Carbon render target.
- **Racing measurements:** scripted race runs at 2x and Auto, with frame-time logs.
- **Testability:** move the budget choice and the pitch window into headers, so they can be
  tested (`reference/TEST_COVERAGE.md`).

## 7. Feasibility: Xbox 360 Carbon

### Why it is plausible (evidence from M0)
- **Shared codebase:** the original PE name is `NfsMWRelease.pe`. Carbon was built from the MW codebase lineage, so MW's EA stream-player and render-queue patterns are likely to recur (inferred from the name; to verify in code).
- **Generic runtime:** the CPU, kernel, GPU, audio and input layers are hardware-level and title-agnostic.
- **Low import burden:** 11 new imports beyond MW.
- **Recompiler inputs:** the helper patterns, `.pdata` and code-section handling all line up with the survey pipeline.
- **Same movie path as MW:** WMV/XMV.

### Risks
| # | Risk | Likelihood | Mitigation |
|---|---|---|---|
| R1 | ~~New PPC/VMX instructions or recompiler edge cases~~ | **Retired in M1** | The whole executable translates (§3). Remaining: semantic bugs show up only at runtime. |
| R2 | Texture formats, memexport, EDRAM aliasing not covered by the renderer | Medium-high | Log unknown formats and dropped draws from day one. Port the missing paths from Xenia (BSD-3). |
| R3 | `XexLoadImage`/`XexGetProcedureAddress`/`XexGetModuleSection` actually used at runtime | Medium | Static call sites exist (§3 limitations), apparently for XAM lookups. Implement or log in M3. |
| R4 | ARM memory-ordering bugs in lock-free engine code (MW needed 7 fence hooks) | Medium | Patch 0005 fences. Find Carbon's equivalent queue early. Run with TSan-style stress. |
| R5 | MacBook Air thermals (fanless), with about 1.37× MW's code | Medium | 1x scale, Auto 30/60, `-O2` for the recompiled library, profile hot paths. |
| R6 | ~~Compile time / memory of the generated code~~ | **Retired in M2** | 98 s clean build, 2.6 GB peak at `-j8` on the MacBook Air (§4). |
| R7 | Licence contamination of disc assets or generated code in git | Low | `.gitignore` covers `/game`, `/ppc` and `/tools`; checked with `git status`. |

### Verdict
**Conditionally feasible, medium risk.** M0–M2 found no blocker; the CPU-translation and build risks are retired. Everything from here is runtime behaviour, which is untested. Expect first boot to the front end after roughly the effort SpeedBreaker spent on MW, minus the generic runtime work. Playable racing is plausible but not guaranteed.

## 8. Recommendation
Proceed to ROADMAP **M4** once approved. Start with why movie frames are black: the XMV player's GPU path, i.e. its textures and resolves. Then take the front end past the loading screen with input.

---

## Appendix A — Original Xbox disc (superseded)

The first image supplied, `Need for Speed - Carbon (USA).iso`, was the **original Xbox** release:
- ISO: 3,033,661,440 B, SHA-256 `ce448b1dc14aaed74e6901de3738864ced60c4f8f4284304d1fb902ec7870153`. XDVDFS at offset 0.
- Executable: `default.xbe`, SHA-256 `d7c9398a312b70273036d4da89191648b8a6707dd86e206fde9fa06250711728`.
  - Title `NFS_Carbon`, 0x4541009E (EA-158).
  - x86-32, XDK 5849, D3D8/DSOUND statically linked.
  - 126 kernel ordinals.
  - Movies are VP6 in EA `MVhd` containers.

None of SpeedBreaker's CPU (PowerPC), GPU (Xenos), audio (XMA) or kernel (360) layers apply to it. A port would need a new x86→arm64 static recompiler, NV2A/D3D8 and DSOUND HLE. Possible references are xemu and Cxbx-Reloaded, which are GPL-2.0, so licence compatibility must be checked. **Not practical**, and out of scope.
