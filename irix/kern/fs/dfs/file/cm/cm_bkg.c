/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_bkg.c,v 65.7 1998/03/23 16:23:53 gwehrman Exp $";
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

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/queue.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_bkg.h>
#include <cm_dcache.h>

#ifdef SGIMIPS
void cm_EnterBKG(void);                 /* For compiler, it doesn't believe */
void cm_LeaveBKG(void);                 /* there are no parms without void */
int cm_StoreDCache(struct cm_scache *, struct cm_dcache *, int,
                        struct cm_rrequest *);
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_bkg.c,v 65.7 1998/03/23 16:23:53 gwehrman Exp $")

/* 
 * Background daemon-related variables 
 */
static struct cm_bkgrequest cm_bkgReqs[BK_NBRS];/* request structures */
static struct cm_bkgrequest_list *cm_bkgrequest_list =
	(struct cm_bkgrequest_list *) 0; /* auxiliary list */
struct lock_data cm_bkgLock;		/* lock for bkg */
short cm_bkgDaemons = 0;		/* # of daemons waiting for bkg reqs */
short cm_bkgUrgent = 0;			/* # of urgent bkg reqs */
short cm_bkgWaiters = 0;		/* # of users waiting for bkg buffers */
static void bkg_Path _TAKES((struct cm_bkgrequest *));
static int bkg_Prefetch _TAKES((struct cm_bkgrequest *));
static void bkg_Store _TAKES((struct cm_bkgrequest *));
static void bkg_ReturnToken _TAKES((struct cm_bkgrequest *));

/*
 * Process a bkg request.  Called from cm_bkgDaemon.
 *
 * Called with cm_bkgLock held; will drop and reobtain it.
 */
#ifdef SGIMIPS
static void cm_bkgDo( struct cm_bkgrequest *breqp)
#else  /* SGIMIPS */
static void cm_bkgDo(breqp)
    struct cm_bkgrequest *breqp;
#endif /* SGIMIPS */
{
    /* 
     * New request, not yet picked up 
     */
    breqp->states |= BK_STARTED;
    if (breqp->states & BK_URGENT) {
	breqp->states &= ~BK_URGENT;
	cm_bkgUrgent--;
    }
    lock_ReleaseWrite(&cm_bkgLock);
    icl_Trace2(cm_iclSetp, CM_TRACE_BKGFOUND, ICL_TYPE_LONG,
    	   breqp->opcode, ICL_TYPE_POINTER, breqp->vnodep);
    if (breqp->opcode == BK_FETCH_OP)
	breqp->code = bkg_Prefetch(breqp);
    else if (breqp->opcode == BK_PATH_OP)
	bkg_Path(breqp);
    else if (breqp->opcode == BK_STORE_OP)
	bkg_Store(breqp);
    else if (breqp->opcode == BK_RET_TOK_OP)
	bkg_ReturnToken(breqp);
    else
	panic("background bop");
    if (breqp->vnodep) {
	/* fix up reference count */
	cm_PutSCache(breqp->vnodep);
	breqp->vnodep = (struct cm_scache *) 0;
    }
    if (breqp->credp) {
	crfree(breqp->credp);
	breqp->credp = (osi_cred_t *) 0;
    }

    /* wakeup anyone waiting for code to become valid */
    lock_ObtainWrite(&cm_bkgLock);
    breqp->states |= BK_CODEVALID;
    if (breqp->states & BK_CODEWAIT) {
	breqp->states &= ~BK_CODEWAIT;
	osi_Wakeup((char *)breqp);
    }
    icl_Trace3(cm_iclSetp, CM_TRACE_BKGDONE,
	       ICL_TYPE_POINTER, breqp,
	       ICL_TYPE_LONG, breqp->opcode,
	       ICL_TYPE_LONG, breqp->code);
    lock_ReleaseWrite(&cm_bkgLock);

    /* Drop our reference.
     * This grabs and releases cm_bkgLock lock.
     */
    cm_bkgRelease(breqp);
    lock_ObtainWrite(&cm_bkgLock);
}

