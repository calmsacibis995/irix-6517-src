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
# Revision 65.2  1997/12/16  17:05:50  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:20:33  jdoak
# *** empty log message ***
#
# Revision 1.1.686.1  1996/10/02  21:10:14  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:51:39  damon]
#
# Revision 1.1.681.3  1994/06/09  14:25:38  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:37:36  annie]
# 
# Revision 1.1.681.2  1994/05/20  22:03:27  sommerfeld
# 	Avoid continuous rebuild of xcred.h
# 	[1994/05/17  20:28:08  sommerfeld]
# 
# Revision 1.1.681.1  1994/02/04  20:36:18  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:20:41  devsrc]
# 
# Revision 1.1.679.1  1993/12/07  17:38:25  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  18:15:59  jaffe]
# 
# Revision 1.1.4.3  1993/09/28  19:36:32  sommerfeld
# 	Clean up bogus history comments.
# 	[1993/09/28  19:36:12  sommerfeld]
# 
# 	embedded copyright notice
# 	[1993/01/20  18:27:30  cjd]
# 
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:40:13  bolinger]
# 
# 	minor changes
# 	[1992/08/30  13:56:28  jaffe]
# 
# Revision 1.1.4.2  1993/09/28  19:31:58  sommerfeld
# 	avoid continuous rebuild of xcred.h.
# 	[1993/09/17  17:13:57  sommerfe]
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
KPATH=$(TOP)/fs/dfs/file/xcred
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libkxcred.a
OBJ_CSRCS=	xcred.c xcred_linkage.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		xcred.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

