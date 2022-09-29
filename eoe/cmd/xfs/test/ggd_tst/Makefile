#!smake
#ident "$Revision: 1.8 $"
#


BASEVERSION=32bit
ALTVERSIONS=n32bit 64bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

WOFF=
KERN=../../../../kern
XFS=$(KERN)/fs/xfs
#LIBGRIO=../../../../lib/libgrio

#
# Edit the lines below to build a ggd_test which runs with the emulator
#
GGD_TST=ggd_tst
#GGD_TST=ggd_tst.sim
#SIMLIB=-lsimc

#LCINCS=-I$(SIM) -I$(XFS) -I$(KERN)

SRCS = ggd_tst.c
OBJS = $(SRCS:.c=.o)

DEBUG = -g -DDEBUG
XFSDEBUG = $(DEBUG) -DXFSDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER -DSTATIC=static -DSIM
LCOPTS = $(REGOPTS)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,852
LLDLIBS = -lgrio $(SIMLIB)
#LLDOPTS = -L$(LIBGRIO)

TARGETS = $(GGD_TST)

default: $(TARGETS)

include $(CMDCOMMONRULES)

$(GGD_TST): $(OBJS) 
	$(CCF) $(OBJS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(DEBUG)'

xfsdebug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(XFSDEBUG)'

lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
