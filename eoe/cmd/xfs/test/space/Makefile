#!smake
#ident	"$Revision: 1.8 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
KERN=../../../../kern
XFS=$(KERN)/fs/xfs
SIM=../../sim
LCINCS=-I$(SIM) -I$(XFS) -I$(KERN)

SRCS = xfs_alist.c xfs_test.c
OBJS = $(SRCS:.c=.o)

DEBUG = -g -DDEBUG
XFSDEBUG = $(DEBUG) -DXFSDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER -DSTATIC=static -DSIM
LCOPTS = $(REGOPTS)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,852
LLDLIBS = -lsim -ldisk -lmalloc
LLDOPTS = -L$(SIM)

TARGETS = xfs_test

default: $(TARGETS)

include $(COMMONRULES)

xfs_test: $(OBJS) $(SIM)/libsim.a
	$(CCF) $(OBJS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(DEBUG)'

xfsdebug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(XFSDEBUG)'

lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
