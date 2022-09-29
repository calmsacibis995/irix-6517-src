include	$(ROOT)/usr/include/make/commondefs

TARGETS=nl
LLDLIBS=-lgen

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
