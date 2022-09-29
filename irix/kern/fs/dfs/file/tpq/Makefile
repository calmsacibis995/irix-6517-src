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
# Revision 65.4  1999/02/04 19:19:45  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.3  1998/03/16 15:37:20  gwehrman
# Cleanup ld warnings.
#
# Revision 65.2  1997/12/16  17:05:45  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:18:10  jdoak
# *** empty log message ***
#
# Revision 1.1.121.1  1996/10/02  18:48:47  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:49:19  damon]
#
# Revision 1.1.116.3  1994/06/09  14:23:04  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:34:40  annie]
# 
# Revision 1.1.116.2  1994/02/04  20:33:12  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:19:15  devsrc]
# 
# Revision 1.1.115.1  1993/12/07  17:35:59  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  17:19:52  jaffe]
# 
# Revision 1.1.2.5  1993/01/21  15:55:55  cjd
# 	embedded copyright notice
# 	[1993/01/20  17:39:27  cjd]
# 
# Revision 1.1.2.4  1992/11/24  19:48:06  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:32:21  bolinger]
# 
# Revision 1.1.2.3  1992/09/25  19:43:23  jaffe
# 	Sync with Transarc
# 	[1992/09/23  20:52:11  jaffe]
# 
# 	Transarc delta: comer-remove-lwpdummy 1.1
# 	  Selected comments:
# 	    Tests in the tpq directory were linking with the lwpdummy
# 	    library but were not using any symbols from it.  -llwpdummy
# 	    was removed from the Makefile.
# 	    Remove unneeded references to lwpdummp by test programs.
# 	    TESTING
# 	    This change was tested by compiling and running the test
# 	    programs on both a RIOS and a PMAX under dfs-102-1.49.
# 	[1992/09/23  19:56:29  jaffe]
# 
# Revision 1.1.2.2  1992/08/31  21:33:37  jaffe
# 	Transarc delta: comer-ot3947-multithread-cm-daemon 1.25
# 	[1992/08/30  13:08:08  jaffe]
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
KPATH=$(TOP)/fs/dfs/file/tpq
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libktpq.a
OBJ_CSRCS=	tpq_die.c \
		tpq_dispatch.c \
		tpq_helper.c \
		tpq_init.c \
		tpq_queue.c \
		tpq_access.c \
		tpq_stat.c \
		tpq_pardo.c \
		tpq_parset.c \
		tpq_linkage.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		tpq.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

