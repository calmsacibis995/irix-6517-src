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
# Revision 65.7  1999/05/21 18:53:49  lauriec
# Deleted utils from SUBDIRS list because it gets built in kutils now.
#
# Revision 65.6  1999/02/03 22:44:18  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.5  1998/01/16 14:35:11  gwehrman
# Created (again) a NO_BSAFE flag to build without the BSAFE libraries.
#
# Revision 65.4  1997/12/03  16:27:28  gwehrman
# Remove the NO_BSAFE switch.  BSAFE is so tightly integrated into DCE
# security, it not feasible to build the domestic version without BSAFE.
#
# Revision 65.3  1997/11/21  21:41:05  gwehrman
# Create a variable NO_BSAFE which, if defined, builds a non-functioning
# public security module.  This is required until the BSAFE source code
# is licensed from OSF.
#
# Revision 65.2  1997/11/18  21:00:50  gwehrman
# Removed psm from the list of subdirectories to build.  This subdirectory
# should be build once the BSAFE library is obtained.
#
# Revision 65.1  1997/10/20  19:46:08  jdoak
# *** empty log message ***
#
# Revision 1.1.18.5  1996/10/15  20:47:33  arvind
# 	OSF DCE 1.2.2 Drop 4.5
# 	[1996/10/15  20:11:36  arvind]
#
# Revision 1.1.18.4  1996/10/09  14:14:31  arvind
# 	OSF DCE 1.2.2 Drop 4 + fix
# 	[1996/10/09  14:13:37  arvind]
# 
# Revision /main/DCE_1.2.2/6  1996/10/08  18:55 UTC  sommerfeld
# 	CHFts20186: fix build of security/kutils
# 	[1996/10/08  16:10 UTC  sommerfeld  /main/DCE_1.2.2/sommerfeld_pk_kdc_2/2]
# 
# Revision /main/DCE_1.2.2/5  1996/10/02  20:23 UTC  sommerfeld
# 	do it right this time :-).
# 	[1996/10/02  20:18 UTC  sommerfeld  /main/DCE_1.2.2/sommerfeld_pk_kdc_2/1]
# 
# Revision /main/DCE_1.2.2/4  1996/10/02  18:28 UTC  cuti
# 	Merge of cuti_pk_fix
# 
# 	Conditionalize  HP build not to use pkss/pkc
# 	[1996/09/10  18:38 UTC  arvind  /main/DCE_1.2.2/arvind_pkss/3]
# 	Revision /main/DCE_1.2.2/cuti_pk_fix/2  1996/10/02  17:48 UTC  cuti
# 	Make HP800 works
# 
# Revision /main/DCE_1.2.2/cuti_pk_fix/1  1996/09/26  16:18 UTC  cuti
# 	Remove pkc for international build
# 
# Revision /main/DCE_1.2.2/2  1996/09/09  21:24 UTC  arvind
# 	PKSS drop from DEC (DCE1.2.2)
# 	[1996/08/30  15:34 UTC  arvind  /main/DCE_1.2.2/arvind_pkss/1]
# 
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/04  18:49 UTC  psn  /main/DCE122_PK/1]
# 	#
# 	Merge udpated pvtkey stuff
# 	#
# 	Add psm
# 	[1996/05/13  20:44 UTC  cuti  /main/DCE_1.2/DCE12_PK/cuti_pk_new/1]
# 	#
# 
# 	Update OSF copyright year
# 	[1996/03/09  22:42:30  marty]
# 
# 	Submit OSF/DCE 1.2.1
# 
# 	HP revision /main/HPDCE02/1  1995/05/15  20:58 UTC  greg
# 	Create SEC_migrate_merge mainline from SEC_migrate and HPDCE02.
# 	[1995/04/22  13:33 UTC  greg  /main/SEC_migrate_merge/1]
# 
# 	build utils before server.  Some idl-derived .h files
# 	are needed during the server/rrs build.
# 	[1995/02/24  15:33 UTC  greg  /main/SEC_migrate/1]
# 
# Revision /main/DCE_1.2.2/1  1996/06/11  17:55 UTC  aha
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 
# Revision /main/DCE122_PK/1  1996/06/04  18:49 UTC  psn
# 	Update OSF copyright year
# 	[1996/03/09  22:42:30  marty]
# 	#
# 
# 	Submit OSF/DCE 1.2.1
# 
# 	HP revision /main/HPDCE02/1  1995/05/15  20:58 UTC  greg
# 	Create SEC_migrate_merge mainline from SEC_migrate and HPDCE02.
# 	[1995/04/22  13:33 UTC  greg  /main/SEC_migrate_merge/1]
# 
# 	build utils before server.  Some idl-derived .h files
# 	are needed during the server/rrs build.
# 	[1995/02/24  15:33 UTC  greg  /main/SEC_migrate/1]
# 
# Revision /main/DCE_1.2/DCE12_PK/1  1996/05/15  13:33 UTC  cuti
# 	Merge udpated pvtkey stuff
# 
# 	Add psm
# 	[1996/05/13  20:44 UTC  cuti  /main/DCE_1.2/DCE12_PK/cuti_pk_new/1]
# 	Revision /main/DCE_1.2/1  1996/01/03  19:05 UTC  psn
# 	Create SEC_migrate_merge mainline from SEC_migrate and HPDCE02.
# 	[1995/04/22  13:33 UTC  greg  /main/SEC_migrate_merge/1]
# 
# 	build utils before server.  Some idl-derived .h files
# 	are needed during the server/rrs build.
# 	[1995/02/24  15:33 UTC  greg  /main/SEC_migrate/1]
# 
# Revision 1.1.9.6  1994/08/10  19:34:57  burati
# 	Don't build security/helper directory if NO_DFS is defined
# 	[1994/08/10  18:37:42  burati]
# 
# Revision 1.1.9.5  1994/08/09  17:32:35  burati
# 	DFS/EPAC/KRPC/dfsbind changes
# 	[1994/08/09  16:58:03  burati]
# 
# Revision 1.1.9.4  1994/03/25  19:35:37  sekhar
# 	Merged with changes from 1.1.9.3
# 	[1994/03/25  19:35:28  sekhar]
# 
# 	[ OT 10164 ] Security SVC code drop ( sec_svc_bl1 )
# 	             ( added idl & h to SAMIDL_SUBDIRS )
# 	[1994/03/25  19:33:05  sekhar]
# 
# Revision 1.1.9.3  1994/03/09  15:50:39  wray
# 	Add GSSAPI (OT#10019)
# 	[1994/03/09  15:39:34  wray]
# 
# Revision 1.1.9.2  1994/01/13  14:28:04  zeliff
# 	OT9732: Change ordering of audit on EXPINC_SUBDIRS line
# 	[1994/01/13  14:15:13  zeliff]
# 
# Revision 1.1.9.1  1994/01/12  20:51:07  zeliff
# 	OT9726: turn Audit on in the build
# 	[1994/01/12  19:36:53  zeliff]
# 
# Revision 1.1.7.2  1992/12/29  12:36:04  zeliff
# 	Embedding copyright notice
# 	[1992/12/28  20:32:02  zeliff]
# 
# Revision 1.1.3.3  1992/04/30  21:17:04  john
# 	Revision 1.1.2.2  1992/04/10  14:42:01  john
# 	Added SETUP_SUBDIRS.
# 	[1992/04/16  13:25:03  john]
# 
# Revision 1.1.3.2  1992/04/07  19:17:57  mckeen
# 	Commented out DATA_SUBDIRS
# 	[1992/03/13  15:41:27  mckeen]
# 
# Revision 1.1  1992/01/19  14:55:50  devrcs
# 	Initial revision
# 
# $EndLog$
#

#   Makefile.ODE V=9 12/09/91 //littl/prgy/src
#
# Copyright (c) Hewlett-Packard Company 1991
# Unpublished work. All Rights Reserved.
#
#
# DCE Security Services 
# top level Makefile
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.7 $
DEPTH?=../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/security
.PATH:$(KPATH)

SUBDIRS = kutils
HEADERS_SUBDIRS= h idl krb5

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	${HEADERS_SUBDIRS_MAKERULE}

