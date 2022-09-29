/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: px_subr.c,v 65.7 1998/06/22 20:00:11 lmc Exp $";
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
#include <dcedfs/sysincludes.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/lock.h>
#include <dcedfs/aggr.h>
#include <dcedfs/volume.h>
#include <dcedfs/osi.h>
#include <dcedfs/xvfs_export.h>
#include <dcedfs/dacl.h>
#include <dcedfs/tkset.h>
#include <dcedfs/fshs.h>
#include <dcedfs/zlc.h>
#include <px.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/px/RCS/px_subr.c,v 65.7 1998/06/22 20:00:11 lmc Exp $")

/*
 * XXX Temporary defines XXX
 */    
#define	SIZE1	1024
#define	SIZE2	SIZE1*8
#define	SIZE3	SIZE2*8
#define	SIZE4	SIZE3*8

/*
 * Construct the afsFetchStatus information to be send back to the client for a
 * particular object (whose attributes are in xvattrp).
 */
void px_xvattr_to_afsstatus(
  struct xvfs_attr *xvattrp,
  afsFetchStatus *OutStatusp,
  struct fshs_principal *princp)
{
    struct vattr *vattrp = &xvattrp->vattr;
    struct Txvattr *attrp = &xvattrp->xvattr;
    int specvp = 0;

    bzero((char *) OutStatusp, sizeof(afsFetchStatus));
    /* 
     * Get what it's applicable from vattr 
     */
    OutStatusp->interfaceVersion = FETCHSTATUS_VERSION;
    if (vattrp->va_type == VREG)
	OutStatusp->fileType = File;
    else if (vattrp->va_type == VLNK)
	OutStatusp->fileType = SymbolicLink;
    else if (vattrp->va_type == VDIR)
	OutStatusp->fileType = Directory;
    else if (vattrp->va_type == VCHR) {
	OutStatusp->fileType = CharDev;
	specvp = 1;
    }
    else if (vattrp->va_type == VBLK) {
	OutStatusp->fileType = BlockDev;
	specvp = 1;
    }
    else if (vattrp->va_type == VFIFO) {
	OutStatusp->fileType = FIFO;
	specvp = 1;
    }
    else {
	/* used to return Invalid here, but that really confuses
	 * the protocol.
	 */
	OutStatusp->fileType = File;
    }
    OutStatusp->linkCount = vattrp->va_nlink;

    AFS_hset64(OutStatusp->length, 0, vattrp->va_size);

    OutStatusp->owner = vattrp->va_uid;
    OutStatusp->mode = vattrp->va_mode & 0xffff;	
    osi_UTimeFromSub(OutStatusp->modTime, vattrp->va_mtime);
    osi_UTimeFromSub(OutStatusp->changeTime, vattrp->va_ctime);
    osi_UTimeFromSub(OutStatusp->accessTime, vattrp->va_atime);
    OutStatusp->group = vattrp->va_gid;
#ifdef AFS_OSF_ENV
    OutStatusp->blocksUsed = (vattrp->va_bytes) >> 10;
#else	/* AFS_OSF_ENV */
#ifdef SGIMIPS
    OutStatusp->blocksUsed = vattrp->va_nblocks;
#else /* SGIMIPS */
    OutStatusp->blocksUsed = vattrp->va_blocks;	/* allocated space (1K units) */
#endif /* SGIMIPS */
#endif	/* AFS_OSF_ENV */
  
    /* 
     * These fields are AFS specific 
     */
    OutStatusp->dataVersion = attrp->dataVersion;
    OutStatusp->author = attrp->author;
    OutStatusp->callerAccess = attrp->callerAccess;
    OutStatusp->anonymousAccess = attrp->anonAccess;
    /* relative expiration near the end of time */
    OutStatusp->aclExpirationTime = 0x7fffffff - 3600 - osi_Time();
    OutStatusp->parentVnode = attrp->parentVnode;
    if (specvp)
	osi_EncodeDeviceNumber(OutStatusp, vattrp->va_rdev);
    else {
	OutStatusp->deviceNumber = 0;
	OutStatusp->deviceNumberHighBits = 0;
    }
    OutStatusp->parentUnique = attrp->parentUnique;
    OutStatusp->serverModTime = attrp->serverModTime;   /* XXX */
    OutStatusp->clientSpare1 = attrp->clientOnlyAttrs;

    /* now translate to mount point */
    if (OutStatusp->fileType == SymbolicLink
	&& (OutStatusp->mode & 0111) == 0) {
	/* this is really a mount point */
	OutStatusp->fileType = MountPoint;
    }

    /*
     * 32/64-bit interoperability: make sure we don't return a status for a
     * long file to an old client.
     */

    if (!princp->hostp->supports64bit &&
	AFS_hcmp(OutStatusp->length, osi_hMaxint32) > 0) {
	OutStatusp->fileType = Invalid;
    }
}


/* make sure that we're setting the time to either something reasonable
 * as specified by the RPC client, or, failing that, to one microsecond
 * after the file's current time (so as to get a useful data version
 * # from the mtime).
 *
 * We don't have to worry about old values for atime, since it isn't
 * being used as a version number (mtime is being used as a data version
 * number).
 *
 * This function must be called after px_afssstatus_to_vattr, since
 * it updates the output vattr in place.
 */
px_check_time_preserve(InStatusp, vattrp, cvattrp)
  register afsStoreStatus *InStatusp;
  register struct vattr *vattrp;
  register struct vattr *cvattrp;
{
    /* if we're setting the time anyway, just use time provided by client */
    if (!(InStatusp->mask & AFS_SETMODTIME)) {
	/* otherwise, we have to set it manually to the old value */
	vattrp->va_mtime.osi_sec = cvattrp->va_mtime.osi_sec;
	vattrp->va_mtime.osi_subsec = cvattrp->va_mtime.osi_subsec+osi_SubIncr;
	/* and watch for microsecond wrap */
	if (vattrp->va_mtime.osi_subsec >= osi_SubUnit) {
	    vattrp->va_mtime.osi_subsec -= osi_SubUnit;
	    vattrp->va_mtime.tv_sec++;
	}
	InStatusp->mask |= AFS_SETMODTIME;
	osi_set_mtime_flag(vattrp);
    }
    if (!(InStatusp->mask & AFS_SETACCESSTIME)) {
	vattrp->va_atime.osi_sec = cvattrp->va_atime.osi_sec;
	vattrp->va_atime.osi_subsec = cvattrp->va_atime.osi_subsec;
	InStatusp->mask |= AFS_SETACCESSTIME;
	osi_set_atime_flag(vattrp);
    }
    return 0;
}

