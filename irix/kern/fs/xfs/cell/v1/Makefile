#!smake
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for Distributed XFS
#
#ident	"$Revision: 1.6 $"

DEPTH?=../../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/fs/xfs/cell/v1
.PATH:$(KPATH)

CXFS_NAME=cxfs.a
#CXFS_IDLS=cxfs_client.idl cxfs_server.idl
#CXFS_CSRCS=cxfs_utils.c \
#	$(CXFS_IDLS:S/^/I_/g:.idl=_stubs.c)
CXFS_CSRCS=cxfs_bmap.c \
	cxfs_dcxvn.c \
	cxfs_dsxvn.c \
	cxfs_vfs.c   \
	cxfs_utils.c

CXIDBG_NAME=cxfsidbg.o
CXIDBG_CSRCS=cxfsidbg.c

CXFS_OBJS=${CXFS_CSRCS:.c=.o}
$(CXFS_NAME):$(CXFS_NAME)($(CXFS_OBJS)) MAKELIB

CXFSARR_NAME=cxfsarr.a
CXFSARR_IDLS=cxfs_array.idl
CXFSARR_CSRCS=cxfs_arrmount.c cxfs_sinfo.c \
	$(CXFSARR_IDLS:S/^/I_/g:.idl=_stubs.c)

CXFSARR_OBJS=${CXFSARR_CSRCS:.c=.o}
$(CXFSARR_NAME):$(CXFSARR_NAME)($(CXFSARR_OBJS)) MAKELIB

KERNIDLFILES=$(CXFS_IDLS) 

TARGETS=$(CXFS_NAME) $(CXIDBG_NAME)

CFILES=$(CXFS_CSRCS) $(CXIDBG_CSRCS)

#if CELL_ARRAY
KERNIDLFILES += $(CXFSARR_IDLS) 

TARGETS += $(CXFSARR_NAME) 

CFILES += $(CXFSARR_CSRCS) 
#endif

.PRECIOUS:$(TARGETS)

LINTNAME=cxfs

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	 ${KINSTALL} ${CXFS_NAME}
	 ${KINSTALL} ${CXIDBG_NAME}
	 ${KDEVINSTALL} ${TARGETS}

cxfsidbg.o: cxfsidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/cxfsidbg.c && \
	$(LDF) $(PRODOBJECT) -G 0 -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)


