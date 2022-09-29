#!smake
#ident	"$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

WOFF=
LCOPTS = -fullwarn
INSTDIR = /usr/sbin

TARGETS = xfs_bmap

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F $(INSTDIR) xfs_bmap
