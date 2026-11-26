# Full game on a Pi 4: GPU buffer memory (26 Nov 2026)

**Result:** the full game (Doom 3 retail data, pak000 to pak008) plays on a Raspberry Pi 4 from
version 0.2.2. Before that it loaded its first level and crashed with SIGSEGV before the first
frame. There were two causes, both in how much of V3DRender's buffer area the game needs:

1. **The area is too small.** V3DRender's default buffer area is 256 MB; the full game holds about
   810-860 MB of GPU buffers (about 4,900 of them) with its full-size textures. The demo needs
   about 125 MB. This is real memory, not a leak: it stayed flat for 14 minutes in play, and no
   buffer was held by a job.
2. **The area fragments.** Even in a 1 GB area, buffers were refused with 230 MB free, because
   V3DRender aligns each buffer of 16 pages or more to 16 pages, and of 256 pages or more to 256
   pages (for the MMU's 64 KB and 1 MB entries). Mesa asks for sizes like 1 MB + 4 KB
   (257 pages), so each such buffer left a 255-page gap, and a 17-page buffer a 15-page gap, that
   only smaller buffers could use. The diagnostic build found the largest free run was 15 pages
   at every refusal. In full screen (1280x1024) every frame wants a 5 MB buffer, which never fit.

A refused buffer reaches Mesa as a bad pointer ("Failed to allocate device memory for BO"), and
the game crashes (dumps D3220 to D3222: &FDFDFDFD in `mgpu_v3d_bo_map`, or a copy to address 0).

## What 0.2.2 does (the launcher, `riscos/launcher/launcher.cpp`)

When the full game is started on a machine with V3DRender loaded (a native Pi 4):
- It checks there's enough free memory (700 MB; 960 MB with full-size textures). If not, it
  says so and doesn't start.
- If V3DRender's area is under 1 GB, it restarts it with a 1 GB area (`RMKill RISC3D`,
  `RMKill V3DRender`, `Set V3DRender$Size 1024`, `RMLoad <RISC3D$Dir>.RISC3D`), but only if no
  other program has the GPU open (`*V3DRender`'s "Files open"); otherwise it asks the user to quit
  the other 3D program. It lasts until the next restart. 1 GB is the most RISC OS gives: a 2 GB
  `V3DRender$Size` got 1 GB.
- It puts smaller-texture settings in a marked block of the game's `autoexec.cfg`
  (`image_downSize`, `image_downSizeBump` and `image_downSizeSpecular`, limits 256, 256 and 128),
  which cuts the full game's GPU memory to about 450-490 MB. That leaves room for the
  fragmentation. `*Set dhewm3$Textures full` keeps full-size textures.
  (`image_downSize` alone barely helps: it only shrinks diffuse maps.)
- `Game` starts dhewm3 with `r_fullscreenDesktop 1`, so Alt+Return goes full screen in the
  desktop's own mode. A full-screen mode of the window's size (800x600) came up in 16 bpp, which
  garbles GL output (RISC3D-for-Andy item 4).

## Tested with

| Machine | RISC OS | RISC3D / V3DRender | Mesa (in the game) |
|---|---|---|---|
| pi4-plato, Raspberry Pi 4, 4 GB | 5.30 (15 May 2024) | RISC3D 0.22 (RISC3D module 0.11, V3DRender 0.37, 25 Nov 2026) | riscos-mesa devkit 20.3.5-22 |
| pi4-socrates, Raspberry Pi 4 | 5.30 | the same | the same (0.2.1 tests only) |
| Diagnostic and fix builds | 5.30 | V3DRender 0.33 (upstream tag v0.33) with patches | the same |

Older RISC3D releases weren't tested with the full game. RoMac (Linux3D, no V3DRender) runs the
full game without any of this.

## The runs (pi4-plato, clean boot each time)

| V3DRender, area | Textures | Test | Result |
|---|---|---|---|
| 0.37, 256 MB (default) | full | load Mars City 1 | 17,157 refused, 109 GPU resets, crash (pi4-socrates: 17,175) |
| 0.37, 256 MB | `image_downSize` only | load | 15,964 refused, crash |
| 0.37, 256 MB, `*V3DRender pool 64` | full | load | 17,157 refused, crash |
| 0.33 + diagnostics, 512 MB | full | load | 11,262 refused, crash; 457 MB live at refusal, largest free run 15 pages |
| 0.37, 1 GB | full | window, 14 min still | 807 MB, flat; 0 refused |
| 0.37, 1 GB | full | window, 5 levels in a row | all loaded; peaks 840-864 MB; **202 refused** during one load (the game survived) |
| 0.37, 1 GB | full | full screen 1280x1024 | 793 MB; refusals climbing ~40 a second (5 MB buffers); later crashed in play |
| **0.33 + alignment fix, 1 GB** | full | full screen 1280x1024 | **0 refused**, 812 MB, plays at about 6.4 fps |
| **0.37, 1 GB** | **smaller** | window, 5 levels in a row | **0 refused**, peaks about 490 MB |
| **0.37, 1 GB** | **smaller** | full screen 1280x1024, 2 levels | **0 refused**, peak 485 MB, 1 GPU reset while loading |
| **0.37, 256 MB at boot; 0.2.2 launcher** | smaller | icon, New Game, Mars City | launcher set up 1 GB; **0 refused**, 446 MB, about 10.5 fps |

## For Andy (RISC3D / V3DRender)

1. **Alignment fragments the area.** Patch (against v0.33, tested on a Pi 4):
   [`tests/v3drender/align-033.patch`](../tests/v3drender/align-033.patch). It aligns a buffer only
   when its size is a whole number of the alignment, so 2^n + 1 page buffers pack together. With
   it, Doom 3's full game in full screen had no refusals in a 1 GB area.
2. **256 MB is too small for big games, and only a restart changes it.** Could V3DRender grow the
   area on demand, or could !RISC3DSetup offer the size (or the default follow the RAM)?
3. A refused buffer still crashes the program instead of giving GL_OUT_OF_MEMORY (summary item 6).
4. GPU resets (bin jobs, ERR_STAT &1000) sometimes happen while a level loads in full screen.
5. `*V3DRender`'s lines end LF CR, which tripped a parser; and its "in use" figure read after a
   program has died is easy to mistake for the program's use.

## How to reproduce

- Reboot the Pi; `WimpMode X1280 Y1024 C16M` (plato's desktop comes back at 8 bpp).
- 0.2.1 behaviour: `Obey <dhewm3$Dir>.Game +map game/mars_city1` with base/ present; `*V3DRender`
  shows the refusals.
- To change the area without a reboot: `RMKill RISC3D`, `RMKill V3DRender`,
  `Set V3DRender$Size 1024`, `RMLoad <RISC3D$Dir>.RISC3D`.
- The diagnostic build: `tests/v3drender/diag-align-033.patch` on v0.33, built with
  `tests/v3drender/build-module.sh`; it logs the page map at each refusal into `*V3DRender`'s log.
