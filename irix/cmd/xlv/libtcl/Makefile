#!smake
#
# Makefile for libtcl
#
#ident "$Revision: 1.2 $"

# N32 is the current default
BASELIB=libtcl7.3.a

MULTIDIR=yes
PRIVATEFILES=

include $(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

default install: $(TARGETS)

include $(LIBROOTRULES)

debug lint:	$(SETUPDIRS)
	$(SUBDIRS_MAKERULE) 
