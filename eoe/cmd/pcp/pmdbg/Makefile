#!smake
#
# $Id: Makefile,v 2.11 1997/10/24 06:14:27 markgw Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmdbg.c
TARGETS	= pmdbg
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp
IDBTAG	= PCP_EOE
LDIRT	=

default: $(CFILES) $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(TAG) 0x00010D13 pmdbg
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmdbg
