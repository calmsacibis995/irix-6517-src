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
# Revision 65.2  1999/02/04 22:37:18  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:15:29  jdoak
# *** empty log message ***
#
# Revision 1.2.13.1  1996/05/10  13:08:23  arvind
# 	Drop 1 of DCE 1.2.2 to OSF
#
# 	HP revision /main/DCE_1.2/2  1996/04/09  15:10 UTC  arvind
# 	Comment out XIDL for HP800.
#
# 	HP revision /main/DCE_1.2/arvind_checkout/1  1996/04/09  15:07 UTC  arvind
# 	Comment out xidl for HP/UX
#
# 	HP revision /main/DCE_1.2/1  1996/01/03  18:49 UTC  psn
# 	Fix typo in SETUP_SUBDIR, now is set to idl_compiler.
# 	[1995/11/21  21:54 UTC  psn  /main/HPDCE02/jrr_1.2_mothra/2]
#
# 	Merge second XIDL drop for DCE 1.2.1
# 	[1995/11/17  17:00 UTC  dat  /main/dat_xidl2/1]
#
# 	fix typo
# 	[1994/12/12  18:40:02  acf]
# 	#
#
# 	visit libidlcxx directory during install pass
# 	[1994/12/12  15:47:08  acf]
#
# 	Add libidlcxx and rpclm directories.
# 	[1994/12/09  19:13:05  acf]
#
# 	DCE for DEC OSF/1 AXP: populate from OSF DCE R1.1
# 	[1994/12/09  19:11:59  acf]
#
# 	Merged from Hpdce02_01
# 	[1994/08/17  20:40 UTC  tatsu_s  /main/HPDCE02/2]
#
# 	Remove nidl_to_idl (NCS stuff no longer supported).
# 	[1994/07/15  15:54:30  tom]
# 	#
#
# 	Merged mothra up to dce 1.1.
# 	[1994/08/03  16:31 UTC  tatsu_s  /main/HPDCE02/1]
#
# 	merge kk and hpdce01
# 	[1994/01/24  19:40  lmm  /main/HPDCE01/1]
#
# 	Build a different idl during the setup pass.
# 	This is a "hack".
# 	[1993/11/18  18:59:46  sommerfeld]
# 	#
#
# 	KK merged upto DCE1_0_3b03.
#
# 	Support KRPC_ONLY.
# 	[1993/03/29  22:42:09  toml]
#
# 	Added Dean Shu's change to make nidl_to_idl build.
# 	[1992/12/22  18:23:38  jake]
#
# 	Loading drop DCE1_0_3b03
# 	[1993/09/15  15:18:38  root  1.1.10.3]
#
# 	Initial King Kong branch
# 	[1993/07/08  08:38:53  root]
#
# Revision 1.2.9.1  1994/07/15  16:02:04  tom
# 	Remove nidl_to_idl (NCS stuff no longer supported).
# 	[1994/07/15  15:54:30  tom]
# 
# Revision 1.2.7.2  1993/07/07  19:53:29  ganni
# 	reduced stub idl sources
# 	[1993/07/07  19:24:40  ganni]
# 
# Revision 1.2.5.3  1993/01/03  21:37:20  bbelch
# 	Embedding copyright notice
# 	[1993/01/03  14:31:33  bbelch]
# 
# Revision 1.2.5.2  1992/12/23  18:42:00  zeliff
# 	Embedding copyright notice
# 	[1992/12/23  00:58:32  zeliff]
# 
# Revision 1.2.2.3  1992/04/30  21:13:26  john
# 	Added SETUP_SUBDIRS.
# 	[1992/04/10  14:40:00  john]
# 
# Revision 1.2.2.2  1992/01/22  22:33:35  jim
# 	COMPAT_SUBDIRS needs to be defined before referenced.
# 	[1992/01/22  21:33:55  jim]
# 
# Revision 1.2  1992/01/19  22:13:24  devrcs
# 	Dropping DCE1.0 OSF1_misc port archive
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.2 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/rpc/idl
.PATH:$(KPATH)

HEADERS_SUBDIRS = nidl_to_idl
SUBDIRS = lib

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	${HEADERS_SUBDIRS_MAKERULE}

