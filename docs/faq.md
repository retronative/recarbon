---
title: FAQ
description: Common questions about ReCarbon, the game files and the project.
---

## Is this an official Need for Speed game?

No. ReCarbon is an **unofficial fan project**. It is not affiliated with, endorsed by or
sponsored by Electronic Arts Inc. or the Need for Speed brand. *Need for Speed* and *Carbon* are
trademarks of Electronic Arts Inc. and are used only to describe the game.

## Where do I get the game?

From **your own, legally obtained Xbox 360 copy** of *Need for Speed: Carbon* (the supported USA
disc). ReCarbon does not include the game's assets or any disc image: the installer copies what it
needs from your disc. Please don't ask for, or share, game files or disc images. See
[Installation](installation.md).

## Why isn't there a download of the game?

Because the game belongs to its publisher. You must provide your own legally obtained copy;
ownership does not grant permission to redistribute the game's files.

## Is it an emulator?

No. It's a **static recompilation**: the game's PowerPC code is translated to C++ ahead of time and
compiled to native arm64. A runtime stands in for the Xbox 360's kernel, GPU, audio and
controllers. There is no Wine, CrossOver or runtime CPU emulation.

## What is it built on?

[SpeedBreaker](https://github.com/SpeedBreakerProject/speedbreaker), the static recompilation of
*Need for Speed: Most Wanted* (2005) by project(u), which itself follows Unleashed Recompiled. It
uses XenonRecomp, and Xenia's research on the Xbox 360. See [Credits](credits.md).

## Does it run on Intel Macs, Windows or Linux?

ReCarbon targets **Apple Silicon Macs**. Nothing else is planned or tested.

## Why does the game say "Cannot connect to Xbox Live!"?

It's the Xbox 360 game's own notice when it can't connect to Xbox Live, which ReCarbon doesn't
provide: it isn't a ReCarbon error. Press **A** (**Space** on the keyboard) once at the main menu
and play on; nothing is lost. The online modes aren't available.

## Can I use a controller?

Yes, through SDL3, but **no physical controller has been tested yet**. See the
[Garage](https://retronative.github.io/recarbon/garage/) for the default mapping and what each control is known to do.

## Is it free?

Yes. ReCarbon is free software under the GNU General Public License, version 3 or later. See
[Licenses](../NOTICE).
