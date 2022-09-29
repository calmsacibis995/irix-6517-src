#!smake
#
# $Id: Makefile,v 2.15 1997/10/24 05:57:17 markgw Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= dkprobe.c
TARGETS	= dkprobe
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG) $(VERSOPT)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp
IDBTAG	= PCP_EOE
LDIRT	=

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" dkprobe

dkprobe:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)
	$(TAG) 0x00010D13 $@
