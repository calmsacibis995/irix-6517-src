#!smake
#
# Copyright 1995, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for Distributed Spec FS
#

#ident  "$Id: Makefile,v 1.2 1997/09/08 18:24:27 jtk Exp $"

DEPTH?=	../../..

include ${DEPTH}/kcommondefs

KPATH=	$(TOP)/fs/specfs/cell

.PATH:$(KPATH)

LINTNAME=	specfs

SPECFS_NAME=	specfs.a

LCFLAGS+=	-I$(TOP)/fs/specfs

SPECFS_IDLS=	spec_dc.idl spec_ds.idl

KERNIDLFILES=	$(SPECFS_IDLS)

SPECFS_CSRCS=	specsubr_dc.c specsubr_ds.c  \
			$(SPECFS_IDLS:S/^/I_/g:.idl=_stubs.c)

SPECFS_OBJS=	${SPECFS_CSRCS:.c=.o}

$(SPECFS_NAME):	$(SPECFS_NAME)($(SPECFS_OBJS)) MAKELIB

TARGETS=	$(SPECFS_NAME)
.PRECIOUS:$(TARGETS)

CFILES=	$(SPECFS_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:	$(TARGETS)

$(KCOMMONPREF)install:	$(KCOMMONPREF)default
