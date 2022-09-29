#!smake
#
# Makefile for rtdisk
#

include $(ROOT)/usr/include/make/commondefs
WOFF=
LOCALHDR=../include
LCINCS=-I$(LOCALHDR)
LOCALLIB=

RTDISK=rtdisk
LLDLIBS=
SIM=


RTDISKSRCS= rtdisk.c scsi.c
RTDISKOBJS = $(RTDISKSRCS:.c=.o)

XFSDEBUG = $(DEBUG) -DXFSDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER -DSTATIC=static -DXFS_BIG_FILES=1 -DXFS_BIG_FILESYSTEMS=1 $(SIM) -dollar
LCOPTS = $(REGOPTS) $(DEBUG)
LINTOPTS=$(XFSDEBUG) -wlint -woff 813,852,826

TARGETS = $(RTDISK)

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)

$(RTDISK): $(RTDISKOBJS)
	$(CCF) $(RTDISKOBJS) $(LOCALLIB) $(LDFLAGS) -o $@

debug:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'

xfsdebug:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(XFSDEBUG)'

lint:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(LINTOPTS)'

