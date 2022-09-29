/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xvfs_vnode.c,v 65.6 1998/04/01 14:16:44 gwehrman Exp $";
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
 * $Log: xvfs_vnode.c,v $
 * Revision 65.6  1998/04/01 14:16:44  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added forward prototype declaration for xvfs_InitFromOFuns().
 *
 * Revision 65.5  1998/03/23 16:37:27  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/02/02 21:45:34  lmc
 * Changes for prototypes, explicit casts added after checking
 * that they are okay.
 *
 * Revision 65.2  1997/11/06  20:00:47  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:24  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.938.1  1996/10/02  19:04:11  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  18:59:42  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1990 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_ufs.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/osi_user.h>
#include <dcedfs/lock.h>
#include <dcedfs/common_data.h>
#include <dcedfs/aclint.h>
#include <dcedfs/dacl.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/xvfs_export.h>
#include <dcedfs/volume.h>
#include <dcedfs/tkc.h>


RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvnode/RCS/xvfs_vnode.c,v 65.6 1998/04/01 14:16:44 gwehrman Exp $")

struct xvfs_opspair {
    struct xvfs_opspair *next;
    struct osi_vnodeops *oldOps;
    struct xvfs_vnodeops *newOps;
};

struct xvfs_opspair *xvfs_AllOpsPairs = 0;

#ifdef SGIMIPS
int xvfs_InitFromOFuns(struct osi_vnodeops *aofunsp,
		       struct xvfs_vnodeops *afunsp);
#endif

void
xvfs_NullTxvattr(struct Txvattr *txvattrp)
{
    osi_Memset((void *)txvattrp, 0xff, sizeof (*txvattrp));
}

#ifndef	EFS_SINGLE
/*
 * dfs_adminGroupID should be initialized during fxd.
 */
long dfs_adminGroupID = -2;
/*
 * The routine is used to determine whether the caller is a member of
 * the system administrator group or not.
 */
int xvfs_IsAdminGroup(osi_cred_t *credp)
{
    int i;
    int ngroups;

    if (dfs_adminGroupID != -2) {
	if (osi_GetGID(credp) == dfs_adminGroupID) {
	    return 1; /* yes! */
	}
	ngroups = osi_GetNGroups(credp);
	for (i = 0; i < ngroups; i++) {
	    if (credp->cr_groups[i] == dfs_adminGroupID)
		return 1; /* yes! */
	}
    }
    return 0;
}

/*
 * Set the dfs_adminGroupID
 */
void
xvfs_SetAdminGroupID(long id)
{
    dfs_adminGroupID = id;
}

/*
 * Enter some old vnode ops into a conversion hash table, which is just a
 * linked list for now.
 */
static struct xvfs_vnodeops *
GetNewVnodeOpsFromOld(struct osi_vnodeops *aoldOpsp)
{
    struct xvfs_opspair *tp;
    struct xvfs_vnodeops *np;

    /* Look for the vnode ops and see if we've already done a conversion
     * from this ops.  If so, return it.
     *
     * Also, we look to see if these vnode ops are already converted.
     * If they are, we just return the same ops pointer.  This
     * path is taken when UFS recycles an inode: it clears v_flag (which
     * clears the converted flag), but doesn't reset the vnode ops pointer.
     * We *really* don't want to convert the vnode a second time.
     */
    for	(tp = xvfs_AllOpsPairs; tp; tp = tp->next) {
	if (aoldOpsp == (struct osi_vnodeops *) tp->newOps)
	    return (struct xvfs_vnodeops *) aoldOpsp;

	if (aoldOpsp == tp->oldOps)
	    return tp->newOps;
    }

    /*
     * Create the new operations, we didn't find it in the new operations
     * hash table. We can either use xvfs_InitFromXFuns or xvfs_InitFromOFuns,
     * depending on the style parameter.
     */
    tp = (struct xvfs_opspair *) osi_Alloc(sizeof (struct xvfs_opspair));
    np = (struct xvfs_vnodeops *) osi_Alloc(sizeof(struct xvfs_vnodeops));
    xvfs_InitFromOFuns(aoldOpsp, np);
    tp->next = xvfs_AllOpsPairs;
    xvfs_AllOpsPairs = tp;
    tp->oldOps = aoldOpsp;
    tp->newOps = np;
    return np;
}

