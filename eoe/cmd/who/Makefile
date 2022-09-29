include	$(ROOT)/usr/include/make/commondefs

TARGETS=who
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/who who
