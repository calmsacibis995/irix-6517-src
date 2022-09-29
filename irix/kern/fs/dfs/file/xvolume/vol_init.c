/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: vol_init.c,v 65.4 1998/04/01 14:16:46 gwehrman Exp $";
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
 * $Log: vol_init.c,v $
 * Revision 65.4  1998/04/01 14:16:46  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Use -1U instead of -1UL for the agid when calling vol_open().  The
 * 	parameter is of type u_int.
 *
 * Revision 65.3  1998/03/23  16:46:53  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:01:46  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:21:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.836.1  1996/10/02  19:04:40  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  18:59:58  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/debug.h>
#include <dcedfs/syscall.h>
#include <dcedfs/aclint.h>
#include <dcedfs/zlc.h>         /* For PxUp macro */
#include <dcedfs/volreg.h>
#include <volume.h>
#include <vol_init.h>
#include <vol_errs.h>
#include <vol_trace.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvolume/RCS/vol_init.c,v 65.4 1998/04/01 14:16:46 gwehrman Exp $")

/* forward references */
static int vol_BulkSetStatus(u_int, struct vol_statusDesc *);

/* VOLOP syscall to volops bit vector translation array */
static u_int volops_sys2bit[] = {
/* 0 thru 4; 4 is VOLOP_OPEN */	0,0,0,0,0,
/* 5, VOLOP_SEEK */	VOL_SYS_SEEK,
/* 6, VOLOP_TELL */	VOL_SYS_TELL,
/* 7, VOLOP_SCAN */	VOL_SYS_SCAN,
/* 8, VOLOP_CLOSE */	0,
/* 9, VOLOP_DESTROY */	VOL_SYS_DESTROY,
/* 10, VOLOP_GETSTATUS */	VOL_SYS_GETSTATUS,
/* 11, VOLOP_SETSTATUS */	VOL_SYS_SETSTATUS,
/* 12, VOLOP_CREATE */		VOL_SYS_CREATE,
/* 13, VOLOP_READ */		VOL_SYS_READ,
/* 14, VOLOP_WRITE */		VOL_SYS_WRITE,
/* 15, VOLOP_TRUNCATE */	VOL_SYS_TRUNCATE,
/* 16, VOLOP_DELETE */		VOL_SYS_DELETE,
/* 17, VOLOP_GETATTR */		VOL_SYS_GETATTR,
/* 18, VOLOP_SETATTR */		VOL_SYS_SETATTR,
/* 19, VOLOP_GETACL */		VOL_SYS_GETACL,
/* 20, VOLOP_SETACL */		VOL_SYS_SETACL,
/* 21, VOLOP_CLONE */		VOL_SYS_CLONE,
/* 22, VOLOP_RECLONE */		VOL_SYS_RECLONE,
/* 23-25, VOLOP_{VGET,ROOT,ISROOT} */	0,0,0,
/* 26, VOLOP_UNCLONE */		VOL_SYS_UNCLONE,
/* 27, VOLOP_FCLOSE */		0,
/* 28, VOLOP_SETVV */		VOL_SYS_SETVV,
/* 29, VOLOP_SWAPVOLIDS */	VOL_SYS_SWAPVOLIDS,
/* 30, VOLOP_COPYACL */		VOL_SYS_COPYACL,
/* 31, VOLOP_AGOPEN */		0,
/* 32, VOLOP_SYNC */		VOL_SYS_SYNC,
/* 33, VOLOP_PUSHSTATUS */	VOL_SYS_PUSHSTATUS,
/* 34-36, VOLOP_{PROBE,LOCK,UNLOCK} */	0,0,0,
/* 37, VOLOP_READDIR */		VOL_SYS_READDIR,
/* 38, VOLOP_APPENDDIR */	VOL_SYS_APPENDDIR,
/* 39, VOLOP_BULKSETSTATUS */	VOL_SYS_SETSTATUS,
/* 40, VOLOP_GETZLC */		0,
/* 41, VOLOP_GETNEXTHOLES */	VOL_SYS_GETNEXTHOLES,
/* 42, VOLOP_DEPLETE */		VOL_SYS_DEPLETE,
/* 43, VOLOP_READHOLE */	VOL_SYS_READHOLE,
};

/*
 *  Ensure that the volop to be invoked is already set in volops bit vector
 */
#define VOLCHECK(Volp, Volsys) \
MACRO_BEGIN \
    u_int vmask = volops_sys2bit[Volsys]; \
    code = ((Volp->v_accStatus & vmask) != vmask) ? EINVAL : 0; \
    if (code != 0) { \
    /* the print statement could be removed after debugging */  \
	printf("volops for syscall %u not set at file %s line %u\n", \
              (unsigned)Volsys, __FILE__, (unsigned)__LINE__); \
	icl_Trace3(xops_iclSetp, XVOL_TRACE_BADSYS, \
		 ICL_TYPE_LONG, (unsigned)Volsys, \
		 ICL_TYPE_LONG, Volp->v_accStatus, \
		 ICL_TYPE_LONG, (unsigned)__LINE__); \
    } \
MACRO_END

/*
 * Check to see if PX is running - needed in vol_open
 */
#define PX_UP (zlc_recoveryState != ZLC_REC_UNKNOWN)

/*
 * Volops that require non-trivial setup or data storage.  Operation
 * VOL_XXX is implemented in do_XXX; the easy cases are handled directly
 * in afscall_volser.
 */
