/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: px_init.c,v 65.5 1998/04/01 14:16:07 gwehrman Exp $";
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
 * HISTORY
 * $Log: px_init.c,v $
 * Revision 65.5  1998/04/01 14:16:07  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for fshs.h.  Changed functions definitions to ansi
 * 	style.
 *
 * Revision 65.4  1998/03/23  16:24:40  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/12/16 17:05:39  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.2  1997/11/06  19:57:55  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:17:29  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.150.1  1996/10/02  18:12:52  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:35  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/fshs.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/osi_dfserrors.h>
#include <dcedfs/xvfs_export.h>
#include <dcedfs/tkc.h>
#include <dcedfs/zlc.h>
#include <dcedfs/xcred.h>
#include <dcedfs/volreg.h>
#include <dcedfs/vol_init.h>
#include <dcedfs/krpc_pool.h>
#include <dcedfs/dacl.h>
#include <px.h>
#ifdef SGIMIPS
#include <dcedfs/fshs.h>
#endif
#if defined(KERNEL) && defined(SGIMIPS64)
#include <sys/kabi.h>
#endif /* KERNEL & SGIMIPS64 */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/px/RCS/px_init.c,v 65.5 1998/04/01 14:16:07 gwehrman Exp $")

extern AFS4Int_v4_0_epv_t AFS4Int_v4_0_manager_epv;

long HD_Running = 0;
long px_initState = 0;			/* Holds initialization synch state */
static struct lock_data px_initStateLock;
struct lock_data px_tsrlock;
afsUUID sysAdminGroupID;

/* See also tkm_localCellID.  It is the fid representation of the local cell:
 * 0,,1 in tkm_tokens.[ch]. */
afsUUID localCellID;

struct icl_set *px_iclSetp = 0;
long px_StartTime;

#ifdef SGIMIPS
px_RunCheckingDaemons(void);
static int shutdownFS(void);
#else
static int shutdownFS();
#endif
static int ValidateSecAdvice(struct fshs_security_advice *secAdvice);

#ifdef	AFS_HPUX_ENV
extern int px_post_config();
#else
#define px_post_config() 0
#endif	/* AFS_HPUX_ENV */

/* Wait until we've passed at least the given init state value. */
#ifdef SGIMIPS
static void px_AwaitInitState(
int stateval)
#else
static void px_AwaitInitState(stateval)
int stateval;
#endif
{
    lock_ObtainWrite(&px_initStateLock);
    while (px_initState < stateval) {
	osi_SleepW(&px_initState, &px_initStateLock);
	lock_ObtainWrite(&px_initStateLock);
    }
    lock_ReleaseWrite(&px_initStateLock);
}

/* Advance the init state to the given value. */
#ifdef SGIMIPS
static void px_AdvanceInitState(
int stateval)
#else
static void px_AdvanceInitState(stateval)
int stateval;
#endif
{
    lock_ObtainWrite(&px_initStateLock);
    if (px_initState < stateval) px_initState = stateval;
    osi_Wakeup(&px_initState);
    lock_ReleaseWrite(&px_initStateLock);
}


#ifdef SGIMIPS
static int px_invokeserver(
    long maxCalls,
    long tknCalls,
    long parm5)
#else
static int px_invokeserver(maxCalls, tknCalls, parm5)
    long maxCalls, tknCalls, parm5;