/*
	Checks for a lower offset for this vnode.
	Come here with cm_bkgLock held.
*/

#ifdef SGIMIPS
lower_offs_qued(struct cm_bkgrequest *pr)
#else  /* SGIMIPS */
lower_offs_qued(pr)
struct cm_bkgrequest *pr;
#endif /* SGIMIPS */
{
	struct cm_bkgrequest *breqp;
	int i;

        breqp = cm_bkgReqs;
        for (i = 0; i < BK_NBRS; i++, breqp++)
	{
        	if ((breqp->refCount > 0) &&
			 !(breqp->states & BK_STARTED))
		{
			if(breqp->vnodep==pr->vnodep &&
				breqp->opcode==pr->opcode &&
				breqp->parm[0]<pr->parm[0])
				return 1;	/* lower offset */
		}
	}

	/* no lower offset */

	return 0;
}

/*
        Checks for a prefetching chunk already queued.
        Come here with cm_bkgLock held.
*/

#ifdef SGIMIPS
struct cm_bkgrequest *already_qued(
    struct cm_scache *scp,
    long offset)
#else  /* SGIMIPS */
struct cm_bkgrequest *already_qued(scp,offset)
struct cm_scache *scp;
long offset;
#endif /* SGIMIPS */
{
        struct cm_bkgrequest *breqp;
        int i;

        breqp = cm_bkgReqs;
        for (i = 0; i < BK_NBRS; i++, breqp++)
        {
                if ((breqp->refCount > 0) &&
                         (breqp->opcode == BK_FETCH_OP) &&
			 (breqp->vnodep == scp) &&
			 (breqp->parm[0] == offset))
                {
                        return breqp;       /* already on queue */
                }
        }

        /* not on queue */

        return (struct cm_bkgrequest *) 0;
}

/* 
 * Background daemon server: Many such daemons may be started by a cache 
 * manager special syscall 
 */
#ifdef SGIMIPS
void cm_bkgDaemon(void) 
#else  /* SGIMIPS */
void cm_bkgDaemon() 
#endif /* SGIMIPS */
{
    register struct cm_bkgrequest *breqp;
    register int i, foundAny;
    struct cm_bkgrequest_list *breqLp;
    static int cm_bkgInit = 0;

    /* 
     * Initialize subsystem 
     */
    if (cm_bkgInit == 0) {
	lock_Init(&cm_bkgLock);
	bzero((char *) cm_bkgReqs, sizeof(cm_bkgReqs));
	cm_bkgInit = 1;
    }
    lock_ObtainWrite(&cm_bkgLock);
    for (;;) {
	/* 
	 * Find a request 
	 */
	lock_ReleaseWrite(&cm_bkgLock);
	cm_EnterBKG();
	lock_ObtainWrite(&cm_bkgLock);
	/* First scan for urgent requests */
	/*
	 * BK_FLAG_ALLOC and BK_FLAG_NOWAITD are assumed to be mutually
	 * exclusive.  The following code only looks on cm_bkgReqs, never in
	 * cm_bkgrequest_list, for "urgent" requests.
	 */
urgents:
	while (cm_bkgUrgent) {
	    breqp = cm_bkgReqs;
	    for (i = 0; i < BK_NBRS; i++, breqp++) {
		/* stupid convention: when refCount is visible while holding
		 * cm_bkgLock, then this is a properly formatted request.
		 */
		if ((breqp->refCount > 0) &&
		    ((breqp->states & (BK_STARTED | BK_URGENT)) == BK_URGENT)) {
		    cm_bkgDo(breqp);
		}
	    }
	}
	/* Now scan for non-urgent requests */
	foundAny = 0;
	breqp = cm_bkgReqs;
	for (i = 0; i < BK_NBRS; i++, breqp++) {
	    /* stupid convention: when refCount is visible while holding
	     * cm_bkgLock, then this is a properly formatted request.
	     */
	    if ((breqp->refCount > 0) && !(breqp->states & BK_STARTED)){

                foundAny = 1;

		/*
		 at this time just fetch or storeback
		 the chunk with lowest offset
		*/

		if((breqp->opcode==BK_STORE_OP ||
			 breqp->opcode==BK_FETCH_OP) &&
			 lower_offs_qued(breqp))
		{
			/*
			skip this one for now,
			pick it up again next
			time through the loop
			*/

			continue;
		}

		cm_bkgDo(breqp);
		if (cm_bkgUrgent) goto urgents;
	    }
	}
	while (cm_bkgrequest_list) {
	    breqLp = cm_bkgrequest_list;
	    breqp = &breqLp->r;
	    if ((breqp->refCount <= 0) || (breqp->states & BK_STARTED))
		break;			/* Not supposed to happen */
	    foundAny = 1;
	    cm_bkgrequest_list = breqLp->next;
	    cm_bkgDo(breqp);
	    osi_Free (breqLp, sizeof(*breqLp));
	    if (cm_bkgUrgent) goto urgents;
	}
	cm_LeaveBKG();
	if (!foundAny) {		/* wait for new request */
	    cm_bkgDaemons++;
	    osi_SleepW((opaque)(&cm_bkgDaemons), &cm_bkgLock);
	    lock_ObtainWrite(&cm_bkgLock);
	    cm_bkgDaemons--;
	}
    }	/* big 'for (;;)' loop */
}

