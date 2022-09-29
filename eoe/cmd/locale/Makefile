#!smake
#
#
# Makefile for locale.
#
#ident  "$Revision: 1.1 $"


include $(ROOT)/usr/include/make/commondefs

OPTIMIZER = -g

TARGETS	  = locale

CFILES    = locale

targets: $(TARGETS)

include $(COMMONRULES)

all default: $(TARGETS)

install: all default
	$(INSTALL) -F /usr/bin $(TARGETS)

#install: all default

headers exports:
