# dhewm3 for RISC OS on RISC3D: the plan

Written 2026-11-22 from a review of: dhewm3 master (505c5c8, 9 Nov 2026), RISC3D release 0.11
(`~/code/rol-run/third_party/riscos-risc3d`), RoMac's RISC3D work (`~/code/rol-run`, branch
`risc3d`, `docs/risc3d.md`), the released ioquake3 port (`~/code/riscos-quake3`, v0.1.2), and
`~/code/riscos-kb`.

## Goal and platforms

Port dhewm3 (the GPL Doom 3 engine) to RISC OS 5, drawing through Andy Youll's RISC3D, on:

1. **RoMac** (RISC OS on Linux's image on Apple Silicon, Tarmac CPU, Runway kernel). RISC3D goes
   through Linux3D, then the `rol-3d` socket, then romac-3d on the Mac's OpenGL 2.1 (Metal).
   This is the primary platform.
2. **RISC OS on Linux** (ROL), through Linux3D, then `rol-3d` on Linux's Mesa (a Pi 4, the
   Windows Dev Kit 2023, and so on).
3. **Native RISC OS on a Raspberry Pi 4**, through V3DRender: Mesa's own v3d driver runs inside
   the program (`libMesaGPU`) and talks to V3DRender's ioctl SWI.

## What the review found

**1. Pi 4 native can probably draw on the GPU.** RISC3D 0.10 and later ships V3DRender for the
Pi 4, 400 and CM4. It has run ClassiCube, Warzone 2100, TORCS and OpenLara on the Pi 4's GPU.
The condition is that the program is linked with `libMesaGPU`; for a static build that is
`-Wl,-u,riscos_mesa_gpu -lMesaGPU`, about 15 MB more. So the third platform should run at a
real frame rate, not the "extremely slowly" of software rendering. A software fallback would
run at well under 1 fps: riscos-mesa's swrast would interpret ARB fragment programs per pixel,
and Quake III already runs at only 30 fps in software with no shaders.

**2. One binary can serve all three platforms.** riscos-mesa chooses the route at run time,
from the driver RISC3D has loaded. A static `!RunImage` linked with libMesaGPU uses the command
stream on ROL and RoMac, and v3d on a native Pi 4. A machine with no driver falls back to
software. On such a machine the app should warn and stop, not crawl.

**3. dhewm3's GL needs are all carried.** dhewm3 now has only the ARB2 back end
(`renderer/draw_arb2.cpp`). It must have `GL_ARB_vertex_program` and `GL_ARB_fragment_program`,
and it uses multitexture, cube maps, `texture_env_combine`/`dot3`, VBOs, `EXT_stencil_two_side`
or `ATI_separate_stencil`, S3TC and `EXT_depth_bounds_test`. Every one of these is in
riscos-mesa 20.3.5's GL 2.1 list (`docs/surface/riscos-mesa-20.3.5-gl21-extensions.txt`). That
list is what RISC3D's `proto/gen.py` builds the command stream from. Still to prove:
- that Apple's GL 2.1 under romac-3d really runs ARB assembly programs and two-sided stencil;
- that Mesa 26's v3d exposes all of these in a compatibility context. Gallium's st/mesa
  normally does.

**4. dhewm3 is already set up for this kind of port:**
- `HARDLINK_GAME=ON` compiles the game code into the executable, so no `dlopen` is needed.
  Only one game fits in a binary, so base Doom 3 and Resurrection of Evil (d3xp) are two builds.
- It uses SDL2 (or SDL3) for windows and input, and OpenAL for sound. riscos-mesa's devkit has
  SDL 2.26, whose GL windows go through EGL and so through RISC3D, and it has OpenAL Soft.
- It is C++11 (C++98 without ImGui) and already builds for 32-bit ARM Linux.
- It runs on the free Doom 3 demo data, which lets the port be tested without the retail game.

