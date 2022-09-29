/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: ag_init.c,v 65.6 1998/04/01 14:16:40 gwehrman Exp $";
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
 * $Log: ag_init.c,v $
 * Revision 65.6  1998/04/01 14:16:40  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added include for volreg.h and xcred.h.
 *
 * Revision 65.5  1998/03/23  16:42:43  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/01/20 17:16:16  lmc
 * Lots of changes from longs to 32 bit ints.  Also added explicit
 * casting of longs to ints where it was appropriate (the values would
 * always be less than 32bits.).
 *
 * Revision 65.2  1997/11/06  20:00:52  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:31  jdoak
 * *** empty log message ***
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/common_data.h>
#include <aggr.h>
#include <dcedfs/volume.h>
#include <dcedfs/vol_errs.h>
#include <dcedfs/vol_init.h>
#include <ag_init.h>
#include <astab.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_mount.h>
#include <dcedfs/tkc.h>
#include "ag_trace.h"
#ifdef SGIMIPS
#include <dcedfs/syscall.h>		/* Picks up io_rwDesc type */
#include <dcedfs/volreg.h>
#include <dcedfs/xcred.h>
#endif

#ifdef AFS_AIX31_ENV
#include <sys/sysmacros.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xaggr/RCS/ag_init.c,v 65.6 1998/04/01 14:16:40 gwehrman Exp $")

#ifndef AFS_DYNAMIC
extern struct aggrops ag_ufsops;
#ifndef AFS_IRIX_ENV
extern struct aggrops ag_efsops;
#endif /* !AFS_IRIX_ENV */
#endif

static int ag_volcreate(long, long, long, long);
static int ag_attach(long, long, long);
static int ag_aggrInfo(long, long);
static int ag_aggrEnumerate(long, long, long);
static int ag_volEnumerate (long, long, long, long);
static int ag_stat(long, caddr_t);
static int ag_volinfo(long, long, caddr_t);
static int ag_detach(long, long);
static int ag_UnRegisterVolumes (struct aggr *, int);

struct aggrops *agOpvec[MAX_AG_TYPE] = {
    (struct aggrops *) 0,		/* UNKNOWN */
#ifdef AFS_DYNAMIC
    (struct aggrops *) 0,		/* Initialized later */
    (struct aggrops *) 0,		/* Initialized later */
#else
    &ag_ufsops,				/* UFS */
#ifdef AFS_IRIX_ENV
    (struct aggrops *)  0,		/* EPISODE */
#else
    &ag_efsops,				/* EPISODE */
#endif /* AFS_IRIX_ENV */
#endif /* AFS_DYNAMIC */
    (struct aggrops *) 0,		/* AIX3 */
    (struct aggrops *) 0,		/* VxFS */
    (struct aggrops *) 0,		/* DMEPI */
};

void
ag_setops (int entry, struct aggrops *ops)
{
    agOpvec[entry] = ops;
}


/* Share-able stubs */

int ag_fsHold(struct aggr *aggrp)
{
    aggrp->a_refCount++;
    icl_Trace2(xops_iclSetp, XAG_TRACE_AGHOLD,
	       ICL_TYPE_POINTER, aggrp, ICL_TYPE_LONG, aggrp->a_refCount);
    return 0;
}

int ag_fsRele(struct aggr *aggrp)
{
    aggrp->a_refCount--;
    icl_Trace2(xops_iclSetp, XAG_TRACE_AGRELE,
	       ICL_TYPE_POINTER, aggrp, ICL_TYPE_LONG, aggrp->a_refCount);
    return 0;
}

#ifdef SGIMIPS
int ag_fsLock(
    struct aggr *aggrp,
    int type)
#else
int ag_fsLock(aggrp, type)
    struct aggr *aggrp;
    int type;
#endif
{
    int code = 0;

    icl_Trace2(xops_iclSetp, XAG_TRACE_ENTER_AGLOCK,
	       ICL_TYPE_POINTER, aggrp, ICL_TYPE_LONG, type);
    if (type == READ_LOCK)
	lock_ObtainRead(&aggrp->a_lock);
    else if (type == WRITE_LOCK)
	lock_ObtainWrite(&aggrp->a_lock);
    else if (type == SHARED_LOCK)
	lock_ObtainShared(&aggrp->a_lock);
    else
	code = EINVAL;
    icl_Trace1(xops_iclSetp, XAG_TRACE_LEAVE_AGLOCK, ICL_TYPE_LONG, code);
    return code;
}

#ifdef SGIMIPS
int ag_fsUnlock(
    struct aggr *aggrp,
    int type)
#else
int ag_fsUnlock(aggrp, type)
    struct aggr *aggrp;
    int type;
