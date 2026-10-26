---
title: Installation
description: Install ReCarbon in five steps. Download it, drag it to Applications, allow it once, choose your own disc image, and play.
---

You need an Apple Silicon Mac with **macOS 27 or newer**, and **your own** disc image of
***Need for Speed: Carbon* (USA), Xbox 360**. One step at a time, with pictures: the
[step-by-step guide](https://retronative.github.io/recarbon/docs/installation/step-1/).

ReCarbon v0.1.1 is an early release: playable, but not finished. See [Known issues](known-issues.md).

## 1. Download ReCarbon

Download [`ReCarbon-0.1.1-macos.dmg`](https://github.com/retronative/recarbon/releases/download/v0.1.1/ReCarbon-0.1.1-macos.dmg)
from the [v0.1.1 release](https://github.com/retronative/recarbon/releases/tag/v0.1.1). The file
shows macOS's generic disk-image icon; ReCarbon's own icon appears once it is open.

## 2. Drag ReCarbon to Applications

Open the dmg and drag **ReCarbon** onto **Applications**.

## 3. If macOS blocks ReCarbon

ReCarbon isn't notarized by Apple, so macOS blocks it the first time. You allow it once:

1. Open **ReCarbon** from Applications. When macOS says it can't verify the app, choose **Done**
   (or **OK**), not **Move to Trash**.
2. Open **System Settings > Privacy & Security** and scroll down to **Security**.
3. Next to the message about ReCarbon, choose **Open Anyway**, then confirm with **Open Anyway**
   (macOS may ask for your password).

From then on, ReCarbon opens normally. **Open Anyway** appears only after a blocked attempt, and
for a limited time; the wording varies with the macOS version. Don't see it? See
[macOS won't open ReCarbon](troubleshooting.md#macos-wont-open-recarbon).

> [!CAUTION]
> Do this only for a copy you trust: one from ReCarbon's official release (you can
> [check the download](#check-the-download-optional)). Never turn Gatekeeper off, and never remove
> the download's quarantine attributes.

## 4. Choose your disc image

ReCarbon's installer opens. Choose your own disc image of ***Need for Speed: Carbon* (USA), Xbox
360** (`.iso`) or its extracted folder: it looks in the usual places, or you browse for it.
Other versions won't work (PC, PlayStation, Wii, original Xbox, other regions or versions): only the USA Xbox 360 edition has been checked. [See disc requirement details.](disc-images.md)

The installer copies and checks about **5.7 GB**, then starts the
game. Nothing is downloaded.

This site doesn't explain how to obtain game files in any other way, and the project won't help
with that.

## 5. Play

You're all set. Start ReCarbon and hit the streets!

Drive with **W** / **S** (throttle, brake) and **A** / **D** (steer). **Space** is the
handbrake, **Backspace** the nitrous, and <kbd aria-label="Command">⌘</kbd> + <kbd>,</kbd> (Command + Comma) opens ReCarbon's Settings
(or **F1**, Fn + F1 on a Mac keyboard). All keys:
[Controls](controls.mdx).

Optional: explore the controls and game features in the
[Garage](https://retronative.github.io/recarbon/garage/).

## Check the download (optional)

In Terminal, in the folder with the download:

```sh
shasum -a 256 ReCarbon-0.1.1-macos.dmg
```

The code it prints must be
`7a43e99189cd809614c68c7dec0dc5dc046b0acd3f250a4c561123f590a55941`.

## Advanced

Installing from Terminal, the installer's other commands, and where ReCarbon keeps its files:
[Advanced installation](installation-advanced.md).
