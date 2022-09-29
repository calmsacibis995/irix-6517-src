#!smake
#
# Makefile for LIBKLIB
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libklib/RCS/Makefile,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#
include $(ROOT)/usr/include/make/commondefs

GCDEFS    = -DICRASH_DEBUG -DI_TEMP
LCDEFS    = -I. -I../../include -g -n32 -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\"
TARGETS   = ../../libklib.a
CFILES    = klib.c klib_cmp.c klib_error.c klib_hwgraph.c klib_kthread.c \
			klib_mem.c klib_page.c klib_proc.c klib_struct.c klib_util.c

OFILES    = $(CFILES:.c=.o)

OSVERS   != export PATH || exit 1 ; echo $${OSVERSION} | \
			sed 'y/\./\_/' | cut -c1-3

COMMONPREF=libklib_

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	ar ccurl $(TARGETS) $(OFILES)