static int
do_open(
  caddr_t uVolIdP,
  int opnType,
  int opnErr,
  int *resultP)
{
    afs_hyper_t volid;
    int code;

    icl_Trace0(xops_iclSetp, XVOL_TRACE_CALL_OPEN);
    code = osi_copyin(uVolIdP, (caddr_t)&volid, sizeof (volid));
    if (code != 0)
	return code;

#ifdef SGIMIPS
    return vol_open(&volid, -1U, opnType, opnErr, resultP, resultP, 0);
#else
    return vol_open(&volid, -1UL, opnType, opnErr, resultP, resultP, 0);
#endif
}

/*
 * VOL_AGOPEN
 */
static int
do_agopen(
  caddr_t uAgOpenP,
  int *udescP,
  int *uerrP)
{
    struct vol_AgOpen agopen;
    int code;

    icl_Trace0(xops_iclSetp, XVOL_TRACE_CALL_AGOPEN);
    code = osi_copyin(uAgOpenP, (caddr_t)&agopen, sizeof (agopen));
    if (code != 0)
	return code;

    code = vol_open(&agopen.volId, agopen.agId, agopen.accStat,
		    agopen.accErr, udescP, uerrP, 0);
    return code;
}

#ifdef AFS_DEBUG
/*
 * VOL_FCLOSE (Debugging only!)
 */
static int
do_fclose(caddr_t uVolIdP)
{
    struct volume *volp;
    struct vol_handle hdl;
    struct afsFid fid;
    int code, code2;

    icl_Trace0(xops_iclSetp, XVOL_TRACE_CALL_FCLOSE);
    code = osi_copyin(uVolIdP, (caddr_t)&fid.Volume, sizeof (fid.Volume));
    if (code != 0)
	return code;

    icl_Trace1(xops_iclSetp, XVOL_TRACE_CALL_FCLOSE_VOL,
		ICL_TYPE_HYPER, &fid.Volume);

    code = volreg_Lookup(&fid, &volp);
    if (code != 0) {
	icl_Trace2(xops_iclSetp, XVOL_TRACE_NOVOLUME,
			ICL_TYPE_LONG, AFS_hgetlo(fid.Volume),
			ICL_TYPE_LONG, code);
	return ENODEV;
    }

    hdl.volp = volp;
    code = VOL_CLOSE(volp, &hdl, 0);

    lock_ObtainWrite(&volp->v_lock);
    (void) vol_close(volp, NULL);
    lock_ReleaseWrite(&volp->v_lock);

    /* If we've deleted the fileset, detach it */
    if (volp->v_states & VOL_DEADMEAT) {
	code2 = vol_Detach(volp, 1);
	if (code == 0)
	    code = code2;
    }

    VOL_RELE(volp);
    return (code);
}
#endif /* AFS_DEBUG */

static int
do_bulksetstatus(u_int arrayLen, caddr_t uArray)
{
    u_int arraySize = arrayLen * sizeof (struct vol_statusDesc);
    struct vol_statusDesc *statusArray;
    int code;

    icl_Trace1(xops_iclSetp, XVOL_TRACE_CALL_BULKSET,
		ICL_TYPE_LONG, arrayLen);

    if (arrayLen == 0)
	return 0;
    else if (arrayLen > VOL_MAX_BULKSETSTATUS)
	return EINVAL;

    statusArray = (struct vol_statusDesc *)
	(arraySize <= osi_BUFFERSIZE ?
	    osi_AllocBufferSpace() : osi_Alloc(arraySize));

    code = osi_copyin(uArray, (caddr_t)statusArray, arraySize);

    if (code == 0)
	code = vol_BulkSetStatus(arrayLen, statusArray);

    if (arraySize <= osi_BUFFERSIZE)
	osi_FreeBufferSpace((struct osi_buffer *)statusArray);
    else
	osi_Free(statusArray, arraySize);

    return code;
}

static int
do_create(
  struct volume *volp,
  struct vol_handle *handle,
  int position,
  caddr_t uXvattr)
{
    struct xvfs_attr xvattr;
    int code;

    code = osi_copyin(uXvattr, (caddr_t)&xvattr, sizeof (xvattr));
    if (code == 0)
	code = VOL_CREATE(volp, position, &xvattr, handle, osi_getucred());

    return code;
}

static int
do_getstatus(struct volume *volp, caddr_t uStatusP)
{
    struct vol_status volStat;
    int code;

    lock_ObtainRead(&volp->v_lock);
    code = VOL_GETSTATUS(volp, &volStat);
    if (code == 0)
	code = osi_copyout((caddr_t)&volStat, uStatusP, sizeof (volStat));
    lock_ReleaseRead(&volp->v_lock);
    return (code);
}

static int
do_setstatus(
  struct volume *volp,
  int flags,
  caddr_t uStatusP)
{
    afs_hyper_t oldVolId;
    char oldVolName[VOLNAMESIZE];
    struct vol_status volStat;
    int code;

    VOLCHECK(volp, VOLOP_SETSTATUS);
    if (code != 0)
	return code;

    if (flags & VOL_STAT_VOLID) {
	VOLCHECK(volp, VOLOP_SWAPVOLIDS);
	if (code != 0)
	    return code;
    }

    code = osi_copyin(uStatusP, (caddr_t)&volStat, sizeof (volStat));
    if (code != 0)
	return code;

    lock_ObtainWrite(&volp->v_lock);

    AFS_hzero(oldVolId);
    if ((flags & VOL_STAT_VOLID) &&
	!AFS_hsame(volp->v_volId, volStat.vol_st.volId)) {
	oldVolId = volp->v_volId;
    }

    oldVolName[0] = '\0';
    if ((flags & VOL_STAT_VOLNAME) &&
	strcmp(volp->v_volName, volStat.vol_st.volName) != 0) {
	(void) strcpy(oldVolName, volp->v_volName);
    }


    code = VOL_SETSTATUS(volp, flags, &volStat);

    if (code == 0) {
	if (!AFS_hiszero(oldVolId)) {
	    /* VOL_SETSTATUS does not change volp->v_volId */
	    volp->v_volId = volStat.vol_st.volId; /* XXXXX */
	}

	if (oldVolName[0] != '\0') {
	    /* VOL_SETSTATUS does not change volp->v_volName */
	    (void) strcpy(volp->v_volName, volStat.vol_st.volName);
	}
    }

    lock_ReleaseWrite(&volp->v_lock);

    if (code == 0 &&
	(!AFS_hiszero(oldVolId) || oldVolName[0] != '\0')) {
	code = volreg_ChangeIdentity(&oldVolId,
				 &volStat.vol_st.volId,
				 oldVolName, volStat.vol_st.volName);
    }

    return (code);
}

