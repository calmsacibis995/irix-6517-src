#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
# src directory for the full copyright text.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.3  1999/02/04 22:37:18  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.2  1997/10/20 19:15:30  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 1.2.15.2  1996/03/09  23:25:52  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:41:33  marty]
#
# Revision 1.2.15.1  1995/12/07  22:23:37  root
# 	Submit OSF/DCE 1.2.1
# 
# 	HP revision /main/dat_xidl2/1  1995/11/17  17:05 UTC  dat
# 	Merge second XIDL drop for DCE 1.2.1
# 	[1995/12/07  21:13:35  root]
# 
# Revision 1.2.12.2  1994/08/18  22:09:26  hopkins
# 	Serviceability:
# 	  Set INCFLAGS for ctxeecli.o
# 	  Add -DDCE_RPC_SVC to CFLAGS
# 	[1994/08/18  21:47:09  hopkins]
# 
# Revision 1.2.12.1  1994/05/02  22:43:34  tom
# 	From DEC: Add binding callout function.
# 	 Add bindcall.o to user_ofiles.
# 	[1994/05/02  21:06:29  tom]
# 
# Revision 1.2.8.2  1993/07/07  20:04:14  ganni
# 	reduced stub idl sources
# 	[1993/07/07  19:35:07  ganni]
# 
# Revision 1.2.6.3  1993/01/03  22:11:56  bbelch
# 	Embedding copyright notice
# 	[1993/01/03  14:38:39  bbelch]
# 
# Revision 1.2.6.2  1992/12/23  19:08:43  zeliff
# 	Embedding copyright notice
# 	[1992/12/23  01:06:01  zeliff]
# 
# Revision 1.2.2.2  1992/05/11  17:51:08  mhickey
# 	Removed obsolete define of NEED_PROJECT_MAKE_DEFS
# 	and include of osf.dce.mk.  Fixes defect 2031.
# 	[1992/04/22  15:21:51  mhickey]
# 
# Revision 1.2  1992/01/19  22:13:29  devrcs
# 	Dropping DCE1.0 OSF1_misc port archive
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.3 $
DEPTH?=../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/rpc/idl/lib
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(KPATH)/../../runtime \
		-I$(ROOT)/usr/include \
		-I$(ROOT)/usr/include/dcedfs

KCDEFS +=	-DIDL_LIB_FLAG -DMIA

OBJ_NAME=	libkidl.a
OBJ_CSRCS=	alfrsupp.c \
		allocate.c \
		ctxeecli.c \
		ctxeectx.c \
		ctxerrtl.c \
		eenodtbl.c \
		ernodtbl.c \
		marbfman.c \
		ndrchars.c \
		nbaseool.c \
		ndrcharp.c \
		nidlalfr.c \
		pipesupp.c \
		sscmaset.c \
		sscmasrv.c \
		ndrui.c \
		ndrui2.c \
		ndrui3.c \
		ndrmi.c \
		ndrmi2.c \
		ndrmi3.c \
		interpsh.c \
		pickling.c

OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!

