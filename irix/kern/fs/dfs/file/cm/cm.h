/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm.h,v 65.7 1999/07/21 19:01:14 gwehrman Exp $ */

#ifndef	TRANSARC_CM_H_
#define	TRANSARC_CM_H_
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#ifdef SGIMIPS
#ifndef _TAKES
#define _TAKES(x) x
#endif
#ifdef _KERNEL
#include <dcedfs/stds.h>                /* To pick up opaque definition */
#include <sys/types.h>                  /* To pick up typedefs for systm.h */
#include <sys/systm.h>                  /* To pick up remapf prototype */
#define cm_printf printf
#else
#include <sys/mount.h>
#endif
#endif

#include <dcedfs/common_data.h>
#include <dcedfs/osi.h>
#include <dcedfs/queue.h>
#include <dcedfs/lock.h>
#include <dcedfs/debug.h>
#include <dcedfs/icl.h>
#include <dcedfs/cm_sysname.h>
#include <dcedfs/cm_trace.h>
#include <dcedfs/cm_chunk.h>
#include <dcedfs/afs4int.h>
/*
 * Special CM system call, afscall_cm, opcodes: Some, like the first three,
 * startup special cm-related daemon processes and others do special system
 * related tasks;  afsd(1) is the main client to these calls.
 */

#define	CMOP_START_TKN		0	/* parm 2 = # procs */
#define	CMOP_START_MAIN		1	/* parm 2 = BioDaemon arg */
#define	CMOP_START_BKG		2	/* no aux parms */
/*#define	CMOP_ADDCELL		3*/	/* now it is obsolete ! */
#define	CMOP_CACHEINIT		4	/* parm 2 = cm_cacheparams struct */
#define	CMOP_CACHEINFO		5	/* the cacheinfo file */
#define	CMOP_VOLUMEINFO		6	/* the volumeinfo file */
#define	CMOP_CACHEFILE		7	/* a random cache file (V*) */
#define	CMOP_CACHEINODE		8	/* random cache file by inode */
#if 0		/* no longer a DFSLog file */
#define	CMOP_AFSLOG		9	/* output log file */
#endif
#define	CMOP_ROOTVOLUME		10	/* explicit root volume name */
#if 0		/* no more DFSLog */
#define	CMOP_STARTLOG		11	/* temporary: Start afs logging */
#define	CMOP_ENDLOG		12	/* temporary: End afs logging */
#endif
#define	CMOP_SHUTDOWN		13	/* Totally shutdown dfs */
#define CMOP_ROOTCELL		14	/* the cell name for explicit roots */

/*
 * The following macro (CMOP_GO) handles racing conditions when initializing
 * the cache manager. Particularly, cm_initState (tells which state of the
 * initialization we're in) has the following meaning:
 *	- in the range 0 < x < CMOP_GO are early initialization states.
 *	- of CMOP_GO means that the cache may be initialized.
 *      - of CMOP_GO+1 means a CMOP_GO operation has been done and all the
 *        cache initialization has been done.
 */
#define	CMOP_GO			100	/* all init is being done */

/*
 * General Sleep (1000 sec) macro
 */
#define	CM_SLEEP(at)		osi_Wait((at)*1000, 0, 0)
#define	CM_SLEEP_INTR(at)	osi_Wait((at)*1000, 0, 1)

/*
 * Some general definitions
 */
#define	CM_ZEROS		64	/* 'zero's buffer */

/*
 * Chunk-related definitions
 */
#define	CM_DEFCHUNK		16	/* Log2 of default chunk size (64K) */
#define	CM_MINCHUNK		13	/* 8K is the minimum chunk size */
#define	CM_MAXCHUNK		18	/* 256K is the maximum chunk size */
#define	CM_NOCHUNKING		0x40000000 /* too high: no chunking is used */


/*
 * Macros to uniquely identify the AFS vfs struct
 */
#define	AFS_VFSMAGIC		0x1234	/* fsid.val[0] */
#define	AFS_VFSFSID		OSI_MOUNT_TYPE_DFS

/* flag values for disklessFlag field */
#define CM_INIT_MEMCACHE	1	/* diskless system */

/*
 * This structure refers to cm's globals that can be dynamically assigned
 * via 'dfsd'
 */