/* called to set attributes on just-created objects.  VOPX_CREATE
 * and company don't set incoming mtime, atime, so we do this
 * manually here, if asked to by caller.
 *
 * Use root credential since setting times requires special privs.
 */
px_SetCreationParms(vp, inStatusp, vattrp)
  register struct vnode *vp;
  register struct afsStoreStatus *inStatusp;
  register struct vattr *vattrp;
{
    struct vattr tvattr;	/* spare */
    long code;

    /* if nothing set that create wouldn't set, just return */
    if (!(inStatusp->mask & (AFS_SETMODTIME | AFS_SETACCESSTIME)))
	return 0;

    /* otherwise, we have something to do, so do the VOPX_SETATTR call */
    osi_vattr_init(&tvattr, 0);
    if (inStatusp->mask & AFS_SETACCESSTIME) {
	tvattr.va_atime = vattrp->va_atime;
	osi_set_atime_flag(&tvattr);
    }
    if (inStatusp->mask & AFS_SETMODTIME) {
	tvattr.va_mtime = vattrp->va_mtime;
	osi_set_mtime_flag(&tvattr);
    }
#ifdef SGIMIPS
    VOPX_SETATTR(vp, ((struct xvfs_attr *)&tvattr), 0, osi_getucred(), code);
#else
    code = VOPX_SETATTR(vp, ((struct xvfs_attr *)&tvattr), 0, osi_getucred());
#endif
    return code;
}

/* do the work required for truncating a file if we've been asked
 * to truncate the file before any other updates.
 */
int px_PreTruncate(vp, instatusp, credp)
  struct vnode *vp;
  afsStoreStatus *instatusp;
  osi_cred_t *credp;
{
    struct vattr tvattr;
    int code;
    
    osi_vattr_init(&tvattr, OSI_VA_SIZE);
    osi_assert (AFS_hcmp(instatusp->truncLength, osi_hMaxint32) <= 0);
    tvattr.va_size = AFS_hgetlo(instatusp->truncLength);
#ifdef SGIMIPS
    VOPX_SETATTR(vp, ((struct xvfs_attr *)&tvattr), 0, credp, code);
#else
    code = VOPX_SETATTR(vp, ((struct xvfs_attr *)&tvattr), 0, credp);
#endif
    return code;
}

/*
 * Construct a vattr structure from incoming (afsStoreStatus) change of status 
 * information for a particular object.  Generate vnode setattr structure
 * from incoming RPC info InStatusp.  If cvattrp is non-null, we should
 * be getting original file's mtime from that field.  Note that ctime is
 * not support (it changes on the server's whim), and atime is only partly
 * supported (it changes with high granularity).  Operations that do
 * change ctime change it in DFS, too, but others do, too.
 */
void px_afsstatus_to_vattr(InStatusp, vattrp, cvattrp)
  register afsStoreStatus *InStatusp;
  register struct vattr *vattrp;
  register struct vattr *cvattrp;
{
    osi_vattr_init(vattrp, 0);
    if (InStatusp->mask & AFS_SETMODTIME) {
	/* If SETMODTIME is set, SETMODEXACT isn't set and cvattrp is passed
	 * in, we check that the modtime being passed in is at least as large
	 * as cvattrp->va_mtime.
	 */
	if (cvattrp && !(InStatusp->mask & AFS_SETMODEXACT)
	    && px_tcmp(cvattrp->va_mtime, InStatusp->modTime) > 0) {
	    /* would be setting clock back, so just bump microseconds */
	    vattrp->va_mtime.osi_sec = cvattrp->va_mtime.osi_sec;
	    vattrp->va_mtime.osi_subsec = cvattrp->va_mtime.osi_subsec+osi_SubIncr;
	    /* and watch for microsecond wrap */
	    if (vattrp->va_mtime.osi_subsec >= osi_SubUnit) {
		vattrp->va_mtime.osi_subsec -= osi_SubUnit;
		vattrp->va_mtime.osi_sec++;
	    }
	}
	else {
	    osi_SubFromUTime(vattrp->va_mtime, InStatusp->modTime);
	}
	osi_set_mtime_flag(vattrp);
    }
    if (InStatusp->mask & AFS_SETACCESSTIME) {
	osi_SubFromUTime(vattrp->va_atime, InStatusp->accessTime);
	osi_set_atime_flag(vattrp);
    }
    /* DON'T USE CHANGETIME: it is a server-only version number */
    if (InStatusp->mask & AFS_SETOWNER) {
	vattrp->va_uid = InStatusp->owner;
	osi_set_uid_flag(vattrp);
    }
    if (InStatusp->mask & AFS_SETGROUP) {
	vattrp->va_gid = InStatusp->group;
	osi_set_gid_flag(vattrp);
    }
    if (InStatusp->mask & AFS_SETMODE) {
	vattrp->va_mode = InStatusp->mode;
	osi_set_mode_flag(vattrp);
    }
    if (InStatusp->mask & AFS_SETLENGTH) {
	osi_assert (AFS_hcmp(InStatusp->length, osi_hMaxint32) <= 0);
	vattrp->va_size = AFS_hgetlo(InStatusp->length);
	osi_set_size_flag(vattrp);
    }
}


