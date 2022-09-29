#!smake
#
# Makefile for libsatmp.
#
# $Revision: 1.1 $

BASELIB = libsatmp_32.a
ALTLIBS = libsatmp_n32_M3.a libsatmp_64.a

DSOEXPORTS = common.exports
DSOEXPORTS_N32 = common.exports
DSOEXPORTS_64 = common.exports

include $(ROOT)/usr/include/make/librootdefs

DSONAMEOPT	= -soname libsatmp.so
LLDLIBS		= -lc

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 -src libsatmp_32.so internal/libsatmp.so
	$(INST_N32) -m 444 -src libsatmp_n32_M3.so internal/libsatmp.so
	$(INST_64) -m 444 -src libsatmp_64.so internal/libsatmp.so
