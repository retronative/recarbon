---
title: Download
description: ReCarbon has no public release yet. What a release will contain, and what you will need.
---

> [!NOTE]
> **No release yet**
>
> ReCarbon publishes **no downloads yet**. This page will carry the release once there is one.
> Nothing on this site links to game files or disc images, and nothing ever will.

## What a release will contain

- `ReCarbon.app`, the native app for Apple Silicon (bundle ID `org.retronative.recarbon`).
- The license notices and texts of every component it ships ([Licenses](../NOTICE)),
  and the source offer for its LGPL parts (FFmpeg, libmspack).
- **No game files.** You install the game from your own disc image: see
  [Installation](installation.md).

## What you will need

- An Apple Silicon Mac. See [Supported hardware](hardware.md) for what has been measured.
- **macOS 27 or newer** for the current builds: they link libraries built for macOS 27.0. A
  build for older macOS versions needs those libraries built from source, which is planned
  (M8, packaging).
- Your own Xbox 360 disc of *Need for Speed: Carbon*, as a disc image (`.iso`) or an extracted
  folder.
- About 5.3 GB of disk space for the installed game (78 files in the install check).

## Developer builds

You can build ReCarbon from source today: see [Developers](developers.mdx). A developer build
links Homebrew's libraries and runs only on the macOS it was built on, or newer.
