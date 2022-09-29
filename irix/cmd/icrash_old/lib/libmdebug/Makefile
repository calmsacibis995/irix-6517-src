#!smake
#
# Makefile for IRIX crash MDEBUG library
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libmdebug/RCS/Makefile,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#
include $(ROOT)/usr/include/make/commondefs

GCDEFS    = -DICRASH_DEBUG -DI_TEMP
LCDEFS    = -I../../include -g -n32 -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\"
TARGETS   = ../../libmdebug.a
CFILES    = md_fix.c md_init.c md_is.c md_lkup.c md_print.c \
			md_store.c md_type.c md_whatis.c
OFILES    = $(CFILES:.c=.o)

OSVERS   != export PATH || exit 1 ; echo $${OSVERSION} | \
			sed 'y/\./\_/' | cut -c1-3

COMMONPREF=libmdebug_

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	ar ccurl $(TARGETS) $(OFILES)

libmdebug.a: $(OFILES)
