/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: fshs_princ.c,v 65.6 1998/04/01 14:15:56 gwehrman Exp $";
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
#include <dcedfs/sysincludes.h>
#include <dcedfs/debug.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>
#include <dcedfs/queue.h>
#include <dcedfs/xcred.h>
#include <fshs.h>
#include <fshs_trace.h>
#include <dce/ker/pthread.h>
#ifdef SGIMIPS
#include <dce/sec_cred.h>
#endif

RCSID ("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/fshost/RCS/fshs_princ.c,v 65.6 1998/04/01 14:15:56 gwehrman Exp $")

/* 
 * Declaration of all principal-related (xx static) variables 
 */
struct lock_data fshs_princlock;	/* allocation lock for principals */
struct fshs_principal *fshs_PrincFreeListp = 0;	/*first free principal entry */
static int fshs_GetStalePrincipals _TAKES((int));
extern sec_id_pa_t unauthPA;

static unsigned32 dfs_rock_key;

static uuid_t unauthSessionID;
typedef struct sec_rock {
    struct sec_rock *next;
    uuid_t uuid;
} sec_rock_t;
 
static sec_rock_t *secRockList = NULL;
static pthread_mutex_t secRockMutex;

#ifdef SGIMIPS
int fshs_GetPrincipal(
	struct context *cookie,
	struct fshs_principal **princPtr,
	int authNeeded);
sec_rock_t *fshs_get_sec_rock(void);
void fshs_InitPrincMod(void);
static struct fshs_principal *fshs_AllocPrincipal(void);
int fshs_FreePrincipal(register struct fshs_principal *princp);
struct fshs_principal *fshs_FindPrincipal(
	 struct context *cookie,
	 struct fshs_host *hostp,
	 pac_list_t **pacListPP);
int fshs_CheckAuthn(struct context *cookie, struct fshs_host *hostp);
void fshs_PutPrincipal(register struct fshs_principal *princp);
int fshs_GCPrinc(register struct fshs_host *hostp, int *nump);
static int fshs_GetStalePrincipals(int number);
#endif

/*
 * Note: This is called from security. osi preemption may or
 * may not be disabled.  That is why pthread mutexes are used.
 * They do not require osi_PreemptionOff().
 */

void fshs_put_sec_rock(void *p)
{
    sec_rock_t *rockp = (sec_rock_t *)p;

    osi_assert(rockp->next == NULL);
    bzero((char *)&rockp->uuid, sizeof(rockp->uuid));

    pthread_mutex_lock(&secRockMutex);
    rockp->next = secRockList;
    secRockList = rockp;
    pthread_mutex_unlock(&secRockMutex);
}

/*
 * Called by fshost to alloc a rock to give to security.
 */
#define ROCKS_PER_ALLOC 400
sec_rock_t *fshs_get_sec_rock()
{
    sec_rock_t *rockp;

    pthread_mutex_lock(&secRockMutex);
    if (rockp = secRockList)
	secRockList = rockp->next;
    pthread_mutex_unlock(&secRockMutex);
    
    if (!rockp) { /* no rocks on list, alloc some giving one to caller */
	int count = ROCKS_PER_ALLOC;

	rockp = (sec_rock_t *)osi_Alloc(count * sizeof(sec_rock_t));
	bzero((char *)rockp, count * sizeof(sec_rock_t));

	pthread_mutex_lock(&secRockMutex);
	while(--count) {
	    rockp->next = secRockList;
	    secRockList = rockp;
	    rockp++;
        }
	pthread_mutex_unlock(&secRockMutex);
    }
    rockp->next = NULL;
    return rockp;
}

/* 
 * fshs_InitPrincMod -- initialize the fshs_princ structures.
 */
#ifdef SGIMIPS
void fshs_InitPrincMod(void)
#else
void fshs_InitPrincMod()
#endif  /* SGIMIPS */
{
    uuid_t  dfs_uuid;
    error_status_t st;

    lock_Init(&fshs_princlock);
    uuid_create(&dfs_uuid, &st);
    osi_assert(st == error_status_ok);
    pthread_mutex_init (&secRockMutex, pthread_mutexattr_default);
    osi_assert(st == error_status_ok);
    uuid_create(&unauthSessionID, &st);
    osi_assert(st == error_status_ok);
    dfs_rock_key = sec_cred_get_key(&dfs_uuid, fshs_put_sec_rock, &st);
    osi_assert(st == error_status_ok);
}

