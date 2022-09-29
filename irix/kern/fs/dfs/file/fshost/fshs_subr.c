/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: fshs_subr.c,v 65.7 1998/04/02 19:44:15 bdr Exp $";
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
 *      Copyright (C) 1995, 1990 Transarc Corporation
 *      All rights reserved.
 */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/debug.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>
#include <dcedfs/queue.h>
#include <dcedfs/volume.h>
#include <dcedfs/vol_init.h>
#include <dcedfs/zlc.h>
#include <fshs.h>
#include <fshs_trace.h>
#include <dce/sec_cred.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/fshost/RCS/fshs_subr.c,v 65.7 1998/04/02 19:44:15 bdr Exp $")

/* declare ICL trace event set structure pointer */
struct icl_set *fshs_iclSetp = (struct icl_set *)0;

/* 
 * Declaration of variables 
 */
struct fshs_tsrParams fshs_tsrParams;
struct fshs_stats fshs_stats = 	
{0, 0, 0, 0, 0, 0, 0, FSHS_PROBETIME, FSHS_HOSTGCTIME, FSHS_PRINCIPALGCTIME};

int fshs_postRecovery = 0;
int fshs_HostCheckInterval = 300;	/* Default every 5 minutes */
char *fshs_cellNamep;             /* Cell the server belongs to */
int fshs_cellNameLen;

osi_dlock_t fshost_SecurityLock;
struct fshs_security_advice fshost_Security;

#ifdef LOCALHOST
struct sockaddr_in *localHost;		/* Id address for local host */
#endif

/* 
 * Initialize the host module; must be called before any other routine 
 * in this module 
 */
static int fshs_initialized=0;
#ifdef SGIMIPS
int fshs_InitHostModule(
    char *cellNamep,
    struct fshs_security_advice *secAdviceP)
#else
int fshs_InitHostModule(cellNamep, secAdviceP)
    char *cellNamep;
    struct fshs_security_advice *secAdviceP;
#endif
{
    int i; 
    long code;
    struct icl_log *logp;
#ifndef SGIMIPS
    error_status_t st;
#endif /* SGIMIPS */

    if (fshs_initialized) 
	return 0;
    fshs_initialized = 1;
    /* leave room for the '/' at the end of the cellname */
#ifdef SGIMIPS
    /* Surely the cell name will fit in 32 bits, so just cast it to
	get rid of the warning. */
    fshs_cellNameLen = (int)strlen(cellNamep)+1;
#else  /* SGIMIPS */
    fshs_cellNameLen = strlen(cellNamep)+1;
#endif /* SGIMIPS */
    /* now leave room for the trailing NUL */
    fshs_cellNamep = osi_Alloc(fshs_cellNameLen+1);
    strcpy(fshs_cellNamep, cellNamep);
    fshs_cellNamep[fshs_cellNameLen-1] = '/';
    fshs_cellNamep[fshs_cellNameLen] = '\0';

    /* set global security advice and initialize lock */
    lock_Init(&fshost_SecurityLock);
    fshost_Security = *secAdviceP;

    /* initialize ICL trace structures */
#ifdef	AFS_OSF11_ENV
    code = icl_CreateLog("cmfx", 2*1024, &logp);
#else
    code = icl_CreateLog("cmfx", 0, &logp);
#endif
    if (code == 0)
	code = icl_CreateSet("fshost", logp, (struct icl_log *) 0, &fshs_iclSetp);
    if (code)
	    printf("fshost: warning: can't initialize fshost package ICL tracing, code %d\n", code);

    fshs_InitPrincMod();
    fshs_InitHostMod();
    fshs_InitUnAuthPA();

    QInit(&fshs_fifo);

    for (i = 0; i < FSHS_NHOSTS; i++) {
        fshs_priHostID[i].next = NULL;
        fshs_secHostID[i].next = NULL;
    }

#ifdef	LOCALHOST
    GetLocalHostId(&localHost); 
#endif
    return 0;
}


