#!smake

BASELIB=libmdbm.a
ALTLIBS=libmdbm_n32.a libmdbm_64.a

include $(ROOT)/usr/include/make/librootdefs
TARGETS=$(BASELIB:.a=.so) $(ALTLIBS:.a=.so)
DSONAMEOPT=-soname libmdbm.so
DSOEXPORTS=common.exports
LLDOPTS=-woff 157

LDIRT=$(BASELIB) $(ALTLIBS) $(TARGETS:.a=.so)

default all: $(TARGETS)

exports install: default
	# o32 
	$(INST_32) -m 444 -idb "nostrip" libmdbm.so
	# n32
	$(INST_N32) -m 444 -idb "nostrip" -src libmdbm_n32.so libmdbm.so
	# n64
	$(INST_64) -m 444 -idb "nostrip" -src libmdbm_64.so libmdbm.so

include $(LIBROOTRULES)
