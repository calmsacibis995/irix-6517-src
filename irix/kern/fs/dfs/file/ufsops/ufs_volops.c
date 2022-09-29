/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: ufs_volops.c,v 65.7 1998/04/01 14:16:38 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

/*
 * Volume ops for UFS filesystems
 */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/osi_ufs.h>
#include <dcedfs/debug.h>
#include <dcedfs/lock.h>
#include <dcedfs/volume.h>
#include <dcedfs/vol_init.h>
#include <dcedfs/common_data.h>
#include <dcedfs/xvfs_export.h>
#include <dcedfs/aclint.h>
#include <dcedfs/vol_init.h>
#include <dcedfs/ufs_trace.h>
#include <ufs.h>
#ifdef SGIMIPS
#include <dcedfs/tkc.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/ufsops/RCS/ufs_volops.c,v 65.7 1998/04/01 14:16:38 gwehrman Exp $")

/* Invalid-afsFid creation and test macros (deleted-afsFid not supported) */

#define vol_MakeInvalidAfsFid(afidP) \
((afidP)->Vnode = 0, (afidP)->Unique = 0)

#define vol_IsInvalidAfsFid(afidP) \
(((afidP)->Vnode == 0) && ((afidP)->Unique == 0))

/* External-index to internal-index convertion macro */

#define UFSINVALIDINO   (0)

#define ExternalToUFSIndex(index) \
(((index) < VOL_ROOTINO) ? UFSINVALIDINO : \
 (((index) == VOL_ROOTINO) ? UFSROOTINO : \
  ((index) + VOL_UFSFIRSTINODE)))

#ifdef SGIMIPS
extern int ag_ufsStat(
	struct aggr *aggrp,
	struct ag_status *astatp
);
#endif

static int vol_ufsOpen(
    struct volume *volp,
    int type,
    int errorType,
    struct vol_handle *handlerp)
{
    struct ufs_volData *vsp;
    struct osi_vfs *vfsp;
    long code = 0;

    icl_Trace3(xufs_iclSetp, UFS_TRACE_ENTER_VOLOPEN,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, type,
	       ICL_TYPE_LONG, errorType);

    /* initialize iteration handle as required */
    handlerp->volp     = volp;
    handlerp->opentype = type;
    vol_MakeInvalidAfsFid(&(handlerp->fid));

    /* determine the level of concurrency */
    if ((type & VOL_ALLOK_OPS) == type)
	volp->v_concurrency = VOL_CONCUR_ALLOPS;
    else if ((type & VOL_READONLY_OPS) == type) /* all ops are read-only */
	volp->v_concurrency = VOL_CONCUR_READONLY;
    else				/* there are some read-write ops */
	volp->v_concurrency = VOL_CONCUR_NOOPS;
    icl_Trace1(xufs_iclSetp, UFS_TRACE_VOLOPEN_CONCUR,
	       ICL_TYPE_LONG, volp->v_concurrency);

#ifdef SGIMIPS
    /*
     * XXX-RAG
     * We do not need machine specific open because we are going to use
     * ufsOpen only for VOL_OP_DETACH and in that case it is not needed.
     */
#endif
#ifdef EXIST_MACH_VOLOPS
    code = vol_ufsOpen_mach(volp, type, errorType, handlerp);
#endif /* EXIST_MACH_VOLOPS */

#ifndef AFS_SUNOS5_ENV
    /*
     * We don't currently support extended volops on SUNOS5, so there is
     * no need to deal with this now.
     */
    if (code == 0) {
#ifdef AFS_HPUX_ENV
	/* Don't bother setting DELONSALVAGE.  It doesn't do much good since
	 * we can't really delete this volume.  It prevents the volume from
	 * being accessed at all (via the local mount or even for dismount)
	 * if anything goes wrong during the restore.
	 *
	 * Setting DELONSALVAGE makes sense in that it prevents the OS from
	 * stumbling on inconsistent on-disk structures that may exist
	 * following the partial-restore ... but even this is of limited
	 * usefullness since the flag is lost following a reboot.
	 */
#else
	/* Set inconsistent state if this is a fileset restoration */
	if (type & VOL_SETINCON_OPS)
	    volp->v_states |= VOL_DELONSALVAGE;
#endif /* AFS_HPUX_ENV */
	/* Save the current max position number */
	vsp = VOLTOUFS(volp);
	vfsp = UFS_AGTOVFSP(volp->v_paggrp);
	if (vsp)
	    vsp->maxInodeIx = UFSMAXINO(vfsp);
    }
#endif
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLOPEN, ICL_TYPE_LONG, code);
#ifdef SGIMIPS
    return (int)code;	/* Chopping off long okay because we don't even set 
				it to anything but 0 */
#else
    return code;
#endif
}


static int vol_ufsSeek(
    struct volume *volp,
    int position,
    struct vol_handle *handlerp)
{
    return ENOSYS;    /* obsolete function */
}


static int vol_ufsTell(
    struct volume *volp,
    struct vol_handle *handlerp,
    int *positionp)
{
    return ENOSYS;    /* obsolete function */
}


static int vol_ufsScan(
    struct volume *volp,
    int position,
    struct vol_handle *handlerp)
{
    struct osi_vfs *vfsp = UFS_AGTOVFSP(volp->v_paggrp);
    struct ufs_volData *vsp = VOLTOUFS(volp);
    long code = 0;
    long index;

    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLSCAN,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, position);
#ifdef SGIMIPS
    /*
     * XXX-RAG Not Supported
     */
    return(ENOSYS);
#else
    osi_assert(handlerp->volp == volp);

    index = ExternalToUFSIndex(position);

    if (index == UFSINVALIDINO) {
	code = EINVAL;
    } else if (index >
	       ((vsp ? vsp->maxInodeIx : UFSMAXINO(vfsp))-VOL_UFSFIRSTINODE)) {
	code = VOL_ERR_EOW;
    }


#ifdef EXIST_MACH_VOLOPS
    if (!code) {
	/* perform scan, setting handlerp->fid if successful */
	code = vol_ufsScan_mach(UFS_AGTOVFSP(volp->v_paggrp),
				index,
				handlerp);
    }
