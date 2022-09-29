/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: ufs_agops.c,v 65.6 1999/02/08 20:52:41 mek Exp $";
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
 * $Log: ufs_agops.c,v $
 * Revision 65.6  1999/02/08 20:52:41  mek
 * Remove references to XFS include files. (No longer needed.)
 *
 * Revision 65.5  1998/03/23 16:42:37  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/02/26 23:58:05  lmc
 * Changed type of "code".
 *
 * Revision 65.2  1997/11/06  20:00:51  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:28  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.132.1  1996/10/02  21:02:18  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:49:48  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */

/*
 * Aggregate ops for UFS file systems
 */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/lock.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/osi_ufs.h>
#include <dcedfs/aggr.h>
#include <dcedfs/astab.h>
#include <dcedfs/volume.h>
#include <dcedfs/ufs_trace.h>
#include <ufs.h>
#ifdef AFS_OSF_ENV
#include <sys/specdev.h>
#endif /* AFS_OSF_ENV */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/ufsops/RCS/ufs_agops.c,v 65.6 1999/02/08 20:52:41 mek Exp $")

#if	  defined(AFS_HPUX_ENV) && __hpux1000p
#include <sys/statvfs.h>
#endif	  /* defined(AFS_HPUX_ENV) && __hpux1000p */

struct icl_set *xufs_iclSetp;
/* XFS interface to get file system name from the mount structure */
#ifdef AFS_IRIX_ENV
extern char *xfs_get_mount_fsname(struct vfs *);
#endif /* AFS_IRIX_ENV */

extern struct volumeops vol_ufsops;


/* Called by vol_ufsGetStatus() */
int ag_ufsStat(aggrp, astatp)
    struct aggr *aggrp;
    struct ag_status *astatp;
{
#ifdef SGIMIPS
    int code;
#else
    long code;
#endif
    struct osi_vfs *vfsp = UFS_AGTOVFSP(aggrp);
#if defined(AFS_HPUX_ENV) && __hpux1000p
    struct statvfs statb;
#else  /*   defined(AFS_HPUX_ENV) && __hpux1000p */
    osi_statfs_t statb;
#endif /*   defined(AFS_HPUX_ENV) && __hpux1000p */
    struct ag_status_dy agDyn;
    long totalblks, reserved, availblks, bsize, nfree, available, factor;

    icl_Trace1(xufs_iclSetp, UFS_TRACE_ENTER_AGSTAT, ICL_TYPE_POINTER, aggrp);
#ifdef AFS_AIX_ENV
    code = vfstovfsop(vfsp, vfs_statfs)(vfsp, &statb);
#elif defined(AFS_OSF_ENV)
    VFS_STATFS(vfsp, code);
    statb = vfsp->m_stat;
    statb.f_flags = vfsp->m_flag & M_VISFLAGMASK;
#elif defined(AFS_SUNOS5_ENV)
    osi_RestorePreemption(0);
    code = VFS_STATVFS(vfsp, &statb);
    osi_PreemptionOff();
#elif defined(AFS_HPUX_ENV) /* AFS_SUNOS5_ENV */
#if __hpux1000p
    code = VFS_STATVFS(vfsp, &statb);
#else
    code = VFS_STATFS(vfsp, &statb);
#endif /* __hp1000p */
#endif /* AFS_HPUX_ENV */
#ifdef AFS_IRIX_ENV
    osi_RestorePreemption(0);
    VFS_STATVFS(vfsp, &statb, NULL, code);
    osi_PreemptionOff();
#endif /* AFS_IRIX_ENV */
    if (code) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_AGSTAT, ICL_TYPE_LONG, code);
	return code;
    }

    /*
     * The following code is used to calculate the proper value for such
     * fields as a_totalUsable, a_free and a_minFree. It's unclear if we
     * should instead pass only raw data and allow the user-level callers
     * (i.e. volume server) to do its own interpretation...
     */
    bzero((char *) &agDyn, sizeof (agDyn));
    agDyn.blocks = totalblks = statb.f_blocks;
#ifdef AFS_OSF_ENV
    agDyn.fragsize = agDyn.blocksize = bsize = statb.f_fsize;
#elif defined(AFS_SUNOS5_ENV)
    agDyn.blocksize = statb.f_bsize;
    agDyn.fragsize = bsize = statb.f_frsize;
#elif defined(AFS_IRIX_ENV)
    agDyn.blocksize = statb.f_bsize;
    agDyn.fragsize = bsize = statb.f_frsize;
