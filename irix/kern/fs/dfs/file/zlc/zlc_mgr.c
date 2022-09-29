/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: zlc_mgr.c,v 65.8 1998/05/26 20:26:51 lmc Exp $";
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

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/volume.h>
#include <dcedfs/tkm_tokens.h>
#include <dcedfs/zlc.h>
#include <dcedfs/zlc_trace.h>
#if defined(AFS_HPUX_ENV) || defined(SGIMIPS)
#include <dce/ker/pthread.h>		/* for osi_ThreadCreate */
#endif
#ifdef SGIMIPS
#include <dcedfs/volreg.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/zlc/RCS/zlc_mgr.c,v 65.8 1998/05/26 20:26:51 lmc Exp $")

/* SHARED */
/* define remove queue and associated locks and counts */
osi_dlock_t zlc_removeQLock;
struct zlc_removeQueue zlc_removeQ = {0, 0};
int zlc_removeQSize  = 0;
int zlc_removeQClean = 0;

/* define held vnode target and threshold limits */
int zlc_vnTarget    = ZLC_VN_TARGET_DEF;
int zlc_vnThreshold = ZLC_VN_THRESHOLD_DEF;

/* define asynchronous OFD token grant lock */
osi_dlock_t zlc_asyncGrantLock;

/* define server crash TSR states and times */
int zlc_recoveryState = ZLC_REC_UNKNOWN;
unsigned long zlc_recoveryEnd = 0;

/* define ICL trace set */
struct icl_set *zlc_iclSetp = 0;

/* define ZLC module state */
int zlc_initted = 0;

/* SHARED */
extern void zlc_GetDeleteToken(struct zlc_removeItem *trp, int itemIsQueued);
extern void zlc_InitHost(void);
extern void zlc_GotoNextState(struct zlc_removeItem *trp);
extern void zlc_WakeupMgr(unsigned long serviceTime);

/* PRIVATE */
static void MgrThread(void *unused);
static unsigned long PruneQueue(void);
static void FreeDownToTarget(void);
static int VolInMoveTSR(struct volume *volp, unsigned long *movetimeoutp);

/* define wakeup handler, signal, and time */
static struct osi_WaitHandle wakeupHandler;
static int wakeupSignal = 0;
#ifdef SGIMIPS
static long wakeupTime = 0;
#else
static int wakeupTime = 0;
#endif

/* define some handy macros */
#define Min(a, b)  (((a) < (b)) ? (a) : (b))
#define Max(a, b)  (((a) > (b)) ? (a) : (b))




/*
 * zlc_Init() -- initialize the ZLC module
 *
 * RETURN CODES:
 *      0 -- ZLC module initialized
 *     !0 -- error initializing ZLC module
 */

/* EXPORT */
int zlc_Init(void)
{
    struct icl_log *logp;
#ifdef SGIMIPS
    int code;
#else
    long code;
#endif

    /* !!! WARNING: potential race for zlc_initted, but apparently this  !!!
     * !!! has not been a problem in the past.  Hmmmm...                 !!!
     */
    if (zlc_initted) {
	return (-1);
    }

    zlc_initted = 1;

    /* initialize ICL trace structures */
#ifdef	AFS_OSF11_ENV
    code = icl_CreateLog("disk", 2*1024, &logp);
#else
    code = icl_CreateLog("disk", 60*1024, &logp);
#endif

    if (code == 0) {
	code = icl_CreateSet("zlc", logp, (struct icl_log *)0, &zlc_iclSetp);
    }

    if (code) {
	printf("zlc: warning: can't initialize ICL tracing, code %d\n",
	       code);
	zlc_iclSetp = 0;
    }

    /* initialize locks and ZLC host structure */
    lock_Init(&zlc_removeQLock);
    lock_Init(&zlc_asyncGrantLock);
#if defined(SGIMIPS) && defined(_KERNEL)
    osi_InitWaitHandle(&wakeupHandler);
#endif
    zlc_InitHost();

    /* create ZLC background manager thread */
    osi_ThreadCreate(MgrThread, NULL, 0, 0, "zlc ", code);

    if (code) {
	printf("zlc: warning: can't start background thread, code %d\n",
	       code);
    }

    icl_Trace1(zlc_iclSetp, ZLC_TRACE_EXIT_INIT,
	       ICL_TYPE_LONG, code);

    return(code);
}


