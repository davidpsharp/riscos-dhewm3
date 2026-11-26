# riscos-dhewm3

Doom 3 for RISC OS 5: a port of [dhewm3](https://github.com/dhewm/dhewm3) 1.5.5 (the GPL
Doom 3 engine), drawing through Andrew Youll's RISC3D (Linux3D under RoMac / RISC OS on
Linux; V3DRender on a native Raspberry Pi 4).

![Doom 3 in an 800x600 window on RoMac: the Mars City hangar](https://github.com/davidpsharp/riscos-dhewm3/releases/download/v0.1.0/riscos-dhewm3-romac-mars-city-hangar.png)

*dhewm3 in a window on RoMac (RISC OS on a Mac, through RISC3D), Mars City hangar, about 30 fps.*

**Status (26 November 2026):** at present the demo works but the full game's data does not. That is
under investigation.
- **RoMac:** plays in a window, about 30 fps.
- **Pi 4:** plays Mars City 1 at about 11-15 fps (800x600, Medium). It needs V3DRender's
  buffer handle limits raised (the test module in the release).
- **Full game data:** doesn't work yet. On a Pi 4 the full game loads (about four minutes) and then
  crashes before the first frame.
- **RISC OS on Linux:** not yet tried.

The issues found along the way, for RISC3D's author, are summarised for all three ports in
[docs/RISC3D-for-Andy.md](docs/RISC3D-for-Andy.md); the Doom 3 evidence is in
[docs/RISC3D-0.17-report.md](docs/RISC3D-0.17-report.md). The plan is in
[docs/PLAN.md](docs/PLAN.md).

**No game data is included.** The icon bar launcher downloads id Software's Doom 3 demo on
request, checks it and shows id's licence. The full game's data can be copied in instead, but
doesn't work yet (see Status).

## Download
The releases page has `riscos-dhewm3-<version>.zip`, a RISC OS zip (types kept by SparkFS,
!SparkPlug or Info-ZIP UnZip) with `!dhewm3`, the V3DRender test module and the GL check
program. Its ReadMe says how to use them.

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
the dhewm3 files it changed. V3DRender (in the release's test module)
is GPL-2.0-or-later; its change is `tests/v3drender/v026-16k.patch`. Doom 3's data is id
Software's and isn't distributed here.
