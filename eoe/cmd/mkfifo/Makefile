include	$(ROOT)/usr/include/make/commondefs

TARGETS=mkfifo

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)

