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
# Revision 65.4  1999/02/03 22:44:18  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.3  1998/07/10 14:12:58  gwehrman
# Added missing cat.m files to ILIST.
#
# Revision 65.2  1997/10/20 19:46:28  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 1.1.20.2  1996/08/09  12:06:08  arvind
# 	CHFts19678: application coredumps when calling rsec_pk_cache_kdc_key
# 	[1996/07/24  18:44 UTC  aha  /main/DCE_1.2.2/aha_dbpk1/1]
#
# 	Add ACF file for rsec_pk_cache interface
# 	[1996/07/18  17:33 UTC  aha  /main/DCE_1.2.2/2]
#
# 	Merge Registry support for KDC private key storage
# 	[1996/07/17  13:33 UTC  aha  /main/DCE122_PK/aha_pk6/4]
#
# 	Do not export passwd_encode.idl
# 	[1996/07/16  22:07 UTC  aha  /main/DCE122_PK/aha_pk6/3]
#
# 	Export passwd_encode.idl for use by kutils
# 	[1996/07/15  19:42 UTC  aha  /main/DCE122_PK/aha_pk6/2]
#
# 	Merge-out and bugfixes and Registry work
# 	[1996/07/13  20:49 UTC  aha  /main/DCE122_PK/aha_pk6/1]
#
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/11  17:56 UTC  aha  /main/DCE_1.2.2/1]
#
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/04  18:50 UTC  psn  /main/DCE122_PK/1]
#
# 	Add -cepv to IDLFLAGS for rsec_pk_cache.idl
# 	[1996/04/12  20:16 UTC  aha  /main/DCE_1.2/aha_pk2/1]
#
# 	Add rsec_pk_cache, sec_pk_base, and sec_pubkey for public key login.
# 	[1996/01/03  19:12 UTC  psn  /main/DCE_1.2/1]
#
# Revision 1.1.20.1  1996/07/08  18:33:47  arvind
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/04  18:50 UTC  psn  /main/DCE122_PK/1]
# 
# 	Add -cepv for rsec_pk_cache.idl
# 
# 	Merge cuti_pk_work to DCE12_PK, add sec_psm.idl and sec_pvtkey.idl
# 
# 	Merge Makefile for sec_pubkey, sec_pk_base, and rsec_pk_cache
# 	[1996/04/22  19:11 UTC  aha  /main/DCE_1.2/DCE12_PK/1]
# 
# 	Add rsec_pk_cache, sec_pk_base, and sec_pubkey for public key login.
# 	[1996/04/12  20:16 UTC  aha  /main/DCE_1.2/aha_pk2/1]
# 
# 	Add sec_psm's idls
# 	[1996/04/29  18:13 UTC  cuti  /main/DCE_1.2/cuti_pk_work/1]
# 
# 	Add -cepv to IDLFLAGS for rsec_pk_cache.idl
# 	[1996/04/12  20:16 UTC  aha  /main/DCE_1.2/aha_pk2/1]
# 
# 	Add rsec_pk_cache, sec_pk_base, and sec_pubkey for public key login.
# 	[1996/01/03  19:12 UTC  psn  /main/DCE_1.2/1]
# 
# 	Add daclmgrv0.h for backward compatibility
# 	[1995/07/18  20:38 UTC  cuti  /main/HPDCE02/7]
# 
# 	Add daclmgrv0.idl for backward compatibility
# 	[1995/07/14  19:08 UTC  cuti  /main/HPDCE02/cuti_mothra10_fix/1]
# 
# 	remove rs_repadm_ACF
# 	[1995/07/12  23:36 UTC  rps  /main/HPDCE02/6]
# 
# 	remove rs_repadm_ACF
# 	[1995/07/10  23:06 UTC  rps  /main/HPDCE02/rps_moth2/1]
# 
# 	Binary Compatibility Merge
# 	[1995/04/03  22:05 UTC  mullan_s  /main/HPDCE02/3]
# 
# 	Add rules for sec_login_base.idl
# 	[1995/01/04  15:39 UTC  mullan_s  /main/HPDCE02/mullan_mothra_bin_compat/1]
# 
# 	Merge from Hpdce02_02
# 	[1994/09/27  10:55 UTC  mothra  /main/HPDCE02/2]
# 
# Revision 1.1.17.3  1996/03/09  23:27:39  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:43:06  marty]
# 
# Revision 1.1.17.2  1995/12/08  17:30:40  root
# 	Submit OSF/DCE 1.2.1
# 
# 	HP revision /main/HPDCE02/7  1995/07/18  20:38 UTC  cuti
# 	Add daclmgrv0.h for backward compatibility
# 
# 	HP revision /main/HPDCE02/cuti_mothra10_fix/1  1995/07/14  19:08 UTC  cuti
# 	Add daclmgrv0.idl for backward compatibility
# 
# 	HP revision /main/HPDCE02/6  1995/07/12  23:36 UTC  rps
# 	remove rs_repadm_ACF
# 
# 	HP revision /main/HPDCE02/rps_moth2/1  1995/07/10  23:06 UTC  rps
# 	remove rs_repadm_ACF
# 
# 	HP revision /main/HPDCE02/5  1995/05/15  21:03 UTC  greg
# 	Mergeout from HPDCE02 to SEC_migrate_merge.
# 	[1995/05/13  02:41 UTC  greg  /main/HPDCE02/SEC_migrate_merge/2]
# 
# 	Exporting rs_repadm
# 	[1995/04/27  15:22 UTC  mdf  /main/HPDCE02/mdf_mothra_defects_3/1]
# 
# 	Create SEC_migrate_merge mainline from SEC_migrate and HPDCE02.
# 	[1995/04/22  13:35 UTC  greg  /main/HPDCE02/SEC_migrate_merge/1]
# 
# 	HP revision /main/HPDCE02/4  1995/04/28  19:58 UTC  mdf
# 	Exporting rs_repadm
# 	[1995/04/27  15:22 UTC  mdf  /main/HPDCE02/mdf_mothra_defects_3/1]
# 
# 	HP revision /main/HPDCE02/3  1995/04/03  22:05 UTC  mullan_s
# 	Binary Compatibility Merge
# 
# 	HP revision /main/HPDCE02/mullan_mothra_bin_compat/1  1995/01/04  15:39 UTC  mullan_s
# 	Add rules for sec_login_base.idl
# 
# 	HP revision /main/HPDCE02/2  1994/09/27  10:55 UTC  mothra
# 	Merge from Hpdce02_02
# 
# 	HP revision /main/HPDCE02/1  1994/08/30  23:07 UTC  sommerfeld
# 	export secsts.idl early on..
# 	[1994/08/04  22:37 UTC  sommerfeld  /main/dce1.1_clearmakefix/1]
# 	[1994/08/30  18:10 UTC  sommerfeld  /main/dce1.1_clearmakefix/2]
# 
# 	OT 11919 --login activity idl change to support different address types
# 	[1994/08/28  21:48:43  hondo]
# 
# Revision 1.1.11.11  1994/08/04  16:12:53  mdf
# 	Hewlett Packard Security Code Drop
# 	[1994/07/26  19:09:43  mdf]
# 
# Revision 1.1.11.10  1994/07/22  21:28:33  pwang
# 	Exported client version of rs_attr_schema.h [OT#11349]
# 	[1994/07/22  21:25:02  pwang]
# 
# Revision 1.1.11.9  1994/07/15  15:00:04  mdf
# 	Hewlett Packard Security Code Drop
# 	[1994/07/14  17:17:20  mdf]
# 
# Revision 1.1.11.8  1994/06/02  21:38:36  mdf
# 	Merged with changes from 1.1.11.7
# 	[1994/06/02  21:35:42  mdf]
# 
# 	hp_sec_to_osf_3 drop, merge up with latest.
# 	[1994/05/24  15:15:46  mdf]
# 
# 	HP revision /main/ODESSA_2/13  1994/06/15  17:46 UTC  mullan_s
# 	Merge -from mullan_el_bl19 -to ODESSA_2
# 
# Revision 1.1.11.7  1994/05/20  22:03:40  sommerfeld
# 	Add secsts.idl to the SAMS_TARGETS variable so it gets built during
# 	the "right" pass..
# 	[1994/05/20  21:41:00  sommerfeld]
# 
# 	fix sams invocation.
# 	[1994/05/17  19:10:46  sommerfeld]
# 
# 	HP revision /main/ODESSA_2/mdf_authzp_trig_0/1  1994/06/06  11:14 UTC  mdf
# 	Added priv_attr_trig.idl to NORMAL_IDLFILES.
# 
# 	HP revision /main/ODESSA_2/11  1994/06/02  19:48 UTC  sekhar
# 	Merge SVC changes back from OSF.
# 
# 	HP revision /main/ODESSA_2/sekhar_svc/4  1994/05/31  12:47 UTC  sekhar
# 	Pick up Bill's changes.
# 
# 	   ===== Summary of Bill's changes =====
# 	   Revision 1.1.11.7  1994/05/20  22:03:40  sommerfeld
# 	Add secsts.idl to the SAMS_TARGETS variable so it gets built during
# 	the "right" pass..
# 	[1994/05/20  21:41:00  sommerfeld]
# 
# 	fix sams invocation.
# 	[1994/05/17  19:10:46  sommerfeld]
# 
# Revision 1.1.11.6  1994/05/11  19:08:52  ahop
# 	hp_sec_to_osf_2 drop
# 	Add rules for sec_pwd_mgmt.idl
# 	Add rpriv_1_0.idl
# 	 export rs_attr*, don't install rsbase, priv, etc
# 	export authz_base.{idl, h}, authz_encode.{idl, h}
# 	Add rsec_login.idl.
# 	[1994/04/29  21:02:30  ahop]
# 
# Revision 1.1.11.5  1994/03/25  20:29:17  sekhar
# 	Fixed merge errors.
# 	[1994/03/25  20:28:29  sekhar]
# 
# Revision 1.1.11.4  1994/03/25  19:00:26  sekhar
# 	Merged with changes from 1.1.11.3
# 	[1994/03/25  19:00:00  sekhar]
# 
# 	[ OT 10164 ]  Security SVC code drop ( sec_svc_bl1 )
# 	              Generate sec.sams from sec.msg ; generate header
# 	              files and message catalogs from sec.sams .
# 	[1994/03/25  18:50:28  sekhar]
# 
# Revision 1.1.11.3  1994/03/02  17:55:14  hanfei
# 	Initial ERA drop
# 	[1994/02/18  19:19:45  hanfei]
# 
# Revision 1.1.11.1  1994/01/28  23:10:37  burati
# 	Delegation changes (dlg_bl1)
# 	[1994/01/18  20:51:27  burati]
# 
# Revision 1.1.6.5  1993/01/29  18:49:47  burati
# 	replist.idl and repadm.idl
# 	[1993/01/14  23:16:37  burati]
# 
# Revision 1.1.6.4  1992/12/29  13:28:37  zeliff
# 	Embedding copyright notice
# 	[1992/12/28  20:47:54  zeliff]
# 
# Revision 1.1.6.3  1992/12/08  19:01:32  rps
# 	Add rs_bind.
# 	[1992/12/03  01:22:52  rps]
# 
# Revision 1.1.6.2  1992/10/08  21:08:30  burati
# 	CR4730 pickle.idl is obsolete, don't build it anymore
# 	[1992/09/24  21:58:29  burati]
# 
# Revision 1.1.3.5  1992/06/02  20:58:58  sekhar
# 	Merged in Unix interface changes made by frisco
# 	from frisco_unix branch to mainline.
# 	[1992/06/01  21:12:45  sekhar]
# 
# Revision 1.1.4.2  1992/05/22  18:58:24  frisco
# 	Unix interface support
# 
# Revision 1.1.3.4  1992/05/26  20:57:44  burati
# 	Remove references to rdacliftmp
# 	[1992/05/26  20:21:18  burati]
# 
# Revision 1.1.3.3  1992/04/07  19:18:32  mckeen
# 	Almost a total rewrite of Makefile.  Removed redundent lists and simplified.
# 	[1992/04/07  16:48:00  mckeen]
# 
# Revision 1.1.3.2  1992/03/27  18:58:07  burati
# 	CR2410 Export daclmgr.idl,daclmgr.h
# 	[1992/03/27  17:08:02  burati]
# 	Revision 1.1.1.2  1992/03/13  15:42:03  mckeen
# 	Added .ORDER to support parallel make
# 
# Revision 1.1  1992/01/19  14:55:31  devrcs
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
KPATH=$(TOP)/fs/dfs/security/idl
.PATH:$(KPATH)