/*
 * MgrThread() -- background thread that sleeps until a remove queue entry
 *     requires service.
 *
 * RETURN CODES: none
 */

static void MgrThread(void *unused)
{
#ifdef SGIMIPS
    long interval;
#else
    unsigned long interval;
#endif

    (void)osi_PreemptionOff();

    /* manage the remove queue; never return */
    while (1) {
	/* Crude wakeup signal sufficient since 1) manager will not sleep
	 * forever in the event of a missed signal (interval has an upper
         * bound) and 2) holding a vnode for too long is not really an error.
	 */
	do {
	    wakeupSignal = 0;
#ifdef SGIMIPS
	    interval     = (long)PruneQueue();
#else
	    interval     = PruneQueue();
#endif
	} while (wakeupSignal || !interval);

	wakeupTime = osi_Time() + interval;

	/* sleep until interval expires or wakeup is signaled */
	icl_Trace1(zlc_iclSetp, ZLC_TRACE_MGR_WAKETIME,
		   ICL_TYPE_LONG, wakeupTime);

#ifdef SGIMIPS
	/*  The old code always made interval non-zero.  The
		1.2.2 code no longer does this.  If we pass a
		0 interval into osi_Wait....sv_timedwait...
		I think we might wait forever.  */
		
	if (!interval) {
		interval = 60 * 2;	/* Arbitrary 2 minutes */
	}
#endif
	osi_Wait(interval * 1000, &wakeupHandler, 0);
    }
    /* NOTREACHED */
}


/*
 * zlc_WakeupMgr() -- wake the background manager thread to schedule service
 *     and/or prune the remove queue, as necessary
 * 
 * ARGUMENTS: serviceTime indicates an upper bound on the time the manager
 *    thread should examine the remove queue; 0 indicates immediate action.
 *
 * ASSUMPTIONS: a positive serviceTime value is in the future; no real negative
 *    consequences if this is not true (see signal comment in MgrThread()).
 */

/* SHARED */
void zlc_WakeupMgr(unsigned long serviceTime)
{
    icl_Trace1(zlc_iclSetp, ZLC_TRACE_ENTER_WAKEUPMGR_2,
	       ICL_TYPE_LONG, serviceTime);

    if (serviceTime < wakeupTime) {
	/* wakeup manager to schedule service */
	osi_CancelWait(&wakeupHandler);
    } else {
	/* signal manager to schedule service if mgr is currently executing */
	wakeupSignal = 1;
    }
}


/*
 * PruneQueue() -- scan remove queue exactly once searching for entries that
 *     require service
 *
 *     NOTE: only items in a dally or cleanup state are considered for
 *     service; items in the ZLC_ST_START, ZLC_ST_OFDREQUEST, and
 *     ZLC_ST_QUEUED states are ignored as they are currently being serviced
 *     by another thread.
 * 
 * LOCKS USED: obtains a write lock on zlc_removeQLock; released on exit.
 *
 * SIDE EFFECTS: any item serviced can be transitioned to the next logical
 *     state and/or deallocated.
 *
 * RETURN CODES: Min(time till next item *seen* requires service, 10 minutes)
 *
 */

