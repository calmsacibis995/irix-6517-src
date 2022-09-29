#!smake
#
# Makefile for ggd (guarantee granting daemon : xfs)
#

include $(ROOT)/usr/include/make/commondefs
WOFF=
LOCALHDR=../include
LCINCS=-I$(LOCALHDR)
LOCALLIB=

GGD=ggd
LLDLIBS = -lgrio -ldisk
SIM=


GGDSRCS = ggd.c lic_flexlm.c prims.c vdisk.c vdisk_cache.c iosize.c reservation.c info.c alloc_streams.c shm.c cache.c griotab.c hw_layer.c disk.c generic_device.c
GGDOBJS = $(GGDSRCS:.c=.o)

XFSDEBUG = $(DEBUG) -DXFSDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -D_KMEMUSER -DSTATIC=static -DXFS_BIG_FILES=1 -DXFS_BIG_FILESYSTEMS=1 $(SIM) -dollar
LCOPTS = $(REGOPTS) $(DEBUG)
LINTOPTS=$(XFSDEBUG) -wlint -woff 813,852,826

TARGETS = $(GGD)

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/sbin $(GGD)
	$(INSTALL) -F /etc grio_disks

$(GGD): $(GGDOBJS)
	$(CCF) $(GGDOBJS) $(LOCALLIB) $(LDFLAGS) -llmsgi -o $@

debug:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'

xfsdebug:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(XFSDEBUG)'

lint:
	$(MAKE) 'LCDEFS=$(REGOPTS) $(LINTOPTS)'

