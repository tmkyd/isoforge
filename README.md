# isoforge

[![License](https://img.shields.io/github/license/tmkyd/isoforge)](LICENSE)
[![Release](https://img.shields.io/github/v/release/tmkyd/isoforge)](https://github.com/tmkyd/isoforge/releases/latest)
![Platform](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20x86-0078D4)

English | [日本語](README.ja.md)

isoforge backs up optical data discs (CD, DVD and Blu-ray) to ISO files on Windows.

Optical drives are disappearing from PCs and becoming hard to buy, so the data saved on CDs, DVDs
and Blu-ray discs over the years needs to be moved to other storage while a drive is still at
hand. Copying the files off a disc one by one can take a long time, especially when there are
many small files. isoforge instead reads the whole disc from start to end in one pass into an ISO
file. The ISO file can be kept as it is, or mounted as a virtual drive (in Windows, double-click
it or choose **Mount**) to copy the files from it.

> [!IMPORTANT]
> isoforge does **not** build ISO files from files (no re-authoring), remove copy protection, rip
> audio CDs or video discs, or write to discs.

## Features

- Saves CD, DVD and Blu-ray data discs to ISO files with a single command line. The disc is
  checked before reading, and if it cannot be saved, isoforge stops and says why.
- Computes the SHA-256 of every ISO and writes it to a hash file in `sha256sum` format.
  `isoforge verify` checks an ISO against its hash file later, and `create --verify` reads the
  disc a second time to confirm the copy.
- Writes a log file (`--log`) with the drive and disc information, the progress and the details
  of any error, which helps to find the cause of a failed backup.
- Retries sectors that fail to read (3 times by default, `--retries`), so that a temporary read
  error does not end the backup.

## Requirements

- Windows 10 or later
- An internal or external optical drive

## Installation

Download the zip for your Windows from the
[latest release](https://github.com/tmkyd/isoforge/releases/latest), extract it and run
`isoforge.exe` from a terminal (PowerShell, Command Prompt or Windows Terminal).

- `isoforge-<version>-win-x64.zip`: 64-bit Windows (most PCs)
- `isoforge-<version>-win-x86.zip`: 32-bit Windows 10 (also runs on 64-bit Windows)

`isoforge.exe` is a single file with no dependencies, and administrator rights are normally not
needed.

## Usage

The simplest use saves the disc in drive Z: to `disc.iso` in the current folder:

```powershell
isoforge create Z: -o disc.iso
```

The SHA-256 of the ISO is written to the hash file `disc.iso.sha256` next to it. To check the ISO
against the hash file later:

```powershell
isoforge verify disc.iso
```

### Commands

```text
isoforge list [--json]
isoforge info <drive> [--json] [--wait <seconds>]
isoforge create <drive> -o <file.iso> [options]
isoforge verify <file.iso> [--hash <sha256 | file>] [-q] [--no-progress]
isoforge help [<command>]
isoforge -h, --help | -V, --version
```

Running `isoforge`, or `info`, `create` or `verify` without arguments, shows the help;
`isoforge help <command>` shows the options of a command.

### Examples

`info` tells in advance whether a disc can be saved. For a DVD-ROM in drive Z:, it shows:

```console
> isoforge info Z:
Device:         \\.\Z: (CdRom0)
Model:          PIONEER BD-RW BDR-209MIO 1.54
Disc type:      DVD-ROM (profile 0010h)
Capacity:       1968880 blocks of 2048 bytes
Disc status:    complete, last session complete, 1 session(s), tracks 1-1
Track 1:        start 0, 1968880 sectors, session 1
Media range:    LBA 0-1968879 (1968880 sectors, 4032266240 bytes)
ISO 9660:       1968880 sectors
UDF:            1968880 sectors
Adopted range:  LBA 0-1968879 (1968880 sectors, 4032266240 bytes)
Supported:      yes
```

More examples:

```powershell
# Optical drives, whether a disc is loaded, and the disc type
isoforge list

# The same check without waiting for a disc that is not ready
isoforge info Z: --wait 0

# Back up the disc, read it a second time to confirm, and keep a log
isoforge create Z: -o disc.iso --verify --log isoforge.log

# Allow up to 10 retries for each sector that fails to read
isoforge create Z: -o disc.iso --retries 10

# Back up the disc and write the result as JSON for scripts
isoforge create Z: -o disc.iso --json
```

### Drives

| Form | Meaning |
| --- | --- |
| `Z`, `Z:`, `Z:\`, `\\.\Z:` | Drive letter |
| `\\.\CdRom0`, `CdRom0` | Device name, for drives without a drive letter |

`CdRom0` needs no quoting in shells such as Git Bash. Device numbers can change when USB drives are
reconnected; `isoforge list` shows the current mapping.

### `create` options

| Option | Default | Description |
| --- | --- | --- |
| `-o, --output <path>` | required | Output ISO file |
| `--overwrite` | off | Replace an existing ISO file or hash file |
| `--keep-partial` | off | Keep the partial file after a failure or interruption |
| `--hash-file <path>` | `<output>.sha256` | Where to write the SHA-256 hash file |
| `--no-hash-file` | off | Do not write a hash file (the hash is still shown) |
| `--verify` | off | Read the disc again after writing and compare the SHA-256 |
| `--retries <n>` | 3 | Retries for each failed sector read (500 ms apart) |
| `--eject` | off | Eject the disc after success |
| `--log <path>` | none | Append a detailed log, including progress every 10% |
| `--wait <seconds>` | 30 | How long to wait for the disc to become ready; 0 does not wait |
| `--json` | off | Write the result as JSON to standard output, also on failure (not with `-v` or `-q`) |
| `-v, --verbose` / `-q, --quiet` | | More or less output |
| `--no-progress` | off | Do not show the progress line |

`info` also accepts `--json` and `--wait`; `list` accepts `--json`. `verify` accepts `-q` (only
the result line) and `--no-progress`, and warns when the hash file names a different file.

## How `create` works

1. Before reading the disc, isoforge checks that the output and hash files do not exist (unless
   `--overwrite`), that the disc is supported and its range can be determined, and that the
   output volume has enough free space and no file size limit in the way (for example FAT32).
2. The disc is read into `<output>.partial`, which is renamed to `<output>` only after everything
   succeeded, including `--verify`. On failure or Ctrl+C the partial file is deleted.
3. The SHA-256 is always shown and, unless `--no-hash-file`, written to the hash file in
   `sha256sum` format: `<hash> *<file name>`. `sha256sum -c` can check it.

A sector that cannot be read after the retries fails the backup (exit code 3). The only
exception is the last one or two sectors of a CD track (often unreadable on CD-Rs written track
at once) when they lie outside the file system; they are left out with a warning.

## Supported discs

- Single-session, single-track data discs with 2048-byte sectors and an ISO 9660 or UDF file
  system, either finalized or appendable with one closed session:
  - CD-ROM, CD-R, CD-RW with one Mode 1 data track
  - DVD-ROM, DVD±R, DVD±RW, DVD±R DL
  - BD-ROM, BD-R, BD-RE, including BDXL (triple and quadruple layer)
- Not supported: audio CDs and mixed-mode CDs, Mode 2 (CD-ROM XA), multi-session discs, discs
  whose last session is still open, packet-written discs, DVD-RAM, HD DVD, and copy-protected
  discs. Video discs (DVD-Video, BD-Video) are not detected; encrypted sectors fail to read.

`isoforge info` explains why a disc is not supported.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Usage error |
| 2 | Unsupported disc or layout, no disc, or not an optical drive |
| 3 | Read error |
| 4 | Output file error (including an existing file without `--overwrite`, not enough space) |
| 5 | Hash mismatch (`create --verify`, `verify`) |
| 6 | Insufficient permission |
| 7 | Other error |
| 130 | Interrupted (Ctrl+C) |

## Building from source

Requires Visual Studio 2026 with the "Desktop development with C++" workload.

```bat
scripts\build.cmd              rem Configure and build x64 and x86, Debug and Release
scripts\build.cmd x86-release  rem Only the given presets
scripts\build.cmd --package    rem Also create the x64 and x86 zips in out\package\
```

The script loads the Visual Studio developer environment of each architecture itself. The CMake
presets (`x64-debug`, `x64-release`, `x86-debug`, `x86-release`) can also be used directly from a
developer command prompt of the matching architecture (`VsDevCmd -arch=amd64` or `-arch=x86`):

```bat
cmake --preset x64-release
cmake --build --preset x64-release
cpack --preset x64-release
```

## Reporting issues

Please report bugs and unsupported discs that should work on
[GitHub Issues](https://github.com/tmkyd/isoforge/issues). The output of `isoforge info <drive>`
(or `--json`) and the `--log` file of a failed `create` help to find the cause.

## License

Copyright 2026 tmkyd. Licensed under the Apache License 2.0; see [LICENSE](LICENSE).
