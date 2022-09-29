#!smake
#ident	"$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
KERN=../../../../kern
XFS=$(KERN)/fs/xfs
SIM=../../sim
LCINCS=-I$(SIM) -I$(XFS) -I$(KERN)


DEBUG = -g -DDEBUG -D_LOG_DEBUG
XFSDEBUG = $(DEBUG) -DXFSDEBUG
LCOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER $(XFSDEBUG)
LLDLIBS = -lsim -lmalloc
LLDOPTS = -L$(SIM)

TARGETS = log

default: $(TARGETS)

include $(COMMONRULES)

log: main.o $(SIM)/libsim.a
	$(CCF) main.o $(LDFLAGS) -o log