#else
#error	"vol_ufsScan: code required"
#endif /* EXIST_MACH_VOLOPS */

    if (code) {
	/* scan failed; set iteration handle fid to invalid */
	vol_MakeInvalidAfsFid(&(handlerp->fid));
    }
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLSCAN,
	       ICL_TYPE_LONG, code);
    return code;
#endif
}


static int vol_ufsClose(
    struct volume *volp,
    struct vol_handle *handlerp,
    int isabort)
{
    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLCLOSE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, isabort);
    /*
     * If this is finishing up a fileset restoration,
     * set the ``inconsistent'' analogue bit.
     */
    if (!isabort && (handlerp->opentype & VOL_SETINCON_OPS))
	volp->v_states &= ~VOL_DELONSALVAGE;
    icl_Trace0(xufs_iclSetp, UFS_TRACE_END_VOLCLOSE);
    return 0;
}

static int vol_ufsDeplete(struct volume *volp)
{
    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLDEPLETE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, volp->v_count);
    /*
     * XXX Remove ".VOLFILE", etc??? XXX
     */
    return EINVAL;
}


static int vol_ufsDestroy(struct volume *volp)
{
    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLDESTROY,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, volp->v_count);
    /*
     * XXX Remove ".VOLFILE", etc??? XXX
     */
    return EINVAL;
}


/*
 * Called after the attach is done, allowing us to register the volume's VFS
 * pointer for later.  For UFS aggregates, the vfs pointer is the aggregate's
 * private data; if this changes, we must fix this code, as well as detach's.
 */
static vol_ufsAttach(struct volume *volp)
{
    struct aggr *ta;
    struct osi_vfs *vfsp;

    lock_ObtainRead(&volp->v_lock);
    ta = volp->v_paggrp;
    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLATTACH,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, ta);
    if (!ta) {
	lock_ReleaseRead(&volp->v_lock);
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLATTACH_TA,
		   ICL_TYPE_LONG, EINVAL);
	return EINVAL;
    }
    vfsp = UFS_AGTOVFSP(ta);
    lock_ReleaseRead(&volp->v_lock);
    vol_AddVfs(vfsp, volp);
#ifndef AFS_OSF_ENV
    vfsp->osi_vfs_op =
	(struct osi_vfsops *)
#ifdef AFS_IRIX_ENV
	    GetNewVfsOpsFromOld (vfsp->osi_vfs_op, xufs_ops.vn_getvolume);
#else    
	    GetNewVfsOpsFromOld (vfsp->osi_vfs_op, xufs_vfsgetvolume);
#endif /* AFS_IRIX_ENV */
#endif
    icl_Trace0(xufs_iclSetp, UFS_TRACE_END_VOLATTACH);
    return 0;
}


#ifdef SGIMIPS
/*
 * XXX-RAG
 * This code should be under premption off to ensure atomicity of
 * tokenflush & remove ops. VERIFY
 * Or there should be some provision to prohibit new tokens.
 * All vnode ops (px & tk) call vol_StartVnodeOp which checks for compatibility.
 * compatibity (v_concu??) is set by vol_open->vol_ufsOpen
 */
#endif
static int vol_ufsDetach(struct volume *volp, int anyForce)
{
    int code;
    struct osi_vfs *vfsp;

    icl_Trace1(xufs_iclSetp, UFS_TRACE_ENTER_VOLDETACH, ICL_TYPE_POINTER, volp);
    lock_ObtainWrite(&volp->v_lock);
    vfsp = UFS_AGTOVFSP(volp->v_paggrp);
    lock_ReleaseWrite(&volp->v_lock);
    if ((code = tkc_flushvfsp(vfsp)) == 0) {
	vol_RemoveVfs (volp);
    }
    else if (code < 0) {
	code = EBUSY;
    }
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLDETACH,
	       ICL_TYPE_LONG, code);
    return code;
}


/*
 * This routine is called with the volume read-locked.  We must prevent
 * a deadlock when calling ag_ufsStat(), which triggers a call chain
 * like: ... -> xglue_root -> ... -> vol_FindVfs -> ... -> vol_ufsHold -> ...
 * To avoid deadlock, we drop the lock before calling ag_ufsStat() and
 * reobtain it after; however, we must be careful not to look at any of the
 * data after reobtaining the lock, since it may have changed.
 */
static int vol_ufsGetStatus(struct volume *volp, struct vol_status *statusp)
{
    struct ufs_volData *vsp = VOLTOUFS(volp);
    struct ag_status agstat;
    struct vol_stat_dy *vsdp;
    struct aggr *aggrp;
    afs_hyper_t size, usage;
    int code;
#ifdef SGIMIPS
    long tmp;
#endif /* SGIMIPS */

    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLGETSTATUS,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, statusp);
    statusp->vol_st = volp->v_stat_st;
    if (vsp != 0) {
	vsdp = &vsp->ufsData;

	vsdp->nodeMax = vsp->maxInodeIx - VOL_UFSFIRSTINODE;

	statusp->vol_dy = *vsdp;
	statusp->vol_dy.aggrId = volp->v_paggrp->a_aggrId;

	/*
	 * For UFS, a volume maps to an aggregate and vice versa, so
	 * get the aggregate status for convenience
	 */
	aggrp = volp->v_paggrp;	/* Stash the aggrp before dropping the lock */
	lock_ReleaseRead(&volp->v_lock);
	code = ag_ufsStat(aggrp, &agstat);
	lock_ObtainRead(&volp->v_lock);
	if (code) {
	    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLGETSTATUS_UFSSTAT,
		       ICL_TYPE_LONG, code);
	    return code;
	}

#ifdef SGIMIPS
	/*
         * XXX-RAG
         * Is there an assumption about block size ??
	 * It is already converted to be base 1k in ag_ufsStat
         */
#endif
	/* Convert 1K blocks to bytes */
