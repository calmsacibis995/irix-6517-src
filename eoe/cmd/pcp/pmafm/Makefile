#!smake
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

SOURCE	= pmafm mkaf pmafm.pcp_eoe.eoe pmafm.pcp_eoe.monitor
#if $(ISM_NAME) == "pcp"
SOURCE	+= pmafm.pcp.base pmafm.pcp.monitor
#endif

LDIRT	=

IDBTAG_EOE	= PCP_EOE
IDBTAG_EOE_MON	= PCP_EOE_MON
IDBTAG_MON	= PCP_MON
IDBTAG_BASE	= PCP_BASE

default: $(SOURCE)

include $(COMMONRULES)

install: default
#if $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG_EOE)" pmafm
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG_EOE)" mkaf
	$(INSTALL) -F /var/pcp/config/pmafm -m 444 -idb "$(IDBTAG_EOE)" -src pmafm.pcp_eoe.eoe pcp_eoe.eoe
	$(INSTALL) -F /var/pcp/config/pmafm -m 444 -idb "$(IDBTAG_EOE_MON)" -src pmafm.pcp_eoe.monitor pcp_eoe.monitor
#endif
#if $(ISM_NAME) == "pcp"
	$(INSTALL) -F /var/pcp/config/pmafm -m 444 -idb "$(IDBTAG_MON)" -src pmafm.pcp.monitor pcp.monitor
	$(INSTALL) -F /var/pcp/config/pmafm -m 444 -idb "$(IDBTAG_BASE)" -src pmafm.pcp.base pcp.base
#endif
