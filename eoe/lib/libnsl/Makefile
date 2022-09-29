#!smake
#ident "$Revision: 1.22 $"

BASELIB=libnsl.a
ALTLIBS=libnsl_32_abi.a libnsl_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_N32=common.exports

ABISO=yes-please

include $(ROOT)/usr/include/make/librootdefs

LLDLIBS=-lc

LLDDSOOPTS=-init _initializer
DSONAMEOPT=-soname libnsl.so
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
	$(INST_32) -m 444 libnsl.a
	$(INST_32) -m 444 libnsl.so
	$(INST_32_ABI) -m 444 -src libnsl_32_abi.so libnsl.so
	$(INST_N32) -m 444 -src libnsl_n32_M3.so libnsl.so
	$(INSTALL) -F /etc $(FILEPERM) $(SUGGEST) netconfig
	$(INSTALL) -F /etc/net/ticlts    $(FILEPERM) $(SUGGEST) services
	$(INSTALL) -F /etc/net/ticots    $(FILEPERM) $(SUGGEST) services
	$(INSTALL) -F /etc/net/ticotsord $(FILEPERM) $(SUGGEST) services

headers:
	${INSTALL} -F /usr/include -idb "noship" -m 444 ${FRIEND_HEADERS}