static int
do_close(
  struct volume *volp,
  struct vol_calldesc *cdp,
  int flags)
{
    int code, code2;

    cdp->handle.volp = (struct volume *) 0;
    /* clear ref from cdp; still held by syscall's VOL_HOLD. */
    VOL_RELE(volp);

    /*
     * Must do the close before letting vnode ops go through.
     */
    code = VOL_CLOSE(volp, &cdp->handle, flags);

    /*
     * Wake up anyone waiting for this volume to be available
     */
    lock_ObtainWrite(&volp->v_lock);
    (void) vol_close(volp, NULL);
    lock_ReleaseWrite(&volp->v_lock);

    /*
     * We don't need cdp any more.
     */
    vol_SetDeleted(cdp);
    vol_PutDesc(cdp);

    /* If we've just deleted the fileset, detach it */
    if (volp->v_states & VOL_DEADMEAT) {
	code2 = vol_Detach(volp, 1);
	if (code == 0)
	    code = code2;
    }

    VOL_RELE(volp);	/* once for this syscall */
    return code;
}

static int
do_getattr(
  struct volume *volp,
  struct afsFid *fidp,
  caddr_t uXvattr)
{
    struct xvfs_attr xvattr;
    int rcode, copycode;

    /* Flag high part of fileID to protect against overloaded VOL_ERR_DELETED
     * result code.
     */
    AFS_hset64(xvattr.xvattr.fileID, -1, -1);

    rcode = VOL_GETATTR(volp, fidp, &xvattr, osi_getucred());

    if ((rcode == 0) ||
	(rcode == VOL_ERR_DELETED && AFS_hgethi(xvattr.xvattr.fileID) != -1)) {
	/* retrieved attributes for a valid or deleted fid */
	copycode = osi_copyout((caddr_t)&xvattr, uXvattr, sizeof (xvattr));

	if (copycode) {
	    rcode = copycode;
	}
    }
    return rcode;
}

static int
do_setattr(
  struct volume *volp,
  struct afsFid *fidp,
  caddr_t uXvattr)
{
    struct xvfs_attr xvattr;
    struct vnode *vnp;
    struct afsFid fid;
    int code;

    code = osi_copyin(uXvattr, (caddr_t)&xvattr, sizeof (xvattr));
    if (code != 0)
	return code;

    if (xvattr.vattr.va_nlink == 0 && (volp->v_states & VOL_RW)) {
	/*
	 * We're creating a zero-linkcount file here.  We
	 * may need to preserve it.  First, create a real fid.
	 */
	fid = *fidp;
	fid.Volume = volp->v_volId;
	fid.Cell = tkm_localCellID;	/* PX convention */

	/* get vnode pointer */
	code = VOL_VGET(volp, &fid, &vnp);
	if (code == 0) {
	    zlc_TryRemove(vnp, &fid);
	    OSI_VN_RELE(vnp);	/* held by VGET */
	}
    }

    code = VOL_SETATTR(volp, fidp, &xvattr, osi_getucred());
    return code;
}

static int
do_clone(struct volume *volp, int op, int descr)
{
    struct vol_calldesc *cd2p;
    struct volume *vol2p;
    int code;

    code = vol_FindDesc(descr, &cd2p);
    if (code != 0) {
	icl_Trace2(xops_iclSetp, XVOL_TRACE_NODESC2,
			ICL_TYPE_LONG, descr, ICL_TYPE_LONG, code);
	return code;
    }

    vol2p = cd2p->handle.volp;
    VOLCHECK(vol2p, op);
    if (code != 0) {
	vol_PutDesc(cd2p);
	return code;
    }
    /* Ensure that both volumes are the same type */
    if (volp->v_volOps->vol_clone != vol2p->v_volOps->vol_clone) {
	icl_Trace2(xops_iclSetp, XVOL_TRACE_DIFFOPS,
		   ICL_TYPE_LONG, AFS_hgetlo(volp->v_volId),
		   ICL_TYPE_LONG, AFS_hgetlo(vol2p->v_volId));
	code = EXDEV;
    } else if (volp->v_paggrp != NULL && vol2p->v_paggrp != NULL &&
	volp->v_paggrp->a_aggrId != vol2p->v_paggrp->a_aggrId) {
	icl_Trace4(xops_iclSetp, XVOL_TRACE_DIFFAGGR,
			ICL_TYPE_LONG, AFS_hgetlo(volp->v_volId),
			ICL_TYPE_LONG, volp->v_paggrp->a_aggrId,
			ICL_TYPE_LONG, AFS_hgetlo(vol2p->v_volId),
			ICL_TYPE_LONG, vol2p->v_paggrp->a_aggrId);
	code = EXDEV;
    } else {
	VOL_HOLD(vol2p);
	if (op == VOLOP_CLONE)
	    code = VOL_CLONE(volp, vol2p, osi_getucred());
	else if (op == VOLOP_RECLONE)
	    code = VOL_RECLONE(volp, vol2p, osi_getucred());
	else
	    code = VOL_UNCLONE(volp, vol2p, osi_getucred());
	VOL_RELE(vol2p);
    }

    vol_PutDesc(cd2p);
    return code;
}

