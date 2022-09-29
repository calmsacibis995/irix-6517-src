#!smake
#
# Makefile for IRIX crash UTIL library
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libutil/RCS/Makefile,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#
include $(ROOT)/usr/include/make/commondefs

GCDEFS    = -DICRASH_DEBUG -DI_TEMP
LCDEFS    = -I../../include -g -n32 -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\"
TARGETS   = ../../libutil.a
CFILES    = alloc.c error.c getstrings.c mload.c proc.c report.c socket.c \
			stream.c struct.c timer.c trace.c util.c

OFILES    = $(CFILES:.c=.o)

OSVERS   != export PATH || exit 1 ; echo $${OSVERSION} | \
			sed 'y/\./\_/' | cut -c1-3

COMMONPREF=libutil_

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	ar ccurl $(TARGETS) $(OFILES)