/*
 * Get the next principal entry from the Principal free linked list; if none is
 * available allocate a bunch of them (FSHS_PRINCSPERBLOCK many) and add them
 * to the free linked list. If the maximum principal allocation quota has
 * reached, try first freeing stale principals (by calling fshs_CheckHost on
 * all hosts) and if that fails free FSHS_MINLRUPRINCS from the fshs_plru list
 * Note that if none can be freed from there we still allocate a group.
 *
 * Locks: must be called with WRITE fshs_princlock
 */
static struct fshs_principal *fshs_AllocPrincipal(void)
{
    register struct fshs_principal *princp;
    register int i;

    icl_Trace0(fshs_iclSetp, FSHS_ALLOCPRINC1);

    if (fshs_PrincFreeListp == 0) {
	if (fshs_stats.allocPrincipalBlocks >= FSHS_MAXPRINCBLOCKS) {
	    /* 
	     * CheckHost and Enumerate will GC any stale host and/or principal
	     * entries, so we might get some free principal entries.
	     */
	    lock_ReleaseWrite(&fshs_princlock); 
	    i = fshs_Enumerate(fshs_CheckHost, (char *)0);
	    if (!fshs_PrincFreeListp) {/* refernece it w/o a lock */
		fshs_GetStalePrincipals(FSHS_MINLRUPRINCS);
	    }
	    lock_ObtainWrite(&fshs_princlock);
	}
	if (!fshs_PrincFreeListp) {
	    princp = (struct fshs_principal *) 
		osi_Alloc(FSHS_PRINCSPERBLOCK * sizeof(*princp));
	    fshs_PrincFreeListp = &princp[0];
	    for (i=0; i < (FSHS_PRINCSPERBLOCK -1); i++)
		princp[i].next = &princp[i+1];
	    princp[FSHS_PRINCSPERBLOCK-1].next = NULL;
	    fshs_stats.allocPrincipalBlocks++;
	}
    }
    princp = fshs_PrincFreeListp;
    fshs_PrincFreeListp = princp->next;
    fshs_stats.usedPrincipals++;
    bzero((char *)princp, sizeof(struct fshs_principal));
    icl_Trace0(fshs_iclSetp, FSHS_ALLOCPRINC2);
    return(princp);
}

/* 
 * Free the principal entry and put it back in the fshs_PrincFreeListp.
 * Note: This routine must be called with WRITE fshs_princlock
 */
#ifdef SGIMIPS
int fshs_FreePrincipal(
    register struct fshs_principal *princp)
#else
int fshs_FreePrincipal(princp)
    register struct fshs_principal *princp;
#endif /* SGIMIPS */
{
    osi_cred_t *credp;
    register long code = 0;

    icl_Trace1(fshs_iclSetp, FSHS_FREEPRINC, ICL_TYPE_POINTER, princp);
    credp = xcred_XCredToUCred(princp->xcredp);
    osi_assert(credp != NULL);
    crfree(credp);
    xcred_Delete(princp->xcredp);
    xcred_Release(princp->xcredp);
    if (princp->delegationChain)
        fshs_FreeDelegationChain(&(princp->delegationChain));
    princp->next = fshs_PrincFreeListp;
    fshs_PrincFreeListp = princp;
    fshs_stats.usedPrincipals--;
#ifdef SGIMIPS
    return 0;
#endif
}
/* 
 * Locate the desired principal. 
 * If it matches caller's {userID, cellID, groupid, etc.}, return a held princ.
 * Called with, at least, a READ host lock
 */
