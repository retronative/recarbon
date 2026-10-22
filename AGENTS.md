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
- Files taken from SpeedBreaker keep their attribution. Mark ReCarbon changes in comments.
- Milestones (`reference/ROADMAP.md`) are gated. Don't start the next one without the user's
  approval.
- Git: 1–3 Conventional Commits per verified milestone, single-line messages, no
  co-author trailers. Review `git status`/`git diff` first. Never commit game data,
  `ppc/`, `game/` or `tools/`. Never push or amend without approval.
