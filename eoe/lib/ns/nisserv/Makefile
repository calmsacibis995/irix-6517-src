#!smake

BASELIB = libns_nisserv_n32.a
include $(ROOT)/usr/include/make/librootdefs

CFILES= init.c lookup.c sgi_util.c
LCOPTS= -fullwarn
LCDEFS= -DNDEBUG
LLDLIBS= -lc
LLDOPTS= -woff 33

default all: $(TARGETS)

include $(ROOT)/usr/include/make/librootrules

install: default
	${INSTALL} -u root -g sys -m 644 -F /var/ns/lib \
	    -src libns_nisserv_n32.so libns_nisserv.so
	${INSTALL} -F /var/ns/lib -lns libns_nisserv.so libns_ypserv.so
