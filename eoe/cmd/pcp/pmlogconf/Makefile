#!smake
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

LDIRT	=

IDBTAG	= PCP_EOE

default: pmlogconf.sh

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG)" -src pmlogconf.sh pmlogconf
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_EOE_NOSHIP" -src pmlogconf.sh pmlogconf
#endif
