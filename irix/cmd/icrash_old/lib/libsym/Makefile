#!smake
#
# Makefile for IRIX crash SYMBOL library
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libsym/RCS/Makefile,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#
include $(ROOT)/usr/include/make/commondefs

GCDEFS    = -DICRASH_DEBUG -DI_TEMP
LCDEFS    = -I../../include -g -n32 -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\"
TARGETS   = ../../libsym.a
CFILES    = dwarflib.c dwarfprint.c elflib.c symlib.c

OFILES    = $(CFILES:.c=.o)

OSVERS   != export PATH || exit 1 ; echo $${OSVERSION} | \
			sed 'y/\./\_/' | cut -c1-3

COMMONPREF=libsym_

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	ar ccurl $(TARGETS) $(OFILES)