#ifdef SGIMIPS
	AFS_hset64(size, AFS_hgethi(agstat.ag_dy.totalUsable), 
		AFS_hgetlo(agstat.ag_dy.totalUsable));
	AFS_hleftshift(size, 10);
	tmp = agstat.ag_dy.totalUsable - agstat.ag_dy.realFree;
	AFS_hset64(usage, AFS_hgethi(tmp), AFS_hgetlo(tmp));
	AFS_hleftshift(usage, 10);
#else  /* SGIMIPS */
	AFS_hset64(size, 0, agstat.ag_dy.totalUsable);
	AFS_hleftshift(size, 10);
	AFS_hset64(usage, 0, agstat.ag_dy.totalUsable - agstat.ag_dy.realFree);
	AFS_hleftshift(usage, 10);
#endif /* SGIMIPS */

	if (AFS_hiszero(statusp->vol_dy.allocLimit)) {
	    statusp->vol_dy.allocLimit = size;
	    statusp->vol_dy.allocUsage = usage;
	}

	if (AFS_hiszero(statusp->vol_dy.visQuotaLimit)) {
	    statusp->vol_dy.visQuotaLimit = size;
	    statusp->vol_dy.visQuotaUsage = usage;
	}
    } else {
	bzero((char *)&(statusp->vol_dy), sizeof(struct vol_stat_dy));
    }

    icl_Trace0(xufs_iclSetp, UFS_TRACE_END_VOLGETSTATUS);
    return 0;
}


#ifdef SGIMIPS
/*
 * XXX-RAG
 * This code should NOT set vfs stat. With non-LFS FS there is no
 * fileset header anyway. fileset stat is derived from vfs-stat.
 * VERIFY that it is only setting incore struct.
 */
#endif
static int vol_ufsSetStatus(
    struct volume *volp,
    int mask,
    struct vol_status *statusp)
{
    struct ufs_volData *vsp = VOLTOUFS(volp);
    struct vol_stat_dy *vsdp = (vsp ? &vsp->ufsData : (struct vol_stat_dy *)0);
    int err = 0, code = 0;
#ifdef SGIMIPS
    unsigned int  newstates;
#else
    unsigned long newstates;
#endif

    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLSETSTATUS,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, mask);
    /*
     * XXX We, of course, should NOT allow callers to change certain fields such
     * as its size, update/access dates, various state flags, etc XXX
     */
#ifdef	notdef
    if (mask & VOL_STAT_VOLID)
	volp->v_volId = statusp->vol_st.volId;
#endif
    if (mask & VOL_STAT_INDEX)
	if (vsdp)
	    vsdp->index = statusp->vol_dy.index;
	else
	    err = 1;
    if (mask & VOL_STAT_OWNER)
	if (vsdp)
	    vsdp->owner = statusp->vol_dy.owner;
	else
	    err = 1;
    if (mask & VOL_STAT_TYPE)
	volp->v_voltype = statusp->vol_st.type;
    if (mask & VOL_STAT_VERSION)
	if (vsdp)
	    vsdp->volversion = statusp->vol_dy.volversion;
	else
	    err = 1;
    if (mask & VOL_STAT_UNIQUE)
	if (vsdp)
	    vsdp->unique = statusp->vol_dy.unique;
	else
	    err = 1;
#ifdef	notdef
    if (mask & VOL_STAT_VOLNAME)
	bcopy(statusp->vol_st.volName, volp->v_volName,
	      sizeof(statusp->vol_st.volName));
#endif
    if (mask & VOL_STAT_CREATEDATE)
	if (vsdp)
	    vsdp->creationDate = statusp->vol_dy.creationDate;
	else
	    err = 1;
    if (mask & VOL_STAT_UPDATEDATE)
	if (vsdp)
	    vsdp->updateDate = statusp->vol_dy.updateDate;
	else
	    err = 1;
    if (mask & VOL_STAT_ACCESSDATE)
	if (vsdp)
	    vsdp->accessDate = statusp->vol_dy.accessDate;
	else
	    err = 1;
    if (mask & VOL_STAT_BACKVOLINDEX)
	if (vsdp)
	    vsdp->backupIndex = statusp->vol_dy.backupIndex;
	else
	    err = 1;
    if (mask & VOL_STAT_BACKUPID)
	if (vsdp)
	    vsdp->backupId = statusp->vol_dy.backupId;
	else
	    err = 1;
    if (mask & VOL_STAT_PARENTID)
	volp->v_parentId = statusp->vol_st.parentId;
    if (mask & VOL_STAT_CLONEID)
	if (vsdp)
	    vsdp->cloneId = statusp->vol_dy.cloneId;
	else
	    err = 1;
    if (mask & VOL_STAT_LLBACKID)
	if (vsdp)
	    vsdp->llBackId = statusp->vol_dy.llBackId;
	else
	    err = 1;
    if (mask & VOL_STAT_LLFWDID)
	if (vsdp)
	    vsdp->llFwdId = statusp->vol_dy.llFwdId;
	else
	    err = 1;
    if (mask & VOL_STAT_MINQUOTA)
	if (vsdp)
	    vsdp->minQuota = statusp->vol_dy.minQuota;
	else
	    err = 1;
    if (mask & VOL_STAT_ALLOCLIMIT)
	if (vsdp)
	    vsdp->allocLimit = statusp->vol_dy.allocLimit;
	else
	    err = 1;
    if (mask & VOL_STAT_VISLIMIT)
	if (vsdp)
	    vsdp->visQuotaLimit = statusp->vol_dy.visQuotaLimit;
	else
	    err = 1;
    if (mask & VOL_STAT_VOLMOVETIMEOUT)
	volp->v_stat_st.volMoveTimeout = statusp->vol_st.volMoveTimeout;
    if (mask & VOL_STAT_NODEMAX) {
	/* This isn't a status-setting op; it's a check on the table size. */
	if ((statusp->vol_dy.nodeMax + VOL_UFSFIRSTINODE) > vsp->maxInodeIx)
	    code = ENFILE;	/* too many slots for this UFS */
    }
    if (mask & VOL_STAT_STATUSMSG)
	if (vsdp)
	    bcopy(statusp->vol_dy.statusMsg, vsdp->statusMsg,
		  sizeof(vsdp->statusMsg));
	else
	    err = 1;
    if (mask & VOL_STAT_STATES) {
	/* Set only those bits that the kernel doesn't maintain itself. */
	/* New values for the user bits: */
	newstates = statusp->vol_st.states & ~(VOL_BITS_NOSETSTATUS);
	/* Old values for the kernel bits: */
	newstates |= volp->v_states & VOL_BITS_NOSETSTATUS;
	volp->v_states = newstates;
    }
