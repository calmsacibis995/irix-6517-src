#!smake
#ident "$Revision: 1.6 $

BASELIB=libabi_32.a
ALTLIBS=libabi_32_abi.a libabi_64.a libabi_n32_M3.a libabi_64_abi.a

SOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

ABISO=-yes-please

include $(ROOT)/usr/include/make/librootdefs

DSONAMEOPT=-soname libabi.so
LDIRT=so_locations

default: $(TARGETS)

include ${LIBROOTRULES}

install exports: default
	$(INST_32) -m 444 -src libabi_32.so libabi.so
	$(INST_32_ABI) -m 444 -src libabi_32_abi.so libabi.so
	$(INST_64) -m 444 -src libabi_64.so libabi.so
	$(INST_64_ABI) -m 444 -src libabi_64_abi.so libabi.so
	$(INST_N32) -m 444 -src libabi_n32_M3.so libabi.so