/*
 * Convert a vnode to the proper converted vnode, by aiming the v_op field to
 * a v_op field whose type is xvnodeops.  Convert is only called on
 * old-interface vnodes (e.g. ufs, not episode or cm); the new ones are
 * converted on creation, by putting a ptr to an xvfs_vnodeops structure in
 * place.
 *
 * Note that episode and cm vnodes should be created with the V_CONVERTED
 * flag already set, so that this function returns immediately.
 */
int
xvfs_convert(struct vnode *avp)
{
    struct xvfs_vnodeops *newOps;

    if (IS_CONVERTED(avp))
	return 0;
    /* some systems can't convert all types of vnodes safely, usually because
     * someone's checking for equality of the v_ops field.
     */
    if (osi_CantCVTVP(avp))
	return -1;
    SET_CONVERTED(avp);
#ifdef SGIMIPS
    newOps = GetNewVnodeOpsFromOld(avp->v_fops);
    avp->v_fops = (struct osi_vnodeops *) newOps;
#else
    newOps = GetNewVnodeOpsFromOld(avp->v_op);
    avp->v_op = (struct osi_vnodeops *) newOps;
#endif
    return 0;
}

#endif	/* EFS_SINGLE */

#ifndef	EFS_SINGLE

/* convert a device to the underlying UFS/Episode device inode.  It won't
 * be usable as a device anymore.
 */
int
xvfs_ConvertDev(struct vnode **avpp)
{
#ifdef SGIMIPS
    int code;
#else
    long code;
#endif

#ifdef AFS_SUNOS5_ENV
    struct vnode *tvp;
    struct vnode *nvp;
#endif

#ifdef AFS_SUNOS5_ENV
    tvp = *avpp;
    osi_RestorePreemption(0);
    code = VOPO_REALVP(tvp, &nvp);
    (void) osi_PreemptionOff();
    if (code == 0) {
	/* new vnode returned, use it instead, and then convert it */
	OSI_VN_HOLD(nvp);
	OSI_VN_RELE(tvp);
	*avpp = nvp;
	code = xvfs_convert(nvp);
    } else {
	code = xvfs_convert(tvp);
    }
#else
   code = xvfs_convert(*avpp);
#endif

    return code;
}

#endif	/* EFS_SINGLE */

/*
 * Simple noop function
 */
int xvfs_noop()
{
    return 0;
}

/*
 * The following functions, xvn_getacl and xvn_setacl, have the role of
 * native VOP_GETACL/VOP_SETACL functions, much as the xglue_XXX functions
 * have in the system-dependent libraries included in subdirectories.  These
 * functions exist because many/most native vnode-operation (the native
 * ``struct vnodeops'') arrays do not include functions for getting and
 * setting DCE ACLs of any form.  Thus, these functions need to have token
 * and volume-op glue applied before calling the VOPX_GETACL or VOPX_SETACL
 * sub-functions.
 */

/* Define an errno value for nonconverted local vnodes. */
#if  defined(AFS_AIX31_ENV) || defined(AFS_SUNOS5_ENV) || defined(AFS_OSF1_ENV) || defined(SGIMIPS)
#define E_NOT_CONVERTED ENOSYS
#else
#define E_NOT_CONVERTED EIO
#endif

/*
 * Get ACL -- veneer function
 */