#ifdef notdef
    /*
     * Not supported for now: COPYDATE, CLONETIME,
     *  VVCURRTIME,VVPINGTIME, BACKUPDATE, RECLAIMDALLY
     */
#endif /* notdef */

    if (!code && err)
	code = ESRCH;
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLSETSTATUS,
	       ICL_TYPE_LONG, code);
    return (code);
}


static int vol_ufsCreate(
    struct volume *volp,
    int position,
    struct xvfs_attr *xvattrp,
    struct vol_handle *handlerp,
    osi_cred_t *credp)
{
#ifdef SGIMIPS
    /*
     * XXX-RAG Not Supported
     */
    return(ENOSYS);
#else
    int imode, errorCode = 0;
    struct vattr *vattrp = &xvattrp->vattr;
    struct osi_vfs *vfsp = UFS_AGTOVFSP(volp->v_paggrp);
    struct inode *ip;
    struct ufs_volData *vsp = VOLTOUFS(volp);
    long index;

    icl_Trace3(xufs_iclSetp, UFS_TRACE_ENTER_VOLCREATE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, position,
	       ICL_TYPE_POINTER, xvattrp);

    /*
     * If the object exists, delete it first.
     */
    errorCode = vol_ufsScan(volp, position, handlerp);

    if (errorCode == 0) {
	/* Object exists, so delete it. */
	if (errorCode = vol_ufsDelete(volp, &handlerp->fid, credp)) {
	    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLCREATE_DEL,
		       ICL_TYPE_LONG, errorCode);
	    goto done;
	}
    } else if (errorCode != ENOENT) {
	/* Scan failed for reason other than non-existent object */
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLCREATE_SCAN,
		   ICL_TYPE_LONG, errorCode);
	goto done;
    }

#ifdef EXIST_MACH_VOLOPS
    /* perform create, setting handlerp->fid if successful */
    index = ExternalToUFSIndex(position); /* checked above by vol_ufsScan() */

    errorCode = vol_ufsCreate_mach(UFS_AGTOVFSP(volp->v_paggrp),
				   index,
				   xvattrp,
				   handlerp,
				   credp);
#else /* EXIST_MACH_VOLOPS */
#error	"vol_ufsCreate: requires code"
#endif /* EXIST_MACH_VOLOPS */

    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLCREATE,
	       ICL_TYPE_LONG, errorCode);
  done:
    if (errorCode) {
	/* create failed; set iteration handle fid to invalid as required */
	vol_MakeInvalidAfsFid(&(handlerp->fid));
    }
    return errorCode;
}

static vol_rdwr(vp, rwFlag, position, length, bufferp, credp, outLenp)
    register struct vnode *vp;
    enum uio_rw rwFlag;
    afs_hyper_t position;
    int length;
    char *bufferp;
    osi_cred_t *credp;
    int *outLenp;
{
    long errorCode = 0;

#ifdef EXIST_MACH_VOLOPS
    errorCode = vol_rdwr_mach(vp, rwFlag, position, length,
	bufferp, credp, outLenp);
#else
    struct vattr vattr;
    long tlen;
    afs_hyper_t htlen, hdeslen;
    struct uio tuio;
    struct iovec iov;
    long inlen = length;

    if (vp->v_type != VREG && vp->v_type != VLNK && vp->v_type != VDIR)
	return EINVAL;

    if (rwFlag == UIO_READ) {
	if (errorCode = VOPX_GETATTR(vp, (struct xvfs_attr *)&vattr, 0, credp))
	    return errorCode;
	tlen = vattr.va_size;

	/* xxx if va_size is 64-bits, change the following line: xxx */
#ifdef SGIMIPS
	AFS_hset64(htlen, AFS_hgethi(tlen), AFS_hgetlo(tlen));
#else  /* SGIMIPS */
	AFS_hset64(htlen, 0, tlen);
#endif /* SGIMIPS */

	if (AFS_hcmp(position, htlen) > 0) { /* position > tlen? */
	    inlen = 0;		/* don't read past end of file */
	} else {
	    hdeslen = position;
#ifdef SGIMIPS
	    AFS_hadd(hdeslen, inlen);
#else  /* SGIMIPS */
	    AFS_hadd32(hdeslen, inlen);
#endif /* SGIMIPS */

	    if (AFS_hcmp(hdeslen, htlen) > 0) { /*  position + inlen > tlen ? */
		AFS_hsub(htlen, position); /* inlen = tlen - position  */
#ifdef SGIMIPS 
		AFS_hset64(inlen, AFS_hgethi(htlen), AFS_hgetlo(htlen));
#else  /* SGIMIPS */
		inlen = AFS_hgetlo(htlen); /* NOTE: htlen must now fit 32bits,
					    * 'cuz inlen was 32bits to start */
#endif /* SGIMIPS */
	    }
	}
    } /* UIO_READ */

    /*
     * Setup uio vector
     */
    tuio.osi_uio_iov = &iov;
    tuio.osi_uio_iovcnt = 1;
    if (osi_uio_set_offset(tuio, position))
      return EINVAL;

    tuio.osi_uio_resid = inlen;
    tuio.osi_uio_seg = OSI_UIOUSER;
    iov.iov_base = bufferp;
    iov.iov_len = inlen;

    if ((vp->v_type == VLNK) && (rwFlag == UIO_READ)) {
	errorCode = VOPX_READLINK(vp, &tuio, credp);
	*outLenp = inlen - tuio.osi_uio_resid;
    } else {
	errorCode = VOPX_RDWR(vp, &tuio, rwFlag, OSI_FSYNC, credp);
	*outLenp = inlen - tuio.osi_uio_resid;
#ifdef	notdef		/* XXX */
	if (!errorCode && (inlen != *outLenp))
	    errorCode = EIO;		/* Didn't write the desired amount */
#endif	/* notdef */
    }
#endif /* EXIST_MACH_VOLOPS */
    return errorCode;

#endif
}


