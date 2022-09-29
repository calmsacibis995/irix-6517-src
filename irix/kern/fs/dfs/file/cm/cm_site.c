/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_site.c,v 65.4 1998/03/23 16:24:33 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * Copyright (C) 1996 Transarc Corporation - All rights reserved.
 */

/* cm_site.c -- site module.
 *
 *   This module implements the site object, which represents the state
 *   of a server's network connections on a multi-homed host (site).
 *   Functions in this file are used to implement multi-homed server
 *   support within the DFS cache manager.
 *
 *   See OSF-RFC 77 for further details.
 */

#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>

#include <cm.h>
#include <cm_server.h>
#include <cm_serverpref.h>
#include <cm_site.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_site.c,v 65.4 1998/03/23 16:24:33 gwehrman Exp $")


/* Macro definitions */

/*    address hash-table size and hashing function */
#define SITE_ADDR_TABLESZ     67
#ifdef SGIMIPS
#define SITE_ADDR_HASH(ap)    ((u_int)SR_ADDR_VAL(ap) % SITE_ADDR_TABLESZ)
#else  /* SGIMIPS */
#define SITE_ADDR_HASH(ap)    ((u_long)SR_ADDR_VAL(ap) % SITE_ADDR_TABLESZ)
#endif /* SGIMIPS */

/*    compare old/new addrCur to see if cm_siteAddrGen must be incremented */
#define SITE_ADDRGEN_INC(acp1, acp2) \
    (!SR_ADDR_EQ(&(acp1)->addr, &(acp2)->addr) || \
     (acp1)->rank != (acp2)->rank || \
     ((acp1)->state & SITEADDR_DOWN) != ((acp2)->state & SITEADDR_DOWN))

/*    action to be taken by SiteAddrUpdate() */
#define SITE_ADDR_REPLACE    0
#define SITE_ADDR_MERGE      1


/* Define site-module global data (exported and local) */

osi_dlock_t cm_siteLock;

u_long cm_siteAddrGen;

static struct cm_siteAddr *addrTable[SITE_ADDR_TABLESZ];


/* Declare local functions */

static void
SiteAddrUpdate(struct cm_site *sitep,
	       struct sockaddr_in *addrvp,
	       int addrvcnt,
	       int action);

static struct cm_siteAddr*
AddrAtSite(struct cm_site *sitep,
	   struct sockaddr_in *addrp);

static void
AddrInfoSet(struct cm_site *sitep);



/* Define exported functions */

/*
 * cm_SiteAlloc() -- allocate a site object for a new server object.
 * 
 * LOCKS USED: locks cm_addrRankReqsLock and cm_siteLock; released on exit.
 *
 * RETURN CODES: reference to allocated site object.
 */

struct cm_site*
cm_SiteAlloc(u_short svc,                 /* server type (SRT_FX/REP/FL) */
	     struct sockaddr_in *addrvp,  /* initial server-addr vector */
	     int addrvcnt)                /* initial server-addr vector size */
{
    struct cm_site *sitep;

    icl_Trace3(cm_iclSetp, CM_TRACE_SITE_ALLOC_ENTER,
	       ICL_TYPE_LONG, svc,
	       ICL_TYPE_POINTER, addrvp,
	       ICL_TYPE_LONG, addrvcnt);

    /* Allocate and initialize a cm_site structure */
    sitep = (struct cm_site *)osi_Alloc(sizeof(*sitep));

    bzero((char *)sitep, sizeof(*sitep));

    sitep->svc = svc;

    /* Obtain locks neccessary to modify address state, including
     * address-rank cache, so as to keep all (addr, svc, rank) consistent.
     */

    lock_ObtainWrite(&cm_addrRankReqsLock);
    lock_ObtainWrite(&cm_siteLock);

    SiteAddrUpdate(sitep, addrvp, addrvcnt, SITE_ADDR_REPLACE);

    lock_ReleaseWrite(&cm_siteLock);
    lock_ReleaseWrite(&cm_addrRankReqsLock);

    icl_Trace3(cm_iclSetp, CM_TRACE_SITE_ALLOC_EXIT,
	       ICL_TYPE_LONG, svc,
	       ICL_TYPE_POINTER, addrvp,
	       ICL_TYPE_POINTER, sitep);

    /* Return reference to newly allocated site object */
    return sitep;
}