int xvn_getacl (
    struct vnode *vp,
    struct dfs_acl *aclp,
    int w,
    osi_cred_t *credp)
{
    struct tkc_vcache *vcp;
#ifdef SGIMIPS
    int code;
#else
    long code;
#endif
    struct volume *volp;
    opaque dmptr = NULL;

    /* There are four interesting cases: (a) raw vnode, (b) unexported UFS or
     * Episode vnode, (c) exported UFS or Episode vnode, and (d) the CM.
     * (a) For the raw vnode, we return ENOSYS or EIO.
     * (b) For unexported UFS or Episode, we call the function without token
     *     glue.
     * (c) For exported UFS or Episode, we call the function with token glue.
     * (d) For the CM, we call the function without token glue.
     * In cases (b) (c) (d), we call the function with volume glue.
     * xvfs_GetVolume will return the same result for raw vnodes and for
     * non-exported ones (ones without a volume pointer)--our cases (a), (b),
     * and (d).  We have to distinguish (a) from (b) and (d), so we call
     * IS_CONVERTED to distinguish that one by hand.
     */

    icl_Trace1(tkc_iclSetp, TKC_TRACE_GETACLSTART,
		ICL_TYPE_POINTER, (long) vp);
    if (!IS_CONVERTED (vp)) {
	/* (a) non-converted:  bail out randomly */
	return E_NOT_CONVERTED;
    }

#ifdef SGIMIPS
    code = (int)xvfs_GetVolume(vp, &volp);
#else
    code = xvfs_GetVolume(vp, &volp);
#endif
    if (code) {
	/* some error even finding the volume */
	icl_Trace1(tkc_iclSetp, TKC_TRACE_GETACLGVFAILED,
		  ICL_TYPE_LONG, code);
	return(code);
    }
redo:
    if (!xvfs_NeedTokens(volp)) {
	/* cases (b) and (d): mostly null volp, sometimes not */
	if (!(code = vol_StartVnodeOp(volp, VNOP_GETACL, 0, &dmptr))) {
#ifdef SGIMIPS
	    VOPX_GETACL (vp, aclp, w, credp, code);
#else
	    code = VOPX_GETACL (vp, aclp, w, credp);
#endif
	    vol_EndVnodeOp(volp, &dmptr);
	}
    } else {
	/* case (c): non-null, exported, R/W, volp */
	if (vcp = tkc_Get(vp, TKN_STATUS_READ, 0)) {
	    if (!(code = vol_StartVnodeOp(volp, VNOP_GETACL, 0, &dmptr))) {
#ifdef SGIMIPS
		VOPX_GETACL (vp, aclp, w, credp, code);
#else
		code = VOPX_GETACL (vp, aclp, w, credp);
#endif
		vol_EndVnodeOp(volp, &dmptr);
	    }
	    tkc_Put(vcp);
	} else {
	    icl_Trace0(tkc_iclSetp, TKC_TRACE_GETACLGETFAILED);
	    code = EINVAL;
	}
    }
    if (dmptr && volp) {
	if (VOL_DMWAIT(volp, &dmptr)) goto redo;
	VOL_DMFREE(volp, &dmptr);
    }
    xvfs_PutVolume(volp);
    icl_Trace2(tkc_iclSetp, TKC_TRACE_GETACLDONE,
		ICL_TYPE_POINTER, (long) vp,
		ICL_TYPE_LONG, code);
    return (code);
}

/*
 * Set ACL -- veneer function
 */
int xvn_setacl(
    struct vnode *vp,
    struct dfs_acl *aclp,
    struct vnode *svp,
    long dw,
    long sw,
    osi_cred_t *credp)
{
    struct tkc_vcache *vcp;
#ifdef SGIMIPS
    int code;
#else
    long code;
#endif
    struct volume *volp;
    opaque dmptr = NULL;

    /* The analysis is really the same here as in xvn_getacl, above. */
    icl_Trace1(tkc_iclSetp, TKC_TRACE_SETACLSTART,
		ICL_TYPE_POINTER, (long) vp);
    if (!IS_CONVERTED (vp)) {
	/* (a) non-converted:  bail out randomly */
	return E_NOT_CONVERTED;
    }

#ifdef SGIMIPS
    code = (int)xvfs_GetVolume(vp, &volp);
#else
    code = xvfs_GetVolume(vp, &volp);
#endif
    if (code) {
	/* some error even finding the volume */
	icl_Trace1(tkc_iclSetp, TKC_TRACE_SETACLGVFAILED,
		  ICL_TYPE_LONG, code);
	return(code);
    }
redo:
    if (!xvfs_NeedTokens(volp)) {
	/* cases (b) and (d): mostly null volp, sometimes not */
	if (!(code = vol_StartVnodeOp(volp, VNOP_SETACL, 0, &dmptr))) {
#ifdef SGIMIPS
	    /*  We don't do anything with this on SGI file system 'cause
		ACLs aren't supported.  Since dw and sw are meaningless,
		I cast them to ints to get rid of warnings. */
	    VOPX_SETACL (vp, aclp, svp, (int)dw, (int)sw, credp, code);
#else
	    code = VOPX_SETACL (vp, aclp, svp, dw, sw, credp);
#endif
	    vol_EndVnodeOp(volp, &dmptr);
	}
    } else {
	/* case (c): non-null, exported, R/W, volp */
	if (vcp = tkc_Get(vp, TKN_STATUS_WRITE, 0)) {
	    if (!(code = vol_StartVnodeOp(volp, VNOP_SETACL, 0, &dmptr))) {
#ifdef SGIMIPS
	        /*  We don't do anything with this on SGI file system 'cause
		ACLs aren't supported.  Since dw and sw are meaningless,
		I cast them to ints to get rid of warnings. */
	        VOPX_SETACL (vp, aclp, svp, (int)dw, (int)sw, credp, code);
#else
		code = VOPX_SETACL (vp, aclp, svp, dw, sw, credp);
#endif
		vol_EndVnodeOp(volp, &dmptr);
	    }
	    tkc_Put(vcp);
	} else {
	    icl_Trace0(tkc_iclSetp, TKC_TRACE_SETACLGETFAILED);
	    code = EINVAL;
	}
    }
    if (dmptr && volp) {
	if (VOL_DMWAIT(volp, &dmptr)) goto redo;
	VOL_DMFREE(volp, &dmptr);
    }
    xvfs_PutVolume(volp);
    icl_Trace2(tkc_iclSetp, TKC_TRACE_SETACLDONE,
		ICL_TYPE_POINTER, (long) vp,
		ICL_TYPE_LONG, code);
    return (code);
}

