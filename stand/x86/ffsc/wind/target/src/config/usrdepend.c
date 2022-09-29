/* usrDepend.c - include dependency rules */

/* Copyright 1992-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01l,27sep95,ms   add INCLUDE_NET if WDB uses network communication (SPR 4506)
01k,28mar95,kkk  added INCLUDE_IO_SYSTEM dependency for INCLUDE_TTY_DEV
01j,27mar95,jag  added INCLUDE_NFS_SERVER dependancy INCLUDE_RPC
01i,22aug94,dzb  cut INCLUDE_NETWORK and STANDALONE ties (SPR #1147).
01h,03apr94,smb  added INCLUDE_NFS dependency for INCLUDE_MIB_VXWORKS.
01g,07dec93,smb  added INCLUDE_ WINDVIEW dependancy INCLUDE_RPC
01f,16aug93,jmm  including rdb now includes loader and unloader
01e,27feb93,rdc  changed logic of check for MMU_BASIC and MMU_FULL to include
		 FULL support if both defined.
01d,14nov92,jcf  guarded against definition of both _MMU_BASIC and _MMU_FULL.
01c,14nov91,kdl  made USER_D_CACHE_ENABLE definition depend on INCLUDE_BP_5_0
		 and SM_OFF_BOARD (SPR #1801).
01b,31oct91,jcf  removed INCLUDE_68881 definition.
01a,18sep92,jcf  written.
*/

/*
DESCRIPTION
This file is used to include all modules necessary given the desired
configuration specified by configAll.h and config.h.  For instance 
if INCLUDE_NFS is defined, and INCLUDE_RPC is not, then INCLUDE_RPC
is automatically defined here.  This file is included by usrConfig.c.

SEE ALSO: usrExtra.c

NOMANUAL
*/

#ifndef  __INCusrDependc 
#define  __INCusrDependc 

/* check include dependencies */

#if	defined(INCLUDE_MIB_VXWORKS)
#define INCLUDE_NFS             /* nfs package */
#endif /* INCLUDE_MIB_VXWORKS */

#ifdef DOC		/* include when building documentation */
#define INCLUDE_SCSI
#endif	/* DOC */

#if     defined(INCLUDE_WINDVIEW)
#define INCLUDE_RPC             /* rpc required by WindView */
#endif	/* INCLUDE_WINDVIEW */

#if     defined(INCLUDE_NFS_SERVER)
#define INCLUDE_RPC             /* rpc required by nfs */
#endif	/* INCLUDE_NFS_SERVER  */

#if     defined(INCLUDE_NFS) || defined(INCLUDE_RDB) 
#define INCLUDE_RPC             /* rpc required by rdb and nfs */
#endif	/* INCLUDE_NFS || INCLUDE_RDB */

#if	((defined(INCLUDE_RPC) || defined(INCLUDE_NET_SYM_TBL)) \
	 && !defined(INCLUDE_NET_INIT))
#define INCLUDE_NET_INIT	/* net init required by rpc and net sym tbl */
#endif	/* INCLUDE_RPC || INCLUDE_NET_SYM_TBL */

#if	defined(INCLUDE_WDB) && (WDB_COMM_TYPE == WDB_COMM_NETWORK)
#define INCLUDE_NET_INIT
#endif	/* defined(INCLUDE_WDB) && (WDB_COMM_TYPE == WDB_COMM_NETWORK) */

#if	defined(INCLUDE_NET_INIT) && !defined(INCLUDE_NETWORK)
#define INCLUDE_NETWORK		/* network required for net init */
#endif	/* INCLUDE_NETWORK */

#if	defined(INCLUDE_TTY_DEV) && !defined(INCLUDE_IO_SYSTEM)
#define INCLUDE_IO_SYSTEM       /* io system require for tty device */
#endif	/* INCLUDE_TTY_DEV && !INCLUDE_IO_SYSTEM */

#if	defined(INCLUDE_BP_5_0) && (SM_OFF_BOARD == FALSE)
#undef USER_D_CACHE_ENABLE	/* disable data cache if old bp on-board */
#endif  /* INCLUDE_BP_5_0 && !SM_OFF_BOARD */

#if     defined(INCLUDE_RDB)
#define INCLUDE_DEBUG           /* native debugging required by rdb */
#define INCLUDE_LOADER		/* loader and unloader required by rdb */
#define INCLUDE_UNLOADER
#endif	/* INCLUDE_RDB */

#if	defined(INCLUDE_FP_EMULATION)
#define INCLUDE_SW_FP		/* floating point software emulation */
#endif

#if	defined(INCLUDE_MMU_BASIC) && defined(INCLUDE_MMU_FULL)
#undef	INCLUDE_MMU_BASIC	/* leave only FULL defined. can't have both */
#endif

/* traditional STANDALONE configuration has network included but not
 * initialized, and standalone symbol table.  To include the network
 * modules, the INCLUDE_NETWORK conditional must be defined.
 */

#if	defined(STANDALONE)
#define	INCLUDE_STANDALONE_SYM_TBL
#undef	INCLUDE_NET_SYM_TBL

#ifndef PRODUCTION
#define	INCLUDE_SHELL
#define	INCLUDE_SHOW_ROUTINES
#define	INCLUDE_DEBUG
#endif  /* !PRODUCTION */

#ifndef STANDALONE_NET
#undef	INCLUDE_NET_INIT
#if     defined(INCLUDE_WDB) && (WDB_COMM_TYPE == WDB_COMM_NETWORK)
#undef	INCLUDE_WDB
#endif  /* defined(INCLUDE_WDB) && (WDB_COMM_TYPE == WDB_COMM_NETWORK) */
#endif	/* STANDALONE_NET */
#endif	/* STANDALONE */

#if     defined(INCLUDE_NET_SYM_TBL) || defined(INCLUDE_STANDALONE_SYM_TBL) || \
        defined(INCLUDE_SECURITY)
#define INCLUDE_SYM_TBL 	/* include symbol table package */
#endif	/* INCLUDE_NET_SYM_TBL||INCLUDE_STANDALONE_SYM_TBL||INCLUDE_SECURITY */

#endif /* __INCusrDependc */
