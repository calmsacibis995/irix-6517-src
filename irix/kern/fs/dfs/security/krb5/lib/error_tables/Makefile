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
# Revision 65.4  1999/02/03 22:44:24  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.3  1998/01/16 14:42:27  gwehrman
# Added dcekdb.cat.m and dcekrb.cat.m to ILIST.
#
# Revision 65.2  1997/10/20 19:47:42  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 1.1.8.2  1996/03/09  23:29:46  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:45:07  marty]
#
# Revision 1.1.8.1  1995/12/08  17:50:58  root
# 	Submit OSF/DCE 1.2.1
# 	[1995/12/08  16:58:40  root]
# 
# Revision 1.1.6.1  1994/06/02  21:29:02  mdf
# 	hp_sec_to_osf_3 drop, merge up with latest.
# 	[1994/05/24  15:16:57  mdf]
# 
# 	hp_sec_to_osf_3 drop
# 
# Revision 1.1.4.2  1993/10/08  19:57:46  sommerfeld
# 	Remove derived objects before recreating them.
# 	[1993/10/08  19:55:58  sommerfeld]
# 
# Revision 1.1.4.2  1992/12/29  14:25:30  zeliff
# 	Embedding copyright notice
# 	[1992/12/28  21:07:11  zeliff]
# 
# Revision 1.1.2.2  1992/04/07  19:18:54  mckeen
# 	Changed compile_et to ${COMPILE_ET}. COMPILE_ET is now defined in osf.dce.mk
# 	[1992/03/13  15:46:38  mckeen]
# 
# Revision 1.1  1992/01/19  14:53:14  devrcs
# 	Initial revision
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.4 $
DEPTH?=../../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/security/krb5/lib/error_tables
.PATH:$(KPATH)

$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

HFILES=		krb5_err.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	$(INSTALL) -m 444 -F /usr/include/krb5/error_tables $(HFILES)

