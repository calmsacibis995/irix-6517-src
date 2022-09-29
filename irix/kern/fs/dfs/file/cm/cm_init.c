/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_init.c,v 65.7 1998/07/29 19:40:27 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


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

#include <dce/ker/pthread.h>
#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/sysincludes.h>
#include <dcedfs/xvfs_export.h>
#include <dcedfs/debug.h>
#include <dcedfs/tkn4int.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/osi_dfserrors.h>
#include <dcedfs/tkc.h>
#include <dcedfs/com_locks.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_volume.h>
#include <cm_cell.h>
#include <cm_conn.h>
#include <cm_dcache.h>
#include <cm_vcache.h>
#include <cm_stats.h>
#include <cm_aclent.h>
#include <cm_dnamehash.h>
#include <cm_bkg.h>

#ifdef SGIMIPS
/* for forming cm_sgi_sysname */
#include <sys/utsname.h>
#endif /* SGIMIPS */


RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_init.c,v 65.7 1998/07/29 19:40:27 gwehrman Exp $")

/*
 * General initialization globals.
 */
char cm_zeros[CM_ZEROS];		/* convenient buffer of zeros */
#ifdef CM_DISKLESS_SUPPORT
char cm_rootVolumeName[120]="";		/* Default root vol name: root.cell */
char cm_rootCellName[120] = "";		/* Default cell name qualifying above*/
struct cm_dlinfo *cm_dlinfo=0;		/* Info for diskless root cell */
#endif /* CM_DISKLESS_SUPPORT */
char cm_localCellName[256] = "";	/* Local Cell name */
char *cm_localPrincNamep = NULL;	/* Local principal name */
char *cm_sysname = 0;			/* @sys value for the cm machine */
#ifdef SGIMIPS
char cm_sgi_sysname[CM_MAXSYSNAME]="mips_irix"; /* to store sgi specific @sys.*/
                                                /* will be set in afs_init()  */
#endif /* SGIMIPS */
int cm_initState = 0;			/* Holds initialization synch state */
static struct lock_data cm_initStateLock;
extern struct lock_data cm_termStateLock;
struct icl_set *cm_iclSetp = 0;
long cm_setTime = 0;			/* Whether to synch time with server */
static int CB_Running = 0;		/* Token server already running flag */
static int CM_Running = 0;		/* Cm daemon service already running */
static int CacheInit_Done = 0;		/* Cache initialization already done */
static int Go_Done = 0;			/* Last init  step done flag */
/*static*/ long cm_cacheCounter=0;	/* number of disk cache entries */
static long cm_cacheInfoModTime=0;	/* last modtime for cacheinfo */
struct osi_file *cm_cacheFilep = 0;	/* file from osi_Open for cache file */
#ifdef AFS_VFSCACHE
struct fid cm_cacheHandle;		/* Handle for cacheInfo file */
struct fid cm_volumeHandle;		/* Handle for volumeInfo file */
struct osi_vfs *cm_cache_mp;		/* mount point for cache info */
static struct osi_vfs *cm_volume_mp;	/* mount point for volume info */
#else
struct osi_dev cacheDev;		/* cm cache device structure */ 
long cm_cacheInode;			/* Inode number for cacheInfo file */
long cm_volumeInode;			/* Inode number for volumeInfo file */
#endif /* AFS_VFSCACHE */
long cm_cacheFiles;			/* size of cm_indexTable */
long cm_cacheBlocks;			/* 1K blocks in cache */
long cm_origCacheBlocks;		/* from boot */
long cm_cacheScaches;			/* stat entries in cache */
long cm_cacheDcaches;			/* data entries in cache */
long cm_cacheVolumes;			/* volume entries in cache */
long cm_blocksUsed;			/* number of blocks in use */
static long cm_reusedFiles = 0;		/* number of files reused */
long cm_othercsize;			/* default chunk size */
long cm_logchunk;			/* log of cm_othercsize */
long cm_firstcsize;			/* first chunk size */
long cm_trySmush = 1;			/* mainly utilized by NFS/AFS trans */
long cm_cacheUnit;			/* smallest cache reservation size */

/* Local security bounds */
struct cm_security_bounds cm_localSec = { 0, 0 };
/* Non-local security bounds */
struct cm_security_bounds cm_nonLocalSec = { 0, 0 };

#ifdef CM_ENABLE_COUNTERS
struct cm_stats cm_stats;
#endif /* CM_ENABLE_COUNTERS */

extern TKN4Int_v4_0_epv_t TKN4Int_v4_0_manager_epv;


/*
 * The following data structures are to represent some properties of TKNserver.
 * They should get re-assigned whenever the TKNserver is rebooted.
 */
struct afsNetData cm_tknServerAddr;
#ifdef SGIMIPS
unsigned32 cm_tknEpoch;			/* the TKN birth time */
#else  /* SGIMIPS */
unsigned long cm_tknEpoch;			/* the TKN birth time */
#endif /* SGIMIPS */

extern struct cm_cacheOps cm_UFSCacheOps;


static int CacheInit _TAKES((long, long, long, long, long, long, long, long));
static int InitVolumeInfo _TAKES((char *));
static int InitCacheInfo _TAKES((char *));
#ifdef AFS_VFSCACHE
static int InitCacheFile _TAKES((char *, osi_fhandle_t *));
#else
static int InitCacheFile _TAKES((char *, long));
#endif /* AFS_VFSCACHE */
#ifdef SGIMIPS
extern void krpc_UnInvokeServer _TAKES((
    char *serverName,           /* server invoked */
    char *caller,               /* who is the caller */
    rpc_if_handle_t ifSpec,     /* server interface spec */
    rpc_mgr_epv_t mgrEpv,       /* manager vector for that IF service */
    uuid_t *uuidPtr,            /* object type uuid pointer */
    long Flags));               /* when verbose is set */

extern void volreg_Init _TAKES(( void ));
#endif /* SGIMIPS */

#ifdef	AFS_HPUX_ENV
extern int cm_post_config();
#else
#define cm_post_config() 0
#endif	/* AFS_HPUX_ENV */

/*
 * Export the AFS ops to the vfs+ layer
 */
#ifdef SGIMIPS
struct xvfs_vnodeops *cm_GetAFSOps(void) 
#else  /* SGIMIPS */
struct xvfs_vnodeops *cm_GetAFSOps() 
#endif /* SGIMIPS */
{
    static setup = 0;

    if (!setup) {
	setup = 1;
	xvfs_ops = (struct xvfs_vnodeops *)osi_Alloc(sizeof(struct xvfs_vnodeops));
	xvfs_InitFromXOps(cm_ops, xvfs_ops);
    }
    return xvfs_ops;
}

/* Wait until we've passed at least the given init state value. */
#ifdef SGIMIPS
static void cm_AwaitInitState(int stateval)
#else  /* SGIMIPS */
static void cm_AwaitInitState(stateval)
int stateval;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_initStateLock);
    while (cm_initState < stateval) {
	osi_SleepW(&cm_initState, &cm_initStateLock);
	lock_ObtainWrite(&cm_initStateLock);
    }
    lock_ReleaseWrite(&cm_initStateLock);
}

/* Advance the init state to the given value. */
#ifdef SGIMIPS
static void cm_AdvanceInitState(int stateval)
#else  /* SGIMIPS */
static void cm_AdvanceInitState(stateval)
int stateval;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_initStateLock);
    if (cm_initState < stateval) cm_initState = stateval;
    osi_Wakeup(&cm_initState);
    lock_ReleaseWrite(&cm_initStateLock);
}


