#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
# Copyright (c) 1994 Transarc Corporation
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.5 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/tkm
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libktkm.a
OBJ_CSRCS=	tkm_recycle.c \
		tkm_misc.c \
		tkm_asyncgrant.c \
		tkm_tokens.c \
		tkm_filetoken.c \
		tkm_voltoken.c \
		tkm_file.c \
		tkm_volume.c \
		tkm_revokes.c \
		tkm_conflicts.c \
		tkm_ctable.c \
		tkm_tokenlist.c \
		tkm_range.c \
		tkm_race.c \
		tkm_linkage.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		tkm_errs.h \
		tkm_race.h \
		tkm_tokens.h \
		tkm_trace.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

