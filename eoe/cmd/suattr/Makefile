#ident	"$Revision: 1.2 $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS= suattr
LLDOPTS=-Wl,-I,$(ROOTRLDNAME) -Wl,-woff,85
LCOPTS=-fullwarn -ignore_unresolved -D_SHAREII
DFL    = "config(suggest)"

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -u root -g sys -m 555 -F /sbin $(TARGETS)
