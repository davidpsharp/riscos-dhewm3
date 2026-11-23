#!/bin/sh
# RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
# dhewm3 against the riscos-mesa devkit's own libSDL2.a (no riscos/sdl2 patches), in build/riscos-stocksdl.
set -e
# Run: riscos/sdk.sh tests/sdl/build-stocksdl.sh
cd /work
D=$RISCOS_DEVKIT
B=build/riscos-stocksdl riscos/build.sh RelWithDebInfo -DSDL2_INCLUDE_DIR=$D/include/SDL2 \
  "-DSDL2_LIBRARY=$D/lib/libSDL2.a;$D/lib/libEGL.a;$D/lib/libOSMesa.a;-Wl,-u,riscos_mesa_gpu;$D/lib/libMesaGPU.a;stdc++;$D/lib/libz.a;m"
grep -o 'SDL2_LIBRARY:STRING=[^;]*' build/riscos-stocksdl/CMakeCache.txt
strings $D/lib/libSDL2.a | grep -c 'Icon bar menu\|riscos_menu' || true
