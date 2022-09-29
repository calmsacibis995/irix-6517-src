#!smake
#ident "$Revision: 1.7 $"

include $(ROOT)/usr/include/make/commondefs

COMMINCL = \
	cldmgr.h \
	content.h \
	content_common.h \
	content_inode.h \
	dlog.h \
	drive.h \
	exit.h \
	global.h \
	jdm.h \
	lock.h \
	media.h \
	mlog.h \
	path.h \
	qlock.h \
	ring.h \
	stkchk.h \
	stream.h \
	types.h \
	util.h

DUMPINCL = \
	getopt.h

COMMON = \
	cldmgr.c \
	dlog.c \
	drive.c \
	drive_scsitape.c \
	drive_simple.c \
	drive_minrmt.c \
	global.c \
	jdm.c \
	lock.c \
	mlog.c \
	path.c \
	qlock.c \
	ring.c \
	stkchk.c \
	stream.c \
	util.c

LOCALS = \
	main.c

CFILES = $(COMMON) $(LOCALS)
OFILES = $(CFILES:.c=.o)
LINKS = $(COMMINCL) $(DUMPINCL) $(COMMON)

LCOPTS = -fullwarn
LCDEFS = -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DDUMP -DRMT -DCLRMTAUD -DEXTATTR -DBANYAN -DFICUS -DNDEBUG
LLDLIBS = -lrmt -B static -ldm -B dynamic
LDIRT = $(LINKS)
WOFF =

TARGETS = xfs_dumpscan

default: $(LINKS) $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)

xfs_dumpscan: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

$(DUMPINCL):
	@rm -f $@; ln -s ../dump/$@ $@

$(COMMINCL) $(COMMON):
	@rm -f $@; ln -s ../common/$@ $@
