#!smake

BASELIB = libns_ldap_n32.a
include $(ROOT)/usr/include/make/librootdefs

CFILES= init.c
LCOPTS= -fullwarn
# LCDEFS= -DDEBUG
LLDLIBS= -lc

OPTIMIZER= -g

default all: $(TARGETS)

include $(ROOT)/usr/include/make/librootrules

install: default
	${INSTALL} -u root -g sys -m 644 -F /var/ns/lib -src libns_ldap_n32.so \
	    libns_ldap.so
	${INSTALL} -F /var/ns/lib -lns libns_dns.so libns_x500.so
