#!smake
#
#ident "$Revision: 1.29 $"
#
# Makefile for make(1).
#
BASEVERSION=n32bit
WANTPARALLEL=yes-please

.SUFFIXES:
.SUFFIXES:.o .c .y
include	${ROOT}/usr/include/make/cmdcommondefs
.MAKEOPTS:-P

COMMANDS=nmake
RULES= mipsrules
# in sbin since MAKEDEV needs it
INSTBIN=/sbin
HELPLIB = /usr/lib/help

LCDEFS+=-DMKDEBUG -DOLDVPATH
LLDLIBS+=$(ROOT)/usr/lib32/libmalloc.a
#LLDLIBS= -ldmalloc -lmld -lexc -lmangle
LLDOPTS+=-Wl,-I,$(ROOTRLDNAME_N32) -Wl,-woff,85

CFILES= parallel.c main.c doname.c misc.c files.c dosys.c dyndep.c ${RULES}.c
YFILES= gram.y
LDIRT=btool_map.count btool_map

default: ${TARGETS}

include ${CMDCOMMONRULES}

n32bitinstall: default
	${INSTALL} -F $(INSTBIN) -src nmake make
	${INSTALL} -F /usr/bin -lns ../../sbin/make make
	${INSTALL} -F $(HELPLIB) -m 444 bu

$(COMMANDS): ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

branch:
	$(MAKE) clobber
	btool_init
	$(MAKE) CC="btool -test-quest-bug -realcc $(CC)" $(OBJECTS)
	$(CCF) -c -DNUM_BRANCHES=`cat btool_map.count` btool_lib.c
	${CCF} ${OBJECTS} btool_lib.o ${LDFLAGS} -o make-branch