struct fshs_principal *fshs_FindPrincipal(cookie, hostp, pacListPP) 
     struct context *cookie;
     struct fshs_host *hostp;
     pac_list_t **pacListPP;
{
    register struct fshs_principal *princp;
    unsigned32 st;
    sec_id_pa_t *s1, *s2;
    unsigned16 i;
    uuid_t *session_uuidP = NULL;
    pac_list_t *cred_paclistP, *princ_paclistP;
    static int rock_hit = 0, rock_miss = 0, repop = 0, cache_empty = 0;
    sec_rock_t *rockp;

    icl_Trace0(fshs_iclSetp, FSHS_FINDPRINC);

    princp = NULL;
    /*
     * Try to find the principal quickly using the session uuid.
     */
    st = error_status_ok;
    session_uuidP = &unauthSessionID; /* in case no auth */
    if (cookie->isAuthn) {
    	sec_cred_get_app_private_data(cookie->cred_ptr,
	    dfs_rock_key, (void **)&rockp, &st);
	if (st == error_status_ok) {
	    session_uuidP = &rockp->uuid;
	} else {
	    session_uuidP = NULL;
	    if (st == sec_cred_s_key_not_found) {
		cache_empty++;
		st = error_status_ok;
	    }
	}
    } 
    if (session_uuidP) { /* Fast path.  Search based on session uuid */
	rock_miss++;
	for (princp = hostp->princListp; princp; princp = princp->next) {
	    lock_ObtainWrite(&princp->lock);
	    if (uuid_equal(&princp->session_uuid, session_uuidP, &st)) {
		princp->refCount++;
		icl_Trace2(fshs_iclSetp, FSHS_FOUNDPRINC1,
		    ICL_TYPE_POINTER, princp, ICL_TYPE_LONG, princp->refCount);
		lock_ReleaseWrite(&princp->lock);
		rock_miss--;
		rock_hit++;
		break;	/* out of for */
	    }
	    lock_ReleaseWrite(&princp->lock);
	}
    }
    if ((princp == NULL) && (st == error_status_ok)) {
	/*
	 * Slow path, no session id found.  Search based on credentials.
	 */

        if (*pacListPP == NULL) {
		fshs_SetupDelegationChain(cookie->cred_ptr,
			cookie->isAuthn, &unauthPA, pacListPP, &st);
		osi_assert(st == error_status_ok);
	}

	for (princp = hostp->princListp; princp; princp = princp->next) {
	    lock_ObtainWrite(&princp->lock);

	    if (princp->states & FSHS_PRINC_STALE)
		goto mismatch;

	    cred_paclistP = *pacListPP;
	    princ_paclistP = princp->delegationChain;
	    while (cred_paclistP) {
	        u_int j;

	    	s1 = cred_paclistP->pacp;
	        s2 = princ_paclistP->pacp;
		if ((!uuid_equal(&s1->principal.uuid,
				 &s2->principal.uuid, &st)) ||
		    (!uuid_equal(&s1->group.uuid, &s2->group.uuid, &st)) ||
		    (!uuid_equal(&s1->realm.uuid, &s2->realm.uuid, &st)) ||
		    (s1->num_foreign_groupsets != s2->num_foreign_groupsets) ||
		    (s1->num_groups != s2->num_groups))
		    goto mismatch;

	    	for (i = 0; i < s1->num_groups; i++)
		    if (!uuid_equal(&s1->groups[i].uuid, &s2->groups[i].uuid, 
				    &st))
			goto mismatch;

		for (j = 0; j < s1->num_foreign_groupsets; j++) {
		    if ((s1->foreign_groupsets[j].num_groups !=
			 s2->foreign_groupsets[j].num_groups) ||
			(!uuid_equal(&(s1->foreign_groupsets[j].realm.uuid),
				     &(s2->foreign_groupsets[j].realm.uuid),
				     &st)))
			goto mismatch;
		    for (i = 0; i < s1->foreign_groupsets[j].num_groups; i++)
			if (!uuid_equal(&s1->foreign_groupsets[j].
					groups[i].uuid,
					&s2->foreign_groupsets[j].
					groups[i].uuid, &st)) 
			    goto mismatch;
		}
		cred_paclistP = cred_paclistP->next;
		princ_paclistP = princ_paclistP->next;
		if ((cred_paclistP && !princ_paclistP) || 
		    (!cred_paclistP && princ_paclistP))
		    goto mismatch;
	    }

	    princp->refCount++;
	    icl_Trace2(fshs_iclSetp, FSHS_FOUNDPRINC,ICL_TYPE_POINTER, princp,
		   ICL_TYPE_LONG, princp->refCount);
	    break; /* found */

mismatch:
	    lock_ReleaseWrite(&princp->lock);

	}
	if (princp) {
	    if (cookie->isAuthn) {
		rockp = fshs_get_sec_rock();
		rockp->uuid = princp->session_uuid;
		osi_RestorePreemption(0);
		sec_cred_set_app_private_data(cookie->cred_ptr,
		    dfs_rock_key, rockp, &st);
		(void)osi_PreemptionOff();
		osi_assert(st == error_status_ok);
		repop++;
	    }
	    lock_ReleaseWrite(&princp->lock);
	}
    } 

    icl_Trace4(fshs_iclSetp, FSHS_ROCKSTATS, ICL_TYPE_LONG, rock_hit,
		ICL_TYPE_LONG, rock_miss, ICL_TYPE_LONG, cache_empty, 
		ICL_TYPE_LONG, repop);
    return princp;
}

