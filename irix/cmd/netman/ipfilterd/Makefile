#
# Makefile for ipfilterd(1M).
#
#ident "$Revision: 1.5 $"

DEPTH=..
include ${DEPTH}/Makedefs

LLDLIBS	= $(LIBRARY)
LCOPTS  = -fullwarn

CFILES	= ipfilterd.c
TARGETS	= ipfilterd

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

default: $(TARGETS)

include ${DEPTH}/Makerules

ipfilterd: ${LIBRARY}

install exports: default
	$(INSTALL) -F /usr/etc -idb "std.sw.ipgateway" ipfilterd
	$(INSTALL) -F /etc -idb "std.sw.ipgateway config(update)" \
		-m 600 ipfilterd.conf