/* Set the status of an existing file.  Called by AFS_StoreData and
 * AFS_StoreStatus.  There are several issues that make this more
 * complex than simply calling VOPX_SETATTR:
 * 
 * * We must ensuring that the mtime remains a good version number,
 * unless SETMODEXACT is set (meaning that a particular value *must*
 * be set exactly, to handle a utimes syscall).  For certain updates,
 * those which update the mtime normally, we have to remember the
 * mtime at the start so we can properly compute the legal new value
 * after the call completes.
 *
 * * We must use the root credential to set the access and modification
 * times, if the user isn't the owner, and SETMODEXACT isn't set.
 * This is because a normal user can update these fields by simply
 * writing data to the file.  We need to guard these updates by the
 * appropriate access checks (read access for setting atime, write
 * access for setting mtime).
 *
 * * Our caller has done a VOPX_GETATTR already; we should
 * avoid doing another (for obvious performance reasons).
 *
 * Note that we don't have to worry about any of these issues when
 * creating an initial instance of a dir, file or symlink.
 *
 * Semantics: set the status of file avp to ainStatusp.  The parm
 * ainAttrp describes the current status of the file (from a call to
 * VOPX_GETATTR).  The acredp parameter gives the credentials of the
 * dude making the update call; we check their rights to this file
 * as part of this call.  The parameter aoutAttrp is used to record
 * the status we expect to set; it changes as we do some of the work,
 * and computing its value is the real work of this function.
 *
 * The work is really split into px_PreSetExistingStatus, and
 * px_PostSetExistingStatus.  The "pre" function is called before
 * the data update (by AFS_StoreData), and the "post" function is
 * called afterwards.  For the AFS_StoreStatus call, we do these
 * calls in succession with no intervening calls.  The only change
 * to the file that may be made by this function is the pretruncate
 * work.
 *
 * This call checks for write permission if the file size is
 * changing.  The "post" function checks for control or write
 * permission for changing the rest of the status, except for
 * atime.
 */

int px_PreSetExistingStatus(avp, ainStatusp, aoutAttrp, ainAttrp, acredp)
  struct vnode *avp;
  afsStoreStatus *ainStatusp;
  struct vattr *aoutAttrp;
  struct xvfs_attr *ainAttrp;
  osi_cred_t *acredp;
{
    int code = 0;

    if (((ainStatusp->mask & AFS_SETTRUNCLENGTH) &&
	 (AFS_hcmp(ainStatusp->truncLength, osi_maxFileServer) > 0)) ||
	((ainStatusp->mask & AFS_SETLENGTH) &&
	 (AFS_hcmp(ainStatusp->length, osi_maxFileServer) > 0))) {

	/* Client trying to set the length too large for us to handle. */

	return EFBIG;
    }

    px_afsstatus_to_vattr(ainStatusp, aoutAttrp, &ainAttrp->vattr);

    if (ainStatusp->mask & (AFS_SETTRUNCLENGTH | AFS_SETLENGTH)) {
	/* if vnode op will change mtime, make sure we reset
	 * it to a reasonable value if we weren't sent a value
	 * to use.  Note that control ACL is sufficient, since we might
	 * have done open/create with mode 0, ftruncate, and close.
	 */
	if ((ainAttrp->xvattr.callerAccess &
	     (dacl_perm_write | dacl_perm_control)) == 0)
	    return EACCES;

	px_check_time_preserve(ainStatusp, aoutAttrp, &ainAttrp->vattr);

	/* Now, if we're supposed to truncate the file first, do so.
	 * Look for easy optimization where the file is already small
	 * enough.  Note that pretruncate is only supposed to ensure
	 * that the file is at least this small, it has no obligation
	 * to make sure the file is exactly this size; that's done
	 * by PostSetExistingStatus.
	 */
	if ((ainStatusp->mask & AFS_SETTRUNCLENGTH)
	    && (AFS_hgetlo(ainStatusp->truncLength) <
		ainAttrp->vattr.va_size)) {
	    code = px_PreTruncate(avp, ainStatusp, acredp);
	}
    }

    /* return final code */
    return code;
}


/* actually do the setattr to the value specified by aoutAttrp.  The
 * fields we really want to set are most conveniently described
 * by ainStatusp->mask.  The structure ainAttrp points to the original
 * status of the file, which is used for doing some additional protection
 * checks here.
 */
