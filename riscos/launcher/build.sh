#!/bin/sh
# Builds the launcher (!dhewm3.!RunImage) in the doom3-riscos image:
#   riscos/sdk.sh riscos/launcher/build.sh
# Output: build/launcher/launcher,ff8 (AIF) and launcher.elf (with symbols).
set -e
cd /work
O=build/launcher
mkdir -p $O
V=$(cat VERSION 2>/dev/null || echo 0.1.0)
arm-riscos-gnueabihf-g++ -O2 -g -std=gnu++11 -mfpu=vfpv3 -mfloat-abi=hard -fstack-clash-protection -Wall \
  -DLAUNCHER_VERSION="\"$V\"" -I$RISCOS_DEVKIT/include -static -o $O/launcher.elf \
  riscos/launcher/launcher.cpp riscos/launcher/sha256.cpp -L$RISCOS_DEVKIT/lib -lz
arm-riscos-gnueabihf-strip -o $O/launcher.stripped $O/launcher.elf
elf2aif -e $O/launcher.stripped $O/launcher,ff8 >/dev/null
rm $O/launcher.stripped
ls -l $O/launcher,ff8
