/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: px.h,v $
 * Revision 65.3  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.2  1998/04/01 14:16:06  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Removed prototypes for exports from kutils/krpc_misc.c and added
 * 	include for krpc_misc.h.  Added extern declarations for px_read()
 * 	and px_write().  Added extern declarations for additional functions
 * 	shared between modules.  Changed parameters for
 * 	pxint_BuldKeepAlive() prototype from unsigned long to unsigned32.
 *
 * Revision 65.1  1997/10/20  19:17:29  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.929.1  1996/10/02  18:12:50  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:34  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/px/RCS/px.h,v 65.3 1999/07/21 19:01:14 gwehrman Exp $ */

#ifndef	_PXH_
#define	_PXH_
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#ifdef KERNEL
#include <dcedfs/osi_uio.h>
#include <dce/ker/pthread.h> 
#include <dce/ker/exc_handling.h>
#include <px_trace.h>
#ifdef SGIMIPS
#include <dcedfs/krpc_misc.h>
#endif
#endif /* KERNEL */
#include <dcedfs/afsvl_data.h>
#include <dcedfs/tkm_tokens.h>		/* TKM_TOKEN_NOEXPTIME */
#include <dcedfs/xvfs_xattr.h>		/* struct xvfs_attr */
#include <dcedfs/tkset.h>		/* struct tkset_set */
#include <dcedfs/xvfs_vnode.h>                

extern struct icl_set *px_iclSetp;

struct px_removeItem {
    struct px_removeItem *next;		/* Next entry on list */
    afs_hyper_t tokenID;		        /* token we're dealing with */
    afsFid   fid;			/* file we are dealing with */
    struct vnode *vp;			/* associated vnode is held */
    short flags;			/* See below */
    short refCount;			/* reference counter */
};


/* 
 * Flags for px_removeItem flags field 
 */
#define PX_REMOVE_GRANTED	1	/* token has been granted */

struct px_remove {
    int queued;				/* true if the request was queued */
    afs_hyper_t tokenID;		        /* set to the token ID if not queued */
};


/*
 * General exporter stats
 */
struct afsIntStats
{
    /*
     * References to AFS interface calls 
     */
    unsigned long	SetContext;
    unsigned long	LookupRoot;
    unsigned long	FetchData;
    unsigned long	FetchACL;
    unsigned long	FetchStatus;
    unsigned long	StoreData;
    unsigned long	StoreACL;
    unsigned long	StoreStatus;
    unsigned long	RemoveFile;
    unsigned long	CreateFile;
    unsigned long	Rename;
    unsigned long	Symlink;
    unsigned long	Link;
    unsigned long	MakeDir;
    unsigned long	RemoveDir;
    unsigned long	Lookup;
    unsigned long	Readdir;
    unsigned long	SetLock;
    unsigned long	ExtendLock;	
    unsigned long	ReleaseLock;
    unsigned long	GetStatistics;
    unsigned long	ReleaseTokens;
    unsigned long	GetToken;
    unsigned long	GetTime;
    unsigned long	BulkStatus;
    unsigned long	BulkLookup;
    unsigned long	MakeMountPoint;
    unsigned long	RenewAllTokens;
    unsigned long	BulkFetchVV;
    unsigned long	BulkKeepAlive;
    unsigned long	SetParams;
    unsigned long	BulkFetchStatus;
    unsigned long	Spare1;
    unsigned long	Spare2;
    unsigned long	Spare3;
    unsigned long	Spare4;
    unsigned long	Spare5;
    unsigned long	Spare6;

    /*
     * General Fetch/Store Stats 
     */
    unsigned long	TotalCalls;
    unsigned long	TotalFetchedBytes;
    unsigned long	AccumFetchTime;
    unsigned long	FetchSize1;
    unsigned long	FetchSize2;
    unsigned long	FetchSize3;
    unsigned long	FetchSize4;
    unsigned long	FetchSize5;
    unsigned long	TotalStoredBytes;
    unsigned long	AccumStoreTime;
    unsigned long	StoreSize1;
    unsigned long	StoreSize2;
    unsigned long	StoreSize3;
    unsigned long	StoreSize4;
    unsigned long	StoreSize5;
};

/* Define the system call magic numbers here. */
#define	PXOP_PUTKEYS		31
#define	PXOP_INITHOST		32
#define	PXOP_RPCDAEMON		33
#define	PXOP_CHECKHOST		34
#define	PXOP_GO		35

/* #define	PXOP_AFSLOG		36	DEFUNCT */
/* #define	PXOP_STARTLOG		37	DEFUNCT */
/* #define	PXOP_ENDLOG		38	DEFUNCT */
#define	PXOP_DUMPPRINCS	39
#define	PXOP_GETSTATS		40
#define	PXOP_DUMPHOSTS		41
#define	PXOP_SHUTDOWN		42
#define PXOP_PUTSECADVICE       43
#define PXOP_GETSECADVICE       44

#define px_tcmp(a,b) ((a).osi_sec < (b).sec? -1 : ((a).osi_sec > (b).sec? 1 : \
    (osi_ToUTime((a)) < (b).usec? -1 : \
     (osi_ToUTime((a)) > (b).usec? 1 : 0))))