#endif
{
	struct afs_ioctl ioctlBuf;
	rpc_thread_pool_handle_t secPoolPtr;
/* This is the maximum size of an IN parameter */
#define	PRINCIPAL_NAME_P_SIZE 1024
/* This is the maximum size of a string binding */
#define	RPC_STRING_BUF_P_SIZE 1024
	struct lpargs {
	    char rpcStringBuf[RPC_STRING_BUF_P_SIZE];
	    unsigned char principalName[PRINCIPAL_NAME_P_SIZE+1];
	} *lp = NULL;
	int auth_proto;
	int bufSize = 0;
	struct icl_log *logp;
	unsigned32 st;
	long code;
#if defined(AFS_SUNOS5_ENV) && !defined(AFS_SUNOS53_ENV)
	int *vmfp;		/* ptr for looking up viking_mfar_bug */
#endif

	/*
	 * Start the DFS Protocol Exporter, FXD, based on NCS RPC
	 */
	osi_Invisible();

	/* now initialize the ICL system */
#ifdef	AFS_OSF11_ENV
	code = icl_CreateLog("cmfx", 2*1024, &logp);
#else
	code = icl_CreateLog("cmfx", 60*1024, &logp);
#endif
	if (code == 0)
	    code = icl_CreateSet("fx", logp, (struct icl_log *) 0, &px_iclSetp);
	if (code)
	    printf("fx: warning: can't init icl for cmfx: code %d\n", code);
#ifdef AFS_SUNOS5_ENV
	(void) once_krpc_setup_dfs_icl_log();
#endif /* AFS_SUNOS5_ENV */

#if defined(AFS_SUNOS5_ENV) && !defined(AFS_SUNOS53_ENV)
	/* The cure is worse than the disease for this bug; disable
	 * the code that fixes up addresses on text faults, since it
	 * causes kernel panics when truncating files.
	 *
	 * The bug in the fixup code is fixed in Solaris 2.3, but present
	 * in 2.2, so when we do the port to Solaris 2.3, we should
	 * remove this code.
	 *
	 * Use kernel dynamic loader to find the variable in case
	 * it isn't present in this type of kernel.
	 */
	vmfp = (int *) modgetsymvalue("viking_mfar_bug", 1);
	if (vmfp) *vmfp = 0;
#endif /* defined(AFS_SUNOS5_ENV) && !defined(AFS_SUNOS53_ENV) */

	/* declare how many client and server calls we'll be making.
	 * Note that the client calls include the checking daemon (one
	 * thread).
	 * The other source of client calls made by threads here are
	 * token revocations that happen to call through the fshost
	 * package.  The accounting for token revocation RPCs is
	 * tricky.  We don't wait for a call pool slot whild holding
	 * one, so deadlock isn't a problem.  Now, we could either
	 * preallocate all the calls that the server could initiate
	 * or use a krpc_pool to manage a bunch.  Preallocation is easy
	 * and there aren't a lof of threads, so we'll do that.
	 *
	 * Now, the TKM can add some concurrency here, and TKM will
	 * add that in, on the assumption that it could be adding
	 * concurrent calls to a call already holding a server call.
	 */
	krpc_AddConcurrentCalls(1+maxCalls, maxCalls+tknCalls);
	krpc_SetConcurrentProc((int (*)())rpc_mgmt_set_max_concurrency);

	/* Allocate big objects in heap space, not on stack */
	lp = (struct lpargs *) osi_AllocBufferSpace();

	/*
	 * Invoke and Register the FX server
	 */
	code = krpc_InvokeServer("FX",		/* Server to invoke */
				 "fx",		/* I am the caller */
				 (rpc_if_handle_t)AFS4Int_v4_0_s_ifspec,
				 (rpc_mgr_epv_t)&AFS4Int_v4_0_manager_epv,
				 NULL,		/* Null Object ID Ptr */
				 0,		/* Flags not used */
				 maxCalls,	/* # of parallel procs */
				 lp->rpcStringBuf,	/* Returned rpc string */
				 &bufSize,
				 NULL);		/* ignore sockaddr */
	if (code || (bufSize == -1)) {
	    uprintf("fx: can't invoke and register DFS Interface (code %d)\n",
		    code);
	    osi_FreeBufferSpace((struct osi_buffer *)lp);
	    return(EINVAL);
	}
	if (parm5)  { 
	    /* Copyin parm5 */
	    if (code = osi_copyin((char *) parm5, (char *) &ioctlBuf,
				  sizeof(struct afs_ioctl))) {
		uprintf("fx: can't copyin ioctlBuf (code %d)\n", code);
		goto cleanup_server;
	    }
	    /*
	    * The RPC string, rpcStringBuf, contains the ipaddr and "listening"
	    * endpoint. Pass this info to the user helper who will register the
	    * binding with rpcd.
	    */
	    if ( bufSize > ioctlBuf.out_size )  {
		code = E2BIG;
		uprintf("fx: rpc string is too big (code %d)\n", code);
		goto cleanup_server;
	    }
	    ioctlBuf.out_size = bufSize;
	    if (code = osi_copyout(lp->rpcStringBuf, ioctlBuf.out, bufSize)) {
		uprintf("fx: can't copy out rpc info (code %d)\n", code);
		goto cleanup_server;
	    }
	    if (code = osi_copyin(ioctlBuf.in, (char *) lp->principalName,
			     PRINCIPAL_NAME_P_SIZE)) {
		uprintf("fx: can't copyin principal name (code %d)\n", code);
		goto cleanup_server;
	    }
	    lp->principalName[PRINCIPAL_NAME_P_SIZE] = '\0';
	}

	osi_RestorePreemption(0);
	rpc_server_register_auth_info (lp->principalName,
				       rpc_c_authn_default,
				       (rpc_auth_key_retrieval_fn_t)NULL,
				       (void *)NULL,
				       &st);
	osi_PreemptionOff();
	if (st != error_status_ok) {
	    if (st == rpc_s_helper_not_running)
	        uprintf("fx: auth helper is not running\n");
	    else if (st == rpc_s_helper_catatonic)
		uprintf("fx: auth helper is catatonic\n");

	    uprintf("fx: can't register auth with princ name '%s' (code %x)\n",
		    lp->principalName, st);
	    code = EINVAL;
	    goto cleanup_server;
	}

	/* 
	 * Setup an extra threads pool for handling FX's secondary service (ie.
	 * AFS_FSSERVICEIDX).
	 */
	secPoolPtr = krpc_SetupThreadsPool("fx", AFS4Int_v4_0_s_ifspec,
					   "FX", tknCalls, 1 /* check objs */);
 	if (secPoolPtr == NULL) {
	    osi_FreeBufferSpace((struct osi_buffer *)lp);
	    /* Should free the rpc binding ??? */
	    return(EINVAL);
	}

	/* open the queues wide, so we don't drop calls under high load */
	rpc_server_set_thread_pool_qlen(secPoolPtr, 400, &st);
	/* and set the field for the default pool, too */
	rpc_server_set_thread_pool_qlen((rpc_thread_pool_handle_t) 0, 400, &st);
	osi_FreeBufferSpace((struct osi_buffer *)lp);
	return 0;

cleanup_server:
	osi_FreeBufferSpace((struct osi_buffer *)lp);
	krpc_UnInvokeServer("FX",
			    "fx",
			    (rpc_if_handle_t)AFS4Int_v4_0_s_ifspec,
			    (rpc_mgr_epv_t)&AFS4Int_v4_0_manager_epv,
			    NULL,
			    0);
	return code;
}



