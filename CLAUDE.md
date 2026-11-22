# riscos-doom3: RISC OS project

Shared RISC OS knowledge is in `~/code/riscos-kb` (read, not imported, to keep context small):
read `INDEX.md` first; `app-conventions.md` before designing any app; `testing.md` before running
on RISC OS machines/emulators (tool: Rorth, see `~/code/RORTH/docs/using-rorth.md`); and check
`reuse-catalogue.md` and `projects.md` for prior art before writing something new. The `riscos`
skill does the same. Add anything reusable you learn as a new file in `~/code/riscos-kb/lessons/`.

## This project
Port dhewm3 (Doom 3 GPL engine) to RISC OS 5, drawing through Andy Youll's RISC3D: primary
target RoMac (`~/code/rol-run`, branch `risc3d`), then RISC OS on Linux, then native Pi 4
(V3DRender, static link with libMesaGPU). Template: `~/code/riscos-quake3`. The plan, risks and
milestones are in `docs/PLAN.md`. Status: runs on RoMac in a window, demo data, about 30 fps in play (2026-11-22). Build: riscos/sdk.sh riscos/build.sh; deploy: tools/romac-deploy; test target: romac-doom3 (tools/romac-run start).
