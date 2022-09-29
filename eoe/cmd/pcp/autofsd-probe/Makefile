#!smake
#
# $Id: Makefile,v 1.1 1999/04/26 20:44:38 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

TARGETS = autofsd-probe
IDBTAG	= PCP_EOE
LCOPTS	= $(PCP_INC_PATH) $(DEBUG)
LLDLIBS	= -lpcp -lgen
LLDOPTS	= $(PCP_LIBS_PATH)

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG)" autofsd-probe
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_EOE_NOSHIP" autofsd-probe
#endif
