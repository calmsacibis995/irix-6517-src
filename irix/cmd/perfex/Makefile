#!smake
# Makefile for perfex
#

OBJECT_STYLE=N32_M3
TARGETS=perfex
include $(ROOT)/usr/include/make/commondefs

CFILES = pcounts.c perfex.c perfy.c

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin -m 755 -idb "mach(CPUARCH=R10000)" perfex

perfex: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