/*
 * cm_SiteAddrReplace() -- replace list of server addresses with
 *     the list provided.
 * 
 * LOCKS USED: locks cm_addrRankReqsLock and cm_siteLock; released on exit.
 *
 * ASSUMPTIONS: addrvp[] contains no duplicate or incorrect addresses.
 *
 * SIDE EFFECTS: sets site's state; increments cm_siteAddrGen if needed.
 *
 * RETURN CODES: none
 */

void
cm_SiteAddrReplace(struct cm_site *sitep,      /* site object */
		   struct sockaddr_in *addrvp, /* server-address vector */
		   int addrvcnt)               /* server-address vector size */
{
    int i;
    struct cm_siteAddr *siteaddrp;

    icl_Trace3(cm_iclSetp, CM_TRACE_SITE_ADDRREPLACE_ENTER,
	       ICL_TYPE_POINTER, sitep,
	       ICL_TYPE_POINTER, addrvp,
	       ICL_TYPE_LONG, addrvcnt);

    if (addrvcnt <= 0) {
	return;
    }

    /* Check for normal case where no address changes have occured */

    lock_ObtainRead(&cm_siteLock);

    i         = 0;
    siteaddrp = sitep->addrListp;

    for (; i < addrvcnt && siteaddrp; i++, siteaddrp = siteaddrp->next_sa) {
	/* compare each addr in vector against those in current list */
	if (!AddrAtSite(sitep, &addrvp[i])) {
	    break;
	}
    }

    lock_ReleaseRead(&cm_siteLock);

    if (i == addrvcnt && siteaddrp == NULL) {
	/* addrs in vector identical to those in list at time checked */
	icl_Trace1(cm_iclSetp, CM_TRACE_SITE_ADDRREPLACE_SAME,
		   ICL_TYPE_POINTER, sitep);
	return;
    }

    /* Replace site address list with addresses provided */

    /* Obtain locks neccessary to modify address state, including
     * address-rank cache, so as to keep all (addr, svc, rank) consistent.
     */

    lock_ObtainWrite(&cm_addrRankReqsLock);
    lock_ObtainWrite(&cm_siteLock);

    SiteAddrUpdate(sitep, addrvp, addrvcnt, SITE_ADDR_REPLACE);

    lock_ReleaseWrite(&cm_siteLock);
    lock_ReleaseWrite(&cm_addrRankReqsLock);

    icl_Trace1(cm_iclSetp, CM_TRACE_SITE_ADDRREPLACE_EXIT,
	       ICL_TYPE_POINTER, sitep);
}



/*
 * cm_SiteAddrSetRankAll() -- assign rank to the specified address for
 *     all servers of the specified type, or cache address-rank for
 *     future reference.
 *
 *     Note that server types SRT_FX and SRT_REP are treated as
 *     equivalent; specifying either updates address rank for both.
 * 
 * LOCKS USED: locks cm_addrRankReqsLock and cm_siteLock; released on exit.
 *
 * SIDE EFFECTS: sets site state appropriately for all relevant servers;
 *     increments cm_siteAddrGen if necessary.
 *
 * RETURN CODES: none
 */