/* routine to wait for code to become valid */
#ifdef SGIMIPS
long cm_bkgWait(struct cm_bkgrequest *areqp)
#else  /* SGIMIPS */
long cm_bkgWait(areqp)
  struct cm_bkgrequest *areqp;
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_bkgLock);
    while(!(areqp->states & BK_CODEVALID)) {
	areqp->states |= BK_CODEWAIT;
	osi_SleepW((char *)areqp, &cm_bkgLock);
	lock_ObtainWrite(&cm_bkgLock);
    }
    lock_ReleaseWrite(&cm_bkgLock);
    /* at this point, no one else is modifying code, so we can
     * just return it.
     */
    icl_Trace2(cm_iclSetp, CM_TRACE_BKGWAITED,
	       ICL_TYPE_POINTER, areqp,
	       ICL_TYPE_LONG, areqp->code);
    return areqp->code;
}

/* 
 * This routine enqueues a new background operation request; if no available 
 * bkg  buffers exist, it waits for one. If the bkg daemons are waiting for 
 * new requests it notifies them too at the end.
 *
 * Flag bit 1: don't wait; the default action is to wait for a free daemon.
 */
#ifdef SGIMIPS
struct cm_bkgrequest *cm_bkgQueue(
  register int opcode,
  register struct cm_scache *scp,
  long flags,
  osi_cred_t *credp,
  long parm0, long parm1, long parm2, long parm3, long parm4) 
#else  /* SGIMIPS */
struct cm_bkgrequest *cm_bkgQueue(opcode, scp, flags, credp,
				parm0, parm1, parm2, parm3, parm4)
  register int opcode;
  long flags;
  register struct cm_scache *scp;
  osi_cred_t *credp;
  long parm0, parm1, parm2, parm3, parm4; 
