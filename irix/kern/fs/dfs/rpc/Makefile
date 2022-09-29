#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
# src directory for the full copyright text.
#
# (c) Copyright 1991, 1992 
#     Siemens-Nixdorf Information Systems, Burlington, MA, USA
#     All Rights Reserved
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.5  1999/05/21 18:53:49  lauriec
# Moved actual building of some of the sources, so the SUBDIRS changed.
#
# Revision 65.4  1999/03/02 21:09:33  gwehrman
# Add kdes to HEADERS_SUBDIRS.
#
# Revision 65.3  1999/02/23 15:24:09  gwehrman
# Removed kdes from the list of SUBDIRS.  The stubs for the DES code
# are now built in irix/kern/stubs.
#
# Revision 65.2  1999/02/04 22:37:18  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:15:29  jdoak
# *** empty log message ***
#
# Revision 1.2.16.2  1996/03/09  23:25:37  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:41:19  marty]
#
# Revision 1.2.16.1  1995/12/07  22:11:44  root
# 	Submit OSF/DCE 1.2.1
# 	[1995/12/07  21:09:17  root]
# 
# Revision 1.2.14.4  1994/09/16  16:03:47  rsalz
# 	Stop building rpcd (OT CR 11488).
# 	[1994/09/15  19:39:06  rsalz]
# 
# Revision 1.2.14.3  1994/05/23  23:13:28  sommerfeld
# 	On instructions from rsalz and as a favor to Mariko, add csrc to
# 	src/rpc/Makefile's SAMIDL_SUBDIR
# 	[1994/05/23  20:16:06  sommerfeld]
# 
# 	Add SAMIDL_SUBDIRS for svc.
# 	[1994/05/23  19:28:54  sommerfeld]
# 
# Revision 1.2.14.2  1994/05/19  21:14:09  hopkins
# 	Serviceability work.
# 	[1994/05/19  02:24:31  hopkins]
# 
# 	Serviceability:
# 	  Add sys_idl before ${KEXPINC_SUBDIRS}
# 	  in EXPINC_SUBDIRS for KRPC_ONLY case.
# 	  kruntime depends on svc headers generated
# 	  in sys_idl build.
# 	[1994/05/18  21:32:46  hopkins]
# 
# Revision 1.2.14.1  1994/02/08  21:48:41  mori_m
# 	CR 9701:  Initial submission for RPC runtime I18N support
# 	[1994/02/08  21:48:28  mori_m]
# 
# Revision 1.2.12.2  1993/07/19  19:38:05  zeliff
# 	HP port of DFS
# 	[1993/07/19  18:29:15  zeliff]
# 
# Revision 1.2.10.3  1993/07/16  21:01:34  kissel
# 	*** empty log message ***
# 	[1993/06/21  14:45:53  kissel]
# 
# Revision 1.1.8.2  1993/04/15  15:35:58  toml
# 	KRPC_ONLY support.
# 	[1993/04/15  15:29:42  toml]
# 
# Revision 1.2.5.5  1993/02/01  20:27:23  hinman
# 	[hinman@sni] - Final merge before bsubmit
# 	[1993/01/31  15:59:20  hinman]
# 
# Revision 1.2.5.4  1993/01/03  21:37:18  bbelch
# 	Embedding copyright notice
# 	[1993/01/03  14:31:29  bbelch]
# 	Revision 1.2.6.2  1993/01/11  16:10:41  hinman
# 	[hinman] - Check in merged SNI version
# 
# Revision 1.2.5.3  1992/12/23  18:41:54  zeliff
# 	Embedding copyright notice
# 	[1992/12/23  00:58:29  zeliff]
# 
# Revision 1.2.5.2  1992/09/29  21:12:31  devsrc
# 	SNI/SVR4 merge.  OT 5373
# 	[1992/09/11  15:17:52  weisman]
# 
# Revision 9.5.4.2  93/01/05  10:16:32  greg
# 	Fold in new OSF copyright.
# 
# Revision 9.5.1.4  92/12/10  13:04:59  greg
# 	fix up for merging back to osf
# 
# Revision 9.5.3.2  92/12/10  12:18:41  greg
# 	uncomment some code and put it inside a .if (${TARGET_MACHINE} != "SVR4")
# 	statement so we can ship this back to OSF.
# 
# Revision 9.5.1.3  92/10/01  11:08:12  greg
# 	Resolve differences between the osf aand SNI version of merged files.
# 	Differences are mainly due to the "mips" preprocessor symbol (osf
# 	did not accept our changes as part of the merge) and the existence
# 	of proprietary licensing code.
# 
# Revision 9.5.2.3  92/09/30  16:00:50  greg
# 	add SNI copyright info
# 
# Revision 9.5.2.2  92/08/18  14:53:34  greg
# 	hacks to build kernel subdir
# 
# Revision 9.5.1.1  92/07/24  15:50:52  hinman
# 	Create merge branch
# 
# Revision 9.5  92/07/24  15:50:49  hinman
# 	Update checkin
# 
# Revision 1.2.2.2  1992/04/30  21:12:33  john
# 	Added SETUP_SUBDIRS.
# 	[1992/04/10  14:39:05  john]
# 
# Revision 1.2  1992/01/19  22:13:24  devrcs
# 	Dropping DCE1.0 OSF1_misc port archive
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.5 $
DEPTH?=../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/rpc
.PATH:$(KPATH)

SUBDIRS= idl kruntime

HEADERS_SUBDIRS= sys_idl idl kruntime kdes

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	${HEADERS_SUBDIRS_MAKERULE}