#endif
{
    int code = 0;

    icl_Trace2(xops_iclSetp, XAG_TRACE_ENTER_AGUNLOCK,
	       ICL_TYPE_POINTER, aggrp, ICL_TYPE_LONG, type);
    if (type == READ_LOCK)
	lock_ReleaseRead(&aggrp->a_lock);
    else if (type == WRITE_LOCK)
	lock_ReleaseWrite(&aggrp->a_lock);
    else if (type == SHARED_LOCK)
	lock_ReleaseShared(&aggrp->a_lock);
    else
	code = EINVAL;
    icl_Trace1(xops_iclSetp, XAG_TRACE_LEAVE_AGUNLOCK, ICL_TYPE_LONG, code);
    return code;
}

int ag_fsDMHold(struct aggr *aggrp)
{
    icl_Trace1(xops_iclSetp, XAG_TRACE_DMHOLD,
	       ICL_TYPE_POINTER, aggrp);
    return 0;
}

int ag_fsDMRele(struct aggr *aggrp)
{
    icl_Trace1(xops_iclSetp, XAG_TRACE_DMRELE,
	       ICL_TYPE_POINTER, aggrp);
    return 0;
}

/*
 * Main aggregate operations kernel entry point
 */
int afscall_aggr(
    long parm, long parm2, long parm3, long parm4, long parm5,
    int *retvalP)
{
    register struct aggr *aggrp;
#ifdef SGIMIPS
    int code=0;
#else
    long code=0;
#endif

    *retvalP = 0;				/* Start afresh */
    icl_Trace1(xops_iclSetp, XAG_TRACE_AFSCALL,
	       ICL_TYPE_LONG, (u_long) parm);
    switch (parm) {
      case AGOP_STAT:
	code = ag_stat(parm2, (caddr_t)parm3);
	break;
      case AGOP_VOLCREATE:
	if (!osi_suser(osi_getucred())) {
	    return(EACCES);
	}
	code = ag_volcreate(parm2, parm3, parm4, parm5);
	break;
      case AGOP_VOLINFO:
	code = ag_volinfo(parm2, parm3, (caddr_t)parm4);
	break;
      case AGOP_DETACH:
	code = ag_detach(parm2, parm3);
	break;
      case AGOP_ATTACH:
	if (!osi_suser(osi_getucred())) {
	    return(EACCES);
	}
	code = ag_attach(parm2, parm3, parm4);
	break;
      case AGOP_SCAN:
#ifdef SGIMIPS
	if (aggrp = ag_GetAggr(((__int32_t)parm2))) {
#else
	if (aggrp = ag_GetAggr((parm2))) {
#endif
	    code = osi_copyout((caddr_t)aggrp, (caddr_t)parm3,
			       sizeof(struct aggr));
	    ag_PutAggr(aggrp);
	} else
	    code = ENOENT;
	break;
      case AGOP_AGGINFO:
	code = ag_aggrInfo(parm2, parm3);
	break;
      case AGOP_AGGENUMERATE:
	code = ag_aggrEnumerate(parm2, parm3, parm4);
	break;
      case AGOP_VOLENUMERATE:
	code = ag_volEnumerate(parm2, parm3, parm4, parm5);
	break;
      case AGOP_SYNC:
#ifdef SGIMIPS
	aggrp = ag_GetAggr((__int32_t)parm2);
#else
	aggrp = ag_GetAggr(parm2);
#endif
	if (aggrp) {
#ifdef SGIMIPS
	    code = AG_SYNC(aggrp, (int)parm3);
#else
	    code = AG_SYNC(aggrp, parm3);
#endif
	    ag_PutAggr(aggrp);
	}
	break;
      default:
	code = EINVAL;
	break;
    }

    icl_Trace1(xops_iclSetp, XAG_TRACE_AFSCALL_END,
	       ICL_TYPE_LONG, (u_long) code);
    return code;
}

static int 
ag_stat(long aggrId, caddr_t ustatusp) 
{
    int code;
    struct ag_status astatus;	/* for some ops */
    struct aggr *aggrp;

#ifdef SGIMIPS
    if (aggrp = ag_GetAggr((__int32_t)aggrId)) {
#else
    if (aggrp = ag_GetAggr(aggrId)) {
#endif
	code = AG_STAT(aggrp, &astatus);
	if (!code)
	    code = osi_copyout((caddr_t)&astatus, ustatusp, 
			       sizeof(struct ag_status));
	ag_PutAggr(aggrp);
    } else
	code = ENOENT;
    return code;
}

static int 
ag_volinfo(
  long aggrId, 
  long index, 
  caddr_t uvolp)
{
    int code = 0;
    struct volume avol;	/* for some ops */
    struct aggr *aggrp;

    bzero((caddr_t)&avol, sizeof(struct volume));
#ifdef SGIMIPS
    if (aggrp = ag_GetAggr((__int32_t)aggrId)) {
	code = AG_VOLINFO(aggrp, (int)index, &avol);
#else
    if (aggrp = ag_GetAggr(aggrId)) {
	code = AG_VOLINFO(aggrp, index, &avol);
#endif
	if (!code)
	    code = osi_copyout((caddr_t)&avol, (caddr_t)uvolp,
			       sizeof(avol));
	ag_PutAggr(aggrp);
    }
    return code;
}

static int
ag_detach(long aggrKey, long flags)
{
    int code = 0;
    register struct aggr *aggrp;

    if (!osi_suser(osi_getucred())) {
	return(EACCES);
    }
    /*
     * if the caller is not exporting the aggregate (e.g. if we are
     * being called on behalf of epiunmount), he cannot be assumed
     * to know the aggregate's id or name (he cannot necessarily
     * look them up in dfstab, as that file may not exist).  therefore
     * if flags indicates noexport, aggrkey is a device; otherwise
     * aggrkey is an id.
     */
    if (flags & AGGR_ATTACH_NOEXPORT)
#ifdef SGIMIPS
	aggrp = ag_GetAggrByDev((dev_t)aggrKey);
#else
	aggrp = ag_GetAggrByDev(aggrKey);
#endif
    else
#ifdef SGIMIPS
	aggrp = ag_GetAggr((__int32_t)aggrKey);
#else
	aggrp = ag_GetAggr(aggrKey);
#endif
    if (aggrp) {
	lock_ObtainRead(&aggrp->a_lock);
	if (aggrp->a_localMounts != 0) {
	    lock_ReleaseRead(&aggrp->a_lock);
	    ag_PutAggr(aggrp);
	    return EBUSY;
	}
	lock_ReleaseRead(&aggrp->a_lock);
	if (!(flags & AGGR_ATTACH_NOEXPORT)) {
	    if (code = ag_UnRegisterVolumes(aggrp,
					    (flags & AGGR_ATTACH_FORCE) ? 1 : 0)) {
		ag_PutAggr(aggrp);
		return code;
	    }
	    lock_ObtainRead(&aggrp->a_lock);
	    aggrp->a_states &= ~AGGR_EXPORTED;
	    lock_ReleaseRead(&aggrp->a_lock);
	} else {
	    code = 0;
	}
	/*
	 * the aggr's refcnt should be at least 2 at this point. one
	 * as a result of the original ag_newaggr and the other from
	 * the ag_getaggr above...
	 */
	ag_PutAggr(aggrp);
	code = ag_PutAggr(aggrp);
    } else
	code = ENOENT;
    return code;
}

/*
 * create a new volume in the aggregate
 */
#ifdef SGIMIPS
static ag_volcreate(
    long parm2, long parm3, long parm4, long parm5)
#else
static ag_volcreate(parm2, parm3, parm4, parm5)
    long parm2, parm3, parm4, parm5;
#endif
{
#ifdef SGIMIPS
    register int code;
#else
    register long code;
#endif
    struct aggr *aggrp;
    struct vol_status vstat;
    afs_hyper_t volid;
    struct afsFid fid;

#ifdef SGIMIPS
    if (!(aggrp = ag_GetAggr((__int32_t)parm2)))
#else
    if (!(aggrp = ag_GetAggr(parm2)))
#endif
	return ENOENT;
    code = osi_copyin((caddr_t)parm3, (caddr_t)&volid, sizeof(afs_hyper_t));
    if (!code)
	code = osi_copyin((caddr_t)parm4,
			  (caddr_t)&vstat, sizeof (struct vol_status));
    if (!code) {
	fid.Volume = volid;
	code = volreg_Lookup(&fid, (struct volume **) 0);
	if (code == 0)
	    code = EEXIST;
	else if (code == ENODEV)
	    code = 0;
    }
    if (!code) {
	icl_Trace3(xops_iclSetp, XAG_TRACE_VOLCREATE,
		   ICL_TYPE_LONG, parm2,
		   ICL_TYPE_HYPER, &volid,
		   ICL_TYPE_LONG, parm5);
	/* prepare to manage these volume status bits */
	vstat.vol_st.states &= ~(VOL_BITS_INITIALMASK);
	vstat.vol_st.states |= (VOL_BITS_INITIAL);
#ifdef SGIMIPS
	/*
	 * Presently we do not support volcreate so OK.
	 * Otherwise need to look at volume struct for 32/64 bit issues.
	 * ref: vol_ufsCreate 
	 */
	code = AG_VOLCREATE(aggrp, &volid, &vstat, (int)parm5);
#else
	code = AG_VOLCREATE(aggrp, &volid, &vstat, parm5);
#endif
    }
    icl_Trace1(xops_iclSetp, XAG_TRACE_VOLCREATE_DONE,
	       ICL_TYPE_LONG, code);
    ag_PutAggr(aggrp);
    return code;
}

/*
 * Attach a new aggregate to the list of attached aggregates.
 */
#ifdef SGIMIPS
static ag_attach(
    long parm2, long parm3, long parm4)
#else
static ag_attach(parm2, parm3, parm4)
    long parm2, parm3, parm4;
#endif
{
    struct aggr *aggrp;
    struct astab *astabp;
    dev_t dev;
    int code, fsdatalen;
    char type = AG_TYPE_UNKNOWN;
    opaque fsdata;
    struct vnode *bdevp;

    icl_Trace3(xops_iclSetp, XAG_TRACE_ATTACH,
	       ICL_TYPE_LONG, parm2,
	       ICL_TYPE_LONG, parm3,
	       ICL_TYPE_LONG, parm4);

    astabp = (struct astab *) osi_AllocBufferSpace();
    code = osi_copyin((caddr_t)parm2, (caddr_t)astabp, sizeof (struct astab));
    if (code != 0) {
	icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_COPYIN,
		   ICL_TYPE_LONG, code);
	goto freebuf;
    }

    /* See if this aggregate is already attached. */
    if (!(parm3 & AGGR_ATTACH_NOEXPORT)) {
	aggrp = ag_GetAggr(astabp->as_aggrId);
	if (aggrp) {
	    if (strcmp(astabp->as_type, ASTABTYPE_UFS) == 0) {
		/*
		 * XXX - just return EEXIST for UFS aggregates.  calling
		 * ag_RegisterVolumes() on a UFS aggregate results in
		 * destroyed fileset id information.  This should be
		 * revisited in a future release to find a better solution.
		 */
		code = EEXIST;
	    } else {
		code = ag_RegisterVolumes(aggrp);
	    }
	    ag_PutAggr(aggrp);
	    icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_EXISTS_2,
		       ICL_TYPE_LONG, code);
	    goto freebuf;
	}
    }
    /*
     * Given the device name for the aggregate, get the device
     * number into 'dev', and a vnode for it into 'bdevp'.
     */
    if (code = osi_getvdev(astabp->as_spec, &dev, &bdevp)) {
	icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_BADGETV,
		   ICL_TYPE_LONG, code);
	goto freebuf;
    }

#ifdef SGIMIPS
    aggrp = ag_GetAggrByDev ((dev_t)dev);
#else
    aggrp = ag_GetAggrByDev (dev);
#endif
    if (aggrp) {
	if (parm3 & AGGR_ATTACH_NOEXPORT) {
	    code = ENOTDIR;	/* sort of like ``wrong type for object'' */
	    icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_NOEXP,
		       ICL_TYPE_LONG, code);
	    goto done;
	}
	if (aggrp->a_states & AGGR_EXPORTED) {
	    ag_PutAggr (aggrp);
	    code = EEXIST;
	    icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_EXPORTED,
		       ICL_TYPE_LONG, code);
	    goto done;
	}
	aggrp->a_states |= AGGR_EXPORTED;
	aggrp->a_aggrId = astabp->as_aggrId;
	strncpy (aggrp->a_aggrName, astabp->as_aggrName, MAX_AGGRNAME);
	ag_RegisterVolumes(aggrp);
	icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_REGGED,
		   ICL_TYPE_LONG, code);
