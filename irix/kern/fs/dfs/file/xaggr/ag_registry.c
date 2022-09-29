/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: ag_registry.c,v 65.5 1998/04/01 14:16:42 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1989, 1990 Transarc Corporation - All rights reserved */

/*
 * Aggregate Registry
 */

#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/osi.h>			/* Standard vendor system headers */
#include <dcedfs/basicincludes.h>
#include <dcedfs/lock.h>
#include <aggr.h>
#include <dcedfs/volume.h>
#include "ag_trace.h"
#ifdef SGIMIPS
#include <ag_init.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xaggr/RCS/ag_registry.c,v 65.5 1998/04/01 14:16:42 gwehrman Exp $")

/*
 * Aggr module related globals
 */
struct aggr *ag_root = 0;		/* Root of aggrs global linked list */
struct lock_data ag_lock;		/* allocation lock for aggregates */
long ag_attached = 0;			/* Number of attached aggregates */
static int ag_registry_init = 0;	/* flag for initializing: 1 = inited */
struct icl_set *xops_iclSetp = 0;

/* 
 * ag_Init -- initialize the xaggr module.
 *
 * This function must be called before using the ag_lock.
 */
void ag_Init(void) 
{
    if (! ag_registry_init) { 
	long code;
	struct icl_log *logp;
	ag_registry_init = 1; 
	lock_Init(&ag_lock); 
	/* now initialize the ICL system */
#ifdef AFS_OSF11_ENV
	code = icl_CreateLog("disk", 2*1024, &logp);
#else
	code = icl_CreateLog("disk", 60*1024, &logp);
#endif
	if (code == 0)
	    code = icl_CreateSet("xops", logp, (struct icl_log *) 0, &xops_iclSetp);
	if (code)
	    printf("xaggr: warning: can't init icl for xops: code %d\n", code);
    }
}

/* 
 * Find an aggregate by its id; bump its reference count on success. 
 */
#ifdef SGIMIPS
struct aggr *ag_GetAggr(__int32_t aggrId)
#else
struct aggr *ag_GetAggr(long aggrId)
#endif
{
    struct aggr *aggrp;

    ag_Init();
    lock_ObtainWrite(&ag_lock);
    for (aggrp = ag_root; aggrp; aggrp = aggrp->a_next) {
	if ((aggrp->a_states & AGGR_EXPORTED) && aggrp->a_aggrId == aggrId) {
	    osi_assert(aggrp->a_refCount > 0);
	    aggrp->a_refCount++;
	    break;
	}
    }
    lock_ReleaseWrite(&ag_lock);
    return aggrp;
}


/* 
 * Find an aggregate by its id; bump its reference count on success. 
 */
struct aggr *ag_GetAggrByDev(dev_t dev)
{
    struct aggr *aggrp;

    ag_Init();
    lock_ObtainWrite(&ag_lock);
    for (aggrp = ag_root; aggrp; aggrp = aggrp->a_next) {
#if AFS_HPUX_ENV
        /* 
	 * Because of the way that efs_mount "forms" a
         * a device# in vfs.vfs_fsid[0], we can only look
         * at the lsbs here.  Otherwise, epiunmount will
         * fail when it calls us. 
	 */
        if ((aggrp->a_device & 0xffff) == (dev & 0xffff))
#else
        if (aggrp->a_device == dev)
#endif
	{
	    osi_assert(aggrp->a_refCount > 0);
	    aggrp->a_refCount++;
	    break;
	}
    }
    lock_ReleaseWrite(&ag_lock);
    return aggrp;
}


/* 
 * Create a new aggregate entry for the aggregate [aggrNamep, aggrId].  Also,
 * put all the aggregate's volumes in the volume registry.
 */