#endif /* SGIMIPS */
{
    register struct cm_bkgrequest *breqp;
    register int i;

    /* Hold scp.  Must do this now, before grabbing bkgLock, because
     * bkgLock is below scachelock in the hierarchy (otherwise we couldn't
     * call bkg_Queue from cm_inactive).
     */
    if (scp) {
	cm_HoldSCache(scp);
    }
    lock_ObtainWrite(&cm_bkgLock);

    /*
    Skip this optimization if called with NOWAITD flag
    set, e.g., from cm_getpage().
    */

    if((opcode == BK_FETCH_OP) && !(flags & BK_FLAG_NOWAITD))
    {
	/* don't put on queue if already there */

	breqp=already_qued(scp,parm0);

	if(breqp)
	{
            /* check if caller wants refcount raised */

            if(flags & BK_FLAG_NOREL)
                breqp->refCount++;

	    lock_ReleaseWrite(&cm_bkgLock);
	    if(scp)
		cm_PutSCache(scp);

	    return breqp;
	}
    }

    for (;;) {
	breqp = cm_bkgReqs;
	for (i = 0; i < BK_NBRS; i++, breqp++) {
	    if (breqp->refCount == 0) 
		break;
	}

	/*
	 * BK_FLAG_ALLOC and BK_FLAG_NOWAITD are assumed to be mutually
	 * exclusive.  The following code, and also some code in bkgDaemon,
	 * will not handle a request correctly if it has both flags set.
	 */

	/* If no request structures available and caller can't wait for one,
	 * allocate one.
	 */
	if (i >= BK_NBRS && (flags & BK_FLAG_ALLOC)) {
	    struct cm_bkgrequest_list *breqLp;

	    breqLp = (struct cm_bkgrequest_list *)
			osi_Alloc (sizeof (struct cm_bkgrequest_list));
	    breqp = &breqLp->r;
	    breqp->refCount = 0;
	    breqLp->next = cm_bkgrequest_list;
	    cm_bkgrequest_list = breqLp;
	}

	if (i < BK_NBRS || (flags & BK_FLAG_ALLOC)) {
	    /* 
	     * Found an empty buffer 
	     */
	    if (flags & BK_FLAG_NOWAITD) {
		if (cm_bkgUrgent >= cm_bkgDaemons) {
		    lock_ReleaseWrite(&cm_bkgLock);
		    if (scp)
			cm_PutSCache(scp);
		    return (struct cm_bkgrequest *) 0;
		}
		breqp->states = BK_URGENT;
		cm_bkgUrgent++;
	    } else
		breqp->states = 0;
	    breqp->opcode = opcode;
	    breqp->vnodep = scp;
	    breqp->credp = credp;
	    crhold(breqp->credp);
	    /* setup so daemon's release still leaves reference
	     * for caller to clean up, if requested.
	     */
	    breqp->refCount = ((flags & BK_FLAG_NOREL)? 2 : 1);
	    breqp->parm[0] = parm0;
	    breqp->parm[1] = parm1;
	    breqp->parm[2] = parm2;
	    breqp->parm[3] = parm3;
	    breqp->code = 0;
	    /* 
	     * If daemons are waiting for work, wake them up 
	     */
	    if (cm_bkgDaemons > 0) {
		osi_Wakeup(&cm_bkgDaemons);
	    }
	    icl_Trace3(cm_iclSetp, CM_TRACE_BKGQUEUED,
		       ICL_TYPE_POINTER, breqp,
		       ICL_TYPE_LONG, breqp->opcode,
		       ICL_TYPE_POINTER, breqp->vnodep);
	    lock_ReleaseWrite(&cm_bkgLock);
	    return breqp;
	}
	/* 
	 * No free buffers, quit or wait.
	 */
	if (!(flags & BK_FLAG_WAIT)) {
	    lock_ReleaseWrite(&cm_bkgLock);
	    if (scp)
		cm_PutSCache(scp);
	    return (struct cm_bkgrequest *) 0;
	}
	/* otherwise, we're sleeping */
	cm_bkgWaiters++;
	osi_SleepW((char *)&cm_bkgWaiters, &cm_bkgLock);
	lock_ObtainWrite(&cm_bkgLock);
    }	/* for(;;) loop */
}


/* 
 * Release a held request buffer.
 * Can only be used for buffers returned by calling cm_bkgQueue
 * with BK_FLAG_NOREL, otherwise we don't have a held reference
 * returned.
 */
#ifdef SGIMIPS
void cm_bkgRelease(register struct cm_bkgrequest *breqp) 
#else  /* SGIMIPS */
void cm_bkgRelease(breqp)
    register struct cm_bkgrequest *breqp; 
