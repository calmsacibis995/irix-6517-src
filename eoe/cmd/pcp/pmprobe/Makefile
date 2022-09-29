#!smake
#
# $Id: Makefile,v 1.8 1999/05/11 00:28:03 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmprobe.c
TARGETS	= pmprobe
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp
IDBTAG	= PCP_EOE
LDIRT	= *.o

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmprobe
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/sbin -m 555 -idb "PCP_EOE_NOSHIP $(STRIPBIN)" pmprobe
#endif

pmprobe:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) -o $@ $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D0A $@
