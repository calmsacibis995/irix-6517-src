include	$(ROOT)/usr/include/make/commondefs

LLDLIBS=-lgen

TARGETS=printf

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)