/* argument structuure for PXOP_INITHOST */
struct fsop_cell {
    afsNetAddr hosts[MAXVLHOSTSPERCELL];
    char cellName[160];
    afsUUID fsServerGp;
};

/* 
 * Argument structure for TSR recovery paramters
 */
struct recovery_params {
    u_long	lifeGuarantee;		/* host lifetime guarantee */
    u_long	RPCGuarantee;		/* host RPC guarantee */
    u_long	deadServer;		/* dead server polling interval */
    u_long	postRecovery;		/* Wait for the TSR work */
    u_long	maxLife;		/* maximum lifeGuarantee */
    u_long	maxRPC;			/* maximum RPCGuarantee */
};

/* Macro for what we'll export for an unbounded expiration time: 60 days */
#define	FX_BIG_EXPIRATION	5184000
/* Macro to adjust expiration times for exporting. */
#define	FX_FIX_EXPIRATION(exptime) \
	if (exptime == ((unsigned32) TKM_TOKEN_NOEXPTIME)) \
		exptime = FX_BIG_EXPIRATION; \
	else \
		exptime -= osi_Time();

#ifdef KERNEL
/*
 * Exported by px_intops.c
 */
extern struct afsIntStats afsStats;
extern char *px_debug;

/*
 * Exported by px_init.c
 */
extern long px_postRecovery;
extern struct icl_set *px_iclSetp;
extern struct lock_data px_tsrlock;

/*
 * Exported by px_subr.c
 */
extern int px_check_time_preserve _TAKES((afsStoreStatus *, struct vattr *,
					  struct vattr *));
extern int px_SetCreationParms _TAKES((struct vnode *, struct afsStoreStatus *,
				       struct vattr *));
extern int px_PreTruncate _TAKES((struct vnode *, afsStoreStatus *, osi_cred_t *));
extern void px_afsstatus_to_vattr _TAKES((afsStoreStatus *, struct vattr *,
					  struct vattr *));
extern int px_PreSetExistingStatus _TAKES((struct vnode *, afsStoreStatus *,
					   struct vattr *, struct xvfs_attr *,
					   osi_cred_t *));
extern int px_PostSetExistingStatus _TAKES((struct vnode *, afsStoreStatus *,
					    struct vattr *, struct xvfs_attr *, osi_cred_t *));
extern int px_SetSync(struct volume *, afsVolSync *, int, osi_cred_t *);
extern int px_AdjustCell _TAKES((afsFid *));
extern int px_ClientHasTokens _TAKES((struct hs_host *, afsFid *));
extern int px_rdwr _TAKES((struct vnode *, enum uio_rw, afs_hyper_t *, long,
			   osi_cred_t *, pipe_t *, int, int *, struct volume *,
			   int *, opaque * volatile));
#ifdef SGIMIPS
extern px_read _TAKES((struct vnode *, afs_hyper_t *, long, struct vattr *,
		       long, osi_cred_t *, pipe_t *, int *, struct volume *,
		       int *, opaque *));
extern px_write _TAKES((struct vnode *, afs_hyper_t *, long, struct vattr *,
			long, osi_cred_t *, pipe_t *, int, int *,
			struct volume *, int *, opaque *));
#endif
extern int px_FileNameOK _TAKES((char *));
extern void px_xvattr_to_afsstatus(
  struct xvfs_attr *xvattrp,
  afsFetchStatus *OutStatusp,
  struct fshs_principal *princp);
extern void px_ComputeTokenRecoveryTime _TAKES((int));
extern void px_CheckTSREndTime _TAKES((void));
extern void GetFileServerStats _TAKES((afsStatistics *));
extern void GetVolumeStats _TAKES((afsStatistics *));
extern void GetSystemStats _TAKES((afsStatistics *));

extern int px_ClearTokenStruct(afs_token_t *OutTokenp);
extern int px_SetTokenStruct(
  struct tkset_set *tsetp,
  afsFid *Fidp,
  afs_token_t *OutTokenp,
  struct fshs_principal *princp);

#ifdef SGIMIPS
/*
 * Exported by px_moveops.c
 */
extern long pxvc_Cleanups _TAKES((void));

/*
 * Exported by px_remove.c
 */
extern int px_TryRemove _TAKES((struct vnode *, afsFid *, struct volume *volp));

/*
 * Exported by px_vlutils.c
 */
extern int px_CheckFLServers _TAKES((void));
extern px_initFLServers _TAKES((struct fsop_cell *));
#endif

/*
 * Exported by px_repops.c
 */
extern void px_InitKeepAliveHost _TAKES((void));
extern u_long pxint_PeriodicKA _TAKES((void));
#ifdef SGIMIPS
extern int pxint_BulkKeepAlive _TAKES((handle_t, struct afsBulkFEX *,
				       unsigned32, unsigned32,
				       unsigned32, unsigned32,
				       unsigned32 *));
#else
extern int pxint_BulkKeepAlive _TAKES((handle_t, struct afsBulkFEX *,
				       unsigned long, unsigned long,
				       unsigned long, unsigned long,
				       unsigned long *));
#endif

#ifndef SGIMIPS
/*
 * Exported by kutils/krpc_misc.c
 */
extern char protSeq[];
extern long krpc_InvokeServer();
extern rpc_thread_pool_handle_t krpc_SetupThreadsPool();
#endif

#endif /* KERNEL */

#endif	/* _PXH_ */
