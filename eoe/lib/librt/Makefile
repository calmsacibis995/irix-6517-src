#!smake
#ident "$Revision: 1.1 $

BASELIB=librt_32.a
ALTLIBS=librt_64.a librt_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include $(ROOT)/usr/include/make/librootdefs

DSONAMEOPT=-soname librt.so
LDIRT=so_locations

default: $(TARGETS)

include ${LIBROOTRULES}

install exports: default
	$(INST_32) -m 444 -src librt_32.so librt.so
	$(INST_64) -m 444 -src librt_64.so librt.so
	$(INST_N32) -m 444 -src librt_n32_M3.so librt.so

headers:
	${INSTALL} -F /usr/include -idb "noship" -m 444 ${FRIEND_HEADERS}
