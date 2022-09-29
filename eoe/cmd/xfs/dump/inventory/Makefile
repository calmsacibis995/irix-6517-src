#!smake

OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs
COMMINCL = cldmgr.h \
           cleanup.h \
           content.h \
           content_inode.h \
           dlog.h \
           drive.h \
           fs.h \
           global.h \
           inomap.h \
           inv_priv.h \
           jdm.h \
           lock.h \
           media.h \
           media_rmvtape.h \
           mlog.h \
           namreg.h \
           openutil.h \
           stream.h \
           types.h \
           util.h


COMMON = mlog.c \
	stream.c \
	lock.c \
	cleanup.c 


LOCALS = inv_core.c \
	inv_idx.c \
	inv_stobj.c \
	inv_api.c \
	inv_fstab.c \
	inv_mgr.c 

	
INVLOCALS = inv_core.c \
	inv_idx.c \
	inv_stobj.c \
	inv_api.c \
	inv_fstab.c \
	inv_mgr.c \
	testmain.c
         
SRCS = $(COMMON) $(LOCALS)
INVSRCS = $(COMMON) $(INVLOCALS)
OBJS = $(SRCS:.c=.o)
INVOBJS = $(INVSRCS:.c=.o)

DEBUG = -g -DDEBUG
#REGOPTS = -fullwarn -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DDUMP -DRMT -DINVT_DEBUG -DDEBUG -DINVT_DELETION

REGOPTS = -fullwarn -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DDUMP -DRMT -DDEBUG -DRECONS -DINVT_DELETION

LCOPTS = $(REGOPTS)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852
LLDLIBS = -lgen -lrmt

TARGETS = inv

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /sbin $(TARGETS)

inv: flag $(INVOBJS)
	$(CCF) $(INVOBJS) $(LDFLAGS) -o $@

dump: flag $(OBJS)
	$(CCF) $(OBJS) $(LDFLAGS) -o $@
debug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(DEBUG)'

lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'

flag: $(COMMINCL) $(COMMON)
	> flag

$(COMMINCL) $(COMMON):
	ln -s ../common/$@ .