static int vol_ufsRead(
    struct volume *volp,
    struct afsFid *Fidp,
    afs_hyper_t position,
    int length,
    char *bufferp,
    osi_cred_t *credp,
    int *outLenp)
{
#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    struct vnode *vp;
    long errorCode;

    icl_Trace4(xufs_iclSetp, UFS_TRACE_ENTER_VOLREAD,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_HYPER, &position, ICL_TYPE_LONG, length);
    if (errorCode = VOL_VGET(volp, Fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLREAD_VGET,
		   ICL_TYPE_LONG, errorCode);
	return errorCode;
    }
    errorCode = vol_rdwr(vp, UIO_READ, position, length, bufferp,
			  credp, outLenp);
    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLREAD,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
#endif
}

static int vol_ufsWrite(
    struct volume *volp,
    struct afsFid *Fidp,
    afs_hyper_t position,
    int length,
    char *bufferp,
    osi_cred_t *credp)
{
    struct vnode *vp;
    long outlen;
    long errorCode;

#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    icl_Trace4(xufs_iclSetp, UFS_TRACE_ENTER_VOLWRITE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_HYPER, &position, ICL_TYPE_LONG, length);

    if (errorCode = VOL_VGET(volp, Fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLREAD_VGET,
		   ICL_TYPE_LONG, errorCode);
	return errorCode;
    }

    errorCode = vol_rdwr(vp, UIO_WRITE, position, length, bufferp,
			  credp, &outlen);

    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLWRITE,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
#endif
}

static int vol_ufsReadHole(volp, Fidp, readHoleP, credp)
  struct volume *volp;			/* volume */
  struct afsFid *Fidp;			/* file ID */
  struct vol_ReadHole *readHoleP;	/* read range, buffer etc */
  osi_cred_t *credp;			/* credential structure */
{
#ifdef SGIMIPS
    int errorCode;
#else
    long errorCode;
#endif

    icl_Trace3(xufs_iclSetp, UFS_TRACE_ENTER_VOLREADHOLE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_POINTER, readHoleP);
    readHoleP->flags = 0;
    AFS_hzero(readHoleP->hole.holeOff);
    AFS_hzero(readHoleP->hole.holeLen);    
    errorCode = vol_ufsRead(volp, Fidp, readHoleP->offset, readHoleP->length,
			    readHoleP->buffer, credp, &readHoleP->length);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLREADHOLE,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
}

static int vol_ufsTruncate(
    struct volume *volp,
    struct afsFid *Fidp,
    afs_hyper_t newSize,
    osi_cred_t *credp)
{
    struct vnode *vp;
    struct vattr tvattr;
    long errorCode;

#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    icl_Trace3(xufs_iclSetp, UFS_TRACE_ENTER_VOLTRUNCATE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_HYPER, &newSize);

    if (errorCode = VOL_VGET(volp, Fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLTRUNC_VGET,
		   ICL_TYPE_LONG, errorCode);
	return errorCode;
    }
    osi_vattr_init(&tvattr, OSI_VA_SIZE);

    /* xxx if va_size is 64bits, this code will need to be changed xxx */
    if (AFS_hfitsinu32(newSize))
	tvattr.va_size = AFS_hgetlo(newSize);
    else {
	errorCode = EINVAL;
	goto drop;
    }

    errorCode = VOPX_SETATTR(vp, ((struct xvfs_attr *)&tvattr), 0, credp);
  drop:
    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLTRUNC,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
#endif
}

static int vol_ufsDelete(
    struct volume *volp,
    struct afsFid *Fidp,
    osi_cred_t *credp)
{
    struct vnode *vp;
    struct inode *ip;
    long errorCode;

    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLDELETE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp);
#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
#ifdef EXIST_MACH_VOLOPS
    if (vol_IsInvalidAfsFid(Fidp)) {
	errorCode = EINVAL;
    } else {
	errorCode = vol_ufsDelete_mach(UFS_AGTOVFSP(volp->v_paggrp),
				       Fidp,
				       credp);
    }
#else /* EXIST_MACH_VOLOPS */
#error	"vol_ufsDelete: requires code"
#endif /* EXIST_MACH_VOLOPS */
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLDELETE,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
#endif
}


static int vol_ufsGetattr(
    struct volume *volp,
    struct afsFid *Fidp,
    struct xvfs_attr *xvattrp,
    osi_cred_t *credp)
{
    struct vnode *vp;
    long errorCode;

#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    icl_Trace3(xufs_iclSetp, UFS_TRACE_ENTER_VOLGETATTR,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_POINTER, xvattrp);
    if (errorCode = VOL_VGET(volp, Fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLGETATTR_VGET,
		   ICL_TYPE_LONG, errorCode);
	return errorCode;
    }
    errorCode = VOPX_GETATTR(vp, xvattrp, 1, credp);

#ifdef AFS_HPUX_ENV
    /* Since Episode doesn't like inode generation #s of 0, increment
     * ones coming from HPUX UFS (which start at 0).
     *
     * LFS interoperability:  for a dump to be restorable into LFS,
     * its root vnode must have a uniquifier of 1, so we ensure that
     * here.  This doesn't pose a problem on UFS restoration because
     * the dumped uniquifier isn't used then.
     */
    if (Fidp->Vnode == UFSROOTINO)
	AFS_hset64(xvattrp->xvattr.fileID,
		   AFS_hgethi(xvattrp->xvattr.fileID), 1);
    else
	AFS_hset64(xvattrp->xvattr.fileID,
		   AFS_hgethi(xvattrp->xvattr.fileID),
		   AFS_hgetlo(xvattrp->xvattr.fileID)+1);
#endif
#ifdef AFS_OSF_ENV
	/*
	 * LFS interoperability:  for a dump to be restorable into LFS,
	 * its root vnode must have a uniquifier of 1, so we ensure that
	 * here.  This doesn't pose a problem on UFS restoration because
	 * the dumped uniquifier isn't used then.
	 */
	if (Fidp->Vnode == UFSROOTINO) {
	    xvattrp->vattr.va_gen = 1;
	    AFS_hset64(xvattrp->xvattr.fileID,
		       AFS_hgethi(xvattrp->xvattr.fileID), 1);
	}
#endif
    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLGETATTR,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
#endif
}


