---
title: Troubleshooting
description: Where ReCarbon's logs are, how to save a bug report, and what to check first.
---

> [!NOTE]
> **Work in progress**
>
> This page grows with the alpha. Problems already known are on [Known issues](known-issues.md).

## macOS won't open ReCarbon

ReCarbon isn't notarized, so macOS blocks a downloaded copy the first time. The steps are in
[Installation](installation.md#3-if-macos-blocks-recarbon). If they don't work:

- **No "Open Anyway" button:** it appears only after you try to open ReCarbon and macOS blocks it.
  Open ReCarbon from Applications again, choose **Done**, then go straight to **System Settings >
  Privacy & Security** and scroll down to **Security**.
- **"ReCarbon is damaged and can't be opened":** the download is incomplete or changed. Delete it,
  download it again from the GitHub release, and
  [check it](installation.md#check-the-download-optional).
- **Nothing happens, or the button doesn't stay:** quit System Settings, open ReCarbon again from
  Applications (not from inside the disc image), and repeat.
- **A managed Mac** (work or school) may not allow apps that aren't notarized at all.
- ReCarbon needs **macOS 27** or newer on an Apple Silicon Mac.

Don't turn Gatekeeper off and don't remove quarantine attributes to get around this.

## Save a bug report

Open Settings (**F1**, **Fn+F1** or **Cmd+,**, or View + Menu on a controller), go to
**Advanced**, and choose **Save Bug Report**. ReCarbon writes one `.zip` to your Desktop with:

- the logs of the recent sessions, and any crash or hang reports;
- the build's version and commit, and a system summary (macOS, CPU, RAM, GPU, display);
- your `settings.toml`;
- a screenshot.

Home-folder paths inside it read `~`. **Nothing is ever sent anywhere**: you choose whether to
share the file.

## Report a problem

Open an issue at [github.com/retronative/recarbon/issues](https://github.com/retronative/recarbon/issues)
with what you did, what happened, your Mac and macOS version, and attach the bug report's `.zip`
if you are comfortable sharing it. Never attach game files.

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
`ReCarbon --verify <image or folder>`. See [Advanced installation](installation-advanced.md).

## Stutters

Brief stalls of up to about a second are a known issue of the alpha: see
[Known issues](known-issues.md). Other apps using a lot of CPU make short stalls more likely: ReCarbon
gives its frame threads a higher priority, but quitting busy apps still helps. To see the frame rate, turn on **Settings > Advanced >
Performance Overlay**.

## Settings went wrong

In Settings, each page has **Reset page**. You can also quit ReCarbon and remove `settings.toml`
from `~/Library/Application Support/ReCarbon/`: the defaults come back on the next start. Your
saves are separate and stay where they are.
