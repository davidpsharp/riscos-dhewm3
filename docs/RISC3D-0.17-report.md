# Doom 3 (dhewm3) on RISC3D 0.17: performance and issues

For Andrew Youll, from the riscos-dhewm3 port. 23 November 2026.

dhewm3 1.5.5 (the GPL Doom 3 engine) now plays on a native Raspberry Pi 4 through RISC3D,
V3DRender and riscos-mesa's libMesaGPU (Mesa 26.1.6 v3d). This report covers what we saw with
**RISC3D 0.17 (RISC3D module 0.09, V3DRender 0.26)**, and the earlier 0.16 findings that
still apply. It's in priority order: the first two items decide whether Doom 3 runs at all.

Thank you for 0.23's "imported BOs follow pages the OS moves". It fixed the worst problem we
had (section 3).

## Summary

| # | Issue | Where | State with 0.17 | Effect on Doom 3 |
|---|---|---|---|---|
| 1 | 4,096 buffer handles per file (8,192 BOs in all) | V3DRender | **Not fixed** | **Blocker:** crashes as the first level finishes loading |
| 2 | The BO area is capped at 128 MB unless DAClamp ran before V3DRender loaded | V3DRender / !RISC3D | **Not fixed** (works only by boot order) | Every BO of 64 KB+ refused, or refusals once 128 MB is used |
| 3 | GPU hangs and a frozen window during level loads | V3DRender | **Fixed by 0.23** | None now |
| 4 | Frame copies (`glCopyTexSubImage2D` from the window) read back through the CPU | Mesa v3d (riscos-mesa) | Not changed in 0.17 | ~40 ms a frame; we turn post-processing off |
| 5 | A refused BO crashes the program instead of GL_OUT_OF_MEMORY | Mesa v3d (riscos-mesa) | Not changed in 0.17 | Turns issues 1 and 2 into crashes |
| 6 | Client-side vertex arrays draw lit surfaces black | Mesa v3d | Not retested on 0.17 | `r_useVertexBuffers 0` unusable |
| 7 | GL_EXT_stencil_two_side has front and back swapped | Mesa v3d | **Still there** | None (dhewm3 uses glStencilOpSeparate) |
| 8 | `RISC3D$Disable` is ignored | RISC3D client library | Not retested (0.17 notes: client library unchanged) | Minor |
| 9 | SDL fails in a numbered screen mode | riscos-mesa's SDL | Not retested | Minor |
| 10 | Software Mesa: immediate-mode draws write nothing after a VBO + ARB-program draw | riscos-mesa 20.3.5 | Not retested | Software fallback only |
| 11 | GL windows garbled on a 16 bpp desktop (found by riscos-mupen64plus) | riscos-mesa (SDL/EGL presentation) | Seen with dhewm3 on RoMac, both SDLs | Unplayable at 64K colours |

**With only issue 1 patched (handles raised to 16,384), V3DRender 0.26 runs Doom 3's first
level with 0 GPU resets and 0 MMU errors.**

## How we tested

- **Machine:** Raspberry Pi 4, 4 GB, RISC OS 5.30 (15 May 2024), ARM at 1500 MHz, desktop
  1280x800 at 32 bpp (pi4-socrates; the 0.16 work was on a second Pi 4, pi4-plato).
- **Install:** RISC3D-0.09-1.zip's `!RISC3D`, started by hand after DAClamp: `DAClamp`, then
  `Obey !RISC3D.!Boot`. Nothing was installed in `!Boot`.
- **The game:** dhewm3 1.5.5 built with GCCSDK (crossdev 1.11) and riscos-mesa's devkit
  20.3.5-22, linked statically with `-lMesaGPU`. Doom 3 demo data, Medium quality, an
  800x600 desktop window, demo_mars_city1 (the intro cinematic, then play in the Mars City
  Hangar).