/* Define the maximum and minimum levels we know about. */
#define	CM_MIN_SECURITY_LEVEL	(rpc_c_protect_level_none)
#define	CM_MAX_SECURITY_LEVEL	(rpc_c_protect_level_pkt_privacy)
/* Now the defaults for the cm_security_bounds structures */
#ifdef CM_DEFAULT_LCLINIT_PROTECTION
#define	CM_DEFAULT_LCLINIT_LEVEL	(CM_DEFAULT_LCLINIT_PROTECTION)
#else /* CM_DEFAULT_LCLINIT_PROTECTION */
#define	CM_DEFAULT_LCLINIT_LEVEL	(rpc_c_protect_level_pkt_integ)
#endif /* CM_DEFAULT_LCLINIT_PROTECTION */

#ifdef CM_DEFAULT_LCLMIN_PROTECTION
#define	CM_DEFAULT_LCLMIN_LEVEL	(CM_DEFAULT_LCLMIN_PROTECTION)
#else /* CM_DEFAULT_LCLMIN_PROTECTION */
#define	CM_DEFAULT_LCLMIN_LEVEL	(rpc_c_protect_level_none)
#endif /* CM_DEFAULT_LCLMIN_PROTECTION */

#ifdef CM_DEFAULT_RMTINIT_PROTECTION
#define	CM_DEFAULT_RMTINIT_LEVEL	(CM_DEFAULT_RMTINIT_PROTECTION)
#else /* CM_DEFAULT_RMTINIT_PROTECTION */
#define	CM_DEFAULT_RMTINIT_LEVEL	(rpc_c_protect_level_pkt_integ)
#endif /* CM_DEFAULT_RMTINIT_PROTECTION */

#ifdef CM_DEFAULT_RMTMIN_PROTECTION
#define	CM_DEFAULT_RMTMIN_LEVEL	(CM_DEFAULT_RMTMIN_PROTECTION)
#else /* CM_DEFAULT_RMTMIN_PROTECTION */
#define	CM_DEFAULT_RMTMIN_LEVEL	(rpc_c_protect_level_pkt)
#endif /* CM_DEFAULT_RMTMIN_PROTECTION */

struct cm_security_bounds {
    unsigned32 initialProtectLevel;
    unsigned32 minProtectLevel;
};
#ifdef SGIMIPS
/*
 * Change so that struct is same in both 64bit as well as 32 bit.
 * All our daemons are 32 bit so we can work with 32 bit quantities.
 */
struct cm_cacheparams {
    int cacheScaches;
    int cacheFiles;
    int cacheBlocks;
    int cacheDcaches;
    int cacheVolumes;
    int chunkSize;
    int setTimeFlag;
    int disklessFlag;
    int namecachesize;
    struct cm_security_bounds localSec;
    struct cm_security_bounds nonLocalSec;
    int spare1;
    int spare2;
    int spare3;
    int spare4;
    int spare5;
    int spare6;
  };
#else  /* SGIMIPS */
struct cm_cacheparams {
    long cacheScaches;
    long cacheFiles;
    long cacheBlocks;
    long cacheDcaches;
    long cacheVolumes;
    long chunkSize;
    long setTimeFlag;
    long disklessFlag;
    long namecachesize;
    struct cm_security_bounds localSec;
    struct cm_security_bounds nonLocalSec;
    long spare1;
    long spare2;
    long spare3;
    long spare4;
    long spare5;
    long spare6;
  };
#endif  /* SGIMIPS */

/* For VIOC_{G,S}ETPROTBNDS pioctls */
struct cm_ProtBounds {
    unsigned32 formatTag;
    struct cm_security_bounds local, nonLocal;
};

/* For VIOC_AFS_CREATE_MT_PT pioctl */
struct cm_CreateMountPoint {
    unsigned32 nameOffset;
    unsigned32 nameLen;
    unsigned32 nameTag;
    unsigned32 pathOffset;
    unsigned32 pathLen;
    unsigned32 pathTag;
    /* data for name and path follows here */
};

/*
 * These error codes should look like normal RPC error codes (because
 * cm_Analyze will handle them), but they are used only within the CM.  So,
 * instead of allocating them from an error code space known to the server
 * (the VOLERR codes), we use a small crawl space above VOLERR_TRANS_HIGHEST.
 *
 * The first few slots of this space are reserved for transient errors, so
 * that transient error checking can be done cheaply (see cm_Analyze and
 * cm_ConnByMHosts).
 */
