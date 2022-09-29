#!smake
#
# Makefile for grio_bandwidth
#
#ident  "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/grio_bandwidth/RCS/Makefile,v 1.5 1997/04/29 22:08:52 kanoj Exp $"


include $(ROOT)/usr/include/make/commondefs
WOFF=
KERN=../../../kern

#
# Edit the lines below to build a ggd which runs with the simulator
#
GRIOBAND=grio_bandwidth
LLDLIBS = -lds -ldisk

GRIOSRCS = grio_bandwidth.c grio_raid.c grio_controller.c
GRIOOBJS = $(GRIOSRCS:.c=.o)

DEBUG=-g -DDEBUG
XFSDEBUG = $(DEBUG) -DXFSDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER -DSTATIC=static -DXFS_BIG_FILES=1 -DXFS_BIG_FILESYSTEMS=1 $(SIM)
LCOPTS = $(REGOPTS)
LINTOPTS=$(XFSDEBUG) -wlint -woff 813,852

TARGETS = $(GRIOBAND) 

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/sbin $(GRIOBAND)

$(GRIOBAND): $(GRIOOBJS)
	$(CCF) $(GRIOOBJS) $(LDFLAGS) -o $@

debug:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'

xfsdebug:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(XFSDEBUG)'

lint:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(LINTOPTS)'

