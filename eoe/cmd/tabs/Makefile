include $(ROOT)/usr/include/make/commondefs

TARGETS= tabs
LLDLIBS= -lcurses

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
