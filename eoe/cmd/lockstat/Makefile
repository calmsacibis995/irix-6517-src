#!smake
# Makefile for lockstat.
#

BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=lockstat

CFILES = lockstat.c elfsubr.c prfsubr.c

LCOPTS+= -fullwarn

default: $(TARGETS)

include $(CMDCOMMONRULES)

lockstat: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

32bitinstall: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG32) $(COMMANDS)

64bitinstall: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG64) $(COMMANDS)