- **The GL check program:** `tests/d3gltest` in this repo. It checks each GL feature dhewm3's
  renderer uses by reading pixels back (ARB programs, VBOs, two-sided and separate stencil,
  depth bounds, S3TC, blending), then times a benchmark.
- **SDL:** this port's build of riscos-mesa's SDL with input and window patches; none touch
  the GL path. Checked against riscos-mesa's own SDL in "Which SDL this port uses".
- **Tools:** `*V3DRender`, `V3DM$Stats`, `V3DM$Why`, and the game's own `com_speeds` (per frame:
  game, front end and back end ms).

## 1. Buffer handles: 4,096 per file is too few (the blocker)

- **With stock 0.26:** right after "all images loaded", stderr fills with 14,772 "MESA: error:
  Failed to allocate device memory for BO" and one "Failed to create tiled blit temp". Then
  `*V3DRender` shows 114 GPU resets ("Resetting GPU for hang. V3D_ERR_STAT: &00001000") and
  1,137 MMU errors (last: client CLE, page address &00000008), and dhewm3 dies with a data abort.
- **Cause:** `create_bo` (core/v3d_ioctl.c) returns -ENOSPC when a file has `V3D_MAX_HANDLES`
  (4096) handles open. `V3D_MAX_BOS` (8192) also bounds the whole GPU. Doom 3 makes a BO for
  every texture, vertex buffer and index buffer, and Mesa adds its own. Linux's driver has no
  comparable small limit.
- **Tested fix:** both raised to 16384 in `core/v3d.h` (`tests/v3drender/v026-16k.patch`, a
  two-line change). Mars City 1 loads and plays, 0 resets, 0 MMU errors. `*V3DRender` then
  showed 5,321 buffers made, 125 MB in use, and the reuse pool full at 15.9 MB.
- **Suggestions:**
  - raise both limits, or grow the tables on demand;
  - log a CREATE_BO refusal and its reason in `*V3DRender` (handles, BO slots, area full or
    claim failed), as nothing did in 0.16;
  - mention the limit in docs/INTERFACE.md, so a program author can see why a create fails.

## 2. The 128 MB dynamic area cap (still an issue, though boot order can hide it)

