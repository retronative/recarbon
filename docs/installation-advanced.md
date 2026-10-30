---
title: Advanced installation
description: Check ReCarbon's downloads, install the game from Terminal, the other installer commands, where ReCarbon keeps its files, and how the installer was tested.
---

For scripts, SSH sessions and troubleshooting. To install ReCarbon the usual way, follow
[Installation](installation.md).

## Check the downloads

Each file on the [v0.1.3 release](https://github.com/retronative/recarbon/releases/tag/v0.1.3)
has a `.sha256` file next to it. In Terminal, in the folder with both files:

```sh
shasum -a 256 -c ReCarbon-0.1.3-macos.dmg.sha256
```

It must print `ReCarbon-0.1.3-macos.dmg: OK`.

| File | SHA-256 |
|---|---|
| `ReCarbon-0.1.3-macos.dmg` | `b0cb142a90d5fe86ee5bc90234c78a824a1a4ff240fe8ebc5f261913e446a071` |
| `ReCarbon-0.1.3-third-party-source.tar.xz` | `2277ed03badeb2da9402e2c186a2ad6d54c255f9a7284d5051b2d50885e784cb` |

## Why macOS blocks ReCarbon

ReCarbon is **signed ad hoc**: it isn't signed with an Apple Developer ID or notarized by Apple.
So the first time you open a downloaded copy, macOS blocks it, and you allow it once
([Installation](installation.md#3-if-macos-blocks-recarbon)). A build you make yourself on your
own Mac isn't blocked.

> [!CAUTION]
> Don't turn Gatekeeper off (`spctl --master-disable`) and don't remove the download's quarantine
> attribute (`xattr`). Allow only ReCarbon, only once, and only a copy you trust.

## Install the game from Terminal

The first-run installer also works from Terminal:

```sh
ReCarbon --install <image.iso or extracted folder> [--dest <folder>]
```

Without `--dest`, the game goes into ReCarbon's data folder:
`~/Library/Application Support/ReCarbon/game`. Ctrl-C cancels cleanly and removes the partial
copy. Other commands:

| Command | Does |
|---|---|
| `ReCarbon --verify <image or folder>` | Hash-checks every file without copying |
| `ReCarbon --find-images [folder…]` | Lists disc images in the usual places, or in these folders |
| `ReCarbon --where` | Prints where the game install was found, and the data folders |
| `ReCarbon --portable on` / `off` | Keeps the data and the game in ReCarbon's own folder (portable mode), or moves them back |

The disc these commands check for, its hashes and the image formats they read:
[Supported disc images](disc-images.md).

## Where ReCarbon keeps things

| What | Where |
|---|---|
| Settings | `~/Library/Application Support/ReCarbon/settings.toml` |
| Saves, logs, installed game | `~/Library/Application Support/ReCarbon/` (`logs/`, `game/`) |
| Caches (shaders, pipelines; safe to delete) | `~/Library/Caches/ReCarbon/` |

## How the installer was tested

The installer copies the files it needs from your disc image and checks each one against the
file list of the USA Xbox 360 edition (a manifest of hashes): 78 files, 5.74 GB. Nothing is downloaded.

- In the release-candidate checks on a MacBook Air M5 (2026-10-25), the first-run installer
  copied and verified the 78 files (5.74 GB) from an ISO in 49–86 seconds, in the main account and
  on a fresh macOS account, and the game started.
- In the earlier packaging check (2026-10-23), `--install` took 4 seconds, and when the install
  folder had moved, the app found the game in the default folder and started.
- An install on a MacBook Air M1 wasn't tested.

The first race after a fresh install can hitch for a moment (up to about half a second) while
ReCarbon fills its shader cache; later races are smoother ([Known issues](known-issues.md)).
