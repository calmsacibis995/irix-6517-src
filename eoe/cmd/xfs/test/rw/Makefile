#!smake
#ident	"$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
KERN=../../../../kern
XFS=$(KERN)/fs/xfs
SIM=../../sim
LCINCS=-I$(SIM) -I$(XFS) -I$(KERN)


DEBUG = -g -DDEBUG
XFSDEBUG = $(DEBUG) -DXFS_DEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER -DSIM
LCOPTS = $(REGOPTS)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,852
LLDLIBS = -lsim -ldisk -lmalloc
LLDOPTS = -L$(SIM)

TARGETS = rw wfile

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)

rw: rw.o $(SIM)/libsim.a
	$(CCF) rw.o $(LDFLAGS) -o $@

wfile: wfile.o $(SIM)/libsim.a
	$(CCF) wfile.o $(LDFLAGS) -o $@

debug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(DEBUG)'

xfsdebug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(XFSDEBUG)'

lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