#define CM_ERR_TRANS_SERVERTSR	(VOLERR_TRANS_HIGHEST+1)
#define CM_ERR_TRANS_CLIENTTSR	(VOLERR_TRANS_HIGHEST+2)
#define CM_ERR_TRANS_MOVE_TSR	(VOLERR_TRANS_HIGHEST+3)
#define CM_ERR_TRANS_HIGHEST	(VOLERR_TRANS_HIGHEST+4)

/* code for cm_CheckVolSync() to return that will make for a fast retry */
#define CM_REP_ADVANCED_AGAIN (VOLERR_TRANS_HIGHEST+5)
/* code for cm_ConnByHost() that will look like a persistent error */
#define CM_ERR_PERS_CONNAUTH (VOLERR_TRANS_HIGHEST+6)


#ifdef KERNEL

/*
 * Include dependencies to other afs modules
 */
#include <dcedfs/osi_uio.h>
#include <dcedfs/osi_buf.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/osi_dfserrors.h>
#include <dcedfs/bomb.h>
#include <dcedfs/afsvl_data.h>

/*
 * Include often needed cm-related headers
 */
#include <cm_rrequest.h>
#include <cm_scache.h>
#include <cm_dcache.h>
#include <cm_tokens.h>
#include <cm_cell.h>

#ifdef CM_DISKLESS_SUPPORT
/*
 * The following data structure is to support the diskless implementation. It
 * is referenced in cm_init.c and cm_vdirent.c
 */
struct cm_dlinfo {
   int nAddrs;
   struct sockaddr_in *serverAddr[MAXVLHOSTSPERCELL];
   long flags;
   afsUUID uuid;
   char *principal[MAXVLHOSTSPERCELL];
};
#endif /* CM_DISKLESS_SUPPORT */

/* This is maximum fid size supported for this platform. */
#if defined(AFS_AIX_ENV) || defined(AFS_HPUX_ENV)
#define	CM_MAXFIDSZ 10
#else	/* AFS_AIX_ENV */
#ifdef	AFS_SUNOS5_ENV
/* On Solaris, also check against NFS_FHMAXDATA of 10 */
#define	CM_MAXFIDSZ (10 /*NFS_FHMAXDATA*/)
#else	/* AFS_SUNOS5_ENV */
/*
 * We default CM_MAXFIDSZ to the max for your platform,
 * and guess that this value is MAXFIDSZ.  However, your
 * NFS server implementation may require a lower limit.
 */
#define	CM_MAXFIDSZ MAXFIDSZ
#endif	/* AFS_SUNOS5_ENV */
#endif	/* AFS_AIX_ENV || AFS_HPUX_ENV */

#if	CM_MAXFIDSZ<10
#error	"You may need to provide a struct SmallFid for your platform"
#endif

#if	CM_MAXFIDSZ<20
/*
 * Smaller Fid struct: used to pass enough info
 * between NFS and AFS (i.e. see cm_fid and cm_vget calls); For NFS we can
 * only pass a maximum of 10 bytes for a handle (we ideally need 16!)
 * The fields are divided in the following way:
 *	Unique: 32 bits
 *	Cell:   8 bits (255 cells)
 *	Volume: 20 bits (1 meg volumes)
 *	Vnode:  20 bits (1 meg files)
 */
struct SmallFid {
	long Unique;
	long CellVolAndVnodeHigh;
	unsigned short Vnodelow;
};

#define SIZEOF_SMALLFID 10 /* sizeof(struct SmallFid) without padding */

#endif	/* #if	CM_MAXFIDSZ<20 */

#if	CM_MAXFIDSZ>=24
/* Just use the native ``struct afsFid'' structure! */
#define	SIZEOF_SMALLFID 24
#else /* CM_MAXFIDSZ<24 */

#if	CM_MAXFIDSZ>=20

/*
 * Small Fid struct: used to pass enough info
 * between NFS and AFS (i.e. see cm_fid and cm_vget calls);
 * If your platform can accomodate a 20 byte FID,
 * you can compress the DFS fid into this form:
 */
#ifdef SGIMIPS64
struct SmallFid {
	afs_hyper_t Cell;
	int Volume;
	int Vnode;
	int Unique;
};
#else  /* SGIMIPS64 */
struct SmallFid {
	afs_hyper_t Cell;
	long Volume;
	long Vnode;
	long Unique;
};
#endif /* SGIMIPS64 */