static unsigned long PruneQueue(void)
{
    register struct zlc_removeItem *trp, *nrp;
    struct vnode *vp;
#ifdef SGIMIPS
    long now, minTry, interval;
#else
    unsigned long now, minTry, interval;
#endif
    int freeVnodes, fromState;

    icl_Trace0(zlc_iclSetp, ZLC_TRACE_ENTER_PRUNEQUEUE_2);

    now    = osi_Time();
    minTry = now + (10 * 60);  /* scan every 10m, if not sooner (arbitrary) */

    /* scan remove queue searching for entries that require service */

    lock_ObtainWrite(&zlc_removeQLock);

    for (trp = zlc_removeQ.head; trp; trp = nrp) {
	nrp = trp->next;

	if (ZLC_ISDALLY_ST(trp->state)) {
	    /* entry is in a dally state and thus a candidate for transition */
	    fromState = trp->state;

	    if (trp->state == ZLC_ST_FXDALLY) {
		/* entry in FXDALLY state */

		if (trp->timeTry <= now ||
		    zlc_recoveryState != ZLC_REC_UNKNOWN) {
		    /* state re-evaluation time or FX up */
		    if (zlc_recoveryState == ZLC_REC_UNKNOWN &&
			trp->timeExp > now) {
			/* continue to wait for FX to start */
			trp->timeTry = Min(now + ZLC_T_FXDALLY_RETRY,
					   trp->timeExp);
		    } else {
			/* FX up or state expired; determine next state */
			zlc_GotoNextState(trp);
		    }
		}
	    } else if (trp->state == ZLC_ST_CRASHDALLY) {
		/* entry in CRASHDALLY state */

		if (trp->timeTry <= now ||
		    zlc_recoveryState == ZLC_REC_OVER) {
		    /* state re-evaluation time or crash TSR over */
		    if (zlc_recoveryState == ZLC_REC_ONGOING &&
			zlc_recoveryEnd > now) {
			/* continue to wait for crash TSR to complete */
			trp->timeTry = zlc_recoveryEnd;
			trp->timeExp = trp->timeTry;
		    } else {
			/* crash TSR done or expired; determine next state */
			zlc_GotoNextState(trp);
		    }
		}
	    } else if (trp->state == ZLC_ST_MOVEDALLY) {
		/* entry in MOVEDALLY state */

		if (trp->timeTry <= now) {
		    /* time to determine next state */
		    zlc_GotoNextState(trp);
		}
	    } else if (trp->state == ZLC_ST_REPDALLY) {
		/* entry in REPDALLY state */

		if (trp->timeTry <= now) {
		    /* time to determine next state */
		    zlc_GotoNextState(trp);
		}
	    }

	    if (fromState != trp->state) {
		/* log state transition */
		icl_Trace3(zlc_iclSetp, ZLC_TRACE_PRUNEQUEUE_STATETRANS,
			   ICL_TYPE_FID, &trp->fid,
			   ICL_TYPE_LONG, fromState,
			   ICL_TYPE_LONG, trp->state);
	    }

	    /* perform auxiliary actions if transitioned entry to a state
	     * that zlc_GotoNextState() considers terminal.
	     */
	    if (trp->state == ZLC_ST_OFDREQUEST) {
		/* request OFD token */
		if (nrp) ZLC_RQ_HOLD(nrp);
		ZLC_RQ_HOLD(trp);
		lock_ReleaseWrite(&zlc_removeQLock);

		lock_ObtainRead(&zlc_asyncGrantLock);
		zlc_GetDeleteToken(trp, 1 /* item IS on remove queue */);
		lock_ReleaseRead(&zlc_asyncGrantLock);

		lock_ObtainWrite(&zlc_removeQLock);
		ZLC_RQ_RELE(trp);
		if (nrp) ZLC_RQ_RELE(nrp);

	    } else if (trp->state == ZLC_ST_CLEANUP) {
		/* release vnode */
		vp = trp->vp;
		zlc_removeQClean++;

		if (nrp) ZLC_RQ_HOLD(nrp);
		ZLC_RQ_HOLD(trp);
		lock_ReleaseWrite(&zlc_removeQLock);

		tkc_FlushVnode(vp);
		OSI_VN_RELE(vp);

		lock_ObtainWrite(&zlc_removeQLock);
		ZLC_RQ_RELE(trp);
		if (nrp) ZLC_RQ_RELE(nrp);

		osi_assert(trp->state == ZLC_ST_CLEANUP);
	    }

	    /* adjust minTry time if entry still in a dally state */
	    if (ZLC_ISDALLY_ST(trp->state)) {
		minTry = Min(minTry, trp->timeTry);
	    }
	}

	/* cleanup entry if possible */
	if ((trp->state == ZLC_ST_CLEANUP) && (!ZLC_RQ_ISHELD(trp))) {
	    /* remove entry from queue */
	    ZLC_RQ_DEQ(trp);
	    zlc_removeQSize--;
	    zlc_removeQClean--;

	    /* deallocate storage */
	    osi_Free((opaque)trp, sizeof(*trp));
	}
    }

    osi_assert(zlc_removeQClean <= zlc_removeQSize);

    /* if queue still needs further pruning, sacrifice vnodes */
    freeVnodes = (zlc_removeQSize - zlc_removeQClean > zlc_vnThreshold);

    lock_ReleaseWrite(&zlc_removeQLock);

    if (freeVnodes) {
	FreeDownToTarget();
    }

    /* determine sleep interval from (adjusted) current time */
    now      = osi_Time();
    interval = ((minTry > now) ? (minTry - now) : 0);

    icl_Trace1(zlc_iclSetp, ZLC_TRACE_EXIT_PRUNEQUEUE_2,
	       ICL_TYPE_LONG, interval);
    return interval;
}