- **What happens:** RISC OS 5.30 caps a dynamic area at 128 MB unless `OS_DynamicArea 8` lifts
  it (riscos-mesa's `DAClamp`). V3DRender's BO area is a PMP of `V3DRender$Size` (256 MB).
  - If V3DRender loads before the cap is lifted, its area stays capped.
  - 0.16's top-down placement then put the first 64 KB+ BOs above 128 MB, so every one was
    refused (-12). That was "nothing draws on a native Pi 4".
  - Bottom-up placement only delays it: claims fail once 128 MB is used ("claim any failed: OS
    error &1D1", at area page &7FF3). Doom 3 uses ~126-137 MB in its first level.
- **Proof:** stock 0.16, loaded after DAClamp: d3gltest passes (13 of 17; the 4 are issue 7) with
  0 MMU errors at 99 fps. Without DAClamp first, 22 of 23 checks fail with ~470,000 MMU errors.
- **Why it works on some machines and not others:** !MesaLibs' !Boot runs DAClamp. Resources
  boot in name order, so `!MesaLibs` before `!RISC3D` lifts the cap in time (found by the
  riscos-n64 port). Our test Pi had no !MesaLibs, because programs that link riscos-mesa
  statically don't need it, so V3DRender got 128 MB.
- **Why it's still worth fixing:** it rests on a boot order that is easy to get wrong and hard
  to diagnose:
  - it depends on an unrelated package being installed, and on its name sorting first;
  - a statically linked program (the recommended way, with `-lMesaGPU`) has no reason to need
    !MesaLibs, so a clean install of just RISC3D and the program fails;
  - any change to boot order (a renamed app, Resources in another place, a boot sequence that
    loads modules differently, or a user loading V3DRender by hand) brings it back;
  - the symptom (Mesa's "Failed to allocate device memory for BO", MMU errors) looks like a
    GPU driver bug, not a memory setting, and `*V3DRender` shows the full 262,144 KB size.
- **Suggestions,** any of:
  - V3DRender lifts the cap itself before creating its area (`OS_DynamicArea 8`, as DAClamp
    does), or creates the area at the largest size the OS allows and says so;
  - !RISC3D's !Boot runs DAClamp (it's your own riscos-mesa tool) before loading RISC3D;
  - at the least, `*V3DRender` shows the area's real maximum, and warns when it's below
    `V3DRender$Size`.

## 3. Hangs and the frozen window: fixed by 0.23

- **Seen with 0.16:** during Doom 3's level load, render jobs hung (GPU resets), then the game
  played but the window kept showing the loading screen.
- **Evidence:** a `*V3DDump` after the first hang showed the render list stuck in its generic
  tile list, and that memory held `00 00 00 ff` over and over (opaque black, the clear colour).
  The GPU had stored pixels over a control list, so two buffers shared physical pages.
- **Cause:** `plat_pages_claim` claims specific pages recommended by OS_Memory 12, which may
  belong to another dynamic area. The kernel then moves that area's data, including the
  program's imported window memory, whose physical addresses the GPU had already been given.
- **0.16 workaround we tested:** never claim specific pages (`high_run` returns DA_RELEASE). It
  fixed the problem (`tests/v3drender/playable.patch`).
- **0.26:** still claims specific pages, but imported BOs now follow moved pages
  (Service_PagesUnsafe/Safe). With only issue 1 patched: 0 resets, 0 MMU errors, the window
  updating throughout. **Fixed.**

## 4. Performance

### Where it stands
On the Pi 4, Medium quality, 800x600 window, Mars City 1:

| | 0.16 + our patches (plato) | 0.26 + 16k handles (socrates) |
|---|---|---|
| Main menu | ~44 fps | — |
| Intro cinematic | ~16-20 fps | — |
| In play, looking across the hangar | ~9 fps at first; 11-15 fps after the port's changes below | ~12 fps (82-100 ms a frame) |
| In play, facing a wall | 26-28 fps | — |
| Render jobs a frame | 7, now 3 | 3 (735 in 20 s) |

Back-to-back runs vary by up to ±30%, so job counts are firmer than single fps figures.
`com_speeds` in play: ~60 ms of each frame is the back end (GL calls and waiting), 7-10 ms the
game, 12-15 ms the front end. Full screen is no faster than a window. Turning off shadows
changes nothing.

For context, the riscos-quake3 port on the same patched V3DRender reported Quake 3 at 138 fps
at 480x300 (software: 35), 72 fps at 1024x768, and 45 fps at 1680x1050 full screen.

### What splits Doom 3's frames into jobs
`V3DM$Why` per frame in play (0.16 + patches), before the port's changes:
- 1 swap (pipe flush from `st_context_flush`);
- **4 flushes before `v3d_blit`**: dhewm3 copied the 800x600 frame into a 1024x1024 texture
  as three `glCopyTexSubImage2D` (the image, an edge column and an edge row), for post-process
  effects;
- 1 from `v3d_draw_vbo` (a draw reading something a job writes);
- 1 from `v3d_resource_transfer_map`, mapping for reading something a job has just drawn.

The port now copies at the frame's own size when non-power-of-two textures are available (1 copy
instead of 3): 7 jobs became 5, and the same view went from 117 to 100 ms. Turning
post-processing off removes the copy **and** the `transfer_map` read-back: 3 jobs, ~55-60 ms.
The port turns post-processing off when V3DRender is loaded, so the heat-haze and glass effects
are lost.

### Issue 4: the frame copy goes through the CPU (~40 ms a frame)
- `glCopyTexSubImage2D` from the window's colour buffer into a texture triggers a
  `transfer_map` *read* of what the GPU just drew. That's Mesa's state tracker falling back to a
  CPU copy (map, read, upload) instead of a GPU blit, and it stalls the pipeline.
- Making the texture GL_RGBA8 instead of GL_RGB8 made no difference, so it isn't the format.
- **Guess:** the window's colour buffer is imported, linear memory (the program's own window
  sprite), which v3d's blit path won't take as a source, so the copy falls back to the CPU.
