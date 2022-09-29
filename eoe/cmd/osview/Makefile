#!smake
# Makefile for osview.
#
BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=osview

LLDLIBS+= -lcurses
LCOPTS+= -fullwarn
LCDEFS+= -DTILES_TO_LPAGES

CFILES = getinfo.c main.c panel.c run.c readstruct.c

default: $(TARGETS)

include $(CMDCOMMONRULES)

osview: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

32bitinstall: default
	$(INSTALL) -F /usr/sbin -m 2755 -O $(IDB_TAG32) osview

64bitinstall: default
	$(INSTALL) -F /usr/sbin -m 2755 -O $(IDB_TAG64) osview
