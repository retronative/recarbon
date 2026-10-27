# Security policy

ReCarbon is an unofficial fan project maintained by volunteers. We take security reports
seriously and handle them as fast as we can, but we can't promise response times.

## Supported versions

ReCarbon is in alpha. Only the latest release gets security fixes.

| Version | Supported |
|---|---|
| 0.1.x (latest alpha) | Yes |
| Older builds, builds from source of older commits | No: update to the latest release |

## Reporting a vulnerability

**Please don't open a public issue for a security problem.**

Report it privately through GitHub:
[Report a vulnerability](https://github.com/retronative/recarbon/security/advisories/new)
(the repository's **Security** tab, then **Report a vulnerability**). Only the maintainers can
read it.

Please include:

- what the problem is and what an attacker could do with it;
- the ReCarbon version (Settings > Advanced shows it) or the commit, your Mac and macOS version;
- the steps to reproduce it, or a proof of concept;
- a bug report `.zip` (Settings > Advanced > **Save Bug Report**) if it helps and you're
  comfortable sharing it.

**Never attach game files, disc images or saves.** If a crafted file is part of the problem,
describe how to make it instead.

We'll confirm we got the report, work on a fix in private, and publish it with a GitHub security
advisory once a fixed release is out. Tell us if you'd like to be credited.

## What's in scope

- The ReCarbon app and its runtime (`runtime/`): for example, how it reads disc images, extracted
  folders, saves and `settings.toml`, which may come from untrusted sources.
- The installer and the checks it runs on your disc image.
- The release downloads: the `.dmg`, its `.sha256` file and the third-party source archive on the
  [GitHub releases](https://github.com/retronative/recarbon/releases).
- The build and release scripts (`scripts/`), the recompiler patches (`patches/`) and the website
  (`website/`).

## What's out of scope

- **Gatekeeper's warning.** ReCarbon is signed ad hoc and not notarized, so macOS asks you to
  choose **Open Anyway** once. This is expected and documented in
  [docs/installation.md](docs/installation.md). Never turn Gatekeeper off or remove quarantine
  attributes.
- **Bugs in the original game's own logic** that need no untrusted input, and cheats or exploits
  in gameplay.
- **Problems in upstream projects** (SpeedBreaker, XenonRecomp, SDL, MoltenVK, FFmpeg and the
  others in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)). Report those to the project
  concerned. Tell us too if ReCarbon ships the affected version.
- Requests for game files or disc images. ReCarbon never distributes them.

## Verifying a download

Every release publishes a SHA-256 checksum next to the `.dmg`. Check it before opening the app:

```sh
shasum -a 256 -c ReCarbon-0.1.2-macos.dmg.sha256
```

Download ReCarbon only from [github.com/retronative/recarbon/releases](https://github.com/retronative/recarbon/releases).
Any other copy isn't ours.
