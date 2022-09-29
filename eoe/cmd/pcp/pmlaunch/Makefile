#!smake
#
# $Id: Makefile,v 1.27 1999/05/11 00:28:03 kenmcd Exp $

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

MENUS		=
SCRIPTS_MON	=

#if $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
MENUS		+= pcp_eoe_eoe.menu pcp_eoe_monitor.menu
#endif

#if $(ISM_NAME) == "pcp"
MENUS		+= pcp_monitor.menu
SCRIPTS_MON	+= _mpvis _nodevis _pmchart _pmdumptext _pmgadgets_frontend \
		   _routervis _dkvis _pcp _xlv_vis
#endif

SCRIPTS_EOE_EOE	= pmlaunch.common mk.rc _pmkstat

SCRIPTS_EOE_MON = _pmval _pmview_frontend

LDIRT		= pmlaunchrc
EXITOP		= exitop(`cat exitop`)
REMOVEOP	= removeop(`cat removeop`)

IDBTAG_EOE_EOE	= PCP_EOE
IDBTAG_EOE_MON	= PCP_EOE_MON
IDBTAG_MON	= PCP_MON

default: $(SCRIPTS_EOE_EOE) $(SCRIPTS_EOE_MON) $(SCRIPTS_MON) $(MENUS)

include $(COMMONRULES)

install: default
#if $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
	$(INSTALL) -F /var/pcp/config/pmlaunch -m 555 -idb "$(IDBTAG_EOE_EOE) $(STRIPBIN)" $(SCRIPTS_EOE_EOE)
	$(INSTALL) -F /var/pcp/config/pmlaunch -m 555 -idb "$(IDBTAG_EOE_MON) $(STRIPBIN)" $(SCRIPTS_EOE_MON)
	$(INSTALL) -F /var/pcp/config/pmlaunch -m 555 -idb "$(IDBTAG_EOE_EOE) $(STRIPBIN) $(EXITOP) $(REMOVEOP)" pcp_eoe_eoe.menu
	$(INSTALL) -F /var/pcp/config/pmlaunch -m 555 -idb "$(IDBTAG_EOE_MON) $(STRIPBIN) $(EXITOP) $(REMOVEOP)" pcp_eoe_monitor.menu
#endif
#if $(ISM_NAME) == "pcp"
	$(INSTALL) -F /var/pcp/config/pmlaunch -m 555 -idb "$(IDBTAG_MON) $(STRIPBIN)" $(SCRIPTS_MON)
	$(INSTALL) -F /var/pcp/config/pmlaunch -m 555 -idb "$(IDBTAG_MON) $(STRIPBIN) $(EXITOP) $(REMOVEOP)" pcp_monitor.menu
#endif




