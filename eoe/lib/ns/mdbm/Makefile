#!smake

BASELIB = libns_mdbm_n32.a
include $(ROOT)/usr/include/make/librootdefs

CFILES= lookup.c
LCOPTS= -fullwarn
LCDEFS= -DNDEBUG
LLDLIBS= -lc

default all: $(TARGETS)

include $(ROOT)/usr/include/make/librootrules

install: default
	${INSTALL} -u root -g sys -m 644 -F /var/ns/lib \
	    -src libns_mdbm_n32.so libns_mdbm.so
	${INSTALL} -F /var/ns/lib -lns libns_mdbm.so libns_hash.so
