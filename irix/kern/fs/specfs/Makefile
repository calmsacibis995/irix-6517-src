#!smake
#
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for fs/specfs
#

#ident	"$Revision: 1.14 $"

DEPTH?=	../..

include $(DEPTH)/kcommondefs

KPATH=	$(TOP)/fs/specfs

.PATH:$(KPATH)

LINTNAME=	specfs

SPECFS_NAME=	specfs.a

SPECFS_CSRCS=	specsubr_attr.c		\
		specsubr_local.c	\
		specsubr_common.c	\
		specvfsops.c		\
		specvnops_attr.c	\
		specvnops_local.c	\
		specvnops_common.c
#if defined(CELL_IRIX)
SUBDIRS=cell
#else	/* ! defined(CELL_IRIX) */
SUBDIRS=
#endif	/* ! defined(CELL_IRIX) */

SPECFS_OBJS=	$(SPECFS_CSRCS:.c=.o)

$(SPECFS_NAME):	$(SPECFS_NAME)($(SPECFS_OBJS)) MAKELIB

MAC_NAME=	mac_specfs.o
MAC_CSRCS=	mac_specfs.c
MAC_OBJS=	$(MAC_CSRCS:.c=.o)

SPECFS_IDBG_NAME=	spec_idbg.a
SPECFS_IDBG_CSRCS=	spec_idbg.c
SPECFS_IDBG_OBJS=	${SPECFS_IDBG_CSRCS:.c=.o}
$(SPECFS_IDBG_NAME):	$(SPECFS_IDBG_NAME)($(SPECFS_IDBG_OBJS)) MAKELIB

ITARGETS=	$(SPECFS_IDBG_NAME)
TARGETS=	$(SPECFS_NAME) $(SPECFS_IDBG_NAME) $(MAC_NAME)

CFILES=	$(SPECFS_CSRCS) $(SPECFS_IDBG_CSRCS) $(MAC_CSRCS)

HFILES=	spec_ops.h spec_lsnode.h spec_csnode.h

#
# Rules
#
include $(DEPTH)/kcommonrules

$(KCOMMONPREF)default: $(TARGETS)
	$(KLINKINSTALL) $(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) $(TARGETS)
	${KDEVINSTALL} ${TARGETS}

headers!
	$(INSTALL) -m 444 -F /usr/include/sys/fs $(HFILES)

spec_idbg.o: spec_idbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/spec_idbg.c