**5. The ioquake3 port is the template.** It is the same kind of program: an id engine on SDL2,
GL, OpenAL and pthreads, cross-built with GCC 10. Reuse its:
- Docker image, `sdk.sh`, `build.sh`, `toolchain.cmake` and `package.sh`;
- SDL2 with the icon bar menu (from DevilutionX);
- `Sys_Milliseconds` from `OS_ReadMonotonicTime` plus HAL counter 0 (UnixLib's clock steps in
  10 ms);
- mouse handling: riscos-mesa's SDL has no relative mouse mode, so read the mouse with
  `OS_Word 21` and re-centre it;
- `!Run`: DA heap, PThreadTicker, sound modules, stdout/stderr redirected;
- the `Q3Prof` sampling profiler and the `com_fpsLog`-style instrumentation;
- Wimp error boxes and the release zip.

One thing changes: the ioquake3 port links riscos-crossdev 1.8.2's software Mesa. This port
needs the RISC3D-aware devkit: !MesaLibs 20.3.5-22 (or later), RISC3D client 0.11, and
libMesaGPU.

## Risks, roughly in order

| Risk | Why it matters | How to find out early |
|---|---|---|
| **Memory** | Doom 3 wants a few hundred MB of heap. The KB says RISC OS 5 caps one dynamic area at 128 MB (a PCem note, not checked on a Pi 4). UnixLib puts its heap in one DA. On a native Pi 4 the GPU's buffers also come out of RISC OS's RAM. | Measure peak heap on an armhf Linux build at low settings. Test how big a DA RO 5.30 on the Pi 4 will make, and what RoMac and ROL allow. Fallbacks: smaller `image_downSize`, sound decompression limits, or an allocator over several DAs. |
| **Apple GL 2.1 and ARB programs** | romac-3d gives a 2.1 legacy context. If ARB_fragment_program is missing, or wrong on Apple Silicon, the Mac player would have to translate ARB to GLSL. That is a big job, in Andy's code. | Milestone 0's smoke test. |
| **Tarmac on a big binary** | OpenTS (9.6 MB of code) ran at a tenth of its speed: Tarmac's code cache flushed every 5 s and lost its flat-map fault marks (lesson 2026-11-16). dhewm3 with libMesaGPU will be 15-25 MB. | Watch Runway's "slow paths" and "tarmac per s" lines. Use the experimental fix branch in `docs/tarmac-requests.md` in the RoMac repo. |
| **Command-stream round trips** | Every `glGet*`/`glGetError` is a `RISC3D_Sync` wait. `GL_CheckErrors` calls `glGetError` even when `r_ignoreGLErrors` is 1 (the default). | Count waits per frame. Patch the hot ones out under `__riscos__`. |
| **Frame readback on RoMac/ROL** | romac-3d reads each frame back into the RISC OS screen. That was fine for TORCS's window; full screen at Retina sizes has not been measured. | Start in an 800x600 window. Measure full screen later. |
| **Unaligned access** | ARMv7 RISC OS faults on unaligned access where Linux fixes it up. File parsers (MD5 models, maps, pk4) are the likely places. | Run the armhf build under the QEMU that traps like RISC OS (`riscos-mgba/docker/armtest/`), and a Mac UBSan build. |
| **Threads** | dhewm3 mixes sound on its own thread. UnixLib pthreads switch on PThreadTicker, which is costly. | The ioquake3 OpenAL thread cost nothing measurable, so this is probably fine. Watch it anyway. |
| **Pi 4 V3DRender limits** | Its GPU memory, and any GL features it fails on, are untested with a load like Doom 3's (stencil shadows, many textures). | Milestone 0 on `pi4-plato`, then the demo level at low settings. |
| **Rorth #11** | GCC 10 EABI apps died with "EMT trap" at first stack growth while Rorth's crash-dump hook was on. | Check the issue's status before the first Pi run. |

## M0 results so far (2026-11-22)

- **Harness:** Rorth runs inside a RoMac of this project's own (`tools/romac-run`, target
  `romac-doom3`, headless, no window). Its disc is the git-ignored `romac/data/hd4`, so files
  are copied straight in, not sent through Rorth. It needs:
  - `crashdumps off` (see `docs/risc3d-issues.md`, "For David");
  - a 16M-colour mode before any SDL program starts (`*WimpMode X1024 Y768 C16M`).