#elif defined(AFS_HPUX_ENV) && __hpux1000p
    agDyn.fragsize = agDyn.blocksize = bsize = statb.f_frsize;
#else
    agDyn.fragsize = agDyn.blocksize = bsize = statb.f_bsize;
#endif /* AFS_OSF_ENV */

    nfree = statb.f_bfree;
    available = statb.f_bavail;
    reserved = nfree - available;
    /*
     * convert from fragment units into 1K units
     */
    if (bsize > 1024) {
	factor = bsize / 1024;
	nfree *= factor;
	totalblks *= factor;
	reserved *= factor;
    } else if (bsize < 1024) {
	factor = 1024 / bsize;
	nfree /= factor;
	totalblks /= factor;
	reserved /= factor;
    }
    availblks = totalblks - reserved;
    agDyn.totalUsable = availblks;
    agDyn.realFree = availblks - (totalblks - nfree);
    agDyn.minFree = reserved;

    astatp->ag_st = aggrp->a_stat_st;
    astatp->ag_dy = agDyn;

    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_AGSTAT, ICL_TYPE_LONG, 0);
    return 0;
}


static int ag_ufsVolCreate(aggrp, volIdp, statusp, flags)
    struct aggr *aggrp;
    afs_hyper_t *volIdp;
    struct vol_status *statusp;
    int flags;	/* currently ignored for UFS */
{
    int code = 0;

    icl_Trace1(xufs_iclSetp, UFS_TRACE_ENTER_AGCREATE, ICL_TYPE_LONG, flags);
    if (!AFS_hiszero(UFS_AGTOVOLID(aggrp))) {
	/*
	 * If we're trying to create a fileset with a different
	 * fileset ID, we return an error since a ufs aggregate can only
	 * hold one fileset and that fileset was created when the aggregate
	 * was attached.  If we're trying to create a volume with the
	 * same volume ID, we simply ignore it and return success.
	 * If one needs to modify the volumes' attrs such as its name,
	 * one should call the appropriate VOL_ ops routines.
	 */
	if (AFS_hcmp(UFS_AGTOVOLID(aggrp), *volIdp) != 0)
	    code = ENOSPC;

	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_AGCREATE, ICL_TYPE_LONG, code);
	return code;
    }
    /*
     * It is not valid for a UFS aggregate to exist w/o a fileset
     */
    panic("ag_ufsVolCreate: no fileset\n");
    /* NOTREACHED */
#ifdef SGIMIPS
    return 0;		/* We'll never get here, but the compiler is happy now*/
#endif
}


/*
 * Returns a pointer to the volume structure in aggregate index, index.
 */
static int ag_ufsVolInfo(aggrp, index, volp)
    register struct aggr *aggrp;
    register int index;
    struct volume *volp;
{
#ifdef AFS_AIX31_ENV
    struct vmount *vmtp = AGRTOUFS(aggrp)->vfsp->vfs_mdata;
    long slen;
#endif /* AFS_AIX31_ENV */

    icl_Trace1(xufs_iclSetp, UFS_TRACE_ENTER_AGINFO, ICL_TYPE_LONG, index);

    if (index != 0) {
	/*
	 * Since there is ONLY one volume associated with each aggregate in UFS,
	 * the only valid volume index is 0.
	 */
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLINFO_BADIX,
		   ICL_TYPE_LONG, VOL_ERR_EOF);
	return VOL_ERR_EOF;
    }
    if (AFS_hiszero(UFS_AGTOVOLID(aggrp))) {
	/*
	 * No volume is associated with this aggregate; it's probably detached!
	 */
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLINFO_BADVOL,
		   ICL_TYPE_LONG, ENOENT);
	return ENOENT;
    }

    volp->v_paggrp = aggrp;
    volp->v_volOps = &vol_ufsops;

    /*
     * At this point we ought to fill in the static and dynamic status from
     * the volume descriptor.  But that isn't implemented yet.  So we just
     * fill in a random field that we happen to know the value for.
     */
    volp->v_volId = UFS_AGTOVOLID(aggrp);
    /* Set the initial flags and type. */
#ifdef AFS_OSF_ENV
    if ((UFS_AGTOVFSP(aggrp)->vfs_flag) & M_RDONLY)
#else /* AFS_OSF_ENV */
    if ((UFS_AGTOVFSP(aggrp)->vfs_flag) & VFS_RDONLY)
