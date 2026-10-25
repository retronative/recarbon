---
title: Installation
description: Install Need for Speed Carbon into ReCarbon from your own Xbox 360 disc image, with the first-run installer or the command line.
---

ReCarbon installs the game **from your own disc image**: it copies the files it needs and checks
each one against a manifest of hashes. Nothing is downloaded.

> [!WARNING]
> **Alpha**
>
> There is no public release yet ([Release status](download.md)). These steps describe the current
> build. A clean install from a disc image on a fresh macOS user account was tested on a MacBook
> Air M5 (M8); an install on a MacBook Air M1 wasn't tested.

## 1. Make a disc image

Make an image (`.iso`) of **your own** Xbox 360 disc of *Need for Speed: Carbon*, or extract it to a
folder. This site doesn't explain how to obtain game files in any other way, and the project won't
help with that.

## 2. Install the game

### With the first-run installer

Start ReCarbon. If no installed game is found, the installer screen opens:

1. It looks for disc images in the usual places, or you browse for yours.
2. It checks the image.
3. It copies and verifies the files, with progress: 78 files, 5.74 GB.
4. Then the game starts.

### From the command line

The same installer works from Terminal, for scripts and SSH sessions:

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

In the release-candidate checks on a MacBook Air M5 (2026-10-25), the first-run installer copied
and verified the 78 files (5.74 GB) from an ISO in 49–86 seconds, in the main account and on a
fresh macOS account, and the game started. In the earlier packaging check (2026-10-23),
`--install` took 4 seconds, and when the install folder had moved, the app found the game in the
default folder and started.

## 3. First launch

ReCarbon's builds are signed ad hoc and not notarized by Apple. A copy that arrives through a
browser download is blocked at its first launch: macOS says it can't check the app. Then open
**System Settings > Privacy & Security**, find the message about ReCarbon, choose **Open Anyway**
and confirm. You do this once. Don't turn Gatekeeper off. (A build made on your own Mac isn't
blocked.)

At the main menu, the game shows **"Cannot connect to Xbox Live!"** at every launch: press **A**
(**Space** on the keyboard) to continue ([Known issues](known-issues.md)).

The first race after a fresh install can hitch for a moment (up to about half a second) while
ReCarbon fills its shader cache; later races are smoother.

## 4. Play

Start ReCarbon. Press **F1** (Fn+F1 on a Mac keyboard, or Cmd+,) for ReCarbon's Settings at any
time; see [Controls](controls.mdx) and the [Garage](https://retronative.github.io/recarbon/garage/).

## Where ReCarbon keeps things

| What | Where |
|---|---|
| Settings | `~/Library/Application Support/ReCarbon/settings.toml` |
| Saves, logs, installed game | `~/Library/Application Support/ReCarbon/` (`logs/`, `game/`) |
| Caches (shaders, pipelines; safe to delete) | `~/Library/Caches/ReCarbon/` |