void
cm_SiteAddrSetRankAll(struct sockaddr_in *addrp,  /* address */
		      u_short svc,   /* server type (SRT_FX/REP/FL) */
		      u_short rank)  /* address rank */
{
    int tidx, addrSet;
    struct cm_siteAddr *listp;
    struct cm_siteAddr oldAddrCur;

    icl_Trace3(cm_iclSetp, CM_TRACE_SITE_SETRANK_ENTER,
	       ICL_TYPE_LONG, SR_ADDR_VAL(addrp),
	       ICL_TYPE_LONG, svc,
	       ICL_TYPE_LONG, rank);

    /* Obtain locks neccessary to modify address state, including
     * address-rank cache, so as to keep all (addr, svc, rank) consistent.
     */

    lock_ObtainWrite(&cm_addrRankReqsLock);
    lock_ObtainWrite(&cm_siteLock);

    /* Assign rank to addr for all servers of the specified type */
    addrSet = 0;
    tidx    = SITE_ADDR_HASH(addrp);

    for (listp = addrTable[tidx]; listp; listp = listp->next_sahash) {
	/* scan addrs looking for (addr, svc) match */

	if (SR_ADDR_EQ(&listp->addr, addrp) &&
	    SRT_SHARE_RANK(listp->sitep->svc, svc)) {
	    /* save addrCur for site */
	    oldAddrCur = *listp->sitep->addrCurp;

	    /* set address rank and flag action */
	    listp->rank = rank;
	    addrSet     = 1;

	    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_SETRANK_CHANGE,
		       ICL_TYPE_LONG, SR_ADDR_VAL(addrp),
		       ICL_TYPE_POINTER, listp->sitep);

	    /* set site's addrCurp and state */
	    AddrInfoSet(listp->sitep);

	    /* if addrCur changed then bump cm_siteAddrGen counter */
	    if (SITE_ADDRGEN_INC(&oldAddrCur, listp->sitep->addrCurp)) {
		cm_siteAddrGen++;

		icl_Trace2(cm_iclSetp, CM_TRACE_SITE_SETRANK_GEN,
			   ICL_TYPE_LONG, SR_ADDR_VAL(addrp),
			   ICL_TYPE_LONG, cm_siteAddrGen);
	    }
	}
    }

    if (!addrSet) {
	/* addr not assigned; queue request */
	cm_AddrRankPut(addrp, svc, rank);
    }

    lock_ReleaseWrite(&cm_siteLock);
    lock_ReleaseWrite(&cm_addrRankReqsLock);

    icl_Trace1(cm_iclSetp, CM_TRACE_SITE_SETRANK_EXIT,
	       ICL_TYPE_LONG, SR_ADDR_VAL(addrp));
}



/*
 * cm_SiteAddrDown() -- report failed communication to server address;
 *     perform address fail-over.
 *
 * LOCKS USED: locks cm_siteLock; released on exit.
 *
 * SIDE EFFECTS: sets site's state; increments cm_siteAddrGen if needed.
 *
 * RETURN CODES:
 *      0 - successful address fail-over
 *     -1 - unsuccessful address fail-over; all server addresses down
 */

int
cm_SiteAddrDown(struct cm_site *sitep,      /* site object */
		struct sockaddr_in *addrp,  /* server address */
		int *wasUpp)                /* indicates if addr was up */
{
    int rval;
    struct cm_siteAddr *siteaddrp, oldAddrCur;

    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_ADDRDOWN_ENTER,
	       ICL_TYPE_POINTER, sitep,
	       ICL_TYPE_LONG, SR_ADDR_VAL(addrp));

    lock_ObtainWrite(&cm_siteLock);

    *wasUpp = 0;

    if (sitep->addrListp == NULL) {
	/* no addresses to mark down or to use for fail-over */
	rval = -1;
    } else {
	/* save addrCur for site */
	oldAddrCur = *sitep->addrCurp;

	/* mark address down */
	if (siteaddrp = AddrAtSite(sitep, addrp)) {
	    *wasUpp = !(siteaddrp->state & SITEADDR_DOWN);
	    siteaddrp->state |= SITEADDR_DOWN;
	}

	/* set site's addrCurp and state */
	AddrInfoSet(sitep);

	/* bump cm_siteAddrGen counter if addrCur changed */
	if (SITE_ADDRGEN_INC(&oldAddrCur, sitep->addrCurp)) {
	    cm_siteAddrGen++;

	    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_ADDRDOWN_GEN,
		       ICL_TYPE_POINTER, sitep,
		       ICL_TYPE_LONG, cm_siteAddrGen);
	}

	/* determine if fail-over was successful or not */
	if (sitep->addrCurp->state & SITEADDR_DOWN) {
	    rval = -1;
	} else {
	    rval = 0;
	}
    }

    lock_ReleaseWrite(&cm_siteLock);

    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_ADDRDOWN_EXIT,
	       ICL_TYPE_POINTER, sitep,
	       ICL_TYPE_LONG, rval);

    return rval;
}



