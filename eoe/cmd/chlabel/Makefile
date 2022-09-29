include	$(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS=chlabel

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
