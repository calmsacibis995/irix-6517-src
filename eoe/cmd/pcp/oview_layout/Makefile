#!smake
#
# $Id: Makefile,v 1.11 1997/10/24 06:07:40 markgw Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

TARGETS	=	oview_layout check_layout
IDBTAG	=	PCP_EOE_MON
LDIRT	=

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG)" oview_layout check_layout
	$(INSTALL) -F $(PMLOGGERCONFIG) -m 444 -idb $(IDBTAG) -src pmlogger.config config.oview_layout

oview_layout: oview_layout.sh
	rm -f $@
	cp oview_layout.sh oview_layout

check_layout: check_layout.sh
	rm -f $@
	cp check_layout.sh check_layout