static int
do_setvv(struct volume *volp, caddr_t uSetVV)
{
    struct vol_SetVV setVV;
    struct vol_status volStat;
    u_int tempStates;
    u_int tempMask = 0;
    int code;

    code = osi_copyin(uSetVV, (caddr_t)&setVV, sizeof (setVV));
    if (code != 0)
	return code;

    /* change only these bits */
    setVV.vv_Mask &= (VOL_REPFIELD | VOL_IS_COMPLETE | VOL_HAS_TOKEN);
    lock_ObtainWrite(&volp->v_lock);
    tempStates = volp->v_states;
    tempStates &= ~(setVV.vv_Mask);
    tempStates |= (setVV.vv_Flags & setVV.vv_Mask);
    if (tempStates != volp->v_states) {
	volStat.vol_st.states = tempStates;
	tempMask |= VOL_STAT_STATES;
    }
    if (bcmp((caddr_t)&setVV.vv_Curr,
	     (caddr_t)&volp->v_vvCurrentTime,
	     sizeof (volp->v_vvCurrentTime))) {
	volStat.vol_st.vvCurrentTime = setVV.vv_Curr;
	tempMask |= VOL_STAT_VVCURRTIME;
    }
    if (bcmp((caddr_t)&setVV.vv_PingCurr,
	     (caddr_t)&volp->v_vvPingCurrentTime,
	     sizeof(volp->v_vvPingCurrentTime))) {
	volStat.vol_st.vvPingCurrentTime = setVV.vv_PingCurr;
	tempMask |= VOL_STAT_VVPINGCURRTIME;
    }
    volp->v_stat_st.tokenTimeout = setVV.vv_TknExp.sec;
    if (tempMask)
	code = VOL_SETSTATUS(volp, tempMask, &volStat);

    lock_ReleaseWrite(&volp->v_lock);
    return code;
}

static int
do_swapvolids(struct volume *volp, int descr)
{
    struct vol_calldesc *cd2p;
    struct volume *vol2p;
    int code;

    code = vol_FindDesc(descr, &cd2p);
    if (code != 0) {
	icl_Trace2(xops_iclSetp, XVOL_TRACE_NODESC3,
			ICL_TYPE_LONG, descr, ICL_TYPE_LONG, code);
	return code;
    }

    vol2p = cd2p->handle.volp;
    VOLCHECK(vol2p, VOLOP_SWAPVOLIDS);
    if (code != 0) {
	vol_PutDesc(cd2p);
	return code;
    }

    if (volp->v_volOps != vol2p->v_volOps) {
	icl_Trace2(xops_iclSetp, XVOL_TRACE_DIFFOPS2,
		   ICL_TYPE_LONG, AFS_hgetlo(volp->v_volId),
		   ICL_TYPE_LONG, AFS_hgetlo(vol2p->v_volId));
	code = EXDEV;
    } else {
	VOL_HOLD(vol2p);
	code = vol_SwapIDs(volp, vol2p);
	VOL_RELE(vol2p);
    }

    vol_PutDesc(cd2p);
    return code;
}

static int
do_getnextholes(struct volume *volp, struct afsFid *fidp, caddr_t uNextHole)
{
    struct vol_NextHole nextHole;
    int code;

    code = osi_copyin(uNextHole, (caddr_t)&nextHole, sizeof (nextHole));
    if (code == 0)
	code = VOL_GETNEXTHOLES(volp, fidp, &nextHole, osi_getucred());
    if (code == 0)
	code = osi_copyout((caddr_t)&nextHole, uNextHole, sizeof (nextHole));

    return code;
}

static int
do_readhole(struct volume *volp, struct afsFid *fidp, caddr_t uReadHole)
{
    struct vol_ReadHole readHole;
    int code;

    code = osi_copyin(uReadHole, (caddr_t)&readHole, sizeof(readHole));
    if (code == 0) 
	code = VOL_READHOLE(volp, fidp, &readHole, osi_getucred());
    if (code == 0)
	code = osi_copyout((caddr_t)&readHole, uReadHole, sizeof(readHole));
    return code;
}

/*
 * Main volume operations kernel entry point
 */
