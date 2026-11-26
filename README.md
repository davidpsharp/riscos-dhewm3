# riscos-dhewm3

Doom 3 for RISC OS 5: a port of [dhewm3](https://github.com/dhewm/dhewm3) 1.5.5 (the GPL
Doom 3 engine), drawing through Andrew Youll's RISC3D (Linux3D under RoMac / RISC OS on
Linux; V3DRender on a native Raspberry Pi 4).

![Doom 3 in an 800x600 window on RoMac: the Mars City hangar](https://github.com/davidpsharp/riscos-dhewm3/releases/download/v0.1.0/riscos-dhewm3-romac-mars-city-hangar.png)

*dhewm3 in a window on RoMac (RISC OS on a Mac, through RISC3D), Mars City hangar, about 30 fps.*

Home page: https://davidsharp.com/riscos/doom3/
Contact (questions, bug reports): https://davidsharp.com/contact/ or this repository's issues.

**Status (version 0.2.2, 26 November 2026):** the free demo and the full game both play.
- **RoMac:** plays in a window, about 30 fps.
- **Raspberry Pi 4 (RISC OS 5.30, RISC3D 0.22 with V3DRender 0.37):** the demo plays at about
  12 fps in an 800x600 window. The full game plays too, at about 10 fps in
  Mars City: on a Pi the launcher gives the GPU a 1 GB buffer area and uses smaller textures,
  so it needs about 700 MB of free memory (a 2 GB or bigger Pi 4; tested on 4 GB). See
  [docs/full-game-gpu-buffers.md](docs/full-game-gpu-buffers.md).
- **RISC OS on Linux:** not yet tried.

**Game data:** none is included. The icon bar launcher downloads id Software's free Doom 3 demo on
request, checks it and shows id's licence. To play the full game, buy Doom 3 (Steam or GOG: the
original Doom 3, not BFG Edition) and copy its `base` folder (pak000 to pak008) into `!dhewm3`;
`!Help` says how.

The issues found along the way, for RISC3D's author, are summarised for all three ports in
[docs/RISC3D-for-Andy.md](docs/RISC3D-for-Andy.md); the full game's GPU memory problem is in
[docs/full-game-gpu-buffers.md](docs/full-game-gpu-buffers.md). The plan is in
[docs/PLAN.md](docs/PLAN.md).

## Download
From the home page, from PackMan (package dhewm3), or from this repository's Releases page:
`dhewm3-riscos-<version>.zip`, a RISC OS zip (types kept by SparkFS, !SparkPlug or Info-ZIP
UnZip) holding `!dhewm3`. It needs RISC OS 5 on ARMv7 or later, SharedUnixLibrary and
ARMEABISupport, and on a Pi 4 RISC3D 0.22 or later in `!Boot.Resources` (PackMan installs them).

## Building
GCCSDK cross-compiler in Docker (image `doom3-riscos`, with riscos-mesa's devkit):
- `riscos/sdk.sh riscos/sdl2/build.sh`: SDL 2 with the RISC OS patches in `riscos/sdl2/patches`;
- `riscos/sdk.sh riscos/build.sh`: the game;
- `riscos/sdk.sh riscos/package.sh`: the app, in `build/riscos/app/!dhewm3`.

`tools/romac-deploy` and `tools/romac-run` install and run it under RoMac.

## Layout
- `dhewm3/`: dhewm3 upstream (505c5c8) with the port's changes (marked `__riscos__` or
  "RISC OS port").
- `riscos/`: the build, the app, the launcher (`riscos/launcher`), SDL 2 patches, icons.
- `tests/`: d3gltest (GL feature checks), V3DRender test patches and logs, probes.
- `docs/`: the plan and the RISC3D reports.

## Licences
dhewm3 is GPL-3.0 (see `dhewm3/COPYING.txt`), with id Software's additional terms. The
launcher and port code are under the same licence. Files that are new in the port carry
"RISC OS port, copyright David Sharp 2026" in their first line; `dhewm3/SOURCE-riscos` lists
the dhewm3 files it changed. The V3DRender test patches in `tests/v3drender` are
GPL-2.0-or-later, as V3DRender is. Doom 3's data is id Software's and isn't distributed here.
