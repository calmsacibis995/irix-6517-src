#!smake
#
# Makefile for grio
#
#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio/RCS/Makefile,v 1.9 1996/12/02 21:59:14 kanoj Exp $"


OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs
WOFF=
KERN=../../../kern

#
# Edit the lines below to build a ggd which runs with the simulator
#
GRIOVIEW=grio
LLDLIBS = -lgrio -ldisk

GRIOSRCS = bandwidth.c file.c main.c path.c prim.c rotation.c trace.c
GRIOOBJS = $(GRIOSRCS:.c=.o)

DEBUG=-g -DDEBUG
XFSDEBUG = $(DEBUG) -DXFSDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER -DSTATIC=static -DXFS_BIG_FILES=1 -DXFS_BIG_FILESYSTEMS=1 $(SIM)
LCOPTS = $(REGOPTS)
LINTOPTS=$(XFSDEBUG) -wlint -woff 813,852

TARGETS = $(GRIOVIEW) 

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/sbin $(GRIOVIEW)

$(GRIOVIEW): $(GRIOOBJS)
	$(CCF) $(GRIOOBJS) $(LDFLAGS) -o $@

debug:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'

xfsdebug:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(XFSDEBUG)'

lint:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(LINTOPTS)'

