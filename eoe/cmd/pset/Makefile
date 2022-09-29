#!smake
#ident "$Revision: 1.6 $"

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=pset

CFILES=pset.c

default: $(TARGETS)

include $(CMDCOMMONRULES)

pset: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

32bitinstall: default
	${INSTALL} -F /sbin -u root -g sys -m 6755 $(IDB_TAG32) $(COMMANDS)

64bitinstall: default
	${INSTALL} -F /sbin -u root -g sys -m 6755 $(IDB_TAG64) $(COMMANDS)
