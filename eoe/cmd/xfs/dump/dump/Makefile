#!smake
#ident "$Revision: 1.25 $"

include $(ROOT)/usr/include/make/commondefs

COMMINCL = \
	cldmgr.h \
	content.h \
	content_common.h \
	content_inode.h \
	dlog.h \
	drive.h \
	exit.h \
	fs.h \
	global.h \
	jdm.h \
	lock.h \
	media.h \
	mlog.h \
	openutil.h \
	path.h \
	qlock.h \
	ring.h \
	stkchk.h \
	stream.h \
	types.h \
	util.h

INVINCL = \
	inventory.h \
	inv_priv.h

INVCOMMON = \
	inv_api.c \
	inv_core.c \
	inv_fstab.c \
	inv_idx.c \
	inv_mgr.c \
	inv_stobj.c

COMMON = \
	cldmgr.c \
	content_common.c \
	dlog.c \
	drive.c \
	drive_scsitape.c \
	drive_simple.c \
	drive_minrmt.c \
	fs.c \
	global.c \
	jdm.c \
	lock.c \
	main.c \
	mlog.c \
	openutil.c \
	qlock.c \
	path.c \
	ring.c \
	stkchk.c \
	stream.c \
	util.c

LOCALS = \
	content.c \
	hsmapi.c \
	inomap.c \
	var.c

CFILES = $(COMMON) $(INVCOMMON) $(LOCALS)
OFILES = $(CFILES:.c=.o)
LINKS = $(COMMINCL) $(COMMON) $(INVINCL) $(INVCOMMON)

LCOPTS = -fullwarn
LCDEFS = -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DDUMP -DRMT -DCLRMTAUD -DEXTATTR -DBASED -DDOSOCKS -DINVCONVFIX -DSIZEEST -DPIPEINVFIX -DBANYAN -DFICUS -DNDEBUG
LLDLIBS = -lgen -lrmt -B static -ldm -B dynamic -ldisk
LDIRT = $(LINKS)
WOFF =

TARGETS = xfsdump

default: $(LINKS) $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)
	$(INSTALL) -F /etc -lns ../usr/sbin/$(TARGETS) $(TARGETS)
	$(INSTALL) -F /sbin -lns ../usr/sbin/$(TARGETS) $(TARGETS)

xfsdump: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

$(COMMINCL) $(COMMON):
	@rm -f $@; ln -s ../common/$@ $@

$(INVINCL) $(INVCOMMON):
	@rm -f $@; ln -s ../inventory/$@ $@
