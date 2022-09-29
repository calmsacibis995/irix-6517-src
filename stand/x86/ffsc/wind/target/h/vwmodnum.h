/* vwModNum.h - VxWorks module numbers */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
03l,04nov94,dzb  added M_mbufLib and M_pingLib.
03k,15oct93,cd   added M_loadElfLib.
03j,04nov93,yao  added M_loadSomCoffLib.
03i,22apr94,jmm  added M_nfsdLib and M_mountLib
03h,14feb94,jag  added M_ for MIB-II library.
03g,22sep92,rrr  added support for c++
03f,01aug92,srh  added M_cxxLib
03e,19jul92,pme  added M_smObjLib and M_smNameLib.
03d,15jul92,jmm  added M_moduleLib and M_unldLib.
03c,08jul92,rdc  added M_vmLib and M_mmuLib.
03b,30jun92,wmd  added M_loadCoffLib.
03a,09jun92,ajm  added M_loadEcoffLib, M_loadAoutLib, M_loadBoutLib,
		  and M_bootLoadLib
02z,02jun92,elh  added M_smPktLib, and M_smUtilLib, renamed M_shMemLib
		 to M_smLib.
02y,26may92,rrr  the tree shuffle
02x,28feb92,elh  added M_bootpLib, M_tftpLib, and M_icmpLib
02w,04feb92,elh  added M_shMemLib, changed year on copyright
02v,07jan92,elh  added M_arpTblLib
02u,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
02t,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02s,02oct90,kdl	 added M_rawFsLib.
02r,09aug90,kdl	 changed M_msdosLib to M_dosFsLib, changed M_rt11Lib to
		 M_rt11FsLib.
02q,18jul90,jcf	 added M_cacheLib
02p,26jun90,jcf	 removed M_semCore, M_semILib. added M_semLib.
02o,04may90,kdl	 added M_dirLib.
02n,18mar90,jcf  added M_hashLib, M_qLib, M_tickLib, M_objLib,
		  M_qPriHeapLib, M_qPriBMapLib, M_bufLib,
		  M_msgQLib, M_classLib
02m,16mar90,kdl  added M_scsiLib, M_msdosLib.
	    rdc  added M_selectLib.
02l,13mar90,shl  added M_loginLib.
02k,27jul89,hjb  added M_if_sl.
02j,25may89,gae  added M_ftnLib.
02i,04jun88,dnw  changed ldLib->loadLib, rtLib->rt11Lib, stsLib->errnoLib.
		 changed M_errno, M_taskLib, M_vrtx, M_psos.
02h,29may88,dnw  added M_sigLib.
		 deleted some obsolete modules.
02g,28may88,dnw  added M_hostLib.
02f,29apr88,gae  added M_stsLib.
02e,19apr88,llk  added M_nfsDrv, M_nfsLib, M_rpcClntStat, M_nfsStat.
02d,25jan88,jcf  added M_taskLib.
02c,01nov87,llk  added M_inetLib and M_routeLib.
...pre 1987 mod history removed.  See RCS.
*/

#ifndef __INCvwModNumh
#define __INCvwModNumh

#ifdef __cplusplus
extern "C" {
#endif

/* module numbers - DO NOT CHANGE NUMBERS! Add or delete only! */
#define M_errno		(0 << 16)	/* THIS MUST BE ZERO! */
#define M_kernel	(1 << 16)
#define M_taskLib	(3 << 16)
#define M_dbgLib	(4 << 16)
#define M_dsmLib	(7 << 16)
#define M_fioLib	(9 << 16)
#define M_ioLib		(12 << 16)
#define M_iosLib	(13 << 16)
#define M_loadLib	(14 << 16)
#define M_lstLib	(15 << 16)
#define M_memLib	(17 << 16)
#define M_rngLib	(19 << 16)
#define M_rt11FsLib	(20 << 16)
#define M_rt11ULib	(21 << 16)
#define M_semLib	(22 << 16)
#define M_vwModNum	(27 << 16)
#define M_symLib	(28 << 16)
#define M_tyLib		(31 << 16)
#define M_wdLib		(34 << 16)
#define M_usrLib	(35 << 16)
#define M_remLib	(37 << 16)
#define M_netDrv	(41 << 16)
#define M_inetLib	(43 << 16)
#define M_routeLib	(44 << 16)
#define M_nfsDrv	(45 << 16)
#define M_nfsLib	(46 << 16)
#define M_rpcClntStat	(47 << 16)
#define M_nfsStat	(48 << 16)
#define M_errnoLib	(49 << 16)
#define M_hostLib	(50 << 16)
#define M_sigLib	(51 << 16)
#define M_ftnLib	(52 << 16)
#define M_if_sl		(53 << 16)
#define M_loginLib      (54 << 16)
#define M_scsiLib       (55 << 16)
#define M_dosFsLib      (56 << 16)
#define M_selectLib	(57 << 16)
#define M_hashLib	(58 << 16)
#define M_qLib		(59 << 16)
#define M_tickLib	(60 << 16)
#define M_objLib	(61 << 16)
#define M_qPriHeapLib	(62 << 16)
#define M_qPriBMapLib	(63 << 16)
#define M_bufLib	(64 << 16)
#define M_msgQLib	(65 << 16)
#define M_classLib	(66 << 16)
#define M_intLib	(67 << 16)
#define M_dirLib	(68 << 16)
#define M_cacheLib	(69 << 16)
#define M_rawFsLib	(70 << 16)
#define M_arpLib	(71 << 16)
#define M_smLib		(72 << 16)
#define M_bootpLib	(73 << 16)
#define M_icmpLib	(74 << 16)
#define M_tftpLib	(75 << 16)
#define M_proxyArpLib	(76 << 16)
#define M_smUtilLib     (77 << 16)
#define M_smPktLib      (78 << 16)
#define M_loadEcoffLib  (79 << 16)
#define M_loadAoutLib   (80 << 16)
#define M_loadBoutLib   (81 << 16)
#define M_bootLoadLib   (82 << 16)
#define M_loadCoffLib   (83 << 16)
#define M_vmLib   	(84 << 16)
#define M_mmuLib   	(85 << 16)
#define M_moduleLib     (86 << 16)
#define M_unldLib       (87 << 16)
#define M_smObjLib      (88 << 16)
#define M_smNameLib     (89 << 16)
#define M_cplusLib	(90 << 16)
#define M_m2Lib		(91 << 16)
#define M_aioPxLib      (92 << 16)
#define M_loadAoutHppaLib (93 << 16)
#define M_mountLib	(94 << 16)
#define M_nfsdLib	(95 << 16)
#define M_loadSomCoffLib (96 << 16)
#define M_loadElfLib	(97 << 16)
#define M_mbufLib       (98 << 16)
#define M_pingLib       (99 << 16)

#ifdef __cplusplus
}
#endif

#endif /* __INCvwModNumh */
