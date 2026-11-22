# RISC3D issues queued for Andy

Found while porting dhewm3 (and, where credited, by the ioquake3 port's session). David
passes these on; nobody here contacts Andy directly. Newest at the bottom of each section;
mark an item **sent** with the date when David has passed it on.

Test program: `tests/d3gltest` (this repo). It checks each GL feature dhewm3's renderer uses
by reading pixels back, and times round trips. Build: `riscos/sdk.sh tests/d3gltest/build.sh`
(static, crossdev 1.11 with riscos-mesa devkit 20.3.5-22, `-Wl,-u,riscos_mesa_gpu -lMesaGPU`).
Outputs kept in `build/d3gltest/*.out`.

## Bugs

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
- **Likely cause** (the riscos-quake3 session's reading of the source, not yet confirmed
  against the 5.30 kernel):
  - 64 KB is `HIGH_MIN_RUN` in `riscos/plat_pi4.c`. From there `plat_pages_claim` asks
    `high_run()` for physically contiguous pages above 1 GB.
  - That call is OS_Memory 12 with bit 9, r4 = &40000000 and r6 = &FFFFFFFF.
  - On 5.30 that exact call (`$.code.q3r3d.mem12`) returns page 246784, which is 964 MB:
    *below* the 1 GB asked for, in the firmware's region. The plain form returns page 12368
    (48 MB).
  - So 5.30 seems to ignore the bit 9 range. V3DRender then tries to claim pages it can't
    own, and the claim, then the fallback, end in -12.
- **Suggestion:** on 5.30, check that the page returned is at or above HIGH_FROM (or don't use
  bit 9 there). Alternatively, state 5.31 as the minimum: the V3DRender README says 5.30 or
  later, while Andy tests on 5.31.

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

## For David, not Andy
- **Rorth under RoMac:** with crash dumps on, every GCC 10 program dies with "EMT trap" at
  start, which is Rorth#11's symptom. The 0.15 fix restores DFAR/DFSR only on ARMv6+ hardware,
  and on RISC OS on Linux data aborts arrive by another route.
  - With `crashdumps off` in `romac/data/hd4/!Rorth/Config`, the programs run.
  - Worth a Rorth issue (label `from-riscos-doom3`); not filed yet.