- **Toolchain:** `riscos/docker` = crossdev 1.11 + riscos-mesa devkit 20.3.5-22 (libMesaGPU),
  the same as the ioquake3 port's r3d image.
- **RoMac's GPU does everything dhewm3 needs** (d3gltest 12/12, no depth bounds), so the
  Apple GL risk is gone.
- **Native Pi 4 is blocked:** V3DRender 0.16 refuses every buffer of 64 KB or more on RISC OS
  5.30 (issue 2 in `docs/risc3d-issues.md`). That's for Andy (or a 5.31 ROM on a test Pi).
  M5 waits on it; RoMac and ROL work doesn't.
- **Memory:** !MesaLibs runs DAClamp, which lifts RISC OS 5's 128 MB limit on dynamic areas,
  so UnixLib's heap can grow past 128 MB. Still to measure what dhewm3 needs.
- **Desktop window** is the default presentation (David, 2026-11-22). David owns Doom 3 and
  RoE on Steam (on ultramagnus): we use the demo until the retail data is needed.

## First light on RoMac (2026-11-22)

dhewm3 1.5.5 (upstream 505c5c8, base game linked in) runs on RoMac in an 800x600 desktop
window, on the M3 Pro's GPU through RISC3D (ARB2 renderer, all extensions but depth bounds),
with the free demo's data:

| Scene | fps |
|---|---|
| Main menu | about 58 |
| Mars City 1 opening cutscene | about 51 |
| Mars City Hangar, in play | 30-31 |

The level loads in 110 s, 50 s of that images.

How to build and run it:
- `riscos/sdk.sh riscos/build.sh`
- `tools/romac-deploy`
- ro-run `launch 'Run IXFS::ROL.$.riscos.HardDisc4.Work.!dhewm3.!Run +set com_showFPS 1 +map game/demo_mars_city1'`

RISC OS changes so far (all `__riscos__`):
- `platform.h` and `Lib.cpp` treat RISC OS as Unix.
- UnixLib's ctype macros are dropped for C++ (`riscos/compat/riscos_compat.h`, force-included).
- No `getifaddrs`, no `SA_RESETHAND`, no `dlopen`.
- A microsecond clock from OS_ReadMonotonicTime plus HAL counter 0.
- The heap in a 1 GB dynamic area, a 4 MB stack, no file-name suffix swapping.
- On the desktop's quit message, the engine shuts down at once (SDL's driver would otherwise
  `exit()` at the next event pump and trip an assertion in static destructors).

Next on RoMac:
- **Sound:** OpenAL finds only "No Output".
- **Detected memory:** the system reports "0 MB", so the engine picks Low quality.
- **Speed:** profile with Runway's slow-path and Tarmac counters, then the player.
- **Settings and screen size.**
- **Mouse look in the window.**
- **App polish:** icon, Info, Help.

## Milestones

Each milestone ends with something that runs and has been measured. Commits stay small, and
numbers go in the README as the ioquake3 port does.

### M0: Prove the hard parts (before porting 600k lines)
1. **Toolchain:** find or build the devkit with !MesaLibs 20.3.5-22 or later, RISC3D client
   0.11 and libMesaGPU. Build the ioquake3 Docker image on it.
2. **A GL smoke test,** `d3gltest`: about 300 lines of SDL2 C. It draws with ARB vertex and
   fragment programs like Doom 3's interaction shader, two-sided stencil shadow volumes, depth
   bounds, cube maps and S3TC, and checks the pixels. Static link with libMesaGPU. Run it on:
   - RoMac (risc3d build);
   - ROL on a Pi 4 or Pi 5 (Linux3D);
   - `pi4-plato` native (V3DRender);
   - and once with `RISC3D$Disable` set, for the software reference.
   Any failure here goes to Andy, or to the RoMac player, before anything else.
3. **Memory:** on each platform, see how big a heap/DA can be made, and how much the GPU path
   can take.
