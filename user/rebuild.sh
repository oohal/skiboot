#!/bin/bash -xe

CROSS=/home/oliver/code/buildroot/output/host/bin/powerpc64-linux-

# lol:
gcc -E skiboot.lds.S | sed 's/^#.*//' > skiboot.lds

${CROSS}gcc -c user.c
${CROSS}gcc -c shim.c

LDFLAGS_FINAL="-EB -m elf64ppc --build-id=none --whole-archive"
LDFLAGS_FINAL="$LDFLAGS_FINAL -nostdlib -pie --oformat=elf64-powerpc --orphan-handling=warn"

make -C ../ skiboot.tmp.a CROSS=${CROSS} -j9


#${CROSS}ld $LDFLAGS_FINAL -T skiboot.lds shim.o ../skiboot.tmp.a -o tmp.o
${CROSS}ld -T skiboot.lds shim.o ../skiboot.tmp.a -r -o tmp.o
${CROSS}gcc user.c -lc tmp.o -o user

