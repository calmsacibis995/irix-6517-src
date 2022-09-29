#!smake
#
# Makefile for IRIX crash HWREG library
#ident "$Header: "
#
# NOTE: The following files:
#		hwreg.c
#		hwreg_c0.c
#		hwreg_dir.c
#		hwreg_hub.c
#		hwreg_router.c
#		hwreg_xbow.c
#       are direct copies of the files of the same name from
#		stand/arcs/tools/hwreg
#	These files use
#		icrash/include/hwreg.h
#	which is a copy from
#		stand/arcs/include/hwreg.h
#	If these files ever change in 'stand/arcs/tools/hwreg'
#	they HAVE TO BE copied manually over here to keep icrash in sync!
#	This is done this way to avoid crossing ISM's.
#

include $(ROOT)/usr/include/make/commondefs

GCDEFS    = -DICRASH_DEBUG -DI_TEMP
LCDEFS    = -I../../include -g -n32 -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\" \
		-DIP27 -DSN0 -D_KERNEL -DHUB1_WAR -D_PAGESZ=4096
TARGETS   = ../../libhwreg.a
CFILES    = hwreg.c hwreg_c0.c hwreg_dir.c hwreg_hub.c hwreg_router.c \
	    hwreg_xbow.c hwregutil.c

OFILES    = $(CFILES:.c=.o)

OSVERS   != export PATH || exit 1 ; echo $${OSVERSION} | \
			sed 'y/\./\_/' | cut -c1-3

COMMONPREF=libhwreg_

all: default

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

default: $(TARGETS)

$(TARGETS): $(OFILES)
	ar ccurl $(TARGETS) $(OFILES)
