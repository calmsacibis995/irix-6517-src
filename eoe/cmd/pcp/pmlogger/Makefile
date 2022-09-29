#!smake
#
# $Id: Makefile,v 2.43 1998/11/15 08:35:24 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmlogger.c fetch.c error.c aux.c callback.c ports.c \
	  dopdu.c check.c preamble.c rewrite.c
OBJECTS = pmlogger.o fetch.o gram.o lex.o error.o aux.o callback.o ports.o \
	  dopdu.o check.o preamble.o rewrite.o
TARGETS	= pmlogger

LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp
LDIRT	= *.log foo.* gram.h lex.yy..c y.tab.?
IDBTAG	= PCP_EOE

default: $(CFILES) lex.l gram.y $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS) $(OTHERS)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmlogger

pmlogger:	$(OBJECTS)
	rm -f $@
	$(CCF) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS) -ll
	$(TAG) 0x00010D1F $@

lex.o gram.o:	gram.h
aux.o fetch.o gram.o lex.o pmlogger.o:	logger.h

gram.h:	gram.y
	$(YACC) -d gram.y
	-if cmp -s y.tab.h $@; then rm y.tab.h; touch gram.h; else mv y.tab.h $@; fi

y.tab.c:	gram.y
	$(YACC) gram.y

lex.yy.c:	lex.l
	$(LEX) lex.l
