#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.7  1999/02/04 19:19:44  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.6  1998/02/27 17:31:57  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.5  1998/02/27 16:15:56  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:54:42  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.1  1997/10/20  19:20:41  jdoak
# *** empty log message ***
#
# Revision 1.1.730.1  1996/10/02  21:00:58  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:47:56  damon]
#
# Revision 1.1.725.2  1994/06/09  14:20:16  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:31:50  annie]
# 
# Revision 1.1.725.1  1994/02/04  20:30:05  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:18:03  devsrc]
# 
# Revision 1.1.723.1  1993/12/07  17:33:42  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  16:57:45  jaffe]
# 
# Revision 1.1.6.4  1993/01/21  15:20:38  cjd
# 	embedded copyright notice
# 	[1993/01/20  15:59:43  cjd]
# 
# Revision 1.1.6.3  1993/01/13  19:15:15  shl
# 	Transarc delta: jaffe-cleanup-2.3-compiler-warnings 1.8
# 	  Selected comments:
# 	    Fix compiler warnings found when moving to OSF/1 1.1.1.
# 	    Remove the function dacl_SetOwnerControl.  It is not used.
# 	    Clean out unused files.
# 	[1993/01/12  21:57:47  shl]
# 
# Revision 1.1.6.2  1992/11/24  19:41:31  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:25:28  bolinger]
# 
# Revision 1.1.3.5  1992/05/20  20:04:52  mason
# 	Transarc delta: cfe-ot3085-fix-msf-dependencies 1.1
# 	  Files modified:
# 	    bak: Makefile; bakserver: Makefile; bosserver: Makefile
# 	    bubasics: Makefile; flserver: Makefile
# 	    flserver.klib: Makefile; fshost: Makefile; fsint: Makefile
# 	    ftserver: Makefile; host: Makefile; ncscompat: Makefile
# 	    ncsubik: Makefile; security/dacl: Makefile
# 	    security/dfsauth: Makefile; sysacl: Makefile; tkm: Makefile
# 	    tools/cmd: Makefile; update: Makefile; xcred: Makefile
# 	    xvolume: Makefile
# 	  Selected comments:
# 	    Make explicit the dependency that builds an .msf file from an .et file.
# 	    Would have added a dependency if there were an installed .cat file.
# 	[1992/05/20  12:00:09  mason]
# 
# Revision 1.1.3.4  1992/01/25  20:49:38  zeliff
# 	dfs6.3 merge, part2
# 	[1992/01/25  20:08:36  zeliff]
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
KPATH=$(TOP)/fs/dfs/file/sysacl
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libkolddacl.a
OBJ_CSRCS=	xvnops_call.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		aclint.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

