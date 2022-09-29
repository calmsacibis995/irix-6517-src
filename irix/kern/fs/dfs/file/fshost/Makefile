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
# Revision 65.7  1999/02/04 19:19:38  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.6  1998/02/27 17:31:51  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.5  1998/02/27 16:15:53  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:54:34  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.1  1997/10/20  19:19:37  jdoak
# *** empty log message ***
#
# Revision 1.1.81.1  1996/10/02  17:46:29  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:37:20  damon]
#
# Revision 1.1.76.3  1994/08/23  18:58:33  rsarbo
# 	delegation
# 	[1994/08/23  16:45:01  rsarbo]
# 
# Revision 1.1.76.2  1994/06/09  14:07:27  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:22:30  annie]
# 
# Revision 1.1.76.1  1994/02/04  20:18:15  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:12:51  devsrc]
# 
# Revision 1.1.74.1  1993/12/07  17:23:48  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  15:18:56  jaffe]
# 
# Revision 1.1.6.4  1993/01/21  19:34:51  zeliff
# 	Embedding copyright notices
# 	[1993/01/19  19:48:29  zeliff]
# 
# Revision 1.1.6.3  1993/01/13  17:30:47  shl
# 	Transarc delta: jaffe-cleanup-2.3-compiler-warnings 1.8
# 	  Selected comments:
# 	    Fix compiler warnings found when moving to OSF/1 1.1.1.
# 	    Remove the function dacl_SetOwnerControl.  It is not used.
# 	    install dfszhs.cat into the NLS directory, not the include dir.
# 	    fix typo here.
# 	Transarc delta: tu-ot5849a-ot5849b-ot6048-super-delta 1.2
# 	  Selected comments:
# 	    This super-delta was created from configuration dfs-102, revision 2.2,
# 	    by importing the following deltas:
# 	    tu-ot5849-add-icl-fshost 1.2
# 	    tu-ot5849-resolve-merge-conflict 1.1
# 	    tu-ot6048-deadlock-primary-sec-service 1.3
# 	    fix some merge problems
# 	[1993/01/12  20:49:52  shl]
# 
# Revision 1.1.6.2  1992/11/24  17:02:49  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  17:58:08  bolinger]
# 
# Revision 1.1.3.6  1992/05/20  19:51:39  mason
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
# 	    Add dependency to build .msf file.
# 	[1992/05/20  11:14:35  mason]
# 
# Revision 1.1.3.5  1992/04/14  04:08:38  mason
# 	Transarc delta: bab-ot2394-change-catalog-names 1.3
# 	  Files modified:
# 	    bak: Makefile; bakserver: Makefile; bosserver: Makefile
# 	    bubasics: Makefile; flserver: Makefile; fshost: Makefile
# 	    fsint: Makefile; ftserver: Makefile; host: Makefile
# 	    ncscompat: Makefile; ncsubik: Makefile
# 	    security/dacl: Makefile; security/dfsauth: Makefile
# 	    tkm: Makefile; tools/cmd: Makefile; update: Makefile
# 	    xcred: Makefile; xvolume: Makefile
# 	  Selected comments:
# 	    The dfs error catalogs were generated with names that made them
# 	    dce_error_inq_text unable to locate them. (dce_error_inq_text pulls
# 	    a facility code out of the error code and uses this to construct a
# 	    filename in which to look for the error message.  Our (dfs) facility
# 	    code is such that our error catalog filenames must start with "dfs",
# 	    rather then "dce".)
# 	    Change occurrences of "dce...\.cat" to "dfs...\.cat"
# 	    ot 2394
# 	    The sed script missed one occurrence of "dfs...\.cat"
# 	    the first time through because it was hidden by another
# 	    occurrence earlier on the line.
# 	[1992/04/13  16:03:27  mason]
# 
# Revision 1.1.3.4  1992/01/25  20:47:37  zeliff
# 	dfs6.3 merge, part2
# 	[1992/01/25  20:05:24  zeliff]
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
KPATH=$(TOP)/fs/dfs/file/fshost
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libhost.a
OBJ_CSRCS=	fshs_host.c \
		fshs_hostops.c \
		fshs_princ.c \
		fshs_prutils.c \
		fshs_subr.c \
		fshs_deleg.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		fshs.h \
		fshs_deleg.h \
		fshs_errs.h \
		fshs_host.h \
		fshs_princ.h \
		fshs_trace.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