int px_PostSetExistingStatus(avp, ainStatusp, aoutAttrp, ainAttrp, acredp)
  struct vnode *avp;
  afsStoreStatus *ainStatusp;
  struct vattr *aoutAttrp;
  struct xvfs_attr *ainAttrp;
  osi_cred_t *acredp;
{
    int code = 0;
    struct vattr tvattr;
    struct xvfs_attr allAttrs;
    int doAMSetattr;

    /* separate request into two requests: one for atime and mtime,
     * which can be done by anyone, and which need a root cred
     * to work (since we are setting to an explicit value),
     * and one for everything else.  Do the mtime and atime
     * update *second*, since the first setattr can include a file
     * length change, and could thus change atime and mtime.
     */
    doAMSetattr = 0;
    if (ainStatusp->mask & (AFS_SETACCESSTIME|AFS_SETMODTIME)) {
	/* set access and mtime using root credential */
	osi_vattr_init(&tvattr, 0);
	doAMSetattr = 1;	/* remember to do the setattr */
	if (ainStatusp->mask & AFS_SETACCESSTIME) {
	    tvattr.va_atime = aoutAttrp->va_atime;
	    osi_set_atime_flag(&tvattr);
	}
	if (ainStatusp->mask & AFS_SETMODTIME) {
	    tvattr.va_mtime = aoutAttrp->va_mtime;
	    osi_set_mtime_flag(&tvattr);
	}
	/* and don't try to set times again with wrong cred below */
	if (ainStatusp->mask & AFS_SETACCESSTIME) {
	    aoutAttrp->va_atime.osi_sec = -1;
	    aoutAttrp->va_atime.osi_subsec = -1;
	}
	if (ainStatusp->mask & AFS_SETMODTIME) {
	    aoutAttrp->va_mtime.osi_sec = -1;
	    aoutAttrp->va_mtime.osi_subsec = -1;
	}
	/* keep track of what remains to be done by "main" setattr */
	ainStatusp->mask &= ~ (AFS_SETACCESSTIME | AFS_SETMODTIME);
    }

    /* setting the file size is never pretty or fast.  Let's see
     * if the file is already the right size.  If it is, skip
     * setting the size.  Note that we use this call to *grow*
     * files, too, so we can't let things go if the file is already
     * small enough.
     */
    if (ainStatusp->mask & AFS_SETLENGTH) {
	/* if we're setting the length, check against current length */
#ifdef SGIMIPS
	VOPX_GETATTR(avp, &allAttrs, 0, acredp, code);
#else
	code = VOPX_GETATTR(avp, &allAttrs, 0, acredp);
#endif
	if (code) return code;
	if (allAttrs.vattr.va_size == AFS_hgetlo(ainStatusp->length)) {
	    ainStatusp->mask &= ~AFS_SETLENGTH;
	    osi_clear_size_flag(aoutAttrp);
	}
	/*
	 * Worse yet:  using SETLENGTH to truncate.  Flag this as an error.
	 */
	if (allAttrs.vattr.va_size > AFS_hgetlo(ainStatusp->length)) {
	    uprintf("fx: unexpected truncation of file.\n");
	    icl_Trace2(px_iclSetp, PX_TRACE_BAD_TRUNCATE,
			ICL_TYPE_LONG, AFS_hgetlo(ainStatusp->length),
			ICL_TYPE_LONG, allAttrs.vattr.va_size);
	    ainStatusp->mask &= ~AFS_SETLENGTH;
	    osi_clear_size_flag(aoutAttrp);
	}
    }

    /* see if there's anything left to do in the main setattr */
    if (ainStatusp->mask) {
	/* check permissions: must have write or control rights */
	if ((ainAttrp->xvattr.callerAccess
	     & (dacl_perm_write | dacl_perm_control))
	    == 0)
	    return EACCES;
	/* Prepare to set one extended attribute. */
	/* XXX Ideally, this would be done in px_afsstatus_to_vattr,
	   but that builds only a vattr structure, not a full xvfs_attr one.
	   (For that matter, VOPX_CREATE takes only vattr, not xvfs_attr.)
	   Since this attribute doesn't affect the timestamp manipulation,
	   we can simply set it here. */
	allAttrs.vattr = *aoutAttrp;
	xvfs_NullTxvattr(&allAttrs.xvattr); /* null out the extended part */
	if (ainStatusp->mask & AFS_SETCLIENTSPARE)
	    allAttrs.xvattr.clientOnlyAttrs = ainStatusp->clientSpare1;
	/* clear these flags, since those attrs will be set separate
	 * below, with different credentials (and of course, in a different
	 * order, since mtime is changed by some of the attrs we're setting
	 * here).
	 */
	osi_clear_atime_flag(&allAttrs.vattr);
	osi_clear_mtime_flag(&allAttrs.vattr);
#ifdef SGIMIPS
	VOPX_SETATTR(avp, &allAttrs, XVN_EXTENDED, acredp, code);
#else
	code = VOPX_SETATTR(avp, &allAttrs, XVN_EXTENDED, acredp);
#endif
	if (code) return code;
    }

    /* and finally do the a/m time setattr, if necessary */
    if (doAMSetattr) {
#ifdef SGIMIPS
	VOPX_SETATTR(avp, ((struct xvfs_attr *)&tvattr), 0, osi_getucred(), 
							code);
#else
	code = VOPX_SETATTR(avp, ((struct xvfs_attr *)&tvattr), 0, osi_getucred());
#endif
    }

    return code;
}


/*
 * Setup the volume synchronization information for the clients. It helps cache
 * managers recongize changes to readonly and other replicated volumes.
 */
int px_SetSync (
  register struct volume *volp,
  register afsVolSync *syncp,
  int getrwVV,				/* repserver calling: get VV of RWs */
  register osi_cred_t *credp)
{
    u_int32 curTime;
    int isRW;

    if (!syncp)
	return 0;
    bzero((char *) syncp, sizeof(*syncp));

    if (!volp)
	return 0;

    lock_ObtainShared(&volp->v_lock);
    syncp->VolID = volp->v_volId;

    isRW = ((volp->v_states & VOL_TYPEBITS) == VOL_RW);
    if (getrwVV || !isRW) {
	VOL_GETVV(volp, &(syncp->VV));
    }

    if (volp->v_states & VOL_REPSERVER_MGD) {
	if (isRW) {
	    icl_Trace2 (px_iclSetp, PX_TRACE_RW_IS_REPMGD,
			ICL_TYPE_HYPER, &volp->v_volId,
			ICL_TYPE_LONG, volp->v_states);
	}
	if (volp->v_states & VOL_IS_COMPLETE) {
	    curTime = osi_Time();
	    if (volp->v_states & VOL_HAS_TOKEN) {
		/* Check for repserver crash. */
		if (volp->v_stat_st.tokenTimeout != 0
		    && ((u_long) volp->v_stat_st.tokenTimeout) <= curTime) {
		    lock_UpgradeSToW(&volp->v_lock);
		    volp->v_states &= ~VOL_HAS_TOKEN;
		    /* Claim that we lost the token right now */
#ifdef SGIMIPS
		    osi_afsGetTime( &volp->v_vvCurrentTime);
#else /* SGIMIPS */
		    osi_GetTime((struct timeval *)
				&volp->v_vvCurrentTime);
#endif /* SGIMIPS */
		    lock_ConvertWToS(&volp->v_lock);
		}
		/* This is OK even if we just timed out the token  */
		syncp->VVAge = 0;
	    } else {
		syncp->VVAge = curTime - volp->v_vvCurrentTime.sec;
	    }
	    syncp->VVPingAge = curTime - volp->v_vvPingCurrentTime.sec;
	} else {			/* Not complete */
	    AFS_hzero(syncp->VV);
	    syncp->VVAge = 0x7fffffff;
	    syncp->VVPingAge = 0x7fffffff;
	}
    } else {				/* Not repserver-managed */
	syncp->VVAge = 0;
	syncp->VVPingAge = 0;
    }
    lock_ReleaseShared(&volp->v_lock);
    return 0;
}