/* Assign to the security bounds structures. */
#ifdef SGIMIPS
int cm_SetSecBounds(
    struct cm_security_bounds *lclsecp,
    struct cm_security_bounds  *rmtsecp,
    int editing)
#else  /* SGIMIPS */
int cm_SetSecBounds(lclsecp, rmtsecp, editing)
struct cm_security_bounds *lclsecp, *rmtsecp;
int editing;
#endif /* SGIMIPS */
{/* Apply defaults and then assign. */
    struct cm_security_bounds lsecb, rsecb;

    lsecb = *lclsecp;
    if (editing) {
	if (lsecb.initialProtectLevel == rpc_c_protect_level_default)
	    lsecb.initialProtectLevel = cm_localSec.initialProtectLevel;
	if (lsecb.minProtectLevel == rpc_c_protect_level_default)
	    lsecb.minProtectLevel = cm_localSec.minProtectLevel;
    }
    if (lsecb.initialProtectLevel != rpc_c_protect_level_default
	&& lsecb.minProtectLevel != rpc_c_protect_level_default
	&& lsecb.initialProtectLevel < lsecb.minProtectLevel)
	return EINVAL;
    if (lsecb.initialProtectLevel == rpc_c_protect_level_default)
	lsecb.initialProtectLevel = CM_DEFAULT_LCLINIT_LEVEL;
    if (lsecb.minProtectLevel == rpc_c_protect_level_default)
	lsecb.minProtectLevel = CM_DEFAULT_LCLMIN_LEVEL;
    if (lsecb.initialProtectLevel < lsecb.minProtectLevel)
	lsecb.initialProtectLevel = lsecb.minProtectLevel;
    rsecb = *rmtsecp;
    if (editing) {
	if (rsecb.initialProtectLevel == rpc_c_protect_level_default)
	    rsecb.initialProtectLevel = cm_nonLocalSec.initialProtectLevel;
	if (rsecb.minProtectLevel == rpc_c_protect_level_default)
	    rsecb.minProtectLevel = cm_nonLocalSec.minProtectLevel;
    }
    if (rsecb.initialProtectLevel != rpc_c_protect_level_default
	&& rsecb.minProtectLevel != rpc_c_protect_level_default
	&& rsecb.initialProtectLevel < rsecb.minProtectLevel)
	return EINVAL;
    if (rsecb.initialProtectLevel == rpc_c_protect_level_default)
	rsecb.initialProtectLevel = CM_DEFAULT_RMTINIT_LEVEL;
    if (rsecb.minProtectLevel == rpc_c_protect_level_default)
	rsecb.minProtectLevel = CM_DEFAULT_RMTMIN_LEVEL;
    if (rsecb.initialProtectLevel < rsecb.minProtectLevel)
	rsecb.initialProtectLevel = rsecb.minProtectLevel;
    cm_localSec = lsecb;
    cm_nonLocalSec = rsecb;
    return 0;
}

/* 
 * handle basic cache initialization 
 */
#ifdef SGIMIPS
static int cm_DoInitCacheInit(
     struct cm_cacheparams *acparmsp,
     char * dirNamep)
#else  /* SGIMIPS */
static int cm_DoInitCacheInit(acparmsp, dirNamep)
     struct cm_cacheparams *acparmsp;
     char * dirNamep;
#endif /* SGIMIPS */
{
#ifndef AFS_VFSCACHE
    struct vnode *filevp;
#endif /* AFS_VFSCACHE */
    int result;

    if (CacheInit_Done)
	return 0;
    CacheInit_Done = 1;
    cm_AwaitInitState(CMOP_START_BKG);     /* wait for basic init */

#ifndef AFS_VFSCACHE
    /* 
     * we allow the called to leave the dirNamep out.  In that case, 
     * we must also be diskless.  (This is checked in afscall_cm)
     */
    if (*dirNamep) {
	if (osi_lookupname(dirNamep, OSI_UIOSYS, FOLLOW_LINK, 
			   (struct vnode *) 0, &filevp)) {
	    cm_printf("dfs: CacheBaseDir '%s' does not exist\n", dirNamep);
	    return ENOENT;
	}
	cacheDev.dev = VTOI(filevp)->i_dev;
	OSI_VN_RELE(filevp);
	/*
	 * Setup up the osi logger device
	 */
	osi_SetLogDev(&cacheDev);
    }
#endif /* AFS_VFSCACHE */

    result = cm_SetSecBounds(&acparmsp->localSec, &acparmsp->nonLocalSec, 0);
    if (result)
	return result;
    cm_setTime = acparmsp->setTimeFlag;
    return (CacheInit(acparmsp->cacheScaches, acparmsp->cacheFiles, 
	      acparmsp->cacheBlocks, acparmsp->cacheDcaches, 
	      acparmsp->cacheVolumes, acparmsp->chunkSize, 
	      acparmsp->disklessFlag, acparmsp->namecachesize));
}

/* cleanly handle waiting for root volume setting call */
#ifdef SGIMIPS
int
cm_DoInitRootVolume(char *avolNamep) 
#else  /* SGIMIPS */
cm_DoInitRootVolume(avolNamep)
     char *avolNamep; 
#endif /* SGIMIPS */
{
    cm_AwaitInitState(CMOP_START_BKG);     /* wait for basic init */
#ifdef CM_DISKLESS_SUPPORT
    if (cm_dlinfo) {
	strncpy(cm_rootVolumeName, avolNamep, sizeof(cm_rootVolumeName)-1);
	cm_rootVolumeName[sizeof(cm_rootVolumeName)-1] = 0;
    } else {
	cm_printf("ignoring root fileset spec: dfs diskless not configured\n");
    }
#else
    cm_printf("ignoring root fileset spec: dfs diskless not configured\n");
#endif /* CM_DISKLESS_SUPPORT */
    return 0;
}

/* cleanly handle waiting for root volume setting call */
#ifdef SGIMIPS
cm_DoInitRootCell(char *acellNamep) 
#else  /* SGIMIPS */
cm_DoInitRootCell(acellNamep)
     char *acellNamep; 
#endif /* SGIMIPS */
{
    cm_AwaitInitState(CMOP_START_BKG);     /* wait for basic init */
#ifdef CM_DISKLESS_SUPPORT
    if (cm_dlinfo) {
	strncpy(cm_rootCellName, acellNamep, sizeof(cm_rootCellName)-1);
	cm_rootCellName[sizeof(cm_rootCellName)-1] = 0;
    } else {
	cm_printf("ignoring root cell spec: dfs diskless not configured\n");
    }
#else
    cm_printf("ignoring root cell spec: dfs diskless not configured\n");
#endif /* CM_DISKLESS_SUPPORT */
    return 0;
}