#define SIZEOF_SMALLFID 20 /* sizeof(struct SmallFid) without padding */

#endif	/* CM_MAXFIDSZ>=20 */
#endif	/* CM_MAXFIDSZ>=24 */

/*
 * The following data structure is used to pass more than one argument
 * to a function that is executed by a thread.
 */
#ifdef SGIMIPS /* 64-bit port */
struct cm_params {
   unsigned long param1;
   unsigned long param2;
   unsigned long param3;
   unsigned long param4;
};
#else  /* SGIMIPS */
struct cm_params {
   unsigned32 param1;
   unsigned32 param2;
   unsigned32 param3;
   unsigned32 param4;
};
#endif /* SGIMIPS */

#ifdef SGIMIPS
#define	cm_vType(scp)	(scp)->vp->v_type
#define	cm_vSetType(scp,type)	(scp)->vp->v_type = (type)
#else  /* SGIMIPS */
#define	cm_vType(scp)	(scp)->v.v_type
#define	cm_vSetType(scp,type)	(scp)->v.v_type = (type)
#endif /* SGIMIPS */

/*
 * Globals/Functions exported by the cm module
 */

/*
 * Exported by cm_daemons.c
 */


/* extern struct osi_WaitHandle AFSWaitHandler; */
extern void cm_Daemon _TAKES((void));
extern int cm_CheckSize _TAKES((long));
extern void cm_CheckVolumeNames _TAKES((int));
extern void cm_ServerTokenMgt _TAKES((void));
extern void *cm_threadPoolHandle;
extern void *cm_auxThreadPoolHandle;

/*
 * Exported by cm_init.c
 */
#ifdef AFS_DEBUG
extern char *cm_debug;
#endif /* AFS_DEBUG */

extern char cm_zeros[CM_ZEROS];
extern char *cm_sysname;
extern char cm_rootCellName[], cm_rootVolumeName[] ;
extern int cm_initState;
extern long cm_setTime;
extern struct osi_file *cm_cacheFilep;
#ifdef AFS_VFSCACHE
extern struct fid cm_cacheHandle, cm_volumeHandle;
extern struct osi_vfs *cm_cache_mp;
#else
extern struct osi_dev cacheDev;
extern long cm_cacheInode, cm_volumeInode;
#endif /* AFS_VFSCACHE */
extern long cm_trySmush;
extern long cm_cacheCounter;
extern long cm_cacheFiles, cm_cacheBlocks, cm_origCacheBlocks, cm_cacheVolumes;
extern long cm_cacheScaches, cm_cacheDcaches, cm_blocksUsed;
extern long cm_othercsize, cm_logchunk, cm_firstcsize;
extern long cm_cacheUnit;
extern struct icl_set *cm_iclSetp;
extern void cm_DoInitDLUnInit _TAKES((void));
extern struct xvfs_vnodeops *cm_GetAFSOps _TAKES((void));
extern struct cm_security_bounds cm_localSec;
extern struct cm_security_bounds cm_nonLocalSec;
extern void cm_BasicInit _TAKES((void));
extern int cm_SetSecBounds _TAKES((struct cm_security_bounds *,
				    struct cm_security_bounds *, int));

/*
 * Exported by cm_pioctl.c
 */
extern int afscall_cm_pioctl _TAKES((long, long, long, long, long, int *));
/*
 * Exported by cm_shutdown.c
 */
extern int cm_shutdown _TAKES((void));

/*
 * Exported by cm_server.c
 */
extern void cm_PrintServerText _TAKES((char *, struct cm_server *, char *));

/*
 * Exported by cm_subr.c
 */
extern void cm_QuickDiscard(struct cm_dcache *);
extern struct lock_data cm_ftxtlock;
extern int cm_GetAccessBits _TAKES ((struct cm_scache *, long, int *, int *,
				     struct cm_rrequest *, int, int *));
extern int cm_AccessError _TAKES ((struct cm_scache *, long,
				   struct cm_rrequest *));
extern int cm_EvalMountPoint _TAKES((struct cm_scache *, struct cm_scache *,
				 struct cm_rrequest *));
extern void cm_SetMountPointDotDot _TAKES((struct cm_scache *,
				struct cm_scache *, struct cm_scache *));
extern int cm_GetMountPointDotDot _TAKES((struct cm_scache *, afsFid *,
				struct cm_scache **, struct cm_rrequest *));