/* Adjust the cell structure of a file ID so that it matches the cell used
 * by the UFS glue (ID on the network is really a cache manager-local cell
 * ID, and not of interest to us in this px implementation).
 */
int px_AdjustCell(Fidp)
  register afsFid *Fidp;
{
    Fidp->Cell = tkm_localCellID;
    return 0;
}

/*
 * Get token to sent back to the client.  All of the work was already done by
 * tkm, so we just adjust the absolute expiration time back to a relative time
 * and proceed.
 */
int px_SetTokenStruct(
  struct tkset_set *tsetp,
  afsFid *Fidp,
  afs_token_t *OutTokenp,
  struct fshs_principal *princp)
{
    int errorCode;

    if (errorCode = tkset_KeepToken(tsetp, Fidp, OutTokenp))
	return errorCode;
    FX_FIX_EXPIRATION(OutTokenp->expirationTime);

    /* If client doesn't support 64bit then don't surprise him with high bits
     * in the token range.  If it is a whole file token, map it to something he
     * will understand.  If the entire range is beyond hMaxint32 invalidate the
     * token. */

    if (!princp->hostp->supports64bit) {
	if (AFS_hcmp(OutTokenp->endRange, osi_hMaxint64) == 0) {
	    OutTokenp->endRange = osi_hMaxint32;
	    if (AFS_hcmp(OutTokenp->beginRange, OutTokenp->endRange) > 0) {

		/* byte range is empty, so invalidate token.  Ideally we should
                 * return the token rather than just discard it, but this
                 * should be very rare. */

		memset(OutTokenp, 0, sizeof(*OutTokenp));
	    }
	}
    }
    return 0;
}


/* Check to see if this client already has enough tokens to do what
 * they need (TKN_STATUS_READ).  If so, we're happy, and probably
 * won't have to return new tokens.  Client can always come back again
 * if they need to.
 */
px_ClientHasTokens(
  struct hs_host *ahostp,
  afsFid *afidp)
{
    tkm_tokenSet_t outRights;
    long code;
    afs_hyper_t	startPos;
    afs_hyper_t	endPos;
    
    AFS_hzero(startPos);
    endPos = osi_hMaxint64;
    code = tkm_GetRightsHeld(ahostp, afidp, &startPos, &endPos, &outRights);
    if (code == 0 &&
	(AFS_hgetlo(outRights) & (TKN_STATUS_READ | TKN_OPEN_PRESERVE)) ==
	(TKN_STATUS_READ | TKN_OPEN_PRESERVE))
	return 1;	/* yes, we have all the rights we need */
    else
	return 0;	/* no, we don't, or we can't tell */
}


/* Same as px_SetTokenStruct, only we don't keep the token for the client.
 * Used in path where we decide not to return tokens to caller.
 * MAY BE CALLED WITH PREEMPTION ENABLED
 */
int px_ClearTokenStruct(afs_token_t *OutTokenp)
{
    OutTokenp->expirationTime = 0;
    AFS_hzero(OutTokenp->type);
#ifdef SGIMIPS
    return 0;
#endif
}

/*
 * Common handling routine for reading/writing data 
 */
px_rdwr(vp, rwFlag, Position, Length, credp, streamPtr, zflag, drained,
	 volp, volpDeactivatedP, dmptrp)
    struct vnode *vp;
    enum uio_rw rwFlag;
    afs_hyper_t *Position;
    long Length;
    osi_cred_t *credp;
    pipe_t *streamPtr;
    int zflag;
    int *drained;
    struct volume *volp;
    int *volpDeactivatedP;
    opaque * volatile dmptrp;
{
    int errorCode = 0;			/* Returned error code to caller */
    struct xvfs_attr xvattr;
    struct vattr *vattrP;
    long optSize;


    if (vp->v_type != VREG && (vp->v_type != VLNK || rwFlag == UIO_WRITE)) {
	icl_Trace2(px_iclSetp, PX_TRACE_RDWRTYPE, ICL_TYPE_LONG, rwFlag,
		   ICL_TYPE_LONG, vp->v_type);
	return EINVAL;
    }
#ifdef SGIMIPS
    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
    if (errorCode)
#else
    if (errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp))
#endif
	return errorCode;
    vattrP = &xvattr.vattr;

    /*
     * Setup the optimal xfer buffer size 
     */
    optSize = osi_BUFFERSIZE;
#ifdef SGIMIPS
    if (((vattrP->va_blksize>>10) * vattrP->va_nblocks) < optSize)
        optSize = ((vattrP->va_blksize>>10) * vattrP->va_nblocks);
#else /* SGIMIPS */
    if (vattrP->va_blocksize < optSize)
	optSize = vattrP->va_blocksize;
#endif /* SGIMIPS */
    if (optSize	< osi_MINBUFSIZE) 
	optSize = osi_BUFFERSIZE;


    if (rwFlag == UIO_READ) {
	if ((vp->v_type != VLNK) &&
	    ((xvattr.xvattr.callerAccess & dacl_perm_read) == 0) &&
	    ((xvattr.xvattr.callerAccess & dacl_perm_execute) == 0) &&
	    ((xvattr.xvattr.callerAccess & dacl_perm_control) == 0)) {
	    errorCode = EACCES;
	}
	if (!errorCode)
	    errorCode = px_read(vp, Position, Length, vattrP, optSize, 
				credp, streamPtr, drained,
				volp, volpDeactivatedP, dmptrp);
    } else {
	if (((xvattr.xvattr.callerAccess & dacl_perm_write) == 0) &&
	    ((xvattr.xvattr.callerAccess & dacl_perm_control) == 0)) {
	    errorCode = EACCES;
	}
	if (!errorCode)
	    errorCode = px_write(vp, Position, Length, vattrP, optSize, credp, 
				 streamPtr, zflag, drained,
				 volp, volpDeactivatedP, dmptrp);
    }
    return errorCode;
}

