#!smake
#ident "$Revision: 1.24 $"

BASELIB=libxnet.a
ALTLIBS=libxnet_32_abi.a libxnet_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

ABISO=yes-please

include $(ROOT)/usr/include/make/librootdefs

LLDLIBS=-lc

LLDDSOOPTS=-init _initializer
DSONAMEOPT=-soname libxnet.so
FRIEND_HEADERS = libnsl_synonyms.h
FILEPERM= -m 0644 -u root -g sys
LDIRT=so_locations

# turn off 'symbol preempts version in  libc'
LLDDSOOPTS+=-woff 85,134

# Configuration files
SUGGEST = -idb "config(suggest)"

default: $(TARGETS)

include ${LIBROOTRULES}

install exports: default
	$(INST_32) -m 444 libxnet.a
	$(INST_32) -m 444 libxnet.so
	$(INST_32_ABI) -m 444 -src libxnet_32_abi.so libxnet.so
	$(INST_N32) -m 444 -src libxnet_n32_M3.so libxnet.so

headers:
	${INSTALL} -F /usr/include -idb "noship" -m 444 ${FRIEND_HEADERS}