done:
	OSI_VN_RELE (bdevp);
	goto freebuf;
    }

    ASTABTYPE_TO_AGTYPE(astabp->as_type, type);
#ifdef AFS_SUNOS5_ENV
    if (type == AG_TYPE_EPI) {
	/*
	 * Autoload Episode if it isn't present yet.
	 */
	if (agOpvec[type] == NULL) {
	    osi_RestorePreemption(0);
	    code = modload("fs", OSI_FSTYPE_LFS);
	    if (code == -1)
		printf("dfs: error loading %s kernel module\n",
		       OSI_FSTYPE_LFS);
	    (void) osi_PreemptionOff();
	}
    } else
    if (type == AG_TYPE_DMEPI) {
	/*
	 * Autoload DM-Episode if it isn't present yet.
	 */
	if (agOpvec[type] == NULL) {
	    osi_RestorePreemption(0);
	    code = modload("fs", OSI_FSTYPE_DMEPI);
	    if (code == -1)
		printf("dfs: error loading %s kernel module\n",
		       OSI_FSTYPE_DMEPI);
	    (void) osi_PreemptionOff();
	}
    } else
#endif /* AFS_SUNOS5_ENV */
    if (type == AG_TYPE_UNKNOWN) {
	/*
	 * Unrecognized aggregate type
	 * Only UFS or Episode for now
	 */
	OSI_VN_RELE(bdevp);
	icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_UNKNOWNTYPE,
		   ICL_TYPE_LONG, code);
	code = EINVAL;
	goto freebuf;
    }
    volreg_Init();
    xcred_Init();
    tkc_Init();
    if (agOpvec[type]) {
#ifdef SGIMIPS
	code = (*agOpvec[type]->ag_attach)(dev, bdevp, (int)parm3, (caddr_t)parm4,
#else
	code = (*agOpvec[type]->ag_attach)(dev, bdevp, parm3, (caddr_t)parm4,
#endif
					   &fsdata, &fsdatalen);
	icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_SUB,
		   ICL_TYPE_LONG, code);
    } else {
	code = ENOSYS;
	icl_Trace1(xops_iclSetp, XAG_TRACE_ATTACH_NOOPS,
		   ICL_TYPE_LONG, code);
    }
    if (!code) {
	code = ag_NewAggr(astabp->as_aggrName, astabp->as_aggrId, 
			  astabp->as_spec, type, agOpvec[type], dev, fsdata, 
#ifdef SGIMIPS
			  &aggrp, (int)parm3);
#else
			  &aggrp, parm3);
