#!/bin/sh
# RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
# Builds d3gltest for RISC OS (static, with the Pi 4 GPU library):
#   riscos/sdk.sh tests/d3gltest/build.sh
# Output: build/d3gltest/d3gltest,ff8 (AIF) and d3gltest.elf (with symbols).
set -e
cd /work
O=build/d3gltest
mkdir -p $O
D=$RISCOS_DEVKIT
arm-riscos-gnueabihf-gcc -O2 -g -mfpu=vfpv3 -mfloat-abi=hard -fstack-clash-protection -Wall \
  -I$D/include -I$D/include/SDL2 -static -o $O/d3gltest.elf tests/d3gltest/d3gltest.c \
  -L$D/lib -lSDL2 -lEGL -lOSMesa -Wl,-u,riscos_mesa_gpu -lMesaGPU -lstdc++ -lz -lm
arm-riscos-gnueabihf-strip -o $O/d3gltest.stripped $O/d3gltest.elf
elf2aif -e $O/d3gltest.stripped $O/d3gltest,ff8 >/dev/null
rm $O/d3gltest.stripped
ls -l $O
