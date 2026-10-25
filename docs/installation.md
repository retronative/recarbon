---
title: Installation
description: Install Need for Speed Carbon into ReCarbon from your own Xbox 360 disc image, with the first-run installer or the command line.
---

ReCarbon installs the game **from your own disc image**: it copies the files it needs and checks
each one against a manifest of hashes. Nothing is downloaded.

> [!WARNING]
> **Alpha**
>
> These steps install ReCarbon v0.1.0 ([Download](download.md)). A clean install from a disc image on a fresh macOS user account was tested on a MacBook
> Air M5 (M8); an install on a MacBook Air M1 wasn't tested.

## 1. Make a disc image

Make an image (`.iso`) of **your own** Xbox 360 disc of *Need for Speed: Carbon*, or extract it to a
folder. This site doesn't explain how to obtain game files in any other way, and the project won't
help with that.

## 2. Install ReCarbon and open it the first time

ReCarbon is **signed ad hoc**: it isn't signed with an Apple Developer ID or notarized by Apple.
So the first time you open a downloaded copy, macOS blocks it. You allow it once:

1. **Check the download.** Proceed only with a copy from ReCarbon's GitHub release whose checksum
   matches. In Terminal, in the folder with the download:
   `shasum -a 256 -c ReCarbon-0.1.0-macos.dmg.sha256` must print `ReCarbon-0.1.0-macos.dmg: OK`.
2. Open `ReCarbon-0.1.0-macos.dmg` and drag **ReCarbon** onto **Applications**.
3. Open **ReCarbon** from Applications. macOS says it can't verify the app (the wording varies with
   the macOS version). Choose **Done** or **OK**, not **Move to Trash**.
4. Open **System Settings > Privacy & Security** and scroll down to **Security**. Next to the
   message about ReCarbon, choose **Open Anyway**.
5. Confirm with **Open Anyway** (macOS may ask for your password). ReCarbon starts, and from then on
   it opens normally.

**Open Anyway** appears only after a blocked attempt to open the app, and for a limited time. If you
don't see it, see [Troubleshooting](troubleshooting.md#macos-wont-open-recarbon).

> [!CAUTION]
> Don't turn Gatekeeper off (`spctl --master-disable`) and don't remove the download's quarantine
> attribute (`xattr`). Allow only ReCarbon, only once, and only a copy you trust.

A build you make yourself on your own Mac isn't blocked.

## 3. Install the game

### With the first-run installer

Start ReCarbon (step 2). If no installed game is found, the installer screen opens:

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

## 4. First launch

At the main menu, the game shows **"Cannot connect to Xbox Live!"** at every launch: press **A**
(**Space** on the keyboard) to continue ([Known issues](known-issues.md)).

The first race after a fresh install can hitch for a moment (up to about half a second) while
ReCarbon fills its shader cache; later races are smoother.

## 5. Play

Start ReCarbon. Press **F1** (Fn+F1 on a Mac keyboard, or Cmd+,) for ReCarbon's Settings at any
time; see [Controls](controls.mdx) and the [Garage](https://retronative.github.io/recarbon/garage/).

## Where ReCarbon keeps things

| What | Where |
|---|---|
| Settings | `~/Library/Application Support/ReCarbon/settings.toml` |
| Saves, logs, installed game | `~/Library/Application Support/ReCarbon/` (`logs/`, `game/`) |
| Caches (shaders, pipelines; safe to delete) | `~/Library/Caches/ReCarbon/` |