#endif
	if (code && fsdatalen) {
	    osi_Free (fsdata, fsdatalen);
	}
    }
    if (!code)
	aggrp->devvp = bdevp;
    else
	OSI_VN_RELE(bdevp);

  freebuf:
    osi_FreeBufferSpace((struct osi_buffer *)astabp);
    return code;    
}

/*
 * Return status info about a particular attached aggregate
 */
#ifdef SGIMIPS
static int ag_aggrInfo(long parm2,
    			long parm3)
#else
static ag_aggrInfo(parm2, parm3)
    long parm2, parm3;
#endif
{
    register struct aggr *aggrp;
    struct ag_status status;
    struct aggrInfo aggrInfo;
#ifdef SGIMIPS
    register int code = ENOENT;
#else
    register long code = ENOENT;
#endif

    icl_Trace1(xops_iclSetp, XAG_TRACE_AGGRINFO,
	       ICL_TYPE_LONG, parm2);
#ifdef SGIMIPS
    if (aggrp = ag_GetAggr((__int32_t)parm2)) {
#else
    if (aggrp = ag_GetAggr(parm2)) {
#endif
	code = AG_STAT(aggrp, &status);
	if (!code) {
	    bzero((caddr_t)&aggrInfo, sizeof(aggrInfo));
	    strncpy(aggrInfo.name, status.ag_st.aggrName, sizeof(aggrInfo.name));
	    strncpy(aggrInfo.devName, status.ag_st.devName, sizeof(aggrInfo.devName));
	    aggrInfo.type = status.ag_st.type;
	    aggrInfo.totalUsable = status.ag_dy.totalUsable;
	    aggrInfo.realFree = status.ag_dy.realFree;
	    aggrInfo.minFree = status.ag_dy.minFree;
	    code = osi_copyout((caddr_t)&aggrInfo,
			       (caddr_t)parm3, sizeof (struct aggrInfo));
	}
	ag_PutAggr(aggrp);
    }
    icl_Trace1(xops_iclSetp, XAG_TRACE_AGGRINFO_END,
	       ICL_TYPE_LONG, code);
    return code;
}


/*
 * Return minimal information about all attached aggregates on this server.
 */
#ifdef SGIMIPS
static ag_aggrEnumerate(
    long parm2, long parm3, long parm4)
#else
static ag_aggrEnumerate(parm2, parm3, parm4)
    long parm2, parm3, parm4;
#endif
{
    register struct aggr *aggrp;
    struct aggrList *aggrListp;
#ifdef SGIMIPS
    long i = 0;
    int code;

    /* outlen redfined as signed32, to match the caller parameters, 
     * so that osi_Copyout (with sizeof(outlen))  will not corrupt the stack.
     */
    signed32 outlen; 
#else
    long i = 0, outlen, code;
#endif /* SGIMIPS */

    ag_Init();
    icl_Trace3(xops_iclSetp, XAG_TRACE_AGGRENUMERATE,
	       ICL_TYPE_LONG, parm2,
	       ICL_TYPE_LONG, parm3,
	       ICL_TYPE_LONG, parm4);
    lock_ObtainRead(&ag_lock);					/* XXXX */
#ifdef SGIMIPS
    /* ag_attached is the number of attached aggregates, which better
	not be bigger than 32bits.  */
    if (parm2 < (outlen = (int)(ag_attached * sizeof(struct aggrList)))) {
#else
    if (parm2 < (outlen = ag_attached * sizeof(struct aggrList))) {
#endif
	/*
	 * Must have supplied at least one int of space
	 */
	lock_ReleaseRead(&ag_lock);
	code = osi_copyout((caddr_t)&outlen, (caddr_t)parm3, sizeof (outlen));
	if (code == 0) {
	    code = E2BIG;	/* Send the proper size back */
	}
	icl_Trace3(xops_iclSetp, XAG_TRACE_AGGRENUMERATE_TOOSMALL,
		   ICL_TYPE_LONG, outlen,
		   ICL_TYPE_LONG, ag_attached,
		   ICL_TYPE_LONG, code);
	return code;
    }
    aggrListp = (struct aggrList *) osi_Alloc(outlen);
    for (aggrp = ag_root; aggrp; aggrp = aggrp->a_next, i++) {
	strncpy(aggrListp[i].name, aggrp->a_aggrName, sizeof(aggrListp[i].name));
	strncpy(aggrListp[i].dvname, aggrp->a_devName, sizeof(aggrListp[i].dvname));
	aggrListp[i].Id = aggrp->a_aggrId;
	aggrListp[i].type = aggrp->a_type;
    }
    lock_ReleaseRead(&ag_lock);
    code = osi_copyout((caddr_t)aggrListp, (caddr_t)parm3, outlen);
    if (!code) {
	code = osi_copyout((caddr_t)&outlen, (caddr_t)parm4, sizeof(outlen));
    }
    osi_Free((opaque)aggrListp, outlen);
    icl_Trace1(xops_iclSetp, XAG_TRACE_AGGRENUMERATE_END,
	       ICL_TYPE_LONG, code);
    return code;
}


/*
 * Return all attached volumes associated with a particular aggregate
 */
#ifdef SGIMIPS
static ag_volEnumerate(
    long parm2, long parm3, long parm4, long parm5)
#else
static ag_volEnumerate(parm2, parm3, parm4, parm5)
    long parm2, parm3, parm4, parm5;
#endif
{
    register struct aggr *aggrp;
    struct volume vol;
    struct io_rwDesc ioDesc;
#ifdef SGIMIPS
    struct io_rwDesc_aligned {
	int	descId;
	int  	position;
	int  	length;
	int	bufferp;	/* Defined as 32 bits because it is a 
				   pointer from a 32bit application.  
				   This allows us to pick up the pointer 
				   value and stuff it into 64 bits. */
     };
     struct io_rwDesc_aligned *user_rwDesc; 
     long temp;
#endif
#ifdef SGIMIPS
    long max = parm2;
    int code;
    /* these 2 variables defined as signed32, to match the caller parameter 
     * type, so that osi_copyout doesn't corrupt the stack.
     */
    signed32  index, outlen;
#else
    long code, index, max = parm2, outlen;
#endif /* SGIMIPS */
    char *bufferp;
    struct vol_status vsbuff;

    icl_Trace4(xops_iclSetp, XAG_TRACE_VOLENUMERATE,
	       ICL_TYPE_LONG, parm2,
	       ICL_TYPE_LONG, parm3,
	       ICL_TYPE_LONG, parm4,
	       ICL_TYPE_LONG, parm5);
#ifdef SGIMIPS64
    /* Copy the fields into our ioDesc, but bufferp is only 32bits */
    code = osi_copyin((caddr_t)parm3,
		      (caddr_t)&ioDesc, sizeof(struct io_rwDesc_aligned));
    user_rwDesc = (struct io_rwDesc_aligned *)&ioDesc;
    temp = (long)user_rwDesc->bufferp;   /* Get the 32bits in a register */
    ioDesc.bufferp = (char *)temp;		   /* Put it back as 64bits. */
#else
    code = osi_copyin((caddr_t)parm3,
		      (caddr_t)&ioDesc, sizeof (struct io_rwDesc));
#endif
    if (code != 0) {
	icl_Trace1(xops_iclSetp, XAG_TRACE_VOLENUMERATE_COPYIN,
	       ICL_TYPE_LONG, code);
	return code;
    }
    if (aggrp = ag_GetAggr(ioDesc.descId)) {
	if (max <= 0 || max > aggrp->a_nVolumes)
	    max = aggrp->a_nVolumes;		/* assume all volumes */
	icl_Trace2(xops_iclSetp, XAG_TRACE_VOLENUMERATE_GOING,
		   ICL_TYPE_LONG, aggrp->a_aggrId,
		   ICL_TYPE_LONG, max);
#ifdef SGIMIPS
	if ((outlen = (int)(max * sizeof(struct vol_status))) > ioDesc.length) {
#else
	if ((outlen = max * sizeof(struct vol_status)) > ioDesc.length) {
#endif
	    /* Must have supplied at least one int of space */
	    ag_PutAggr(aggrp);
	    if (ioDesc.length < sizeof(outlen))
		code = ENOSPC;		/* Need some space for outlen! */
	    if (!code)
		code = osi_copyout((caddr_t)&outlen,
				   ioDesc.bufferp, sizeof (outlen));
	    if (!code)
		code = E2BIG;		/* Send the proper size back */
	    icl_Trace3(xops_iclSetp, XAG_TRACE_VOLENUMERATE_TOOBIG,
		       ICL_TYPE_LONG, outlen,
		       ICL_TYPE_LONG, max,
		       ICL_TYPE_LONG, code);
	    return code;
	}
	index = ioDesc.position;
	bufferp = ioDesc.bufferp;
	for (; max > 0; index++) {
	    bzero((caddr_t)&vol, sizeof(struct volume));
	    if (code = AG_VOLINFO(aggrp, index, &vol)) {
		icl_Trace2(xops_iclSetp, XAG_TRACE_VOLENUMERATE_VOLINFOERR,
			   ICL_TYPE_LONG, index,
			   ICL_TYPE_LONG, code);
		if (code == ENOENT) continue;
		ag_PutAggr(aggrp);
		if (code == VOL_ERR_EOF) {
#ifdef SGIMIPS
		    /* Since the buffer pointer comes from an n32 
			application, it will fit in 32bits fine. */
		    outlen =  (int)(bufferp - ioDesc.bufferp);
#else
		    outlen =  bufferp - ioDesc.bufferp;
#endif
		    code = osi_copyout((caddr_t)&outlen,
					(caddr_t)parm5, sizeof (outlen));
		    if (!code)
			code = VOL_ERR_EOF;
		}
		icl_Trace2(xops_iclSetp, XAG_TRACE_VOLENUMERATE_DONE,
			   ICL_TYPE_LONG, index,
			   ICL_TYPE_LONG, code);
		return code;
	    }
	    max--;
	    /*
	     * XXX For expendiency we pass back vol_status data and let the
	     * calling user process sort out the data... XXX
	     */
	    code = VOL_GETSTATUS(&vol, &vsbuff);
	    if (code == 0)
		code = osi_copyout((caddr_t)&vsbuff,
				   bufferp, sizeof(struct vol_status));
	    if (code) {
		ag_PutAggr(aggrp);
		icl_Trace2(xops_iclSetp, XAG_TRACE_VOLENUMERATE_GETSTS,
			   ICL_TYPE_LONG, index,
			   ICL_TYPE_LONG, code);
		return code;
	    }
	    bufferp += sizeof(struct vol_status);
	}
	ag_PutAggr(aggrp);
	code = osi_copyout((caddr_t)&index, (caddr_t)parm4, sizeof (index));
	if (!code) {
#ifdef SGIMIPS
	    /* Since the buffer pointer comes from an n32 
		application, it will fit in 32bits fine. */
	    outlen =  (int)(bufferp - ioDesc.bufferp);
#else
	    outlen =  bufferp - ioDesc.bufferp;
#endif
	    code = osi_copyout((caddr_t)&outlen, (caddr_t)parm5, sizeof (outlen));
	}
	icl_Trace1(xops_iclSetp, XAG_TRACE_VOLENUMERATE_MIDEND,
			   ICL_TYPE_LONG, code);
	return code;
    }
    icl_Trace1(xops_iclSetp, XAG_TRACE_VOLENUMERATE_NOAGGR,
	       ICL_TYPE_LONG, ioDesc.descId);
    return ENODEV;				/* aggr not found */
}

/*
 * Register all volumes in an aggregate.  Note that we currently don't
 * return code on failure of individual volume attaches.
 * Return EEXIST if all vol_Attach calls returned EEXIST, else zero.
 */
int ag_RegisterVolumes(struct aggr *aggrp)
{
    int i;
    int code, nonExist;
    struct vol_status *vstatp;
    struct volume *volp;

    icl_Trace1(xops_iclSetp, XAG_TRACE_REGISTERVOLUMES,
		   ICL_TYPE_LONG, aggrp->a_aggrId);

    vstatp = (struct vol_status *)osi_Alloc(sizeof(struct vol_status));
    volp = (struct volume *)osi_Alloc(sizeof(struct volume));

    nonExist = 0;
    for (i = 0;; i++) {
	bzero((caddr_t)volp, sizeof(struct volume));
	code = AG_VOLINFO(aggrp, i, volp);
	icl_Trace3(xops_iclSetp, XAG_TRACE_REGISTERVOLUMES_INFO,
		   ICL_TYPE_LONG, i,
		   ICL_TYPE_HYPER, &volp->v_stat_st.volId,
		   ICL_TYPE_LONG, code);
	if (code == VOL_ERR_EOF)/* past end of table */
	    break;
	else if (code != 0) {
	    /* We get ENOENT if the slot is unused.  All else is an error. */
	    if (code != ENOENT)
		printf("dfs: error attaching fileset at index %u of aggr %s (%u): %u\n",
		       i, aggrp->a_aggrName, aggrp->a_aggrId, code);
	    continue;
	}
	/* Prepare to manage these volume status bits */
	volp->v_stat_st.states &= ~(VOL_BITS_INITIALMASK);
	volp->v_stat_st.states |= (VOL_BITS_INITIAL);
	bzero((caddr_t)vstatp, sizeof (struct vol_status));
	vstatp->vol_st = volp->v_stat_st;
	vstatp->vol_dy.index = i;
	strncpy(vstatp->vol_st.volName, volp->v_volName, VOLNAMESIZE);
	code = vol_Attach(&volp->v_volId, vstatp, aggrp, volp->v_volOps);
	icl_Trace3(xops_iclSetp, XAG_TRACE_REGISTERVOLUMES_ATTACH,
		   ICL_TYPE_HYPER, &volp->v_stat_st.volId,
		   ICL_TYPE_LONG, vstatp->vol_st.states,
		   ICL_TYPE_LONG, code);
	if (code != EEXIST)
	    nonExist = 1;
	if (code != 0) {
	    if (code != EEXIST)
		printf("dfs: error attaching fileset %u,,%u on aggr %s (%u): %u\n",
		       AFS_HGETBOTH(volp->v_volId),
		       aggrp->a_aggrName, aggrp->a_aggrId, code);
	} else if (vstatp->vol_st.states & VOL_DELONSALVAGE) {
	    /* Just print a warning if it's inconsistent */
	    printf("dfs: attached inconsistent fileset %u,,%u on aggr %s (%u)\n",
		   AFS_HGETBOTH(volp->v_volId),
		   aggrp->a_aggrName, aggrp->a_aggrId);
	}
    }
    icl_Trace0(xops_iclSetp, XAG_TRACE_REGISTERVOLUMES_DONE);
    osi_Free(vstatp, sizeof(struct vol_status));
    osi_Free(volp, sizeof(struct volume));
    return (nonExist ? 0 : EEXIST);
}

/*
 * Remove all volumes of an aggregate from the volume registry.
 *
 * Don't know how we should deal with failures of individual volumes;
 * for now we return an error code.
 */
static int ag_UnRegisterVolumes (struct aggr *aggrp, int anyForce)
{
    int i;
    int code, code2;
    int volDesc, openError;
    struct vol_calldesc *cdp;
    struct vol_handle vol_handle;
    vol_vp_elem_p_t vpList;
    int didDetach = 0;

    icl_Trace1(xops_iclSetp, XAG_TRACE_UNREGISTERVOLUMES,
		   ICL_TYPE_LONG, aggrp->a_aggrId);
    for (i = 0;; i++) {
	struct volume vol;		/* placeholder volume for VOLINFO */
	struct volume *volp;		/* real volume from registry */
	struct afsFid fid;		/* fid to pass to registry */

	bzero((caddr_t)&vol, sizeof(struct volume));
	code = AG_VOLINFO (aggrp, i, &vol);
	icl_Trace3(xops_iclSetp, XAG_TRACE_UNREGISTERVOLUMES_INFO,
		   ICL_TYPE_LONG, i,
		   ICL_TYPE_HYPER, &vol.v_stat_st.volId,
		   ICL_TYPE_LONG, code);
	if (code == VOL_ERR_EOF) {
	    code = 0;
	    break;
	}
	if (code)
	    continue;
	bzero ((caddr_t)&fid, sizeof(struct afsFid));
	fid.Volume = vol.v_volId;
	if (code = volreg_Lookup(&fid, &volp)) {
	    icl_Trace2(xops_iclSetp, XAG_TRACE_UNREGISTERVOLUMES_VOLREG,
		       ICL_TYPE_HYPER, &vol.v_stat_st.volId,
		       ICL_TYPE_LONG, code);
	    if (code == ENODEV)		/* allow volume to be unattached */
		continue;
	    else
		break;
	}
	lock_ObtainRead(&volp->v_lock);
	if (volp->v_states & VOL_BUSY) {
	    lock_ReleaseRead(&volp->v_lock);
	    code = EBUSY;
	    VOL_RELE(volp);
	    icl_Trace2(xops_iclSetp, XAG_TRACE_UNREGISTERVOLUMES_BUSY,
		       ICL_TYPE_HYPER, &vol.v_stat_st.volId,
		       ICL_TYPE_LONG, code);
	    break;
	}
	lock_ReleaseRead(&volp->v_lock);

	/*
	 * We need to wait for existing vnode ops to finish and keep new ones
	 * from starting, so we call vol_open() to do all of the dirty work.
	 */
	code = vol_open(&vol.v_volId, aggrp->a_aggrId,
			VOL_SYS_DETACH, VOLERR_PERS_DETACH,
			&volDesc, &openError, 1);
	if (code) {
	    VOL_RELE(volp);
	    icl_Trace2(xops_iclSetp, XAG_TRACE_UNREGISTERVOLUMES_NOOPEN,
		       ICL_TYPE_HYPER, &vol.v_stat_st.volId,
		       ICL_TYPE_LONG, code);
	    break;
	}

	code2 = vol_FindDesc(volDesc, &cdp);
	MBZ(code2);

	vol_handle = cdp->handle;
	VOL_HOLD(vol_handle.volp);

	vol_SetDeleted(cdp);
	vol_PutDesc(cdp);

	code = vol_Detach(volp, anyForce);	/* Do the detach */
	icl_Trace2(xops_iclSetp, XAG_TRACE_UNREGISTERVOLUMES_DETACH,
		   ICL_TYPE_HYPER, &vol.v_stat_st.volId,
		   ICL_TYPE_LONG, code);

	code2 = VOL_CLOSE(volp, &vol_handle, 0);

	lock_ObtainWrite(&volp->v_lock);
	if (!code) {
	    /*
	     * Set DEADMEAT so that waiting vnode ops will notice that this
	     * thing is gone.
	     */
	    volp->v_states |= VOL_DEADMEAT;
	}
	(void) vol_close(volp, NULL);
	lock_ReleaseWrite(&volp->v_lock);

	if (!code)
	    code = code2;

	VOL_RELE(volp);			/* Once for the volreg_Lookup */
	VOL_RELE(vol_handle.volp);	/* Once for the vol_handle copy */
	icl_Trace2(xops_iclSetp, XAG_TRACE_UNREGISTERVOLUMES_RELE,
		   ICL_TYPE_HYPER, &vol.v_stat_st.volId,
		   ICL_TYPE_LONG, code);

	if (code)
	    break;

	didDetach = 1;		/* We succeeeded in detaching this fileset */
    }
    
    /*
     * If we successfully detached at least one fileset but failed to detach
     * a subsequent one, we make a best effort attempt to bring the entire
     * aggregate back on line.
     */
    if (code && didDetach)
	(void)ag_RegisterVolumes(aggrp);

    return code;
}
