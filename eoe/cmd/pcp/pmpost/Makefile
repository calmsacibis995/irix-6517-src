#!smake
#
# $Id: Makefile,v 2.6 1999/05/11 00:28:03 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmpost.c
TARGETS	= pmpost
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	=
LLDLIBS	= -lgen
IDBTAG	= PCP_EOE
LDIRT	=

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/pcp/bin -u root -m 4555 -idb "$(IDBTAG) $(STRIPBIN)" pmpost

pmpost:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) -o $@ $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D13 $@
