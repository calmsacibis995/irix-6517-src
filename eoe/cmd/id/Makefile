# $Revision: 1.13 $
include	$(ROOT)/usr/include/make/commondefs

TARGETS=id
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
