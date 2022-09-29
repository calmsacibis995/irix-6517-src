#!smake
#
# @OSF_COPYRIGHT@
# Copyright (c) 1993, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
# src directory for the full copyright text.
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.2  1999/02/03 21:24:52  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:15:06  jdoak
# *** empty log message ***
#
# Revision 1.1.6.2  1996/03/09  23:10:34  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:28:23  marty]
#
# Revision 1.1.6.1  1995/12/08  17:36:07  root
# 	Submit OSF/DCE 1.2.1
# 	[1995/12/08  17:17:08  root]
# 
# Revision 1.1.4.15  1994/07/22  21:39:37  pwang
# 	Added attr directory [OT#11287]
# 	[1994/07/22  21:38:16  pwang]
# 
# Revision 1.1.4.14  1994/01/25  22:53:07  bowe
# 	Moved all the libdceutils stuff down to misc directory.
# 	Now this just descends into its SUBDIRS.  (CR 9814)
# 	[1994/01/25  22:46:09  bowe]
# 
# Revision 1.1.4.13  1994/01/24  22:01:30  bowe
# 	Setup for use with ./libdce/Makefile (define libdceutils_OFILES).
# 	Removed references to db and acl files, which were moved to a
# 	directory below.  Set *SUBDIRS to visit "svc db44 acldb dcecf".
# 	[1994/01/24  21:58:47  bowe]
# 
# Revision 1.1.4.12  1994/01/14  21:03:41  rsalz
# 	Remove dependency on dceacl.h since it is not yet build (OT CR 9731)
# 	[1994/01/14  21:03:25  rsalz]
# 
# Revision 1.1.4.11  1994/01/14  18:53:52  bowe
# 	Added dce_priv_proc.h to INCLUDE list.
# 	[1994/01/14  18:35:23  bowe]
# 
# Revision 1.1.4.10  1994/01/06  19:43:56  rsalz
# 	Comment out database stuff temporarily (OT CR 9663).
# 	[1994/01/06  19:43:46  rsalz]
# 
# Revision 1.1.4.9  1994/01/06  18:36:21  rsalz
# 	Undo 1.2.32.10 change (for now) for OT CR 9663.
# 	[1994/01/06  18:31:57  rsalz]
# 
# Revision 1.1.4.8  1994/01/06  13:51:17  rsalz
# 	Last fix for OT CR 9663 (I promise!... I hope.)
# 	[1994/01/06  13:51:01  rsalz]
# 
# Revision 1.1.4.7  1994/01/05  21:08:36  rsalz
# 	More OT CR 9663 changes.
# 	[1994/01/05  21:08:24  rsalz]
# 
# Revision 1.1.4.6  1994/01/05  20:39:35  rsalz
# 	More OT CR 9663 changes.
# 	[1994/01/05  20:39:27  rsalz]
# 
# Revision 1.1.4.5  1994/01/05  18:32:40  rsalz
# 	Yet more build-order fixes (OT CR 9663)
# 	[1994/01/05  18:32:22  rsalz]
# 
# Revision 1.1.4.4  1994/01/03  19:35:03  rsalz
# 	Add SETUP_SUBDIRS (part of OT CR 9663).
# 	[1994/01/03  18:19:39  rsalz]
# 
# Revision 1.1.4.3  1993/12/29  19:22:25  rsalz
# 	Move dce_cf_XXX from config to dce/utils/dcecf (OT CR 9663).
# 	[1993/12/29  16:00:47  rsalz]
# 
# Revision 1.1.4.2  1993/12/20  13:43:53  bowe
# 	Added provisions for new files: aclif.h, dceacl.acf, dceacl.idl, and
# 	dceaclimpl.c.
# 	[1993/12/17  22:39:18  bowe]
# 
# Revision 1.1.4.1  1993/12/14  16:57:53  mario
# 	Changes required to get svc built in libdce
# 	[1993/12/14  16:57:20  mario]
# 
# Revision 1.1.2.6  1993/12/10  00:04:07  melman
# 	Made editline.h a private editlinep.h and added a public editline.h
# 	[1993/12/10  00:03:50  melman]
# 
# Revision 1.1.2.5  1993/12/03  21:05:44  bowe
# 	Changed SUBDIRS to EXPLIB_SUBDIRS.
# 	[1993/12/03  21:05:31  bowe]
# 
# Revision 1.1.2.4  1993/12/02  21:51:22  bowe
# 	Added provisions for database/backing-store files, including sending .a
# 	files and .h files to the right places in the export tree and descending
# 	the 4.4 bsd ("db44") subdirectory.
# 	[1993/12/02  21:51:10  bowe]
# 
# Revision 1.1.2.3  1993/08/26  12:37:55  melman
# 	Renamed sec_priv_* to dce_priv_*
# 	[1993/08/26  12:37:15  melman]
# 
# Revision 1.1.2.2  1993/07/14  19:08:54  melman
# 	Initial version
# 	[1993/07/14  18:49:21  melman]
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
KPATH=$(TOP)/fs/dfs/dce/utils
.PATH:$(KPATH)

HEADERS_SUBDIRS= svc dcecf

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	${HEADERS_SUBDIRS_MAKERULE}

