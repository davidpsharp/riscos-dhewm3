#!/bin/sh
# RISC OS port, copyright David Sharp 2026. GPL-3.0 (see dhewm3/COPYING.txt).
# Builds dhewm3 for RISC OS in the doom3-riscos Docker image:
#   riscos/sdk.sh riscos/build.sh [build type] [extra cmake args]
# Output: build/riscos/dhewm3 (ELF with symbols).
set -e
cd /work
TYPE=${1:-RelWithDebInfo}
[ $# -gt 0 ] && shift
B=${B:-build/riscos}
if [ ! -f $B/build.ninja ]; then
  cmake -S dhewm3/neo -B $B -G Ninja -DCMAKE_BUILD_TYPE=$TYPE \
    -DCMAKE_TOOLCHAIN_FILE=/work/riscos/toolchain.cmake \
    -DHARDLINK_GAME=ON -DBASE=ON -DD3XP=OFF -DDEDICATED=OFF -DTOOLS=OFF \
    -DSDL2=ON -DIMGUI=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF "$@"
fi
ninja -C $B