4. **Test harness:** decide how agents drive RoMac and ROL. Either a Rorth target inside RoMac,
   or Runway's `RUNWAY_EVENTS`/`RUNWAY_SHOTDIR` headless runs (see Questions).

### M1: Reference builds on the host
- dhewm3 natively on the Mac, plus the demo data: `timedemo demo1`, reference screenshots and
  fps. (Game windows open only under a dummy video driver, or with David's OK.)
- An armhf Linux build with `HARDLINK_GAME=ON` in the alignment-trapping QEMU. It must load the
  demo map and run a timedemo with no unaligned faults. Record peak heap.

### M2: Cross-compile for RISC OS
- Add `riscos/` to the repo (copied from riscos-quake3) and dhewm3 as a pinned subtree or
  submodule. Every change goes under `__riscos__` or `if(RISCOS)`, upstream-first style.
- `HARDLINK_GAME=ON`, base first; no curl; ImGui on if it builds (it is useful for settings
  menus).
- Platform layer:
  - a `__riscos__` section in `sys/platform.h`;
  - timing from the ioquake3 port;
  - CPU identification from the ID register;
  - `fs_basepath` = `<dhewm3$Dir>`, saves in `<Choices$Write>.dhewm3`;
  - no backtrace or `dlopen`;
  - a passive console.
- Hard-wire the cvars: drop the per-frame `glGetError` calls, `r_useVertexBuffers 1`, and try
  `r_useIndexBuffers 1` (RISC3D reads back client-memory indices only when the arrays are in a
  buffer and the indices aren't).

### M3: First light on RoMac (the main target)
- Console, then the main menu, then the demo's first map, at 800x600 in a window. Then
  `timedemo` against M1's numbers.
- Profile both sides: Tarmac (code cache, slow paths) and the player. The player's figures are
  CPU per frame on the Mac and waits per frame.
- Sound through OpenAL, keyboard, mouse (from the ioquake3 port), saving and loading.

### M4: RISC OS on Linux
- The same `!RunImage` on ROL on a Pi 4/5 (and the Dev Kit if Chris Gransden is willing).
- Check zero-copy frames through rol-fb's display planes in 16M-colour modes.

### M5: Native Pi 4
- The same binary on `pi4-plato` through V3DRender. Measure with `V3DM$Stats 1` and the
  sampling profiler.
- Tune: render size against overlay scaling (the ioquake3 port's `r_renderWidth`/
  `r_renderHeight` approach), `image_downSize`, shadows and specular.
- Find the playable settings. Pi 3 and Pi 2 have no RISC3D driver, so the app tells the user
  instead of running.

### M6: The app and a release
- `!dhewm3` following `app-conventions.md`:
  - icon bar icon with Info, Help and Quit;
  - window or full screen (to be decided);
  - Shift+F12 back to the desktop;
  - Insert for screenshots, Shift+F8 for fps;
  - `RMEnsure RISC3D 0.05`, MesaLibs and SharedLibs checks;
  - a first-run screen size chosen from a speed test.
- Data install: the demo, or retail `base/*.pk4` from Steam or GOG, dragged onto the icon and
  checked by CRC (VanillaConquer `vcprep` style). id's data is never bundled.
- d3xp as a second build or a second `!RunImage`.
- Release zip with roszip, SemVer and tag `riscos-v<ver>`. Nothing is published without
  David's go-ahead.

## Not in scope (for now)
- Software rendering as a supported way to play.
- Pi 3 and earlier, and the Risc PC.
- RoWin. It should follow from RoMac.
- Multiplayer beyond a LAN check.
- Changing RISC3D or romac-3d ourselves. Problems found there go to Andy, or to RoMac's own
  session, with the smoke test as evidence.

## Questions for David (answered 2026-11-22)
1. Harness: a Rorth target inside RoMac (done: `romac-doom3`). The ROL machine is still to be
   chosen.
2. Data: demo first; the retail Doom 3 and RoE are on Steam on ultramagnus, when needed.
3. A desktop window by default.
4. RISC3D bugs are queued in `docs/risc3d-issues.md` for David to pass to Andy.
