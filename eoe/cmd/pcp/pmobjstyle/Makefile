#!smake
#
# $Id: Makefile,v 1.3 1997/11/13 03:36:28 markgw Exp $
#
PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

CFILES	= pmobjstyle.c
TARGETS	= pmobjstyle
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LCDEFS	= $(DEBUG)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp
IDBTAG	= PCP_EOE
LDIRT	=

default: $(CFILES) $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(TAG) 0x00010D13 pmobjstyle
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmobjstyle
