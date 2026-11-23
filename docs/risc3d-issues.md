# RISC3D issues: working log

**For RISC3D's author: read [RISC3D-for-Andy.md](RISC3D-for-Andy.md) instead.** This is the
port's working log, kept for its dates and evidence; the summary and
[RISC3D-0.17-report.md](RISC3D-0.17-report.md) supersede it.

Found while porting dhewm3 (and, where credited, by the ioquake3 port's session). Newest at
the bottom of each section.

Test program: `tests/d3gltest` (this repo). It checks each GL feature dhewm3's renderer uses
by reading pixels back, and times round trips. Build: `riscos/sdk.sh tests/d3gltest/build.sh`
(static, crossdev 1.11 with riscos-mesa devkit 20.3.5-22, `-Wl,-u,riscos_mesa_gpu -lMesaGPU`).
Outputs kept in `build/d3gltest/*.out`.

## Bugs


## RISC3D 0.17 (V3DRender 0.26), tested 23 Nov 2026 on pi4-socrates
Loaded by hand after DAClamp (`$.code.!RISC3D`), 32 bpp desktop, dhewm3 at Medium, 800x600.
- **Item 8 (hangs, frozen window): fixed by 0.23** ("imported BOs follow pages the OS moves").
  0.26 still claims specific pages, but with only item 7 patched (`tests/v3drender/v026-16k.patch`)
  dhewm3 plays Mars City 1 with 0 resets and 0 MMU errors, the window updating, ~12 fps, 3 render
  jobs a frame. 126 MB of BOs in use, 16 MB kept for reuse.
- **Item 7 (4,096 handles): not fixed, now the only blocker.** Stock 0.26: right after the
  level loads, 14,772 "Failed to allocate device memory for BO", then 114 GPU resets, 1,137 MMU
  errors (last: client CLE, page address &00000008) and dhewm3 crashes (item 11).
- **Item 2 (128 MB cap):** unchanged in V3DRender; fine when DAClamp (or !MesaLibs at boot) runs
  first.
- **Item 3:** fixed in 0.22 (per its changelog).
- **Item 6 (stencil two-side swapped):** still there: d3gltest 13 passed, 4 failed (the same 4),
  0 MMU errors, 102 fps.
- **Items 10 and 11 (frame copy through the CPU; Mesa crash on a refused BO):** Mesa's, not
  touched by this release.

### 1. `RISC3D$Disable` is ignored (client library, `lib/r3d_gl.c`, `r3d_available`)
- **Seen:** RoMac (Linux3D), 2026-11-22. With `*Set RISC3D$Disable 1` (set globally, or in
  the Obey file that starts the program), GL_RENDERER is still "Apple M3 Pro (RISC3D, Linux
  GPU)". `LIBGL_ALWAYS_SOFTWARE` does switch to software ("Mesa OffScreen").
- **Cause:**
  - The code is `if (!_swix (OS_ReadVarVal, _INR (0, 4) | _OUT (2), "RISC3D$Disable", 0, -1, 0, 0, &len) && len != 0) return 0;`.
  - With R2 < 0, OS_ReadVarVal returns an **error (V set)** when the variable exists, with
    R2 = NOT length. Checked in BASIC on RoMac: `SYS "XOS_ReadVarVal","RISC3D$Disable",0,-1,0,0`
    with the variable set to "1" returns V set and R2 = -2.
  - So `!_swix(...)` is never true, and the check never fires. Also, `_swix` may not write
    outputs when it returns an error.
- **Why the tests missed it:** the host stand-in, `test/host/swis.h`, returns no error for this
  call, so the host checks pass.
- **Suggested fix:** ignore the error and use R2 on exit (`_swi` with regs, or `_kernel_swi`),
  treating R2 != 0 as "set". Or read into a small buffer, as `modules/RISC3D/risc3d.c`
  `find_driver` already does. Make the stand-in return an error with R2 = NOT len, as RISC OS
  does.

### 2. V3DRender 0.16 on RISC OS 5.30: every buffer of 64 KB or more is refused (-12), so nothing draws on a native Pi 4
- **Seen:** pi4-plato (Pi 4, 4 GB, RISC OS 5.30 of 15 May 2024), !RISC3D 0.05-1 (RISC3D 0.05,
  V3DRender 0.16), 2026-11-22. Found by the riscos-quake3 session first, and reproduced here.
  - It has never worked on this Pi: RISC3D was first installed there today, and a clean
    restart changes nothing.
  - ioquake3 and d3gltest both get GL_RENDERER "V3D 4.2.14.0" and GL 2.1 Mesa 26.1.6. Then
    stderr fills with "MESA: error: Failed to allocate device memory for BO" (19,507 lines in
    one d3gltest run). glGetError gives 0x0505, and the window is black or wrong.
  - In d3gltest, 22 of 23 checks fail. The pixels read back are partly the desktop's backdrop
    colour.
- **Evidence:**
  - `CREATE_BO` called directly (`V3DRender_Ioctl`, &C0106442; BASIC probe by the riscos-quake3
    session, `$.code.q3r3d.probe` on plato):
    - 4, 8, 16 and 32 KB return 0;
    - 64 KB to 2 MB return -12 (ENOMEM);
    - the free pool is 2 GB.
  - `V3DM$Stats` (riscos-quake3) while the game runs: "CREATE_BO ~2400 (all refused) ~1.7 GB,
    ~3.2 s spent" per 5 s, while `*V3DRender` shows "in use 316 KB".
  - Andy's kit V3DCheck (kit 19) fails at "buffer 0 (64 KB): handle 0", then aborts at
    line 410 when it uses handle 0.
  - `*V3DRender` after one d3gltest run: MMU errors go from 473,694 to 950,197, about 470,000
    each run. The last one is always at page address &0931E9E0, status &00000030.
- **Not the 5.30 kernel** (checked 22 Nov 2026). The earlier reading here was that 5.30 ignores
  OS_Memory 12's bit 9 range, because the call returned "page 246784, 964 MB". That number is a
  RISC OS *page number*, not a physical address / 4096:
  - Converted with OS_Memory 0 on plato (`tests/probes/m12-recommendpage.bas`), page 246784 is
    physical **&40400000**, above 1 GB as asked, for runs of 4 KB to 4 MB.
  - RecommendPage (Kernel `s/MemInfo`) has handled bit 9 since df4efb6 (Jeffrey Lee, 2019; first
    in Kernel-6_56). It is byte-for-byte the same in 5.30 (Kernel-6_70-1/-2). The source was
    read from a local mirror (riscos311-re's), as ROOL's GitLab was offline.
- **First reading (superseded by the update below): 0.13's top-down placement of aligned BOs** (tested 22 Nov 2026 on plato, by
  swapping in test builds of 0.16 with `RMKill RISC3D`, `RMKill V3DRender`, `RMLoad test`,
  `RMLoad RISC3D`):
  - *nohigh* (`high_run` always returns DA_RELEASE, as 0.11): the same failure (22 of 23 checks
    fail, 19,507 BO failures, ~470k MMU errors). So memory above 1 GB is **not** the cause.
  - *bottomup* (in `find_pages_aligned`, `align = 1`: every BO first fit from the bottom of the
    area; high memory left as 0.16): d3gltest passes 13 of 17 checks with **0 MMU errors**,
    BOs above 1 GB, at 99 fps. The 4 failures are item 6.
  - So a BO placed near the top of the BO area (PMP page index close to `area_pages`) fails to
    claim or map: either `OS_DynamicArea 21/22` at high PMP page indexes on 5.30, or V3DRender's
    page table or MMU entries for those GPU addresses. (A BASIC probe of the PMP calls couldn't
    create a PMP without a handler, so the kernel side isn't isolated yet.)
- **Update, later on 22 Nov 2026: the real cause is RISC OS 5.30's 128 MB cap on a dynamic
  area's size**, not top-down placement as such. V3DRender's buffer area is a PMP of
  `V3DRender$Size` (256 MB), but 5.30 caps a dynamic area at 128 MB unless something lifts it
  (the `DAClamp` utility, `OS_DynamicArea 8`). Top-down placement puts the first 64 KB+ BOs in
  the area's top half, past 128 MB, where every claim fails ("claim any failed: OS error &1D1",
  at area page &7FF3, which is the 128 MB line). Bottom-up placement only hid it until the area
  filled to 128 MB (then a level load failed the same way, and Mesa crashed, below).
  - Tested: **unmodified 0.16** from !RISC3D, loaded after DAClamp: d3gltest passes 13 of 17
    (the 4 are item 6) with 0 MMU errors at 99 fps. Without DAClamp first: 22 of 23 fail.
  - Tested: run DAClamp, *then* load V3DRender (so its area is made under the lifted cap), with
    0.16's own top-down placement restored: dhewm3 plays Mars City 1 with **137 MB** of BOs in
    use, 0 resets, 0 MMU errors.
  - Suggestion: create the area at a size the OS allows (read the cap, or lift it with
    `OS_DynamicArea 8` as DAClamp does), and say so in `*V3DRender` when the area is smaller
    than `V3DRender$Size`. A game can't fix this itself once booted (the area
    exists from boot), **but install order can** (riscos-n64, 23 Nov 2026): with riscos-mesa's
    !MesaLibs in `$.!Boot.Resources`, its !Boot runs DAClamp before !RISC3D (Resources boot in
    name order) loads V3DRender, so stock 0.16 gets its full 256 MB after a clean restart.
    Mupen64Plus on plato: 0 BO errors with !MesaLibs, BO errors and a crash without. plato had
    no !MesaLibs because the GPU test programs link riscos-mesa statically. It now has it.
    Still worth fixing in V3DRender (or making !RISC3D run DAClamp), since a static-linked
    program can't rely on !MesaLibs being installed.
