include	$(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS=sysinfo

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)
