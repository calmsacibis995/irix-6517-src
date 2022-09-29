#!smake
#ident	"$Revision: 1.11 $"

include $(ROOT)/usr/include/make/commondefs

WOFF=
KERN=../../../../kern
XFS=$(KERN)/fs/xfs
SIM=../../sim
LCINCS=-I$(SIM) -I$(XFS) -I$(KERN)


DEBUG = -g -DDEBUG
XFSDEBUG = $(DEBUG) -DXFSDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER -DSIM
LCOPTS = $(REGOPTS)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,852
LLDLIBS = -lsim -ldisk -lmalloc
LLDOPTS = -L$(SIM)

TARGETS = vbigdir vdir vlink

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)

vbigdir: vbigdir.o $(SIM)/libsim.a
	$(CCF) vbigdir.o $(LDFLAGS) -o $@

vdir: vdir.o $(SIM)/libsim.a
	$(CCF) vdir.o $(LDFLAGS) -o $@

vlink: vlink.o $(SIM)/libsim.a
	$(CCF) vlink.o $(LDFLAGS) -o $@

debug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(DEBUG)'

xfsdebug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(XFSDEBUG)'

lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
