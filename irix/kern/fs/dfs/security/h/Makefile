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
# Revision 65.4  1999/05/04 19:19:32  gwehrman
# Added macro.h to the list of HFILES exported.  Fix for bug 691629.
#
# Revision 65.3  1999/02/03 22:44:18  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.2  1998/07/10 14:12:58  gwehrman
# Added missing cat.m files to ILIST.
#
# Revision 65.1  1997/10/20 19:46:09  jdoak
# *** empty log message ***
#
# Revision 1.1.10.6  1996/10/03  14:49:58  arvind
# 	PKSS drop from DEC (DCE1.2.2)
# 	[1996/08/30  15:34 UTC  arvind  /main/DCE_1.2.2/arvind_pkss/1]
#
# Revision 1.1.10.5  1996/08/09  12:04:10  arvind
# 	DCE 1.2.2 intermediate drop (post bl3/pre bl4)
# 	[1996/08/08  20:09:43  arvind]
# 
# Revision 1.1.10.4  1996/07/24  19:03:26  arvind
# 	OSF DCE 1.2.2 - Intl. build changes
# 	[1996/07/24  19:02:54  arvind]
# 
# Revision /main/DCE_1.2.2/5  1996/07/12  13:52 UTC  cuti
# 	Fixed, so dcepsm.cat can be installed automatically.
# 
# Revision /main/DCE_1.2.2/DCE122_PK/cuti_pk_migration/1  1996/07/11  14:10 UTC  cuti
# 	Add dcepsm.cat to OTHERS, so psm cat can be installed
# 
# Revision /main/DCE_1.2.2/DCE122_PK/3  1996/07/03  12:45 UTC  cuti
# 	Merge from cuti_pk_export to DCE_1.2.2
# 	[1996/06/30  21:59 UTC  cuti  /main/DCE_1.2.2/4]
# 
# 	Merge out from DCE122
# 
# 	add sec_pvtkey_base.h
# 	[1996/06/27  13:55 UTC  cuti  /main/DCE_1.2.2/DCE122_PK/cuti_pk_export/1]
# 
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/11  17:56 UTC  aha  /main/DCE_1.2.2/3]
# 
# Revision /main/DCE_1.2.2/DCE122_PK/2  1996/06/05  14:37 UTC  psn
# 	Merge OSF endgame fixes to HP.
# 	[1996/03/14  19:15 UTC  psn  /main/DCE_1.2/2]
# 
# 	Fix fix for OT13344 as done during OSF 1.2.1 Endgame.
# 	[1996/02/28  22:09 UTC  psn  /main/DCE_1.2/psn_1.2_endgame/2]
# 
# 	Fix OT13345 as done during OSF 1.2.1 Endgame.
# 	[1996/02/28  22:02 UTC  psn  /main/DCE_1.2/psn_1.2_endgame/1]
# 
# 	merge back in sys.group
# 	[1995/10/04  18:37 UTC  bartf  /main/HPDCE02/5]
# 
# 	add sys.group.txt back in, it will exit with group_override
# 	[1995/09/29  14:23 UTC  bartf  /main/HPDCE02/bartf_sec_wc.10/1]
# 
# 	finish removing sys.group
# 	[1995/07/14  11:13 UTC  root  /main/HPDCE02/4]
# 
# 	merge group_override file addition
# 	[1995/07/12  20:15 UTC  bartf  /main/HPDCE02/3]
# 
# 	add group_override
# 	[1995/07/11  18:10 UTC  bartf  /main/HPDCE02/bartf_sec_10/1]
# 
# 	remove sys.group
# 	[1995/07/11  19:35 UTC  bartf  /main/HPDCE02/bartf_sec_10/2]
# 
# 	add group_override
# 	[1995/07/11  18:10 UTC  bartf  /main/HPDCE02/bartf_sec_10/1]
# 
# 	Merge from KK
# 	[1995/02/10  14:20 UTC  mullan_s  /main/HPDCE02/1]
# 
# 	Merge passwd_override and sys.group rules.
# 	[1995/02/10  14:19 UTC  mullan_s  /main/mullan_merge_pwdoverride/1]
# 
# 	Update OSF copyright year
# 	[1996/03/09  22:43:01  marty]
# 	#
# 
# 	ENDGAME Submission
# 	[1996/02/22  16:10:54  root]
# 
# 	Submit
# 
# 	HP revision /main/HPDCE02/5  1995/10/04  18:37 UTC  bartf
# 	merge back in sys.group
# 
# 	HP revision /main/HPDCE02/bartf_sec_wc.10/1  1995/09/29  14:23 UTC  bartf
# 	add sys.group.txt back in, it will exit with group_override
# 
# 	HP revision /main/HPDCE02/4  1995/07/14  11:13 UTC  root
# 	finish removing sys.group
# 
# 	HP revision /main/HPDCE02/3  1995/07/12  20:15 UTC  bartf
# 	merge group_override file addition
# 
# 	HP revision /main/HPDCE02/bartf_sec_10/1  1995/07/11  18:10 UTC  bartf
# 	add group_override
# 
# 	HP revision /main/HPDCE02/bartf_sec_10/2  1995/07/11  19:35 UTC  bartf
# 	remove sys.group
# 
# 	HP revision /main/HPDCE02/bartf_sec_10/1  1995/07/11  18:10 UTC  bartf
# 	add group_override
# 
# 	HP revision /main/HPDCE02/1  1995/02/10  14:20 UTC  mullan_s
# 	Merge from KK
# 
# 	HP revision /main/mullan_merge_pwdoverride/1  1995/02/10  14:19 UTC  mullan_s
# 	Merge passwd_override and sys.group rules.
# 	[1995/12/11  15:14:08  root]
# 
# 	Merge psm, pvtkey work
# 
# 	Merge cuti_pk_new branch which is merged from sec_pk_work.
# 	[1996/05/09  16:41 UTC  cuti  /main/DCE_1.2/DCE12_PK/2]
# 
# 	Add sec_psm_base.h
# 	[1996/05/24  19:25 UTC  cuti  /main/DCE_1.2/DCE12_PK/cuti_pk_new/2]
# 
# 	Merge cuti_pk_work branch.
# 
# 	Add sec_pk files
# 	[1996/04/23  20:02 UTC  aha  /main/DCE_1.2/DCE12_PK/1]
# 
# 	Add sec_pk.h
# 	[1996/04/16  19:54 UTC  aha  /main/DCE_1.2/aha_pk2/1]
# 
# 	Add psm.sams.
# 	[1996/05/08  15:05 UTC  cuti  /main/DCE_1.2/cuti_pk_work/1]
# 
# 	Merge OSF endgame fixes to HP.
# 	[1996/03/14  19:15 UTC  psn  /main/DCE_1.2/2]
# 
# 	Fix fix for OT13344 as done during OSF 1.2.1 Endgame.
# 	[1996/02/28  22:09 UTC  psn  /main/DCE_1.2/psn_1.2_endgame/2]
# 
# 	Fix OT13345 as done during OSF 1.2.1 Endgame.
# 	[1996/02/28  22:02 UTC  psn  /main/DCE_1.2/psn_1.2_endgame/1]
# 
# 	merge back in sys.group
# 	[1995/10/04  18:37 UTC  bartf  /main/HPDCE02/5]
# 
# 	add sys.group.txt back in, it will exit with group_override
# 	[1995/09/29  14:23 UTC  bartf  /main/HPDCE02/bartf_sec_wc.10/1]
# 
# 	finish removing sys.group
# 	[1995/07/14  11:13 UTC  root  /main/HPDCE02/4]
# 
# 	merge group_override file addition
# 	[1995/07/12  20:15 UTC  bartf  /main/HPDCE02/3]
# 
# 	add group_override
# 	[1995/07/11  18:10 UTC  bartf  /main/HPDCE02/bartf_sec_10/1]
# 
# 	remove sys.group
# 	[1995/07/11  19:35 UTC  bartf  /main/HPDCE02/bartf_sec_10/2]
# 
# 	add group_override
# 	[1995/07/11  18:10 UTC  bartf  /main/HPDCE02/bartf_sec_10/1]
# 
# 	Merge from KK
# 	[1995/02/10  14:20 UTC  mullan_s  /main/HPDCE02/1]
# 
# 	Merge passwd_override and sys.group rules.
# 	[1995/02/10  14:19 UTC  mullan_s  /main/mullan_merge_pwdoverride/1]
# 
# Revision /main/DCE_1.2.2/2  1996/05/29  15:04 UTC  arvind
# 	Remove passwd_override, group_override, sys.group
# 
# Revision /main/DCE_1.2.2/arvind_work/1  1996/05/29  14:52 UTC  arvind
# 	Remove password_override, group_override, sys.group
# 
# Revision /main/DCE_1.2.2/1  1996/05/24  23:14 UTC  sommerfeld
# 	Merge OSF endgame fixes to HP.
# 	[1996/03/14  19:15 UTC  psn  /main/DCE_1.2/2]
# 
# 	Fix fix for OT13344 as done during OSF 1.2.1 Endgame.
# 	[1996/02/28  22:09 UTC  psn  /main/DCE_1.2/psn_1.2_endgame/2]
# 
# 	Fix OT13345 as done during OSF 1.2.1 Endgame.
# 	[1996/02/28  22:02 UTC  psn  /main/DCE_1.2/psn_1.2_endgame/1]
# 
# 	merge back in sys.group
# 	[1995/10/04  18:37 UTC  bartf  /main/HPDCE02/5]
# 
# 	add sys.group.txt back in, it will exit with group_override
# 	[1995/09/29  14:23 UTC  bartf  /main/HPDCE02/bartf_sec_wc.10/1]
# 
# 	finish removing sys.group
# 	[1995/07/14  11:13 UTC  root  /main/HPDCE02/4]
# 
# 	merge group_override file addition
# 	[1995/07/12  20:15 UTC  bartf  /main/HPDCE02/3]
# 
# 	add group_override
# 	[1995/07/11  18:10 UTC  bartf  /main/HPDCE02/bartf_sec_10/1]
# 
# 	remove sys.group
# 	[1995/07/11  19:35 UTC  bartf  /main/HPDCE02/bartf_sec_10/2]
# 
# 	add group_override
# 	[1995/07/11  18:10 UTC  bartf  /main/HPDCE02/bartf_sec_10/1]
# 
# 	Merge from KK
# 	[1995/02/10  14:20 UTC  mullan_s  /main/HPDCE02/1]
# 
# 	Merge passwd_override and sys.group rules.
# 	[1995/02/10  14:19 UTC  mullan_s  /main/mullan_merge_pwdoverride/1]
# 
# Revision 1.1.8.3  1996/03/09  23:27:32  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:43:01  marty]
# 
# Revision 1.1.8.2  1996/02/22  16:38:29  root
# 	ENDGAME Submission
# 	[1996/02/22  16:10:54  root]
# 
# Revision 1.1.8.1  1995/12/13  16:24:22  root
# 	Submit
# 
# 	HP revision /main/HPDCE02/5  1995/10/04  18:37 UTC  bartf
# 	merge back in sys.group
# 
# 	HP revision /main/HPDCE02/bartf_sec_wc.10/1  1995/09/29  14:23 UTC  bartf
# 	add sys.group.txt back in, it will exit with group_override
# 
# 	HP revision /main/HPDCE02/4  1995/07/14  11:13 UTC  root
# 	finish removing sys.group
# 
# 	HP revision /main/HPDCE02/3  1995/07/12  20:15 UTC  bartf
# 	merge group_override file addition
# 
# 	HP revision /main/HPDCE02/bartf_sec_10/1  1995/07/11  18:10 UTC  bartf
# 	add group_override
# 
# 	HP revision /main/HPDCE02/bartf_sec_10/2  1995/07/11  19:35 UTC  bartf
# 	remove sys.group
# 
# 	HP revision /main/HPDCE02/bartf_sec_10/1  1995/07/11  18:10 UTC  bartf
# 	add group_override
# 
# 	HP revision /main/HPDCE02/1  1995/02/10  14:20 UTC  mullan_s
# 	Merge from KK
# 
# 	HP revision /main/mullan_merge_pwdoverride/1  1995/02/10  14:19 UTC  mullan_s
# 	Merge passwd_override and sys.group rules.
# 	[1995/12/11  15:14:08  root]
# 
# Revision 1.1.6.8  1994/08/29  15:51:28  hondo
# 	OT 11919 --login activity idl change to support different address types
# 	[1994/08/28  21:47:26  hondo]
# 
# Revision 1.1.6.7  1994/08/04  16:12:42  mdf
# 	Hewlett Packard Security Code Drop
# 	[1994/07/26  19:09:21  mdf]
# 
# Revision 1.1.6.6  1994/07/15  14:58:57  mdf
# 	Hewlett Packard Security Code Drop
# 	[1994/07/14  17:15:36  mdf]
# 
# Revision 1.1.6.4  1994/05/11  19:04:31  ahop
# 	fix merge problems
# 	[1994/05/02  18:19:54  ahop]
# 
# 	 hp_sec_to_osf_2 drop
# 	[1994/04/29  20:47:46  ahop]
# 
# 	This file is for export only!  It is not intended to go to the install
# 	tree.
# 
# 	Also flush sad_err.msf..
# 	[1993/10/11  18:01:58  sommerfeld]
# 
# 	Makefile cleanups .. add rm -f's
# 	[1993/10/08  19:45:53  sommerfeld]
# 
# 	Initial King Kong branch
# 	[1993/07/08  11:12:07  root]
# 
# Revision 1.1.5.2  1993/03/30  20:22:04  ahop
# 	Merge in HP changes: support sys.group and passwd_override
# 	[1993/03/30  20:11:37  ahop]
# 
# 	Embedding copyright notice
# 	[1992/12/28  20:43:04  zeliff]
# 
# Revision 1.1.1.8  1993/03/11  19:46:31  osfrcs
# 	    Loading drop DCE1_0_2B18
# 
# Revision 1.1.2.2  1992/04/07  19:18:29  mckeen
# 	Now usese ${COMPILE_ET} instead of compile_et
# 	[1992/03/13  15:41:56  mckeen]
# 
# Revision 1.1  1992/01/19  14:43:35  devrcs
# 	Initial revision
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.4 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/security/h
.PATH:$(KPATH)

HFILES=		dfs_dce_acl.h \
		macros.h


include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	$(INSTALL) -m 444 -F /usr/include/dce $(HFILES)