#ifdef SGIMIPS
int fshs_CheckAuthn(
    struct context *cookie,
    struct fshs_host *hostp)
#else
int fshs_CheckAuthn(cookie, hostp)
    struct context *cookie;
    struct fshs_host *hostp;
#endif /* SGIMIPS */
{
    struct fshs_security_bounds secb;
    int code = 0;

    lock_ObtainRead(&fshost_SecurityLock);
    if (hostp->flags & FSHS_HOST_LOCALCELL)
	secb = fshost_Security.local;
    else
	secb = fshost_Security.nonLocal;
    lock_ReleaseRead(&fshost_SecurityLock);

    if (secb.minProtectLevel > cookie->authnLevel)
	code = FSHS_ERR_AUTHNLEVEL_S_TOO_LOW;
    else if (secb.maxProtectLevel < cookie->authnLevel)
	code = FSHS_ERR_AUTHNLEVEL_S_TOO_HIGH;
    else
	code = 0;

    icl_Trace3(fshs_iclSetp, FSHS_CHECKAUTHN1,
	       ICL_TYPE_POINTER, cookie,
	       ICL_TYPE_POINTER, hostp,
	       ICL_TYPE_LONG, hostp->flags);
    icl_Trace4(fshs_iclSetp, FSHS_CHECKAUTHN2,
	       ICL_TYPE_LONG, secb.minProtectLevel,
	       ICL_TYPE_LONG, secb.maxProtectLevel,
	       ICL_TYPE_LONG, cookie->authnLevel,
	       ICL_TYPE_LONG, code);
    return code;
}


/* 
 * Return (held) the principal entry(in 'principal') associated with the rpc
 * binding. If it is not found, the "context" of the caller must be invalid.
 * The caller has to re-establish the context through AFS_SetContext().
 */
#ifdef SGIMIPS
int fshs_GetPrincipal(
    struct context *cookie,
    struct fshs_principal **princPtr,
    int authNeeded)
#else
int fshs_GetPrincipal(cookie, princPtr, authNeeded)
    struct context *cookie;
    struct fshs_principal **princPtr;
    int authNeeded;
