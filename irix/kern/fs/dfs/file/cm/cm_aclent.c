/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_aclent.c,v 65.6 1998/03/23 16:24:03 gwehrman Exp $";
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
 *      Copyright (C) 1994, 1990 Transarc Corporation
 *      All rights reserved.
 */
#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/queue.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_conn.h>
#include <cm_aclent.h>

#ifdef SGIMIPS
time_t cm_TGTLifeTime(unsigned32, int *);               /* for compiler */
#endif /* SGIMIPS */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_aclent.c,v 65.6 1998/03/23 16:24:03 gwehrman Exp $")

/* 
 * This next lock controls access to all cm_aclent structures in the system,
 * in either the free list or in the LRU queue.  A read lock prevents someone
 * from modifying the list(s), and a write lock is required for modifying
 * the list.  The actual data stored in the randomUid and randomAccess fields
 * is actually maintained as up-to-date or not via the scache llock.
 * An aclent structure is free if it has no back vnode pointer.
 */
static struct lock_data cm_acllock;		/* lock for system's aclents */
static struct squeue cm_aclLRU;		/* LRUQ for dudes in vnodes' lists */


/* 
 * Get an acl cache entry, or return that it doesn't exist 
 */
#ifdef SGIMIPS
long cm_GetACLCache(
    struct cm_scache *scp,
    struct cm_rrequest *rreqp,
    long *rightsp) 
#else  /* SGIMIPS */
long cm_GetACLCache(scp, rreqp, rightsp)
    struct cm_scache *scp;
    struct cm_rrequest *rreqp;
    long *rightsp; 
#endif /* SGIMIPS */
{
    register struct cm_aclent *aclp;
    unsigned32 pag;

#ifdef SGIMIPS
    pag = (unsigned32) rreqp->pag;
#else  /* SGIMIPS */
    pag = rreqp->pag;
#endif /* SGIMIPS */
#ifndef CM_DISABLE_ROOT_AS_SELF
    /* If no PAG at all, but it's local root, use the ...../self identity. */
    if (rreqp->pag == rreqp->uid && rreqp->uid == 0) {
	    /* this is root, use self identity */
	    pag = ~0;
    }
#endif /* !CM_DISABLE_ROOT_AS_SELF */
    icl_Trace2(cm_iclSetp, CM_TRACE_SEARCHACLCACHE, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, pag);
    lock_ObtainWrite(&cm_acllock);
    for (aclp = scp->randomACL; aclp; aclp = aclp->next) {
	if (aclp->randomUid == pag) {
	    if (aclp->tgtLifetime && aclp->tgtLifetime <= osi_Time()) {
	        /* ticket expired */
	        aclp->tgtLifetime = 0;
	        *rightsp = 0;
		break; /* get a new acl from server */
	    }
	    else {
	        *rightsp = aclp->randomAccess;
		icl_Trace1(cm_iclSetp, CM_TRACE_FOUNDACLCACHE, ICL_TYPE_LONG,
			   aclp->randomAccess);
		QRemove(&aclp->lruq);
		QAdd(&cm_aclLRU, &aclp->lruq);	/* add to the head of queue */
	    }
	    lock_ReleaseWrite(&cm_acllock);
	    return 0;
	}
    }
    /* 
     * If we make it here, this entry isn't present, so we're going to fail. 
     */
    lock_ReleaseWrite(&cm_acllock);
    return -1;
}


/* 
 * This function returns a free (not in the LRU queue) acl cache entry.
 * It must be called with the cm_acllock lock held.
 */
#ifdef SGIMIPS
static struct cm_aclent *GetFreeACLEnt(void) 
#else  /* SGIMIPS */
static struct cm_aclent *GetFreeACLEnt() 
#endif /* SGIMIPS */
{
    register struct cm_aclent *aclp, *taclp, **laclpp;

    if (cm_aclLRU.prev == &cm_aclLRU) 
	panic("empty aclent LRU");

    aclp = (struct cm_aclent *) QPrev(&cm_aclLRU);
    QRemove(&aclp->lruq);
    if (aclp->back) {
	/* 
	 * Remove the entry from the vnode's list 
	 */
	laclpp = &aclp->back->randomACL;
	for (taclp = *laclpp; taclp; laclpp = &taclp->next, taclp = *laclpp) {
	    if (taclp == aclp) 
		break;
	}
	if (!taclp) 
	    panic("GetFreeACLEnt race");
	*laclpp = aclp->next;			/* remove from vnode list */
	aclp->back = (struct cm_scache *) 0;
	icl_Trace1(cm_iclSetp, CM_TRACE_ACLRECYCLE,
		   ICL_TYPE_POINTER, aclp->back);
    }
    return aclp;
}

/* 
 * Add rights to an acl cache entry.  Do the right thing if not present, 
 * including digging up an entry from the LRU queue.  I'd expect that the scp 
 * is locked whethis function is called.
 */
#ifdef SGIMIPS
void cm_AddACLCache(
    struct cm_scache *scp,
    struct cm_rrequest *rreqp,
    long rights) 
