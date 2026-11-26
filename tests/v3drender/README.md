# V3DRender test builds (for RISC3D issue 2)

Test builds of V3DRender 0.16 (github.com/adyoull/riscos-v3drender, 21cbdb7), used on pi4-plato
on 22 Nov 2026 to find why every buffer of 64 KB or more failed. See `docs/risc3d-issues.md`.

- `nohigh.patch`: never ask OS_Memory 12 for memory above 1 GB. **No change** (still failed).
- `bottomup.patch`: place every BO first fit from the bottom of the area (no top-down aligned
  placement). **d3gltest passes**, and dhewm3 reaches its main menu.

To build: clone V3DRender into `build/v3drender/src`, apply a patch, then
`riscos/sdk.sh tests/v3drender/build-module.sh` (the module part of V3DRender's own build.sh,
in the doom3-riscos image; it expects the source at /work/build/v3drender/src).
Gives `build/v3drender/src/out/V3DRender,ffa`. Load it in place of !RISC3D's until a reboot:
`RMKill RISC3D`, `RMKill V3DRender`, `RMLoad <test module>`, `RMLoad <RISC3D$Dir>.RISC3D`.

## The playable build (22 Nov 2026)
`playable.patch` (against 21cbdb7): 16384 handles and BOs, never claim specific pages
(`high_run` returns DA_RELEASE), and logging of refused BOs; 0.16's placement is unchanged.
It must be loaded **after DAClamp** so its 256 MB area isn't capped at 128 MB: on plato,
`Obey $.code.UseTestV3D` (`UseTestV3D,feb` here) does that until the next reboot. See
`docs/risc3d-issues.md` items 2, 7, 8, 10 and 11.

## BOFrag: the alignment fragmentation without a game (26 Nov 2026)
`BOFrag,ffb` is a BBC BASIC program: no GL, Mesa or game data. It opens V3DRender as a DRM file
(`V3DRender_Open`) and makes buffers with `V3DRender_Ioctl` CREATE_BO, of the sizes Mesa asks
for, until one is refused, then frees them all. It takes about 4 seconds. Run it with nothing else
using the GPU and at least the area's size of free memory: `*BASIC -quit BOFrag`. Test 1 makes
1 MB + 4 KB (257-page) buffers; test 2 makes rounds of one 257-page, four 17-page and eight 1-page
buffers, then tries a few sizes.

On pi4-socrates (Pi 4, RISC OS 5.30, 256 MB area, the default):

| V3DRender | Test 1: area in use at refusal | Test 2: area in use at refusal | 1 MB + 4 KB still fits? |
|---|---|---|---|
| 0.37 (RISC3D 0.22) | 127 buffers, 129 MB (51%) | 1,586 buffers, 160 MB (63%) | no |
| 0.33 (tag v0.33) | 127 buffers, 129 MB (51%) | 1,586 buffers, 160 MB (63%) | no |
| 0.33 + `align-033.patch` | 253 buffers, 255 MB (100%) | 2,535 buffers, 255 MB (100%) | no (the area is full) |

`build-module.sh` takes the source folder under `build/v3drender` as its argument (default `src`):
`riscos/sdk.sh tests/v3drender/build-module.sh v033a`.
