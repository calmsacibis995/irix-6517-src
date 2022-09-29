#!smake
#
# $Id: Makefile,v 2.13 1997/10/24 06:18:06 markgw Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmlc.c error.c io.c aux.c actions.c
OBJECTS = pmlc.o gram.o lex.o error.o io.o aux.o actions.o
TARGETS	= pmlc
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG) '-DPCP_VERSION="$(PCP_VERSION)"'
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp
LDIRT	= *.log foo.* gram.h yy.lex.c y.tab.?

IDBTAG = PCP_EOE

default: $(CFILES) lex.l gram.y $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS) 
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmlc

pmlc:	$(OBJECTS)
	rm -f $@
	$(CCF) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D15 $@

pmlc.o lex.o gram.o:	gram.h
pmlc.o aux.o fetch.o gram.o lex.o pmlogger.o actions.c:	pmlc.h

gram.h:	gram.y
	$(YACC) -d gram.y
	-if cmp -s y.tab.h $@; then rm y.tab.h; touch gram.h; else mv y.tab.h $@; fi