KCINCS=		-I$(ROOT)/usr/include \
		-I$(ROOT)/usr/include/dcedfs

HFILES=		aclbase.h \
		authz_base.h \
		authz_encode.h \
		binding.h \
		daclif.h \
		daclmgr.h \
		daclmgrv0.h \
		id_base.h \
		id_encode.h \
		id_epac.h \
		keymgmt.h \
		misc.h \
		oride_base.h \
		override.h \
		passwd.h \
		passwd_encode.h \
		pgo.h \
		pkl.h \
		pklbase.h \
		policy.h \
		priv_attr_trig.h \
		prvnbase.h \
		rdaclbase.h \
		rdaclif.h \
		rdaclifv0.h \
		repadm.h \
		replist.h \
		rgybase.h \
		rgynbase.h \
		rpladmn.h \
		rplbase.h \
		rpriv_1_0.h \
		rs_bind.h \
		rs_login_activity.h \
		rs_login_base.h \
		rsec_login.h \
		rsec_pk_cache.h \
		rsec_pwd_mgmt.h \
		secsts.h \
		sec_acl_encode.h \
		sec_attr_base.h \
		sec_attr_trig.h \
		sec_attr_util.h \
		sec_authn.h \
		sec_base.h \
		sec_cert.h \
		sec_cred.h \
		sec_login.h \
		sec_login_base.h \
		sec_login_encode.h \
		sec_pk_base.h \
		sec_psm.h \
		sec_pubkey.h \
		sec_pvtkey.h \
		sec_pwd_mgmt.h \
		sec_rgy_attr.h \
		sec_rgy_attr_sch.h \
		sec_rgy_attr_srch.h \
		sec_rgy_login_activity.h \
		secidmap.h \
		secsts.h \
		unix.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	$(INSTALL) -m 444 -F /usr/include/dce $(HFILES)

