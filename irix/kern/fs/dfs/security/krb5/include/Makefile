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
# Revision 65.2  1999/02/03 22:44:24  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:47:43  jdoak
# *** empty log message ***
#
# Revision 1.1.4.2  1996/03/09  23:29:37  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:44:58  marty]
#
# Revision 1.1.4.1  1995/12/08  17:40:39  root
# 	Submit OSF/DCE 1.2.1
# 	[1995/12/08  16:56:47  root]
# 
# Revision 1.1.2.2  1992/12/29  13:57:06  zeliff
# 	Embedding copyright notice
# 	[1992/12/28  20:51:18  zeliff]
# 
# Revision 1.1  1992/01/19  14:49:14  devrcs
# 	Initial revision
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.2 $
DEPTH?=../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/security/krb5/include
.PATH:$(KPATH)

HEADERS_SUBDIRS= krb5

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	${HEADERS_SUBDIRS_MAKERULE}