/*
 * cm_SiteAddrUp() -- report that server address is reachable; perform
 *     address revival.
 *
 * LOCKS USED: locks cm_siteLock; released on exit.
 *
 * SIDE EFFECTS: sets site's state; increments cm_siteAddrGen if needed.
 *
 * RETURN CODES: none
 */

void
cm_SiteAddrUp(struct cm_site *sitep,      /* site object */
	      struct sockaddr_in *addrp,  /* server address */
	      int *wasDownp)              /* indicates if addr was down */
{
    struct cm_siteAddr *siteaddrp, oldAddrCur;

    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_ADDRUP_ENTER,
	       ICL_TYPE_POINTER, sitep,
	       ICL_TYPE_LONG, SR_ADDR_VAL(addrp));

    lock_ObtainWrite(&cm_siteLock);

    *wasDownp = 0;

    if (sitep->addrListp != NULL) {
	/* save addrCur for site */
	oldAddrCur = *sitep->addrCurp;

	/* mark address up */
	if (siteaddrp = AddrAtSite(sitep, addrp)) {
	    *wasDownp = (siteaddrp->state & SITEADDR_DOWN);
	    siteaddrp->state &= ~(SITEADDR_DOWN);
	}

	/* set site's addrCurp and state */
	AddrInfoSet(sitep);

	/* bump cm_siteAddrGen counter if addrCur changed */
	if (SITE_ADDRGEN_INC(&oldAddrCur, sitep->addrCurp)) {
	    cm_siteAddrGen++;

	    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_ADDRUP_GEN,
		       ICL_TYPE_POINTER, sitep,
		       ICL_TYPE_LONG, cm_siteAddrGen);
	}
    }

    lock_ReleaseWrite(&cm_siteLock);

    icl_Trace1(cm_iclSetp, CM_TRACE_SITE_ADDRUP_EXIT,
	       ICL_TYPE_POINTER, sitep);
}



/*
 * cm_SiteAddrMarkAllUp() -- mark all server's addresses as up,
 *     unconditionally or only if all down.
 * 
 * LOCKS USED: locks cm_siteLock; released on exit.
 *
 * SIDE EFFECTS: sets site's state; increments cm_siteAddrGen if needed.
 *
 * RETURN CODES: none
 */

void
cm_SiteAddrMarkAllUp(struct cm_site *sitep,  /* site object */
		     int ifAllDown)  /* unconditionally (0), or if all down */
{
    struct cm_siteAddr *listp, oldAddrCur;

    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_MARKALLUP_ENTER,
	       ICL_TYPE_POINTER, sitep,
	       ICL_TYPE_LONG, ifAllDown);

    lock_ObtainWrite(&cm_siteLock);

    if (sitep->addrListp != NULL) {
	/* site's current addr is down iff all site's addrs are down */
	if ((!ifAllDown) || (sitep->addrCurp->state & SITEADDR_DOWN)) {
	    /* save addrCur for site */
	    oldAddrCur = *sitep->addrCurp;

	    /* mark all site's addresses as up */
	    for (listp = sitep->addrListp; listp; listp = listp->next_sa) {
		listp->state &= ~(SITEADDR_DOWN);
	    }

	    /* set site's addrCurp and state */
	    AddrInfoSet(sitep);

	    /* bump cm_siteAddrGen counter if addrCur changed */
	    if (SITE_ADDRGEN_INC(&oldAddrCur, sitep->addrCurp)) {
		cm_siteAddrGen++;

		icl_Trace2(cm_iclSetp, CM_TRACE_SITE_MARKALLUP_GEN,
			   ICL_TYPE_POINTER, sitep,
			   ICL_TYPE_LONG, cm_siteAddrGen);
	    }
	}
    }

    lock_ReleaseWrite(&cm_siteLock);

    icl_Trace1(cm_iclSetp, CM_TRACE_SITE_MARKALLUP_EXIT,
	       ICL_TYPE_POINTER, sitep);
}



