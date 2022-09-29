include	$(ROOT)/usr/include/make/commondefs

TARGETS=stat
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