#ifndef	EFS_SINGLE

/*
 * We initialize the extended VNOPS vector as follows:
 *
 *	OOPS  point to xglue_*
 *	XOPS  point to the passed in 'axfuns'
 *	NOPS  point to the passed in 'ofuncs'
 *
 * The CM calls   with  'axfuns' pointing to the CM VNOPS;
 * EPISODE calls  with 'axfuns' pointing to the EFS VNOPS;
 * Everyone calls with 'ofuncs' pointing to n<os>_ops.
 *
 * This results in an extended VNOPS vector such that
 * VOP_xxx calls on vnodes in the AFS VFS generate a
 * call chain like this:
 *
 *	VOP_xxx
 *        xglue_xxx()     * does tkc synchronization *
 *	    n<os>_xxx()   * maps VFS to VFS+         *
 *	      cm_xxx()
 *
 * VOP_xxx calls on vnodes in the LFS VFS generate a
 * call chain like this:
 *
 *	VOP_xxx
 *        xglue_xxx()     * does tkc synchronization *
 *	    n<os>_xxx()   * maps VFS to VFS+         *
 *	      efs_xxx()
 */
#if 0
int xvfs_InitFromXFuns(
    struct xvfs_xops *axfuns,
    struct xvfs_vnodeops *afuns,
    struct osi_vnodeops *ofuncs)
{
    afuns->xops = *axfuns;
    afuns->nops = *ofuncs;
    afuns->oops = xglue_ops;
#ifdef AFS_AIX31_ENV
    /* Fix these for AIX VMM */
    afuns->nops.vn_strategy = axfuns->vn_strategy; /* Use X-op */
    afuns->oops.vn_strategy = axfuns->vn_strategy; /* Use X-op */
#endif

    return 0;
}
#endif /* 0 */

int xvfs_InitFromXOps (
    struct xvfs_xops *axfuns,
    struct xvfs_vnodeops *afuns)
{

#ifdef	AFS_AIX_ENV
#define O2X_OPS naix_ops
#endif
#ifdef	AFS_OSF_ENV
#define O2X_OPS nosf_ops
#endif
#ifdef	AFS_HPUX_ENV
#define O2X_OPS nux_ops
#endif
#ifdef  AFS_SUNOS5_ENV
#define O2X_OPS nsun5_ops
#endif
#ifdef AFS_IRIX_ENV
#define O2X_OPS  sgi2cm_ops
#endif /* AFS_IRIX_ENV */     
#ifdef	AFS_DEFAULT_ENV
#error  "you must supply an ops vector in your glue code."
#endif
#ifndef O2X_OPS
#error  "you must supply an ops vector in your glue code."
#endif /* O2X_OPS */

    extern struct osi_vnodeops O2X_OPS;
    struct osi_vnodeops *ofuncs=&O2X_OPS;

    afuns->xops = *axfuns;
    afuns->nops = *ofuncs;
    afuns->oops = xglue_ops;
#ifdef AFS_AIX31_ENV
    /* Fix these for AIX VMM */
    afuns->nops.vn_strategy = axfuns->vn_strategy; /* Use X-op */
    afuns->oops.vn_strategy = axfuns->vn_strategy; /* Use X-op */
#endif

    return 0;
}

