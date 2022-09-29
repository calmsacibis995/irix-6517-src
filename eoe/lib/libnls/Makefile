#!smake
#
# Makefile for libnls.
#
#ident	"$Revision: 1.3 $"

BASELIB=libnls.a

include	$(ROOT)/usr/include/make/librootdefs

LLDLIBS=-lnsl -lc

CFILES=nlsdata.c nlsenv.c nlsrequest.c

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib -m 444 $(LIBS)

include $(LIBROOTRULES)
