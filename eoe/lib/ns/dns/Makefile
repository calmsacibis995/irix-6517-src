#!smake

BASELIB = libns_dns_n32.a
include $(ROOT)/usr/include/make/librootdefs

CFILES= init.c lookup.c
LCOPTS= -fullwarn
LCDEFS= -DNDEBUG
LLDLIBS= -lc

default all: $(TARGETS)

include $(ROOT)/usr/include/make/librootrules

install: default
	${INSTALL} -u root -g sys -m 644 -F /var/ns/lib -src libns_dns_n32.so \
	    libns_dns.so
	${INSTALL} -F /var/ns/lib -lns libns_dns.so libns_hesiod.so
	${INSTALL} -F /var/ns/lib -lns libns_dns.so libns_named.so
