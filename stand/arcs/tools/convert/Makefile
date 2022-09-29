# Makefile for convert, ripped off from MIPS' SPP and SGI-ized.

include ${ROOT}/usr/include/make/commondefs

DEF_CSTYLE= ${CSTYLE_32_M2}
DEF_GLDOPTS= ${GLDOPTS_32_M2}

MKDEPFILE= Makedepend

LLDLIBS=-lmld -lelf

HFILES= convert.h	\
	mips.h		\
	stubs.h		\
	intel_hex.h	\
	s_rec.h		\
	binary.h	\
	pure.h		\
	elf.h		\
	racer.h

CFILES= convert.c	\
	mips.c 		\
	stubs.c		\
	intel_hex.c	\
	s_rec.c		\
	binary.c	\
	pure.c		\
	elf.c		\
	racer.c

TARGETS= convert

default: ${TARGETS}

include ${COMMONRULES}

convert: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o ${TARGETS}

install: default
# don't actually do anything on install
