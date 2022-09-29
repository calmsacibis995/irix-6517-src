include	$(ROOT)/usr/include/make/commondefs

TARGETS=chcap
LCOPTS=-fullwarn
LLDLIBS=

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -m 555 -F /sbin $(TARGETS)
