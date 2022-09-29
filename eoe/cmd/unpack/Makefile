include	$(ROOT)/usr/include/make/commondefs

TARGETS=unpack

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -f /usr/bin unpack
	$(INSTALL) -ln unpack -F /usr/bin pcat
