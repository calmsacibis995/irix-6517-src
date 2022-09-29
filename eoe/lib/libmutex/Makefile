#!smake
#
# Makefile for libmutex.
#
#ident	"$Revision: 1.8 $"

BASELIB=libmutex_32.a
ALTLIBS=libmutex_32_abi.a libmutex_64.a libmutex_n32_M3.a libmutex_64_abi.a

DSOEXPORTS=common.exports
DSOEXPORTS_32=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64=common.exports

ABISO=yes-please
LDIRT=so_locations

include	$(ROOT)/usr/include/make/librootdefs

DSONAMEOPT=-soname libmutex.so

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 -src libmutex_32.so libmutex.so
	$(INST_32_ABI) -m 444 -src libmutex_32_abi.so libmutex.so
	$(INST_64_ABI) -m 444 -src libmutex_64_abi.so libmutex.so
	$(INST_64) -m 444 -src libmutex_64.so libmutex.so
	$(INST_N32) -m 444 -src libmutex_n32_M3.so libmutex.so