int
afscall_volser(
    long op, long parm2, long parm3, long parm4, long parm5,
    int *retvalP)
{
    struct vol_calldesc *cdp;
    struct volume *volp;
    struct io_rwHyperDesc ioDesc;
    afs_hyper_t newSize;
    int outlen;
    struct dfs_acl *aclp;
    int numDirEntries;
    int code = 0;

    /*
     * XXX VOL auth varies per call so the following isn't right! XXX
     */
    if (!osi_suser(osi_getucred())) {
	return EACCES;
    }

    *retvalP = 0;		/* Always return 0 in retvalP */

    /*
     * First, handle some special cases:
     */
    switch (op) {
      case VOLOP_OPEN:
	return do_open((caddr_t)parm2, parm3, parm4, (int *)parm5);

      case VOLOP_AGOPEN:
	return do_agopen((caddr_t)parm2, (int *)parm3, (int *)parm4);

#ifdef	AFS_DEBUG
      case VOLOP_FCLOSE:
	return do_fclose((caddr_t)parm2);
#endif	/* AFS_DEBUG */
      case VOLOP_BULKSETSTATUS:
	return do_bulksetstatus((u_int)parm2, (caddr_t)parm3);

      default:
	break;
    }

    /*
     * Now take care of all the ordinary ops.
     */
    code = vol_FindDesc(parm2, &cdp);
    if (code != 0) {
	icl_Trace2(xops_iclSetp, XVOL_TRACE_NODESC,
			ICL_TYPE_LONG, parm2, ICL_TYPE_LONG, code);
	return (code);
    }

    volp = cdp->handle.volp;
    VOL_HOLD(volp);
    icl_Trace3(xops_iclSetp, XVOL_TRACE_CALL_EXISTING,
			ICL_TYPE_LONG, op,
			ICL_TYPE_POINTER, volp,
			ICL_TYPE_HYPER, &volp->v_volId);

    VOLCHECK(volp, op);
    if (code != 0)
	goto done;

    switch (op) {
      case VOLOP_SEEK:
	code = VOL_SEEK(volp, parm3, &cdp->handle);
	break;
      case VOLOP_TELL:
	code = VOL_TELL(volp, &cdp->handle, &outlen);
	if (code == 0)
	    code = osi_copyout((caddr_t)&outlen, (caddr_t)parm3,
					sizeof (outlen));
	break;
      case VOLOP_SCAN:
	code = VOL_SCAN(volp, parm3, &cdp->handle);
	break;
      case VOLOP_LOCK:
	/* code = VOL_LOCK(volp, parm3); */
	break;
      case VOLOP_UNLOCK:
	/* code = VOL_UNLOCK(volp, parm3); */
	break;
      case VOLOP_CLOSE:
	return do_close(volp, cdp, parm3);
      case VOLOP_DEPLETE:
	code = VOL_DEPLETE(volp);
	break;
      case VOLOP_DESTROY:
	code = VOL_DESTROY(volp);
	if (!code) {
	    zlc_CleanVolume(&volp->v_volId);
	    lock_ObtainWrite(&volp->v_lock);
	    volp->v_states |= VOL_DEADMEAT;
	    lock_ReleaseWrite(&volp->v_lock);
	}
	break;
      case VOLOP_GETSTATUS:
	code = do_getstatus(volp, (caddr_t)parm3);
	break;
      case VOLOP_SETSTATUS:
	code = do_setstatus(volp, parm3, (caddr_t)parm4);
	break;
      case VOLOP_CREATE:
	code = do_create(volp, &cdp->handle, parm3, (caddr_t)parm4);
	break;
      case VOLOP_READ:
	code = osi_copyin((caddr_t)parm3, (caddr_t)&ioDesc, sizeof (ioDesc));
	if (code == 0) {
	    code = VOL_READ(volp, &cdp->handle.fid, ioDesc.position,
			    ioDesc.length, ioDesc.bufferp,
			    osi_getucred(), &outlen);
	}
	if (code == 0) {
	    code = osi_copyout((caddr_t)&outlen, (caddr_t)parm4,
				sizeof (outlen));
	}
	break;
      case VOLOP_WRITE:
	code = osi_copyin((caddr_t)parm3, (caddr_t)&ioDesc, sizeof (ioDesc));
	if (!code)
	    code = VOL_WRITE(volp, &cdp->handle.fid, ioDesc.position,
			     ioDesc.length, ioDesc.bufferp, osi_getucred());
	break;
      case VOLOP_READDIR:
	code = osi_copyin((caddr_t)parm3, (caddr_t)&ioDesc, sizeof (ioDesc));
	if (code == 0) {
	    code = VOL_READDIR(volp, &cdp->handle.fid,
			       ioDesc.length, ioDesc.bufferp, osi_getucred(),
			       &ioDesc.position, &numDirEntries);
	}
	if (code == 0) {
	    code = osi_copyout((caddr_t)&numDirEntries,
				(caddr_t)parm4, sizeof (numDirEntries));

	}
	if (code == 0 && numDirEntries == 0) {
	    code = VOL_ERR_EOF;
	    break;
	}

	if (code == 0) {
	    code = osi_copyout((caddr_t)&ioDesc.position,
			(caddr_t)&((struct io_rwHyperDesc *)parm3)->position,
			sizeof (ioDesc.position));
	}
	break;
      case VOLOP_APPENDDIR:
	code = osi_copyin((caddr_t)parm3, (caddr_t)&ioDesc, sizeof(ioDesc));
	if (!code)
	    code = VOL_APPENDDIR(volp, &cdp->handle.fid, parm4,
				 ioDesc.length, ioDesc.bufferp, parm5,
				 osi_getucred());
	break;
      case VOLOP_TRUNCATE:
	code = osi_copyin((caddr_t)parm3, (caddr_t)&newSize, sizeof (newSize));
	if (!code)
	    code = VOL_TRUNCATE(volp, &cdp->handle.fid, newSize,
			      osi_getucred());
	break;
      case VOLOP_DELETE:
	code = VOL_DELETE(volp, &cdp->handle.fid, osi_getucred());
	break;
      case VOLOP_GETATTR:
	code = do_getattr(volp, &cdp->handle.fid, (caddr_t)parm3);
	break;
      case VOLOP_SETATTR:
	code = do_setattr(volp, &cdp->handle.fid, (caddr_t)parm3);
	break;
      case VOLOP_GETACL:
	aclp = (struct dfs_acl *)osi_AllocBufferSpace();
	code = VOL_GETACL(volp, &cdp->handle.fid, aclp,
			  parm4, osi_getucred());
	if (code == 0) {
	    code = osi_copyout((caddr_t)aclp, (caddr_t)parm3, sizeof (*aclp));
	}
	osi_FreeBufferSpace((struct osi_buffer *)aclp);
	break;
      case VOLOP_SETACL:
	aclp = (struct dfs_acl *) osi_AllocBufferSpace();
	code = osi_copyin((caddr_t)parm3, (caddr_t)aclp, sizeof (*aclp));
	/* Ignore parm5 for now */
	if (!code) {
	    code = VOL_SETACL(volp, &cdp->handle.fid, aclp, 0,
			      parm4, osi_getucred());
	}
	osi_FreeBufferSpace((struct osi_buffer *)aclp);
	break;
      case VOLOP_CLONE:
      case VOLOP_RECLONE:
      case VOLOP_UNCLONE:
	code = do_clone(volp, op, parm3);
	break;
      case VOLOP_VGET:
      case VOLOP_ROOT:
      case VOLOP_ISROOT:
      case VOLOP_GETZLC:
	/*
	 * Ignore these
	 */
	break;
      case VOLOP_SETVV:
	code = do_setvv(volp, (caddr_t)parm3);
	break;
      case VOLOP_SWAPVOLIDS:
	code = do_swapvolids(volp, parm3);
	break;
      case VOLOP_COPYACL:
	code = VOL_COPYACL(volp, &cdp->handle.fid, parm3,
			   parm4, parm5, osi_getucred());
	break;
      case VOLOP_SYNC:
	code = VOL_SYNC(volp, parm3);
	break;
      case VOLOP_PUSHSTATUS:
	code = VOL_PUSHSTATUS(volp);
	break;
      case VOLOP_GETNEXTHOLES:
	code = do_getnextholes(volp, &cdp->handle.fid, (caddr_t)parm3);
	break;
      case VOLOP_PROBE:
	/*
	 * Don't do anything. This volop is used to keep the fileset
	 * descriptor from being GC'd during long running fileset
	 * operations.  The act of calling vol_FindDesc() above is
	 * enough to accomplish this.
	 */
	break;
      case VOLOP_READHOLE: 
	code = do_readhole(volp, &cdp->handle.fid, (caddr_t)parm3);
	break;
      default:
	code = ESRCH;
	break;
     }

done:
    vol_PutDesc(cdp);
    VOL_RELE(volp);

    return code;
}