static int vol_ufsSetattr(
    struct volume *volp,
    struct afsFid *Fidp,
    struct xvfs_attr *xvattrp,
    osi_cred_t *credp)
{
    struct vnode *vp;
    struct inode *ip;
    struct xvfs_attr lxvattr = *xvattrp;
    long errorCode;

#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    icl_Trace3(xufs_iclSetp, UFS_TRACE_ENTER_VOLSETATTR,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_POINTER, xvattrp);
    if (errorCode = VOL_VGET(volp, Fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLSETATTR_VGET,
		   ICL_TYPE_LONG, errorCode);
	return errorCode;
    }

    /* Turn off attributes that should have been taken care of by
     * vol_ufsCreate */
    lxvattr.vattr.va_type = -1;
    lxvattr.vattr.va_uid = -1;
    lxvattr.vattr.va_gid = -1;
    lxvattr.vattr.va_mode = -1;

#ifdef EXIST_MACH_VOLOPS
    if (errorCode = vol_ufsSetattr_mach(vp, &lxvattr, credp))
	goto out;
#else /* EXIST_MACH_VOLOPS */
#error  "vol_ufsSetAttr: requires code"
#endif /* EXIST_MACH_VOLOPS */

    errorCode = VOPX_SETATTR(vp, &lxvattr, XVN_EXTENDED, credp);

out:
    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLSETATTR,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
#endif
}


static int vol_ufsGetAcl(
    struct volume *volp,
    struct afsFid *Fidp,
    struct dfs_acl *aclp,
    int w,
    osi_cred_t *credp)
{
    struct vnode *vp;
    long errorCode;

#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    icl_Trace3(xufs_iclSetp, UFS_TRACE_ENTER_VOLGETACL,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_POINTER, aclp);
    if (errorCode = VOL_VGET(volp, Fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLGETACL_VGET,
		   ICL_TYPE_LONG, errorCode);
	return errorCode;
    }
    errorCode = VOPX_GETACL(vp, aclp, w, credp);
    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLGETACL,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
#endif
}


static int vol_ufsSetAcl(
    struct volume *volp,
    struct afsFid *Fidp,
    struct dfs_acl *aclp,
    int index,
    int w,
    osi_cred_t *credp)
{
    struct vnode *vp;
    long errorCode;

#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    icl_Trace3(xufs_iclSetp, UFS_TRACE_ENTER_VOLSETACL,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_POINTER, aclp);
    if (errorCode = VOL_VGET(volp, Fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLSETACL_VGET,
		   ICL_TYPE_LONG, errorCode);
	return errorCode;
    }
    /* Temporarily ignore index */
    errorCode = VOPX_SETACL(vp, aclp, 0, w, w, credp);
    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLSETACL,
	       ICL_TYPE_LONG, errorCode);
    return errorCode;
#endif
}


static int vol_ufsClone(
    struct volume *volp,
    struct volume *vol2p,
    osi_cred_t *credp)
{
    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLCLONE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, vol2p);
    return EINVAL;
}


static int vol_ufsReclone(
    struct volume *volp,
    struct volume *vol2p,
    osi_cred_t *credp)
{
     icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLRECLONE,
		ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, vol2p);
    return EINVAL;
}


static int vol_ufsUnclone(
    struct volume *volp,
    struct volume *vol2p,
    osi_cred_t *credp)
{
    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLUNCLONE,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, vol2p);
    return EINVAL;
}


/*
 * Get the inode associated with the fid, fidp
 */
static int vol_ufsVget(volp, fidp, vpp)
    register struct volume *volp;
    register struct afsFid *fidp;
    struct vnode **vpp;
{
    struct osi_vfs *vfsp;
    int code;

    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLVGET,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, fidp);

    if (vol_IsInvalidAfsFid(fidp)) {
	code = EINVAL;
    } else {
	lock_ObtainRead(&volp->v_lock);
	vfsp = UFS_AGTOVFSP(volp->v_paggrp);
	lock_ReleaseRead(&volp->v_lock);
#ifdef SGIMIPS
    /* XXX-RAG check if we need SGI specific like HPUX does */
#endif
#ifdef EXIST_MACH_VOLOPS
	code = vol_ufsVget_mach(vfsp, fidp, vpp);
#else /* EXIST_MACH_VOLOPS */
#ifdef SGIMIPS
	/* _TODO_ This code needs to be filled in XXX */
#else
#error "vol_ufsVget: requires code"
#endif
#endif /* EXIST_MACH_VOLOPS */
    }

    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLVGET,
	       ICL_TYPE_LONG, code);
    return code;
}


/*
 * Get the inode associated with the fid, fipd
 */
static int vol_ufsRoot(volp, vpp)
    register struct volume *volp;
    struct vnode **vpp;
{
    struct osi_vfs *vfsp;
    int code;

    icl_Trace1(xufs_iclSetp, UFS_TRACE_ENTER_VOLROOT, ICL_TYPE_POINTER, volp);
    /*
     * XXX Special case volume's ROOT XXX
     */

    lock_ObtainRead(&volp->v_lock);
    vfsp = UFS_AGTOVFSP(volp->v_paggrp);
    lock_ReleaseRead(&volp->v_lock);
    osi_RestorePreemption(0);
#ifdef  AFS_OSF_ENV
    /* XXX need to avoid deadlock */
    VFS_ROOT(vfsp, vpp, code);
#else /* AFS_OSF_ENV */
#ifdef AFS_HPUX_ENV
    code = VFSX_ROOT(vfsp, vpp, NULL);
#else
    code = VFSX_ROOT(vfsp, vpp);
#endif /* AFS_HPUX_ENV */
#endif /* AFS_OSF_ENV */
    osi_PreemptionOff();
    if (!code) {
	/*
	 * A zero return from VFS_VGET isn't a guarantee for a successful call!
	 */
	if (!*vpp) {
	    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLROOT_ROOT,
		       ICL_TYPE_LONG, ENOENT);
	    return ENOENT;
	}
	xvfs_ConvertDev(vpp);
    }
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLROOT,
	       ICL_TYPE_LONG, code);
    return code;
}



