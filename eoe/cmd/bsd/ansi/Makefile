#!smake

include $(ROOT)/usr/include/make/commondefs

CFILES = ansitape.c vmsprep.c
TARGETS = ansitape vmsprep
OPTIMIZER = -O2

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bsd $(TARGETS)
