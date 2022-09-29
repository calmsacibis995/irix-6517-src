include	$(ROOT)/usr/include/make/commondefs

TARGETS=cat
LLDLIBS=$(ROOT)/usr/lib32/libw.a

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)