/*
 * Sends back to the client (via a streaming rpc interface) Length bytes of
 * data starting at position 'Position' of the file, vp. Note that it also
 * handles symlinks
 */
px_read(vp, Position, Length, vattrp, optSize, credp, streamPtr, 
	drained, volp, volpDeactivatedP, dmptrp) 
    struct vnode *vp;
    long Length, optSize;
    afs_hyper_t *Position;
    struct vattr *vattrp;
    osi_cred_t *credp;
    pipe_t *streamPtr;
    int * volatile drained;
    struct volume *volp;
    int *volpDeactivatedP;
    opaque * volatile dmptrp;
{
    volatile struct timeval StartTime;	/* used to calculate file xfer rates */
    struct timeval StopTime;		/* used to calculate file xfer rates */
    volatile int errorCode = 0;		/* Returned error code to caller */
    char * volatile bufferp;
    long tposition;
    unsigned long tlen;
    volatile unsigned long xferSize=0;
    
    osi_assert (AFS_hcmp(*Position, osi_hMaxint32) <= 0);
    tposition = AFS_hgetlo(*Position);
    osi_GetTime((struct timeval *)&StartTime);
    tlen = vattrp->va_size;		/* don't send past EOF */
    if (tposition + Length > tlen) 	
	Length = tlen - tposition;	/* get length we should send */
    bufferp = osi_AllocBufferSpace();

    while (Length > 0) {
	struct uio tuio;
	struct iovec iov;
	
	tlen = (Length > optSize? optSize : Length);
	/*
	 * Set up uio vector
	 */
	osi_InitUIO(&tuio);
	tuio.osi_uio_iov = &iov;
	tuio.osi_uio_iovcnt = 1;
	tuio.osi_uio_offset = tposition;
	tuio.osi_uio_resid = tlen;
	tuio.osi_uio_seg = OSI_UIOSYS;
	iov.iov_base = bufferp;
	iov.iov_len = tlen;
	if (vp->v_type == VLNK) {
#ifdef SGIMIPS
	    VOPX_READLINK(vp, &tuio, credp, errorCode);
#else
	    errorCode = VOPX_READLINK(vp, &tuio, credp);
#endif
	} else {
#ifdef SGIMIPS
	    VOPX_RDWR(vp, &tuio, UIO_READ, 0, credp, errorCode);
#else
	    errorCode = VOPX_RDWR(vp, &tuio, UIO_READ, 0, credp);
#endif
	}
	if (errorCode) {
	    break;
	}	    
	tlen = tlen - tuio.osi_uio_resid;	/* Actually read */
	if (tlen == 0) break;		/* hit EOF early */
	/* Break the hold on the volume while waiting for RPC. */
	vol_EndVnodeOp(volp, dmptrp);
	*volpDeactivatedP = 1;
	/* 
	 * Use the Stream function PUSH to write data to the "pipe" 
	 */
	osi_RestorePreemption(0);
	TRY {  /* Try to catch an exception from a broken pipe, in case */
	    (*(streamPtr->push))(streamPtr->state, 
					(unsigned char *)bufferp, tlen);
	} CATCH_ALL {
	    osi_PreemptionOff();
	    uprintf("fx: unexpected exception (%d) while pushing!\n", 
		THIS_CATCH);
	    icl_Trace1(px_iclSetp, PX_TRACE_PUSHEXCEPT_EXC,
		       ICL_TYPE_LONG, THIS_CATCH);
	    errorCode = EIO;  /* What error ? ???? */
	    break;	/* out of while loop */
	} ENDTRY
	osi_PreemptionOff();

	tposition += tlen;
	xferSize += tlen;
	Length -= tlen;
	/* Re-establish the hold on the volume. */
	errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READWRITE, 
			VOL_XCODE_NOWAIT, dmptrp);
	if (errorCode) {
	    break;
	} else {
	    *volpDeactivatedP = 0;
	}
    } /* while */
    if (*volpDeactivatedP == 0) {
	/* Break the hold on the volume while waiting for RPC. */
	vol_EndVnodeOp(volp, dmptrp);
	*volpDeactivatedP = 1;
    }
    osi_RestorePreemption(0);
    TRY {  /* Try to catch an exception from a broken pipe, in case */
	(*(streamPtr->push))(streamPtr->state, (unsigned char *)bufferp, 0);
    } CATCH_ALL {
	osi_PreemptionOff();
	uprintf("fx: unexpected exception (%d) while pushing!\n", THIS_CATCH);
	icl_Trace1(px_iclSetp, PX_TRACE_PUSHEXCEPT_EXC,
		   ICL_TYPE_LONG, THIS_CATCH);
	errorCode = EIO;  /* What error ? ???? */
	osi_RestorePreemption(0);
    } ENDTRY
    osi_PreemptionOff();
    if (errorCode == 0) {
	/* Re-establish the hold on the volume. */
	errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READWRITE,
					 VOL_XCODE_NOWAIT, dmptrp);
	if (errorCode == 0) {
	    *volpDeactivatedP = 0;
	}
    }

    osi_GetTime(&StopTime);
    osi_FreeBufferSpace((struct osi_buffer *)bufferp);
    *drained = 1;	/* pipe is drained */
    /*
     * XXX Most of the following stats are really obsolete and useless
     * (i.e. FetchSize?) and should be removed XXX
     */

    /* 
     * Adjust all Fetch Data related stats 
     */
    if (afsStats.TotalFetchedBytes > 2000000000) /* Reset if over 2 billion */
	afsStats.TotalFetchedBytes = afsStats.AccumFetchTime = 0;
    afsStats.AccumFetchTime += (((StopTime.tv_sec - StartTime.tv_sec) * 1000)
	    + ((StopTime.tv_usec - StartTime.tv_usec) / 1000));
    afsStats.TotalFetchedBytes += xferSize;
    afsStats.FetchSize1++;
    if (xferSize < SIZE2)
	afsStats.FetchSize2++;  
    else if (xferSize < SIZE3)
	afsStats.FetchSize3++;
    else if (xferSize < SIZE4)
	afsStats.FetchSize4++;  
    else
	afsStats.FetchSize5++;
    return errorCode;

}
/*
 * writes from the client (via a streaming rpc interface) Length bytes of
 * data starting at position 'Position' for the file, vp.  If zflag is set,
 * our pipe has 0 bytes in it, and we should supply zeros for the data.  This
 * allows the CM to pass us a hint telling us that the data is all zero.
 */