#else  /* SGIMIPS */
void cm_AddACLCache(scp, rreqp, rights)
    struct cm_scache *scp;
    struct cm_rrequest *rreqp;
    long rights; 
#endif /* SGIMIPS */
{
    register struct cm_aclent *aclp;
    int isExpired;
    unsigned32 pag;

#ifdef SGIMIPS
    pag = (unsigned32) rreqp->pag;
#else  /* SGIMIPS */
    pag = rreqp->pag;
#endif /* SGIMIPS */
#ifndef CM_DISABLE_ROOT_AS_SELF
    /* If no PAG at all, but it's local root, use the ...../self identity. */
    if (rreqp->pag == rreqp->uid && rreqp->uid == 0) {
	    /* this is root, use self identity */
	    pag = ~0;
    }
#endif /* !CM_DISABLE_ROOT_AS_SELF */
    icl_Trace3(cm_iclSetp, CM_TRACE_ADDACL, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, pag, ICL_TYPE_LONG, rights);

    lock_ObtainWrite(&cm_acllock);
    for (aclp = scp->randomACL; aclp; aclp = aclp->next) {
	if (aclp->randomUid == pag) {
	    aclp->randomAccess = rights;
	    if (aclp->tgtLifetime == 0) 
	        aclp->tgtLifetime = cm_TGTLifeTime(pag, &isExpired);
	    lock_ReleaseWrite(&cm_acllock);
	    return;
	}
    }
    /* 
     * Didn't find the dude we're looking for, so take someone from the LRUQ 
     * and  reuse. But first try the free list and see if there's already 
     * someone there.
     */
    aclp = GetFreeACLEnt();		 /* can't fail, panics instead */
    QAdd(&cm_aclLRU, &aclp->lruq);
    aclp->back = scp;
    aclp->next = scp->randomACL;
    scp->randomACL = aclp;
    aclp->randomUid = pag;
    aclp->randomAccess = rights;
    aclp->tgtLifetime = cm_TGTLifeTime(pag, &isExpired);
    lock_ReleaseWrite(&cm_acllock);
    return;
}

/* 
 * Initialize the cache to have an entries 
 */
#ifdef SGIMIPS
void cm_InitACLCache(long size)
#else  /* SGIMIPS */
void cm_InitACLCache(size)
    long size; 
#endif /* SGIMIPS */
{
    struct cm_aclent *aclp;
    long i;
    static initted = 0;

    if (!initted) {
	initted = 1;
	lock_Init(&cm_acllock);
    }
    lock_ObtainWrite(&cm_acllock);
    QInit(&cm_aclLRU);
    aclp = (struct cm_aclent *) osi_Alloc(size * sizeof(struct cm_aclent));
    bzero((char *) aclp, size * sizeof(struct cm_aclent));
    /* 
     * Put all of these guys on the LRU queue 
     */
    for (i = 0; i < size; i++) {
	QAdd(&cm_aclLRU, &aclp->lruq);
	aclp++;
    }
    lock_ReleaseWrite(&cm_acllock);
}


/* 
 * Free all associated acl entries.  We actually just clear the back pointer
 * since the acl entries are already in the free list.
 */
#ifdef SGIMIPS
void cm_FreeAllACLEnts( register struct cm_scache *scp )
#else  /* SGIMIPS */
void cm_FreeAllACLEnts(scp)
    register struct cm_scache *scp; 
#endif /* SGIMIPS */
{
    register struct cm_aclent *aclp, *taclp;

    icl_Trace1(cm_iclSetp, CM_TRACE_FREEALLACLS, ICL_TYPE_POINTER, scp);
    lock_ObtainWrite(&cm_acllock);
    for (aclp = scp->randomACL; aclp; aclp = taclp) {
	taclp = aclp->next;	
	aclp->back = (struct cm_scache *) 0;
    }
    scp->randomACL = (struct cm_aclent *) 0;
    scp->anyAccess = 0;		/* reset this, too */
    lock_ReleaseWrite(&cm_acllock);
}


/* 
 * Invalidate all ACL entries for particular user on this particular vnode 
 */
#ifdef SGIMIPS
void cm_InvalidateACLUser(
    register struct cm_scache *scp,
    long pag)
#else  /* SGIMIPS */
void cm_InvalidateACLUser(scp, pag)
    register struct cm_scache *scp;
    long pag;
#endif /* SGIMIPS */
 {
    register struct cm_aclent *aclp, **laclpp;

    icl_Trace2(cm_iclSetp, CM_TRACE_INVALIDATEACL, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, pag);

    lock_ObtainWrite(&cm_acllock);
    laclpp = &scp->randomACL;
    for (aclp = *laclpp; aclp; laclpp = &aclp->next, aclp = *laclpp) {
	if (pag == aclp->randomUid) {	/* One for a given user/scache */
	    *laclpp = aclp->next;
	    aclp->back = (struct cm_scache *) 0;
	    break;
	}
    }
    lock_ReleaseWrite(&cm_acllock);
}
