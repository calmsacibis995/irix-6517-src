#!smake
#ident	"$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

WOFF=
LCOPTS = -fullwarn
INSTDIR = /usr/sbin

TARGETS = xfs_imap

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F $(INSTDIR) xfs_imap
