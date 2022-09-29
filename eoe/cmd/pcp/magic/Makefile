#!smake
#
# $Id: Makefile,v 1.4 1997/11/13 02:06:20 markgw Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

IDBTAG	= PCP_EOE
LDIRT	=
#if $(BEFORE_IRIX6_5)
EXITOP	= `cat exitop`
#else
EXITOP	=
#endif

default: magic update-magic

include $(COMMONRULES)

install:	default
	$(INSTALL) -F /var/pcp/config/magic -m 555 -idb "$(IDBTAG) $(EXITOP)" update-magic
	$(INSTALL) -F /var/pcp/config/magic -m 444 -idb "$(IDBTAG)" magic
