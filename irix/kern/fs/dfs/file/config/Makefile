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
# Revision 65.3  1999/02/04 19:19:37  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.2  1997/12/16 17:05:32  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:21:48  jdoak
# *** empty log message ***
#
# Revision 1.1.81.1  1996/10/02  17:14:30  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:06:28  damon]
#
# Revision 1.1.75.4  1994/09/07  15:48:22  marty
# 	CR # 11497 - Remove '-v' option from IDLFLAGS defineittion.
# 	[1994/09/06  21:39:18  marty]
# 
# Revision 1.1.75.3  1994/07/13  22:19:17  devsrc
# 	merged with bl-10
# 	[1994/06/28  17:58:22  devsrc]
# 
# 	Added params.h and stds.h to ILIST
# 	[1994/03/08  21:20:01  mckeen]
# 
# Revision 1.1.75.2  1994/06/09  13:55:22  annie
# 	fixed copyright in src/file
# 	[1994/06/08  21:29:55  annie]
# 
# Revision 1.1.75.1  1994/02/04  20:09:33  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:08:16  devsrc]
# 
# Revision 1.1.73.1  1993/12/07  17:15:32  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  13:29:56  jaffe]
# 
# Revision 1.1.6.3  1993/01/18  20:53:24  cjd
# 	embedded copyright notice
# 	[1993/01/18  17:39:45  cjd]
# 
# Revision 1.1.6.2  1992/11/24  16:17:18  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  16:15:42  bolinger]
# 
# Revision 1.1.2.4  1992/06/30  21:43:13  mason
# 	Add the STUB_OPT_LEVEL variable to control optimization of stub files.
# 	[1992/06/30  21:28:25  mason]
# 
# Revision 1.1.2.3  1992/04/16  19:59:18  mckeen
# 	Removed definition of IDL
# 	[1992/04/15  18:35:12  mckeen]
# 
# Revision 1.1.2.2  1992/01/31  17:05:09  delgado
# 	Add modifications to build libdcedfs.a
# 	[1992/01/31  17:04:47  delgado]
# 
# Revision 1.1  1992/01/19  02:51:02  devrcs
# 	Initial revision
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
KPATH=$(TOP)/fs/dfs/file/config
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libkcommondata.a
OBJ_CSRCS=	local_64bit_xlate.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		common_data.h \
		common_def.h \
		debug.h \
		local_64bit_types.h \
		param.h \
		stds.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

