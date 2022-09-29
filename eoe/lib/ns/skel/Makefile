#!smake

BASELIB = libns_skel_n32.a
OBJECT_STYLE=N32_M3
include $(ROOT)/usr/include/make/librootdefs

CFILES= test.c
LCOPTS= -fullwarn
# LCDEFS= -DDEBUG
LLDLIBS= -lc

OPTIMIZER= -g

default all: $(TARGETS)

install: default
	$(INSTALL) -m 444 -F /var/ns/lib -src libns_skel_n32.so libns_skel.so

include $(ROOT)/usr/include/make/librootrules
