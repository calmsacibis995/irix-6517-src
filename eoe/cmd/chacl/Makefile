include	$(ROOT)/usr/include/make/commondefs

TARGETS=chacl
LCOPTS=-fullwarn
LLDLIBS=

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
