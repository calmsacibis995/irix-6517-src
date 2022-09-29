#!smake
#
# $Id: Makefile,v 1.2 1999/04/28 10:06:17 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

TARGETS	= pmcd_wait
OTHERS	= 

LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp
LDIRT	= *.log foo.* gram.h yy.lex.c y.tab.?

IDBTAG	= PCP_EOE

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS) $(OTHERS)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmcd_wait
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_EOE_NOSHIP $(STRIPBIN)" pmcd_wait
#endif

pmcd_wait:	pmcd_wait.o
	rm -f $@
	$(CCF) -o $@ pmcd_wait.o $(LDOPTS) -lpcp_dev $(LDLIBS)
	$(TAG) 0x00010D13 $@
