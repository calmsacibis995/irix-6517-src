include	$(ROOT)/usr/include/make/commondefs

TARGETS=deroff
LCOPTS=-Olimit 2000
LLDLIBS=-lw

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
