#
# Makefile for convert, ripped off from MIPS' SPP and SGI-ized.
#
include ${ROOT}/usr/include/make/commondefs
MKDEPFILE= Makedepend

LCOPTS= -p0 -O0
LDFLAGS= -non_shared -coff -nostdlib -T 80800000 -N -G 0

CVERSION= -cckr
LCOPTS= -non_shared -coff -DIP19 -DEVEREST -DPROM -D_STANDALONE
ASFLAGS= -non_shared -coff -Wa,-mips3 -32bit -Wa,-r4000
OPTIMIZER = -g

CFILES= eawrite.c
ASFILES= libasm.s

TARGETS= eawrite

default: ${TARGETS}

include ${COMMONRULES}

eawrite: ${OBJECTS}
	${LD} ${LDFLAGS} ${OBJECTS} -o ${TARGETS} -e main