/*
 * code to watch for and detect a UFS volume's root fid.
 */
static int vol_ufsIsRoot(volp, fidp, flagp)
    register struct volume *volp;
    register struct afsFid *fidp;
    int *flagp;
{
#ifdef SGIMIPS
    return(ENOSYS);
#else
    *flagp = (fidp->Vnode == UFSROOTINO ? 1 : 0);
    icl_Trace3(xufs_iclSetp, UFS_TRACE_VOLISROOT,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, fidp,
	       ICL_TYPE_LONG, *flagp);
    return 0;
#endif
}


/* Called with volp->v_lock shared-locked. */
static int vol_ufsGetVV(volp, vvp)
    struct volume *volp;
    afs_hyper_t *vvp;
{
    struct ufs_volData *vsp = VOLTOUFS(volp);

    if (vsp != 0) {
	AFS_hset64(*vvp, vsp->ufsData.creationDate.sec,
		   vsp->ufsData.creationDate.usec);
    } else {
	AFS_hzero(*vvp);
    }
    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLGETVV,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_HYPER, vvp);
    return 0;
}


static int vol_ufsSetDyStat(volp, dystatp)
    struct volume *volp;
    struct vol_stat_dy *dystatp;
{
    struct ufs_volData *vsp;

    lock_ObtainWrite(&volp->v_lock);
    vsp = VOLTOUFS(volp);
    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLSETDYSTAT,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, vsp);
    if (vsp == 0) {
	vsp = (struct ufs_volData *) osi_Alloc(sizeof(struct ufs_volData));
	bzero((char *) vsp, sizeof(struct ufs_volData));
	volp->v_fsDatap = (opaque) vsp;
    }
    vsp->ufsData = *dystatp;

    /* Pick up the file system's creation time. */
    if (volp->v_paggrp)
	UFS_CRTIME(UFS_AGTOVFSP(volp->v_paggrp), vsp);
    lock_ReleaseWrite(&volp->v_lock);
    icl_Trace0(xufs_iclSetp, UFS_TRACE_END_VOLSETDYSTAT);
    return 0;
}

/*
 * Set the volume ID for volp to *newidp.
 */
static int vol_ufsSetNewVID(volp, newidp)
    struct volume *volp;
    afs_hyper_t *newidp;
{
    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLSETNEWVID,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_HYPER, newidp);
    return 0;
}


/*
 * Copy the indicated ACL from one file to another.
 */
static int vol_ufsCopyAcl(volp, Fidp, destw, index, srcw, credp)
    struct volume *volp;
    struct afsFid *Fidp;
    int destw;
    int index;
    int srcw;
    osi_cred_t *credp;
{
    icl_Trace4(xufs_iclSetp, UFS_TRACE_VOLCOPYACL,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_LONG, destw, ICL_TYPE_LONG, index);
#ifdef SGIMIPS
    return(ENOSYS);
#else
    return ENOTTY;
#endif
}


/* Called with volp->v_lock write-locked. */
static int vol_ufsFreeDyStat(volp)
    struct volume *volp;
{
    register struct aggr *ta;

    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLFREEDYSTAT,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_POINTER, volp->v_fsDatap);
    if (volp->v_fsDatap != 0) {
	osi_Free((opaque) volp->v_fsDatap, sizeof(struct ufs_volData));
	volp->v_fsDatap = 0;
    }
    /* moved here from vol_Detach in xvolume/vol_misc.c CFE 27 Nov 91 */
    ta = volp->v_paggrp;
    if (ta && AFS_hsame(volp->v_volId, UFS_AGTOVOLID(ta))) {
	/*
	 * XXX Very UFS specific; if more than a volume available we would like
	 * the aggrp to point to the "now" first volume... XXX
	 */
	AFS_hzero(UFS_AGTOVOLID(ta));
    }
    icl_Trace0(xufs_iclSetp, UFS_TRACE_END_VOLFREEDYSTAT);
    return 0;
}

/*
 * Determine the level of concurrency and return it to fs independent layer
 */
static int vol_ufsConcurr(volp, type, errorType, concurr)
     struct volume *volp;
     int type;
     int errorType;
     unsigned char *concurr;
{
    unsigned char tcon;

    /* determine level of concurrency */
    if ((type & VOL_ALLOK_OPS) == type)
	tcon = VOL_CONCUR_ALLOPS;
    else if ((type & VOL_READONLY_OPS) == type)
	tcon = VOL_CONCUR_READONLY;
    else
	tcon = VOL_CONCUR_NOOPS;

    *concurr = tcon;
    icl_Trace4(xufs_iclSetp, UFS_TRACE_VOLCONCURR,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_LONG, type,
	       ICL_TYPE_LONG, errorType, ICL_TYPE_LONG, tcon);

    return 0;
}

/*
 * Swap the volume IDs of the two given volumes.
 */
static int vol_ufsSwapIDs(vol1p, vol2p, credp)
    register struct volume *vol1p, *vol2p;
    osi_cred_t *credp;
{
    icl_Trace4(xufs_iclSetp, UFS_TRACE_VOLSWAPIDS,
	       ICL_TYPE_POINTER, vol1p, ICL_TYPE_HYPER, &vol1p->v_volId,
	       ICL_TYPE_POINTER, vol2p, ICL_TYPE_HYPER, &vol2p->v_volId);
    return 0;
}


/*
 * Synchronously commit data to disk, depending on value of guarantee.
 */
static int vol_ufsSync(volp, guarantee)
    register struct volume *volp;
    int guarantee;
{

    icl_Trace3(xufs_iclSetp, UFS_TRACE_VOLSYNC,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_HYPER, &volp->v_volId,
	       ICL_TYPE_LONG, guarantee);
    return 0;
}


