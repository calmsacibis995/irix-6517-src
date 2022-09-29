#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
# src directory for the full copyright text.
#
# @HP_COPYRIGHT@
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.7  1999/05/21 18:53:49  lauriec
# Deleted building because its moved to security/kutils
#
# Revision 65.6  1999/02/03 22:44:25  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.5  1998/12/04 17:26:38  bobo
# O32 build changes
#
# Revision 65.4  1998/03/16  15:37:24  gwehrman
# Cleanup ld warnings.
#
# Revision 65.3  1997/12/12  14:32:50  gwehrman
# Export libraries to either domestic or international directory, depending
# on whether the library is using encryption or not.
#
# Revision 65.2  1997/10/20  19:48:31  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 1.2.27.5  1996/10/15  20:57:08  arvind
# 	Change for international build
# 	[1996/10/07  19:26 UTC  arvind  /main/DCE_1.2.2/arvind_work/4]
#
# 	CHFts19678: application coredumps when calling rsec_pk_cache_kdc_key
# 	[1996/07/24  18:44 UTC  aha  /main/DCE_1.2.2/aha_dbpk1/1]
#
# 	Add ACF file for rsec_pk_cache interface
# 	[1996/07/18  17:34 UTC  aha  /main/DCE_1.2.2/4]
#
# 	Merge Registry support for KDC private key storage
# 	[1996/07/16  14:11 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/8]
#
# 	Changes for Public Key Login; ensure sec_pk_base.idl is built before
# 	sec_encode.c
# 	[1996/07/15  19:43 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/7]
#
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/10  14:58 UTC  aha  /main/DCE_1.2.2/DCE122_PK/2]
#
# 	Merge to DCE122_PK
# 	[1996/06/07  13:07 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/4]
#
# 	Re-include dependencis of dacl_ifr.o and daclbind.o on
# 	rdaclif_cstub.c and rdaclifv0_cstub.c.
# 	[1996/06/06  17:43 UTC  arvind  /main/DCE_1.2.2/DCE122_PK/aha_pk6/3]
#
# 	Try to fix AIX build.
# 	[1996/06/06  16:37 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/2]
#
# 	Try to fix RIOS build
# 	[1996/06/06  16:13 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/1]
#
# 	Try to fix RIOS build
# 	[1996/06/04  18:51 UTC  psn  /main/DCE_1.2.2/DCE122_PK/1]
#
# 	Add build instructions for rsec_pk_cache_cstub
# 	[1996/04/23  19:18 UTC  aha  /main/DCE_1.2/aha_pk2/1]
#
# 	Additions for public key login
# 	[1996/01/03  19:20 UTC  psn  /main/DCE_1.2/1]
#
# Revision 1.2.27.4  1996/10/03  15:13:47  arvind
# 	CHFts19678: application coredumps when calling rsec_pk_cache_kdc_key
# 	[1996/07/26  20:46 UTC  aha  /main/DCE_1.2.2/5]
# 
# 	don't build sec_pubkey.idl here; it's not needed.
# 	[1996/08/09  17:16 UTC  sommerfeld  /main/DCE_1.2.2/DCE122_PK/sommerfeld_pk_kdc/1]
# 
# 	CHFts19678: application coredumps when calling rsec_pk_cache_kdc_key
# 	[1996/07/24  18:44 UTC  aha  /main/DCE_1.2.2/aha_dbpk1/1]
# 
# 	Add ACF file for rsec_pk_cache interface
# 	[1996/07/18  17:34 UTC  aha  /main/DCE_1.2.2/4]
# 
# 	Merge Registry support for KDC private key storage
# 	[1996/07/16  14:11 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/8]
# 
# 	Changes for Public Key Login; ensure sec_pk_base.idl is built before
# 	sec_encode.c
# 	[1996/07/15  19:43 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/7]
# 
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/10  14:58 UTC  aha  /main/DCE_1.2.2/DCE122_PK/2]
# 
# 	Merge to DCE122_PK
# 	[1996/06/07  13:07 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/4]
# 
# 	Re-include dependencis of dacl_ifr.o and daclbind.o on
# 	rdaclif_cstub.c and rdaclifv0_cstub.c.
# 	[1996/06/06  17:43 UTC  arvind  /main/DCE_1.2.2/DCE122_PK/aha_pk6/3]
# 
# 	Try to fix AIX build.
# 	[1996/06/06  16:37 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/2]
# 
# 	Try to fix RIOS build
# 	[1996/06/06  16:13 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/1]
# 
# 	Try to fix RIOS build
# 	[1996/06/04  18:51 UTC  psn  /main/DCE_1.2.2/DCE122_PK/1]
# 
# 	Add build instructions for rsec_pk_cache_cstub
# 	[1996/04/23  19:18 UTC  aha  /main/DCE_1.2/aha_pk2/1]
# 
# 	Additions for public key login
# 	[1996/01/03  19:20 UTC  psn  /main/DCE_1.2/1]
# 
# Revision 1.2.27.3  1996/08/09  12:12:10  arvind
# 	CHFts19678: application coredumps when calling rsec_pk_cache_kdc_key
# 	[1996/07/24  18:44 UTC  aha  /main/DCE_1.2.2/aha_dbpk1/1]
# 
# 	Add ACF file for rsec_pk_cache interface
# 	[1996/07/18  17:34 UTC  aha  /main/DCE_1.2.2/4]
# 
# 	Merge Registry support for KDC private key storage
# 	[1996/07/16  14:11 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/8]
# 
# 	Changes for Public Key Login; ensure sec_pk_base.idl is built before
# 	sec_encode.c
# 	[1996/07/15  19:43 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/7]
# 
# 	Merge out from DCE122_PK
# 	[1996/06/18  20:56 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/5]
# 
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/11  17:57 UTC  aha  /main/DCE_1.2.2/2]
# 
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/10  14:58 UTC  aha  /main/DCE_1.2.2/DCE122_PK/2]
# 
# 	Merge to DCE122_PK
# 	[1996/06/07  13:07 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/4]
# 
# 	Re-include dependencis of dacl_ifr.o and daclbind.o on
# 	rdaclif_cstub.c and rdaclifv0_cstub.c.
# 	[1996/06/06  17:43 UTC  arvind  /main/DCE_1.2.2/DCE122_PK/aha_pk6/3]
# 
# 	Try to fix AIX build.
# 	[1996/06/06  16:37 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/2]
# 
# 	Try to fix RIOS build
# 	[1996/06/06  16:13 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/1]
# 
# 	Try to fix RIOS build
# 	[1996/06/04  18:51 UTC  psn  /main/DCE_1.2.2/DCE122_PK/1]
# 
# 	Add build instructions for rsec_pk_cache_cstub
# 	[1996/04/23  19:18 UTC  aha  /main/DCE_1.2/aha_pk2/1]
# 
# 	Additions for public key login
# 	[1996/01/03  19:20 UTC  psn  /main/DCE_1.2/1]
# 
# Revision 1.2.27.2  1996/07/08  18:39:16  arvind
# 	no acf file for sec_pk_base.idl...
# 	[1996/06/11  18:58 UTC  sommerfeld  /main/DCE_1.2.2/DCE122_PK/sommerfeld_pk_indir/1]
# 
# 	Merge PSM, sec_pvtkey, sec_pubkey, dced, base defs for Public Key Login
# 	[1996/06/10  14:58 UTC  aha  /main/DCE_1.2.2/DCE122_PK/2]
# 
# 	Merge to DCE122_PK
# 	[1996/06/07  13:07 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/4]
# 
# 	Re-include dependencis of dacl_ifr.o and daclbind.o on
# 	rdaclif_cstub.c and rdaclifv0_cstub.c.
# 	[1996/06/06  17:43 UTC  arvind  /main/DCE_1.2.2/DCE122_PK/aha_pk6/3]
# 
# 	Try to fix AIX build.
# 	[1996/06/06  16:37 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/2]
# 
# 	Try to fix RIOS build
# 	[1996/06/06  16:13 UTC  aha  /main/DCE_1.2.2/DCE122_PK/aha_pk6/1]
# 
# 	Try to fix RIOS build
# 	[1996/06/04  18:51 UTC  psn  /main/DCE_1.2.2/DCE122_PK/1]
# 
# 	Additions for Public Key Login
# 
# 	Add sec_pubkey
# 	[1996/04/26  20:45 UTC  aha  /main/DCE_1.2/DCE12_PK/2]
# 
# 	Add sec_pk files
# 	[1996/04/23  20:02 UTC  aha  /main/DCE_1.2/DCE12_PK/1]
# 
# 	Add build instructions for rsec_pk_cache_cstub
# 	[1996/04/23  19:18 UTC  aha  /main/DCE_1.2/aha_pk2/1]
# 
# 	Additions for public key login
# 	[1996/01/03  19:20 UTC  psn  /main/DCE_1.2/1]
# 
# Revision 1.2.27.1  1996/06/04  22:04:27  arvind
# 	Add sec_krb_u2u.c
# 	[1996/05/06  20:56 UTC  burati  /main/DCE_1.2/3]
# 
# 	merge u2u changes (add sec_krb_u2u.c)
# 	[1996/05/02  15:53 UTC  burati  /main/DCE_1.2/mb_u2u/1]
# 
# 	Remove DES code from gssapi build.
# 	[1995/12/19  16:29 UTC  psn  /main/HPDCE02/jrr_1.2_mothra/1]
# 
# 	Binary Compatibility Merge
# 	[1995/04/03  22:14 UTC  mullan_s  /main/HPDCE02/1]
# 
# 	Extend INCFLAGS for priv_krb_util.o to pick up sec_login_db.h
# 	[1995/03/20  14:54 UTC  mullan_s  /main/mullan_mothra_bin_compat/mullan_mothra_bin_compat2/3]
# 
# 	For priv.c, extend include path to pick up sec_login_db.h
# 	[1995/01/26  20:59 UTC  mullan_s  /main/mullan_mothra_bin_compat/mullan_mothra_bin_compat2/2]
# 
# 	Add rules for sec_login_db.c
# 	[1995/01/11  18:17 UTC  mullan_s  /main/mullan_mothra_bin_compat/1]
# 
# Revision 1.2.24.4  1996/03/09  23:30:23  marty
# 	Update OSF copyright year
# 	[1996/03/09  22:45:41  marty]
# 
# Revision 1.2.24.3  1995/12/18  17:22:46  psn
# 	Submit OSF/DCE 1.2.1
# 
# 	HP revision /main/HPDCE02/3  1995/12/08  16:49 UTC  rps
# 	international build omission
# 	[1995/12/05  01:39 UTC  rps  /main/HPDCE02/rps_moth5/1]
# 
# 	HP revision /main/HPDCE02/2  1995/05/15  21:12 UTC  greg
# 	Create SEC_migrate_merge mainline from SEC_migrate and HPDCE02.
# 	[1995/04/22  13:37 UTC  greg  /main/HPDCE02/SEC_migrate_merge/1]
# 
# 	HP revision /main/HPDCE02/1  1995/04/03  22:14 UTC  mullan_s
# 	Binary Compatibility Merge
# 
# 	HP revision /main/mullan_mothra_bin_compat/mullan_mothra_bin_compat2/3  1995/03/20  14:54 UTC  mullan_s
# 	Extend INCFLAGS for priv_krb_util.o to pick up sec_login_db.h
# 
# 	HP revision /main/mullan_mothra_bin_compat/mullan_mothra_bin_compat2/2  1995/01/26  20:59 UTC  mullan_s
# 	For priv.c, extend include path to pick up sec_login_db.h
# 
# 	HP revision /main/mullan_mothra_bin_compat/1  1995/01/11  18:17 UTC  mullan_s
# 	Add rules for sec_login_db.c
# 	[1995/12/18  17:21:53  psn]
# 
# Revision 1.2.19.15  1994/09/07  15:48:39  marty
# 	CR # 11497 - Remove '-v' option from IDLFLAGS defineittion.
# 	[1994/09/06  21:39:35  marty]
# 
# Revision 1.2.19.14  1994/08/08  14:23:50  mdf
# 	Hewlett Packard Security Code Drop
# 	[1994/08/04  19:24:36  mdf]
# 
# Revision 1.2.19.12  1994/07/17  20:22:31  baum
# 	Added dependency sec_pwd_mgmt.o:rs_pwd_mgmt.h
# 	[1994/07/17  20:12:01  baum]
# 
# Revision 1.2.19.11  1994/07/15  15:03:07  mdf
# 	Hewlett Packard Security Code Drop
# 	[1994/07/14  17:24:07  mdf]
# 
# Revision 1.2.19.10  1994/06/28  20:01:00  wray
# 	GSSAPI BL3 (OT 10824)
# 	[1994/06/28  19:25:20  wray]
# 
# Revision 1.2.19.9  1994/06/02  21:17:51  mdf
# 	hp_sec_to_osf_3 drop, merge up with latest.
# 	[1994/05/24  15:20:45  mdf]
# 
# 	hp_sec_to_osf_3 drop
# 
# Revision 1.2.19.8  1994/05/12  18:07:27  ahop
# 	state explicit dependencies for priv.o and sec_login_pvt.o
# 	[1994/05/12  18:04:56  ahop]
# 
# Revision 1.2.19.7  1994/05/11  19:33:46  ahop
# 	hp_sec_to_osf_2 drop
# 	fix: PTGT refresh now takes delegation into account
# 	build sec_login_encode interface
# 	Add rpriv_1_0.idl
# 	build authz_encode_cstub.o
# 	rm sec_attr_trig.idl, sec_attr_base.idl, add IDLINCFLAGS for attr stuff
# 	[1994/04/29  21:49:33  ahop]
# 
# Revision 1.2.19.6  1994/03/25  20:30:06  sekhar
# 	Fixed errors due to merge.
# 	[1994/03/25  20:21:54  sekhar]
# 
# Revision 1.2.19.5  1994/03/25  18:38:17  sekhar
# 	Merged with changes from 1.2.19.4
# 	[1994/03/25  18:38:01  sekhar]
# 
# 	[ OT 10164 ]  Security SVC code drop ( sec_svc_bl1 )
# 	[1994/03/25  18:31:03  sekhar]
# 
# Revision 1.2.19.4  1994/03/23  16:47:32  wray
# 	GSSAPI BL2 (OT#10179)
# 	[1994/03/23  16:44:05  wray]
# 
# 	GSSAPI BL2 (OT#10179)
# 
# Revision 1.2.19.3  1994/03/09  15:50:59  wray
# 	Add GSSAPI (OT#10019)
# 	[1994/03/09  15:39:16  wray]
# 
# Revision 1.2.19.2  1994/03/02  17:56:14  hanfei
# 	era initial drop.
# 	[1994/03/01  15:58:20  hanfei]
# 
# Revision 1.2.19.1  1994/01/28  23:11:27  burati
# 	Add dependency on rdacl stubs to dacl_ifr.o and daclbind.o
# 	so the ACF'd hdr file gets built before it needs to be included
# 	[1994/01/21  21:18:56  burati]
# 
# 	Dlg/EPAC changes - Add rdaclifv0.*, id_encode.*/sec_encode.* (dlg_bl1)
# 	[1994/01/20  02:42:53  burati]
# 
# Revision 1.2.17.1  1993/09/13  15:52:59  delgado
# 	More DFS 486 changes - change TARGET_MACHINE == PMAX to
# 	TARGET_OS == OSF1
# 	[1993/09/13  15:48:14  delgado]
# 
# Revision 1.2.15.2  1993/07/19  19:38:32  zeliff
# 	HP port of DFS
# 	[1993/07/19  18:29:48  zeliff]
# 
# Revision 1.2.13.3  1993/07/16  21:16:00  kissel
# 	Change sense of NO_LATENT_DFS_SUPPORT to LATENT_DFS_SUPPORT
# 	[1993/06/21  14:47:19  kissel]
# 
# Revision 1.2.11.2  1993/06/04  20:20:51  robinson
# 	Update with HP product sources.
# 	[1993/06/04  20:01:10  robinson]
# 
# Revision 1.2.9.8  1993/01/29  18:51:31  burati
# 	Changes for replication client agent
# 	[1993/01/14  23:21:57  burati]
# 
# Revision 1.2.9.7  1992/12/29  16:40:09  zeliff
# 	Embedding copyright notice
# 	[1992/12/28  21:31:17  zeliff]
# 
# Revision 1.2.9.6  1992/12/08  19:02:22  rps
# 	Add rs_bind.
# 	[1992/12/03  01:26:58  rps]
# 
# Revision 1.2.9.5  1992/12/04  21:31:25  burati
# 	CR5978 Move OS dependent sec_login code to machine specific dirs
# 	[1992/12/04  16:46:11  burati]
# 
# Revision 1.2.9.4  1992/11/12  18:08:49  rsarbo
# 	Include i/f's in libdce for Episode ACL
# 	support
# 	[1992/11/12  18:05:13  rsarbo]
# 
# 	turn on DFS ACL's on PMAX
# 	[1992/11/09  14:59:38  rsarbo]
# 
# Revision 1.2.9.3  1992/10/20  19:40:29  rsarbo
# 	update file specific CFLAGS for Episode ACL's.
# 	[1992/10/20  19:40:00  rsarbo]
# 
# Revision 1.2.9.2  1992/10/16  17:47:53  rsarbo
# 	added file-specific CFLAGS dependent on NO_DFS (moved from security.mk)
# 	[1992/10/09  18:19:38  rsarbo]
# 
# Revision 1.2.4.7  1992/06/30  21:54:34  burati
# 	CR4516 HP/UX porting
# 	[1992/06/30  04:43:19  burati]
# 
# Revision 1.2.4.6  1992/06/02  21:18:40  sekhar
# 	Merged Unix interface changes made by frisco
# 	to the mainline.
# 
# 	    ========== Start of Merged changes ===============
# 	    Revision 1.2.7.2  1992/05/22  19:53:22  frisco
# 	    Add Unix interface support
# 	    ========== End of Merged changes ================
# 	[1992/06/02  20:30:18  sekhar]
# 
# Revision 1.2.4.5  1992/05/11  17:51:31  mhickey
# 	Removed obsolete define of NEED_PROJECT_MAKE_DEFS
# 	and include of osf.dce.mk.  Fixes defect 2031.
# 	[1992/04/22  15:23:26  mhickey]
# 
# Revision 1.2.5.2  1992/04/15  21:45:56  sommerfeld
# 	Add sec_login_cache.o
# 
# Revision 1.2.4.4  1992/05/08  20:56:22  sommerfeld
# 	Add sec_login_cache.o_INCFLAGS
# 	[1992/04/30  16:51:07  sommerfeld]
# 
# 	Merge in 1.0.1 changes after b9.
# 	[1992/04/17  19:49:12  sommerfeld]
# 
# Revision 1.2.4.3  1992/04/07  19:19:26  mckeen
# 	Added rpladmn.o dependency on rs_rpladmn.h
# 	[1992/03/13  15:47:13  mckeen]
# 
# Revision 1.2.4.2  1992/03/31  22:45:12  burati
# 	CR2419 Add sec_lrgy.o for local registry support
# 	[1992/03/31  01:34:39  burati]
# 
# Revision 1.2  1992/01/19  22:13:53  devrcs
# 	Dropping DCE1.0 OSF1_misc port archive
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.7 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/security/utils
.PATH:$(KPATH)

TARGETS=

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!