#endif /* SGIMIPS */
{
    lock_ObtainWrite(&cm_bkgLock);
    icl_Trace1(cm_iclSetp, CM_TRACE_BKGRELEASE,
	       ICL_TYPE_POINTER, breqp);
    if (--breqp->refCount <= 0) {
	breqp->states = 0;
	if (cm_bkgWaiters) {
	    cm_bkgWaiters = 0;
	    osi_Wakeup((char *) &cm_bkgWaiters);
	}
    }
    lock_ReleaseWrite(&cm_bkgLock);
}


/* 
 * Parm 0 is the pathname, parm 1 to the fetch is the chunk number 
 * parm 2 tells whether to follow symbolic links
 */
#ifdef SGIMIPS
static void bkg_Path(register struct cm_bkgrequest *breqp) 
#else  /* SGIMIPS */
static void bkg_Path(breqp)
    register struct cm_bkgrequest *breqp; 
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    struct vnode *vp;
    struct cm_rrequest rreq;
    struct cm_scache *scp;
    long code;

    cm_InitReq(&rreq, breqp->credp);
    cm_SetConnFlags(&rreq, CM_RREQFLAG_NOVOLWAIT);
    /*
     * XXX osi_lookupname is going to make RPC's, and those RPC's will have
     * their own rrequest structures that don't have NOVOLWAIT set. XXX
     */
    code = osi_lookupname((char *)breqp->parm[0], OSI_UIOSYS,
			  breqp->parm[2] ? FOLLOW_LINK : NO_FOLLOW,
			  (struct vnode **) 0, &vp);
    osi_FreeBufferSpace((struct osi_buffer *)(breqp->parm[0]));	
    if (code) 
	return;
    /* 
     * Now path may not have been in dfs, so check that before calling our cache mgr 
     */
    if (!osi_IsAfsVnode(vp)) {
	OSI_VN_RELE(vp);		/* release it and give up */
	return;
    }
#ifdef SGIMIPS
    VTOSC(vp, scp);
#else  /* SGIMIPS */
    scp = VTOSC(vp);
#endif /* SGIMIPS */
    /* 
     * Here we know it's a dfs vnode, so we can get the data for the chunk 
     */
    if (dcp = cm_GetDCache(scp, breqp->parm[1])) {
	lock_ObtainRead(&scp->llock);
	code = cm_GetDLock(scp, dcp, CM_DLOCK_FETCHOK, READ_LOCK, &rreq);
	if (code == 0)
	    lock_ReleaseRead(&dcp->llock);
	cm_PutDCache(dcp);
	lock_ReleaseRead(&scp->llock);
    }
    OSI_VN_RELE(vp);
}


/* 
 * Prefetch a data chunk: parm 0 to the fetch is the chunk number; parm 1 is
 * null or a dcache entry to wakeup.  Parm 2 is the type of GETDLOCK to do
 * (e.g. FETCHOK, READ or WRITE).
 */
#ifdef SGIMIPS
static int bkg_Prefetch(register struct cm_bkgrequest *breqp) 
#else  /* SGIMIPS */
static int bkg_Prefetch(breqp)
    register struct cm_bkgrequest *breqp; 
#endif /* SGIMIPS */
{
    register struct cm_dcache *dcp;
    struct cm_rrequest rreq;
    register long code;
    register struct cm_scache *scp;
    int type;

    cm_InitReq(&rreq, breqp->credp);
    cm_SetConnFlags(&rreq, CM_RREQFLAG_NOVOLWAIT);
    scp = (struct cm_scache *) breqp->vnodep;
    if (dcp = cm_GetDCache(breqp->vnodep, breqp->parm[0])) {
	type = (breqp->parm[2] == CM_DLOCK_FETCHOK ? READ_LOCK : WRITE_LOCK);
	lock_Obtain(&scp->llock, type);
#ifdef SGIMIPS
	code = cm_GetDLock(scp, dcp, (int)breqp->parm[2], type, &rreq);
#else  /* SGIMIPS */
	code = cm_GetDLock(scp, dcp, breqp->parm[2], type, &rreq);
#endif /* SGIMIPS */
	if (code == 0)
	    lock_Release(&dcp->llock, type);
	cm_PutDCache(dcp);
	lock_Release(&scp->llock, type);
    }
    else code = ENOENT;
    /* 
     * Now, dude may be waiting for us to clear DC_DFFETCHREQ bit.
     */
    dcp = (struct cm_dcache *) (breqp->parm[1]);
    if (dcp) {
	/* 
	 * Can't use dcp from GetDCache since cm_GetDCache may fail, but someone
	 * may be waiting for our wakeup anyway 
	 */
	lock_ObtainWrite(&dcp->llock);
	dcp->states &= ~DC_DFFETCHREQ;
	lock_ReleaseWrite(&dcp->llock);
	osi_Wakeup(&dcp->states);
    }
    return cm_CheckError(code, &rreq);
}


