#!smake
#ident	"$Revision"

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

TARGETS = realloc_root realloc_ext ialloc ialloc2 iread

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)

realloc_root: realloc_root.o $(SIM)/libsim.a
	$(CCF) realloc_root.o $(LDFLAGS) -o $@

realloc_ext: realloc_ext.o $(SIM)/libsim.a
	$(CCF) realloc_ext.o $(LDFLAGS) -o $@

realloc_data: realloc_data.o $(SIM)/libsim.a
	$(CCF) realloc_data.o $(LDFLAGS) -o $@

ialloc: ialloc.o $(SIM)/libsim.a
	$(CCF) ialloc.o $(LDFLAGS) -o $@

ialloc2: ialloc2.o $(SIM)/libsim.a
	$(CCF) ialloc2.o $(LDFLAGS) -o $@

iread: iread.o $(SIM)/libsim.a
	$(CCF) iread.o $(LDFLAGS) -o $@

debug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(DEBUG)'

xfsdebug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(XFSDEBUG)'

lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
