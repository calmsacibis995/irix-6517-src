#!smake

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/cmdcommondefs

COMMANDS = numa_view

CFILES	= numa_view.c

LCOPTS	+=-fullwarn -no_unresolved
LCDEFS	+=-D_KMEMUSER 

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	${INSTALL} -F /usr/sbin -m 755 -O $(IDB_TAG32) numa_view

64bitinstall: default
	${INSTALL} -F /usr/sbin -m 755 -O $(IDB_TAG64) numa_view

