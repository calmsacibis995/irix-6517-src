# $Revision: 1.3 $
include	$(ROOT)/usr/include/make/commondefs

TARGETS=userck
LLDLIBS=-lmls

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