/*
 * Serialize changes to volume header (status)
 */
static int vol_ufsPushStatus(volp)
    struct volume *volp;
{
    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLPUSHSTATUS,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_HYPER, &volp->v_volId);
    return 0;
}


static int vol_ufsReaddir(volp, fidp, length, bufferp,
    credp, positionp, numEntriesp)
    struct volume *volp;
    struct afsFid *fidp;
    int length;
    char *bufferp;
    osi_cred_t *credp;
    afs_hyper_t *positionp;
    int *numEntriesp;
{
    struct vnode *vp;
    long rc;

    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLREADDIR,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_HYPER, &volp->v_volId);

#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    if (rc = VOL_VGET(volp, fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLREADDIR_VGET,
		   ICL_TYPE_LONG, rc);
	return rc;
    }

#if EXIST_MACH_VOLOPS
    rc = vol_ufsReaddir_mach(vp, length, bufferp, credp,
	positionp, numEntriesp);
#else
    rc = ENOSYS;
#endif /* EXIST_MACH_VOLOPS */

    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLREADDIR,
	       ICL_TYPE_LONG, rc);
    return rc;
#endif
}

static int vol_ufsAppenddir(volp, fidp, numEntries, length, bufferp, preserveOffsets, credp)
    struct volume *volp;
    struct afsFid *fidp;
    u_int numEntries;
    u_int length;
    char *bufferp;
    int preserveOffsets;
    osi_cred_t *credp;
{
    struct vnode *vp;
    long rc;

    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLAPPENDDIR,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_HYPER, &volp->v_volId);
#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else

    if (rc = VOL_VGET(volp, fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLAPPENDDIR_VGET,
		   ICL_TYPE_LONG, rc);
	return rc;
    }

#if EXIST_MACH_VOLOPS
    rc = vol_ufsAppenddir_mach(vp, numEntries, length, bufferp,
	preserveOffsets, credp);
#else
    rc = ENOSYS;
#endif /* EXIST_MACH_VOLOPS */

    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLREADDIR,
	       ICL_TYPE_LONG, rc);
    return rc;
#endif
}

static int vol_ufsGetZLC(volp, iterP, vpP)
    struct volume *volp;
    unsigned int *iterP;
    struct vnode **vpP;
{
    icl_Trace4(xufs_iclSetp, UFS_TRACE_VOLGETZLC,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_HYPER, &volp->v_volId,
	       ICL_TYPE_POINTER, iterP, ICL_TYPE_POINTER, vpP);

    /* setting this guy to 0 means that there aren't any more */
    *vpP = (struct vnode *)0;
    return 0;
}

static int vol_ufsGetNextHoles(volp, fidp, iterp, credp)
    struct volume *volp;
    struct afsFid *fidp;
    register struct vol_NextHole *iterp;
    osi_cred_t *credp;
{
    struct vnode *vp;
    long rc;

    icl_Trace2(xufs_iclSetp, UFS_TRACE_ENTER_VOLGETNEXTHOLES,
	       ICL_TYPE_POINTER, volp, ICL_TYPE_HYPER, &volp->v_volId);
#ifdef SGIMIPS
    /*
     * XXX-RAG Not supported
     */
    return(ENOSYS);
#else
    if (rc = VOL_VGET(volp, fidp, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLGETNEXTHOLES_VGET,
		   ICL_TYPE_LONG, rc);
	return rc;
    }

#if EXIST_MACH_VOLOPS
    rc = vol_ufsGetNextHoles_mach(vp, iterp, credp);
#else
    rc = ENOSYS;
#endif /* EXIST_MACH_VOLOPS */

    OSI_VN_RELE(vp);
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLGETNEXTHOLES,
	       ICL_TYPE_LONG, rc);
    return rc;
#endif
}

static int vol_ufsBulkSetStatus(arrayLen, statusArray)
    unsigned int arrayLen;
    struct vol_statusDesc *statusArray;
{
    icl_Trace2(xufs_iclSetp, UFS_TRACE_VOLBULKSETSTATUS,
	       ICL_TYPE_LONG, arrayLen, ICL_TYPE_POINTER, statusArray);
    return EINVAL;
}

/* The volume ops vector itself */
struct volumeops vol_ufsops = {
    vol_fsHold,
    vol_fsRele,
    vol_fsLock,
    vol_fsUnlock,
    vol_ufsOpen,
    vol_ufsSeek,          /* obsolete */
    vol_ufsTell,          /* obsolete */
    vol_ufsScan,
    vol_ufsClose,
    vol_ufsDestroy,
    vol_ufsAttach,
    vol_ufsDetach,
    vol_ufsGetStatus,
    vol_ufsSetStatus,

    vol_ufsCreate,
    vol_ufsRead,
    vol_ufsWrite,
    vol_ufsTruncate,
    vol_ufsDelete,
    vol_ufsGetattr,
    vol_ufsSetattr,
    vol_ufsGetAcl,
    vol_ufsSetAcl,
    vol_ufsClone,
    vol_ufsReclone,
    vol_ufsUnclone,

    vol_ufsVget,
    vol_ufsRoot,
    vol_ufsIsRoot,
    vol_ufsGetVV,
    vol_ufsSetDyStat,
    vol_ufsFreeDyStat,
    vol_ufsSetNewVID,
    vol_ufsCopyAcl,
    vol_ufsConcurr,
    vol_ufsSwapIDs,
    vol_ufsSync,
    vol_ufsPushStatus,
    vol_ufsReaddir,
    vol_ufsAppenddir,
    vol_ufsBulkSetStatus,
    vol_ufsGetZLC,
    vol_ufsGetNextHoles,
    vol_ufsDeplete,
    vol_ufsReadHole,

    vol_fsDMWait,
    vol_fsDMFree,
    vol_fsStartVnodeOp,
    vol_fsEndVnodeOp,
    vol_fsStartBusyOp,
    vol_fsStartInactiveVnodeOp,
};