static int volOpenWaiters = 0;

/*
 * vol_open
 *
 * Worker for VOL_OPEN and VOL_AGOPEN.
 */
int
vol_open(
  afs_hyper_t *vidp, /* ID to open */
  u_int agid,		/* -1, or ID of aggr */
  int opnType,		/* kind of open */
  int opnErr,		/* error to hand back to non-waiters */
  int *udescp,		/* where to store descriptor index, on success */
  int *uerrp,		/* where to store error, on failure */
  int internalCall)	/* non-zero when routine called on behalf of kernel */
{
    struct vol_calldesc *cdp;
    struct volume *volp;
    struct afsFid fid;
    int code;
    unsigned char concurr;
    unsigned char gcDone=0;
    unsigned int copyClosedStates;
    vol_vp_elem_p_t vpList;
    int processVpList;

    icl_Trace3(xops_iclSetp, XVOL_TRACE_VOLOPEN,
		ICL_TYPE_LONG, AFS_hgetlo(*vidp),
		ICL_TYPE_LONG, opnType,
		ICL_TYPE_LONG, opnErr);
    if (opnErr == 0) return EINVAL;
    for (;;) {
	copyClosedStates = 0;
	processVpList = 0;
	bzero((caddr_t)&fid, sizeof(fid));
	fid.Volume = *vidp;
	if (code = volreg_Lookup(&fid, &volp)) {
	    icl_Trace2(xops_iclSetp, XVOL_TRACE_NOVOL2,
			ICL_TYPE_LONG, AFS_hgetlo(*vidp),
			ICL_TYPE_LONG, code);
	    return code;
	}
	if (agid != ((unsigned int) -1)) {
	    if (!volp->v_paggrp) {
		VOL_RELE(volp);
		return ENXIO;	/* no aggr known for volume */
	    }
	    if (volp->v_paggrp->a_aggrId != agid) {
		VOL_RELE(volp);
		return EXDEV;	/* vol not on the given aggr! */
	    }
	}

	/*
	 * GC fileset descriptor if PX is not running on the server.
	 */
	if (!(PX_UP)) {
	    vol_GCDesc(1);
	    gcDone=1;
	}

	/* Now proceed with the ordinary opening of the volume. */
	code = vol_GetDesc(&cdp);
	if (code) {
	    VOL_RELE(volp);
	    return code;
	}

	lock_ObtainWrite(&volp->v_lock);

	/*
	 * If gc has not been done in this call to vol_open,
	 * try it
	 */
	if ((volp->v_states & VOL_BUSY) && (!gcDone)) {
	    lock_ReleaseWrite(&volp->v_lock);
	    vol_GCDesc(1);
	    lock_ObtainWrite(&volp->v_lock);
	}

	/* Return error if still busy */
	if (volp->v_states & VOL_BUSY) {
	    /*
	     * Simply return the error code that the earlier
	     * guy left here.
	     */
	    icl_Trace3(xops_iclSetp, XVOL_TRACE_ISBUSY,
			ICL_TYPE_LONG, AFS_hgetlo(*vidp),
			ICL_TYPE_LONG, volp->v_states,
			ICL_TYPE_LONG, volp->v_accError);
	    code = volp->v_accError;
	    osi_assert(code);
	}

	if (!code && (volp->v_states & VOL_DEADMEAT)) {
	    code = VOLERR_PERS_DELETED;
	}

	if (code == 0) {
	    /* find out concurrency first, so we don't have to release
	     * lock and admit race conditions later on.
	     */
	    code = VOL_CONCURR(volp, opnType, opnErr, &concurr);
	}

	if (code == 0) {
	    /* Safe since two volops cannot be here at the same time */
	    osi_assert(volp->v_count >= volp->v_activeVnops);
	    if (concurr != VOL_CONCUR_ALLOPS) {

		/* Test whether there are any concurrent glue or PX
		 * references.
		 */
		if (volp->v_activeVnops > 0) {
		    /* Not volop_opened, but somebody is doing some
		     * vnode operations.
		     * We have to inform the file system dependent layer of our
		     * intentions of calling VOL_OPEN, so that it can block any
		     * vnode ops that enter the layer.
		     */
		    icl_Trace4(xops_iclSetp, XVOL_TRACE_VNOPWAIT,
			       ICL_TYPE_LONG, AFS_hgetlo(*vidp),
			       ICL_TYPE_LONG, (int)concurr,
			       ICL_TYPE_LONG, volp->v_count,
			       ICL_TYPE_LONG, volp->v_activeVnops);
		    volp->v_states |= VOL_GRABWAITING;
		    ++volOpenWaiters;
		    osi_SleepW(&volp->v_accStatus, &volp->v_lock);
		    icl_Trace1(xops_iclSetp, XVOL_TRACE_VNOPWAITDONE,
			       ICL_TYPE_LONG, AFS_hgetlo(*vidp));
		    /* We're not going to use this cdp, so free it. */
		    vol_SetDeleted(cdp);
		    vol_PutDesc(cdp);
		    cdp = NULL;
		    VOL_RELE(volp);
		    --volOpenWaiters;
		    continue;	/* the big loop, doing the lookup again too. */
		}
	    } /* concurr != VOL_CONCUR_ALLOPS */
	}	/* code == 0 */

	if (!code) {
	    volp->v_states |= VOL_BUSY;	/* it's ours! */
	    volp->v_concurrency = concurr;
	    volp->v_accError = opnErr;
	    volp->v_accStatus = opnType;
	    volp->v_procID = osi_GetPid();
	    /*
	     * The FS-specific code can take a long time,
	     * so release this here.
	     */
	    lock_ReleaseWrite(&volp->v_lock);
	    code = VOL_OPEN(volp, opnType, opnErr, &cdp->handle);
	    lock_ObtainWrite(&volp->v_lock);
	    if (code) {
		icl_Trace2(xops_iclSetp, XVOL_TRACE_VOLOPENERR,
			   ICL_TYPE_LONG, AFS_hgetlo(*vidp),
			   ICL_TYPE_LONG, code);
		(void) vol_close(volp, &vpList);
		processVpList = 1;
	    } else {
		volp->v_states |= VOL_OPENDONE;
	    }
	}
	if (!code) {
	    /* it's still ours. */
	    /* don't do copyout if this is an internal call */
	    if (internalCall) {
		*udescp = cdp->descId;
		code = 0;
	    } else {
		code = osi_copyout((caddr_t)&cdp->descId, (caddr_t)udescp,
				   sizeof (cdp->descId));
	    }
	    /* Set these for the use of the VOL_CLOSE call */
	    cdp->handle.opentype = opnType;
	    cdp->handle.volp = volp;	/* we're holding it for now */
	    if (code) {	/* osi_copyout failed */
		lock_ReleaseWrite(&volp->v_lock);
		(void) VOL_CLOSE(volp, &cdp->handle, 1);
		lock_ObtainWrite(&volp->v_lock);

		icl_Trace2(xops_iclSetp, XVOL_TRACE_COPYOUTERR,
			   ICL_TYPE_LONG, AFS_hgetlo(*vidp),
			   ICL_TYPE_LONG, code);
		if (volp->v_states & VOL_BUSY) {
		    (void) vol_close(volp, &vpList);
		    processVpList = 1;

		    /* We'll check below whether this had been destroyed */
		    copyClosedStates = volp->v_states;
		}
		cdp->handle.opentype = 0;
		/* cdp isn't going to be holding the volume reference */
		cdp->handle.volp = (struct volume *)0;
	    } else {
		icl_Trace1(xops_iclSetp, XVOL_TRACE_OPENDONE,
			   ICL_TYPE_LONG, AFS_hgetlo(*vidp));
		/* Transfer reference to cdp->handle.volp */
		volp->v_count++;/* lock-free VOL_HOLD for cdp reference */
	    }
	} else {
	    /* don't do copyout if this is an internal call */
	    if (internalCall) {
		*uerrp = code;
		code = 0;
	    } else {
		code = osi_copyout((caddr_t)&code,
				   (caddr_t)uerrp, sizeof (code));
	    }
	    if (!code) code = EBUSY;	/* Special code saying to
					   look at the uerrp result. */
	}
	lock_ReleaseWrite(&volp->v_lock);

	if (processVpList)
	    (void) vol_ProcessDeferredReles(vpList);

	if (code) {
	    /* Eliminate this cdp structure from the last vol_PutDesc call */
	    /* since the user won't be able to use it to refer to a volume */
	    vol_SetDeleted(cdp);
	    /* If we turned BUSY off, the fileset might have been destroyed */
	    if (copyClosedStates & VOL_DEADMEAT)
		(void) vol_Detach(volp, 1);
	}
	vol_PutDesc(cdp);
	VOL_RELE(volp);
	return code;
    }
}

