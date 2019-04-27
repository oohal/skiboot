# -*-Makefile-*-

SUBDIRS += asm
ASM_OBJS = misc.o kernel-wrapper.o cvc_entry.o
ASM=asm/built-in.a

$(ASM): $(ASM_OBJS:%=asm/%)

