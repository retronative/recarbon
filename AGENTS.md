# AGENTS.md

ReCarbon: a static recompilation of *Need for Speed: Carbon* (Xbox 360, title 0x454107EC) to
native arm64 macOS. It is built on SpeedBreaker (the NFS: Most Wanted port), and is GPL-3.0-or-later.

## Workspace

| Path | Role | Rules |
|---|---|---|
| `../speedbreaker/` | Upstream reference (SpeedBreaker v0.1.7) | **Read-only.** Never run its scripts: they write into it. |
| `../rom_extracted/` | The player's extracted disc | **Read-only.** |
| `game/files` | Symlink → `../../rom_extracted` | git-ignored |
| `game/survey/` | Decrypted image, survey/recompiler logs | git-ignored, proprietary-derived |
| `ppc/` | XenonRecomp output (generated game code) | git-ignored, never commit |
| `tools/` | XenonRecomp (pinned + patches), extract-xiso, xexsurvey | git-ignored |
| `runtime/` | SpeedBreaker's runtime with the MW game layer replaced by `game/title_hooks.cpp` | identity changes only, outside `game/` |
| `config/` | `recarbon.toml` + switch tables | committed: addresses only, no game bytes |
| `survey/` | Analysis pipeline (from SpeedBreaker, parametrized by `survey/layout.py`) | |
| `patches/xenonrecomp/` | XenonRecomp patches applied by `scripts/setup_tools.sh` (0001–0008 SpeedBreaker, 0009+ ReCarbon) | |
| `tests/` | Instruction tests: SpeedBreaker's `nfsmw_*.s` and ReCarbon's hand-computed `carbon_*.s` | |
| `reference/` | Feasibility, architecture, roadmap, header parsers | `docs/` is reserved for future project docs |

## Commands

```sh
scripts/setup_tools.sh     # build tools/ (idempotent)
scripts/survey.sh          # xexsurvey → XenonAnalyse → table fixes → XenonRecomp → validate
/opt/homebrew/bin/python3 survey/validate_recomp.py game/survey/recomp.log   # structural check
tests/run_instr_tests.sh   # assemble tests/instr/*.s, recompile, run (fails on skipped tests)
cmake -S . -B build/main -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
ninja -C build/main -j8    # build/main/runtime/ReCarbon (about 100 s clean, 2.6 GB peak)
```

- The survey scripts need Python 3.11+ (`tomllib`). `/usr/bin/python3` is 3.9, so use
  `/opt/homebrew/bin/python3`. Run them from the repo root.
- After changing a patch in `patches/xenonrecomp/`, delete the `recarbon` branch in
  `tools/XenonRecomp` and re-run `setup_tools.sh`. It applies patches only when the branch is
  missing.
- `survey.sh` resets the generated config blocks and the inline tables first, so it is
  reproducible. Two runs from scratch must produce byte-identical `config/` and `ppc/`.
- Generated config blocks in `config/recarbon.toml` are rewritten by
  `survey/leaf_switch_bounds.py` and `survey/find_entry_points.py`. Hand-written keys go above
  them.

## Conventions

- A new instruction needs a XenonRecomp patch and hand-computed tests in `tests/instr/`, with
  expected values from the ISA, never from the recompiler's output.
- Fix recompiler problems with real fixes: correct config, switch tables, function bounds, or
  generic XenonRecomp patches with documented semantics. Never suppress errors.
- Never port a SpeedBreaker hook, override or guest-memory write that names an MW address
  (`sub_8…`, `0x82…`). The same address in Carbon is unrelated code or data. Carbon hooks are
  found in Carbon's image and documented with their evidence.
- After adding or implementing a kernel/XAM import, rerun `scripts/gen_import_stubs.py`.
- Files taken from SpeedBreaker keep their attribution. Mark ReCarbon changes in comments.
- Milestones (`reference/ROADMAP.md`) are gated. Don't start the next one without the user's
  approval.

## Git

- One atomic commit per independently verified bug fix or logical change. Group tightly
  related fixes into one commit. There is no fixed commit count per milestone.
- Never commit debugging attempts, temporary instrumentation or speculative workarounds.
- Before each commit, run the relevant tests and regression checks (`ctest`,
  `survey/validate_recomp.py`, `tests/run_instr_tests.sh`, a launch with
  `scripts/run_attempt.sh`, as the change needs). Every commit must build: keep history
  bisectable (e.g. `runtime/kernel/import_stubs.cpp` regenerated in the same commit as
  the imports it stops stubbing).
- Conventional Commits (`feat:`, `fix:`, `refactor:`, `test:`, `docs:`, `chore:`),
  single-line messages, no co-author trailers. Review `git status`/`git diff` first.
- Never commit game data, decrypted images, `ppc/`, `game/`, `tools/` or build output.
- Commit locally only. Never push, amend or rewrite history without approval.
- Record debugging attempts and findings in `WORKLOG.local.md` (untracked, git-ignored).
