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
# Revision 65.6  1998/02/27 17:32:01  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.5  1998/02/27 16:16:00  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:54:50  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.1  1997/10/20  19:21:47  jdoak
# *** empty log message ***
#
# Revision 1.1.428.1  1996/10/02  21:13:26  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:51:10  damon]
#
# Revision 1.1.423.2  1994/06/09  14:25:10  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:37:08  annie]
# 
# Revision 1.1.423.1  1994/02/04  20:35:41  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:20:23  devsrc]
# 
# Revision 1.1.421.1  1993/12/07  17:37:52  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  18:09:58  jaffe]
# 
# Revision 1.1.3.4  1993/01/21  16:32:09  cjd
# 	embedded copyright notice
# 	[1993/01/20  18:25:50  cjd]
# 
# Revision 1.1.3.3  1993/01/13  19:51:21  shl
# 	Transarc delta: jdp-ot6444-add-kernel-debugging-to-more-layers 1.1
# 	  Selected comments:
# 	    This delta allows for local kernel debugging of xaggr, xvolume, and volreg.
# 	    Add $volreg_KERNELCFLAGS to CFLAGS definition.
# 	[1993/01/12  22:32:53  shl]
# 
# Revision 1.1.3.2  1992/11/24  20:43:12  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:38:49  bolinger]
# 
# Revision 1.1  1992/01/19  02:57:40  devrcs
# 	Initial revision
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.7 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/volreg
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libvolreg.a
OBJ_CSRCS=	volreg.c volreg_linkage.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		volreg.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

