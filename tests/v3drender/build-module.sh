#!/bin/bash
# Builds only V3DRender's module (the module part of its build.sh), in doom3-riscos.
set -euo pipefail
cd /work/build/v3drender/src
mkdir -p out/obj
CORE="core/v3d_hw.c core/v3d_sched.c core/v3d_bo.c core/v3d_ioctl.c core/v3d_2d.c"
INC="-Iinclude -Icore -Icore/compat"
T=/opt/riscos-crossdev/toolchain/env/bin/arm-riscos-gnueabihf-
MC="-std=gnu11 -O2 -marm -march=armv7-a -mno-unaligned-access -fpie -fvisibility=hidden \
 -mgeneral-regs-only -ffreestanding -fno-common -fno-stack-protector \
 -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-builtin \
 -nostdinc -isystem $(${T}gcc -print-file-name=include) -Wall -Wextra -Icommon -Iriscos $INC"
ML="-nostdlib -static -Wl,--no-dynamic-linker -Wl,-T,common/module.ld -Wl,--build-id=none -Wl,-z,norelro"
${T}as -I common -o out/obj/header.o riscos/header.s
objs="out/obj/header.o"
for c in common/cmodule.c riscos/module.c riscos/plat_pi4.c riscos/twod.c $CORE; do
  o=out/obj/$(basename "${c%.c}").o
  ${T}gcc $MC -c -o "$o" "$c"
  objs="$objs $o"
done
${T}gcc $MC $ML -o out/obj/V3DRender.elf $objs -lgcc
${T}gcc $MC $ML -Wl,--defsym=__link_base=0x10000 -o out/obj/V3DRender-64k.elf $objs -lgcc
${T}objcopy -O binary out/obj/V3DRender.elf out/obj/V3DRender-0.bin
${T}objcopy -O binary out/obj/V3DRender-64k.elf out/obj/V3DRender-1.bin
python3 -B common/check-pic.py out/obj/V3DRender-0.bin out/obj/V3DRender-1.bin 0x10000 \
  0x$(${T}nm out/obj/V3DRender.elf | awk '$3=="__got_start"{print $1}') \
  0x$(${T}nm out/obj/V3DRender.elf | awk '$3=="__got_end"{print $1}')
cp out/obj/V3DRender-0.bin "out/V3DRender,ffa"
ls -l "out/V3DRender,ffa"
