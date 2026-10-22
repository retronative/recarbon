# ReCarbon — Feasibility of a Native Apple Silicon Port of Need for Speed: Carbon

Date: 2026-10-22. Host: arm64, macOS 27.0.1. Reference: SpeedBreaker v0.1.7 (`31849f5`).

## TL;DR

| Question | Answer |
|---|---|
| Is the input the Xbox 360 release? | **Yes.** `default.xex` is XEX2, title **0x454107EC**, PowerPC, retail-encrypted and LZX-compressed. It decrypts and decompresses with XenonRecomp's loader (§1). |
| Does it fit SpeedBreaker's pipeline? | **Yes, with no blockers found in M0.** All 8 compiler-helper patterns match, and every code section is covered by `.pdata`. 195 of 369 imports are already handled; 163 are imports MW also stubs, and only 11 are new. The original PE name is `NfsMWRelease.pe`, the same codebase lineage as MW. |
| Feasibility | **Conditionally feasible, medium risk.** Carbon has about 1.37× MW's code. The game-specific layer (hooks, widescreen, render queue) must be re-found. |
| Recompilation (M1) | **Complete.** The whole executable translates: 72,864 functions, 0 recompiler errors or warnings, 0 failure markers, all structural checks pass, and the output is reproducible byte for byte (§3). |
| Next step | ROADMAP **M2**: native compilation and link against SpeedBreaker's runtime (awaiting approval). |

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

## 4. Feasibility: Xbox 360 Carbon

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
| R6 | Compile time / memory of the generated code (about 31k functions) on a laptop | Medium | Split into translation units, tune `ninja -j`, ccache. |
| R7 | Licence contamination of disc assets or generated code in git | Low | `.gitignore` covers `/game`, `/ppc` and `/tools`; checked with `git status`. |

### Verdict
**Conditionally feasible, medium risk.** M0 and M1 found no blocker; the CPU-translation risk is retired. Expect first boot to the front end after roughly the effort SpeedBreaker spent on MW, minus the generic runtime work. Playable racing is plausible but not guaranteed.

## 5. Recommendation
Proceed to ROADMAP **M2** (native compile and link of `ppc/` against SpeedBreaker's runtime with the MW game layer removed) once approved.

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
