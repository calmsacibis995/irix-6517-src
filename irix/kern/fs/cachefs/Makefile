#!smake
#
# Makefile for the SGI IRIS-4D CFS port.
#
#ident	"$$"
DEPTH?=../..
include $(DEPTH)/kcommondefs
KPATH=$(TOP)/fs/cachefs
.PATH:$(KPATH)

#
# add -DLRU_SYNC if it is desired that the LRU pointers and/or idents be
# written to disk after every modification
#
KCOPTS=-fullwarn

CACHEFS_NAME=cachefs.a
CACHEFS_CSRCS=  cachefs_cnode.c cachefs_resource.c cachefs_singlewrc.c \
	cachefs_strict.c cachefs_subr.c cachefs_fscache.c cachefs_vfsops.c \
	cachefs_module.c cachefs_vnops.c cachefs_noopc.c cachefs_backops.c \
	cachefs_frontops.c	\
	cachefs_icrash.c
CACHEFS_OBJS=$(CACHEFS_CSRCS:.c=.o)
$(CACHEFS_NAME):$(CACHEFS_NAME)($(CACHEFS_OBJS)) MAKELIB

CACHEFSIDBG_NAME=cachefsidbg.o
CACHEFSIDBG_CSRCS=cachefsidbg.c

TARGETS=$(CACHEFS_NAME) $(CACHEFSIDBG_NAME)

LINTNAME=cachefs
CFILES=$(CACHEFS_CSRCS) $(CACHEFSIDBG_CSRCS)
HFILES=cachefs_fs.h

#
# Rules
#
include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(LBOOTPOSTOP) -idb onc3.sw.cachefs $(TARGETS)
	$(KDEVINSTALL) $(TARGETS)

headers!
	$(INSTALL) -m 444 -F /usr/include/cachefs $(HFILES)

cachefsidbg.o: cachefsidbg.c
	$(CCF) -c $(JALR) -G 0 $*.c

cachefs_icrash.o: cachefs_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/cachefs_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

