---
title: Download
description: Download ReCarbon v0.1.0, the first playable alpha, for Apple Silicon Macs with macOS 27 or newer. Bring your own disc.
---

ReCarbon is an unofficial native port of *Need for Speed: Carbon* (Xbox 360) to Apple Silicon
Macs.

**[Download ReCarbon v0.1.0 for Apple Silicon](https://github.com/retronative/recarbon/releases/download/v0.1.0/ReCarbon-0.1.0-macos.dmg)**
(`ReCarbon-0.1.0-macos.dmg`, from the [v0.1.0 release](https://github.com/retronative/recarbon/releases/tag/v0.1.0))

> [!WARNING]
> **Alpha**
>
> v0.1.0 is the first playable alpha: races, career progress and saves work, but it is not
> finished. Read [Known issues](known-issues.md) before you play.

## You need

- An **Apple Silicon** Mac with **macOS 27 or newer**.
- **Your own** Xbox 360 disc of *Need for Speed: Carbon*, as a disc image (`.iso`) or extracted
  folder. **No game files are included.**
- About **5.7 GB** free for the game, plus about 70 MB for the app.

## How to install

1. Download ReCarbon.
2. Open the dmg and drag ReCarbon to Applications.
3. If macOS blocks it, allow it once in System Settings > Privacy & Security.
4. Choose your own disc image in ReCarbon's installer.
5. Press Space at the "Cannot connect to Xbox Live!" message, and play.

The details: [Installation](installation.md).

## More

[Performance](performance.md) · [Supported hardware](hardware.md) ·
[Known issues](known-issues.md) · [Controls](controls.mdx) ·
[Troubleshooting and bug reports](troubleshooting.md)

## Licenses and source

ReCarbon is free software under the GPL-3.0-or-later; the app carries every component's notice
and license text in `ReCarbon.app/Contents/Resources/licenses/` ([License](../COPYING),
[Notices](../NOTICE), [Third-party notices](../THIRD_PARTY_NOTICES.md)).

FFmpeg's libavcodec and libavutil and libmspack's LZX decoder (LGPL-2.1) are separate libraries
in `ReCarbon.app/Contents/Frameworks/` that you can replace with your own builds; their complete
source is on the [release page](https://github.com/retronative/recarbon/releases/tag/v0.1.0) as
`ReCarbon-0.1.0-third-party-source.tar.xz` (SHA-256
`a0e805b44a4724acc445e18049ed5662946376c4a94f52d8ef311704cc19e883`, also in its `.sha256`), and
the steps to replace one are in [Third-party notices](../THIRD_PARTY_NOTICES.md). ReCarbon's own
source for this build is its repository at the tag
[`v0.1.0`](https://github.com/retronative/recarbon/tree/v0.1.0).

ReCarbon is not affiliated with or endorsed by Electronic Arts. "Need for Speed" and "Carbon" are
trademarks of Electronic Arts Inc.
