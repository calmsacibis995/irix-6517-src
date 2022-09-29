include	$(ROOT)/usr/include/make/commondefs

TARGETS=uniq
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
