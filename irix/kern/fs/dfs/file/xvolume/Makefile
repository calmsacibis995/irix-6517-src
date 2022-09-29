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
# Revision 65.4  1999/02/04 19:19:46  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.3  1998/03/16 15:37:21  gwehrman
# Cleanup ld warnings.
#
# Revision 65.2  1997/12/16 17:05:51  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:21:46  jdoak
# *** empty log message ***
#
# Revision 1.1.132.1  1996/10/02  19:04:23  damon
# 	New DFS from Transarc
# 	[1996/10/01  18:59:51  damon]
#
# Revision 1.1.127.2  1994/06/09  14:26:38  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:38:28  annie]
# 
# Revision 1.1.127.1  1994/02/04  20:37:31  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:21:14  devsrc]
# 
# Revision 1.1.125.1  1993/12/07  17:39:20  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  18:23:29  jaffe]
# 
# Revision 1.1.5.4  1993/01/21  16:36:11  cjd
# 	embedded copyright notice
# 	[1993/01/20  18:30:03  cjd]
# 
# Revision 1.1.5.3  1992/11/24  20:46:13  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:42:19  bolinger]
# 
# Revision 1.1.5.2  1992/11/20  20:21:39  rsarbo
# 	volclient needs to link libdce rather than
# 	libafsys on PMAX
# 	[1992/11/20  19:31:46  rsarbo]
# 
# Revision 1.1.3.6  1992/05/20  20:22:39  mason
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
# 	[1992/05/20  17:27:50  mason]
# 
# Revision 1.1.3.5  1992/04/14  03:52:50  mason
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
# 	[1992/04/13  18:14:32  mason]
# 
# Revision 1.1.3.4  1992/01/25  20:51:33  zeliff
# 	dfs6.3 merge, part2
# 	[1992/01/25  20:11:46  zeliff]
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.4 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/xvolume
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libvolume.a
OBJ_CSRCS=	vol_init.c vol_desc.c vol_misc.c vol_attvfs.c vol_linkage.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		volume.h vol_init.h vol_errs.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