extern int cm_HandleLink _TAKES((struct cm_scache *, struct cm_rrequest *));
extern int cm_Active _TAKES((struct cm_scache *));
extern void cm_FlushText _TAKES((struct cm_scache *));
extern void cm_PutActiveRevokes _TAKES((struct cm_scache *));
extern int cm_CheckVolSync _TAKES((afsFid *, afsVolSync *, struct cm_volume *,
				   u_long, int, struct cm_rrequest *));
extern void cm_AdjustSize _TAKES((struct cm_dcache *, long));
extern void cm_AdjustSizeNL _TAKES((struct cm_dcache *, long));
extern int cm_ReserveBlocks _TAKES((struct cm_scache *,
					struct cm_dcache *, long));
#ifdef SGIMIPS
extern int cm_LookupCookie _TAKES((struct cm_scache *, long, afs_hyper_t *));
extern void cm_FindClosestChunk _TAKES((struct cm_scache *, afs_hyper_t,
			long *));
extern void cm_EnterCookie _TAKES((struct cm_scache *, long, afs_hyper_t));
#else  /* SGIMIPS */
extern int cm_LookupCookie _TAKES((struct cm_scache *, long, long *));
extern void cm_FindClosestChunk _TAKES((struct cm_scache *, long, long *));
extern void cm_EnterCookie _TAKES((struct cm_scache *, long, long));
#endif /* SGIMIPS */
extern void cm_FreeAllCookies _TAKES((struct cm_scache *));
#ifdef SGIMIPS
extern void cm_BufAlloc _TAKES((rpc_ss_pipe_state_t, idl_ulong_int, idl_byte **,
			idl_ulong_int *));
#else  /* SGIMIPS */
extern void cm_BufAlloc _TAKES((void *, unsigned long, unsigned char **,
				unsigned long *));
#endif /* SGIMIPS */
extern void cm_StabilizeSCache _TAKES((struct cm_scache *));
#if 0
extern void cm_DirFetchProc _TAKES((void *, unsigned char *, unsigned long));
#endif /* 0 */
#ifdef SGIMIPS
extern void cm_CacheStoreProc _TAKES((rpc_ss_pipe_state_t, idl_byte *, idl_ulong_int,
				      idl_ulong_int *));
extern void cm_CacheFetchProc _TAKES((rpc_ss_pipe_state_t, idl_byte *, idl_ulong_int));
#else  /* SGIMIPS */
extern void cm_CacheStoreProc _TAKES((void *, unsigned char *, unsigned long,
				      unsigned long *));
extern void cm_CacheFetchProc _TAKES((void *, unsigned char *, unsigned long));
#endif /* SGIMIPS */

extern int cm_IsDevVP _TAKES((struct cm_scache *));
extern struct cm_scache *cm_MakeDev _TAKES((struct cm_scache *, long));
#ifndef SGIMIPS
extern void cm_printf();
#endif /* ! SGIMIPS */
extern void cm_TranslateStatus _TAKES((struct vattr *, afsStoreStatus *, int));
extern void cm_GetReturnOpenToken _TAKES((struct cm_scache *, char *,
					  afs_hyper_t *));
#ifdef SGIMIPS
extern int cm_TruncateFile _TAKES((struct cm_scache *, afs_hyper_t, 
				   osi_cred_t *,
				   struct cm_rrequest *, int));
#else  /* SGIMIPS */
extern int cm_TruncateFile _TAKES((struct cm_scache *, long, osi_cred_t *,
				   struct cm_rrequest *));
#endif /* SGIMIPS */

/*
 * Exported by cm_tknimp.c
 */
extern struct lock_data cm_qtknlock;
extern int cm_RevokeOneToken
    _TAKES((struct cm_scache *, afs_hyper_t *,
	    afs_hyper_t *, afs_recordLock_t *, afs_token_t *,
	    afs_token_t *, long *, int));
extern void cm_StripRedundantTokens
    _TAKES((struct cm_scache *, struct cm_tokenList *, long, int, int));
extern void cm_GetServerSize(struct cm_server *serverp);

/*
 * Exported by cm_tokens.c
 */
extern struct lock_data cm_tokenlock;

/* Define bits for reallyOK output parameter of cm_HaveTokensRange. */
#define CM_REALLYOK_REPRECHECK 1
#define CM_REALLYOK_LENINVAL 2

extern int cm_HaveTokens
    _TAKES((struct cm_scache *, long));