/* Procedure to handle a low ref count on VOL_RELE.  Call with volp write-locked. */
void vol_RCZero(struct volume *volp)
{
    if (volp->v_states & VOL_GRABWAITING) {
	icl_Trace1(xops_iclSetp, XVOL_TRACE_RCGRABWAKE,
		   ICL_TYPE_LONG, AFS_hgetlo(volp->v_volId));
	volp->v_states &= ~VOL_GRABWAITING;
	osi_Wakeup(&volp->v_accStatus);
    }
    if (volp->v_states & VOL_LOOKUPWAITING) {
	icl_Trace1(xops_iclSetp, XVOL_TRACE_RCLOOKWAKE,
		   ICL_TYPE_LONG, AFS_hgetlo(volp->v_volId));
	volp->v_states &= ~VOL_LOOKUPWAITING;
	osi_Wakeup(&volp->v_states);
    }
}

static int vol_BulkSetStatus(
    u_int arrayLen,
    struct vol_statusDesc *statusArray)
{
    int code;
    struct vol_calldesc *callDescArray[VOL_MAX_BULKSETSTATUS];
    int i, j;
    struct volume *volp;
    struct volume *vol1p = 0, *vol2p = 0;
    int anySwapped;
    struct volume *volSortP[VOL_MAX_BULKSETSTATUS];

