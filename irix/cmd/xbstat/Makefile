#!smake
# Makefile for xbstat.
#
BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=xbstat

LLDLIBS+= -lcurses
LCOPTS+= -fullwarn

# Could add MODE=64bit, but we really do not need it since all machines
# with xbow are 64-bit.
MACHTAG=-idb "mach(CPUBOARD=IP27 CPUBOARD=IP30)"

CFILES = xbstat.c 

default: $(TARGETS)

include $(CMDCOMMONRULES)

xbstat: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

32bitinstall: default
	$(INSTALL) -F /usr/sbin -m 2755 -O $(IDB_TAG32) xbstat

64bitinstall: default
	$(INSTALL) -F /usr/sbin -m 2755 -O $(MACHTAG) xbstat