- **Asks:** a GPU path for copies out of an imported/linear colour buffer (a TFU or render-based
  blit), or rendering into a tiled buffer and converting once at the swap. Many GL games copy
  the frame (Doom 3's post-processing, Quake 3's and others' screen effects), so this would help
  well beyond Doom 3.

### BO churn and waiting
`V3DM$Stats` in play (0.16, 5 s windows): ~2,200 `CREATE_BO` (~840 MB) and ~7,800 `WAIT_BO`
(~2.3 s spent waiting). GPU time over 10 s of play was bin 405 ms and render 4,055 ms, so the
GPU was busy under half the time. 0.25's reuse pool helps (in 0.26: 1,124 of 5,321 BOs made
entirely from kept pages), but it is full at 16 MB within seconds, so a larger or configurable
pool may be worth trying.

## 5. A refused BO crashes the program (Mesa v3d)

When `CREATE_BO` fails, riscos-mesa carries on with the missing buffer:
- once a data abort in `mgpu_v3d_bo_map` reading freed memory (r6 = &FDFDFDFD,
  `ldr r0,[r6,#&754]`);
- once `memcpy` to address 0 (`bcopy`, called from libMesaGPU).

Linux's Mesa has the same gaps, but there the kernel rarely refuses. Returning GL_OUT_OF_MEMORY
would let a program report "out of GPU memory" instead of crashing, and would have made issues
1 and 2 far quicker to find.

## 6. Client-side vertex arrays draw lit surfaces black (Mesa v3d)

With `r_useVertexBuffers 0` (vertices from client memory, not VBOs) the level draws almost
entirely black: only some lights, the HUD and a few surfaces show. Frame times also drop,
suggesting draws are skipped. It looks like the same kind of failure as issue 10 in software
Mesa. Not retested on 0.17. Doom 3 uses VBOs by default, so it doesn't need this, but other
GL 1.x/2.x programs use client arrays heavily.

## 7. GL_EXT_stencil_two_side front and back swapped (Mesa v3d)

d3gltest's `stencil-two-side-zpass` and `-zfail` fail with the two faces' results exchanged.
`GL_ATI_separate_stencil` with the same geometry passes, as do both on RoMac's Linux3D. Still
there with 0.26: 13 passed, 4 failed (these 4), 0 MMU errors, 102 fps on the benchmark (64
interaction quads at 256x256). Probably Mesa's `glActiveStencilFaceEXT` handling for v3d.
dhewm3 prefers glStencilOpSeparate, so it doesn't affect the game.

## 8-10. Older items, not retested on 0.17

Details are in `docs/risc3d-issues.md` (items 1, 4 and 5 there):
- **`RISC3D$Disable` is ignored:** `r3d_available` tests `!_swix(OS_ReadVarVal, ...)`, but with
  R2 < 0 OS_ReadVarVal returns an error (V set) when the variable *exists*, so the check never
  fires. The host stand-in returns no error, so the tests miss it.
- **SDL in a numbered screen mode:** `RISCOS_InitModes` treats OS_ScreenMode 1's R1 as a
  selector pointer even when it's a mode number, giving "Unsupported mode block format
  1380275278". Seen on RoMac's default 640x480 256-colour desktop.
- **Software Mesa (20.3.5 OSMesa):** after a VBO + ARB-program draw, every later
  immediate-mode draw writes nothing, until one plain glBegin/glEnd quad is drawn.

## Also fixed in 0.17, as far as we can tell

- `*V3DRender`'s "last:" MMU error line printed garbage for the client name in 0.16; 0.22's
  changelog fixes it, and 0.26 printed "client CLE".