/*
 * FreeDownToTarget() -- release vnodes in FIFO order until target is reached
 *
 * LOCKS USED: obtains a write lock on zlc_removeQLock; released on exit.
 *
 * SIDE EFFECTS: can transition any remove queue entry to the ZLC_ST_CLEANUP
 *     state, and then deallocate it if not held.
 *
 * RETURN CODES: none
 */

static void FreeDownToTarget(void)
{
    register struct zlc_removeItem *trp, *nrp;
    struct vnode *vp;

    /* scan queue, releasing vnodes in FIFO order until target is reached */

    lock_ObtainWrite(&zlc_removeQLock);

    icl_Trace1(zlc_iclSetp, ZLC_TRACE_ENTER_FD2T,
	       ICL_TYPE_LONG, (zlc_removeQSize - zlc_removeQClean));

    for(trp = zlc_removeQ.tail;
	(trp) && (zlc_removeQSize - zlc_removeQClean > zlc_vnTarget);
	trp = nrp) {

	nrp = trp->prev;

	if (trp->state != ZLC_ST_CLEANUP) {
	    /* release vnode */
	    icl_Trace3(zlc_iclSetp, ZLC_TRACE_FD2T_STATETRANS,
		       ICL_TYPE_FID, &trp->fid,
		       ICL_TYPE_LONG, trp->state,
		       ICL_TYPE_LONG, ZLC_ST_CLEANUP);

	    vp = trp->vp;
	    trp->state = ZLC_ST_CLEANUP;
	    zlc_removeQClean++;

	    /* hold items so they'll be there when lock is re-obtained */
	    if (nrp) ZLC_RQ_HOLD(nrp);
	    ZLC_RQ_HOLD(trp);
	    lock_ReleaseWrite(&zlc_removeQLock);

	    tkc_FlushVnode(vp);
	    OSI_VN_RELE(vp);

	    lock_ObtainWrite(&zlc_removeQLock);
	    ZLC_RQ_RELE(trp);
	    if (nrp) ZLC_RQ_RELE(nrp);

	    osi_assert(trp->state == ZLC_ST_CLEANUP);
	}

	/* cleanup entry if possible */
	if (!ZLC_RQ_ISHELD(trp)) {
	    /* remove item from queue */
	    ZLC_RQ_DEQ(trp);
	    zlc_removeQSize--;
	    zlc_removeQClean--;

	    /* deallocate storage */
	    osi_Free((opaque)trp, sizeof(*trp));
	}
    }

    osi_assert(zlc_removeQClean <= zlc_removeQSize);

    icl_Trace1(zlc_iclSetp, ZLC_TRACE_EXIT_FD2T,
	       ICL_TYPE_LONG, (zlc_removeQSize - zlc_removeQClean));

    lock_ReleaseWrite(&zlc_removeQLock);

    return;
}


/*
 * zlc_GotoNextState() -- transition specified remove item to the next logical
 *     state based on the current state and the prevailing conditions.
 *
 *     NOTE: the states ZLC_ST_OFDREQUEST and ZLC_ST_CLEANUP are considered
 *     terminal by this function, and hence no further transition occurs.
 *
 *     Furthermore, it is the responsiblity of the caller to perform any
 *     auxiliary action associated with entering a terminal state, e.g.
 *     releasing the vnode on a transition to ZLC_ST_CLEANUP.
 *
 * ASSUMPTIONS: caller has exclusive access to *trp.
 *
 * CAUTIONS: all (non-terminal) states transition to the ZLC_ST_CLEANUP
 *     state if the fileset descriptor can not be located in the registry.
 *
 * RETURN CODES: none
 */

