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
# Revision 65.5  1999/02/04 19:19:36  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.4  1998/12/04 17:26:37  bobo
# O32 build changes
#
# Revision 65.3  1998/03/04  21:55:20  gwehrman
# Changed definition of bomb_LIBS and bomb_test_LIBS to fix unresolved symbols.
#
# Revision 65.2  1997/12/16 17:05:31  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:18:17  jdoak
# *** empty log message ***
#
# Revision 1.1.46.1  1996/10/02  17:03:09  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:04:37  damon]
#
# Revision 1.1.40.2  1994/06/09  13:51:40  annie
# 	fixed copyright in src/file
# 	[1994/06/08  21:25:12  annie]
# 
# Revision 1.1.40.1  1994/02/04  20:05:52  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:06:27  devsrc]
# 
# Revision 1.1.38.1  1993/12/07  17:12:37  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  12:53:15  jaffe]
# 
# Revision 1.1.2.3  1993/06/09  21:22:29  zeliff
# 	OT 8132: provide IDIR for libbomb, bomb.h
# 	[1993/06/09  20:06:20  zeliff]
# 
# Revision 1.1.2.2  1993/05/11  12:43:52  jaffe
# 	Transarc delta: jdp-db3358-add-bomb-point-utilities 1.5
# 	  Selected comments:
# 	    This delta adds a bomb point (error injection) package to DFS.  The
# 	    README file in src/file/bomb describes the package in detail.
# 	    Builds the DFS bomb package.
# 	    Made a variety of improvements like including the proper copyright/history
# 	    header at the top of each file, fixing initialization problems,
# 	    changing the delay and wait bomb points to affect the thread instead
# 	    of the process, and making bomb points cheaper when none are set to go off.
# 	    Add header.
# 	    Fix an oversight that caused compilation errors.
# 	    Integrate the package into the big build.
# 	    Install bomb_set in /opt/dcelocal/bin/.
# 	[1993/05/10  16:31:43  jaffe]
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.5 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/bomb
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

KCDEFS+=	-DDFS_BOMB

OBJ_NAME=	libkbomb.a
OBJ_CSRCS=	bomb_core.c bomb_syscall.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		bomb.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

