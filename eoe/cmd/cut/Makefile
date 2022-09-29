include	$(ROOT)/usr/include/make/commondefs

TARGETS=cut
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
