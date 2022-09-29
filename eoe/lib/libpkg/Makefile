#!smake
#
# Makefile for libpkg.
#
#ident	"$Revision: 1.8 $"

BASELIB=libpkg_32.a
ALTLIBS=libpkg_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include	$(ROOT)/usr/include/make/librootdefs

LLDLIBS=-ladm -lc

DSONAMEOPT=-soname libpkg.so

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 -src libpkg_32.so libpkg.so
	$(INST_32) -m 444 -src libpkg_32.a libpkg.a
	$(INST_N32) -m 444 -src libpkg_n32_M3.so libpkg.so
	$(INST_N32) -m 444 -src libpkg_n32_M3.a libpkg.a
