# CMake toolchain for dhewm3 on RISC OS 5 (ARMv7 + NEON: RoMac, RISC OS on Linux, Pi 2 and
# later): riscos-crossdev's toolchain file plus the riscos-mesa devkit's SDL2 and OpenAL.
include($ENV{RCD_DIR}/cmake/riscos.cmake)

# RISC OS traps unaligned accesses; the compiler never generates them by default.
set(CMAKE_C_FLAGS_INIT "-mfpu=neon -mfloat-abi=hard -mtune=cortex-a72 -fstack-clash-protection")
set(CMAKE_CXX_FLAGS_INIT "-mfpu=neon -mfloat-abi=hard -mtune=cortex-a72 -fstack-clash-protection -include ${CMAKE_CURRENT_LIST_DIR}/compat/riscos_compat.h")

set(_devkit $ENV{RISCOS_DEVKIT})
# SDL 2 from the devkit, static. Its GL windows go through EGL to RISC3D where RISC3D has a
# driver (Linux3D under RoMac and RISC OS on Linux), else OSMesa in software. libMesaGPU is
# the Pi 4's GPU driver (V3DRender); it's only used where RISC3D has loaded V3DRender.
# riscos/sdl2/build.sh's SDL (riscos-mesa's, plus the ioquake3 port's fixes: icon bar menu
# with Info and Help, RoMac's absolute pointer for mouse look) when it's built, else the devkit's.
get_filename_component(_sdl_stage ${CMAKE_CURRENT_LIST_DIR}/../build/riscos-sdl2/stage ABSOLUTE)
if(EXISTS ${_sdl_stage}/lib/libSDL2.a)
  set(_sdl_lib ${_sdl_stage}/lib/libSDL2.a)
  set(_sdl_inc ${_sdl_stage}/include/SDL2)
else()
  set(_sdl_lib ${_devkit}/lib/libSDL2.a)
  set(_sdl_inc ${_devkit}/include/SDL2)
endif()
set(SDL2_INCLUDE_DIR ${_sdl_inc} CACHE PATH "")
set(SDL2_LIBRARY "${_sdl_lib};${_devkit}/lib/libEGL.a;${_devkit}/lib/libOSMesa.a;-Wl,-u,riscos_mesa_gpu;${_devkit}/lib/libMesaGPU.a;stdc++;${_devkit}/lib/libz.a;m" CACHE STRING "")
set(OPENAL_INCLUDE_DIR ${_devkit}/include CACHE PATH "")
set(OPENAL_LIBRARY ${_devkit}/lib/libopenal.a CACHE FILEPATH "")