extern int cm_HaveTokensRange
    _TAKES((struct cm_scache *, afs_token_t *, int, int *reallOKp));

extern long cm_GetTokens _TAKES((struct cm_scache *, long,
				struct cm_rrequest *, int));
extern int cm_GetHereToken _TAKES((struct cm_volume *, struct cm_rrequest *,
				   long, long));
extern int cm_RestoreMoveTokens _TAKES((struct cm_volume *, struct cm_server *,
					struct cm_rrequest *, int));
extern int cm_RecoverTokenState _TAKES((struct cm_server *, long, unsigned32));
extern void cm_QueueAToken _TAKES((struct cm_server *, struct cm_tokenList *,
				   int));
extern void cm_RemoveToken _TAKES((struct cm_scache *, struct cm_tokenList *,
				  afs_hyper_t *));
extern void cm_FreeAllTokens _TAKES((struct squeue *));
extern int cm_ForceReturnToken _TAKES((struct cm_scache *, long, int));
extern int cm_SimulateTokenRevoke _TAKES((struct cm_scache *,
					  struct cm_tokenList *, int *));
extern void cm_FlushQueuedServerTokens _TAKES((struct cm_server *));
extern void cm_ReturnSingleOpenToken
   _TAKES((struct cm_scache *, afs_hyper_t *));
extern void cm_QuickQueueTokens _TAKES((struct cm_scache *));
extern int cm_TryDLock _TAKES((struct cm_scache *, struct cm_dcache *, int));

extern struct cm_tokenStat {
    long tokenAdded;
    long Lookup;
    long FetchStatus;
    long FetchData;
    long GetToken;
    long ReleaseTokens;
    long CreateFile;
    long SymLink;
    long Readdir;
} tokenStat;   /* Temp */

#ifdef EXPRESS_OPS
/*
 * Exported by cm_express.c
 */
extern long cm_ExpressPath _TAKES((struct cm_scache *, struct cm_scache *,
				   struct volume **, struct vnode **,
				   struct vnode **));
extern void cm_ExpressRele _TAKES((struct volume *, struct vnode *,
				   struct vnode *));
#endif /* EXPRESS_OPS */


/*
 * Exported by cm_vfsops.c
 */
extern struct osi_vfs *cm_globalVFS;

/*
 * Exported by cm_shutdown.c
 */
extern int cm_EnterTKN _TAKES((void));
extern void cm_LeaveTKN _TAKES((void));
extern void cm_EnterBKG _TAKES((void));
extern void cm_LeaveBKG _TAKES((void));
extern int cm_EnterVnode _TAKES((void));
extern void cm_LeaveVnode _TAKES((void));
extern void cm_WaitForShutdown _TAKES((void));
extern void cm_EnterDaemon _TAKES((void));
extern void cm_LeaveDaemon _TAKES((void));
extern void cm_UnShutdown _TAKES((void));

/*
 * Exported by cm_lockf.c
 */
extern int cm_TryLockRevoke _TAKES((struct cm_scache *,
				    afs_hyper_t *,
				    long,
				    afs_recordLock_t *,
				    afs_token_t *,
				    afs_token_t *,
				    long *,
				    struct cm_server *));

/*
 * Exported by cm_vnodeops.c
 */
extern struct xvfs_xops *cm_ops;
extern void cm_FlushExists _TAKES((struct cm_scache *, char *));
extern struct xvfs_vnodeops *xvfs_ops;
extern void cm_MergeStatus _TAKES((struct cm_scache *,
			  afsFetchStatus *,
			  afs_token_t *,
			  int,
			  struct cm_rrequest *));
extern int cm_UpdateStatus _TAKES((struct cm_scache *, afsFetchStatus *,
				   char *, char *, afsFid *,
				   struct cm_rrequest *, long));
extern int cm_setattr _TAKES((struct vnode *,
			      struct xvfs_attr *,
			      int,
			      osi_cred_t *));
extern int cm_nlrw _TAKES((struct vnode *,
			   struct uio *,
			   enum uio_rw,
			   int,
			   osi_cred_t *));
extern int32 cm_SymlinkOrMount _TAKES((struct cm_scache *,
				       char *, unsigned32, unsigned32,
				       char *, unsigned32, unsigned32,
				       int, struct cm_rrequest *));

/*
 * Exported by cm_vdirent.c
 */
