/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

#ident	"$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <ksys/ddmap.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/fs_subr.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/open.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/poll.h>
#include <sys/strsubr.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/runq.h>
#include <sys/ddi.h>
#include <sys/attributes.h>
#include <sys/major.h>

#include <fs/specfs/spec_atnode.h>


extern void spec_at_free(atnode_t *);



/* ARGSUSED */
STATIC int
spec_at_open(
	bhv_desc_t	*bdp,
	struct vnode	**vpp,
	mode_t		flag,
	struct cred	*credp)
{
	panic("spec_at_open: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_close(
	bhv_desc_t	*bdp,
	int		flag,
	lastclose_t	lastclose,
	struct cred	*credp)
{
	panic("spec_at_close: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_read(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl)
{
	panic("spec_at_read: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_write(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl)
{
	panic("spec_at_write: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_ioctl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		mode,
	struct cred	*credp,
	int		*rvalp,
	struct vopbd	*vbd)
{
	panic("spec_at_ioctl: called on attribute snode!");

	return ENODEV;
}


/*
 * The "real" reason this layer exists.
 */
/* ARGSUSED */
STATIC int
spec_at_getattr(
		 bhv_desc_t	*bdp,
	register struct vattr	*vap,
		 int		flags,
		 struct cred	*credp)
{
	int		error;
	atnode_t	*atp;
	vnode_t		*avp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_at_getattr);

	avp = BHV_TO_VNODE(bdp);

	atp = BHV_TO_ATP(bdp);
	
	SPEC_ATP_LOCK(atp);

	/*
	 * If this is a "replicate" case where we've recorded
	 * an "alternate" vp, go ask that file system for the
	 * attributes.
	 */
	if ((fsvp = atp->at_fsvp) != NULL) {

#ifdef	JTK_DEBUG
		printf(
	    "spec_at_getattr: Replicate dev(%d/%d, %d) vp/0x%x fsvp/0x%x\n",
							major(avp->v_rdev),
							emajor(avp->v_rdev),
							minor(avp->v_rdev),
							avp, fsvp);
#endif	/* JTK_DEBUG */

		VOP_GETATTR(fsvp, vap, flags, credp, error);

		SPEC_ATP_UNLOCK(atp);

		return error;
	}


#ifdef	JTK_DEBUG
	printf("spec_at_getattr: dev(%d/%d, %d) vp/0x%x fsvp/0x%x\n",
							major(avp->v_rdev),
							emajor(avp->v_rdev),
							minor(avp->v_rdev),
							avp, fsvp);
#endif	/* JTK_DEBUG */

	/*
	 * There isn't an "alternate", do the work ourselves.
	 */

	vap->va_size = BBTOB(atp->at_size);

	if (vap->va_mask == AT_SIZE) {

		SPEC_ATP_UNLOCK(atp);

		return 0;
	}

	vap->va_fsid   = atp->at_fsid;
	vap->va_nodeid = (long)atp & 0xFFFF;	/* XXXjtk -- must fix?	*/
	vap->va_nlink  = 1;

	/*
	 * Quick exit for non-stat callers
	 */
	if ((vap->va_mask & ~(AT_SIZE|AT_FSID|AT_NODEID|AT_NLINK)) == 0) {

		SPEC_ATP_UNLOCK(atp);

		return 0;
	}

	/*
	 * Copy from in-core snode.
	 */
	vap->va_type = avp->v_type;
	vap->va_mode = atp->at_mode;

	vap->va_uid    = atp->at_uid;
	vap->va_gid    = atp->at_gid;
	vap->va_projid = atp->at_projid;

	vap->va_rdev = atp->at_dev;

	vap->va_atime.tv_sec  = atp->at_atime;
	vap->va_atime.tv_nsec = 0;

	vap->va_mtime.tv_sec  = atp->at_mtime;
	vap->va_mtime.tv_nsec = 0;

	vap->va_ctime.tv_sec  = atp->at_ctime;
	vap->va_ctime.tv_nsec = 0;

	vap->va_blksize = MAXBSIZE;
	vap->va_nblocks = btod(vap->va_size);

	/*
	 * Exit for stat callers.
	 * See if any of the rest of the fields to be filled in are needed.
	 */
	if ((vap->va_mask &
	     (AT_XFLAGS|AT_EXTSIZE|AT_NEXTENTS|AT_ANEXTENTS|
	      AT_GENCOUNT|AT_VCODE)) == 0) {

		SPEC_ATP_UNLOCK(atp);

		return 0;
	}

	vap->va_xflags    = 0;
	vap->va_extsize   = 0;
	vap->va_nextents  = 0;
	vap->va_anextents = 0;
	vap->va_gencount  = 0;
	vap->va_vcode     = 0L;


	SPEC_ATP_UNLOCK(atp);

	return 0;
}


/* ARGSUSED */
STATIC int
spec_at_setattr(
		 bhv_desc_t	*bdp,
	register struct vattr	*vap,
		 int		flags,
		 struct cred	*credp)
{
	int		error;
	int		file_owner;
	int		mask;
	int		timeflags = 0;
	u_int16_t	projid, iprojid;
	atnode_t	*atp;
	gid_t		gid, igid;
	timespec_t	tv;
	uid_t		uid, iuid;
	vnode_t		*avp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_at_setattr);

	avp = BHV_TO_VNODE(bdp);

	atp = BHV_TO_ATP(bdp);
	
	SPEC_ATP_LOCK(atp);

	/*
	 * If this is a "replicate" case where we've recorded
	 * an "alternate" vp, go tell that file system about the
	 * attributes.
	 */
	if ((fsvp = atp->at_fsvp) != NULL) {

#ifdef	JTK_DEBUG
		printf(
	    "spec_at_setattr: Replicate dev(%d/%d, %d) vp/0x%x fsvp/0x%x\n",
							major(avp->v_rdev),
							emajor(avp->v_rdev),
							minor(avp->v_rdev),
							avp, fsvp);
#endif	/* JTK_DEBUG */

		VOP_SETATTR(fsvp, vap, flags, credp, error);

		SPEC_ATP_UNLOCK(atp);

		return error;
	}

	/*
	 * There isn't an "alternate", do the work ourselves.
	 */


#ifdef	JTK_DEBUG
	printf("spec_at_setattr: dev(%d/%d, %d) vp/0x%x fsvp/0x%x\n",
							major(avp->v_rdev),
							emajor(avp->v_rdev),
							minor(avp->v_rdev),
							avp, fsvp);
#endif	/* JTK_DEBUG */

	/*
	 * Cannot set certain attributes.
         */
        mask = vap->va_mask;
        if (mask & AT_NOSET) {

		SPEC_ATP_UNLOCK(atp);

		return EINVAL;
	}

        /*
         * Update requested timestamps.
         */
        if (mask & AT_UPDTIMES) {

                ASSERT((mask & ~AT_UPDTIMES) == 0);

		timeflags = ((mask & AT_UPDATIME) ? SPECFS_ICHGTIME_ACC : 0)
			  | ((mask & AT_UPDCTIME) ? SPECFS_ICHGTIME_CHG : 0)
			  | ((mask & AT_UPDMTIME) ? SPECFS_ICHGTIME_MOD : 0);

		nanotime_syscall(&tv);

		if (timeflags & SPECFS_ICHGTIME_MOD)
			atp->at_mtime = tv.tv_sec;

		if (timeflags & SPECFS_ICHGTIME_ACC)
			atp->at_atime = tv.tv_sec;

		if (timeflags & SPECFS_ICHGTIME_CHG)
			atp->at_ctime = tv.tv_sec;

		SPEC_ATP_UNLOCK(atp);

		return 0;
        }

	/*
	 * boolean: are we the file owner?
	 */
	file_owner = (credp->cr_uid == atp->at_uid);

	/*
	 * Change various properties of a file.
	 * Only the owner or users with CAP_FOWNER capability
	 * may do these things.
	 */
        if (mask & (AT_MODE|AT_XFLAGS|AT_EXTSIZE|AT_UID|AT_GID|AT_PROJID)) {

		/*
		 * CAP_FOWNER overrides the following restrictions:
		 *
		 * The user ID of the calling process must be equal
		 * to the file owner ID, except in cases where the
		 * CAP_FSETID capability is applicable.
		 */
                if (!file_owner && !cap_able_cred(credp, CAP_FOWNER)) {

			SPEC_ATP_UNLOCK(atp);

			return EPERM;
                }

		/*
		 * CAP_FSETID overrides the following restrictions:
		 *
		 * The effective user ID of the calling process shall match
		 * the file owner when setting the set-user-ID and
		 * set-group-ID bits on that file.
		 *
		 * The effective group ID or one of the supplementary group
		 * IDs of the calling process shall match the group owner of
		 * the file when setting the set-group-ID bit on that file
		 */
		if (mask & AT_MODE) {
			mode_t m = 0;

			if ((vap->va_mode & ISUID) && !file_owner)
				m |= ISUID;

			if (  (vap->va_mode & ISGID)
			    && !groupmember(atp->at_gid, credp))
				m |= ISGID;

			if ((vap->va_mode & ISVTX) && avp->v_type != VDIR)
				m |= ISVTX;

			if (m && !cap_able_cred(credp, CAP_FSETID))
				vap->va_mode &= ~m;
		}
        }

        /*
         * Change file ownership.  Must be the owner or privileged.
         * If the system was configured with the "restricted_chown"
         * option, the owner is not permitted to give away the file,
         * and can change the group id only to a group of which he
         * or she is a member.
         */
        if (mask & (AT_UID|AT_GID|AT_PROJID)) {

                /*
		 * These IDs could have changed since we last looked at them.
		 * But, we're assured that if the ownership did change
		 * while we didn't have the inode locked, inode's dquot(s) 
		 * would have changed also.
		 */
		iuid    = atp->at_uid;
		igid    = atp->at_gid; 
		iprojid = atp->at_projid;

                gid    = (mask & AT_GID) ? vap->va_gid : igid;
		uid    = (mask & AT_UID) ? vap->va_uid : iuid;
		projid = (mask & AT_PROJID)
				? (u_int16_t)vap->va_projid
				: iprojid;

		/*
		 * CAP_CHOWN overrides the following restrictions:
		 *
		 * If _POSIX_CHOWN_RESTRICTED is defined, this capability
		 * shall override the restriction that a process cannot
		 * change the user ID of a file it owns and the restriction
		 * that the group ID supplied to the chown() function
		 * shall be equal to either the group ID or one of the
		 * supplementary group IDs of the calling process.
		 *
		 * XXX: How does restricted_chown affect projid?
		 */
		if (   restricted_chown
		    && (       iuid != uid
			|| (   igid != gid
			    && !groupmember(gid, credp) ) )
		    && !cap_able_cred(credp, CAP_CHOWN)     ) {

			SPEC_ATP_UNLOCK(atp);

			return EPERM;
		}

        }

        /*
         * Truncate file.
	 * Nothing to do here for "special" files.
         */
        if (mask & AT_SIZE) {

		SPEC_ATP_UNLOCK(atp);

		return 0;
        }

        /*
         * Change file access or modified times.
         */
        if (mask & (AT_ATIME|AT_MTIME)) {

		if (!file_owner) {

			if (   (flags & ATTR_UTIME)
			    && !cap_able_cred(credp, CAP_FOWNER)) {

				SPEC_ATP_UNLOCK(atp);

				return EPERM;
			}
		}
        }

	/*
	 * Change extent size or realtime flag.
	 */
	if (mask & (AT_EXTSIZE|AT_XFLAGS)) {

		SPEC_ATP_UNLOCK(atp);

		return EINVAL;
	}

        /*
         * Change file access modes.
         */
        if (mask & AT_MODE) {

                atp->at_mode &= IFMT;

                atp->at_mode |= vap->va_mode & ~IFMT;

		timeflags |= SPECFS_ICHGTIME_CHG;
        }

	/*
	 * Change file ownership.  Must be the owner or privileged.
         * If the system was configured with the "restricted_chown"
         * option, the owner is not permitted to give away the file,
         * and can change the group id only to a group of which he
         * or she is a member.
         */
        if (mask & (AT_UID|AT_GID|AT_PROJID)) {

		/*
		 * CAP_FSETID overrides the following restrictions:
		 *
		 * The set-user-ID and set-group-ID bits of a file will be
		 * cleared upon successful return from chown()
		 */
                if (  (atp->at_mode & (ISUID|ISGID))
		    && !cap_able_cred(credp, CAP_FSETID)) {

                        atp->at_mode &= ~(ISUID|ISGID);
                }
                
		atp->at_uid    = uid;
		atp->at_gid    = gid;
		atp->at_projid = projid;

		timeflags |= SPECFS_ICHGTIME_CHG;
        }


	/*
         * Change file access or modified times.
         */
        if (mask & (AT_ATIME|AT_MTIME)) {

                if (mask & AT_ATIME) {

                        atp->at_atime = vap->va_atime.tv_sec;

			timeflags &= ~SPECFS_ICHGTIME_ACC;
		}

                if (mask & AT_MTIME) {

			atp->at_mtime = vap->va_mtime.tv_sec;

			timeflags &= ~SPECFS_ICHGTIME_MOD;
			timeflags |= SPECFS_ICHGTIME_CHG;
                }
        }

	/*
	 * Send out timestamp changes that need to be set to the
	 * current time.  Not done when called by a DMI function.
	 */
	if (timeflags && !(flags & ATTR_DMI)) {

		nanotime_syscall(&tv);

		if (timeflags & SPECFS_ICHGTIME_MOD)
			atp->at_mtime = tv.tv_sec;

		if (timeflags & SPECFS_ICHGTIME_ACC)
			atp->at_atime = tv.tv_sec;

		if (timeflags & SPECFS_ICHGTIME_CHG)
			atp->at_ctime = tv.tv_sec;
	}


	SPEC_ATP_UNLOCK(atp);

	return 0;
}


/* ARGSUSED */
STATIC int
spec_at_access(
	bhv_desc_t	*bdp,
	int		mode,
	struct cred	*credp)
{
	panic("spec_at_access: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_readlink(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	struct cred	*credp)
{
	panic("spec_at_readlink: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_fsync(
	bhv_desc_t	*bdp,
	int		flag,
	struct cred	*credp,
	off_t		start,
	off_t		stop)
{
	panic("spec_at_fsync: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_fid(
	bhv_desc_t	*bdp,
	struct fid	**fidpp)
{
	panic("spec_at_fid: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_fid2(
	bhv_desc_t	*bdp,
	struct fid	*fidp)
{
	panic("spec_at_fid2: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC void
spec_at_rwlock(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	panic("spec_at_rwlock: called on attribute snode!");
}


/* ARGSUSED */
STATIC void
spec_at_rwunlock(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	panic("spec_at_unrwlock: called on attribute snode!");
}


/* ARGSUSED */
STATIC int
spec_at_seek(
	bhv_desc_t	*bdp,
	off_t		ooff,
	off_t		*noffp)
{
	panic("spec_at_seek: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_frlock(
	bhv_desc_t	*bdp,
	int		cmd,
	struct flock	*bfp,
	int		flag,
	off_t		offset,
	vrwlock_t	vrwlock,
	struct cred	*credp)
{
	panic("spec_at_frlock: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_realvp(
	register bhv_desc_t	*bdp,
	register struct vnode	**vpp)
{
	panic("spec_at_realvp: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_poll(
	bhv_desc_t	*bdp,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead	**phpp,
	unsigned int	*genp)
{
	panic("spec_at_poll: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
int
spec_at_bmap(
	bhv_desc_t	*bdp,
	off_t		offset,
	ssize_t		count,
	int		rw,
	struct cred	*credp,
	struct bmapval	*bmap,
	int		*nbmap)
{
	panic("spec_at_bmap: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC void
spec_at_strategy(
	bhv_desc_t	*bdp,
	struct buf	*bp)
{
	panic("spec_at_strategy: called on attribute snode!");
}


/* ARGSUSED */
STATIC int
spec_at_map(
	bhv_desc_t	*bdp,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	u_int		flags,
	struct cred	*credp,
	vnode_t		**nvp)
{
	panic("spec_at_map: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_addmap(
	bhv_desc_t	*bdp,
	vaddmap_t	op,
	vhandl_t	*vt,
	pgno_t		*pgno,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	struct cred	*credp)
{
	panic("spec_at_addmap: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_delmap(
	bhv_desc_t	*bdp,
	vhandl_t	*vt,
	size_t		len,
	struct cred	*credp)
{
	panic("spec_at_delmap: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_attr_get(
	bhv_desc_t	*bdp,
	char		*name,
	char		*value,
	int		*valuelenp,
	int		flags,
	struct cred	*credp)
{
	panic("spec_at_attr_get: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_attr_set(
	bhv_desc_t	*bdp,
	char		*name,
	char		*value,
	int		valuelen,
	int		flags,
	struct cred	*credp)
{
	panic("spec_at_attr_set: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_attr_remove(
	bhv_desc_t	*bdp,
	char		*name,
	int		flags,
	struct cred	*credp)
{
	panic("spec_at_attr_remove: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_attr_list(
	bhv_desc_t		*bdp,
	char			*buffer,
	int			bufsize,
	int			flags,
	attrlist_cursor_kern_t	*cursor,
	struct cred		*credp)
{
	panic("spec_at_attr_list: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
int
spec_at_strgetmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	panic("spec_at_strgetmsg: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
int
spec_at_strputmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	panic("spec_at_strputmsg: called on attribute snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_at_inactive(
	bhv_desc_t	*bdp,
	struct cred	*credp)
{
	atnode_t	*atp;
	/* REFERENCED */
	vnode_t		*vp;


	SPEC_STATS(spec_at_inactive);

	vp  = BHV_TO_VNODE(bdp);
	atp = BHV_TO_ATP(bdp);

	vn_trace_entry(vp, "spec_at_inactive", (inst_t *)__return_address);

	if (atp->at_fsvp) {

		vn_trace_entry(atp->at_fsvp, "spec__at_inactive",
						(inst_t *)__return_address);
		VN_RELE(atp->at_fsvp);

		atp->at_fsvp = NULL;
	}

	/*
	 * Pull ourselves out of the behavior chain for this vnode.
	 */
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);

	spec_at_free(atp);

	/*
	 * The inactive_teardown flag must have been set at vn_alloc time
	 */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	return VN_INACTIVE_NOCACHE;
}


vnodeops_t spec_at_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_SPECFS_AT),
	spec_at_open,					/* Panic	*/
	spec_at_close,					/* Panic	*/
	spec_at_read,					/* Panic	*/
	spec_at_write,					/* Panic	*/
	spec_at_ioctl,					/* Panic	*/
	fs_setfl,
	spec_at_getattr,			/* Usable		*/
	spec_at_setattr,			/* Usable		*/
	spec_at_access,				/* Usable		*/
	(vop_lookup_t)fs_nosys,
	(vop_create_t)fs_nosys,
	(vop_remove_t)fs_nosys,
	(vop_link_t)fs_nosys,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	(vop_readdir_t)fs_nosys,
	(vop_symlink_t)fs_nosys,
	spec_at_readlink,				/* Panic	*/
	spec_at_fsync,					/* Panic	*/
	spec_at_inactive,			/* Usable		*/
	spec_at_fid,					/* Panic	*/
	spec_at_fid2,					/* Panic	*/
	spec_at_rwlock,					/* Panic	*/
	spec_at_rwunlock,				/* Panic	*/
	spec_at_seek,					/* Panic	*/
	fs_cmp,
	spec_at_frlock,					/* Panic	*/
	spec_at_realvp,					/* Panic	*/
	spec_at_bmap,					/* Panic	*/
	spec_at_strategy,				/* Panic	*/
	spec_at_map,					/* Panic	*/
	spec_at_addmap,					/* Panic	*/
	spec_at_delmap,					/* Panic	*/
	spec_at_poll,					/* Panic	*/
	(vop_dump_t)fs_nosys,
	fs_pathconf,
	(vop_allocstore_t)fs_noerr,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	spec_at_attr_get,				/* Panic	*/
	spec_at_attr_set,				/* Panic	*/
	spec_at_attr_remove,				/* Panic	*/
	spec_at_attr_list,				/* Panic	*/
	(vop_cover_t)fs_nosys,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	fs_tosspages,
	fs_flushinval_pages,
	fs_flush_pages,
	fs_invalfree_pages,
	fs_pages_sethole,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	spec_at_strgetmsg,				/* Panic	*/
	spec_at_strputmsg,				/* Panic	*/
};