- **With bottomup, dhewm3 reaches its main menu on the Pi 4's GPU**, but loading
  demo_mars_city1 at Ultra quality gets 292 "Failed to allocate device memory for BO". Mesa then
  crashes in `mgpu_v3d_bo_map`, reading through freed memory (r6 = &FDFDFDFD, `ldr r0,[r6,#&754]`).
  So riscos-mesa's v3d also mishandles a failed BO allocation. The same 292 failures happen with
  `V3DRender$Size` 1024, so it isn't the area filling up. A 1 GB area also leaves too little
  address space for dhewm3's 1 GB heap ("Unable to allocate logical address space").

### 3. `*V3DRender`'s "last:" MMU error line prints garbage for the client name
- **Seen:** pi4-plato, V3DRender 0.16. The first time it read "last: client õ  áÿ/á ãøAmáÀMàø`Íá°å°âàåèå,
  page address &0931E9E0"; on the next run it read "last: client , page address ...".
- It looks like the client name is a lookup out of range, or a pointer to memory that's no
  longer valid. Minor, but it hides which GPU client faulted, which item 2 needs.

### 4. riscos-mesa's SDL fails to start in a numbered screen mode
- **Seen:** RoMac's default headless desktop (640x480, 256 colours, a mode number), by both
  sessions. `SDL_Init(SDL_INIT_VIDEO)` fails with "Unsupported mode block format 1380275278"
  (&5245544E, "NTER": the bytes it happened to read).
- **Cause:** `RISCOS_InitModes` (`src/video/riscos/SDL_riscosmodes.c`, SDL 2.32.10 in riscos-mesa
  2df5c85) calls OS_ScreenMode 1 and treats R1 as a pointer to a mode selector block. When the
  current mode is a number (< 256), R1 is that number, and SDL reads low memory.
  - This code is upstream SDL's, which riscos-mesa carries.
  - A numbered mode is rare on a real desktop, but RISC OS on Linux and RoMac can start in one.