/* SHARED */
void zlc_GotoNextState(struct zlc_removeItem *trp)
{
    struct volume *volp;
    long code;
    unsigned long now, movetimeout;

    /* check that item is not in a terminal state as defined by this fn */
    switch(trp->state) {
      case ZLC_ST_OFDREQUEST:
      case ZLC_ST_CLEANUP:
#ifdef SGIMIPS
	goto EXIT1;		/* SGI doesn't like using "EXIT" */
#else
	goto EXIT;
#endif
    }

    /* lookup fileset descriptor in the registry */
    if (code = volreg_Lookup(&trp->fid, &volp)) {
	/* lookup failed; fileset not attached; vnode can be released */
	icl_Trace2(zlc_iclSetp, ZLC_TRACE_GOTONEXTSTATE_NOVOL,
		   ICL_TYPE_FID, &trp->fid,
		   ICL_TYPE_LONG, code);

	trp->state = ZLC_ST_CLEANUP;
#ifdef SGIMIPS
	goto EXIT1;		/* SGI doesn't like using "EXIT" */
#else
	goto EXIT;
#endif
    }

    /* get shared (rather than read) volume lock required by VolInMoveTSR() */
    lock_ObtainShared(&volp->v_lock);

    now = osi_Time();

    /* jump to current state in transition table */
    switch(trp->state) {
      case ZLC_ST_START:      goto START;
      case ZLC_ST_QUEUED:     goto QUEUED;
      case ZLC_ST_FXDALLY:    goto FXDALLY;
      case ZLC_ST_CRASHDALLY: goto CRASHDALLY;
      case ZLC_ST_MOVEDALLY:  goto MOVEDALLY;
      case ZLC_ST_REPDALLY:   goto REPDALLY;
      default:                goto FINISH;  /* should never occur */
    }

    /* transition table for determining next state */
  START:
    if (volp->v_states & VOL_NOEXPORT) {
	/* fileset is not exported; can request OFD token */
	trp->state = ZLC_ST_OFDREQUEST;

	goto FINISH;
    }

  T1:
    if (zlc_recoveryState == ZLC_REC_UNKNOWN) {
	/* FX is not running */
	trp->state   = ZLC_ST_FXDALLY;
	trp->timeTry = now + ZLC_T_FXDALLY_RETRY;
	trp->timeExp = now + ZLC_T_FXDALLY_MAX;

	goto FINISH;
    }

  T2:
    if (zlc_recoveryState == ZLC_REC_ONGOING) {
	/* server in crash TSR */
	trp->state   = ZLC_ST_CRASHDALLY;
	trp->timeTry = zlc_recoveryEnd;
	trp->timeExp = trp->timeTry;

	goto FINISH;
    }

  T3:
    if (VolInMoveTSR(volp, &movetimeout)) {
	/* fileset in move TSR */
	trp->state   = ZLC_ST_MOVEDALLY;
	trp->timeTry = movetimeout;
	trp->timeExp = movetimeout;

	goto FINISH;
    }

  T4:
    if (volp->v_states & VOL_IS_REPLICATED) {
	/* fileset is replicated */
	trp->state   = ZLC_ST_REPDALLY;
	trp->timeTry = now + ZLC_T_REPDALLY_FIX;
	trp->timeExp = now + ZLC_T_REPDALLY_FIX;

	goto FINISH;
    }

  T5:
    /* can request OFD token */
    trp->state = ZLC_ST_OFDREQUEST;

    goto FINISH;

  QUEUED:
    if (VolInMoveTSR(volp, &movetimeout)) {
	/* fileset in move TSR */
	trp->state   = ZLC_ST_MOVEDALLY;
	trp->timeTry = movetimeout;
	trp->timeExp = movetimeout;
    } else {
	/* fileset not in motion; vnode can be released */
	trp->state = ZLC_ST_CLEANUP;
    }

    goto FINISH;

  FXDALLY:
    if (zlc_recoveryState == ZLC_REC_UNKNOWN) {
	/* FX still not running; can request OFD token */
	trp->state = ZLC_ST_OFDREQUEST;

	goto FINISH;
    } else {
	/* FX is finally running; continue state evaluation */

	goto T2;
    }

  CRASHDALLY:
    /* extraneous indirection for readability and completeness of table */
    /* continue state evaluation */

    goto T3;

  MOVEDALLY:
    /* extraneous indirection for readability and completeness of table */
    /* continue state evaluation */

    goto T3;

  REPDALLY:
    if (VolInMoveTSR(volp, &movetimeout)) {
	/* fileset in move TSR */
	trp->state   = ZLC_ST_MOVEDALLY;
	trp->timeTry = movetimeout;
	trp->timeExp = movetimeout;
    } else {
	/* fileset not in motion; can request OFD token */
	trp->state = ZLC_ST_OFDREQUEST;
    }

    goto FINISH;

  FINISH:
    /* release volume lock and rele volume */
    lock_ReleaseShared(&volp->v_lock);
    VOL_RELE(volp);

#ifdef SGIMIPS
  EXIT1:	/* SGI doesn't like use of "EXIT" */
#else
  EXIT:
#endif
    return;
}


