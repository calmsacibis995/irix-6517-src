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
# Revision 65.4  1999/05/04 19:19:32  gwehrman
# Added hyper.h to the list of HFILES exported.  Fix for bug 691629.
#
# Revision 65.3  1999/02/04 19:19:45  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.2  1997/12/16 17:05:48  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:19:22  jdoak
# *** empty log message ***
#
# Revision 1.1.922.1  1996/10/02  21:13:09  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:50:59  damon]
#
# Revision 1.1.917.2  1994/06/09  14:24:41  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:36:48  annie]
# 
# Revision 1.1.917.1  1994/02/04  20:35:33  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:20:17  devsrc]
# 
# Revision 1.1.915.1  1993/12/07  17:37:44  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  18:08:17  jaffe]
# 
# Revision 1.1.6.3  1993/01/21  16:31:03  cjd
# 	embedded copyright notice
# 	[1993/01/20  18:24:38  cjd]
# 
# Revision 1.1.6.2  1992/11/24  20:42:34  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:38:17  bolinger]
# 
# Revision 1.1.2.2  1992/01/31  16:27:15  delgado
# 	Add modifications for building libdcedfs.a
# 	[1992/01/31  16:26:33  delgado]
# 
# Revision 1.1  1992/01/19  02:45:42  devrcs
# 	Initial revision
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
KPATH=$(TOP)/fs/dfs/file/util
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libkafsutil.a
OBJ_CSRCS=	hash.c hyper.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		hash.h hyper.h ktime.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