/*
 * Main fs_syscall kernel entry point
 */
#ifdef SGIMIPS
int afscall_exporter(
    long parm,
    long parm2,
    long parm3,
    long parm4,
    long parm5,
    int * retvalP)
#else
int afscall_exporter(parm, parm2, parm3, parm4, parm5, retvalP)
    long parm, parm2, parm3, parm4, parm5;
    int * retvalP;
#endif
{
    long code=0, temp;
    long long_admin_id;
    struct recovery_params recoveryParams;
    unsigned32 st;

    struct fshs_security_advice secAdvice = {
        FSHS_SEC_ADVICE_FORMAT_1,
    {   rpc_c_protect_level_default,    /* local.minProtectLevel */
        rpc_c_protect_level_default     /* local.maxProtectLevel */
    },
    {   rpc_c_protect_level_default,    /* nonLocal.minProtectLevel */
	rpc_c_protect_level_default     /* nonLocal.maxProtectLevel */
    }};

    if (!osi_suser(osi_getucred())) {	/* only root can run this code */
	return EACCES;
    }

    *retvalP = 0;	/* Start afresh */
    if (parm == PXOP_PUTKEYS) {
	if (code = osi_copyin((char *)parm2, (char *) &sysAdminGroupID,
			  sizeof(afsUUID)) )
	    return code;

	/* convert the uuid form admin id to a long admin id for xvnode */
	bcopy ((char *)&sysAdminGroupID, (char *)&long_admin_id,
	       sizeof (unsigned32));
	xvfs_SetAdminGroupID(long_admin_id);	

	if (code = osi_copyin((char *)parm3, (char *) &localCellID,
			  sizeof(afsUUID)) )
	    return code;

	dacl_SetSysAdminGroupID(&sysAdminGroupID);
	dacl_SetLocalCellID(&localCellID);

	/* This call should be the first one from any fxd */
	/* It's conditional only in case of fxd restarting. */
	if (px_initState < PXOP_PUTKEYS) {
	    lock_Init(&px_initStateLock);	/* lock only for writing it */
	    lock_Init(&px_tsrlock);
	    px_initState = PXOP_PUTKEYS;
	} else {
	    /* else ....some code that addresses the restarting-fxd case,
	     *   presumably addressing the resetting of px_initState.
	     * At the moment, if we're past RPCDAEMON, we'll screw up.
	     */
	    if (px_initState >= PXOP_RPCDAEMON)
		return(EEXIST);
	}
	return 0;

    } else if (parm == PXOP_RPCDAEMON) {

        /*
         * fxd makes the RPCDAEMON syscall twice.
         * First with parm2==0, to invoke and register the server with rpc,
         * then a second time with parm2==1, to do the rpc listen.
         */
	if (parm2==0) {
	    px_AwaitInitState(PXOP_INITHOST);
	    code = px_invokeserver(parm3, parm4, parm5);
	    if (code == 0) {
		/* We're committed to going ahead now. */
		px_AdvanceInitState(PXOP_RPCDAEMON);
	    }
	    return code;
	}

	/* The state should already be up to here */
	if (px_initState < PXOP_RPCDAEMON)
	    return(ENOENT);
	uprintf("fx: FX server starts listening...\n");
	px_StartTime = osi_Time();
	osi_RestorePreemption(0);
	rpc_mgmt_set_server_com_timeout(rpc_c_binding_default_timeout+2, &st);
	osi_PreemptionOff();
	/*
	 * The first call to PXOP_RPCDAEMON, with parm2==0, has already
	 * advanced the state to PXOP_RPCDAEMON, so we borrow the next
	 * state code.
	 */
	px_AdvanceInitState(PXOP_CHECKHOST);
	osi_RestorePreemption(0);
	rpc_server_listen(parm3, &st);
	osi_PreemptionOff();
	if (st != error_status_ok) {
	    if (st == rpc_s_already_listening) {
		uprintf("fx: Warning: rpc_s_already_listening!\n");
		return 0;
	    }
	   uprintf("fx: RPC listen fails (code %d)\n", st);
	   return(EINVAL);
	}
	return 0;

     } else if (parm == PXOP_INITHOST) {
	struct fsop_cell *tcellp;

#ifdef notdef
	/* no need in new position, since prev guy has already
	 * returned.
	 */
	px_AwaitInitState(PXOP_PUTKEYS);
#endif /* notdef */
	tcellp = (struct fsop_cell *) osi_AllocBufferSpace();
	if (code = osi_copyin((char *)parm2, (char *) tcellp, sizeof(*tcellp))) {
	    osi_FreeBufferSpace((struct osi_buffer *)tcellp);
	    return code;
	}
	if (code = osi_copyin((char *)parm3, (char *) &recoveryParams,
			      sizeof(recoveryParams))) {
	    osi_FreeBufferSpace((struct osi_buffer *)tcellp);
	    return code;
	}
	/* get security advice, if specified; otherwise, use default values */
	if (parm4 != 0) {
	    if (code = osi_copyin((char *)parm4, (char *) &secAdvice,
				  sizeof(secAdvice))) {
		osi_FreeBufferSpace((struct osi_buffer *)tcellp);
		return code;
	    }
	}
	/* fill in security advice defaults and validate values */
	if (code = ValidateSecAdvice(&secAdvice)) {
	    return code;
	}

	code = fshs_InitHostModule(tcellp->cellName, &secAdvice);
	if (code) {
	    osi_FreeBufferSpace((struct osi_buffer *)tcellp);
	    return code;
	}
	/* 
	 * Initialize TSR recovery paramters 
	 */
	fshs_SetRecoveryParameters(recoveryParams.lifeGuarantee,
				   recoveryParams.RPCGuarantee,
				   recoveryParams.deadServer,
				   recoveryParams.postRecovery,
				   recoveryParams.maxLife,
				   recoveryParams.maxRPC);

	px_initFLServers(tcellp);
	osi_FreeBufferSpace((struct osi_buffer *)tcellp);

	/*
	 * Create a special host: px_keepAliveHost
	 */
	px_InitKeepAliveHost();

	/*
	 * XXX
	 * Miscellaneous file server related initializations; some of the below
	 * should be called from other places (i.e. implicitly rather than 
	 * explicitly!) XXX
	 */
	tkset_Init();
	vol_Init();			/* Init volume module */
	volreg_Init();			/* Init volume registry module */
	xcred_Init();			/* Init the xcred module */
	tkc_Init();
	zlc_Init();
	osi_initEncodeTable();		/* Init the error mapping table */
	(void) px_post_config();
	px_AdvanceInitState(PXOP_INITHOST);

	return 0;
    } else if (parm == PXOP_CHECKHOST) {
	if (HD_Running)
	    return 0;
	HD_Running = 1;
	/*
	 * The PXOP_RPCDAEMON code consumed two counter values for the
	 * px_initState counting since it's invoked twice.  The second
	 * PXOP_RPCDAEMON should have advanced the px_initState to
	 * PXOP_CHECKHOST, so that's what we wait for here.
	 */
	px_AwaitInitState(PXOP_CHECKHOST);
	osi_Invisible();
	/*
	 * Having simply finished the synchronization, we announce that
	 * everything is a GO now, so that all the fx's daemons are running.
	 */
	px_AdvanceInitState(PXOP_GO);
	px_RunCheckingDaemons();

	return 0;
#ifdef	AFS_DEBUG
    } else if (parm == PXOP_DUMPHOSTS) {
	px_AwaitInitState(PXOP_GO);
	code = DumpHosts();

	return code;
    } else if (parm == PXOP_DUMPPRINCS) {
	px_AwaitInitState(PXOP_GO);
	code = DumpPrincipals();

	return code;
    } else if (parm == PXOP_GETSTATS) {
	px_AwaitInitState(PXOP_GO);
	code = GetStats();

	return code;
#endif	/* AFS_DEBUG */
    } else if (parm == PXOP_SHUTDOWN) {
	px_AwaitInitState(PXOP_GO);
	code = shutdownFS();

	return code;

    } else if (parm == PXOP_PUTSECADVICE) {
	/* copy in security advice argument from user space */
	if (code = osi_copyin((char *)parm2,
			      (char *)&secAdvice, sizeof(secAdvice))) {
	    return code;
	}

	/* fill in security advice defaults and validate values */
	if (code = ValidateSecAdvice(&secAdvice)) {
	    return code;
	}

	/* update global security advice */
	lock_ObtainWrite(&fshost_SecurityLock);
	fshost_Security = secAdvice;
	lock_ReleaseWrite(&fshost_SecurityLock);

	return 0;

    } else if (parm == PXOP_GETSECADVICE) {
	/* read global security advice */
	lock_ObtainRead(&fshost_SecurityLock);
	code = osi_copyout((char *)&fshost_Security,
			   (char *)parm2, sizeof(fshost_Security));
	lock_ReleaseRead(&fshost_SecurityLock);

	return code;

    } else /* invalid PXOP */ {
	return EINVAL;
    }
}


