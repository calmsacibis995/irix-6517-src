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
# Revision 65.7  1999/02/04 19:19:45  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.6  1998/03/02 20:00:22  gwehrman
# Changed libufsops.a_EXPLIB defintion to the proper name libufsops.a_EXPDIR.
#
# Revision 65.5  1998/02/27 17:32:00  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.4  1998/02/27 16:15:59  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.3  1998/02/26 21:55:47  lmc
# Changes for SGIMIPS for multiple defines and install list including cat.m files.
#
# Revision 65.1  1997/10/20  19:20:28  jdoak
# *** empty log message ***
#
# Revision 1.1.538.1  1996/10/02  18:49:40  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:49:41  damon]
#
# Revision /main/HPDCE02/brezak_transarc/1  1995/06/01  22:07 UTC  brezak
# 	create msgcat for ufs_trace
# 
# Revision /main/HPDCE02/1  1994/06/29  17:33 UTC  kinney
# 	Merge hpdce02 (DCE 1.1 beta) and HPDCE01 into HPDCE02
# 
# 	  Revision /main/HPDCE01/1  1994/05/19  17:09 UTC  rmm
# 	merge +e mod
# 
# 	  Revision /main/rmm_rodan_work/1  1994/05/19  17:07 UTC  rmm
# 	add +e option for osi_fio.c to work around dirent.h changes
# 
# Revision 1.1.532.3  1994/06/09  14:23:26  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:35:14  annie]
# 
# Revision 1.1.532.2  1994/02/04  20:33:48  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:19:31  devsrc]
# 
# Revision 1.1.531.1  1993/12/07  17:36:25  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  17:40:36  jaffe]
# 
# Revision 1.1.2.4  1993/02/24  21:56:54  bolinger
# 	OT defect 3542 (OSF/1 volops support):  Look in TARGET_OS
# 	directory for a Makefile.
# 	[1993/02/24  21:39:21  bolinger]
# 
# Revision 1.1.2.3  1993/01/28  18:14:27  shl
# 	Transarc delta: cburnett-ot6781-aix-jfs-dump-restore-support 1.3
# 	  Selected comments:
# 	    Add AIX JFS support to ufsops for the new portable directory
# 	    dump/restore format.  Also fix some existing bugs in the
# 	    ufsops code.  In addition push AIX specific function into
# 	    a platform specific dir.
# 	    Look for platform specific Makefile.
# 	    The first revision of delta didn't compile under OSF/1.
# 	    We need one more round to get this to compile under OSF/1.
# 	[1993/01/27  21:04:14  shl]
# 
# Revision 1.1.2.2  1993/01/21  15:56:53  cjd
# 	embedded copyright notice
# 	[1993/01/20  17:41:08  cjd]
# 
# Revision 1.1  1992/01/19  02:57:34  devrcs
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
KPATH=$(TOP)/fs/dfs/file/ufsops
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libufsops.a
OBJ_CSRCS=	ufs_agops.c ufs_volops.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		ufs_trace.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

