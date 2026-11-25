# RISC3D on a Raspberry Pi 4: what three ports found

For Andrew Youll. 23 November 2026. One page; each item links to the evidence.

**Setup:** Raspberry Pi 4, RISC OS 5.30, **RISC3D 0.17 (V3DRender 0.26)**, riscos-mesa devkit
20.3.5-22 (programs linked statically with `-lMesaGPU`), 16M colour desktop.

| Port | On the GPU with stock 0.17 | Speed |
|---|---|---|
| [Quake 3 (ioquake3)](https://github.com/davidpsharp/riscos-quake3) | Yes | 138 fps 480x300, 77 fps 1024x768, 69 fps 1280x800 full screen (software Mesa: 35) |
| [Mupen64Plus](https://github.com/davidpsharp/riscos-mupen64plus) | Yes | Super Mario 64 at 163-169 VI/s (full speed is 60; software: 12-15) |
| [Doom 3 (dhewm3)](https://github.com/davidpsharp/riscos-dhewm3) | Only with item 2 fixed | ~12 fps 800x600, Medium |

All three need DAClamp to have run before V3DRender loaded (item 1). The binaries we tested are
on each repo's Releases page as RISC OS zips (Doom 3 and Mupen64Plus: v0.1.0; Quake 3: the
risc3d-preview2 pre-release). This page is the summary; each port's repo keeps
only its own evidence, linked below.

**Retest, 25 November, on RISC3D 0.22 (V3DRender 0.37) and riscos-mesa 20.3.5-23:**
- **Quake 3** (its own retest on pi4-plato, no workarounds): `vid_restart` still aborts with "wait
  failed: -9", and restarting SDL video still gives "Mesa OffScreen" (software). Both are probably
  fixed in the unreleased riscos-mesa 20.3.5-24. Its banded screenshots are fixed by PRESENT.
- **Doom 3** (pi4-socrates): a 64K-colour desktop still garbles GL windows (item 4 below). In a
  16M-colour desktop the window draws correctly. The first map's load hit a heap crash in the
  -23 build (`idBTree::Add` from the allocator), and a -22 build failed at start-up with "EMT
  trap". We haven't got Doom 3 to play on 0.22 yet.

**Thank you for 0.23** ("imported BOs follow pages the OS moves"): it cured the GPU hangs and
frozen windows Doom 3 had with 0.16.

## V3DRender

1. **The 128 MB dynamic-area cap.** Unless DAClamp runs before V3DRender loads, its 256 MB
   buffer area stops at 128 MB. Every program then gets "Failed to allocate device memory for BO"
   and crashes (all three ports). Today it works only because !MesaLibs' !Boot runs DAClamp and
   happens to sort before !RISC3D in `!Boot.Resources`. A user with only !RISC3D, which is all
   a statically linked program needs, gets crashes, and `*V3DRender` still shows 262,144 KB.
   **Ask:** V3DRender lifts the cap itself (`OS_DynamicArea 8`, as DAClamp does), or !RISC3D's
   !Boot runs DAClamp; at least, `*V3DRender` shows the area's real limit.
   [Details](RISC3D-0.17-report.md#2-the-128-mb-dynamic-area-cap-still-an-issue-though-boot-order-can-hide-it)
2. **4,096 buffer handles per program (8,192 BOs in all).** Doom 3 makes ~5,300; it crashes as
   its first level finishes loading. Quake 3 (~1,900) and Mario (~224) don't hit it.
   **Ask:** raise `V3D_MAX_HANDLES` and `V3D_MAX_BOS` (16,384 works:
   [two-line patch](../tests/v3drender/v026-16k.patch)); log refused CREATE_BOs in `*V3DRender`.
   [Details](RISC3D-0.17-report.md#1-buffer-handles-4096-per-file-is-too-few-the-blocker)
3. **Question:** does V3DRender or the swap wait for a frame to finish (and for vsync) before the
   window's memory is shown? Screenshots in play show bands from different frames.
   [riscos-quake3 item 4](https://github.com/davidpsharp/riscos-quake3/blob/main/docs/risc3d-issues.md)

## riscos-mesa

4. **GL windows on a 16 bpp (64K colour) desktop are garbled** (all ports, Pi GPU, software
   Mesa and RoMac; full screen was fine in Quake 3). Screen memory shows each 32-bit pixel written as two
   16-bit pixels: the EGL window path copies the frame without converting it. The riscos-half-life
   port has an SDL patch for the software sprite path.
   [riscos-mupen64plus item 1](https://github.com/davidpsharp/riscos-mupen64plus/blob/main/docs/RISC3D-0.17-report.md),
   with a test program (gl16).
5. **Copying from the window (`glCopyTexSubImage2D`) goes through the CPU:** ~40 ms a frame in
   Doom 3 (its post-process effects, now off by default on the Pi), and each copy ends the GPU
   job. Probably because the window's colour buffer is imported linear memory.
   [Details](RISC3D-0.17-report.md#4-performance)
6. **A refused BO crashes the program** instead of GL_OUT_OF_MEMORY (seen in all three ports
   with item 1). That made items 1 and 2 look like driver bugs.
7. **Deleting an SDL window before its GL context aborts** ("MESA: error: wait failed: -9"), and
   **restarting SDL video loses the GPU** (the next context is software). Both hit ioquake3's
   `vid_restart`; worked around in the game.
   [riscos-quake3 items 1-2](https://github.com/davidpsharp/riscos-quake3/blob/main/docs/risc3d-issues.md)
8. **SDL mouse look:** three small fixes to relative mouse mode (patches 06-08 in
   `riscos/sdl2/patches`, against your 2df5c85 overlay), offered for the overlay. Without 08,
   the view drifts after 06. Your SDL as it is has no drift; its mouse-look speed changes with
   the render size.
9. **Smaller:** in Mesa v3d, client-side vertex arrays draw lit surfaces black, and
   GL_EXT_stencil_two_side has front and back swapped; SDL fails to start in a numbered screen
   mode; software Mesa skips immediate-mode draws after a VBO + ARB-program draw.
   [Details](RISC3D-0.17-report.md#5-a-refused-bo-crashes-the-program-mesa-v3d)

## RISC3D client library

10. **`RISC3D$Disable` is ignored:** `r3d_available` tests `!_swix(OS_ReadVarVal, ...)`, but
    OS_ReadVarVal with R2 < 0 returns an error when the variable *exists*.
    [Details](RISC3D-0.17-report.md#8-10-older-items-not-retested-on-017)
11. **Question:** is `RISC3D_Info` (R3 driver name, R4 bit 1 "draws in software") a supported
    way for a game to choose its defaults before it makes a context? ioquake3 uses it.
