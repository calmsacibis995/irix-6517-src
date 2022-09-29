#!smake

#ident	"$Revision: 1.20 $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS=getty
LCOPTS=-fullwarn
LLDOPTS=-Wl,-T,7FC00000,-D,7FC40000 -Wl,-I,$(ROOTRLDNAME)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)