    for (i = 0; i < arrayLen; ++i) {
	if (code = vol_FindDesc(statusArray[i].vsd_volDesc,
				&callDescArray[i])) {
	    --i;	/* bounce out with failure code */
	    break;
	}

	volp = volSortP[i] = statusArray[i].vsd_volp =
	    callDescArray[i]->handle.volp;
	VOL_HOLD(volp);

	VOLCHECK(volp, VOLOP_SETSTATUS);
	if (!code &&
	    (statusArray[i].vsd_mask & VOL_STAT_VOLID) &&
	    !AFS_hsame(volp->v_volId,
		       statusArray[i].vsd_status.vol_st.volId)) {
	    VOLCHECK(volp, VOLOP_SWAPVOLIDS);
	    if (!code) {
		if (vol1p == 0) {
		    vol1p = volp;
		} else if (vol2p == 0) {
		    vol2p = volp;
		} else {
		    code = EINVAL;	/* too many ID-settings */
		}
	    }
	}

	if (!code && (volp->v_volOps != statusArray[0].vsd_volp->v_volOps))
	    code = EXDEV;

	if (code)
	    break;	/* bounce out on any failure */
    }

    if (code) {	/* clean up on failure */
	for (j = 0; j <= i; ++j) {
	    VOL_RELE(statusArray[j].vsd_volp);
	    vol_PutDesc(callDescArray[j]);
	}
	return code;
    }

    /* Acquire write locks, but in order.  arrayLen will never be large. */
    anySwapped = 1;	/* whether any swaps seen */
    while (anySwapped != 0) {
	anySwapped = 0;	/* no swaps yet */
	for (j = 1; j < arrayLen; ++j) {
	    if (AFS_hcmp(volSortP[j-1]->v_volId, volSortP[j]->v_volId) > 0) {
		volp = volSortP[j-1];
		volSortP[j-1] = volSortP[j];
		volSortP[j] = volp;
		anySwapped = 1;	/* needed a swap */
	    }
	}
    }
    for (j = 0; j < arrayLen; ++j)
	lock_ObtainWrite(&volSortP[j]->v_lock);

    code = VOL_BULKSETSTATUS(statusArray[0].vsd_volp,
			      arrayLen, statusArray);

    if (vol1p != 0) {
	if (vol2p != 0) {
	    vol_SwapIdentities(vol1p, vol2p);
	} else {
	    code = EINVAL;
	}
    }
    for (j = 0; j < arrayLen; ++j) {
	lock_ReleaseWrite(&statusArray[j].vsd_volp->v_lock);
	VOL_RELE(statusArray[j].vsd_volp);
	vol_PutDesc(callDescArray[j]);
    }
    return code;
}

/*
 * vol_close
 *
 * NOTES
 *    1. Caller has LOCKED the volp before calling us.
 *
 *    2. If the caller does not want us to release the volp lock to
 *       process the deferred vnode rele's, then the caller should
 *       pass in a non-null vpList. We also assume that the caller
 *       will call vol_ProcessDeferredReles with the list of vnode
 *       after unlocking the volp..
 */
/* SHARED */
void vol_close(volp, vpListP)
  struct volume *volp;
  vol_vp_elem_p_t *vpListP;
{
    vol_vp_elem_p_t releVpList;

    osi_assert(lock_WriteLocked(&volp->v_lock));

    if (volp->v_states & (VOL_BUSY|VOL_OPENDONE)) {
	volp->v_states &= ~(VOL_BUSY|VOL_OPENDONE);
	volp->v_accStatus = volp->v_accError = 0;
	volp->v_concurrency = 0; /* allow vnode ops in */
	volp->v_procID = 0;
    }

    if (volp->v_stat_st.volMoveTimeout != 0)
	vol_SetMoveTimeoutTrigger(volp);

    /*
     * Wakeup anyone waiting for this volume to be available
     */
    vol_RCZero(volp);

    releVpList = volp->v_vp;
    volp->v_vp = NULL;

    if (vpListP != NULL) {
	*vpListP = releVpList;
    } else {
	lock_ReleaseWrite(&volp->v_lock);
	(void) vol_ProcessDeferredReles(releVpList);
	lock_ObtainWrite(&volp->v_lock);
    }
}

