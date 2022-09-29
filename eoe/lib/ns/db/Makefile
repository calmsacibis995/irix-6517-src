#!smake

BASELIB = libns_db_n32.a
include $(ROOT)/usr/include/make/librootdefs


CFILES= lookup.c
LCOPTS= -fullwarn
LCDEFS= -DNDEBUG 

# leave -lc before -ldb!   This stop use of ldb's ndbm overrides
LLDLIBS= -lc -ldb
LLDOPTS= -woff 85


default all: $(TARGETS)

include $(ROOT)/usr/include/make/librootrules

install: default
	${INSTALL} -u root -g sys -m 644 -F /var/ns/lib \
	    -src libns_db_n32.so libns_db.so