#endif /* AFS_OSF_ENV */
	{	/* Read-only UFS mount */
	/* Call it READONLY and not RW, but keep the VOL_TYPE_RW bit. */
	volp->v_voltype = VOL_READONLY;
	volp->v_states = VOL_TYPE_RW | VOL_IS_COMPLETE | VOL_REP_NONE
	  | VOL_LCLMOUNT | VOL_READONLY;
    } else {	/* Read-write UFS mount */
	volp->v_voltype = VOL_RW;
	volp->v_states = VOL_TYPE_RW | VOL_IS_COMPLETE | VOL_REP_NONE
	  | VOL_LCLMOUNT | VOL_RW;
    }
    (void)strncpy(volp->v_volName, aggrp->a_stat_st.aggrName,
		  sizeof volp->v_volName);
    volp->v_volName[sizeof volp->v_volName - 1] = '\0';
#if 0
#ifdef AFS_AIX31_ENV
    if ((slen = vmt2datasize(vmtp, VMT_OBJECT)) >= VOLNAMESIZE)
	slen = VOLNAMESIZE-1;
    strncpy(volp->v_volName, vmt2dataptr(vmtp, VMT_OBJECT), slen);
#endif /* AFS_AIX31_ENV */
#ifdef AFS_OSF_ENV
    strcpy(volp->v_volName, UFS_AGTOVFSP(aggrp)->m_stat.f_mntonname);
#endif /* AFS_OSF_ENV */
#ifdef	AFS_HPUX_ENV
    strcpy(volp->v_volName, UFS_AGTOVFSP(aggrp)->vfs_name);
#endif	/* AFS_HPUX_ENV */
#ifdef AFS_SUNOS5_ENV
    strcpy(volp->v_volName, (getfs(UFS_AGTOVFSP(aggrp)))->fs_fsmnt);
#endif /* AFS_SUNOS5_ENV */
#ifdef AFS_IRIX_ENV
    /*
     * XXX RAG vfs name is kept in FS specific data.
     */
    strcpy(volp->v_volName, (char *)(xfs_get_mount_fsname(UFS_AGTOVFSP(aggrp))));
    
#endif /* AFS_IRIX_ENV */
#ifdef	AFS_DEFAULT_ENV
#error	"ag_ufsVolInfo(): need a strcpy"
#endif	/* AFS_DEFAULT_ENV */
#endif /* 0 */
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_VOLINFO, ICL_TYPE_LONG, 0);
    return 0;
}


static int ag_ufsDetach(aggrp)
    register struct aggr *aggrp;
{
    icl_Trace0(xufs_iclSetp, UFS_TRACE_ENTER_AGDETACH);
    OSI_VN_RELE(((struct ufs_agData*)aggrp->a_fsDatap)->rootvp);
    osi_Free (aggrp->a_fsDatap, sizeof(struct ufs_agData));
    icl_Trace1(xufs_iclSetp, UFS_TRACE_END_AGDETACH, ICL_TYPE_LONG, 0);
    return 0;
}


static int ag_ufsAttach(dev, bdevvP, flags, ufstabparm, fsdatapP, fsdatalenP)
    dev_t dev;				/* Device number */
    struct vnode *bdevvP;		/* vnode for block device */
    int flags;				/* various bits */
    caddr_t ufstabparm;			/* FS-specific aggregate info (in user space) */
    opaque *fsdatapP;		/* place to put FS-specific data */
    int *fsdatalenP;			/* place to put length of fsdatapP */
{
    struct ufs_agData *datap;
    struct ufs_astab ufstab;
    dev_t vdev;
#ifdef AFS_OSF_ENV
    struct vnode  *newvp;
    struct nameidata *ndp = &u.u_nd;
    struct ufsmount *mntp;
#endif /* AFS_OSF_ENV */
#ifdef	AFS_HPUX_ENV
    struct mount *mntp;
#endif	/* AFS_HPUX_ENV */
#ifdef AFS_SUNOS5_ENV
    extern int ufsfstype;
#endif /* AFS_SUNOS5_ENV */
    struct vnode *vp;
    struct osi_vfs *vfsp;
    int code;
    struct icl_log *logp;

    /*
     * Create ICL log for tracing disk ops
     */
    if (xufs_iclSetp == NULL) {
	code = icl_CreateLog("disk", 60*1024, &logp);
	if (code == 0) {
	    code = icl_CreateSetWithFlags("xufs", logp, (struct icl_log *)0,
					  ICL_CRSET_FLAG_DEFAULT_OFF,
					  &xufs_iclSetp);
	    if (code) {
		printf("fx: Failed to create ICL set for xufs\n");
		xufs_iclSetp = NULL;
	    }
	}
    }

    icl_Trace0(xufs_iclSetp, UFS_TRACE_ENTER_AGATTACH);
    code = osi_copyin(ufstabparm, (char *) &ufstab, sizeof(struct ufs_astab));
    if (code != 0) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_AGATTACH_COPYIN,
		   ICL_TYPE_LONG, code);
	return (code);
    }
    icl_Trace1(xufs_iclSetp, UFS_TRACE_ENTER_AGATTACH_LOOKUP,
	       ICL_TYPE_STRING, ufstab.uas_mountedon);
    if (code = osi_lookupname(ufstab.uas_mountedon, OSI_UIOSYS,
			      FOLLOW_LINK, (struct vnode **) 0, &vp)) {
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_AGATTACH_LNAME,
		   ICL_TYPE_LONG, code);
	return (code);
    }
    vfsp = OSI_VP_TO_VFSP(vp);

    /*
     * Check that the filesystem in astab->as_spec is mounted, and that
     * vp is the root of it.
     */
