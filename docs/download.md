---
title: Download
description: Download ReCarbon v0.1.0, an early playable release, for Apple Silicon Macs with macOS 27 or newer. Bring your own disc.
---

ReCarbon is an unofficial native port of *Need for Speed: Carbon* (Xbox 360) to Apple Silicon
Macs.

**[Download ReCarbon v0.1.0 for Apple Silicon](https://github.com/retronative/recarbon/releases/download/v0.1.0/ReCarbon-0.1.0-macos.dmg)**
(`ReCarbon-0.1.0-macos.dmg`, from the [v0.1.0 release](https://github.com/retronative/recarbon/releases/tag/v0.1.0))

v0.1.0 is an early release: races, career progress and saves work, but it is not finished.
Read [Known issues](known-issues.md) before you play.

## You need

- An **Apple Silicon** Mac with **macOS 27 or newer**.
- ***Need for Speed: Carbon* (USA), Xbox 360:** your own disc image (`.iso`) or extracted
  folder. **No game files are included.** Other versions won't work (PC, PlayStation, Wii, original Xbox, other regions or versions): only the USA Xbox 360 edition has been checked.
- About **5.7 GB** free for the game, plus about 70 MB for the app.

**[How to install →](installation.md)**

## Check the download (optional)

In Terminal, in the folder with the download:

```sh
shasum -a 256 ReCarbon-0.1.0-macos.dmg
```

The code it prints must be
`abc376fb04550842fe4a5a34d4feb75c8c9dd302a8b45db4f590be3018d60d9b`. It's also in the release's
[`ReCarbon-0.1.0-macos.dmg.sha256`](https://github.com/retronative/recarbon/releases/download/v0.1.0/ReCarbon-0.1.0-macos.dmg.sha256).

## More

[Performance](performance.md) · [Supported hardware](hardware.md) ·
[Known issues](known-issues.md) · [Controls](controls.mdx) ·
[Troubleshooting and bug reports](troubleshooting.md) ·
[Advanced installation](installation-advanced.md) · [Licenses](../LICENSES)

## Licenses and source

ReCarbon is free software under the GPL-3.0-or-later ([License](../COPYING),
[Notices](../NOTICE)). The source of its replaceable LGPL libraries
([Third-party notices](../THIRD_PARTY_NOTICES.md)) is on the
[release page](https://github.com/retronative/recarbon/releases/tag/v0.1.0) as
`ReCarbon-0.1.0-third-party-source.tar.xz` (SHA-256
`a0e805b44a4724acc445e18049ed5662946376c4a94f52d8ef311704cc19e883`). ReCarbon's own source for
this build is the tag [`v0.1.0`](https://github.com/retronative/recarbon/tree/v0.1.0).

ReCarbon is not affiliated with or endorsed by Electronic Arts. "Need for Speed" and "Carbon" are
trademarks of Electronic Arts Inc.