- **Workaround:** `*WimpMode X1024 Y768 C16M` before starting the program.
- **Suggested fix:** if R1 < 256, convert the mode number to a selector (OS_ReadModeVariable
  for each variable), or ask with OS_ScreenMode 1 after a `*WimpMode` to a selector.

### 5. Software Mesa (riscos-mesa 20.3.5, OSMesa): after a VBO and ARB-program draw, immediate-mode drawing writes nothing
- **Seen:** `LIBGL_ALWAYS_SOFTWARE 1`, `d3gltest -nocanary`:
  - on RoMac, 3 runs out of 3;
  - on a real Pi 4 (pi4-plato, RISC OS 5.30), the same 19 failures (`build/d3gltest/pi4-sw-nocanary.out`).
  So it's riscos-mesa's, not the emulator's.
- **What happens:** the ARB program and VBO checks pass. Every draw after them writes nothing:
  stencil, depth bounds, S3TC, blending and depth-equal, all glBegin/glEnd quads.
- **What makes it go away:** one plain `glBegin`/`glEnd` quad straight after the VBO checks
  (d3gltest's default "canary"), with no other changes. Then all 16 checks pass, on RoMac and
  on the Pi.
- The GPU (Linux3D on the Mac) passes the `-nocanary` order.
- **The sequence just before it** (`t_vbo`):
  1. glDrawElements with vertices in a VBO (`glBufferData` NULL, then `glBufferSubData`),
     indices in client memory, generic attributes 8 and 9, the ARB vertex and fragment programs
     enabled;
  2. the same with indices in an element buffer;
  3. unbind both buffers and disable the programs.
- **The next draw** has the colour mask off, depth writes off, two-sided stencil and
  immediate-mode quads (`shadow_faces`).
- dhewm3 itself will run on the GPU, so this matters to it only as a software fallback. It
  might be Mesa 20.3.5's own vbo/immediate-mode code, not a RISC OS change.

## Not bugs, but worth Andy knowing
- **RoMac's Mac player has no `GL_EXT_depth_bounds_test`.** Apple's GL 2.1 doesn't offer it.
  dhewm3 copes without it (it skips the depth-bounds optimisation), so it's information only.
  V3D (Mesa 26) doesn't offer it either.
- **Round-trip costs on RoMac (M3 Pro, Linux3D):**

  | Call | Cost |
  |---|---|
  | glGetError | about 11 µs |
  | glReadPixels 1x1 | about 42 µs |
  | glFinish | about 4 µs |

  That's cheap enough that dhewm3's `GL_CheckErrors` (a glGetError for each check, even with
  `r_ignoreGLErrors 1`) isn't worth patching yet.
- **d3gltest on RoMac's GPU passes everything dhewm3 needs:** ARB vertex and fragment programs
  (including `state.matrix.mvp`), attributes 8 and 9, VBOs with indices in memory and in a
  buffer, two-sided and ATI stencil (z-pass and z-fail), S3TC DXT1, and additive blending. The
  benchmark runs at 320 fps for 64 interaction quads at 256x256.

## Not RISC3D: Rorth under RoMac
- **Rorth under RoMac:** with crash dumps on, every GCC 10 program dies with "EMT trap" at
  start, which is Rorth#11's symptom. The 0.15 fix restores DFAR/DFSR only on ARMv6+ hardware,
  and on RISC OS on Linux data aborts arrive by another route.
  - With `crashdumps off` in `romac/data/hd4/!Rorth/Config`, the programs run.
  - Worth a Rorth issue (label `from-riscos-doom3`); not filed yet.

### 6. Mesa v3d (Pi 4): GL_EXT_stencil_two_side has front and back swapped
- **Seen:** d3gltest on plato with the bottomup V3DRender: `stencil-two-side-zpass` and
  `-zfail` fail with the two faces' results exchanged (red where green is expected and the other
  way round). `GL_ATI_separate_stencil` with the same geometry passes, as do both on RoMac.
