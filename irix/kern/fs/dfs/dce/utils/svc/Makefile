#!smake
#
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.4  1999/02/03 21:24:52  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.3  1998/07/10 14:12:55  gwehrman
# Added missing cat.m files to ILIST.
#
# Revision 65.2  1997/10/20 19:15:06  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 64.1  1997/02/14  19:44:10  dce
# *** empty log message ***
# 
# Revision 1.5  1996/12/24  19:06:37  vrk
# PV Incidence - 451963. Added *.cat to OTHERS macro and *.cat.m to ILIST.
#
# Revision 1.4  1996/12/17  02:27:22  bhim
# Added *.cat.m files to the INSTALL LIST.
#
# Revision 1.3  1996/09/27  15:40:14  bhim
# Improved SGIMIPS install condition checks.
#
# Revision 1.2  1996/09/11  16:54:34  bhim
# Modified for new build procedure.
#
# Revision 1.1  1994/09/07  21:04:59  dcebuild
# Original File from OSF
#
# Revision 1.1.4.18  1994/09/07  21:04:58  bowe
# 	Added msginqcma.o [CR 11885]
# 	[1994/09/07  21:03:49  bowe]
# 
# Revision 1.1.4.17  1994/07/27  12:27:54  rsalz
# 	Turn off strict compiler warnings on HP to avoid lots of
# 	messages from their bug in <stdarg.h>
# 	[1994/07/27  03:39:28  rsalz]
# 
# 	Silence compiler warnings (OT CR 11455).
# 	[1994/07/27  03:33:27  rsalz]
# 
# Revision 1.1.4.16  1994/07/07  19:45:59  rsalz
# 	First part of removing SVC ACL Manager code (OT CR 10856).
# 	[1994/07/07  19:24:39  rsalz]
# 
# Revision 1.1.4.15  1994/06/09  16:05:50  devsrc
# 	cr10892 - fix copyright
# 	[1994/06/09  15:50:24  devsrc]
# 
# Revision 1.1.4.14  1994/05/27  18:27:03  bowe
# 	Set IDLINCFLAGS and INCFLAGS for building svcbin.h and svcbin_cstub.*
# 	(needs utctypes.{h,idl}) [CR 10797]
# 	[1994/05/27  18:26:35  bowe]
# 
# Revision 1.1.4.13  1994/05/26  18:51:34  bowe
# 	Added references to svcbin, svcdumplog.
# 	[1994/05/26  18:10:43  bowe]
# 
# Revision 1.1.4.12  1994/05/12  14:06:22  rsalz
# 	Don't append .cat to message catalog on HP (OT CR 10480).
# 	[1994/05/12  13:48:41  rsalz]
# 
# Revision 1.1.4.11  1994/04/25  21:41:59  salamone
# 	CR10430 - Export service.idl for dcecp
# 	[1994/04/25  21:41:24  salamone]
# 
# Revision 1.1.4.10  1994/04/19  20:01:52  rsalz
# 	Install dumplog in right place (OT CR 10382).
# 	Add a sample SVC routing file (OT CR 10397).
# 	[1994/04/19  16:39:28  rsalz]
# 
# Revision 1.1.4.9  1994/03/18  17:15:42  rsalz
# 	Fix sams flag so dcesvc.cat is generated (OT CR 10162).
# 	[1994/03/18  17:08:05  rsalz]
# 
# Revision 1.1.4.8  1994/03/16  03:50:13  rsalz
# 	More cleanups; get dumplog to build (OT CR 9823).
# 	[1994/03/15  20:05:50  rsalz]
# 
# 	Add service.acf (OT CR 9935); also Makefile cleanups.
# 	[1994/03/15  17:09:30  rsalz]
# 
# Revision 1.1.4.7  1994/01/28  18:03:15  bowe
# 	Comment out PROGRAMS (temporarily to fix rios build).
# 	[1994/01/28  18:02:14  bowe]
# 
# Revision 1.1.4.6  1994/01/24  22:01:50  bowe
# 	Set ILIST to the INCLUDES, not INCLUDES and LIBRARIES.
# 	Remove libsvc.a_IDIR, since that is not installed.
# 	[1994/01/24  21:56:57  bowe]
# 
# Revision 1.1.4.5  1994/01/18  23:17:33  bowe
# 	Set IDIR to the right value.
# 	[1994/01/18  23:16:32  bowe]
# 
# Revision 1.1.4.4  1994/01/03  19:35:04  rsalz
# 	Added dce_error.h from rpc/sys_idl.
# 	[1994/01/03  15:34:16  rsalz]
# 
# Revision 1.1.4.3  1993/12/20  20:50:42  rsalz
# 	Merge msg.sams into svc.sams
# 	[1993/12/20  20:50:18  rsalz]
# 
# Revision 1.1.4.2  1993/12/14  23:24:27  mario
# 	Added LIBRARIES and libsvc.a_EXPDIR so that libsvc.a gets placed in usr/lib
# 	[1993/12/14  23:24:15  mario]
# 
# Revision 1.1.4.1  1993/12/14  16:57:54  mario
# 	Changes required to get svc built in libdce
# 	[1993/12/14  16:57:22  mario]
# 
# Revision 1.1.2.4  1993/10/07  17:07:20  bowe
# 	Fixed type-o in target.  "dcesvcsmsg" changed to "dcesvcs.msg".
# 	[1993/10/07  16:59:14  bowe]
# 
# Revision 1.1.2.3  1993/08/25  20:17:59  rsalz
# 	Set IDIR, etc., so install pass works.
# 	Remove outdated manpage targets.
# 	[1993/08/25  20:17:09  rsalz]
# 
# Revision 1.1.2.2  1993/08/16  18:07:48  rsalz
# 	Initial release
# 	[1993/08/16  18:02:13  rsalz]
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.4 $
DEPTH?=../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/dce/utils/svc
.PATH:$(KPATH)

KCINCS=		-I$(ROOT)/usr/include \
		-I$(ROOT)/usr/include/dcedfs

OBJ_NAME=	
OBJ_CSRCS=	
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		assert.h dce_msg.h dce_error.h dce_svc.h service.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dce $(HFILES)

