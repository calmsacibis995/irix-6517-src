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
# Revision 65.3  1997/10/24 20:11:51  gwehrman
# Changed method used for including des_inline.h in the list of headers.
#
# Revision 65.2  1997/10/21  15:49:05  tdm
# Included des_inline.h in the list of headersto be moved to include directory.
#
# Revision 65.1  1997/10/20  19:47:44  jdoak
# *** empty log message ***
#
# Revision 1.1.9.2  1996/03/09  23:29:38  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:44:59  marty]
#
# Revision 1.1.9.1  1995/12/08  17:41:06  root
# 	Submit OSF/DCE 1.2.1
# 	[1995/12/08  16:56:51  root]
# 
# Revision 1.1.7.1  1994/05/11  19:15:02  ahop
# 	hp_sec_to_osf_2 drop
# 	[1994/04/29  21:11:13  ahop]
# 
# Revision 1.1.5.2  1992/12/29  13:57:39  zeliff
# 	Embedding copyright notice
# 	[1992/12/28  20:51:37  zeliff]
# 
# Revision 1.1.3.2  1992/06/01  20:43:04  sekhar
# 	Drop the changes made by Joe Pato for des code
# 	restructuring.
# 	[1992/06/01  19:53:50  sekhar]
# 
# Revision 1.1.1.2  1992/05/14  17:16:01  pato
# 	Add des_int.h to exported set (OT#2330)
# 
# Revision 1.1  1992/01/19  14:49:09  devrcs
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
KPATH=$(TOP)/fs/dfs/security/krb5/include/krb5
.PATH:$(KPATH)

HFILES=		base-defs.h wordsize.h osf_dce.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	$(INSTALL) -m 444 -F /usr/include/krb5 $(HFILES)

