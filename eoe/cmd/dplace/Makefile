#!smake
# Makefile for dplace
#

OBJECT_STYLE=32_M2
TARGETS=dplace
include $(ROOT)/usr/include/make/commondefs

CFILES = dplace.c

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin -m 755 dplace

dplace: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
