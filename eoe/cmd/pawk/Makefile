#!smake
#
# Makefile for pawk(1).
#
#ident "$Revision: 1.1 $"

BASEVERSION=n32bit
ALTVERSIONS=troot
WANTPARALLEL=yes-please
include	$(ROOT)/usr/include/make/cmdcommondefs

LFILES= awk.lx.l
YFILES= awk.g.y
CFILES= b.c lib.c main.c parse.c regex.c run.c tran.c
COMMANDS=pawk
LLDLIBS=-lw -lmalloc -lm
GLDOPTS=

LDIRT= maketab awk.g.c y.tab.h proctab.[co]

default: y.tab.h $(TARGETS)

include $(CMDCOMMONRULES)
TLINKXARGS+=-x maketab -x awk.g.c -x proctab.c

n32bitinstall: default
	$(INSTALL) -F /usr/bin $(COMMANDS)

trootinstall: default
	$(INSTALL) -F /usr/bin $(COMMANDS)

$(OBJECTS): y.tab.h
y.tab.h: awk.g.y
	$(YACCF) -d awk.g.y
	mv y.tab.c awk.g.c
awk.g.c: y.tab.h

proctab.c: maketab
	./maketab > $@

# Maketab should be built using native tools, since it's an intermediate
# tool that doesn't ship.
maketab: $$@.c y.tab.h
	${HOST_CC} $@.c -o $@

CVERSION= -cckr

pawk: $(OBJECTS) proctab.o
	$(CC) $(CFLAGS) $(OBJECTS) proctab.o $(LDFLAGS) -o $@