#endif
{
    register struct fshs_host *hostp = 0;
    register struct fshs_principal *princp;
    error_status_t st;
    int code = 0;
    pac_list_t *pacListP = NULL;

    icl_Trace0(fshs_iclSetp, FSHS_GETAPRINC1);
    hostp = fshs_GetHost(cookie);
    if (hostp == NULL) {
	if (cookie->initContext) 
	    return FSHS_ERR_FATALCONN; 
	else
	    return FSHS_ERR_STALEHOST;
    }
    lock_ObtainRead(&hostp->h.lock);
    princp = fshs_FindPrincipal(cookie, hostp, &pacListP);
    lock_ReleaseRead(&hostp->h.lock);
    
    if (princp) {  /* Fast Path */
	lock_ObtainWrite(&princp->lock);
	princp->lastCall = osi_Time();
	icl_Trace2(fshs_iclSetp, FSHS_EXISTPRINC, ICL_TYPE_POINTER, princp,
		   ICL_TYPE_LONG, princp->refCount);
	lock_ReleaseWrite(&princp->lock);
	if (authNeeded) {
	    code = fshs_CheckAuthn(cookie, hostp);
	    if (code) {
		fshs_PutPrincipal(princp);
		goto cleanup;
	    }
	}
    } 
    else { /* Slow Path */
	/*
	 * Establish a new data structure for the caller. Either the principal 
	 * representing the caller was GCed or it is a brand new caller.
	 */
	lock_ObtainWrite(&hostp->h.lock);
	/* 
	 * find the princ one more time in case there is a race condition.
	 */
	if (princp = fshs_FindPrincipal(cookie, hostp, &pacListP)) {
	    if (authNeeded) {
		code = fshs_CheckAuthn(cookie, hostp);
		if (code) {
		    lock_ReleaseWrite(&hostp->h.lock);
		    fshs_PutPrincipal(princp);
		    goto cleanup;
		}
	    }
	    lock_ObtainWrite(&princp->lock);
	    princp->lastCall = osi_Time();
	    lock_ReleaseWrite(&princp->lock);
	}
	else {
	    uuid_t *ses_uuidP;

	    lock_ObtainWrite(&fshs_princlock);
	    princp = fshs_AllocPrincipal();
	    princp->hostp = hostp;
	    princp->states = 0; 
	    princp->refCount = 1;
	    princp->lastCall = osi_Time();
	    if (pacListP == NULL) {
		fshs_SetupDelegationChain(cookie->cred_ptr, cookie->isAuthn,
					  &unauthPA, &(princp->delegationChain), &st);
		if (st != error_status_ok) {
		    code =  FSHS_ERR_FATALCONN;
		    goto cleanup;
		}
	    } else
		princp->delegationChain = pacListP;
	    if (fshs_GetPAC(cookie, princp)) {
		/* This should not occur unless a system bug. */
		osi_assert(princp->refCount == 1);
		princp->states |= FSHS_PRINC_STALE;
		hostp->h.refCount--;		/* forget this host */
		lock_ReleaseWrite(&fshs_princlock);	   
		lock_ReleaseWrite(&hostp->h.lock);
		printf("fx: GetPAC failed\n");   /* panic ? */
		code =  FSHS_ERR_FATALCONN;
		goto cleanup;
	    }
	    if (authNeeded) {
		code = fshs_CheckAuthn(cookie, hostp);
		if (code) {
		    hostp->h.refCount--;		/* forget this host */
		    fshs_FreePrincipal(princp);
		    lock_ReleaseWrite(&fshs_princlock);	   
		    lock_ReleaseWrite(&hostp->h.lock);
		    princp = NULL;
		    pacListP = NULL;
		    goto cleanup;
		}
	    }
	    /*
	     * Set up the session id and tie it to the EPAC data.
	     * This allows fast matching of principals to cookie cred_ptrs
	     */
	    if (cookie->isAuthn) {
		error_status_t st;
		sec_rock_t *rp;

		uuid_create(&princp->session_uuid, &st);
		osi_assert(st == error_status_ok);
		rp = fshs_get_sec_rock();
		rp->uuid = princp->session_uuid;
		osi_RestorePreemption(0);
		sec_cred_set_app_private_data(cookie->cred_ptr,
					      dfs_rock_key, rp, &st);
		(void)osi_PreemptionOff();
		osi_assert(st == error_status_ok);
	    } else { /* use the unauthSessionID */
		princp->session_uuid = unauthSessionID;
	    }
	    princp->next = hostp->princListp;
	    hostp->princListp = princp;
	    lock_ReleaseWrite(&fshs_princlock);
	    icl_Trace1(fshs_iclSetp, FSHS_ASSIGNPRINC, ICL_TYPE_POINTER, princp);
	}
	lock_ReleaseWrite(&hostp->h.lock);
    }
    *princPtr = princp;
    icl_Trace2(fshs_iclSetp, FSHS_GETAPRINC2, ICL_TYPE_POINTER, princp,
	       ICL_TYPE_LONG, princp->refCount);

cleanup:
    if (pacListP) {
	if (princp) {
	    lock_ObtainRead(&princp->lock);
	    if (pacListP != princp->delegationChain)
		fshs_FreeDelegationChain(&pacListP);
	    lock_ReleaseRead(&princp->lock);
	} else
	    fshs_FreeDelegationChain(&pacListP);
    }
    return code;
}

