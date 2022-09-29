#!smake
#
# $Id: Makefile,v 1.2 1999/04/28 10:32:28 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

IDBTAG	= PCP_EOE

default: hipprobe

include $(COMMONRULES)

install: hipprobe
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG)" hipprobe
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_EOE_NOSHIP" hipprobe
#endif
