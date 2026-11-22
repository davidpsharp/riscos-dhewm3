#!/bin/sh
# Makes build/riscos/app/!dhewm3 from build/riscos/dhewm3, in the doom3-riscos image:
#   riscos/sdk.sh riscos/package.sh
# The unstripped ELF stays at build/riscos/dhewm3 for ro-run dump --elf.
set -e
cd /work
A=build/riscos/app/!dhewm3
rm -rf "$A"
mkdir -p "$A"
cp -r riscos/app/!dhewm3/. "$A"/
arm-riscos-gnueabihf-strip -o build/riscos/dhewm3.stripped build/riscos/dhewm3
elf2aif -e build/riscos/dhewm3.stripped "$A/!RunImage,ff8" >/dev/null
rm build/riscos/dhewm3.stripped
cp "$RISCOS_DEVKIT/riscos/PThrTicker,ffa" "$A/"
ls -l "$A"
