#!smake
#
# $Id: Makefile,v 1.2 1997/10/24 06:14:48 markgw Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

TARGETS	= pmdumplog

LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp
LDIRT	= *.log foo.* gram.h yy.lex.c y.tab.?

IDBTAG		= PCP_EOE

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS) $(OTHERS)
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmdumplog

pmdumplog:	pmdumplog.o
	rm -f $@
	$(CCF) -o $@ pmdumplog.o $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D12 $@
