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
# Revision 65.3  1999/02/04 19:19:41  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.2  1997/12/16 17:05:35  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:19:21  jdoak
# *** empty log message ***
#
# Revision 1.1.78.1  1996/10/02  17:51:40  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:40:27  damon]
#
# Revision 1.1.73.2  1994/06/09  14:10:51  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:24:57  annie]
# 
# Revision 1.1.73.1  1994/02/04  20:20:46  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:14:04  devsrc]
# 
# Revision 1.1.71.1  1993/12/07  17:26:39  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  15:35:14  jaffe]
# 
# Revision 1.1.6.3  1993/01/19  16:04:27  cjd
# 	embedded copyright notice
# 	[1993/01/19  14:14:25  cjd]
# 
# Revision 1.1.6.2  1992/11/24  17:53:07  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  18:06:35  bolinger]
# 
# Revision 1.1.3.6  1992/05/20  19:55:56  mason
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
# 	[1992/05/20  11:37:56  mason]
# 
# Revision 1.1.3.5  1992/04/14  04:08:56  mason
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
# 	[1992/04/13  16:06:09  mason]
# 
# Revision 1.1.3.4  1992/01/25  20:48:04  zeliff
# 	dfs6.3 merge, part2
# 	[1992/01/25  20:06:11  zeliff]
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.3 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/host
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	
OBJ_CSRCS=	
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		hs.h hs_errs.h hs_host.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

