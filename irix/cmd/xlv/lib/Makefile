#!smake
#
# Makefile for libxlv
#
#ident "$Revision: 1.7 $"

# N32 is the current default
BASELIB=libxlv.a
# Build an older O32
ALTLIBS=libxlv_32_M2.a

#MULTIDIR=yes
PRIVATEFILES=

include $(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

default install: $(TARGETS)

include $(LIBROOTRULES)

debug lint:	$(SETUPDIRS)
	$(SUBDIRS_MAKERULE) 
