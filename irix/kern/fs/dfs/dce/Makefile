#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
# 
# HISTORY
# $Log: Makefile,v $
# Revision 65.3  1999/02/04 23:15:10  mek
# Makefile tuning.
#
# Revision 65.2  1999/02/03 21:24:52  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:15:05  jdoak
# *** empty log message ***
#
# Revision 1.1.8.1  1996/06/04  21:44:50  arvind
# 	fix compile error (dce subtree not visited during AUTOGEN pass..)
# 	[1996/03/09  23:10:33  marty  1.1.6.2]
#
# Revision 1.1.6.2  1996/03/09  23:10:33  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:28:22  marty]
# 
# Revision 1.1.6.1  1995/12/08  17:36:04  root
# 	Submit OSF/DCE 1.2.1
# 	[1995/12/08  17:17:08  root]
# 
# Revision 1.1.4.5  1994/06/09  16:05:37  devsrc
# 	cr10892 - fix copyright
# 	[1994/06/09  15:50:15  devsrc]
# 
# Revision 1.1.4.4  1994/04/19  13:21:32  bowe
# 	Added SUBDIRS = utils
# 	[1994/04/18  18:18:56  bowe]
# 
# Revision 1.1.4.3  1994/02/01  21:33:41  bowe
# 	Defined ILIST, IDIR, and IMODE.  (dce.h was not being exported)
# 	[1994/02/01  20:47:12  bowe]
# 
# Revision 1.1.4.2  1994/01/03  19:35:02  rsalz
# 	Add SETUP_SUBDIRS (part of OT CR 9663).
# 	[1994/01/03  19:31:37  rsalz]
# 
# Revision 1.1.4.1  1993/12/14  16:57:51  mario
# 	Changes required to get svc built in libdce
# 	[1993/12/14  16:57:19  mario]
# 
# Revision 1.1.2.3  1993/12/09  23:40:34  melman
# 	Made dce.h machine specific
# 	[1993/12/09  23:40:17  melman]
# 
# Revision 1.1.2.2  1993/07/14  19:08:44  melman
# 	Initial version
# 	[1993/07/14  18:49:15  melman]
# 
# $EndLog$
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.3 $
DEPTH?=../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/dce
.PATH:$(KPATH)

HEADERS_SUBDIRS= utils

HFILES=		dce.h

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	$(INSTALL) -m 444 -F /usr/include/dce $(HFILES)
	${HEADERS_SUBDIRS_MAKERULE}

