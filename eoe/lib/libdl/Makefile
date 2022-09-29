#!smake
#
# Makefile for libdl.
#
#ident	"$Revision: 1.8 $

BASELIB=libdl.a
ALTLIBS=libdl_32_abi.a libdl_n32_M3.a libdl_64_abi.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

ABISO=-yes-please

include	$(ROOT)/usr/include/make/librootdefs

DSONAMEOPT=-soname libdl.so
LDIRT=so_locations

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INSTALL) -F /usr/lib -m 444 libdl.so
	$(INSTALL) -F $(INSTDIR_32_ABI) -m 444 -src libdl_32_abi.so libdl.so
	$(INSTALL) -F $(INSTDIR_64_ABI) -m 444 -src libdl_64_abi.so libdl.so
	$(INSTALL) -F $(INSTDIR_N32) -m 444 -src libdl_n32_M3.so libdl.so
