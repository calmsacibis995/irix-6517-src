#!smake

#ident	"$Revision: 1.2 $"

LMC=libmediaclient
BASELIB=$(LMC)_32.a
ALTLIBS=$(LMC)_64.a $(LMC)_n32.a

DSOEXPORTS=common.exports
DSOEXPORTS_32=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64=common.exports

include	$(ROOT)/usr/include/make/librootdefs
LLDLIBS=-lc

DSONAMEOPT=-soname libmediaclient.so

default: $(TARGETS)

include $(LIBROOTRULES)

install exports: default
	$(INST_32)  -m 444 -idb "nostrip" -src $(LMC)_32.so  $(LMC).so
	$(INST_64)  -m 444 -idb "nostrip" -src $(LMC)_64.so  $(LMC).so
	$(INST_N32) -m 444 -idb "nostrip" -src $(LMC)_n32.so $(LMC).so