int ag_NewAggr(
    char *aggrNamep,
#ifdef SGIMIPS
    __int32_t aggrId,
#else
    long aggrId,
#endif
    char *aggrDevnamep,
    char aggrType,
    struct aggrops *aggrOpsp,
    dev_t dev,
    opaque fsdata,
    struct aggr **aggrpp,
    unsigned flags)
{
    struct aggr *aggrp;
    DEFINE_OSI_UERROR;      /* default to no error */

    icl_Trace3(xops_iclSetp, XAG_TRACE_NEWAGGR,
	       ICL_TYPE_STRING, aggrNamep,
	       ICL_TYPE_LONG, aggrId,
	       ICL_TYPE_POINTER, aggrOpsp);
    ag_Init();
    lock_ObtainWrite(&ag_lock);
    if (!(flags & AGGR_ATTACH_NOEXPORT))
	for (aggrp = ag_root; aggrp; aggrp = aggrp->a_next)
	    if (aggrp->a_states & AGGR_EXPORTED &&
		((aggrp->a_device == dev) ||
		 (aggrp->a_aggrId == aggrId) ||
		 !strcmp(aggrp->a_aggrName, aggrNamep))) {
		icl_Trace1(xops_iclSetp, XAG_TRACE_NEWAGGR_EXISTS,
			   ICL_TYPE_POINTER, aggrp);
		lock_ReleaseWrite(&ag_lock);
		osi_setuerror(EEXIST);
		*aggrpp = (struct aggr *) 0;
		return(EEXIST);
	    }
    aggrp = (struct aggr *) osi_Alloc(sizeof(struct aggr));
    bzero((caddr_t)aggrp, sizeof (struct aggr));
    aggrp->a_next = ag_root;
    ag_root = aggrp;
    strncpy(aggrp->a_devName, aggrDevnamep, sizeof(aggrp->a_devName));
    aggrp->a_refCount = 1;
    aggrp->a_type = aggrType;		/* XXX Verify it's valid XXX */
    aggrp->a_aggrOpsp = aggrOpsp;
    aggrp->a_device = dev;
    aggrp->a_fsDatap = fsdata;
    aggrp->a_localMounts = 0;
    lock_Init(&aggrp->a_lock);
    ag_attached++;
    if (!(flags & AGGR_ATTACH_NOEXPORT)) {
	aggrp->a_states |= AGGR_EXPORTED;
	aggrp->a_aggrId = aggrId;
	strncpy(aggrp->a_aggrName, aggrNamep, MAX_AGGRNAME);
	ag_RegisterVolumes(aggrp);
    }
    lock_ReleaseWrite(&ag_lock);
    icl_Trace1(xops_iclSetp, XAG_TRACE_NEWAGGR_NEW,
	       ICL_TYPE_POINTER, aggrp);
    *aggrpp = aggrp;
    osi_setuerror(0);
    return(0);
}


/*
 * Release aggregate's reference counter
 */
ag_PutAggr(struct aggr *aggrp)
{
    struct aggr *taggrp;
    int code;

    ag_Init();
    lock_ObtainWrite(&ag_lock);
    osi_assert(aggrp->a_refCount > 0);
    if (--aggrp->a_refCount > 0) {
	lock_ReleaseWrite(&ag_lock);
	return 0;
    }
    icl_Trace1(xops_iclSetp, XAG_TRACE_PUTAGGR_FINAL,
	       ICL_TYPE_POINTER, aggrp);
    if (aggrp == ag_root) {		/* Check the simple case first */
	ag_root = aggrp->a_next;
	code = AG_DETACH (aggrp);
	if (!code)
	    OSI_VN_RELE(aggrp->devvp);
	osi_Free((opaque)aggrp, sizeof(struct aggr));
	ag_attached--;
	lock_ReleaseWrite(&ag_lock);
	icl_Trace1(xops_iclSetp, XAG_TRACE_PUTAGGR_FINAL_END,
		   ICL_TYPE_LONG, code);
	return code;
    }
    for (taggrp = ag_root; taggrp; taggrp = taggrp->a_next) {
	if (taggrp->a_next == aggrp) {
	    taggrp->a_next = aggrp->a_next;
	    code = AG_DETACH (aggrp);
	    if (!code)
		OSI_VN_RELE(aggrp->devvp);
	    osi_Free((opaque)aggrp, sizeof(struct aggr));
	    ag_attached--;
	    lock_ReleaseWrite(&ag_lock);
	    icl_Trace1(xops_iclSetp, XAG_TRACE_PUTAGGR_FINAL_END,
		       ICL_TYPE_LONG, code);
	    return code;
	}
    }
    lock_ReleaseWrite(&ag_lock);
    icl_Trace1(xops_iclSetp, XAG_TRACE_PUTAGGR_FINAL_BOGUS,
	       ICL_TYPE_POINTER, aggrp);
    return ENOENT;
}