px_write(vp, Position, Length, vattrp, optSize, credp, streamPtr,
	 zflag, drained, volp, volpDeactivatedP, dmptrp)
    struct vnode *vp;
    long Length, optSize;
    afs_hyper_t *Position;
    struct vattr *vattrp;
    osi_cred_t *credp;
    pipe_t *streamPtr;
    int zflag;
    int * volatile drained;
    struct volume *volp;
    int *volpDeactivatedP;
    opaque * volatile dmptrp;
{
    volatile struct timeval StartTime;	/* used to calculate file xfer rates */
    struct timeval StopTime;		/* used to calculate file xfer rates */
    volatile int errorCode = 0;		/* Returned error code to caller */
    char * volatile bufp;
    long tposition;
#ifdef SGIMIPS
    idl_ulong_int tlen, numByte;
#else
    unsigned long tlen, numByte;
#endif
    volatile unsigned long xferSize=0;

    osi_assert (AFS_hcmp(*Position, osi_hMaxint32) <= 0);
    tposition = AFS_hgetlo(*Position);
    osi_GetTime((struct timeval *)&StartTime);

    bufp = osi_AllocBufferSpace();
    if (zflag) 
        bzero(bufp, optSize);

    icl_Trace1(px_iclSetp, PX_TRACE_STARTPIPE, ICL_TYPE_LONG, Length);
    while (Length > 0) {
 	struct uio tuio;
	struct iovec iov;
	    
	tlen = (Length > optSize? optSize : Length);
	if (!zflag) {
	    /* Break the hold on the volume while waiting for RPC. */
	    vol_EndVnodeOp(volp, dmptrp);
	    *volpDeactivatedP = 1;
	    /* 
	     * get the data, otherwise tlen is already set to #ofbytes read 
	     */
	     osi_RestorePreemption(0);
	     TRY {
		(*(streamPtr->pull))(streamPtr->state, (unsigned char *)bufp,
				     tlen, &numByte);
	     } CATCH_ALL {
		osi_PreemptionOff();	/* typically from pull routine */
		icl_Trace1(px_iclSetp, PX_TRACE_PULLEXCEPT_EXC,
			   ICL_TYPE_LONG, THIS_CATCH);
		uprintf("fx: unexpected exception (%d) while pulling!\n",
			THIS_CATCH);
		errorCode = EIO;  /* What error ? ???? */
		break;
	     } ENDTRY
	     osi_PreemptionOff();
	     tlen = numByte;
	     /* Re-establish the hold on the volume. */
	     errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READWRITE,
					     VOL_XCODE_NOWAIT, dmptrp);
	     if (errorCode) {
		break;
	     } else {
		*volpDeactivatedP = 0;
	     }
	}
	/*
	 * Set up uio vector
	 */
	osi_InitUIO(&tuio);
	tuio.osi_uio_iov = &iov;
	tuio.osi_uio_iovcnt = 1;
	tuio.osi_uio_offset = tposition;
	tuio.osi_uio_resid = tlen;
	tuio.osi_uio_seg = OSI_UIOSYS;
	iov.iov_base = bufp;
	iov.iov_len = tlen;

#ifdef SGIMIPS
	VOPX_RDWR(vp, &tuio, UIO_WRITE, 0, credp, errorCode);
#else
	errorCode = VOPX_RDWR(vp, &tuio, UIO_WRITE, 0, credp);
#endif
	if (errorCode) {
		break;
	}	    
	tlen = tlen - tuio.osi_uio_resid;	/* Actually written */
	tposition += tlen;
	xferSize += tlen;
	Length -= tlen;
    } /* while */
    if (*volpDeactivatedP == 0) {
        /* Break the hold on the volume while waiting for RPC. */
        vol_EndVnodeOp(volp, dmptrp);
        *volpDeactivatedP = 1;
    }
    do {
	osi_RestorePreemption(0);
	TRY {
	    (*(streamPtr->pull))(streamPtr->state, (unsigned char *)bufp,
				 osi_BUFFERSIZE, &numByte);
	} CATCH_ALL {
	    osi_PreemptionOff();	/* typically from pull routine */
	    icl_Trace1(px_iclSetp, PX_TRACE_PULLEXCEPT_EXC,
		       ICL_TYPE_LONG, THIS_CATCH);
	    uprintf("fx: unexpected exception (%d) while pulling!\n",
		    THIS_CATCH);
            errorCode = EIO;  /* What error ? ???? */
            break;
	} ENDTRY
	osi_PreemptionOff();
    } while (numByte != 0);
    if (errorCode == 0) {
        /* Re-establish the hold on the volume. */
        errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READWRITE,
				 VOL_XCODE_NOWAIT, dmptrp);
        if (errorCode == 0) {
	    *volpDeactivatedP = 0;
	}
    }

    osi_GetTime(&StopTime);
    osi_FreeBufferSpace((struct osi_buffer *)bufp);
    icl_Trace0(px_iclSetp, PX_TRACE_ENDPIPE);
    *drained = 1; /* pipe is drained */   
    /*
     * XXX Most of the following stats are really obsolete and useless
     * (i.e. StoreSize?) and should be removed XXX
     */

    /* 
     * Adjust all Store Data related stats 
     */
    if (afsStats.TotalStoredBytes > 2000000000)	/* Reset if over 2 billion */
	afsStats.TotalStoredBytes = afsStats.AccumStoreTime = 0;
    afsStats.AccumStoreTime += (((StopTime.tv_sec - StartTime.tv_sec) * 1000)
	+ ((StopTime.tv_usec - StartTime.tv_usec) / 1000));
    afsStats.TotalStoredBytes += xferSize;
    afsStats.StoreSize1++;
    if (xferSize < SIZE2)
	afsStats.StoreSize2++;  
    else if (xferSize < SIZE3)
	afsStats.StoreSize3++;
    else if (xferSize < SIZE4)
	afsStats.StoreSize4++;  
    else
	afsStats.StoreSize5++;
    return errorCode;
}


