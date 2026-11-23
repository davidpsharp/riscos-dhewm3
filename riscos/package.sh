#!/bin/sh
# RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
# Makes build/riscos/app/!dhewm3 from build/riscos/dhewm3 and the launcher, in the doom3-riscos
# image:
#   riscos/sdk.sh riscos/package.sh
# !RunImage is the launcher (icon bar, demo download); dhewm3 is the game, started by Game.
# The unstripped ELFs stay in build/ for ro-run dump --elf.
set -e
cd /work
riscos/launcher/build.sh >/dev/null
A=build/riscos/app/!dhewm3
rm -rf "$A"
mkdir -p "$A"
cp -r riscos/app/!dhewm3/. "$A"/
arm-riscos-gnueabihf-strip -o build/riscos/dhewm3.stripped build/riscos/dhewm3
elf2aif -e build/riscos/dhewm3.stripped "$A/dhewm3,ff8" >/dev/null
rm build/riscos/dhewm3.stripped
cp build/launcher/launcher,ff8 "$A/!RunImage,ff8"
cp "$RISCOS_DEVKIT/riscos/PThrTicker,ffa" "$A/"
# riscos-mesa's DAClamp (lifts RISC OS 5's 128 MB cap on dynamic areas; Game runs it)
cp "$RISCOS_DEVKIT/riscos/!MesaLibs/DAClamp,ffc" "$A/"
ls -l "$A"
