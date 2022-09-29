#!smake
#ident	"$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

WOFF=
LCOPTS = -fullwarn
INSTDIR = /usr/sbin
LLDLIBS = -ldisk

TARGETS = xfs_growfs

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F $(INSTDIR) xfs_growfs
