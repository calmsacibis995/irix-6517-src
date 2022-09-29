/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_serverpref.c,v 65.4 1998/03/23 16:23:54 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 *      Copyright (C) 1996, 1995, 1994 Transarc Corporation
 *      All rights reserved.
 */

/*
 * This module manages server-address rankings and requests for setting them.
 */

#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/sysincludes.h>		/* Standard vendor system headers */
#include <dcedfs/stds.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_server.h>
#include <cm_serverpref.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_serverpref.c,v 65.4 1998/03/23 16:23:54 gwehrman Exp $")


/* Define preference-module global data (local and exported) */

osi_dlock_t     cm_addrRankReqsLock;
cm_addrRank_t   *cm_addrRankReqs = NULL;

/* Define some useful macros */
#define IPADDR_EQ(ap1, ap2)  ((ap1)->s_addr == (ap2)->s_addr)

#ifdef SGIMIPS
extern int osi_SameHost(struct in_addr *);
extern int osi_SameSubNet(struct in_addr *);
extern int osi_SameNet(struct in_addr *);
#endif /* SGIMIPS */


/* Declare local functions */

static cm_addrRank_t *reqInQueue(struct in_addr *ipaddrp, u_short svc);


/*
 * cm_AddrRankPut() -- queue address-rank request; if (addr, svc) already
 *     in queue, then update rank.
 *
 *     Note that server types SRT_FX and SRT_REP are treated as
 *     equivalent; a single entry is queued for both.
 * 
 * ASSUMPTIONS: caller holds write-lock on cm_addrRankReqsLock
 *
 * RETURN CODES: none
 */

extern void
cm_AddrRankPut(struct sockaddr_in *addrp,  /* address */
	       u_short svc,                /* server type (SRT_FX/REP/FL) */
	       u_short rank)               /* address rank */
{
    cm_addrRank_t *reqp;

    osi_assert(lock_WriteLocked(&cm_addrRankReqsLock));

    icl_Trace3(cm_iclSetp, CM_TRACE_RANK_PUT,
	       ICL_TYPE_LONG, SR_ADDR_VAL(addrp),
	       ICL_TYPE_LONG, svc,
	       ICL_TYPE_LONG, rank);

    /* Put request in queue or update existing entry, as required */

    if (reqp = reqInQueue(&addrp->sin_addr, svc)) {
	/* request (addr, svc) already in queue; update rank */
	reqp->rank = rank;
    } else {
	/* queue new request */
	reqp = (cm_addrRank_t *)osi_Alloc(sizeof(cm_addrRank_t));

	reqp->addr = addrp->sin_addr;
	reqp->svc  = svc;
	reqp->rank = rank;

	reqp->next = cm_addrRankReqs;
	reqp->prev = NULL;

	if (cm_addrRankReqs) {
	    cm_addrRankReqs->prev = reqp;
	}

	cm_addrRankReqs = reqp;
    }
}



/*
 * cm_AddrRankGet() -- dequeue address-rank request for (addr, svc), if
 *     extant, and return rank.
 * 
 *     Note that server types SRT_FX and SRT_REP are treated as equivalent.
 *
 * ASSUMPTIONS: caller holds write-lock on cm_addrRankReqsLock
 *
 * RETURN CODES:
 *      0 - rank returned
 *     -1 - (addr, svc) not in queue
 */

extern int
cm_AddrRankGet(struct sockaddr_in *addrp,  /* address */
	       u_short svc,                /* server type (SRT_FX/REP/FL) */
	       u_short *rankp)             /* returned address rank */
{
    cm_addrRank_t *reqp;
    int rval;

    osi_assert(lock_WriteLocked(&cm_addrRankReqsLock));

    /* Get rank from queue if extant */

    if (reqp = reqInQueue(&addrp->sin_addr, svc)) {
	/* request (addr, svc) in queue; remove and return rank */
	*rankp = reqp->rank;

	if (reqp->prev) {
	    reqp->prev->next = reqp->next;
	} else {
	    cm_addrRankReqs = reqp->next;
	}

	if (reqp->next) {
	    reqp->next->prev = reqp->prev;
	}

	osi_Free(reqp, sizeof(cm_addrRank_t));

	rval = 0;
    } else {
	/* request (addr, svc) not in queue */
	rval = -1;
    }

    icl_Trace4(cm_iclSetp, CM_TRACE_RANK_GET,
	       ICL_TYPE_LONG, SR_ADDR_VAL(addrp),
	       ICL_TYPE_LONG, svc,
	       ICL_TYPE_LONG, *rankp,
	       ICL_TYPE_LONG, rval);

    return (rval);
}



/*
 * cm_AddrRankCompute() -- compute a rank for a given address
 * 
 * RETURN CODES: computed rank
 */

u_short
cm_AddrRankCompute(struct sockaddr_in *addrp)  /* address */
{
    u_short rank;

    if (osi_SameHost(&addrp->sin_addr)) {
	rank = CM_RANK_DEF_SAMEHOST;
    } else if (osi_SameSubNet(&addrp->sin_addr)) {
	rank = CM_RANK_DEF_SAMESUBNET;
    } else if (osi_SameNet(&addrp->sin_addr)) {
	rank = CM_RANK_DEF_SAMENET;
    } else {
	rank = CM_RANK_DEF_OTHER;
    }

    icl_Trace2(cm_iclSetp, CM_TRACE_RANK_COMP,
	       ICL_TYPE_LONG, SR_ADDR_VAL(addrp),
	       ICL_TYPE_LONG, rank);

    return(rank);
}



/* Define local functions */

/*
 * reqInQueue() -- determine if (addr, svc) already in queue
 * 
 * ASSUMPTIONS: caller holds lock on cm_addrRankReqsLock
 *
 */

static cm_addrRank_t*
reqInQueue(struct in_addr *ipaddrp,
	   u_short svc)
{
    cm_addrRank_t *reqp;

    for (reqp = cm_addrRankReqs; reqp; reqp = reqp->next) {
        if (IPADDR_EQ(ipaddrp, &reqp->addr) &&
	    SRT_SHARE_RANK(svc, reqp->svc)) {
	    break;
        }
    }

    return(reqp);
}
