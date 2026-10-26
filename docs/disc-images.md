---
title: Supported disc images
description: The one Xbox 360 disc ReCarbon supports, its identity, hashes and layout, the image formats the installer reads, and how its 78-file check differs from an exact image hash.
---

Technical details for checking a disc image before or after a failed install. To install
ReCarbon, follow [Installation](installation.md): you don't need any of this.

ReCarbon never includes or downloads game files. Everything below is metadata: names, sizes,
hashes and header fields. The values were checked on 2026-10-26 with ReCarbon v0.1.0 against a
backup of a retail disc.

## The supported disc

ReCarbon is a recompilation of one specific executable, so it runs only the disc that executable
comes from.

| | |
|---|---|
| Game | *Need for Speed: Carbon* |
| Platform | Xbox 360 (not the original Xbox, PC, PlayStation or Wii versions) |
| Edition | USA |
| Title ID | `0x454107EC` (`EA-2028`) |
| Media ID | `0x51D2B53F` |
| Version | `0.0.0.1` (base version `0.0.0.1`) |
| Disc | 1 of 1 |

The title ID, media ID, version and disc number are read from `default.xex`'s execution info.
Its region field allows every region (`0x000000FF`), so it doesn't identify the edition: the file
hashes below do.

## The reference disc image

The image the hashes were checked against: a full dump of the disc.

| | |
|---|---|
| Size | 7,834,892,288 bytes |
| SHA-256 | `6e5fbdca502880af48310e905c34dd4418a7950f4da252c10f12dd358ebf0b5a` |
| SHA-1 | `283b06e4f1a54cc7d152e1d31c9584fe13fb8dd5` |
| MD5 | `a2579fe908c6bdfb6613c37c497aed65` |
| Layout | XGD2: a DVD-video partition at the start (ISO 9660 and UDF), and the Xbox 360 game partition (XDVDFS, `MICROSOFT*XBOX*MEDIA`) at byte offset `0xFD90000` |

To compare your image (this reads the whole file):

```sh
shasum -a 256 "<your image>.iso"
```

A different hash doesn't mean your image won't work: see
[Image hash or file check](#image-hash-or-file-check).

## The game files

The game partition holds 79 files. The installer copies and checks 78 of them, 5,738,577,280
bytes (5.74 GB):

| File | Count | Notes |
|---|---|---|
| `default.xex` | 1 | The game's executable: 4,861,952 bytes, SHA-256 `947f95c7306eabf5bc72972938df57a922798bafbc02edf1a9107f0462c721e1` |
| `Movies/*.wmv` | 65 | The movies |
| `NFS/ZDIR.BIN` | 1 | The index of the game's data archives |
| `NFS/ZZDATA0.BIN` to `NFS/ZZDATA10.BIN` | 11 | The data archives (`ZZDATA10.BIN` is empty, 0 bytes, on the disc) |

The 79th file, `$SystemUpdate/su20076000_00000000` (3,100,672 bytes), is an Xbox 360 console
update. The game doesn't need it, so it isn't copied or checked.

The `default.xex` hash is the file exactly as it is on the disc: an encrypted, compressed
Xbox 360 executable (`XEX2`). Every file's size and SHA-256 is in ReCarbon's source, in
[`runtime/install/manifest.inc`](https://github.com/retronative/recarbon/blob/main/runtime/install/manifest.inc)
(manifest version `448b999d64da9272`).

## Image hash or file check

There are two different ways to say a disc image is the right one:

- **An exact image hash match** means your `.iso` is byte for byte the reference image above.
  It then passes the installer's check too. But the hash covers the whole file, including the
  video partition and the unused space, so two good dumps of the same disc can have different
  hashes. The installer never computes it.
- **The installer's 78-file check** is what ReCarbon actually uses. It reads the game partition and
  checks each of the 78 files above against its size and SHA-256 in the manifest. An image, or an
  extracted folder, passes when all 78 match, whatever its overall hash.

The installer checks in this order:

1. `default.xex`: its title ID and its SHA-256. Another game, or another region's or version's
   executable, stops here with a message that says which.
2. The file list: every one of the 78 files is present with the right size, and the image isn't
   cut short. Files the manifest doesn't list, like `$SystemUpdate/`, are ignored.
3. While copying, each file's SHA-256. A damaged dump stops the install, and nothing is left
   half-installed.

`ReCarbon --verify "<image or folder>"` runs all three checks without copying anything (see
[Advanced installation](installation-advanced.md)). On the reference image, and on the folder
extracted from it, it reports `All 78 files match the original disc`.

## Image formats

| Source | Status |
|---|---|
| Full disc image (`.iso`, XGD2 layout as above) | **Verified**: the reference image passes `--verify`, and installs from it were tested on a MacBook Air M5 |
| Extracted folder made with [extract-xiso](https://github.com/XboxDev/extract-xiso) (`extract-xiso -x "<image>.iso"`) | **Verified**: it passes `--verify` |
| A folder holding the extracted folder (with `default.xex` one level down) | Accepted |
| Game-partition-only images (XISO, game partition at offset 0) and images with the partition at the offsets older dump tools use | Read by the installer, not tested with this game |
| Folders extracted with other tools | Not tested: they pass if the 78 files match |
| Games on Demand or Xbox Live packages (`CON`, `LIVE`, `PIRS`) | Refused: not a disc image |
| Archives (`.zip`, `.7z`, `.rar`) | Refused: extract the `.iso` first |
| A `default.xex` on its own | Refused: choose the whole folder or the image |
| Only the DVD-video partition, or another platform's DVD | Refused |
| An image cut short (for example by FAT32's 4 GB file size limit) | Refused as incomplete |

You don't need extract-xiso or any other tool: the installer reads `.iso` images itself.

## Verified and untested

- **Verified:** the USA Xbox 360 disc above, as a full image and as an extracted folder.
- **Refused:** any other `default.xex`, including other regions' editions and other versions of the
  game. The recompiled code only works with the one executable it was made from.
- **Untested:** other pressings or re-releases of the USA disc. One with the same `default.xex` and
  the same 78 files would pass the check. One with a different `default.xex` would be refused.
  None has been tried.
- **Not supported:** the original Xbox version (`default.xbe`) and the PC, PlayStation and Wii
  versions.

If your USA disc is refused as another version or as damaged, dump it again. See
[Troubleshooting](troubleshooting.md#the-game-isnt-found).