/*
 * PX's background daemons
 */
#ifdef SGIMIPS
px_RunCheckingDaemons(void)
#else
px_RunCheckingDaemons()
#endif
{
    u_long fshsTime, kaTime, vlTime, xvolTime, vcTime, tempTime, tempNow, now;
    u_long tempIv;
    long code;

    fshsTime = kaTime = vlTime = xvolTime = vcTime = osi_Time();
    while (1) {
	tempNow = osi_Time();
	if (fshsTime <= tempNow)
	    fshsTime = (tempNow = osi_Time()) + fshs_HostCheckDaemon();
	if (kaTime <= tempNow)
	    kaTime = (tempNow = osi_Time()) + pxint_PeriodicKA();
	if (vlTime <= tempNow)
	    vlTime = (tempNow = osi_Time()) + px_CheckFLServers();
	if (xvolTime <= tempNow)
	    xvolTime = (tempNow = osi_Time()) + vol_GCDesc(1);
	if (vcTime <= tempNow)
	    vcTime = (tempNow = osi_Time()) + pxvc_Cleanups();
	tempTime = fshsTime;
	if (tempTime > kaTime) tempTime = kaTime;
	if (tempTime > vlTime) tempTime = vlTime;
	if (tempTime > xvolTime) tempTime = xvolTime;
	if (tempTime > vcTime) tempTime = vcTime;
	if (fshs_postRecovery) {
	    lock_ObtainWrite(&px_tsrlock);
	    if (tempTime > (fshs_tsrParams.endRecoveryTime+1))
		tempTime = fshs_tsrParams.endRecoveryTime+1;
	    lock_ReleaseWrite(&px_tsrlock);
	}

	if (tempTime > tempNow)
	    osi_Pause((tempTime - tempNow));
	/*
	 * Check to see whether its time to turn off the TSR flag
	 */
	if (fshs_postRecovery) {
	    lock_ObtainWrite(&px_tsrlock);
	    now = osi_Time();
	    if (now > fshs_tsrParams.endRecoveryTime) {
		icl_Trace2(px_iclSetp, PX_TRACE_BKGENDRECOVERY,
			   ICL_TYPE_LONG, now,
			   ICL_TYPE_LONG, fshs_tsrParams.endRecoveryTime);
		fshs_postRecovery = 0;
		zlc_SetRestartState(0);
	    }
	    lock_ReleaseWrite(&px_tsrlock);
	}
    } /* while */
}