extern void cm_VDirInit(void);
extern struct cm_scache *cm_GetVDirSCache(int);
extern void cm_ConfigRootVDir(struct cm_scache *);
#ifdef CM_DISKLESS_SUPPORT
extern int cm_ConfigDisklessRoot(char *, char *,
				 struct cm_scache **, struct cm_rrequest *);
#endif /* CM_DISKLESS_SUPPORT */
extern void cm_ResetAllVDirs(void);
extern long cm_GetAttrVDir(struct cm_scache *,
				struct vattr *, struct cm_rrequest *);
extern long cm_ReadVDir(struct cm_scache *, struct uio *, int *,
			struct cm_rrequest *);
extern long cm_LookupVDir(struct cm_scache *,
			  char *, struct cm_scache **, struct cm_rrequest *);
extern long cm_ConfigureCell(char *, struct cm_cell **, struct cm_rrequest *);
extern void cm_FreeAllVDirs(struct cm_scache *);

/*
 * Exported by cm_rrequest.c
 */
extern int cm_Analyze _TAKES((struct cm_conn *, long, afsFid *, struct cm_rrequest *,
			      struct cm_volume *, long, u_long));

/*
 * Exported by kutils/krpc_misc.c
 */
extern char protSeq[];
extern long krpc_InvokeServer _TAKES((char *, char *, rpc_if_handle_t, rpc_mgr_epv_t,
				      uuid_t *, long, int, char *, int *,
				      struct sockaddr_in *));
extern long krpc_BindingFromSockaddr _TAKES((char *, char *, afsNetAddr *, handle_t *));
extern long krpc_GetPrincName _TAKES((char *, char *, char *));
extern int dfsuuid_GetParity _TAKES((uuid_t *));
extern int dfsuuid_Create _TAKES((uuid_t *, int));

/* definitions and exports for VM */

#ifdef AFS_SUNOS5_ENV

extern int cm_VMWriteWholeFile _TAKES((struct cm_scache *, osi_cred_t *));
extern int cm_VMDiscardWholeFile _TAKES((struct cm_scache *));
extern int cm_VMDiscardWholeFileNoLock _TAKES((struct cm_scache *, int));
extern int cm_VMDiscardChunk _TAKES((struct cm_scache *, struct cm_dcache *, int));
extern void cm_VMPreTruncate _TAKES((struct cm_scache *, long));
extern int cm_VMFlushSCache _TAKES((struct cm_scache *));

#else

#define cm_VMWriteWholeFile(scp,credp)		0
#define cm_VMDiscardWholeFile(scp)		0
#define cm_VMDiscardWholeFileNoLock(scp,sync)	0
#define cm_VMDiscardChunk(scp,dcp,force)	0
#define cm_VMPreTruncate(scp,length)
#define cm_VMFlushSCache(scp)			0

#endif /* AFS_SUNOS5_ENV */

#if defined(AFS_SUNOS5_ENV) || defined(AFS_HPUX_ENV)

extern void cm_VMTruncate _TAKES((struct cm_scache *, long));

#else

#define cm_VMTruncate(scp,length)

#endif /* AFS_SUNOS_ENV, AFS_HPUX_ENV */

/* Other platform-dependent declarations */

#ifdef AFS_SUNOS5_ENV

extern void cm_CheckOpens _TAKES((struct cm_scache *, int));

extern void cm_sun_rwlock _TAKES((struct cm_scache *));
extern void cm_sun_rwunlock _TAKES((struct cm_scache *));

#else

#ifdef SGIMIPS
extern void cm_rwlock _TAKES((struct vnode *vp, vrwlock_t locktype));
extern void cm_rwunlock _TAKES((struct vnode *vp, vrwlock_t locktype ));
extern int cm_rwlock_nowait _TAKES((struct vnode *vp));
extern int cm_SetVolumeServerBadness _TAKES(( struct cm_volume *, 
                                      struct cm_server *, 
                                      long code, 
                                      long subCode ));

#else  /* SGIMIPS */
#define cm_sun_rwlock(scp)
#define cm_sun_rwunlock(scp)
#endif  /* SGIMIPS */

#endif /* AFS_SUNOS5_ENV */

#else	/* KERNEL */
/*
 * Exported by cm_init.c
 */
extern struct icl_set *cm_iclSetp;
#endif	/* KERNEL */
#endif	/* TRANSARC_CM_H_ */