/* 
 * Validate target file 
 */
px_FileNameOK(fileNamep)
    register char *fileNamep; 
{
    register long i, tc;

    i = strlen(fileNamep);
    if (i >= 4) {
	/* 
	 * watch for @sys on the right 
	 */
	if (strcmp(fileNamep+i-4, "@sys") == 0) 
	    return 0;
    }
    while (tc = *fileNamep++) {
	if (tc == '/') 
	    return 0;    		/* very bad character to encounter */
    }
    return 1;				/* file name is ok */
}
/*
 * This function is to compute the remaining time for the fx server performing
 * the TSR work. It uses a simple heuristic method to do so. That is, if the 
 * rpc  rate (ie., the number token-renew requests for the last 10 secs) is 
 * greater than 20, then we may want to extend the "recovery period" by another
 * 15 secs ONLY IF the "recovery period" will expire in 15 seconds. 
 */
static long px_lastCompute = 0;	/* time we did the previous computation */
static long px_lastRequests = 0;
static long int px_RecoverTokenRequests = 0;	/* Number of calls to TSR */

void px_ComputeTokenRecoveryTime(bump)
     int bump;		/* whether to bump the count first */
{
    long now;
    long delta, rpcRate, recoveryTooLong;
    long initRecoveryTime; 
    long initInterval;

    lock_ObtainWrite(&px_tsrlock);
    now = osi_Time();
    if (px_lastCompute == 0) {
        px_lastCompute = now;
    }
    if (bump) {
	++px_RecoverTokenRequests;
    }

    delta = now - px_lastCompute;
    /* Check if we still need to recover, after locking */
    if (fshs_postRecovery && (delta >= 10 /* sec */)) {
	/* 
	 * Compute the token renew requests per 10 seconds and  also 
	 * compute the remaining recovery time to see whether the TSR work
	 * already takes too long (more than twice the original interval). 
	 */
	initRecoveryTime = fshs_tsrParams.serverRestart + INITIAL_GRACE_PERIOD;
	initInterval = fshs_tsrParams.maxLife > fshs_tsrParams.deadServer ? 
	  fshs_tsrParams.maxLife : fshs_tsrParams.deadServer;
	recoveryTooLong = (now - initRecoveryTime) > 2 * initInterval;
	
	rpcRate = ((px_RecoverTokenRequests - px_lastRequests) * 10) / delta;
	icl_Trace4(px_iclSetp, PX_TRACE_RECOVERYRATE,
		   ICL_TYPE_LONG, rpcRate,
		   ICL_TYPE_LONG, px_RecoverTokenRequests,
		   ICL_TYPE_LONG, px_lastRequests,
		   ICL_TYPE_LONG, delta);
	
	px_lastCompute = now;
	px_lastRequests = px_RecoverTokenRequests;

	if ((now > fshs_tsrParams.endRecoveryTime) || recoveryTooLong) {
	    /* End the recovery period */
	    icl_Trace4(px_iclSetp, PX_TRACE_ENDRECOVERY,
		       ICL_TYPE_LONG, now,
		       ICL_TYPE_LONG, initRecoveryTime,
		       ICL_TYPE_LONG, fshs_tsrParams.endRecoveryTime,
		       ICL_TYPE_LONG, initInterval);
	    fshs_postRecovery = 0;
	    zlc_SetRestartState(0);
	}
	else if (rpcRate > 20 &&
		 (fshs_tsrParams.endRecoveryTime - now ) <= 15 /* seconds */) {
	    fshs_tsrParams.endRecoveryTime += 15;
	    zlc_SetRestartState(fshs_tsrParams.endRecoveryTime);
	    icl_Trace3(px_iclSetp, PX_TRACE_EXTENDRECOVERY,
		       ICL_TYPE_LONG, rpcRate,
		       ICL_TYPE_LONG, fshs_tsrParams.endRecoveryTime,
		       ICL_TYPE_LONG, now);
	}
    }
    lock_ReleaseWrite(&px_tsrlock);
}

void px_CheckTSREndTime()
{
    long now;

    lock_ObtainWrite(&px_tsrlock);
    now = osi_Time();
    if (fshs_postRecovery != 0 &&
	((now-15) > fshs_tsrParams.endRecoveryTime)) {
	/* End the recovery period */
	icl_Trace4(px_iclSetp, PX_TRACE_ENDRECOVERY,
		   ICL_TYPE_LONG, now,
		   ICL_TYPE_LONG, 0,
		   ICL_TYPE_LONG, fshs_tsrParams.endRecoveryTime,
		   ICL_TYPE_LONG, 42);
	fshs_postRecovery = 0;
	zlc_SetRestartState(0);
    }
    lock_ReleaseWrite(&px_tsrlock);
}

/*
 * Return Fileserver statistics
 */
void
GetFileServerStats(Statisticsp)
    afsStatistics *Statisticsp;
{
    extern long px_StartTime;
	
    Statisticsp->StartTime = px_StartTime;
    /* Call GetRPCStats() someday */
}

void
GetVolumeStats(Statisticsp)
    afsStatistics *Statisticsp;
{
}

#ifndef AFS_HPUX_ENV
void
GetSystemStats(Statisticsp)
    afsStatistics *Statisticsp;
{
}
#endif /* AFS_HPUX_ENV */