- Likely in Mesa's handling of `glActiveStencilFaceEXT` for v3d, or the winding it assumes.
  dhewm3 uses glStencilOpSeparate (GL 2.0) when it has it, so it may not matter to the game.

### 7. V3DRender: 4,096 buffer handles per file (8,192 in all) are too few for Doom 3
- **Seen:** pi4-plato, dhewm3 loading demo_mars_city1 at Medium quality with the bottomup test
  build: CREATE_BO fails 3 times at the end of the level load (the textures' upload), and
  nothing in V3DRender's own failure paths logs it. Mesa then crashes (see item 2's last
  point). At Ultra the failures run to over 1,000.
- **Cause:** `create_bo` (core/v3d_ioctl.c) returns -ENOSPC when a file's `V3D_MAX_HANDLES`
  (4096) handles are in use; `V3D_MAX_BOS` is 8192 for the whole GPU. Doom 3 makes a buffer for
  every texture and vertex buffer. Linux's limits are far higher.
- **Tested fix:** both raised to 16384 (`tests/v3drender/bottomup-16k-log.patch`, which also
  logs every CREATE_BO failure in `*V3DRender`): no more failures; the level loads.
- **Suggestion:** raise both, or grow the tables, and put a CREATE_BO failure in the log.

### 8. V3DRender: claiming specific physical pages moves imported memory under the GPU (the hangs and the frozen window)
- **Seen:** pi4-plato, dhewm3 loading demo_mars_city1: two render jobs hang (V3DRender resets
  the GPU), then the game plays but the window keeps showing the loading screen.
- **The evidence:** the first hang's `*V3DDump`
  (`tests/v3drender/logs/2026-11-22-hang1-decoded.txt`): the render list was stuck in its
  generic tile list at &057286E4, and that memory held `00 00 00 ff` over and over: opaque
  black, the frame's clear colour. **The GPU had stored a tile of pixels over its own control
  list**, so two GPU buffers shared physical pages.
- **Cause (confirmed by the fix):** `plat_pages_claim` asks OS_Memory 12 for a run of pages
  and claims those *specific* pages (OS_DynamicArea 21 with a page number). RecommendPage only
  skips Unavailable/Required/Reserved pages, so it can name a page another dynamic area owns,
  and the kernel then **moves that area's contents** to another page to hand it over. The
  window surface is the program's own memory, imported (`DRM_IOCTL_RISCOS_IMPORT_MEMORY`) with
  its physical addresses read once (`b->pa`). When its pages move, the GPU keeps rendering the
  window into the old pages, which now belong to V3DRender's own BOs (here a control list).
  The window stops updating and the corrupted lists hang the GPU.
- **Tested fix:** never claim specific pages (`high_run` returns DA_RELEASE, so every page is
  "any page" from the free pool). With that, 16k handles (item 7), and the area made after DAClamp (item 2)
  (`tests/v3drender/playable.patch`; load it with `tests/v3drender/UseTestV3D`):
  **dhewm3 plays Mars City 1 on the Pi 4** (intro ~20 fps, in play ~9 fps at 800x600, Medium),
  0 resets, 0 MMU errors. The pages still come from above 1 GB (`*V3DRender` shows all of them
  there), so 0.12's goal may still hold.
- **Suggestions:** claim only free-pool pages (or check a recommended page is free first);
  and make imported memory stay put while the GPU holds it (lock its pages, e.g. the PMP-style
  "page unavailable" flags, as Linux pins userptr pages).

### 9. (Solved: see item 8.) GPU hangs while dhewm3 loads a level, then a frozen window
- The first report of item 8's symptoms; the register logs are in
  `tests/v3drender/logs/2026-11-22-mars-city1-medium-16k.txt`.

### 10. Performance on the Pi 4: dhewm3 in play at ~9 fps, about 8 GPU render jobs a frame
- **Seen:** pi4-plato, test V3DRender (item 8), dhewm3 Mars City 1 in play, 800x600, Medium
  quality. `com_speeds`: ~111 ms a frame, of which ~84 ms is the back end (GL calls and waits),
  ~10 ms game, ~13 ms front end. Intro cinematic ~16-20 fps, menus ~44 fps. Full screen is no
  faster; turning off shadows changes nothing; skipping lighting interactions saves ~25 ms.
