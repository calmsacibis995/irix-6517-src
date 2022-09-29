#!smake
# Makefile for dprof
#

OBJECT_STYLE=32_M2
TARGETS=dprof
include $(ROOT)/usr/include/make/commondefs

CFILES = dprof.c

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin -m 755 dprof

dprof: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
