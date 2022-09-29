#!smake
#ident	"$Revision"

include $(ROOT)/usr/include/make/commondefs
WOFF=
LCOPTS = -fullwarn
INSTDIR = /usr/stress/xfs/error
TARGETS = errorinduce randsleep runall errorloop killemall

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F $(INSTDIR) $(TARGETS)
