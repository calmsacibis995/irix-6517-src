#!smake
#ident	"$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
WOFF=
LCOPTS = -fullwarn

TARGETS = fstyp.xfs

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /sbin/fstyp.d $(TARGETS)

fstyp.xfs: fstyp.o
	$(CCF) fstyp.o $(LDFLAGS) -o $@