fshs_SetRecoveryParameters(lifeGuarantee, RPCGuarantee, deadServer, 
			   postRecovery, maxLife, maxRPC)
    u_long lifeGuarantee;
    u_long RPCGuarantee;
    u_long deadServer;
    u_long postRecovery;
    u_long maxLife;
    u_long maxRPC;
{
    icl_Trace0(fshs_iclSetp, FSHS_SETRECOVPARM);

    bzero((char *) &fshs_tsrParams, sizeof (struct fshs_tsrParams));
    fshs_tsrParams.endRecoveryTime = fshs_tsrParams.serverRestart = osi_Time();
    fshs_tsrParams.lifeGuarantee = lifeGuarantee;
    fshs_tsrParams.RPCGuarantee = RPCGuarantee;
    fshs_tsrParams.deadServer = deadServer;
    fshs_tsrParams.maxLife = maxLife;
    fshs_tsrParams.maxRPC = maxRPC;

    if (postRecovery)  {
	u_long tsrInterval;

	/* 
	 * set the initial TSR recovery interval to the max of 
	 * the maximum host life guarantee and the dead server 
	 * poll interval.
	 */
	tsrInterval = fshs_tsrParams.maxLife > fshs_tsrParams.deadServer ? 
	    fshs_tsrParams.maxLife : fshs_tsrParams.deadServer;
        fshs_postRecovery = 1;
        fshs_tsrParams.endRecoveryTime = fshs_tsrParams.serverRestart +
	    tsrInterval + INITIAL_GRACE_PERIOD;
	zlc_SetRestartState(fshs_tsrParams.endRecoveryTime);
    } else {
	zlc_SetRestartState(0);		/* already over!! */
    }
#ifdef SGIMIPS
    return 0;
#endif
}

/* 
 * This routine calls the (*proc) routine for every host in the hash table; if
 * an error is returned by (*proc) it exitsprematurely. This 
 * routine is called by periodic GC daemons (i.e. fshs_HostCheckDaemon), and 
 * various debugging routines.
 * Note, This is called with no lock held.
 */
int fshs_Enumerate(proc, param)
    int (*proc)();
    char *param;
{
    register struct fshs_host *hostp;
    register struct squeue *tq;
    long errorCode; 

    icl_Trace0(fshs_iclSetp, FSHS_STARTENUMERATE);

    lock_ObtainRead(&fshs_hostlock);
    /* 
     * In search of hosts that could be marked STALE.
     */
    for (tq = fshs_fifo.prev; tq != &fshs_fifo; tq = QPrev(tq)) {
        hostp = FSHS_QTOH(tq);
	if (errorCode = (*proc)(hostp, param)) {
	    lock_ReleaseRead(&fshs_hostlock);
	    return errorCode;
	}
    }
    lock_ReleaseRead(&fshs_hostlock);
    /*
     * Move those STALE ones to the freeHost list.
     */
    lock_ObtainWrite(&fshs_hostlock);
    fshs_UpdateHostList(FSHS_MINFIFOHOSTS);
    lock_ReleaseWrite(&fshs_hostlock);
    icl_Trace0(fshs_iclSetp, FSHS_ENDENUMERATE);
    return 0;
}

/*    
 * Periodically called to invalidate obsolete host/principal entries 
 * Locks: called with READ fshs_hostlock.
 */
int fshs_CheckHost(hostp)
    register struct fshs_host *hostp;
{
    register struct fshs_principal *princp;
#ifndef SGIMIPS
    struct fshs_host *shostp;
#endif /* SGIMIPS */
    u_long now = osi_Time();
    int num;

    icl_Trace1(fshs_iclSetp, FSHS_STARTCHECKHOST, ICL_TYPE_POINTER, hostp);

    lock_ObtainWrite(&hostp->h.lock);
    if (!(hostp->flags & FSHS_HOST_STALE)) {
        /* 
	 * Don't mark the host STALE unless (refCount == 0 AND host DOWN).
	 * Once the host is marked DOWN, the server (after receiving the
	 * the request from the same host later on), will do the callback 
	 * which triggers the TSR work on the host. 
	 * Therefore, we mark the host DOWN only when the token revocation
	 * fails after some 'hostLifeGuarantee' interval. Or, the server 
	 * has not heard from this host for some 'hostRPCGuarantee' interval.
	 */
	if ((hostp->lastCall + hostp->hostRPCGuarantee) < now)
	    hostp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);

	if (hostp->h.states & HS_HOST_HOSTDOWN && (hostp->h.refCount == 0)) {
	    hostp->flags |= FSHS_HOST_STALE;
	}
	if (hostp->flags & FSHS_HOST_STALE) 
	    goto done;

	for (princp = hostp->princListp; princp; princp = princp->next) {
	    lock_ObtainWrite(&princp->lock);
	    if (!(princp->states & FSHS_PRINC_STALE)) {
	        if (princp->lastCall + fshs_stats.principalGCTime < now
		    && princp->refCount == 0) {
		    princp->states |= FSHS_PRINC_STALE;
		    icl_Trace2(fshs_iclSetp, FSHS_STALEPRINC,ICL_TYPE_POINTER,
			       princp, ICL_TYPE_LONG, princp->refCount);
		}
	    }
	    lock_ReleaseWrite(&princp->lock);
	}
	fshs_GCPrinc(hostp, &num);	/* garbage collect STALE principals */
    }