/*
 * cm_SiteAddrRankOverride() -- make specified address the current address
 *     for the server, regardless of rank.
 *     If address was down, mark it as up; if address was previously unknown,
 *     add it to the server-address list.
 *
 * LOCKS USED: locks cm_addrRankReqsLock and cm_siteLock; released on exit.
 *
 * SIDE EFFECTS: sets site's state; increments cm_siteAddrGen if needed.
 *
 * RETURN CODES: none
 */

void
cm_SiteAddrRankOverride(struct cm_site *sitep,      /* site object */
			struct sockaddr_in *addrp,  /* server address */
			int *wasCurp)    /* indicates if addr was current */
{
    struct cm_siteAddr *siteaddrp, oldAddrCur;
    int oldAddrValid;

    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_RANKOVERRIDE_ENTER,
	       ICL_TYPE_POINTER, sitep,
	       ICL_TYPE_LONG, SR_ADDR_VAL(addrp));

    /* Obtain locks neccessary to modify address state, including
     * address-rank cache, so as to keep all (addr, svc, rank) consistent.
     */

    lock_ObtainWrite(&cm_addrRankReqsLock);
    lock_ObtainWrite(&cm_siteLock);

    /* Save current addrCur to compare against addrCur in updated addr list */

    if (sitep->addrCurp) {
	oldAddrCur   = *sitep->addrCurp;
	oldAddrValid = 1;
    } else {
	oldAddrValid = 0;
    }

    /* Mark address up if extant, add to address-list if not */

    if (siteaddrp = AddrAtSite(sitep, addrp)) {
	siteaddrp->state &= ~(SITEADDR_DOWN);
	*wasCurp = (sitep->addrCurp == siteaddrp);
	/* set site flags; note that will override site's addrCurp below */
	AddrInfoSet(sitep);
    } else {
	SiteAddrUpdate(sitep, addrp, 1, SITE_ADDR_MERGE);
	*wasCurp = 0;
    }

    /* Override rank-based addrCurp setting */

    sitep->addrCurp = AddrAtSite(sitep, addrp);

    /* Bump cm_siteAddrGen counter if addrCur changed */

    if (oldAddrValid) {
	if (SITE_ADDRGEN_INC(&oldAddrCur, sitep->addrCurp)) {
	    cm_siteAddrGen++;

	    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_RANKOVERRIDE_GEN,
		       ICL_TYPE_POINTER, sitep,
		       ICL_TYPE_LONG, cm_siteAddrGen);
	}
    }

    lock_ReleaseWrite(&cm_siteLock);
    lock_ReleaseWrite(&cm_addrRankReqsLock);

    icl_Trace1(cm_iclSetp, CM_TRACE_SITE_RANKOVERRIDE_EXIT,
	       ICL_TYPE_POINTER, sitep);
}



/* Define local functions */

/*
 * SiteAddrUpdate() -- update list of server addresses from
 *     list provided; either replace or merge, as specified.
 * 
 * ASSUMPTIONS: caller holds WRITE locks on cm_addrRankReqsLock and
 *     cm_siteLock, and arguments checked.
 *
 * SIDE EFFECTS: sets site's state; increments cm_siteAddrGen if needed.
 *
 * RETURN CODES: none
 */

