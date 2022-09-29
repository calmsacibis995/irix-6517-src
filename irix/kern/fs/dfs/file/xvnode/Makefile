#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.7  1999/02/04 19:19:46  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.6  1998/02/27 17:32:03  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.5  1998/02/27 16:16:02  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:54:53  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.1  1997/10/20  19:20:23  jdoak
# *** empty log message ***
#
# Revision 1.1.717.1  1996/10/02  19:03:38  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:51:59  damon]
#
# $EndLog$
#
# Copyright (C) 1996 Transarc Corporation -- All Rights Reserved
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.7 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/xvnode
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

CFILES=		xvfs_vnode.c \
		xvfs_volume.c \
		xvfs_linkage.c \
		xvfs_os2vfs.c \
		xvfs_osglue.c \
		xvfs_vfs2os.c

HFILES=		xvfs_vnode.h xvfs_xattr.h xvfs_export.h xvfs_osvnode.h

OBJ_NAME=	libxvnode.a
OBJS=		$(CFILES:.c=.o)
OBJ_FILES=	$(OBJS)

$(OBJ_NAME):$(OBJ_NAME)($(OBJ_FILES)) MAKELIB

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(OBJ_NAME)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