#ifdef SGIMIPS
static shutdownFS(void)
#else
static shutdownFS()
#endif
{
    return -1;
}


/*
 * ValidateSecAdvice() -- for specified security advice, fill in defaults
 *     and validate values.
 *
 * RETURN CODES: 0 - OK,    EINVAL - security advice invalid
 */
static int ValidateSecAdvice(struct fshs_security_advice *secAdvice)
{
#define BadProtLevel(x) \
    ((x) < rpc_c_protect_level_none || \
     (x) > rpc_c_protect_level_pkt_privacy)

    /* check security advice format and fill in defaults */
    if (secAdvice->formatTag != FSHS_SEC_ADVICE_FORMAT_1) {
	return EINVAL;
    }
    if (secAdvice->local.minProtectLevel == rpc_c_protect_level_default) {
	secAdvice->local.minProtectLevel = rpc_c_protect_level_none;
    }
    if (secAdvice->local.maxProtectLevel == rpc_c_protect_level_default) {
	secAdvice->local.maxProtectLevel = rpc_c_protect_level_pkt_privacy;
    }
    if (secAdvice->nonLocal.minProtectLevel == rpc_c_protect_level_default) {
	secAdvice->nonLocal.minProtectLevel = rpc_c_protect_level_none;
    }
    if (secAdvice->nonLocal.maxProtectLevel == rpc_c_protect_level_default) {
	secAdvice->nonLocal.maxProtectLevel = rpc_c_protect_level_pkt_privacy;
    }

    /* validate security advice values */
    if (BadProtLevel(secAdvice->local.minProtectLevel) ||
	BadProtLevel(secAdvice->local.maxProtectLevel) ||
	secAdvice->local.minProtectLevel > secAdvice->local.maxProtectLevel ||

	BadProtLevel(secAdvice->nonLocal.minProtectLevel) ||
	BadProtLevel(secAdvice->nonLocal.maxProtectLevel) ||
	secAdvice->nonLocal.minProtectLevel >
	secAdvice->nonLocal.maxProtectLevel) {
	return EINVAL;
    } else {
	return 0;
    }
}