static void
SiteAddrUpdate(struct cm_site *sitep,      /* site object */
	       struct sockaddr_in *addrvp, /* server-address vector */
	       int addrvcnt,               /* server-address vector size */
	       int action)                 /* REPLACE or MERGE */
{
    int i, tidx, oldAddrValid;
    int sameCnt, newCnt;
    struct cm_siteAddr *listp, *siteaddrp, *stalep, *stalep_next;
    struct cm_siteAddr oldAddrCur;

    /* Update site address list from addresses provided */

    /* Caller holds locks neccessary to modify address state, including
     * address-rank cache, so as to keep all (addr, svc, rank) consistent.
     */

    /* Save current addrCur to compare against addrCur in new addr list */

    if (sitep->addrCurp) {
	oldAddrCur   = *sitep->addrCurp;
	oldAddrValid = 1;
    } else {
	oldAddrValid = 0;
    }

    /* If merging, mark all current site addresses as "keepers" */

    if (action == SITE_ADDR_MERGE) {
	for (listp = sitep->addrListp; listp; listp = listp->next_sa) {
	    listp->state |= SITEADDR_KEEPER;
	}
    }

    /* Update site address-list from the address-vector args */

    newCnt = sameCnt = 0;

    for (i = 0; i < addrvcnt; i++) {
	if (siteaddrp = AddrAtSite(sitep, &addrvp[i])) {
	    /* address already in list; mark as "keeper" */
	    siteaddrp->state |= SITEADDR_KEEPER;

	    sameCnt++;
	} else {
	    /* allocate/define new addr entry */
	    siteaddrp = (struct cm_siteAddr *)osi_Alloc(sizeof(*siteaddrp));

	    siteaddrp->sitep = sitep;
	    siteaddrp->addr  = addrvp[i];
	    siteaddrp->state = SITEADDR_KEEPER;

#ifdef AFS_OSF_ENV
	    /* cribbed from (old) cm_GetServer(); don't know if/why needed */
	    siteaddrp->addr.sin_len = sizeof(siteaddrp->addr);
#endif

	    /* determine new addr rank */
	    tidx = SITE_ADDR_HASH(&siteaddrp->addr);

	    for (listp = addrTable[tidx]; listp; listp = listp->next_sahash) {
		/* scan addrs looking for (addr, svc) match to get rank from */
		if (SR_ADDR_EQ(&listp->addr, &siteaddrp->addr) &&
		    SRT_SHARE_RANK(listp->sitep->svc,
				   siteaddrp->sitep->svc)) {
		    break;
		}
	    }

	    if (listp) {
		/* use rank from another server at same address */
		siteaddrp->rank = listp->rank;
	    } else {
		/* use specified/computed rank value */
		if (cm_AddrRankGet(&siteaddrp->addr,
				   siteaddrp->sitep->svc,
				   &siteaddrp->rank)) {
		    /* no rank specified for (addr, svc); compute a rank */
		    siteaddrp->rank = cm_AddrRankCompute(&siteaddrp->addr);
		}
	    }

	    /* put new addr entry at head of site-address list */
	    siteaddrp->next_sa  = sitep->addrListp;
	    sitep->addrListp    = siteaddrp;

	    /* put new addr entry into hash table */
	    siteaddrp->next_sahash = addrTable[tidx];
	    addrTable[tidx]        = siteaddrp;

	    newCnt++;
	}
    }

    icl_Trace3(cm_iclSetp, CM_TRACE_SITE_ADDRUPDATE_CHANGE,
	       ICL_TYPE_POINTER, sitep,
	       ICL_TYPE_LONG, sameCnt,
	       ICL_TYPE_LONG, newCnt);

    /* Purge new address list of stale entries */

    listp = NULL;

    for (stalep = sitep->addrListp; stalep; stalep = stalep_next) {
	/* remember where to go next */
	stalep_next = stalep->next_sa;

	/* determine if addr struct is good (keeper) or stale */
	if (stalep->state & SITEADDR_KEEPER) {
	    /* good entry */
	    stalep->state &= ~(SITEADDR_KEEPER);

	    /* advance trailing site-address pointer */
	    listp = stalep;

	} else {
	    /* stale entry */

	    /* remove addr struct from hash table */
	    tidx      = SITE_ADDR_HASH(&stalep->addr);
	    siteaddrp = addrTable[tidx];

	    if (siteaddrp == stalep) {
		/* special case; first element in hash list */
		addrTable[tidx] = stalep->next_sahash;
	    } else {
		/* second or later element in hash list */
		for (; siteaddrp; siteaddrp = siteaddrp->next_sahash) {
		    if (siteaddrp->next_sahash == stalep) {
			siteaddrp->next_sahash = stalep->next_sahash;
			break;
		    }
		}
	    }
	    osi_assert(siteaddrp);

	    /* remove addr struct from site-address list */
	    if (listp == NULL) {
		/* special case; first element in site-address list */
		sitep->addrListp = stalep->next_sa;
	    } else {
		/* second or later element in site-address list */
		listp->next_sa = stalep->next_sa;
	    }

	    /* deallocate addr struct */
	    osi_Free((opaque)stalep, sizeof(*stalep));
	}
    }

    /* Set site's addrCurp and state */
    AddrInfoSet(sitep);

    /* Determine if addrCur changes force increment of cm_siteAddrGen */
    if (oldAddrValid) {
	if (SITE_ADDRGEN_INC(&oldAddrCur, sitep->addrCurp)) {
	    cm_siteAddrGen++;

	    icl_Trace2(cm_iclSetp, CM_TRACE_SITE_ADDRUPDATE_GEN,
		       ICL_TYPE_POINTER, sitep,
		       ICL_TYPE_LONG, cm_siteAddrGen);
	}
    }
}