#ifdef SGIMIPS
static void bkg_Store(register struct cm_bkgrequest *breqp)
#else  /* SGIMIPS */
static void bkg_Store(breqp)
register struct cm_bkgrequest *breqp;
#endif /* SGIMIPS */
{
    struct cm_rrequest rreq;
    register struct cm_scache *scp;
    register struct cm_dcache *dcp;
    long offset;

    cm_InitReq(&rreq, breqp->credp);
    cm_SetConnFlags(&rreq, CM_RREQFLAG_NOVOLWAIT);
    scp = breqp->vnodep;
    offset = breqp->parm[0];

    dcp = cm_FindDCache(scp, offset);
    /* if we find the chunk, we're fine.  If not, that's fine,
     * too, since then we don't have anything to write.
     */
    if (dcp) {
	(void) cm_StoreDCache(scp, dcp, CM_STORED_ASYNC, &rreq);
	cm_PutDCache(dcp);
    }
}


/* Parameter 0 is the fid of the vnode whose TKN_OPEN_PRESERVE token is
 * being returned.
 *
 * It would be nice if we could just pass a pointer to the vnode in the
 * vnodep field of the bkg request, but this would cause deadlock.
 * bkg_Queue grabs the global scache lock in order to bump that vnode's
 * reference count, but bkg_Queue would in this case be called from
 * cm_inactive, which is often called from cm_PutSCache, which itself
 * is holding the global scache lock.
 *
 * As with other bkg handlers, we would like to avoid retry loops when we
 * make RPC's.  We call cm_ReturnOpenToken, which calls
 * cm_FlushQueuedServerTokens, which makes RPC's.  That function is already
 * pretty careful to avoid this kind of problem, so we did not add any extra
 * code to tell it that it is called from a bkg handler.
 */
#ifdef SGIMIPS
static void bkg_ReturnToken(register struct cm_bkgrequest *breqp)
#else  /* SGIMIPS */
static void bkg_ReturnToken(breqp)
    register struct cm_bkgrequest *breqp;
#endif /* SGIMIPS */
{
    afsFid *fidp;
    register struct cm_scache *scp;
    afs_hyper_t type;

    fidp = (afsFid *)(breqp->parm[0]);
    lock_ObtainWrite(&cm_scachelock);
    scp = cm_FindSCache(fidp);
    lock_ReleaseWrite(&cm_scachelock);
    osi_Free (fidp, sizeof(*fidp));
    if (!scp) return;

    AFS_hzero(type);
    lock_ObtainWrite(&scp->llock);
    if (scp->states & SC_RETURNREF)
	AFS_HOP32(type, |, TKN_OPEN_PRESERVE);
#ifdef AFS_SUNOS5_ENV
    if (scp->states & SC_RETURNOPEN) {
	AFS_HOP32(type, |, (TKN_OPEN_READ | TKN_OPEN_WRITE |
			    TKN_OPEN_SHARED | TKN_OPEN_EXCLUSIVE));
	scp->states &= ~SC_RETURNOPEN;
    }
#endif /* AFS_SUNOS5_ENV */
    lock_ReleaseWrite(&scp->llock);

    if (!AFS_hiszero(type))
	cm_ReturnOpenTokens(scp, &type);

    cm_PutSCache(scp);
}
