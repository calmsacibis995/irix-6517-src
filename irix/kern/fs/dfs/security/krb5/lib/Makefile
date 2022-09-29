#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
# src directory for the full copyright text.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.2  1999/02/03 22:44:24  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:47:04  jdoak
# *** empty log message ***
#
# Revision 1.1.9.1  1996/05/10  13:19:14  arvind
# 	Drop 1 of DCE 1.2.2 to OSF
#
# 	HP revision /main/DCE_1.2/2  1996/03/14  19:20 UTC  psn
# 	Merge OSF endgame fixes to HP.
#
# 	HP revision /main/DCE_1.2/psn_1.2_endgame/1  1996/02/28  22:07 UTC  psn
# 	Fix OT13344 as done during OSF 1.2.1 Endgame.
#
# 	HP revision /main/DCE_1.2/1  1996/01/03  19:13 UTC  psn
# 	CHFts16048: Add crypto regression tests..
# 	[1995/08/17  22:02 UTC  sommerfeld  /main/sommerfeld_mothpatch/1]
#
# Revision 1.1.5.2  1992/12/29  14:19:09  zeliff
# 	Embedding copyright notice
# 	[1992/12/28  20:55:24  zeliff]
# 
# Revision 1.1.2.3  1992/07/08  22:29:39  sekhar
# 	Removed destoo from EXPINC_SUBDIRS because destoo and desneuter
# 	directories are now replaced by crypto/common,domestic,international
# 	[1992/07/08  20:09:23  sekhar]
# 
# Revision 1.1.2.2  1992/04/01  19:14:40  mhickey
# 	Modify the assignment for EXPINC_SUBDIRS to make visiting
# 	the destoo directory conditional on the setting of the
# 	make variable USE_DES (set in Makeconf, if appropriate).
# 
# 	This fixes defect 2065.
# 	[1992/03/31  16:56:40  mhickey]
# 
# Revision 1.1  1992/01/19  14:53:44  devrcs
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
KPATH=$(TOP)/fs/dfs/security/krb5/lib
.PATH:$(KPATH)

HEADERS_SUBDIRS= error_tables

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	${HEADERS_SUBDIRS_MAKERULE}