- **The GPU isn't full:** over 10 s of play `*V3DRender` counted 742 render jobs (bin 405 ms,
  render 4,055 ms of GPU time), so it was busy under half the time, but each frame is split into
  about **8 render jobs**, each loading and storing the whole 800x600 tile buffer.
- **Why the jobs are split** (`V3DM$Why`, one run): 1,308 "pipe flush (glFlush, glFinish, a
  swap, a read-back)", all from one caller; 462 "directly"; 53 "reading a resource a job
  writes" (dhewm3 copies the frame into textures for its post-process and GUI effects, with
  `glCopyTexSubImage2D`).
- **And BO churn:** `V3DM$Stats` showed ~2,200 `CREATE_BO` (~840 MB) and ~7,800 `WAIT_BO`
  (~2.3 s waiting) per 5 s: Mesa makes and frees buffers (probably streaming vertex/index
  uploads) far faster than on Linux, and much of the frame is spent waiting on BOs.
- **Questions for Andy:** where the pipe flushes come from (the caller offset is in the
  `V3DM$Why` output); whether the BO cache (Mesa's `v3d_bo_cache`) is active on RISC OS; and
  whether a frame copy could stay on the GPU without ending the job.

- **Follow-up, 22 Nov 2026 evening (per-frame `V3DM$Why` in play, same view each run):**
  - Of ~7 jobs a frame: 1 swap; 4 flushes before `v3d_blit` (frame copies,
    `glCopyTexSubImage2D`); 1 from `v3d_draw_vbo`; 1 from `v3d_resource_transfer_map`
    mapping for *reading* something a job just drew.
  - dhewm3 copied the 800x600 frame into a 1024x1024 texture as three copies (the image, an
    extra column, an extra row). Copying at the frame's own size (non-power-of-two textures, now
    the default in the port, `r_npotFramebufferCopy`): **7 -> 5 jobs**, 117 -> 100 ms a frame.
  - Turning post-processing off (`r_skipPostProcess 1`) removes the last copy *and* the
    `transfer_map` read: **3 jobs a frame**. So Mesa's `glCopyTexSubImage2D` from the window
    reads the frame back through the CPU (st's fallback) instead of blitting on the GPU. An
    RGBA8 texture instead of RGB8 made no difference. Possibly because the window's colour
    buffer is linear imported memory that v3d's blit can't take as a source. **This is the
    single biggest cost**: ~40 ms a frame. The port now turns post-processing off when
    V3DRender is loaded. Fixing the copy would bring back the heat haze and glass effects.
  - Frame rates now: ~11-15 fps looking across the hangar, ~26-28 fps facing a wall (from ~9).
    Repeated runs vary a lot, so trust the job counts more than single fps numbers.
  - **A rendering bug:** with `r_useVertexBuffers 0` (client-side vertex arrays) the lit
    surfaces draw black (only some lights and the HUD show). Compare item 5, the same kind of
    failure in software Mesa.

### 11. Mesa v3d: a refused BO allocation crashes the program
- Whenever `CREATE_BO` fails (the full area of item 2), riscos-mesa goes on to use the missing
  buffer: once a data abort in `mgpu_v3d_bo_map` reading freed memory (r6 = &FDFDFDFD), once
  `memcpy` to address 0 (dhewm3 on plato, 22 Nov 2026, 14:07: "data abort accessing &00000000",
  in `bcopy` called from libMesaGPU). Linux's Mesa has the same checks missing, but there the
  kernel rarely refuses. A GL_OUT_OF_MEMORY would let the game say so instead.
- Also seen once: d3gltest ended with "Fatal signal received: EMT trap" while exiting, on stock
  0.16 loaded after DAClamp (all checks had already run).