#ifdef AFS_AIX_ENV
    vdev = ((struct gnode *)vfsp->vfs_data)->gn_rdev;
#else
#ifdef AFS_OSF_ENV
    mntp = VFSTOUFS(vfsp);
    vdev = mntp->um_dev;
#endif /* AFS_OSF_ENV */
#ifdef AFS_SUNOS5_ENV
    vdev = vfsp->vfs_dev;
    /* Check to ensure that this is a filesystem type that we support.
     * Currently, the only choices are ufs, which we assume is always loaded,
     * and hsfs, which may or my not be loaded. */
    if (vfsp->vfs_fstype != ufsfstype) {
	int hsfstype = -1;
	struct vfssw *vswp;

	RLOCK_VFSSW();
	if ((vswp = vfs_getvfsswbyname("hsfs")) != NULL)
	    hsfstype = vswp - vfssw;
	RUNLOCK_VFSSW();

	if (hsfstype == -1 || vfsp->vfs_fstype != hsfstype)
	    goto inval;
    }
#endif /* AFS_SUNOS5_ENV */
#ifdef AFS_IRIX_ENV
    vdev = vfsp->vfs_dev;
#endif /* AFS_IRIX_ENV */
#ifdef AFS_HPUX_ENV
    mntp = VFSTOM(vfsp);
    vdev = mntp->m_dev;
    if (vfsp->vfs_mtype != MOUNT_UFS) {
	icl_Trace2(xufs_iclSetp, UFS_TRACE_END_AGATTACH_MTYPE,
		   ICL_TYPE_LONG, MOUNT_UFS,
		   ICL_TYPE_LONG, code);
	goto inval;
    }
#endif	/* AFS_HPUX_ENV */
#endif /* AFS_AIX_ENV */
    if (dev != vdev) goto inval;

    if (!(vp->v_flag & VROOT)) {
inval:
	OSI_VN_RELE(vp);
	icl_Trace1(xufs_iclSetp, UFS_TRACE_END_AGATTACH_INVAL,
		   ICL_TYPE_LONG, EINVAL);
	return (EINVAL);
    }

    datap = (struct ufs_agData *) osi_Alloc(sizeof(struct ufs_agData));
    bzero((char *) datap, sizeof(*datap));
    datap->volId = ufstab.uas_volId;
    datap->vfsp = vfsp;
    /*
     * There used to be a VN_RELE(vp) here.  Instead of dropping the refcount
     * here, we stash away the root vp and do the VN_RELE at detach time.
     * This will prevent unmounts while the aggregate is still exported.
     */
    datap->rootvp = vp;
    *fsdatapP = datap;
    *fsdatalenP = sizeof(*datap);
    icl_Trace0(xufs_iclSetp, UFS_TRACE_END_AGATTACH);
    return 0;
}


static int ag_ufsSync(aggrp, syncType)
    struct aggr* aggrp;
    int syncType;
{
    icl_Trace0(xufs_iclSetp, UFS_TRACE_AGSYNC);
    return 0;
}

/* The aggregate ops vector itself */
struct aggrops ag_ufsops = {
    ag_fsHold,
    ag_fsRele,
    ag_fsLock,
    ag_fsUnlock,
    ag_ufsStat,
    ag_ufsVolCreate,
    ag_ufsVolInfo,
    ag_ufsDetach,
    ag_ufsAttach,
    ag_ufsSync,
    ag_fsDMHold,
    ag_fsDMRele,
};
