#!smake
#
# $Id: Makefile,v 2.11 1997/10/24 06:15:28 markgw Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmerr.c
TARGETS	= pmerr
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp
IDBTAG	= PCP_EOE
LDIRT	=

default: $(CFILES) $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(TAG) 0x00010D13 pmerr
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmerr
