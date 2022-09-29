#!smake
#
# Makefile for netsnoop
#
TARGETS=netsnoop
CFILES= netsnoop.c options.c
LCINCS=-I$(ROOT)/usr/include/netman
LLDLIBS=-lnetman

include	$(ROOT)/usr/include/make/commondefs


default: $(TARGETS)

$(TARGETS): $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)
	$(TAG) 0x610 $@;

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)