done:
    lock_ReleaseWrite(&hostp->h.lock);
    icl_Trace0(fshs_iclSetp, FSHS_ENDCHECKHOST);
    return 0;
}

/* 
 * Special kernel host module related process (worker): It does periodic GC 
 * of host/principal entries 
 */
u_long fshs_HostCheckDaemon()
{
    icl_Trace0(fshs_iclSetp, FSHS_CHECKDAEMON);
    (void) fshs_Enumerate(fshs_CheckHost, (char *)0);
    return fshs_HostCheckInterval;
}


/* wait for other callers to finish, and then mark call as running.
 * called without any locks held.
 */
fshs_StartCall(hp)
  register struct fshs_host *hp;
{
    lock_ObtainWrite(&hp->h.lock);
    while (1) {
	/* wait for calls to complete, so we can serialize this mess */
	if (hp->flags & FSHS_HOST_INCALL) {
	    hp->flags |= FSHS_HOST_WAITCALL;
	    osi_SleepW(hp, &hp->h.lock);
	    lock_ObtainWrite(&hp->h.lock);
	}
	else {
	    hp->flags |= FSHS_HOST_INCALL;
	    lock_ReleaseWrite(&hp->h.lock);
	    return 0;
	}
    }
}


/* end a call, waking others waiting for this call to complete;
 * called with host write-locked!
 */
fshs_EndCall(hp)
  register struct fshs_host *hp;
{
    hp->flags &= ~FSHS_HOST_INCALL;
    if (hp->flags & FSHS_HOST_WAITCALL) {
	hp->flags &= ~FSHS_HOST_WAITCALL;
	osi_Wakeup((char *)hp);
    }
    return 0;
}


/*
 * Debugging Routines; they are called from external debugging utilities
 */
#ifdef  AFS_DEBUG
/*
 * Print all hosts in the hash table
 */
DumpHosts()
{
    int PrintHost();

    (void) fshs_Enumerate(PrintHost, (char *)0);    /* Ignore error code */
}


/*
 * Print all hosts with their principals
 */
DumpPrincipals()
{
    int PrintHostPrincipals();
    (void)fshs_Enumerate(PrintHostPrincipals,(char *)0);/* Ignore error code */
}


static fshs_dummy()
{
    return 0;
}

/*
 * Get some stats about the host entries
 */
GetStats()
{
   (void) fshs_Enumerate(fshs_dummy, (char *)0);
}



/*
 * Print out a host entry
 */
PrintHost(hostp)
    register struct fshs_host *hostp;
{
  char *objID;
  unsigned32 st;

  uuid_to_string(&(hostp->clientMainID), &objID, &st);

  rpc_string_free(&objID, &st);
}


/*
 * Print out a host entry with all the principals that are associated with it
 */
PrintHostPrincipals(hostp)
    register struct fshs_host *hostp;
{

    register struct fshs_principal *princp;

    if (hostp->flags & FSHS_HOST_STALE)
        return;                            /* Ignore stale hosts */
    PrintHost(hostp);
    lock_ObtainRead(&hostp->h.lock);
    for (princp = hostp->princListp; princp; princp = princp->next) {
        /*
         * If principal not stale, print it
         */
        if (!(princp->states & FSHS_PRINC_STALE))
            PrintPrincipal(princp);
      }
    lock_ReleaseRead(&hostp->h.lock);
  }


/*
 * Print out a principal entry
 */
PrintPrincipal(princp)
    register struct fshs_principal *princp;
{
  char *uid;
  char *cid;
  unsigned32 st;

  uuid_to_string(&princp->delegationChain->lastDelegate->pacp->principal.uuid,
		 &uid, &st);
  uuid_to_string(&princp->delegationChain->lastDelegate->pacp->realm.uuid,
		 &cid, &st);
  printf("0x%x: state=0x%x userid=%s cellId=%s hostp=0x%x lastCall=%d\n",
	 princp, princp->states, uid, cid, princp->hostp, princp->lastCall);
  rpc_string_free(&cid, &st);
  rpc_string_free(&uid, &st);
}
#endif  /* AFS_DEBUG */