## 16 bpp (64K colour) desktops: GL windows are garbled

The riscos-mupen64plus port reported (23 Nov 2026) that on a 64K colour desktop every GL
window through riscos-mesa's SDL/EGL is garbled: each 32-bit pixel lands as two 16-bit
pixels, so the window shows the left half of the frame at double width in the wrong colours.
They saw it on the Pi 4 GPU (0.16 and 0.26), in software Mesa and on RoMac. Details and a test
program (gl16) are in that port's report, item 1.

**dhewm3 has it too:** on RoMac at 1024x768 in 64K colours, the main menu is garbled exactly
that way. It's the same with this port's SDL and with riscos-mesa's own (next section). Every
other test in this report ran on a 32 bpp desktop.

## Which SDL this port uses, and riscos-mesa's own SDL

This port links its own build of SDL 2.32.10 (`riscos/sdl2`), not the devkit's `libSDL2.a`.
It is riscos-mesa's SDL source (the overlay at riscos-mesa 2df5c85, unchanged, the latest),
plus eight patches shared with the riscos-quake3 port.

**None of the patches touch SDL's GL path (`SDL_riscosopengl.c`) or its audio.** So nothing in
sections 1-7 depends on them. They change only:

| Patch | What it does | Matters for |
|---|---|---|
| 01 | RISC OS 5 sprite mode words: the eigen factors for SDL's software (non-GL) framebuffer | 16 bpp software windows, not GL |
| 02 | Icon bar menu with Info and Help | Looks only |
| 03 | The application directory as the program's name | Looks only |
| 04 | RoMac's absolute pointer (RoMac before 0.1.228) | Mouse look on older RoMac |
| 05 | After a mode change: window kept on screen, first mouse reading dropped | Full screen mode changes |
| 06 | Mouse look in screen pixels, not window pixels | Mouse-look speed at a reduced render size |
| 07, 08 | The first reading after relative mode starts isn't a move; the pointer is centred on a whole pixel | 08 fixes a drift that 06 introduced; riscos-mesa's SDL has no drift |

**Tested with riscos-mesa's SDL** (the devkit's `libSDL2.a`, 20.3.5-22), on RoMac, 32 bpp,
Mars City 1, 23 Nov 2026:
- it starts, opens the 800x600 window through RISC3D ("Apple M3 Pro (RISC3D, Linux GPU)"),
  and plays the level at the same ~35-60 fps as with this port's SDL;
- mouse look works: a 150-pixel move turned the view and the pointer was re-centred;
- at 16 bpp it's garbled exactly as with this port's SDL (above);
- one frame during the intro cinematic showed horizontal shearing (tearing). Frames in play
  didn't, and a comparison with this port's SDL wasn't conclusive, so it isn't counted as a
  difference.

Not yet tried on the Pi 4 with riscos-mesa's SDL. Nothing in the GL path differs, so the
frame rates and GPU findings shouldn't change.

## Files in this repo

The **v0.1.0 release** on GitHub has the binaries used for this report:
`riscos-dhewm3-0.1.0.zip` (a RISC OS zip that keeps file types) with `!dhewm3`, `V3DTest` (0.26
with the 16k handle limits, and a `Load` Obey file that runs DAClamp first) and `D3GLTest`.


- `tests/v3drender/v026-16k.patch`: issue 1's fix on 0.26 (against riscos-v3drender 941b500).
- `tests/v3drender/playable.patch`: the 0.16 build we played on (16k handles, no specific-page
  claims, CREATE_BO failure logging).
- `tests/v3drender/build-module.sh`: builds the module alone with GCCSDK.
- `tests/v3drender/logs/`: the decoded `*V3DDump` from issue 3, and register logs.
- `tests/d3gltest/`: the GL check program.
- `riscos/sdl2/patches/`: this port's SDL patches (see "Which SDL this port uses").
- `docs/risc3d-issues.md`: the running log of every item, with dates and evidence.
