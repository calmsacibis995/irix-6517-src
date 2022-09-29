#!smake
# Makefile for dlook
#

OBJECT_STYLE=32_M2
TARGETS=dlook
include $(ROOT)/usr/include/make/commondefs

CFILES = dlook.c

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin -m 755 dlook

dprof: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