/* 
 * Decrement the reference counts associated with the principal entry AND its 
 * parent host entry; note that on last reference all stale clients and host 
 * associated with princp->hostp will be garbage collected.
 */
#ifdef SGIMIPS
void fshs_PutPrincipal(
    register struct fshs_principal *princp)
#else
void fshs_PutPrincipal(princp)
    register struct fshs_principal *princp;
#endif /* SGIMIPS */
{
    struct fshs_host *hostp;
    icl_Trace2(fshs_iclSetp, FSHS_PUTPRINC, ICL_TYPE_POINTER, princp,
	       ICL_TYPE_LONG, princp->refCount);
    lock_ObtainWrite(&princp->lock);
    princp->refCount--;
    hostp = princp->hostp;
    lock_ReleaseWrite(&princp->lock);
    fshs_PutHost(hostp);
}
/*
 * GC stale principals of a given host.
 * Must be called a WRITE host lock
 */
#ifdef SGIMIPS
int fshs_GCPrinc(
     register struct fshs_host *hostp,
     int *nump)
#else
int fshs_GCPrinc(hostp, nump)
     register struct fshs_host *hostp;
     int *nump;
#endif /* SGIMIPS */
{
    register struct fshs_principal **cp, *princp, *tprincp;

    if (hostp->flags & FSHS_HOST_STALE) 
#ifdef SGIMIPS
	return -1;
#else
        return;
#endif

    icl_Trace1(fshs_iclSetp, FSHS_GCPRINC, ICL_TYPE_POINTER, hostp);
    lock_ObtainWrite(&fshs_princlock);
    for (cp = &hostp->princListp; princp = *cp;) {
        lock_ObtainRead(&princp->lock);
        if (princp->states & FSHS_PRINC_STALE) {
	    icl_Trace1(fshs_iclSetp, FSHS_FOUNDSTALEPRINC, ICL_TYPE_POINTER, 
		       princp);
	    *cp = princp->next;
	    lock_ReleaseRead(&princp->lock);
	    fshs_FreePrincipal(princp);	/* gc this one */
	    *nump++; 
        } else {
            cp = &princp->next;
	    lock_ReleaseRead(&princp->lock);
        }
    }
    lock_ReleaseWrite(&fshs_princlock);
    return 0;
}

/* 
 * This routine is responsible for moving up to 'number' STALE principal 
 * entries from active hosts to the principal free linked list. 
 * This routine must be called with NO lock.
 */
#ifdef SGIMIPS
static int fshs_GetStalePrincipals(int number)
#else
static fshs_GetStalePrincipals(number)
    int number;
#endif
{
    register struct squeue *tq;
    register struct fshs_host *hostp;
    int num;

    icl_Trace0(fshs_iclSetp, FSHS_GETSTALECPRINC1);
    lock_ObtainRead(&fshs_hostlock);
    for (tq = fshs_fifo.prev; tq != &fshs_fifo && number > 0; tq = QPrev(tq)) {
        hostp = FSHS_QTOH(tq);
        lock_ObtainWrite(&hostp->h.lock);
        if (!(hostp->flags & FSHS_HOST_STALE)) {
	     num = 0;
	     fshs_GCPrinc(hostp, &num);
	     lock_ReleaseWrite(&hostp->h.lock);
	     number -= num;
	}
        else
            lock_ReleaseWrite(&hostp->h.lock);
    }
    lock_ReleaseRead(&fshs_hostlock);
    icl_Trace0(fshs_iclSetp, FSHS_GETSTALECPRINC2);
#ifdef SGIMIPS
    return 0;
#else
    return;
#endif
}
