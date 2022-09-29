#!smake

BASELIB = libns_nis_n32.a
include $(ROOT)/usr/include/make/librootdefs

CFILES= bind.c binder.c dstring.c htree.c init.c list.c lookup.c netgroup.c
LCOPTS= -fullwarn
LCDEFS= -DNDEBUG
LLDLIBS= -lc
LLDOPTS= -woff 33

default all: $(TARGETS)

include $(ROOT)/usr/include/make/librootrules

install: default
	${INSTALL} -u root -g sys -m 644 -F /var/ns/lib \
	    -src libns_nis_n32.so libns_nis.so
	${INSTALL} -F /var/ns/lib -lns libns_nis.so libns_yp.so
