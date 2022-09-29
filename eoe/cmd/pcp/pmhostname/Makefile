#!smake
#
# $Id: Makefile,v 1.1 1999/04/28 10:06:17 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmhostname.c
TARGETS	= pmhostname
LCOPTS	= -fullwarn
LCDEFS	= $(DEBUG)
LLDOPTS	= 
LLDLIBS	= 
LDIRT	= 

IDBTAG	= PCP_EOE

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmhostname
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_EOE_NOSHIP $(STRIPBIN)" pmhostname
#endif

pmhostname:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) -o $@ $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D13 $@
