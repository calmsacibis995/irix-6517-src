#!smake
#
# Makefile for IRIX crash
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/libfru/RCS/Makefile,v 1.1 1999/05/25 19:19:20 tjm Exp $"
#
include $(ROOT)/usr/include/make/commondefs

# FRU analyzer version
LCDEFS    += -I../../include -g -n32 -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\" 
LLDLIBS   += -L../../../lib -L../../lib -L./lib -lmld -lreadline -ltermcap
TARGETS   = ../../libfru.a

CFILES    = fru_main.c everror.c fru_io4.c fru_ip19.c fru_ip21.c fru_mc3.c \
			fru_decode.c fru_pattern.c fru_matcher.c icrash_fru.c

OFILES    = $(CFILES:.c=.o)

COMMONPREF=libfru_

OSVERS   != export PATH || exit 1 ; echo $${OSVERSION} | \
			sed 'y/\./\_/' | cut -c1-3

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	ar ccurl $(TARGETS) $(OFILES)

frush: frush.o ../../libfru.a
	${CCF} -o frush frush.o ../../libfru.a

