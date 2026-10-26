# Contributing to ReCarbon

Thank you for helping. ReCarbon is an unofficial fan project: a static recompilation of
*Need for Speed: Carbon* (Xbox 360) to native Apple Silicon macOS, built on
[SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker). Bug reports, tests on
different Macs, documentation fixes and code are all welcome.

For a security problem, don't open an issue: follow [SECURITY.md](SECURITY.md).

## Ground rules: no game files

These rules protect the project. A contribution that breaks them can't be accepted.

- **Never commit, attach, upload or link game files**: the disc image, `default.xex`, decrypted
  or extracted files, recompiled output (`ppc/`), saves, or anything derived from them beyond
  the metadata described in the [README](README.md).
- `game/`, `ppc/`, `tools/` and `build/` are git-ignored. Keep them that way.
- Don't ask for game files, and don't share them in issues or pull requests.
- Only contribute work you wrote or have the right to contribute. Never use code from leaked or
  proprietary sources.

## Reporting a bug

Open an issue at [github.com/retronative/recarbon/issues](https://github.com/retronative/recarbon/issues).
First check [Known issues](docs/known-issues.md) and [Troubleshooting](docs/troubleshooting.md).

Please include:

- what you did, what you expected and what happened;
- the ReCarbon version, your Mac model and macOS version;
- the bug report `.zip`, if you're comfortable sharing it (Settings > Advanced > **Save Bug
  Report**). It holds the logs, a system summary, your `settings.toml` and a screenshot, with
  home-folder paths shown as `~`.

Reports from Macs other than the MacBook Air M5 are especially useful, as is anything about
physical controllers, which are not yet verified.

## Building and testing

You need an Apple Silicon Mac with Homebrew and your own disc. The steps are in the README's
[Building from source](README.md#building-from-source) and in [docs/developers.mdx](docs/developers.mdx).

Before you open a pull request, run the checks your change touches:

```sh
tests/run_instr_tests.sh                                     # recompiler instruction tests
/opt/homebrew/bin/python3 survey/validate_recomp.py game/survey/recomp.log   # after a survey
ninja -C build/main && ctest --test-dir build/main          # unit tests (-DRECARBON_TESTS=ON)
```

For the website (`website/`): `npm run check` and `npm run build` (see
[website/README.md](website/README.md)).

## Code conventions

- **Fix causes, never symptoms.** Fix recompiler problems with correct config, switch tables,
  function bounds or generic XenonRecomp patches with documented semantics. Never suppress
  errors or warnings.
- **A new instruction** needs a XenonRecomp patch in `patches/xenonrecomp/` and hand-computed
  tests in `tests/instr/`, with expected values taken from the PowerPC ISA, never from the
  recompiler's own output.
- **Never port a SpeedBreaker hook** that names a *Most Wanted* address (`sub_8…`, `0x82…`). The
  same address in Carbon is unrelated code or data. Carbon hooks are found in Carbon's image and
  documented with their evidence.
- After adding or implementing a kernel or XAM import, rerun `scripts/gen_import_stubs.py` and
  commit the regenerated `runtime/kernel/import_stubs.cpp` with it.
- Files taken from SpeedBreaker keep their attribution. Mark ReCarbon's changes in comments.
- Match the style of the code around your change, and keep changes small and focused.

## Documentation

`docs/` is the player documentation, both on GitHub and on the website's `/docs/` pages. Write it
to read on GitHub: link between pages by file (`faq.md`), and use GitHub alerts (`> [!NOTE]`) for
callouts. The website converts them. Never edit the website's `*.generated.json` files: they are
generated from the source (`npm run gen:controls`, `npm run gen:progress`), and the build fails
when they drift.

## Commits and pull requests

- One logical change per commit. Every commit must build and pass its tests, so history stays
  bisectable.
- [Conventional Commits](https://www.conventionalcommits.org/), single line: `feat:`, `fix:`,
  `refactor:`, `test:`, `docs:`, `chore:` (e.g. `fix(website): …`).
- Don't commit debugging attempts, temporary instrumentation or speculative workarounds.
- In the pull request, say what changed, why, how you tested it and on which Mac.

## License

ReCarbon is GPL-3.0-or-later (see [COPYING](COPYING) and [NOTICE](NOTICE)). By contributing, you
agree that your contribution is licensed under the same terms.
