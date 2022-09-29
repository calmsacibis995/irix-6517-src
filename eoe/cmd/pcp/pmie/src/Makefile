#!smake
#
# $Id: Makefile,v 1.1 1999/04/28 10:06:17 kenmcd Exp $

PCP_SRC_DEPTH = ../..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs
include versiondefs

TARGETS	= pmie
CFILES	= symbol.c dstruct.c lexicon.c syntax.c pragmatics.c eval.c \
	  show.c pmie.c match_inst.c syslog.c
OBJECTS	= symbol.o dstruct.o lexicon.o grammar.o syntax.o pragmatics.o \
	  fun.o eval.o show.o pmie.o match_inst.o syslog.o
SKELETAL= hdr.sk fetch.sk misc.sk aggregate.sk unary.sk binary.sk \
	  merge.sk act.sk

LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG) '-DPCP_VERSION="$(PCP_VERSION)"'
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp -llmsgi -lm
LDIRT	= *.log grammar.h yy.lex.c fun.c y.tab.? y.output

#if ! $(BEFORE_IRIX6_5)
LLDLIBS	+= -lexc
LCDEFS	+= -DHAVE_TRACE_BACK_STACK=1
#endif

default: $(CFILES) grammar.y $(TARGETS)

include $(COMMONRULES)

pmie:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) -o $@ $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D0D $@

fun.o:	fun.c fun.h

fun.c:	fun.h $(SKELETAL) meta
	./meta

grammar.o lexicon.o syntax.o: grammar.h

grammar.h: grammar.y
	@echo "expect 172 harmless shift/reduce conflicts"
	$(YACC) -d grammar.y
	-if cmp -s y.tab.h $@; then rm y.tab.h; touch grammar.h; else mv y.tab.h $@; fi
