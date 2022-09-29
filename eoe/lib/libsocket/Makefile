#!smake
#ident "$Revision: 1.15 $

BASELIB=libsocket_32.a
ALTLIBS=libsocket_32_abi.a libsocket_64.a libsocket_n32_M3.a libsocket_64_abi.a
ABISO=-yes-please

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include $(ROOT)/usr/include/make/librootdefs

DSONAMEOPT=-soname libsocket.so
FRIEND_HEADERS = libsocket_synonyms.h
LDIRT=so_locations

default: $(TARGETS)

include ${LIBROOTRULES}

install exports: default
	$(INST_32) -m 444 -src libsocket_32.so libsocket.so
	$(INST_32_ABI) -m 444 -src libsocket_32_abi.so libsocket.so
	$(INST_64) -m 444 -src libsocket_64.so libsocket.so
	$(INST_64_ABI) -m 444 -src libsocket_64_abi.so libsocket.so
	$(INST_N32) -m 444 -src libsocket_n32_M3.so libsocket.so

headers:
	${INSTALL} -F /usr/include -idb "noship" -m 444 ${FRIEND_HEADERS}