/* At the moment, all this initialization is tightly interwoven with how
 * dfsd starts us.
 *
 * Initially, cm_initState is 0 (CMOP_START_TKN), and in that state, the only
 * allowed action is to call CMOP_START_TKN with a second parameter of 0.
 * This will initialize locks, register the TKN service with the KRPC runtime,
 * set up the thread pool, advance the cm_initState to 1 (CMOP_START_MAIN),
 * and return.
 *
 * After cm_initState has been advanced to 1 (CMOP_START_MAIN), several
 * actions may occur:
 *	- the CMOP_START_TKN call may be given a non-zero second parameter,
 *	  which will make the rpc_server_listen() call and probably never
 *	  return.
 *	- the CMOP_START_MAIN call can take action.  This thread will wait
 *	  until cm_initState has reached the value 1 (CMOP_START_MAIN), will
 *	  advance cm_initState to 2 (CMOP_START_BKG), and will then wait for
 *	  cm_initState to advance to 101 (CMOP_GO+1) and call cm_Daemon() to
 *	  do cache manager background work.
 *
 * After cm_initState is advanced to 2 (CMOP_START_BKG), several threads may
 * continue:
 *	- cm_DoInitCacheInit can consume the cache parameters and return;
 *	  this is called with the CMOP_CACHEINIT
 *	- cm_DoInitRootVolume and cm_DoInitRootCell can copy in their
 *	  parameters (crom the CMOP_ROOTVOLUME and CMOP_ROOTCELL call values)
 *	- the CMOP_CACHEINODE call may be made to tell where a cache file is
 *	- the CMOP_CACHEFILE, CMOP_CACHEINFO, and CMOP_VOLUMEINFO calls
 *	  may be made to tell where a cache file, the CacheItems file, or the
 *	  FilesetItems file is
 *	- the CMOP_START_BKG call may be made, which will advance the
 *	  cm_InitState value to 100 (CMOP_GO), will wait for cm_initState to
 *	  advance to 101 (CMOP_GO+1) and will then call cm_bkgDaemon() and
 *	  never return.
 *
 * After cm_initState is advanced to 100 (CMOP_GO), the CMOP_GO call may be
 * made, which does nothing other than tell the cache manager that dfsd is
 * finished with its sequence of CMOP_CACHEINODE, CMOP_CACHEFILE,
 * CMOP_CACHEINFO, and CMOP_VOLUMEINFO calls.  The CMOP_GO call advances
 * cm_initState to the value 101 (CMOP_GO+1), which allows all the daemons to
 * proceed (and allows cm_root to function correctly).
 */


/*
 * Main syscall kernel entry point
 */
#ifdef SGIMIPS
int afscall_cm(
    long parm, long parm2, long parm3, long parm4, long parm5,
    int * retvalP)
#else  /* SGIMIPS */
int afscall_cm(parm, parm2, parm3, parm4, parm5, retvalP)
    long parm, parm2, parm3, parm4, parm5;
    int * retvalP;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS 
    int code=0;
    size_t temp;
#else  /* SGIMIPS */
    long code=0;
    unsigned int temp;
