---
title: Troubleshooting
description: Where ReCarbon's logs are, how to save a bug report, and what to check first.
---

> [!NOTE]
> **Work in progress**
>
> This page grows with the alpha. Problems already known are on [Known issues](known-issues.md).

## Save a bug report

Open Settings (**F1**, **Fn+F1** or **Cmd+,**, or View + Menu on a controller), go to
**Advanced**, and choose **Save Bug Report**. ReCarbon writes one `.zip` to your Desktop with:

- the logs of the recent sessions, and any crash or hang reports;
- the build's version and commit, and a system summary (macOS, CPU, RAM, GPU, display);
- your `settings.toml`;
- a screenshot.

Home-folder paths inside it read `~`. **Nothing is ever sent anywhere**: you choose whether to
share the file.

## Logs

Every session writes a log to `~/Library/Application Support/ReCarbon/logs/`, named by date and
time. The newest five sessions are kept.

- `crash-<session>.txt`: written if the game crashes. The next launch tells you once.
- `hang-<session>-<n>.txt`: written if the game stops responding for far longer than any loading
  screen or movie.

## The game isn't found

Run `ReCarbon --where` in Terminal: it prints where the game install was found and the data
folders. If you moved the install, ReCarbon looks in the default folder,
`~/Library/Application Support/ReCarbon/game`. To check an image or a folder without copying it:
`ReCarbon --verify <image or folder>`. See [Installation](installation.md).

## Stutters

Brief stalls of up to about a second are a known issue of the alpha: see
[Known issues](known-issues.md). To see the frame rate, turn on **Settings > Advanced >
Performance Overlay**.

## Settings went wrong

In Settings, each page has **Reset page**. You can also quit ReCarbon and remove `settings.toml`
from `~/Library/Application Support/ReCarbon/`: the defaults come back on the next start. Your
saves are separate and stay where they are.
