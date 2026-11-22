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