/*
 * VolInMoveTSR() -- determine if fileset is source or target of an
 *     inter-machine move, or target of an intra-machine move.
 * 
 * LOCKS USED: *volp must be held and SHARE locked on entry; lock may be
 *     upgraded to write, but if so it will be downgraded to share on exit.
 *
 * RETURN CODES:
 *     0 -- fileset is not in move TSR
 *     1 -- fileset is in move TSR; *movetimeoutp set to move timeout
 */

static int VolInMoveTSR(struct volume *volp, unsigned long *movetimeoutp)
{
    int code;
    unsigned long now;

    code = 0;

    /* First attempt to force clean-up on inter-machine move, if necessary */

    if (volp->v_states & (VOL_MOVE_TARGET | VOL_MOVE_SOURCE)) {
	if (volp->v_volMoveTimeout != 0 && (volp->v_states & VOL_BUSY) == 0) {
	    if (lock_UpgradeSToWNoBlock(&volp->v_lock) != LOCK_NOLOCK) {
		vol_SetMoveTimeoutTrigger(volp);
		lock_ConvertWToS(&volp->v_lock);
	    }
	}
    }

    /* Now check if move is in progress */

    now = osi_Time();

    if (volp->v_states & (VOL_MOVE_TARGET | VOL_MOVE_SOURCE)) {
	/* Inter-machine move indicated; check timeout */
	if (volp->v_volMoveTimeout <= now) {
	    /* Timeout has expired.  Rather than trying to reset these flags,
	     * we're just going to try again later, with the hope that this
	     * fileset will be cleaned up or destroyed.
	     */
	    *movetimeoutp = now + (10 * 60);
	} else {
	    /* Move still in progress */
	    *movetimeoutp = volp->v_volMoveTimeout;
	}
	code = 1;
    } else if (!AFS_hiszero(volp->v_parentId) &&
	       !AFS_hsame(volp->v_parentId, volp->v_volId)) {
	/* Fileset is the target of a local move; set timeout to 60 seconds. */
	*movetimeoutp = now + 60;
	code = 1;
    }

    return code;
}


/*
 * zlc_SetRestartState() -- indicate that crash TSR is entered/extended or
 *     has completed; called by FX.
 * 
 * ARGUMENTS: a positive expiration time indicates entry/extension of
 *     crash TSR period; a time of 0 indicates that crash TSR is complete.
 *
 * RETURN CODES: none
 */

/* EXPORT */
void zlc_SetRestartState(unsigned long expire)
{
    int fromState;

    if (!zlc_initted) {
	(void)zlc_Init();
    }

    icl_Trace1(zlc_iclSetp, ZLC_TRACE_ENTER_SETRESTARTSTATE,
	       ICL_TYPE_LONG, expire);

    fromState = zlc_recoveryState;

    /* state/time update ordering chosen to obviate need for lock */

    if (expire == 0) {
	/* leave recovery expiration time set to previous value */
	zlc_recoveryState = ZLC_REC_OVER;
    } else {
	/* set/update recovery expiration time before setting (new) state */
	zlc_recoveryEnd   = expire;
	zlc_recoveryState = ZLC_REC_ONGOING;
    }

    if (fromState != zlc_recoveryState) {
	/* wake mgr thread to evaluate items given state change */
	zlc_WakeupMgr(0);
    }

    icl_Trace0(zlc_iclSetp, ZLC_TRACE_EXIT_SETRESTARTSTATE);
}