/*
 * AddrAtSite() -- determine if address is in site's address-list
 * 
 * ASSUMPTIONS: caller holds lock on cm_siteLock.
 *
 */

static struct cm_siteAddr*
AddrAtSite(struct cm_site *sitep,
	   struct sockaddr_in *addrp)
{
    int tidx;
    struct cm_siteAddr *listp;

    /* scan hash list for (sitep, addrp) match */
    tidx = SITE_ADDR_HASH(addrp);

    for (listp = addrTable[tidx]; listp; listp = listp->next_sahash) {
	if (listp->sitep == sitep && SR_ADDR_EQ(&listp->addr, addrp)) {
	    break;
	}
    }

    return (listp);
}



/*
 * AddrInfoSet() -- set site's addrCurp field to best-ranking up address,
 *     if any; otherwise set it to absolute best-ranking (down) address.
 *     Also set site's state field to indicate if any address is down.
 * 
 * ASSUMPTIONS: caller holds WRITE lock on cm_siteLock.
 */

static void
AddrInfoSet(struct cm_site *sitep)
{
    struct cm_siteAddr *listp, *absBestp, *upBestp;
    int downAddr;

    absBestp = upBestp = NULL;
    downAddr = 0;

    /* Scan site's address list looking for up-best/absolute-best addr */
    for (listp = sitep->addrListp; listp; listp = listp->next_sa) {
	if (!absBestp || absBestp->rank > listp->rank) {
	    absBestp = listp;
	}

	if (!(listp->state & SITEADDR_DOWN)) {
	    if (!upBestp || upBestp->rank > listp->rank) {
		upBestp = listp;
	    }
	} else {
	    downAddr = 1;
	}
    }

    /* Set addrCurp to up-best, if extant; else set to absolute-best */
    if (upBestp) {
	sitep->addrCurp = upBestp;
    } else {
	sitep->addrCurp = absBestp;
    }

    /* Set state to indicate if any address is down */
    if (downAddr) {
	sitep->state |= SITE_DOWNADDR;
    } else {
	sitep->state &= ~(SITE_DOWNADDR);
    }
}
