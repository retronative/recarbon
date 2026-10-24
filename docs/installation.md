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
> build. A clean install on a fresh macOS user account is still to be tested (M8).

## 1. Make a disc image

Make an image (`.iso`) of **your own** Xbox 360 disc of *Need for Speed: Carbon*, or extract it to a
folder. This site doesn't explain how to obtain game files in any other way, and the project won't
help with that.

## 2. Install the game

### With the first-run installer

Start ReCarbon. If no installed game is found, the installer screen opens:

1. It looks for disc images in the usual places, or you browse for yours.
2. It checks the image.
3. It copies and verifies the files, with progress.
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

In the alpha packaging check (2026-10-23), `--install` copied and verified 78 files (5.3 GB) in
4 seconds. When the install folder had moved, the app found the game in the default folder and
started.

## 3. Play

Start ReCarbon. Press **F1** (Fn+F1 on a Mac keyboard, or Cmd+,) for ReCarbon's Settings at any
time; see [Controls](controls.mdx) and the [Garage](https://retronative.github.io/recarbon/garage/).

## Where ReCarbon keeps things

| What | Where |
|---|---|
| Settings | `~/Library/Application Support/ReCarbon/settings.toml` |
| Saves, logs, installed game | `~/Library/Application Support/ReCarbon/` (`logs/`, `game/`) |
| Caches (shaders, pipelines; safe to delete) | `~/Library/Caches/ReCarbon/` |