#endif /* SGIMIPS */
    struct lclHeap {
	struct cm_cacheparams cparms;
	struct sockaddr_in ipaddrPtr;
	char tbuffer[256];
	char princBuff[sizeof(cm_tknServerAddr.principalName)];
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    long  serverInvoked;
    long  maxCalls;
    int bufSize;
    unsigned32 st;
    rpc_thread_pool_handle_t secPoolPtr;
#if defined(KERNEL) && defined(SGIMIPS)
extern rpc_thread_pool_handle_t krpc_SetupThreadsPool(char *,
                rpc_if_handle_t, char *, int, int);
#else  /* KERNEL && SGIMIPS */
    extern rpc_thread_pool_handle_t krpc_SetupThreadsPool();
#endif /* KERNEL && SGIMIPS */
#ifdef  AFS_VFSCACHE
    osi_fhandle_t       fh;
#endif

    if (!osi_suser(osi_getucred())) {	/* only root can run this code */
	return EACCES;
    }

    /* Ensure that we've set up the basic locks and things */
    if (cm_iclSetp == NULL) {
	cm_BasicInit();
    }
    *retvalP = 0;

    if (parm == CMOP_START_TKN) {	/* Start up token importer server */
	serverInvoked = parm2;
	maxCalls = parm3;
	bufSize = 0;
 
	if (CB_Running) {
	    return(0);
	}

	osi_Invisible();

	if (serverInvoked) {
	    cm_AwaitInitState(CMOP_START_MAIN);
	    uprintf("dfs: TKN server starts listening...\n");
	    /*
	     * Create multiple threads for serving incoming remote requests
	     */
	    CB_Running = 1;
	    osi_RestorePreemption(0);
#ifdef SGIMIPS
	    rpc_server_listen((unsigned32)maxCalls, &st);
#else  /* SGIMIPS */
	    rpc_server_listen(maxCalls, &st);
#endif /* SGIMIPS */
	    osi_PreemptionOff();
	    if (st != error_status_ok && st != rpc_s_already_listening) {
		cm_printf("dfs: RPC listen failed (code %d)\n", st);
		return(EINVAL);
	    }
	    code = 0;
	} else {

#ifdef SGIMIPS
	    st = cm_ConnInit((int)maxCalls);
#else  /* SGIMIPS */
	    st = cm_ConnInit(maxCalls);
#endif /* SGIMIPS */
	    if (st != error_status_ok)
		return st;
	    /* 
	     * Invoke and Register the TKN server
	     */
#ifdef AFS_SMALLSTACK_ENV
	    lhp = (struct lclHeap *)osi_AllocBufferSpace();
#endif /* AFS_SMALLSTACK_ENV */
#ifdef SGIMIPS 
	    code = (int) krpc_InvokeServer("TKN",        /* Server to invoke */
#else  /* SGIMIPS */
	    code = krpc_InvokeServer("TKN",         /* Server to invoke */
#endif /* SGIMIPS */
				     "dfs",       	/* I am the caller */
				     (rpc_if_handle_t)TKN4Int_v4_0_s_ifspec,
				     (rpc_mgr_epv_t)&TKN4Int_v4_0_manager_epv,
				     NULL,         	/* Null Object ID Ptr */
				     0,            	/* Flags not used */
#ifdef SGIMIPS
				     (int)maxCalls,   /* # of kernel threads */
#else  /* SGIMIPS */
				     maxCalls,     	/* # of kernel threads */
#endif /* SGIMIPS */
				     NULL,         	/* No need rpc binding */
				     &bufSize,      /* No need */
				     &lhp->ipaddrPtr);  /* server sock addr */
	    if (code) {
		cm_printf("dfs: can't invoke TKN server (code %d)\n", code);
		code = EINVAL;
		goto cleanup_server;
	    }

	    /* Set up cm_tknServerAddr */
	    cm_tknServerAddr.sockAddr = *((afsNetAddr *) &lhp->ipaddrPtr);
	    cm_tknServerAddr.principalName[0] = '\0';
	    lhp->princBuff[0] = '\0';
	    /*
	     * Copy in the principal name under which we can authenticate the
	     * token-revocation service (the local self entry).  We pass this
	     * principal name to the file exporter, as well, so that it can
	     * use it to authenticate its calls back to us.
	     */
	    if (parm4) {
		code = osi_copyinstr((char *) parm4, &lhp->princBuff[0],
				     sizeof(lhp->princBuff), &temp);
		if (code != 0) {
		    cm_printf("dfs: can't copy in principal name: %d\n", code);
		    goto cleanup_server;
		}
		if (temp == sizeof(lhp->princBuff)) { /* name too long */
		    code = EINVAL;
		    goto cleanup_server;
		}
		/* We now know that principal name will fit. */
	    }
	    if (lhp->princBuff[0]) {
		osi_RestorePreemption(0);
		rpc_server_register_auth_info ((u_char *)lhp->princBuff,
					       rpc_c_authn_default,
					       (rpc_auth_key_retrieval_fn_t)NULL,
					       (void *)NULL,
					       &st);
		osi_PreemptionOff();
		if (st != error_status_ok) {
		    if (st == rpc_s_helper_not_running)
			cm_printf("dfs: auth helper is not running\n");
		    else if (st == rpc_s_helper_catatonic)
			cm_printf("dfs: auth helper is catatonic\n");

		    cm_printf("dfs: can't register auth with princ name '%s' (code %x)\n",
			    lhp->princBuff, st);
		    code = EINVAL;
		    goto cleanup_server;
		}
		strcpy((char*)cm_tknServerAddr.principalName, lhp->princBuff);
	    }

	    cm_tknEpoch = osi_Time();    /* TKN server's birth time */

	    /* Setup up extra threads pool for revocation server */
	    secPoolPtr = krpc_SetupThreadsPool("dfs", TKN4Int_v4_0_s_ifspec, 
					       "TKN", 1 /* thread is enough */, 0 /* !check obj uuids */);
	    if (secPoolPtr == NULL) {
		cm_printf("dfs: revocation threads pool setup failed\n");
		code = EINVAL;
		goto cleanup_server;
	    }
#ifdef AFS_SMALLSTACK_ENV
	    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	    rpc_server_set_thread_pool_qlen(secPoolPtr, 400, &st);
	    cm_AdvanceInitState(CMOP_START_MAIN);
	    return 0;

cleanup_server:
#ifdef AFS_SMALLSTACK_ENV
	    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	    krpc_UnInvokeServer("TKN",
				"dfs",
				(rpc_if_handle_t)TKN4Int_v4_0_s_ifspec,
				(rpc_mgr_epv_t)&TKN4Int_v4_0_manager_epv,
				NULL,
				0);
	    return code;
	}
  	
    } else if (parm == CMOP_START_MAIN) {  /* startup main afs daemon */
	if (CM_Running) {
	    return 0;
	}
	CM_Running = 1;

	cm_AwaitInitState(CMOP_START_MAIN);
	cm_AdvanceInitState(CMOP_START_BKG);
	osi_Invisible();
	/* sleep until the cache is ready to go */
	cm_AwaitInitState(CMOP_GO+1);
	cm_Daemon();
	/* Never return... */

	code = 0;
    } else if (parm == CMOP_START_BKG) {	/* start up the bkg daemon */
	cm_AwaitInitState(CMOP_START_BKG);
	cm_AdvanceInitState(CMOP_GO);
	osi_Invisible();
	/* sleep until the cache is ready to go */
	cm_AwaitInitState(CMOP_GO+1);
	cm_bkgDaemon();
	/* Never return... */

	code = 0;
    } else if (parm == CMOP_CACHEINIT) {	/* Initialize the cache */
#ifdef AFS_SMALLSTACK_ENV
	lhp = (struct lclHeap *)osi_AllocBufferSpace();
#ifdef SGIMIPS
#pragma set woff 1171
#endif
	/* CONSTCOND */
	osi_assert(sizeof(struct lclHeap) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif
#endif /* AFS_SMALLSTACK_ENV */
	code = osi_copyin((char *) parm2, (caddr_t) &lhp->cparms, sizeof (lhp->cparms));
	if (code != 0) {
#ifdef AFS_SMALLSTACK_ENV
	    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	    return code;
	}

	if (parm3) { /* copy the local cell name */
	    code = osi_copyinstr((char *) parm3, cm_localCellName,
				 sizeof (cm_localCellName), &temp);
	    if (code != 0) {
#ifdef AFS_SMALLSTACK_ENV
		osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
		return code;
	    }
	} else {
	    cm_printf("dfs: missing the localCellName parameter\n");
#ifdef AFS_SMALLSTACK_ENV
	    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	    return EINVAL;
	}

	cm_localCellName[sizeof(cm_localCellName)-1] = 0;

	if (parm4) { /* copy the cache base dir */
	    code = osi_copyinstr((char *) parm4,
				 lhp->tbuffer, sizeof (lhp->tbuffer), &temp);
	    if (code != 0) {
#ifdef AFS_SMALLSTACK_ENV
		osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
		return code;
	    }
	} else {
	    /* Only ok if diskless */
	    if (lhp->cparms.disklessFlag) {
		lhp->tbuffer[0] = '\0';
	    } else {
		cm_printf("dfs: missing the cacheBaseDir parameter\n");
#ifdef AFS_SMALLSTACK_ENV
		osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
		return EINVAL;
	    }		
	}
	lhp->tbuffer[sizeof(lhp->tbuffer)-1] = 0;

	code = cm_DoInitCacheInit(&lhp->cparms, lhp->tbuffer);
#ifdef AFS_SMALLSTACK_ENV
	osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */

    } else if (parm == CMOP_CACHEINODE) {/* Initialize the 'CacheItems' file */
	cm_AwaitInitState(CMOP_START_BKG);	/* wait for basic init */
#ifdef AFS_VFSCACHE
	code = osi_copyin((char *)parm2, (caddr_t)&fh, sizeof (osi_fhandle_t));
	if (code != 0) {
	    return code;
	}
	code = InitCacheFile((char *) 0, &fh);
#else
	code = InitCacheFile((char *) 0, parm2);
#endif /* AFS_VFSCACHE */

    } else if (parm == CMOP_ROOTVOLUME) {/* Default root volume is in parm2 */
#ifdef AFS_SMALLSTACK_ENV
	lhp = (struct lclHeap *)osi_AllocBufferSpace();
#endif /* AFS_SMALLSTACK_ENV */
	if (parm2) {
	    code = osi_copyinstr((char *)parm2,
				 lhp->tbuffer, sizeof (lhp->tbuffer), &temp);
	    if (code != 0) {
#ifdef AFS_SMALLSTACK_ENV
		osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
		return code;
	    }
	    lhp->tbuffer[sizeof(lhp->tbuffer)-1] = 0;
	} else {
#ifdef AFS_SMALLSTACK_ENV
	    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	    return(EINVAL);
	}

	code = cm_DoInitRootVolume(lhp->tbuffer);
#ifdef AFS_SMALLSTACK_ENV
	osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */

    } else if (parm == CMOP_ROOTCELL) {
#ifdef AFS_SMALLSTACK_ENV
	lhp = (struct lclHeap *)osi_AllocBufferSpace();
#endif /* AFS_SMALLSTACK_ENV */
	if (parm2) {
	    code = osi_copyinstr((char *)parm2,
				 lhp->tbuffer, sizeof (lhp->tbuffer), &temp);
	    if (code != 0) {
#ifdef AFS_SMALLSTACK_ENV
		osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
		return code;
	    }
	    lhp->tbuffer[sizeof(lhp->tbuffer)-1] = 0;
	} else {
#ifdef AFS_SMALLSTACK_ENV
	    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	    return(EINVAL);
	}

	code = cm_DoInitRootCell(lhp->tbuffer);
#ifdef AFS_SMALLSTACK_ENV
	osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */

    } else if (parm == CMOP_CACHEFILE || parm == CMOP_CACHEINFO ||
	      parm == CMOP_VOLUMEINFO) {
#ifdef AFS_SMALLSTACK_ENV
	lhp = (struct lclHeap *)osi_AllocBufferSpace();
#endif /* AFS_SMALLSTACK_ENV */
	code = osi_copyinstr((char *)parm2, lhp->tbuffer, sizeof (lhp->tbuffer), &temp);
	if (code != 0) {
#ifdef AFS_SMALLSTACK_ENV
	    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
	    return code;
	}
	lhp->tbuffer[sizeof(lhp->tbuffer) - 1] = 0;	/* null-terminate the name */

	/* 
	 * We we have the cache dir copied in.  Call the cache init routines 
	 */
	cm_AwaitInitState(CMOP_START_BKG);	/* wait for basic init */

	if (parm == CMOP_CACHEFILE)	/* Initialize the 'CacheItems' file */ 
	    code = InitCacheFile(lhp->tbuffer, 0); /*similarity to CMOP_CACHEINODE*/
	else if (parm == CMOP_CACHEINFO)/* Initialize the 'CacheItems' file */
	    code = InitCacheInfo(lhp->tbuffer);
	else if (parm == CMOP_VOLUMEINFO) /* Init the 'VolumeItems' file */
	    code = InitVolumeInfo(lhp->tbuffer);

#ifdef AFS_SMALLSTACK_ENV
	osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */

    } else if (parm == CMOP_GO) {	/* All systems ready to go! */
	if (Go_Done) {
	    return 0;
	}
	Go_Done = 1;
	cm_AwaitInitState(CMOP_GO);
        /* do some final checks to prevent panics later on */
        if (cm_cacheCounter != cm_cacheFiles){
            printf("dfs: incorrect number of cache files initialized \n");
            printf("dfs: initialized = %d, cache files expected = %d\n",
                       cm_cacheCounter, cm_cacheFiles);
            Go_Done=0;
            return 1;
        }
	(void) cm_post_config();
	osi_ThreadCreate(cm_CacheTruncateDaemon, 0, 0, 0, "cmtd", code);
	if (code) panic("dfs: can't create cache truncate daemon");
	cm_AdvanceInitState(CMOP_GO+1);
#ifndef IP30
	printf("dfs: found %d cache files.\n", (long)cm_reusedFiles);
#endif /* IP30 */
	code = 0;
    } else if (parm == CMOP_SHUTDOWN) {	/* Shutdown the cache manager */
#if 0
	cold_shutdown = 0;
	if (parm == 1) 
	    cold_shutdown = 1;
#endif /* 0 */
	if (cm_globalVFS != 0) {
	    return EINVAL;
	}
	code = cm_shutdown();
    }
    return code;
}

#ifdef CM_DISKLESS_SUPPORT
/*
 * This routine should only be called by the diskless initialization code.
 *
 * It takes the number of server addresses and principals, the list of
 * encoded addresses for the servers, the address type (probably
 * AF_INET), the cell flags, a pointer to the UUID for the root cell (or is
 * that cell root?), and a list of the principals that correspond to each
 * server.
 *
 * This routine requires that the number of addresses ("addrs") and
 * principals ("principalsp") are indexed in their respective arrays of
 * pointers from [0..(naddrs-1)].
 *
 * Space is dynamically allocated for each of the "num_addrs" "serverAddr"s
 * and "principalsp"s, in addition to the space for the cm_dlinfo structure.
 */
#ifdef SGIMIPS
void
cm_DoInitDLInit(
     int num_addrs,
     u_long *addrs,
     short family,
     long flags,
     afsUUID *uuid_p,
     char **principalsp)
#else  /* SGIMIPS */
void
cm_DoInitDLInit(num_addrs, addrs, family, flags, uuid_p, principalsp)
     int num_addrs;
     u_long *addrs;
     short family;
     long flags;
     afsUUID *uuid_p;
     char **principalsp;
#endif /* SGIMIPS */
{
    int i;
    
    if (cm_dlinfo)
        panic("cm_DoInitDLInit");

    if (num_addrs > MAXVLHOSTSPERCELL)
        num_addrs = MAXVLHOSTSPERCELL;
    
    cm_dlinfo = (struct cm_dlinfo *)osi_Alloc(sizeof(struct cm_dlinfo));
    cm_dlinfo->nAddrs = num_addrs;
    cm_dlinfo->flags  = flags;
    cm_dlinfo->uuid   = *uuid_p;
    for (i=0; i<num_addrs; i++) {
        cm_dlinfo->serverAddr[i] =
	  (struct sockaddr_in *)osi_Alloc((long)(sizeof(struct sockaddr_in)));
	cm_dlinfo->serverAddr[i]->sin_family = family;
	cm_dlinfo->serverAddr[i]->sin_addr.s_addr = addrs[i];
	cm_dlinfo->principal[i] =
	  (char *)osi_Alloc((long)(strlen(principalsp[i])+1));
	strcpy(cm_dlinfo->principal[i], principalsp[i]);
    }
}

/*
 * Free up the entire cm_dlinfo structure.  This includes all of the
 * server socket-address structures and the principal strings that were
 * allocated in addition to the structure itself.
 */
#ifdef SGIMIPS
void cm_DoInitDLUnInit(void)
#else  /* SGIMIPS */
void cm_DoInitDLUnInit()
#endif /* SGIMIPS */
{
    int i;
    if (!cm_dlinfo)
        panic("cm_DoInitDLUnInit");
    
    for (i=0; i<cm_dlinfo->nAddrs; i++) {
        osi_Free((opaque)cm_dlinfo->principal[i],
		 (long)(strlen(cm_dlinfo->principal[i])+1));
	osi_Free((opaque)cm_dlinfo->serverAddr[i], 
		 (long)(sizeof(struct sockaddr_in)));
    }
    osi_Free((opaque)cm_dlinfo, (long)sizeof(struct cm_dlinfo));
}
#endif /* CM_DISKLESS_SUPPORT */

#ifdef SGIMIPS
static int cm_OnceBasicInit(void)
#else  /* SGIMIPS */
static int cm_OnceBasicInit()
#endif /* SGIMIPS */
{
    int code, i;
    struct icl_log *logp;
    struct cm_conn *connp;
    struct cm_tgt *tgtp;

    /* 
     * Initialize lock_data structures 
     */
    lock_Init(&cm_ftxtlock);
    lock_Init(&cm_scachelock);
    lock_Init(&cm_tokenlock);
    lock_Init(&cm_getdcachelock);
    lock_Init(&cm_dcachelock);
    lock_Init(&cm_qtknlock);
    lock_Init(&cm_racinglock);
    lock_Init(&cm_connlock);
    lock_Init(&cm_tgtlock);
    lock_Init(&cm_volumelock);
    lock_Init(&cm_celllock);
    lock_Init(&cm_serverlock);
    cm_VDirInit();

    bzero(cm_zeros, CM_ZEROS);
    /*
     * Initialize some external dependent packages too...
     */
#if	!defined(AFS_DYNAMIC) || defined(AFS_HPUX_ENV)
    /* Done during kernel extension load */
    volreg_Init();
    tkc_Init();
#endif
    lock_Init(&cm_initStateLock);
    lock_Init(&cm_termStateLock);
    vnl_init(100);			/* initialize the Sys V lock pkg */
    cm_InitACLCache(600);		/* 600 acl cache entries */

    /* 
     * Initialize the error mapping table used for doing decoding work.
     */
    osi_initDecodeTable();

    /*
     * Initialize the tgt linked list by allocating 16 of cm_tgts first. 
     */
    for (i = 0; i < 16; i++) {
	tgtp = (struct cm_tgt *) osi_Alloc(sizeof(struct cm_tgt));
	tgtp->next = FreeTGTList;
	FreeTGTList = tgtp;
    }
    /*
     * Initialize the connp linked list by allocating 20 of entries. 
     * These should fit in one 1-k page. 
     */
    for (i = 0; i < 20; i++) {
	connp = (struct cm_conn *) osi_Alloc(sizeof(struct cm_conn));
	connp->next = FreeConnList;
	FreeConnList = connp;
    }
    /* 
     * Initialize hash tables 
     */
    for (i = 0; i < DC_VHASHSIZE; i++) 
	cm_dvhashTable[i] = DC_NULLIDX;
    for (i = 0; i < DC_CHASHSIZE; i++) 
	cm_dchashTable[i] = DC_NULLIDX;

#ifdef AFS_OSF11_ENV
    code = icl_CreateLog("cmfx", 2*1024, &logp);
#else
    code = icl_CreateLog("cmfx", 60*1024, &logp);
#endif
    if (code == 0)
	code = icl_CreateSet("cm", logp, (struct icl_log *) 0, &cm_iclSetp);
    if (code)
	cm_printf("dfs: warning: can't init icl for cmfx, code %d\n", code);
#ifdef AFS_SUNOS5_ENV
    (void) once_krpc_setup_dfs_icl_log();
#endif /* AFS_SUNOS5_ENV */
    return code;
}

#ifdef SGIMIPS
void cm_BasicInit(void)
#else  /* SGIMIPS */
void cm_BasicInit()
#endif /* SGIMIPS */
{
#ifndef SGIMIPS
    long code;
#endif /* !SGIMIPS */
    static pthread_once_t onceBlock = pthread_once_init;

    pthread_once(&onceBlock, (pthread_initroutine_t)cm_OnceBasicInit);
}

/* 
 * Initialize the cache layer module 
 */
#ifdef SGIMIPS
static int CacheInit(
    long astatSize, long afiles, long ablocks, long adcs, long avols, 
    long achunk, long adiskless,
    long anamecachesize)
#else  /* SGIMIPS */
static int CacheInit(astatSize, afiles, ablocks, adcs, avols, achunk, adiskless,
		 anamecachesize)
    long afiles, astatSize, ablocks, adcs, avols, achunk, adiskless,
	anamecachesize;
#endif /* SGIMIPS */
{
    register struct cm_scache *scp;
    register struct cm_dcache *tdp;
#ifndef SGIMIPS
    long code;
#endif /* !SGIMIPS */
    register struct cm_volume *volp;
    register long i, avolumes = avols, adcaches = adcs;
    register long chunksize, chunk = achunk, diskless = adiskless;
    static long cacheinit_flag = 0;
#ifdef SGIMIPS
    void set_cm_sgi_sysname(void);
#endif /* SGIMIPS */

#if defined(KERNEL) && defined(SGIMIPS)
    extern void osi_initDecodeTable(void);
    extern void nh_init(int);
#endif /* KERNEL && SGIMIPS */
    

    if (cacheinit_flag) 
	return 0;
    if (afiles <= 0 || astatSize <= 0 || anamecachesize <= 0) {
        return EINVAL;
    }

    if (cm_iclSetp == NULL) {
	cm_BasicInit();
    }
    printf("dfs: starting dfs cache scan...\n");

    cacheinit_flag = 1;

    /* 
     * Allocate and thread the struct cm_scache entries 
     */
    scp = Initial_freeSCList = (struct cm_scache *) 
	osi_Alloc(astatSize * sizeof(struct cm_scache));
    bzero((char *) scp, astatSize * sizeof(struct cm_scache));
    CM_BUMP_COUNTER_BY(statusCacheEntries, astatSize);

    freeSCList = (struct cm_scache *) &(scp[0]);
    for (i = 0; i < astatSize-1; i++)
	scp[i].lruq.next = (struct squeue *) (&scp[i+1]);
    scp[astatSize-1].lruq.next = (struct squeue *) 0;
    cm_cacheScaches = astatSize;

    /* 
     * Setup diskless parameters.
     */
    if (achunk < CM_MINCHUNK || achunk > CM_MAXCHUNK)
	achunk = 0;	/* check for reasonableness of achunk parm */
    if (diskless) {
	cm_diskless = 1;
	/* 
	 * compute chunk size.
	 */
	if (achunk)
	    chunk = achunk;
	else
	    chunk = 13;		/* default diskless chunk size */

#ifdef SGIMIPS
	while (DFS_BLKSIZE > (1<<chunk)) {
		chunk++;
	}
#endif /* SGIMIPS */

	/* compute dcache entries to be big enough not to require swapping */
	if (adcaches < afiles) adcaches = afiles;

	/* compute 1024-byte blocks in terms of chunk size */
	ablocks = ((afiles * (1<<chunk)) >> 10);

	/* Setup cm_cacheUnit. For Memcache this is the chunk size. */
	cm_cacheUnit = 1<<chunk;
    }
    else {
	/* defaults for operation with disks */
	cm_diskless = 0;
	if (!achunk)
	    chunk = CM_DEFCHUNK;

	/* Setup up cm_cacheUnit for disk cache */
	cm_cacheUnit = CM_DISK_CACHE_UNIT;
    }
    cm_logchunk = chunk;
    chunksize = (1<<chunk);
    /* XXX
     * This should be variable, as implemented in cm_chunk.h
     * Change dfsd to accept a first c size argument then apply it here
     * and record it in the fcache file header.
     */
    cm_firstcsize = cm_othercsize = chunksize;

    /* 
     * Allocate and zero the pointer array to the dcache entries 
     */
    cm_indexTable = (struct cm_dcache **)osi_Alloc(sizeof(struct cm_dcache *) * afiles);
    bzero((char *) cm_indexTable, sizeof(struct cm_dcache *) * afiles);
    cm_indexTimes = (long *) osi_Alloc(afiles * sizeof(long));
    bzero((char *) cm_indexTimes, afiles * sizeof(long));
    cm_indexFlags = (short *) osi_Alloc(afiles * sizeof(short));
    bzero((char *)cm_indexFlags, afiles * sizeof(short));
    cm_cacheFiles = afiles;

    /*
     * Initialize queues.
     */
    QInit(&SLRU);
    QInit(&DLRU);

    /* 
     * Allocate and thread the struct cm_dcache entries themselves 
     */
    if (adcaches > afiles || adcaches < DC_MINCACHEDCACHES)
	adcaches = DC_DEFCACHEDCACHES;
    tdp = Initial_freeDSList = (struct cm_dcache *) 
	osi_Alloc(adcaches * sizeof(struct cm_dcache));

    CM_BUMP_COUNTER_BY(dataCacheEntries, adcaches);
    bzero((char *) tdp, adcaches * sizeof(struct cm_dcache));
    if (diskless) {
	/* put them directly into the cm_indexTable, since we have an
	 * exact match in number, and we don't want cm_GetDSlot to try to
	 * go to the (non-existent) disk to get a slot.
	 */
	for(i=0; i<afiles; i++) {
	    cm_indexTable[i] = &tdp[i];
	    tdp[i].f.states = 0;
#ifdef SGIMIPS
	    tdp[i].index = (short)i;	/* normally done when reading dcache entry from disk */
#else  /* SGIMIPS */
	    tdp[i].index = i;	/* normally done when reading dcache entry from disk */
#endif /* SGIMIPS */
	    QAdd(&DLRU, &tdp[i].lruq);
	}
    }
    else {
	/* put these things on the freelist */
	freeDSList = &tdp[0];
	for (i = 0; i < adcaches-1; i++)
	    tdp[i].lruq.next = (struct squeue *) (&tdp[i+1]);
	tdp[adcaches-1].lruq.next = (struct squeue *) 0;
    }
    cm_cacheDcaches = adcaches;
    freeDSCount = adcaches;

    /* finally, after computing parameters and allocating appropriate
     * space for structures, initialize the cache itself.
     */
    if (diskless) {
	cm_InitMemCache(afiles, (1<<chunk));
    } else {
	/* initialize the UFS cache type */
	i = cm_RegisterCacheType(&cm_UFSCacheOps);
	if (i<0) panic("register cacheops");
#ifdef SGIMIPS
	cm_SetCacheType((int)i);
#else  /* SGIMIPS */
	cm_SetCacheType(i);
#endif /* SGIMIPS */
    }

    /* 
     * Create volume list structure 
     */
    if (avolumes > VL_MAXCACHEVOLUMES || avolumes < VL_MINCACHEVOLUMES)
	avolumes = VL_DEFCACHEVOLUMES;
    volp = (struct cm_volume *) osi_Alloc(avolumes * sizeof(struct cm_volume));
    for (i = 0; i < avolumes - 1; i++) 
	volp[i].next = &volp[i+1];
    volp[avolumes-1].next = (struct cm_volume *) 0;
    cm_freeVolList = Initialcm_freeVolList = volp;
    for (i = 0; i < VL_NFENTRIES; i++)
	cm_fvTable[i] = 0;
    cm_cacheVolumes = avolumes;

    /* 
     * Initialize some related globals 
     */
    ablocks = cm_GetSafeCacheSize(ablocks);	/* ensure we have 2 chunks */
    cm_origCacheBlocks = cm_cacheBlocks = ablocks;
    cm_blocksUsed = 0;
    cm_sysname = osi_Alloc(CM_MAXSYSNAME);
#ifdef SGIMIPS
    set_cm_sgi_sysname();
    strcpy(cm_sysname, cm_sgi_sysname);
#else  /* SGIMIPS */
    strcpy(cm_sysname, SYS_NAME);
#endif /* SGIMIPS */

    /*
     * Don't allow more than 2/3 of the files in the cache to be dirty.
     */
    cm_maxCacheDirty = (2*afiles) / 3;
    /*
     * Also, don't allow more than 2/3 of the total space get filled
     * with dirty chunks.  Compute the total number of chunks required
     * to fill the cache, make sure we don't set out limit above 2/3 of
     * that.
     */
    i = (cm_cacheBlocks << 10) / chunksize;
    i = (2*i) / 3;
    if (cm_maxCacheDirty > i)
	cm_maxCacheDirty = i;
    if (cm_maxCacheDirty < 1)
	cm_maxCacheDirty = 1;

    /*
     * Start-up directory name hash module
     */
#ifdef SGIMIPS
    nh_init((int) anamecachesize);	/* Make it variable (hashsize) */
#else  /* SGIMIPS */
    nh_init(anamecachesize);	/* Make it variable (hashsize) */
#endif /* SGIMIPS */

    return 0;
}


/* 
 * This function is called only during initialization, and is passed one 
 * parameter, a file name.  This file is declared to be the volume info 
 * storage file for the AFS.  It must be already truncated to 0 length.
 * Warning: data will be written to this file over time by AFS.
 */
#ifdef SGIMIPS
static int InitVolumeInfo (register char *fileNamep)
#else  /* SGIMIPS */
static InitVolumeInfo (fileNamep)
    register char *fileNamep; 
#endif /* SGIMIPS */
{
    register struct osi_file *osiFilep;
    struct vnode *filevp;
#ifdef AFS_VFSCACHE
    int code;
#endif


    if (cm_diskless) panic("memcache initvolumeinfo");
    if (osi_lookupname(fileNamep, OSI_UIOSYS, NO_FOLLOW, 
		       (struct vnode **) 0, &filevp))
	return ENOENT;
#ifdef AFS_VFSCACHE
    code = osi_vptofid(filevp, &cm_volumeHandle);
    if (code != 0)
       panic("InitVolumeInfo: unable to get handle\n");
    cm_volume_mp = OSI_VP_TO_VFSP(filevp);
#else
    cm_volumeInode = VTOI(filevp)->i_number;
#endif /* AFS_VFSCACHE */
    OSI_VN_RELE(filevp);
#ifdef AFS_VFSCACHE
    osiFilep = osi_Open(cm_volume_mp, &cm_volumeHandle);
#else
    osiFilep = osi_Open(&cacheDev, cm_volumeInode);
#endif /* AFS_VFSCACHE */
    osi_Truncate(osiFilep, 0);
    osi_Close(osiFilep);
    return 0;
}

/* 
 * This function is called only during initialization, and is passed one 
 * parameter, a file name.  This file is assumed to be the cache info file for 
 * the cache manager, and will be used as such.  This file should *not* be 
 * truncated to 0 length; its contents describe what data is really in the 
 * cache.   
 * Warning: data will be written to this file over time by the AFS.
 */
#ifdef SGIMIPS
static int InitCacheInfo(register char *fileNamep) 
#else  /* SGIMIPS */
static InitCacheInfo(fileNamep)
    register char *fileNamep; 
#endif /* SGIMIPS */
{
    register long goodFile;
    struct osi_stat osiStat;
    register struct osi_file *osiFilep;
    struct cm_fheader theader;
#ifdef AFS_VFSCACHE
    int code;
#endif
   struct vnode *filevp;

    if (cm_diskless) panic("memcache initcacheinfo");
    if (osi_lookupname(fileNamep, OSI_UIOSYS, NO_FOLLOW, 
		       (struct vnode **) 0, &filevp)){
	return ENOENT;
    }
#ifdef AFS_VFSCACHE
    code = osi_vptofid(filevp, &cm_cacheHandle);
    if (code != 0)
       panic("InitCacheInfo: unable to get handle\n");
    cm_cache_mp = OSI_VP_TO_VFSP(filevp);
#else
    cm_cacheInode = VTOI(filevp)->i_number;
#endif /* AFS_VFSCACHE */
    OSI_VN_RELE(filevp);

#ifdef AFS_VFSCACHE
    if (!(osiFilep = osi_Open(cm_cache_mp, &cm_cacheHandle)))
#else
    if (!(osiFilep = osi_Open(&cacheDev, cm_cacheInode)))
#endif
	panic("initcacheinfo");
    osi_Stat(osiFilep, &osiStat);
    cm_cacheInfoModTime = osiStat.mtime;
    goodFile = 0;
    if (osi_Read(osiFilep, (char *)(&theader), sizeof(theader)) == sizeof(theader)) {
	/* 
	 * read the header correctly 
	 */
	if (theader.magic == DC_FHMAGIC && 
	    theader.firstCSize == cm_firstcsize && 
	    theader.otherCSize == cm_othercsize)
	    goodFile = 1;
    }
    if (!goodFile) {
	/* 
	 * Write out a good file label 
	 */
	theader.magic = DC_FHMAGIC;
	theader.firstCSize = cm_firstcsize;
	theader.otherCSize = cm_othercsize;
	osi_Seek(osiFilep, 0);
	osi_Write(osiFilep, (char *)(&theader), sizeof(theader));
	/* 
	 * Now truncate the rest of the file,  it may be arbitrarily wrong 
	 */
	osi_Truncate(osiFilep, sizeof(struct cm_fheader));
    }
    /* leave the file open now, since reopening the file makes public
     * pool vnode systems much harder to handle.  That's cause they
     * can do a cache manager vnode recycle operation any time we
     * open a file, which we'd do on any cm_GetDSlot.
     */
    cm_cacheFilep = osiFilep;
    return 0;
}

/* 
 * This function is called only during initialization.  It is passed one 
 * parameter:  a file name of a file in the cache.
 * 
 * The file specified will be written to be the Andrew file system.
 */
#ifdef AFS_VFSCACHE
#ifdef SGIMIPS
static int InitCacheFile(
    char *fileNamep,
    osi_fhandle_t * fhp)
#else  /* SGIMIPS */
static InitCacheFile(fileNamep, fhp)
    char *fileNamep;
    osi_fhandle_t * fhp;
#endif /* SGIMIPS */
#else

static InitCacheFile(fileNamep, inodeNumber)
    char *fileNamep; 
    long inodeNumber;
#endif /* AFS_VFSCACHE */
{
#ifdef SGIMIPS 
    register long index, fileIsBad;
    register int code;
#else  /* SGIMIPS */
    register long code, index, fileIsBad;
#endif /* SGIMIPS */
    struct osi_file *osiFilep;
    struct osi_stat osiStat;
    struct vnode *filevp;
    register struct cm_dcache *tdc;

    if (cm_diskless) panic("memcache initcachefile");
    index = cm_cacheCounter;
    if (index >= cm_cacheFiles)
	return EINVAL;
    lock_ObtainWrite(&cm_dcachelock);
    tdc = cm_GetDSlot(index, (struct cm_dcache *)0);
    lock_ReleaseWrite(&cm_dcachelock);
    if (fileNamep) {
	if (code = osi_lookupname(fileNamep, OSI_UIOSYS, NO_FOLLOW,
				  (struct vnode **)0,  &filevp)) {
	    cm_PutDCache(tdc);
	    return code;
	}
	/* 
	 * Otherwise we have a VN_HOLD on filevp.  Get the useful info out and 
	 * return.  we make use here of the fact that the cache is in the UFS 
	 * file system, and just record the inode number. 
	 */
#ifdef AFS_VFSCACHE
	code = osi_vptofid(filevp, &tdc->f.handle);
	if (code != 0)
          panic("InitCacheFile: unable to get handle\n");
#else
	tdc->f.inode = VTOI(filevp)->i_number;
#endif /* AFS_VFSCACHE */
	OSI_VN_RELE(filevp);
    } else {
#ifdef AFS_VFSCACHE
	tdc->f.handle = *((osi_fid_t *) &fhp->fh_fid);
#else
	tdc->f.inode = inodeNumber;
#endif /* AFS_VFSCACHE */
    }
    tdc->f.chunkBytes = 0;
    tdc->f.cacheBlocks = 0;
    fileIsBad = 0;
    if ((tdc->f.states & DC_DWRITING) || tdc->f.fid.Vnode == 0)
	fileIsBad = 1;
#ifdef AFS_VFSCACHE
    if (!(osiFilep = osi_Open(cm_cache_mp, &tdc->f.handle)))
#else
    if (!(osiFilep = osi_Open(&cacheDev, tdc->f.inode)))
#endif /* AFS_VFSCACHE */
	panic("initcachefile open");
    if (osi_Stat(osiFilep, &osiStat))
	panic("initcachefile stat");
    /*
     * If file changed within T (120?) seconds of cache info file, it's
     * probably bad. In addition, if slot changed within last T seconds, the
     * cache info file may  incorrectly identified, and so slot may be bad.
     */
    if (cm_cacheInfoModTime < osiStat.mtime + 120)
	fileIsBad = 1;
    if (cm_cacheInfoModTime < tdc->f.modTime + 120)
	fileIsBad = 1;
    if (fileIsBad) {
	tdc->f.fid.Vnode = 0;		/* not in the hash table */
	if (osiStat.size != 0)
	    osi_Truncate(osiFilep, 0);
	tdc->f.hvNextp = freeDCList;	/* put entry in free cache slot list */
#ifdef SGIMIPS
	freeDCList = (short) index;
#else  /* SGIMIPS */
	freeDCList = index;
#endif /* SGIMIPS */
	freeDCCount++;
	cm_indexFlags[index] |= DC_IFFREE;
    } else {
	/*
	 * We must put this entry in the appropriate hash tables
	 */
#ifdef SGIMIPS
	code = (int) DC_CHASH(&tdc->f.fid, tdc->f.chunk);
#else  /* SGIMIPS */
	code = DC_CHASH(&tdc->f.fid, tdc->f.chunk);
#endif /* SGIMIPS */
	tdc->f.hcNextp = cm_dchashTable[code];
	cm_dchashTable[code] = tdc->index;
	code = DC_VHASH(&tdc->f.fid);
	tdc->f.hvNextp = cm_dvhashTable[code];
	cm_dvhashTable[code] = tdc->index;
	/* adjust to new size */
	cm_AdjustSize(tdc, osiStat.size);
	AFS_hzero(tdc->f.tokenID);                  /* Invalid value */
	if (osiStat.size > 0)
	    cm_indexFlags[index] |= DC_IFEVERUSED;
	cm_reusedFiles++;
	/*
	 * Initialize index times to file's mod times;
	 * init indexCounter to max thereof
	 */
	cm_indexTimes[index] = osiStat.atime;
	if (cm_indexCounter < osiStat.atime)
	    cm_indexCounter = osiStat.atime;
    }
    osi_Close(osiFilep);
    tdc->f.states &= ~(DC_DWRITING | DC_DONLINE);
    cm_indexFlags[index] &= ~DC_IFENTRYMOD;
    cm_WriteDCache(tdc, 0);
    cm_PutDCache(tdc);
    cm_cacheCounter++;
    return 0;
}

#ifdef SGIMIPS
/*
 * the sysname formed will be mips_irix?? - where ?? will be release
 * number (without dot).
 *
 * Eg., mips_irix53, mips_irix62, mips_irix63, mips_irix64, ....
 *
 */
void
set_cm_sgi_sysname(void)
{
        extern struct utsname utsname;
        extern char cm_sgi_sysname[];
        char tmp_buf[CM_MAXSYSNAME];



        /* Capture first 3 chars for utsname.release. Can't do strcpy
         * directly because release string might contain other infos in
         * case of alpha/beta releases
         *
         * Not a very clean implementation... But couldn't think of any thing
         * better....
         */
        tmp_buf[0] = utsname.release[0];
        tmp_buf[1] = utsname.release[2]; /* skip the dot */
        tmp_buf[2] = '\0';

        strcpy(cm_sgi_sysname, "mips_irix");
        strcat(cm_sgi_sysname, tmp_buf);

}
#endif /* SGIMIPS */
