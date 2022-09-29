#!smake
#
# Makefile for libnanothread.
#
#ident	"$Revision: 1.3 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

VERSION_LDOPTS=-call_shared
LDIRT=ugenassym

include $(LIBLEAFRULES)

ugenassym: ugenassym.c
	${CC} ${CFLAGS} ugenassym.c -o ugenassym