/*
 * Used by GetNewVnodeOpsFromOld (above) to generate new style vnode operations
 * from original-style vnode operations.  In other words, this is used by ufs
 * (not episode or cm) to get extended vnode structures for its vnodes.
 *
 * We initialize the extended VNOPS vector as follows:
 *
 *	OOPS  point to xglue_*
 *	XOPS  point to xufs_*
 *	NOPS  point to the passed in 'aofuns'
 *
 * The PX calls xvfs_convert(vp) to convert a vnode to a vnode
 * with the extended vnode ops.  This results in a call
 * to this routine, where 'aofuns' points to ufs vnops or
 * efs vnops, depending on which VFS was referenced by PX.
 *
 * This results in an extended VNOPS vector such that
 * VOP_xxx calls on vnodes in the AFS VFS generate a
 * call chain like this:
 *
 *	VOP_xxx
 *        xglue_xxx()     * does tkc synchronization *
 *	    ufs_xxx()
 *
 * or
 *
 *	VOP_xxx
 *        xglue_xxx()     * does tkc synchronization *
 *	    nosf_xxx()    * maps VFS to VFS+         *
 *	      efs_xxx()
 *
 *
 * Calls via VOPX (e.g. by PX) generate a call chain like this:
 *
 *	VOPX_xxx
 *	  efs_xxx()
 *
 * or
 *
 *      VOPX_xxx
 *        xufs_xxx()      * maps VFS+ to VFS         *
 *          ufs_xxx
 */
int xvfs_InitFromOFuns(
    struct osi_vnodeops *aofunsp,
    struct xvfs_vnodeops *afunsp)
{
    afunsp->nops = *aofunsp;
    afunsp->xops = xufs_ops;
    afunsp->oops = xglue_ops;

    /* Insist that we don't get loops due to converting a converted vnode.  As
     * an approximation, sample one member of the ops structure to make sure
     * the the nops and oops vectors aren't the same.  The getattr function is
     * a good choice since it is called early and often on newly returned
     * vnodes. */
#ifdef AFS_SUNOS5_ENV
#define SAMPLE_OP vop_getattr
#else
#define SAMPLE_OP vn_getattr
#endif
    afsl_Assert (afunsp->nops.SAMPLE_OP != afunsp->oops.SAMPLE_OP);
#undef SAMPLE_OP
    return 0;
}

/*
 * VFS stuff
 *
 * Deliberately written by analogy from the vnode stuff.
 */

struct xvfs_vfsopspair {
    struct xvfs_vfsopspair *next;
    struct osi_vfsops *oldOps;
    struct xvfs_vfsops *newOps;
};

struct xvfs_vfsopspair *xvfs_AllVfsOpsPairs = 0;

struct xvfs_vfsops *GetNewVfsOpsFromOld(
    struct osi_vfsops *aoldOpsp,
    int (*getvolfn)())
{
    struct xvfs_vfsopspair *tp;
    struct xvfs_vfsops *np;

    for (tp = xvfs_AllVfsOpsPairs; tp; tp = tp->next) {
	/* First, catch the case where we're already converted */
	if (aoldOpsp == (struct osi_vfsops *) tp->newOps)
	    return (struct xvfs_vfsops *) aoldOpsp;
	/* Next, catch the case where the appropriate conversion is there */
	if (aoldOpsp == tp->oldOps)
	    return tp->newOps;
    }
    /* Now the case where we have to do the conversion ourselves */
    tp = (struct xvfs_vfsopspair *) osi_Alloc (sizeof (struct xvfs_vfsopspair));
    np = (struct xvfs_vfsops *) osi_Alloc (sizeof (struct xvfs_vfsops));
    xvfs_InitFromVFSOps (aoldOpsp, np, getvolfn);
    tp->next = xvfs_AllVfsOpsPairs;
    xvfs_AllVfsOpsPairs = tp;
    tp->oldOps = aoldOpsp;
    tp->newOps = np;
    return np;
}

void
xvfs_InitFromVFSOps(
    struct osi_vfsops *aofuns,
    struct xvfs_vfsops *afuns,
    int (*getvolfn)())
{
    afuns->vfsops = *aofuns;
    afuns->xvfsops = *aofuns;
    afuns->xvfsops.vfs_unmount = xglue_unmount;
    afuns->xvfsops.vfs_root = xglue_root;
#ifdef AFS_OSF_ENV
    afuns->xvfsops.vfs_fhtovp = xglue_fhtovp;
#else
    afuns->xvfsops.vfs_vget = xglue_vget;
#endif /* AFS_OSF_ENV */
    afuns->vfsgetvolume = getvolfn;
}

#endif	/* EFS_SINGLE */
