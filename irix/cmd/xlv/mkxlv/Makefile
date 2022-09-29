#!smake
#ident "$Revision: 1.7 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
LIB=../lib
KERN=../../../kern
LCINCS=-I../include -I$(KERN)

CFILES = xlv_mk.c lex.yy.c
LEXFILES = xlv.lex
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE
LCOPTS = $(REGOPTS)
LLDLIBS = -lxlv -ll -ldisk
LLDOPTS = -L$(LIB)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

TARGETS = mkxlv

default install: $(TARGETS)

include $(COMMONRULES)

lex.yy.c: xlv.lex
	$(LEX) xlv.lex

mkxlv: $(OBJECTS) $(LIB)/libxlv.a
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
