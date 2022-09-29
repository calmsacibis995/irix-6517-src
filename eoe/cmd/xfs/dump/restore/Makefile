#!smake
#ident "$Revision: 1.22 $"

include $(ROOT)/usr/include/make/commondefs

COMMINCL = \
	cldmgr.h \
	content.h \
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
	path.c \
	qlock.c \
	ring.c \
	stkchk.c \
	stream.c \
	util.c

LOCALS = \
	bag.c \
	content.c \
	dirattr.c \
	inomap.c \
	namreg.c \
	node.c \
	tree.c \
	win.c

CFILES = $(COMMON) $(INVCOMMON) $(LOCALS)
OFILES = $(CFILES:.c=.o)
LINKS = $(COMMINCL) $(COMMON) $(INVINCL) $(INVCOMMON)

LCOPTS = -fullwarn
LCDEFS = -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DRESTORE -DRMT -DCLRMTAUD -DEXTATTR -DDOSOCKS -DEOMFIX -DINVCONVFIX -DSESSCPLT -DWHITEPARSE -DPIPEINVFIX -DBANYAN -DFICUS -DNDEBUG
LLDLIBS = -B static -lgen -lrmt -ldm -B dynamic -ldisk
LLDOPTS = -Wl,-I,$(ROOTRLDNAME)
LDIRT = $(LINKS)
WOFF =

TARGETS = xfsrestore

default: $(LINKS) $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
	$(INSTALL) -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)

xfsrestore: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

$(COMMINCL) $(COMMON):
	@rm -f $@; ln -s ../common/$@ $@

$(INVINCL) $(INVCOMMON):
	@rm -f $@; ln -s ../inventory/$@ $@
