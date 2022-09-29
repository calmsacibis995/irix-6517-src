include	$(ROOT)/usr/include/make/commondefs

TARGETS=banner
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
