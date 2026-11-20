# kaiHEN改変

kaiHEN改変 is a homebrew environment and minimal custom firmware for Switch 2.

## Warning

It's still a work in progress, and might have bugs.

We allow full access to the system, including the system NAND. A malicious
homebrew might write to your Switch 2 and brick it forever, so be warned.

## Components

- `ro/`: an `ldr:ro` service. It maps NROs into its clients' processes,
  publishes JIT code for them, and gives initialized clients a session with
  HEN.
- `bootstrap/`: the startup program. It starts HEN and the plugins, hands
  them the sessions they keep, and connects RO to HEN.
- `hen/`: HEN service. It starts the homebrew loader when a
  native game is opened with R held, gives each program its own session on
  an SM proxy, and serves the homebrew interface. See `hen/README.md`.
- `libkaihen/`: kaiHEN's homebrew API, with added JIT support.
- `common/`: code and protocol definitions.

## Starting it

HEN is started from various acquisition tools available for Switch 2. Run a
native game by holding R. The second launch and onwards will launch into
hbmenu.

A Switch 1 game started with R held launches into hbmenu when its own
donor package is at `/kaihen/bin/s1/<program ID>.nsp` on the SD card (see
`hen/README.md`), and starts normally otherwise.

## Developing homebrew

See the devkit repository,
[ouncebrew/ounce-devkit](https://github.com/ouncebrew/ounce-devkit), to
build homebrew. Existing Switch 1 homebrew that uses libnx and OpenGL or
Vulkan can be compiled for Switch 2 with it.

Homebrew and plugins are allowed access to all system modules fs can access,
so a large number of them.

## Building

With the Switch 2 devkit installed in devkitPro (see the devkit repository,
ouncebrew/ounce-devkit):

```sh
make
```

This builds RO (`ro/build/main`), HEN (`hen/build/main`), the bootstrap
(`bootstrap/build/bootstrap.bin`) and libkaihen (`libkaihen/build/`).
`make LIBOUNCE=DIR` can build against another libounce.

With Docker (or Podman), in the devkit's image.

```sh
docker build --output out .
```

writes `out/ro/main`, `out/hen/main`, `out/bootstrap.bin` and
`out/libkaihen/`.

```sh
docker build --build-arg RELEASE=1 --output out .
```

writes the release, `out/kaiHEN.zip`. Its `sdcard/` folder is the SD card:
nx-hbmenu (`sdcard/hbmenu.nro`), RO, kaiHEN and nx-hbloader
(`sdcard/kaihen/bin/ro/main`, `sdcard/kaihen/bin/hen/main`,
`sdcard/kaihen/bin/game/main`), OunceShell
(`sdcard/switch/ounceshell/ounceshell.nro`, a file manager with an FTP server), and the
shared libraries of dynamically linked programs from the devkit's packages
(`sdcard/kaihen/lib/`), all built at the commits that the Dockerfile names.
The acquisition tool also reads RO from there during startup. It reads
`bootstrap.bin` itself, and that file does not go on the SD card. `licenses/` has the licenses of
everything in the zip.

The host tests, `sh hen/test.sh` and `sh ro/test.sh`,
build with the host compiler and read libounce's headers from
`LIBOUNCE`.

## License

Copyright (c) 2026 kaiHEN contributors.

kaiHEN is licensed under the GNU General Public License, version 2 only
(GPL-2.0-only, `LICENSE`), with the exemption below. `libkaihen/` is ISC.

Each file names its license in an SPDX header.

Exemptions:

- [Nintendo](https://github.com/Nintendo) is exempt from GPLv2 licensing
  and may (at its option) instead license any source code authored for the
  kaiHEN project under the Zero-Clause BSD license.

Contributions to kaiHEN are accepted under these terms: GPL-2.0-only with
this exemption, or the permissive license of the part they change.

Thanks to

- All kaiHEN改変 private beta testers.
- r/switch2hacks discord for a place to do and share research.
