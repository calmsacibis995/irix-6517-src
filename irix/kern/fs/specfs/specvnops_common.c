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

#ident	"$Revision: 1.44 $"

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
#include <fs/specfs/spec_csnode.h>


STATIC void spec_cs_rwlock(bhv_desc_t *, vrwlock_t);
STATIC void spec_cs_rwunlock(bhv_desc_t *, vrwlock_t);

STATIC void spec_cs_rwlock_lcl(bhv_desc_t *, vrwlock_t);
STATIC void spec_cs_rwunlock_lcl(bhv_desc_t *, vrwlock_t);

int spec_cs_bmap_vop(bhv_desc_t *, off_t, ssize_t, int, struct cred *,
			struct bmapval *, int *);
  
#define PCASTTYPE (int (*)(void *, short, int, short *,		\
					void **, unsigned int *))

extern mutex_t spec_csnode_tablock;		/* spec_csnode_table lock */

/* 
 * Macros to lock and to unlock csnode hash table, used during modifications
 * and during traversal of the hash list with possibility of sleep
 */
#define SPEC_CSTAB_LOCK()	mutex_lock(&spec_csnode_tablock, PINOD)
#define SPEC_CSTAB_UNLOCK()	mutex_unlock(&spec_csnode_tablock)
#define SPEC_CSTAB_ISLOCKED()	mutex_mine(&spec_csnode_tablock)


/*
 * VN_KILL poll stub
 */
/* ARGSUSED */
STATIC int
spec_cs_kill_poll(
	void		*a0,
	short		events,
	int		anyyet,
	short		*reventsp,
	void		**vpp,
	unsigned int	*genp)
{
	SPEC_STATS(spec_cs_kill_poll);

	return EIO;
}


/*
 * Return default answer to poll() for those without poll handlers.
 */
/* ARGSUSED */
STATIC int
spec_cs_dflt_poll(
	void		*a0,
	short		events,
	int		anyyet,
	short		*reventsp,
	void		**vpp,
	unsigned int	*genp)
{
	SPEC_STATS(spec_cs_dflt_poll);

	*reventsp = events & (POLLIN|POLLRDNORM|POLLOUT);

	return 0;
}


/*
 * Allow handlers of special files to initialize and validate before
 * actual I/O.
 *
 * called through the spec_ls2cs_ops vector.
 */
/* ARGSUSED */
int
spec_cs_open_hndl(
	spec_handle_t	*handle,
	int		gen,
	dev_t		*newdev,
	daddr_t		*cs_size,
	struct stdata	**stp,
	mode_t		flag,
	struct cred	*credp)
{
	int		(*size)(dev_t);
	int		(*size64)(dev_t, daddr_t *);

#ifdef	MP
	int		dflags;
#endif	/* MP */
	int		error;
	int		held;
	csnode_t	*csp;
	daddr_t		sz;
	dev_t		dev;
	enum vtype	type;
	/* REFERENCED */
	vnode_t		*vp;
	vnode_t		*vpp;
	struct vnode	vpp_vnode;


	SPEC_STATS(spec_cs_open_hndl);

	error = 0;
	held  = 0;

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp != NULL);			/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	vp = CSP_TO_VP(csp);

	/*
	 * If the containing filesystem was mounted "nodev", deny access.
	 */
	if (vp->v_vfsp && (vp->v_vfsp->vfs_flag & VFS_NODEV))
		return EACCES;

	SPEC_CSP_LOCK(csp);

	*stp = NULL;

	if (gen != csp->cs_gen)	{		/* was it "vn_kill'd" ?   */
#ifdef	JTK_DEBUG
		printf("spec_cs_open_hndl: EIO returned, gen/%d cs_gen/%d\n",
							gen, csp->cs_gen);
#endif	/* JTK_DEBUG */

		SPEC_CSP_UNLOCK(csp);

		return EIO;		/* lost race with vn_kill/spec_get */
	}

	flag &= ~FCREAT;		/* paranoia */

	/*
	 * If this csnode is in the process of being closed
	 * by another thread, we need to wait until that's
	 * done before we let somebody else have it.
	 */
	while (csp->cs_close_flag & SCLOSING) {

		csp->cs_close_flag |= SWAITING;

		sv_wait(&csp->cs_close_sync, PZERO,
					&csp->cs_lock, 0);
		SPEC_CSP_LOCK(csp);
	}

	csp->cs_opencnt++;		/* one more open reference */

	dev  = vp->v_rdev;
	type = vp->v_type;

	*newdev = dev;

	switch (type) {

	case VCHR:
		if (csp->cs_cdevsw == NULL) {
			error = ENXIO;
			break;
		}

		if (csp->cs_flag & SSTREAM) {

			SPEC_CSP_UNLOCK(csp);

			/*
			 * We "fake" a vpp on our own stack for stropen()
			 * to twiddle with, otherwise we could have several
			 * simultaneous streams cloning operations going on
			 * all at the same time trying to use the "lead-in"
			 * vnodes to hold "temporary" v_stream values.
			 *
			 * Some thought:	XXXjtk
			 * Since specfs is the only caller of stropen
			 * that uses the "vpp" argument, let's change
			 * it to simply the stream head pointer.
			 *
			 * Perhaps after we "get rid" of the older
			 * modules we can do that.
			 */
			vpp = &vpp_vnode;

			error = stropen(vp, newdev, &vpp, flag, credp);

			SPEC_CSP_LOCK(csp);

			if (error)
				break;

			if (vp->v_rdev != *newdev) {

				/*
				 * The world has "cloned" on us, drop our
				 * "hold" on the old common snode and use
				 * "spec_cs_get" to get us the "new" common
				 * snode, locked.
				 */
				csp->cs_opencnt--;

				SPEC_CSP_UNLOCK(csp);

				csp = spec_cs_get(*newdev, type);

#ifdef	JTK_STR_DEBUG
				printf(
			"spec_cs_open_hndl: d_str CLONING vp/0x%x stp/0x%x dev(%d/%d, %d) ",
							vp, vpp->v_stream,
							major(vp->v_rdev),
							emajor(vp->v_rdev),
							minor(vp->v_rdev));
				printf("ndev(%d/%d, %d)!\n",
							major(*newdev),
							emajor(*newdev),
							minor(*newdev));
#endif	/* JTK_STR_DEBUG */
			}

			*stp = vpp->v_stream;

			csp->cs_poll   = PCASTTYPE strpoll;
			csp->cs_polla0 = (void *)(vpp->v_stream);

		} else {
			/* 
			 * If it's a loadable module, hold a refcnt while the 
			 * device is open.
			 * For streams, this is done in qattach.
			 */
			if (error = spec_cs_device_hold(csp))
				break;

			held = 1;

			SPEC_CSP_UNLOCK(csp);

			error = cdopen(csp->cs_cdevsw, newdev,
							flag, OTYP_CHR, credp);

			SPEC_CSP_LOCK(csp);

			if (error)
				break;

			if (vp->v_rdev != *newdev) {

				/*
				 * The world has "cloned" on us, drop our
				 * "hold" on the old common snode and use
				 * "spec_cs_get" to get us the "new" common
				 * snode, locked.
				 */
				csp->cs_opencnt--;

				SPEC_CSP_UNLOCK(csp);

				csp = spec_cs_get(*newdev, type);

#ifdef	JTK_STR_DEBUG
				printf("spec_cs_open_hndl: VCHR CLONING dev(%d/%d, %d) ",
							major(vp->v_rdev),
							emajor(vp->v_rdev),
							minor(vp->v_rdev));
				printf("ndev(%d/%d, %d)!\n",
							major(*newdev),
							emajor(*newdev),
							minor(*newdev));
#endif	/* JTK_STR_DEBUG */
			}

#ifdef	MP
			dflags = *csp->cs_cdevsw->d_flags;

			if (csp->cs_cdevsw->d_poll != NULL) {
				csp->cs_poll = PCASTTYPE csp->cs_cdevsw->d_poll;

				if ((dflags & DLOCK_MASK) != D_MP)
				    csp->cs_poll = NULL;
			} else {
				csp->cs_poll = spec_cs_dflt_poll;
			}
#else	/* ! MP */
			if (csp->cs_cdevsw->d_poll != NULL)
				csp->cs_poll = PCASTTYPE csp->cs_cdevsw->d_poll;
			else 
				csp->cs_poll = spec_cs_dflt_poll;
#endif	/* ! MP */
			csp->cs_polla0 = (void *)(__psunsigned_t)*newdev;
		}

		break;

	case VBLK:
		if (csp->cs_bdevsw == NULL) {
			error = ENXIO;
			break;
		}

		/* 
		 * Hold a refcnt while the device is open. 
		 */
		if (error = spec_cs_device_hold(csp))
			break;

		held = 1;

		SPEC_CSP_UNLOCK(csp);

		error = bdopen(csp->cs_bdevsw, newdev, flag, OTYP_BLK, credp);

		SPEC_CSP_LOCK(csp);

		if (error)
			break;

		if (vp->v_rdev != *newdev) {

			/*
			 * The world has "cloned" on us, drop our
			 * "hold" on the old common snode and use
			 * "spec_cs_get" to get us the "new" common
			 * snode, locked.
			 */
			csp->cs_opencnt--;

			SPEC_CSP_UNLOCK(csp);

			csp = spec_cs_get(*newdev, type);

#ifdef	JTK_STR_DEBUG
			printf("spec_cs_open_hndl: VBLK CLONING dev(%d/%d, %d) ",
						major(vp->v_rdev),
						emajor(vp->v_rdev),
						minor(vp->v_rdev));
			printf("ndev(%d/%d, %d)!\n",
						major(*newdev),
						emajor(*newdev),
						minor(*newdev));
#endif	/* JTK_STR_DEBUG */
		}

		size   = csp->cs_bdevsw->d_size;
		size64 = csp->cs_bdevsw->d_size64;

		/*
		 * A bad lboot, or mis-configuration could leave "size" &/or
		 * "size64" as NULL instead of "nulldev", which would cause
		 * us to jump into the weeds.
		 *
		 * Catch and prevent that situation now.
		 * Odds are, if the bdevsw isn't right, we're not going to get
		 * booted properly anyway.
		 */
		if (size == NULL || size64 == NULL) {
			panic(
	"spec_cs_open: bdevsw->d_size==NULL for <%v>, check config & lboot\n",
								dev);
		}

		if ((int (*)(void))size64 != nulldev)
			(*size64)(dev, &sz);
		else if ((int (*)(void))size != nulldev)
			sz = (*size)(dev);
		else
			sz = -1;

		*cs_size = csp->cs_size = sz;

		csp->cs_poll   = spec_cs_dflt_poll;
		csp->cs_polla0 = (void *)(__psunsigned_t)*newdev;

		break;

	default:
		cmn_err_tag(87,CE_PANIC, "spec_cs_open_hndl: type not VCHR or VBLK\n");
		break;
	}


	if (error != 0) {
		csp->cs_opencnt--;		/* one fewer open reference */

		/* If the device was opened simultaneously with this open
		 * & already closed, and this open fails, then the driver's
		 * close function may need to be called here.
		 * The reference count is going to zero, but spec_cs_close
		 * will not be called on the second open since it failed.
		 */
		if (  (csp->cs_flag & SWANTCLOSE)
		    && csp->cs_opencnt == 0
		    && csp->cs_mapcnt  == 0) {

			csp->cs_flag &= ~SWANTCLOSE;

			(void) spec_cs_device_close(csp, flag, credp);
		}

		if (held)
			spec_cs_device_rele(csp);

		SPEC_CSP_UNLOCK(csp);

		return error;
	} else {
		/*
		 * Normal open.
		 */
		csp->cs_flag |= SWANTCLOSE;
	}


	SPEC_CSP_UNLOCK(csp);
	
	return error;
}


/*
 * "spec_cs_open_vop" is the routine in the specfs common vnodeops
 * vector used to service anyone directly using the common vnode.
 * This usually turns out to just be the direct console devices
 * serviced by "getty".
 */
/* ARGSUSED */
STATIC int
spec_cs_open_vop(
	bhv_desc_t	*bdp,
	struct vnode	**vpp,
	mode_t		flag,
	struct cred	*credp)
{
	int		error;
	csnode_t	*csp;
	daddr_t		cs_size;
	dev_t		newdev;
#ifdef	CELL_IRIX
	service_t	svc;
#endif	/* CELL_IRIX */
	spec_handle_t	handle;
	/* REFERENCED */
	vnode_t		*cvp;
	struct stdata	*stp;


	SPEC_STATS(spec_cs_open_vop);

	cvp = BHV_TO_VNODE(bdp);
	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

#ifdef	CELL_IRIX
	SERVICE_MAKE(svc, cellid(), SVC_SPECFS);

	SPEC_HANDLE_MAKE(handle, svc, csp, cvp->v_rdev, cvp->v_type);
#else	/* ! CELL_IRIX */
	handle.objid = (void *)csp;
#endif	/* ! CELL_IRIX */

	error = spec_cs_open_hndl(&handle, csp->cs_gen, &newdev, &cs_size,
							&stp, flag, credp);
	return error;
}


/*
 * The "common" side close.
 *
 * called through the spec_ls2cs_ops vector.
 */
/* ARGSUSED */
int
spec_cs_close_hndl(
	spec_handle_t	*handle,
	int		flag,
	lastclose_t	lastclose,
	struct cred	*credp)
{
	int		error;
	csnode_t	*csp;
	enum vtype	type;
	vnode_t		*vp;


	SPEC_STATS(spec_cs_close_hndl);

	csp = HANDLE_TO_CSP(handle);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	vp  = CSP_TO_VP(csp);

	error = 0;

	/*
	 * Handle differently if we're called from vn_kill
	 *	- with a FROM_VN_KILL flag.
	 *
	 * In this case we don't really close anything
	 * 	- just make unusable as well as inacccessible from spec_get.
	 *
	 * A comment about previous code that attempted to kill the vnode
	 * by changing its vnodeops to dead_vnodeops:
	 *
	 * There is a race where one process is doing a lookup on a device
	 * (e.g from a stat()), and the efs_lookup code calls specvp.
	 *
	 * In the meantime the session leader is shutting everything down
	 * So spec_vp can return (via spec_get) a valid vnode that has
	 * an out of date gen number by the time the upper layer code gets
	 * around to calling thru the VOP layer.
	 *
	 * Clearly we need to permit a stat to always succeed,
	 * 	- thus changing out the vnodeops doesn't really work.
	 */
	if (lastclose == FROM_VN_KILL) {

		ASSERT(vp->v_count > 0);

		SPEC_CSP_LOCK(csp);

		/*
		 * When we obsolete the csnode, change the poll function
		 * pointer to a stub that returns EIO.
		 *
		 * This saves the overhead of comparing the csnode generation
		 * numbers in the spec_cs_poll() routine on every call.
		 */
		csp->cs_poll = spec_cs_kill_poll;

#ifdef	JTK_DEBUG
		printf(
"spec_cs_close: CS_GEN++ vp 0x%x dev(%d/%d, %d) ogen/%d ngen/%d\n",
					vp,
					major(vp->v_rdev),
					emajor(vp->v_rdev), minor(vp->v_rdev),
					csp->cs_gen, csp->cs_gen + 1);
#endif	/* JTK_DEBUG */

		csp->cs_gen++;

		SPEC_CSP_UNLOCK(csp);

		return 0;
	}

	if (vp->v_stream)
		strclean(vp->v_stream);

	if (lastclose == L_FALSE)
		return 0;

	/*
	 * Drop references added by open.
	 */
	SPEC_CSP_LOCK(csp);

	ASSERT(csp->cs_opencnt > 0);

	csp->cs_opencnt--;

	type = vp->v_type;

	ASSERT(type == VCHR || type == VBLK);

	/*
	 * Call the close routine only when the last open reference
	 * through any [s,v]node goes away.
	 */
	if (csp->cs_opencnt == 0 && csp->cs_mapcnt == 0) {

		ASSERT(csp->cs_flag & SWANTCLOSE);

		csp->cs_flag &= ~SWANTCLOSE;

		error = spec_cs_device_close(csp, flag, credp);
	}

	/* 
	 * If it's a loadable module, decrement the refcnt,
	 * unless its a streams driver, then its handled in qdetach.
	 */
	if (   (type == VCHR && ((csp->cs_flag & SSTREAM) == 0))
	    || (type == VBLK) ) {

		spec_cs_device_rele(csp);
	}

	SPEC_CSP_UNLOCK(csp);

	return error;
}


/*
 * "spec_cs_close_vop" is the routine in the specfs common vnodeops
 * vector used to service anyone directly using the common vnode.
 * This usually turns out to just be the direct console devices
 * serviced by "getty".
 */
/* ARGSUSED */
STATIC int
spec_cs_close_vop(
	bhv_desc_t	*bdp,
	int		flag,
	lastclose_t	lastclose,
	struct cred	*credp)
{
	int		error;
	csnode_t	*csp;
#ifdef	CELL_IRIX
	service_t	svc;
#endif	/* CELL_IRIX */
	spec_handle_t	handle;
	/* REFERENCED */
	vnode_t		*cvp;


	SPEC_STATS(spec_cs_close_vop);

	cvp = BHV_TO_VNODE(bdp);
	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

#ifdef	CELL_IRIX
	SERVICE_MAKE(svc, cellid(), SVC_SPECFS);

	SPEC_HANDLE_MAKE(handle, svc, csp, cvp->v_rdev, cvp->v_type);
#else	/* ! CELL_IRIX */
	handle.objid = (void *)csp;
#endif	/* ! CELL_IRIX */

	error = spec_cs_close_hndl(&handle, flag, lastclose, credp);

	return error;
}


/*
 * Do the right flushing for the read case when the IO_RSYNC flag is set.
 */
/* ARGSUSED */
STATIC void
spec_cs_read_rsync(
	bhv_desc_t	*bdp,
	csnode_t	*csp,
	int		ioflag,
	dev_t		dev,	 
	cred_t		*credp)		
{
	SPEC_STATS(spec_cs_read_rsync);

	if (ioflag & (IO_SYNC | IO_DSYNC))
		bflush(dev);

	if (!(ioflag & IO_SYNC))
		return;

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
	ASSERT(bdp->bd_ops == &spec_cs_vnodeops); /* what we think they are */

	/*
	 * If times didn't change, don't flush anything.
	 */
	if (   ((csp->cs_flag & (SACC|SUPD|SCHG)) != 0)
	    || (CSP_TO_VP(csp)->v_type == VBLK)) {
		
		csp->cs_flag &= ~(SACC|SUPD|SCHG);
	}
}

	   
/*
 * The "common" side read called via the VOP_ vector.
 *
 * This is usually just the console interface, which, for the moment,
 * only likes to operate against the common vnode...
 */
/*
 * VOP_RWLOCK has been done (if IO_ISLOCKED flag is set)
 */
/* ARGSUSED */
STATIC int
spec_cs_read_vop(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl)
{
		 int		error;
		 int		nbmv;
		 int		oresid;
	register unsigned	n;
	register dev_t		dev;
		 vnode_t	*vp;
		 struct csnode	*csp;
		 struct bmapval	bmv[2];
		 struct buf	*bp;


	SPEC_STATS(spec_cs_read_vop);

	vp  = BHV_TO_VNODE(bdp);
	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	if (! (ioflag & IO_ISLOCKED))
		spec_cs_rwlock(bdp, VRWLOCK_READ);

	/*
	 * 1st check for null (0 bytes) reads.
	 */
	if (uiop->uio_resid == 0) {
		error = 0;
		goto out;
	}

	ASSERT(vp->v_type == VCHR || vp->v_type == VBLK);

	/*
	 * Don't allow negative offsets into the block device code.
	 * It causes problems.  Some devices, most likely /dev/mem,
	 * do actually use negative offsets.
	 * This check is only up here so that we don't do the
	 * spec_cs_mark() call on a write that is going to fail.
	 */
	if ((uiop->uio_offset < 0) && (vp->v_type == VBLK)) {
		error = EINVAL;
		goto out;
	}

	if (WRITEALLOWED(vp, credp))
		spec_cs_mark(csp, SACC);

	dev = csp->cs_dev;

	if (vp->v_type == VCHR) {

		spec_cs_rwunlock_lcl(bdp, VRWLOCK_READ);

		if (csp->cs_flag & SSTREAM) {
			error = strread(vp, uiop, credp);
		} else {
			ASSERT(csp->cs_cdevsw != NULL);

			error = cdread(csp->cs_cdevsw, dev, uiop, credp);
		}

		spec_cs_rwlock_lcl(bdp, VRWLOCK_READ);

		goto out;
	}

	/*
	 * If we're here, it must be a VBLK device.
	 *
	 * Lock csp to keep the common csnode/vnode from being killed.
	 */
	SPEC_CSP_LOCK(csp);

	if ((ioflag & IO_DIRECT) == 0 && (csp->cs_flag & SMOUNTED))
		ioflag |= IO_DIRECT;

	if (ioflag & IO_DIRECT) {
		ASSERT(csp->cs_bdevsw != NULL);

		SPEC_CSP_UNLOCK(csp);

		error = biophysio(csp->cs_bdevsw->d_strategy, 0, dev,
				  B_READ, OFFTOBB(uiop->uio_offset), uiop);
		goto out;
	}

	error  = 0;
	oresid = uiop->uio_resid;
	
	if (ioflag & IO_RSYNC)
		spec_cs_read_rsync(bdp, csp, ioflag, dev, credp);
	
	do {
		nbmv = 2;

		error = spec_cs_bmap_vop(bdp, uiop->uio_offset,
					 uiop->uio_resid, B_READ,
					 credp, &bmv[0], &nbmv);

		if (error || (n = bmv[0].pbsize) == 0)
			break;

		dev = bmv[0].pbdev;

		ASSERT(bmv[0].bn >= 0);

		if (nbmv > 1)
			bp = breada(dev, bmv[0].bn, bmv[0].length,
				    bmv[1].bn, bmv[1].length);
		else
			bp = bread(dev, bmv[0].bn, bmv[0].length);

		if (bp->b_resid)
			n = 0;
		else {
			error = uiomove(bp->b_un.b_addr + bmv[0].pboff,
					n, UIO_READ, uiop);
		}

		brelse(bp);

	} while (error == 0 && uiop->uio_resid > 0 && n != 0);

	SPEC_CSP_UNLOCK(csp);

	if (oresid != uiop->uio_resid)		/* partial read */
		error = 0;

out:
	if (! (ioflag & IO_ISLOCKED))
		spec_cs_rwunlock(bdp, VRWLOCK_READ);

	return error;
}


/*
 * The "common" side read subroutine.
 *
 * called through the spec_ls2cs_ops vector.
 */
/* ARGSUSED */
int
spec_cs_read_hndl(
	spec_handle_t	*handle,
	int		gen,
	vnode_t		*vp,			/* Not Used, needed for "dc" */
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl)
{
	int		error;
	csnode_t	*csp;


	SPEC_STATS(spec_cs_read_hndl);

	csp = HANDLE_TO_CSP(handle);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	/*
	 * check if its's a "killed" csnode - we don't worry about the race
	 * - at some point in the future we could add some counters to record
	 * who is under the switch before killing ... for now we let
	 * folks under the switch stay there
	 */
	if (gen != csp->cs_gen)	{
#ifdef	JTK_DEBUG
		printf("spec_cs_read_hndl: EIO returned, gen/%d cs_gen/%d\n",
							gen, csp->cs_gen);
#endif	/* JTK_DEBUG */

		return EIO;
	}

	/*
	 * Use the common _read routine as if by way of a VOP_.
	 */
	error = spec_cs_read_vop(CSP_TO_BHV(csp), uiop, ioflag, credp, fl);

	SPEC_STATS(spec_cs_read_hndl_exit);

	return error;
}


/*
 * The "common" side write called via the VOP_ vector.
 *
 * This is usually just the console interface, which, for the moment,
 * only likes to operate against the common vnode...
 */
/*
 * VOP_RWLOCK has been done (if IO_ISLOCKED flag is set)
 */
/* ARGSUSED */
STATIC int
spec_cs_write_vop(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl)
{
		 int		error;
		 int		nbmv;
		 int		oresid;
	register unsigned	n;
	register dev_t		dev;
		 vnode_t	*vp;
		 struct csnode	*csp;
		 struct bmapval	bmv;
		 struct buf	*bp;


	SPEC_STATS(spec_cs_write_vop);

	vp  = BHV_TO_VNODE(bdp);
	csp = BHV_TO_CSP(bdp);

	ASSERT(credp != NULL);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	if (! (ioflag & IO_ISLOCKED))
		spec_cs_rwlock(bdp, VRWLOCK_WRITE);

	dev = csp->cs_dev;

	if (vp->v_type == VCHR) {

		spec_cs_mark(csp, SUPD|SCHG);

		spec_cs_rwunlock_lcl(bdp, VRWLOCK_READ);

		if (csp->cs_flag & SSTREAM) {
			error = strwrite(vp, uiop, credp);
		} else {
			ASSERT(csp->cs_cdevsw != NULL);

			error = cdwrite(csp->cs_cdevsw, dev, uiop, credp);
		}

		spec_cs_rwlock_lcl(bdp, VRWLOCK_READ);

		goto out;
	}


	/*
	 * If we're here, it must be a VBLK device.
	 *
	 * 1st check for null (0 bytes) writes.
	 */
	if (uiop->uio_resid == 0) {
		error = 0;
		goto out;
	}

	/*
	 * Don't allow negative offsets into the block device code.
	 * It causes problems.  Some devices, most likely /dev/mem,
	 * do actually use negative offsets.
	 */
	if (uiop->uio_offset < 0) {
		error = EINVAL;
		goto out;
	}

	SPEC_CSP_LOCK(csp);

	if ((ioflag & IO_DIRECT) == 0 && (csp->cs_flag & SMOUNTED))
		ioflag |= IO_DIRECT;

	if (ioflag & IO_DIRECT) {
		ASSERT(csp->cs_bdevsw != NULL);

		SPEC_CSP_UNLOCK(csp);

		error = biophysio(csp->cs_bdevsw->d_strategy, 0, dev,
				  B_WRITE, OFFTOBB(uiop->uio_offset), uiop);
		goto out;
	}

	oresid = uiop->uio_resid;
	do {
		nbmv = 1;

		error = spec_cs_bmap_vop(bdp, uiop->uio_offset,
					 uiop->uio_resid, B_WRITE,
					 credp, &bmv, &nbmv);
		if (error)
			break;

		dev = bmv.pbdev;

		if ((n = bmv.pbsize) == bmv.bsize) 
			bp = getblk(dev, bmv.bn, bmv.length);
		else
			bp = bread(dev, bmv.bn, bmv.length);

		error = uiomove(bp->b_un.b_addr + bmv.pboff, n, UIO_WRITE,
				uiop);
		if (error)
			brelse(bp);
		else if (ioflag & (IO_SYNC | IO_DSYNC))
			bwrite(bp);
		else
			bdwrite(bp);

		/* XXXbe svr4 does spec_cs_mark here */
	} while (error == 0 && uiop->uio_resid > 0 && n != 0);

	/*
	 * SPEC_CSP_LOCK/SPEC_CSP_UNLOCK are double-trippable,
	 * so order spec_cs_mark()/SPEC_CSP_UNLOCK() to take advantage
	 * of faster double-tripping route.
	 */
	spec_cs_mark(csp, SUPD|SCHG);

	SPEC_CSP_UNLOCK(csp);

	if (oresid != uiop->uio_resid)		/* partial write */
		error = 0;

out:
	if (! (ioflag & IO_ISLOCKED))
		spec_cs_rwunlock(bdp, VRWLOCK_WRITE);

	return error;
}


/*
 * The "common" side write subroutine.
 *
 * called through the spec_ls2cs_ops vector.
 */
/* ARGSUSED */
int
spec_cs_write_hndl(
	spec_handle_t	*handle,
	int		gen,
	vnode_t		*vp,			/* Not Used, needed for "dc" */
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl)
{
	int		error;
	csnode_t	*csp;


	SPEC_STATS(spec_cs_write_hndl);

	csp = HANDLE_TO_CSP(handle);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	/*
	 * check if its's a "killed" csnode - we don't worry about the race
	 * - at some point in the future we could add some counters to record
	 * who is under the switch before killing ... for now we let
	 * folks under the switch stay there
	 */
	if (gen != csp->cs_gen)	{
#ifdef	JTK_DEBUG
		printf("spec_cs_write_hndl: EIO returned, gen/%d cs_gen/%d\n",
							gen, csp->cs_gen);
#endif	/* JTK_DEBUG */

		return EIO;
	}

	/*
	 * Use the common _write routine as if by way of a VOP_.
	 */
	error = spec_cs_write_vop(CSP_TO_BHV(csp), uiop, ioflag, credp, fl);

	SPEC_STATS(spec_cs_write_hndl_exit);

	return error;
}




/*
 * "spec_cs_ioctl_hndl" is a "common" subroutine that interfaces
 * into the "ioctl" complex using the common vnode.
 *
 * called through the spec_ls2cs_ops vector.
 */
/* ARGSUSED */
int
spec_cs_ioctl_hndl(
	spec_handle_t	*handle,
	int		gen,
	int		cmd,
	void		*arg,
	int		mode,
	struct cred	*credp,
	int		*rvalp,
	struct vopbd	*vbd)
{
	csnode_t	*csp;
	dev_t		dev;
	vnode_t		*vp;


	SPEC_STATS(spec_cs_ioctl_hndl);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp != NULL);			/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */


	vp = CSP_TO_VP(csp);

	if (gen != csp->cs_gen)	{		/* was it "vn_kill'd" ?   */
#ifdef	JTK_DEBUG
		printf("spec_cs_ioctl_hndl: EIO returned, gen/%d cs_gen/%d\n",
							gen, csp->cs_gen);
#endif	/* JTK_DEBUG */

		return EIO;
	}

	if (vp->v_type != VCHR)
		return ENOTTY;

	dev = csp->cs_dev;

	if (csp->cs_cdevsw == NULL)
		return ENODEV;

	if (csp->cs_flag & SSTREAM)
		return strioctl(vp, cmd, arg, mode, U_TO_K, credp, rvalp);
	else
		return cdioctl(csp->cs_cdevsw, dev,
						cmd, arg, mode, credp, rvalp);
}


/*
 * "spec_cs_ioctl_vop" is the routine in the specfs common vnodeops
 * vector used to service anyone directly using the common vnode.
 * This usually turns out to just be the direct console devices
 * serviced by "getty".
 */
/* ARGSUSED */
STATIC int
spec_cs_ioctl_vop(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		mode,
	struct cred	*credp,
	int		*rvalp,
	struct vopbd	*vbd)
{
	int		error;
	csnode_t	*csp;
#ifdef	CELL_IRIX
	service_t	svc;
#endif	/* CELL_IRIX */
	spec_handle_t	handle;
	/* REFERENCED */
	vnode_t		*cvp;


	SPEC_STATS(spec_cs_ioctl_vop);

	cvp = BHV_TO_VNODE(bdp);
	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

#ifdef	CELL_IRIX
	SERVICE_MAKE(svc, cellid(), SVC_SPECFS);

	SPEC_HANDLE_MAKE(handle, svc, csp, cvp->v_rdev, cvp->v_type);
#else	/* ! CELL_IRIX */
	handle.objid = (void *)csp;
#endif	/* ! CELL_IRIX */

	error = spec_cs_ioctl_hndl(&handle, csp->cs_gen,
					cmd, arg, mode, credp, rvalp, vbd);
	return error;
}


/* ARGSUSED */
STATIC int
spec_cs_getattr_vop(
		 bhv_desc_t	*bdp,
	register struct vattr	*vap,
		 int		flags,
		 struct cred	*credp)
{
	csnode_t	*csp;
	vnode_t		*cvp;


	SPEC_STATS(spec_cs_getattr_vop);

	cvp = BHV_TO_VNODE(bdp);

	csp = BHV_TO_CSP(bdp);
	
	SPEC_CSP_LOCK(csp);


	vap->va_size = BBTOB(csp->cs_size);

	if (vap->va_mask == AT_SIZE) {

		SPEC_CSP_UNLOCK(csp);

		return 0;
	}

	vap->va_fsid   = csp->cs_fsid;
	vap->va_nodeid = (long)csp & 0xFFFF;	/* XXXjtk -- must fix?	*/
	vap->va_nlink  = csp->cs_opencnt;

	/*
	 * Quick exit for non-stat callers
	 */
	if ((vap->va_mask & ~(AT_SIZE|AT_FSID|AT_NODEID|AT_NLINK)) == 0) {

		SPEC_CSP_UNLOCK(csp);

		return 0;
	}

	/*
	 * Copy from in-core snode.
	 */
	vap->va_type = cvp->v_type;
	vap->va_mode = csp->cs_mode;

	vap->va_uid    = csp->cs_uid;
	vap->va_gid    = csp->cs_gid;
	vap->va_projid = csp->cs_projid;

	vap->va_rdev = csp->cs_dev;

	vap->va_atime.tv_sec  = csp->cs_atime;
	vap->va_atime.tv_nsec = 0;

	vap->va_mtime.tv_sec  = csp->cs_mtime;
	vap->va_mtime.tv_nsec = 0;

	vap->va_ctime.tv_sec  = csp->cs_ctime;
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

		SPEC_CSP_UNLOCK(csp);

		return 0;
	}

	vap->va_xflags    = 0;
	vap->va_extsize   = 0;
	vap->va_nextents  = 0;
	vap->va_anextents = 0;
	vap->va_gencount  = csp->cs_gen;
	vap->va_vcode     = 0L;


	SPEC_CSP_UNLOCK(csp);

	return 0;
}


/*
 * "spec_cs_setattr_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_setattr_vop(
		 bhv_desc_t	*bdp,
	register struct vattr	*vap,
		 int		flags,
		 struct cred	*credp)
{
	int		file_owner;
	int		mask;
	int		timeflags = 0;
	u_int16_t	projid, iprojid;
	csnode_t	*csp;
	gid_t		gid, igid;
	timespec_t	tv;
	uid_t		uid, iuid;
	vnode_t		*cvp;


	SPEC_STATS(spec_cs_setattr_vop);

	cvp = BHV_TO_VNODE(bdp);

	csp = BHV_TO_CSP(bdp);
	
	/*
	 * Cannot set certain attributes.
	 */
	mask = vap->va_mask;
	if (mask & AT_NOSET)
		return EINVAL;

	SPEC_CSP_LOCK(csp);

	/*
	 * Timestamps do not need to be logged and hence do not
	 * need to be done within a transaction.
	 */
	if (mask & AT_UPDTIMES) {

		ASSERT((mask & ~AT_UPDTIMES) == 0);

		timeflags = ((mask & AT_UPDATIME) ? SPECFS_ICHGTIME_ACC : 0)
			  | ((mask & AT_UPDCTIME) ? SPECFS_ICHGTIME_CHG : 0)
			  | ((mask & AT_UPDMTIME) ? SPECFS_ICHGTIME_MOD : 0);

		nanotime_syscall(&tv);

		if (timeflags & SPECFS_ICHGTIME_MOD)
			csp->cs_mtime = tv.tv_sec;

		if (timeflags & SPECFS_ICHGTIME_ACC)
			csp->cs_atime = tv.tv_sec;

		if (timeflags & SPECFS_ICHGTIME_CHG)
			csp->cs_ctime = tv.tv_sec;

		SPEC_CSP_UNLOCK(csp);

		return 0;
	}

	/*
	 * boolean: are we the file owner?
	 */
	file_owner = (credp->cr_uid == csp->cs_uid);

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

			SPEC_CSP_UNLOCK(csp);

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
			    && !groupmember(csp->cs_gid, credp))
				m |= ISGID;

			if ((vap->va_mode & ISVTX) && cvp->v_type != VDIR)
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
		iuid    = csp->cs_uid;
		igid    = csp->cs_gid; 
		iprojid = csp->cs_projid;

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

			SPEC_CSP_UNLOCK(csp);

			return EPERM;
		}

	}

	/*
	 * Truncate file.  Must have write permission and not be a directory.
	 */
	if (mask & AT_SIZE) {

		SPEC_CSP_UNLOCK(csp);

		return 0;
	}

	/*
	 * Change file access or modified times.
	 */
	if (mask & (AT_ATIME|AT_MTIME)) {

		if (!file_owner) {

			if (   (flags & ATTR_UTIME)
			    && !cap_able_cred(credp, CAP_FOWNER)) {

				SPEC_CSP_UNLOCK(csp);

				return EPERM;
			}
		}
	}

	/*
	 * Change extent size or realtime flag.
	 */
	if (mask & (AT_EXTSIZE|AT_XFLAGS)) {

		SPEC_CSP_UNLOCK(csp);

		return EINVAL;
	}

	/*
	 * Change file access modes.
	 */
	if (mask & AT_MODE) {

		csp->cs_mode &= IFMT;

		csp->cs_mode |= vap->va_mode & ~IFMT;

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
		if (  (csp->cs_mode & (ISUID|ISGID))
		    && !cap_able_cred(credp, CAP_FSETID)) {

			csp->cs_mode &= ~(ISUID|ISGID);
		}
		
		csp->cs_uid    = uid;
		csp->cs_gid    = gid;
		csp->cs_projid = projid;

		timeflags |= SPECFS_ICHGTIME_CHG;
	}


	/*
	 * Change file access or modified times.
	 */
	if (mask & (AT_ATIME|AT_MTIME)) {

		if (mask & AT_ATIME) {

			csp->cs_atime = vap->va_atime.tv_sec;

			timeflags &= ~SPECFS_ICHGTIME_ACC;
		}

		if (mask & AT_MTIME) {

			csp->cs_mtime = vap->va_mtime.tv_sec;

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
			csp->cs_mtime = tv.tv_sec;

		if (timeflags & SPECFS_ICHGTIME_ACC)
			csp->cs_atime = tv.tv_sec;

		if (timeflags & SPECFS_ICHGTIME_CHG)
			csp->cs_ctime = tv.tv_sec;
	}


	SPEC_CSP_UNLOCK(csp);

	return 0;
}


/*
 * "spec_cs_access_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_access_vop(
	bhv_desc_t	*bdp,
	int		mode,
	struct cred	*credp)
{
	panic("spec_cs_access: called on common snode!");

	return ENODEV;
}


/*
 * "spec_cs_readlink_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_readlink_vop(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	struct cred	*credp)
{
	panic("spec_cs_readlink: called on common snode!");

	return ENODEV;
}


/* ARGSUSED */
STATIC int
spec_cs_fsync_vop(
	bhv_desc_t	*bdp,
	int		flag,
	struct cred	*credp,
	off_t		start,
	off_t		stop)
{
	SPEC_STATS(spec_cs_fsync_vop);

	/*
	 * XXXjtk - needs to be more here?
	 */
	return 0;
}


/* ARGSUSED */
STATIC int
spec_cs_inactive_vop(
	bhv_desc_t	*bdp,
	struct cred	*credp)
{
	int		already_locked;
	csnode_t	*csp;
	vnode_t		*vp;


	SPEC_STATS(spec_cs_inactive_vop);

	vp = BHV_TO_VNODE(bdp);
	csp = BHV_TO_CSP(bdp);

	vn_trace_entry(vp, "spec_cs_inactive_vop", (inst_t *)__return_address);

	already_locked = SPEC_CSTAB_ISLOCKED();

	/*
	 * lock the hash table while we tear things down.
	 * spec_delete will understand..
	 */
	if (! already_locked)
		SPEC_CSTAB_LOCK();

	spec_cs_delete(csp);

	/*
	 * Pull ourselves out of the behavior chain for this vnode.
	 */
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);

	spec_cs_free(csp);

	/*
	 * The inactive_teardown flag must have been set at vn_alloc time
	 */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	if (! already_locked)
		SPEC_CSTAB_UNLOCK();

	return VN_INACTIVE_NOCACHE;
}


/*
 * "spec_cs_fid_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_fid_vop(
	bhv_desc_t	*bdp,
	struct fid	**fidpp)
{
	panic("spec_cs_fid: called on common snode!");

	return ENODEV;
}


/*
 * "spec_cs_fid2_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_fid2_vop(
	bhv_desc_t	*bdp,
	struct fid	*fidp)
{
	panic("spec_cs_fid2: called on common snode!");

	return ENODEV;
}


/*
 * The "internal" version of spec_cs_rwlock().
 */
/* ARGSUSED */
STATIC void
spec_cs_rwlock(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	csnode_t	*csp;
	

	SPEC_STATS(spec_cs_rwlock);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	SPEC_CSP_LOCK(csp);
}


/*
 * The "internal" version of spec_cs_rwunlock().
 */
/* ARGSUSED */
STATIC void
spec_cs_rwunlock(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_rwunlock);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	SPEC_CSP_UNLOCK(csp);
}


/*
 * The only real reason we need these is for file locking -
 *
 *	if a write lock must sleep, it will call VOP_RWUNLOCK
 *	which needs to unlock the csnode - otherwise while waiting
 *	for a file lock things like stat, etc. will fail.
 */
/* ARGSUSED */
STATIC void
spec_cs_rwlock_vop(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	csnode_t	*csp;
	

	SPEC_STATS(spec_cs_rwlock_vop);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	SPEC_CSP_LOCK(csp);
}


/* ARGSUSED */
STATIC void
spec_cs_rwunlock_vop(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_rwunlock_vop);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	SPEC_CSP_UNLOCK(csp);
}


/*
 * "Local" copies of rwlock/rwunlock.
 *
 * These exist only for the moment to help in
 * validating how many locks are gotten/released
 * by various code paths.
 */
/* ARGSUSED */
STATIC void
spec_cs_rwlock_lcl(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	csnode_t	*csp;
	

	SPEC_STATS(spec_cs_rwlock_lcl);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	SPEC_CSP_LOCK(csp);
}


/* ARGSUSED */
STATIC void
spec_cs_rwunlock_lcl(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_rwunlock_lcl);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	SPEC_CSP_UNLOCK(csp);
}


/*
 * "spec_cs_seek_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_seek_vop(
	bhv_desc_t	*bdp,
	off_t		ooff,
	off_t		*noffp)
{
	panic("spec_cs_seek: called on common snode!");

	return ENODEV;
}


/*
 * "spec_cs_frlock_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_frlock_vop(
	bhv_desc_t	*bdp,
	int		cmd,
	struct flock	*bfp,
	int		flag,
	off_t		offset,
	vrwlock_t	vrwlock,
	struct cred	*credp)
{
	panic("spec_cs_frlock: called on common snode!");

	return ENODEV;
}


/*
 * "spec_cs_realvp_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_realvp_vop(
	register bhv_desc_t	*bdp,
	register struct vnode	**vpp)
{
	panic("spec_cs_realvp: called on common snode!");

	return ENODEV;
}


/*
 * "spec_cs_poll_hndl" is a "common" subroutine that interfaces
 * into the "poll" complex using the common vnode.
 *
 * called through the spec_ls2cs_ops vector.
 */
int
spec_cs_poll_hndl(
	spec_handle_t	*handle,
	long		gen,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead	**phpp,
	unsigned int	*genp)
{
	int		error;
	dev_t		dev;
	csnode_t	*csp;
	/* REFERENCED */
	vnode_t		*vp;


	SPEC_STATS(spec_cs_poll_hndl);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp != NULL);			/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	vp = CSP_TO_VP(csp);

	if (gen != csp->cs_gen)	{		/* was it "vn_kill'd" ?   */
#ifdef	JTK_DEBUG
		printf("spec_cs_poll_hndl: EIO returned, gen/%d cs_gen/%d\n",
							gen, csp->cs_gen);
#endif	/* JTK_DEBUG */

		return EIO;		/* lost race with vn_kill/spec_get */
	}

	dev = csp->cs_dev;

	ASSERT(csp->cs_cdevsw != NULL);

#ifdef	MP
	if (csp->cs_poll) {
		error = csp->cs_poll(csp->cs_polla0, events, anyyet,
						reventsp, (void **)phpp, genp);
	} else {
		ASSERT(vp->v_type == VCHR);

		error = cdpoll(csp->cs_cdevsw, dev, events, anyyet, reventsp,
								phpp, genp);
	}
#else  /* ! MP */

	error = csp->cs_poll(csp->cs_polla0, events, anyyet,
						reventsp, (void **)phpp, genp);
#endif /* ! MP */

	return error;
}


/*
 * "spec_cs_poll_vop" is the routine in the specfs common vnodeops
 * vector used to service anyone directly using the common vnode.
 * This usually turns out to just be the direct console devices
 * serviced by "getty".
 */
/* ARGSUSED */
STATIC int
spec_cs_poll_vop(
	bhv_desc_t	*bdp,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead	**phpp,
	unsigned int	*genp)
{
	int		error;
	csnode_t	*csp;
#ifdef	CELL_IRIX
	service_t	svc;
#endif	/* CELL_IRIX */
	spec_handle_t	handle;
	/* REFERENCED */
	vnode_t		*cvp;


	SPEC_STATS(spec_cs_poll_vop);

	cvp = BHV_TO_VNODE(bdp);
	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

#ifdef	CELL_IRIX
	SERVICE_MAKE(svc, cellid(), SVC_SPECFS);

	SPEC_HANDLE_MAKE(handle, svc, csp, cvp->v_rdev, cvp->v_type);
#else	/* ! CELL_IRIX */
	handle.objid = (void *)csp;
#endif	/* ! CELL_IRIX */

	error = spec_cs_poll_hndl(&handle, csp->cs_gen,
					events, anyyet, reventsp, phpp, genp);
	return error;
}


/* ARGSUSED */
int
spec_cs_bmap_vop(
	bhv_desc_t	*bdp,
	off_t		offset,
	ssize_t		count,
	int		rw,
	struct cred	*credp,
	struct bmapval	*bmap,
	int		*nbmap)
{
	csnode_t	*csp;
	daddr_t		devblocks;


	SPEC_STATS(spec_cs_bmap_vop);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	ASSERT(BHV_TO_VNODE(bdp)->v_type == VBLK);

	/*
	 * Convert users i/o offset into a block dev logical block number,
	 * then convert that into a physical block number.  Check against
	 * device size stored in csnode to avoid issuing a bread running over
	 * the end of the device.  Note safety net: stored size may be zero if
	 * the driver wasn't able to supply it; skip this check in that case.
	 * Also, if size is actually bigger than the 2 GB lseek range, trim
	 * it to avoid attempts to read over...
	 */
	devblocks = csp->cs_size; 

	if (devblocks > BBSEEKLIMIT)
		devblocks = BBSEEKLIMIT;

	bmap->bn = BLKDEV_LTOP(BLKDEV_LBN(offset));

	if (devblocks && (devblocks - bmap->bn) < (daddr_t)BLKDEV_BB) {
		if ((offset >> BBSHIFT) >= devblocks) {
			/*
			 * At or over end of device. If reading, coerce
			 * bmap->pbsize to zero to let spec_cs_read know.
			 * If writing, return ENOSPC to spec_cs_write.
			 */
			bmap->length = 0;
			bmap->pbsize = 0;

			if (rw == B_WRITE)
				return (ENOSPC);

			return (0);
		}

		bmap->length = devblocks - bmap->bn;
	} else {
		bmap->length = BLKDEV_BB;
	}

	bmap->pbdev = csp->cs_dev;
	bmap->pboff = BLKDEV_OFF(offset);
	bmap->bsize = BBTOB(bmap->length);
	bmap->pbsize = bmap->bsize - bmap->pboff;

	ASSERT(bmap->pbsize >= 0);

	if (count < bmap->pbsize)
		bmap->pbsize = count;

	if (rw == B_READ) {
		if (   *nbmap > 1
		    && (count > bmap->pbsize || csp->cs_nextr == bmap->bn)) {
			struct bmapval *rabmap = bmap + 1;

			*nbmap = 2;

			/*
			 * Read-ahead should be triggered if the user is
			 * reading beyond bmap or if it appears that this
			 * read is sequential from the last read.
			 * Again we must test for overrunning the device
			 * and shorten the request if necessary.
			 */
			rabmap->bn = bmap->bn + bmap->length;

			if (devblocks && (devblocks - rabmap->bn) < BLKDEV_BB) {
				rabmap->length = (devblocks - rabmap->bn > 0) ?
						devblocks - rabmap->bn : 0;

				if (rabmap->length <= 0)
					*nbmap = 1;
			} else
				rabmap->length = BLKDEV_BB;
		} else {
			*nbmap = 1;	/* paranoia */
		}

		csp->cs_nextr = bmap->bn + bmap->length;
	} else
		*nbmap = 1;

	return (0);
}


/*
 * "spec_cs_bmap_subr" is a "common" subroutine that interfaces
 * into the vnops "bmap" complex using the common vnode.
 *
 * called through the spec_ls2cs_ops vector.
 */
/* ARGSUSED */
int
spec_cs_bmap_subr(
	spec_handle_t	*handle,
	off_t		offset,
	ssize_t		count,
	int		rw,
	struct cred	*credp,
	struct bmapval	*bmap,
	int		*nbmap)
{
	int		error;
	csnode_t	*csp;


	SPEC_STATS(spec_cs_bmap_subr);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp != NULL);			/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	/*
	 * Call the common _bmap routine in specvnops_common.c
	 */
	error = spec_cs_bmap_vop(CSP_TO_BHV(csp),
				offset, count, rw, credp, bmap, nbmap);

	return error;
}


/*
 * "spec_cs_strategy_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC void
spec_cs_strategy_vop(
	bhv_desc_t	*bdp,
	struct buf	*bp)
{
	panic("spec_cs_strategy: called on common snode!");
}


/* ARGSUSED */
void
spec_cs_strategy_subr(
	spec_handle_t	*handle,
	struct buf	*bp)
{
	/* REFERENCED */
	csnode_t	*csp;


	SPEC_STATS(spec_cs_strategy_subr);

	csp = HANDLE_TO_CSP(handle);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	ASSERT(csp->cs_bdevsw != NULL);

	bdstrat(csp->cs_bdevsw, bp);
}


/*
 * "spec_cs_map_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_map_vop(
	bhv_desc_t	*bdp,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	u_int		flags,
	struct cred	*credp,
	vnode_t		**nvp)
{
	panic("spec_cs_map: called on common snode!");

	return ENODEV;
}


/*
 * "spec_cs_addmap_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_addmap_vop(
	bhv_desc_t	*bdp,
	vaddmap_t	op,
	vhandl_t	*vt,
	pgno_t		*pgno,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	struct cred	*credp)
{
	panic("spec_cs_addmap: called on common snode!");

	return ENODEV;
}


/* ARGSUSED */
int
spec_cs_addmap_subr(
	spec_handle_t	*handle,
	vaddmap_t	op,
	vhandl_t	*vt,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	struct cred	*credp,
	pgno_t		*pgno)
{
	int		error;
	csnode_t	*csp;
	vnode_t		*vp;


	SPEC_STATS(spec_cs_addmap_subr);

	csp = HANDLE_TO_CSP(handle);
	vp  = CSP_TO_VP(csp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	error = 0;

	if (op == VNEWMAP) {
		switch (vp->v_type) {
		case VCHR:
			ASSERT(csp->cs_cdevsw != NULL);

			error = cdmap(csp->cs_cdevsw, csp->cs_dev,
							vt, off, len, prot);

			if (error == ENODEV) {
				/*
				 * check for SVR4 style mmap interface
				 */
				if (csp->cs_cdevsw->d_mmap ==
					    (int (*)(dev_t, off_t, int))nodev) {
					return ENODEV;
				}

				/*
				 * return 'secret' message
				 * but go ahead and set mapcnt ..
				 */
				error = EBADE;
			}

			break;

		case VBLK:
			ASSERT(csp->cs_bdevsw != NULL);

			error = bdmap(csp->cs_bdevsw, vt, off, len, prot);

			break;

		default:
			error = ENODEV;
			break;
		}

	} else if (op == VITERMAP) {

		ASSERT(vp->v_type == VCHR);

		ASSERT(csp->cs_cdevsw != NULL);

		if ((*pgno = cdmmap(csp->cs_cdevsw, csp->cs_dev, off, prot))
								== NOPAGE) {
			error = ENXIO;
		}

		/*
		 * VITERMAP doesn't bump count - NEWMAP already did that
		 */
		return error;
	}

	/* VDUPMAP just bumps the mapcnt */
	
	if (error == 0 || error == EBADE) {
		/*
		 * Note that the mapcnt is the total 'mapping' size and
		 * doesn't really have anything to do with how many
		 * valid pages there are. The driver really never deals
		 * with the length issue
		 */
		SPEC_CSP_LOCK(csp);

		csp->cs_mapcnt += btopr(len);

		SPEC_CSP_UNLOCK(csp);
	}

	return error;
}


/*
 * "spec_cs_delmap_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_delmap_vop(
	bhv_desc_t	*bdp,
	vhandl_t	*vt,
	size_t		len,
	struct cred	*credp)
{
	panic("spec_cs_delmap: called on common snode!");

	return ENODEV;
}


/* ARGSUSED */
int
spec_cs_delmap_subr(
	spec_handle_t	*handle,
	vhandl_t	*vt,
	size_t		len,
	struct cred	*credp)
{
	long		len2use;
	csnode_t	*csp;
	vnode_t		*vp;


	SPEC_STATS(spec_cs_delmap_subr);

	csp = HANDLE_TO_CSP(handle);
	vp  = CSP_TO_VP(csp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	switch (vp->v_type) {
	case VCHR:
		ASSERT(csp->cs_cdevsw != NULL);

		(void) cdunmap(csp->cs_cdevsw, csp->cs_dev, vt);

		break;

	case VBLK:
		ASSERT(csp->cs_bdevsw != NULL);

		(void) bdunmap(csp->cs_bdevsw, vt);

		break;
	}

	SPEC_CSP_LOCK(csp);

	len2use = btopr(len);

	if (len2use > csp->cs_mapcnt) {

		len2use = csp->cs_mapcnt;

		cmn_err(CE_WARN,
			    "spec_cs_delmap: vp/0x%x mapcnt underflow %d/%d\n",
						vp, csp->cs_mapcnt, len2use);
	}

	csp->cs_mapcnt -= len2use;

	/*
	 * Call the close routine when the last reference of any
	 * kind through any [s,v]node goes away.
	 */
	if (csp->cs_opencnt <= 0 && csp->cs_mapcnt == 0) {

		if (csp->cs_flag & SWANTCLOSE) {

			csp->cs_flag &= ~SWANTCLOSE;

			/* XXXbe - want real file flags here. */
			(void) spec_cs_device_close(csp, 0, credp);
		}
	}

	SPEC_CSP_UNLOCK(csp);

	return 0;
}


/*
 * Return the canonical name for a device.
 */
/* ARGSUSED */
STATIC int
spec_cs_canonical_name_get(
	bhv_desc_t	*bdp, 
	char		*name, 
	char		*value, 
	int		*valuelenp, 
	int		flags, 
	struct cred	*credp)
{
	int		devlen;
	char		*canonical_name;
	char		devname[MAXDEVNAME];
	csnode_t	*csp;


	SPEC_STATS(spec_cs_canonical_name_get);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	/*
	 * 1st find out what the actual name & length is.
	 *
	 * Note:  dev_to_name may return a different address
	 *	  than "devname" if it's an "UnKnowDevice".
	 */
	canonical_name = dev_to_name(csp->cs_dev, devname, MAXDEVNAME);

	devlen = strlen(canonical_name) + 1;

	/*
	 * If we were called with a NULL "value", it means somebody
	 * above is "fishing" to find out the needed length.
	 */
	if (value == NULL) {

		*valuelenp = devlen;

		return 0;
	}

	/*
	 * Catch the case where the caller provided a zero, negative, or
	 * "just plain too short" buffer length and short-circuit back
	 * right now, in particular, keep "0" and negative lengths away from
	 * the dev_to_name() interface as it doesn't handle this too well.
	 */
	if (*valuelenp < devlen) {

		*valuelenp = devlen;

		return E2BIG;
	}

	/*
	 * Okay, we have the name, the user's length looks alright, so just
	 * copy it over, we "know" we're not dealing with a user address
	 * at this level.
	 */
	bcopy(canonical_name, value, devlen);

	*valuelenp = devlen;

	return 0;
}


extern mac_label *mac_low_high_lp;

/* ARGSUSED */
int
spec_cs_mac_pointer_get(
	bhv_desc_t	*bdp, 
	char		*name, 
	char		*value, 
	int		*valuelenp, 
	int		flags, 
	struct cred	*credp)
{
	
	SPEC_STATS(spec_cs_mac_pointer_get);

	bcopy(&mac_low_high_lp, value, sizeof(mac_label *));

	*valuelenp = sizeof(mac_label *);

	return(0);
}


/* ARGSUSED */
int
spec_cs_mac_label_get(
	bhv_desc_t	*bdp, 
	char		*name, 
	char		*value, 
	int		*valuelenp, 
	int		flags, 
	struct cred	*credp)
{
	int	i;


	SPEC_STATS(spec_cs_mac_label_get);

	i = (int) mac_size(mac_low_high_lp);

	bcopy(mac_low_high_lp, value, i);

	*valuelenp = i;

	return(0);
}


/*
 * The spec_cs_attr_to_fn_table defines which function should be called for
 * a given attribute.  Only attributes handled locally by specfs are listed.
 */
typedef int (*attr_fn_t)
	(bhv_desc_t *, char *, char *, int *, int, struct cred *);

STATIC
struct spec_cs_attr_to_fn {
	char		*saf_name;
	attr_fn_t	saf_func;
} spec_cs_attr_to_fn_table[] = {
	{_DEVNAME_ATTR,		spec_cs_canonical_name_get},
	{SGI_MAC_POINTER,	spec_cs_mac_pointer_get},
	{SGI_MAC_FILE,		spec_cs_mac_label_get},
};

int num_spec_cs_attr_fn = sizeof(spec_cs_attr_to_fn_table)
					/ sizeof(struct spec_cs_attr_to_fn);

/*
 * Given an attribute name, return a local function that handles that
 * attribute name, or NULL if the attribute is not supposed to be
 * handled locally by specfs.
 */
STATIC attr_fn_t
spec_cs_attr_func(char *name)
{
	int	i;

	SPEC_STATS(spec_cs_attr_func);

	for (i = 0; i < num_spec_cs_attr_fn; i++)
		if (!strcmp(spec_cs_attr_to_fn_table[i].saf_name, name))
			return(spec_cs_attr_to_fn_table[i].saf_func);

	return NULL;
}


/*
 * "spec_cs_attr_get_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
STATIC int
spec_cs_attr_get_vop(
	bhv_desc_t	*bdp,
	char		*name,
	char		*value,
	int		*valuelenp,
	int		flags,
	struct cred	*credp)
{
	int		error;
	attr_fn_t	func;
	csnode_t	*csp;


	SPEC_STATS(spec_cs_attr_get_vop);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	/*
	 * If the specified attribute is one that's supposed to be handled
	 * directly by specfs, then handle it.  Otherwise, use the info we
	 * got earlier.
	 */
	func = spec_cs_attr_func(name);

	if (func != NULL) {

		error = func(bdp, name, value, valuelenp, flags, credp);

		return error;
	}
		
	/*
	 * If we're here, then we didn't find a way of handling it via
	 * spec_cs_attr_func(), just return an error.
	 */
	return EINVAL;
}


/*
 * "spec_cs_attr_set_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_attr_set_vop(
	bhv_desc_t	*bdp,
	char		*name,
	char		*value,
	int		valuelen,
	int		flags,
	struct cred	*credp)
{
	/* REFERENCED */
	csnode_t	*csp;


	SPEC_STATS(spec_cs_attr_set_vop);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	return EINVAL;
}


/*
 * "spec_cs_attr_remove_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_attr_remove_vop(
	bhv_desc_t	*bdp,
	char		*name,
	int		flags,
	struct cred	*credp)
{
	/* REFERENCED */
	csnode_t	*csp;


	SPEC_STATS(spec_cs_attr_remove_vop);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	return EINVAL;
}


/*
 * "spec_cs_attr_list_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
STATIC int
spec_cs_attr_list_vop(
	bhv_desc_t		*bdp,
	char			*buffer,
	int			bufsize,
	int			flags,
	attrlist_cursor_kern_t	*cursor,
	struct cred		*credp)
{
	/* REFERENCED */
	csnode_t	*csp;


	SPEC_STATS(spec_cs_attr_list_vop);

	csp = BHV_TO_CSP(bdp);
						/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	return EINVAL;
}


/*
 * "spec_cs_strgetmsg_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
int
spec_cs_strgetmsg_vop(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	panic("spec_cs_strgetmsg_vop: called on common snode!");

	return ENODEV;
}


int
spec_cs_strgetmsg_hndl(
	spec_handle_t	*handle,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	int		error;
	csnode_t	*csp;

	SPEC_STATS(spec_cs_strgetmsg_hndl);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp != NULL);			/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	error = fs_strgetmsg(CSP_TO_BHV(csp), mctl, mdata, prip, flagsp,
								fmode, rvp);

	return error;
}


/*
 * "spec_cs_strputmsg_vop" is the routine in the specfs common vnodeops
 * vector used to catch anyone directly using the common vnode.
 */
/* ARGSUSED */
int
spec_cs_strputmsg_vop(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	panic("spec_cs_strputmsg_vop: called on common snode!");

	return ENODEV;
}


int
spec_cs_strputmsg_hndl(
	spec_handle_t	*handle,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	int		error;
	csnode_t	*csp;

	SPEC_STATS(spec_cs_strputmsg_hndl);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp != NULL);			/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	error = fs_strputmsg(CSP_TO_BHV(csp), mctl, mdata, pri, flag, fmode);

	return error;
}


vnodeops_t spec_cs_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_SPECFS_DS),
	spec_cs_open_vop,			/* Usable		*/
	spec_cs_close_vop,			/* Usable		*/
	spec_cs_read_vop,			/* Usable		*/
	spec_cs_write_vop,			/* Usable		*/
	spec_cs_ioctl_vop,			/* Usable		*/
	fs_setfl,
	spec_cs_getattr_vop,			/* Usable		*/
	spec_cs_setattr_vop,			/* Usable		*/
	spec_cs_access_vop,				/* Panic	*/
	(vop_lookup_t)fs_nosys,
	(vop_create_t)fs_nosys,
	(vop_remove_t)fs_nosys,
	(vop_link_t)fs_nosys,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	(vop_readdir_t)fs_nosys,
	(vop_symlink_t)fs_nosys,
	spec_cs_readlink_vop,				/* Panic	*/
	spec_cs_fsync_vop,			/* Usable		*/
	spec_cs_inactive_vop,			/* Usable		*/
	spec_cs_fid_vop,				/* Panic	*/
	spec_cs_fid2_vop,				/* Panic	*/
	spec_cs_rwlock_vop,			/* Usable		*/
	spec_cs_rwunlock_vop,			/* Usable		*/
	spec_cs_seek_vop,				/* Panic	*/
	fs_cmp,
	spec_cs_frlock_vop,				/* Panic	*/
	spec_cs_realvp_vop,				/* Panic	*/
	spec_cs_bmap_vop,			/* Usable		*/
	spec_cs_strategy_vop,				/* Panic	*/
	spec_cs_map_vop,				/* Panic	*/
	spec_cs_addmap_vop,				/* Panic	*/
	spec_cs_delmap_vop,				/* Panic	*/
	spec_cs_poll_vop,			/* Usable		*/
	(vop_dump_t)fs_nosys,
	fs_pathconf,
	(vop_allocstore_t)fs_noerr,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	spec_cs_attr_get_vop,			/* Usable		*/
	spec_cs_attr_set_vop,			/* Usable		*/
	spec_cs_attr_remove_vop,		/* Usable		*/
	spec_cs_attr_list_vop,			/* Usable		*/
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
	fs_strgetmsg,
	fs_strputmsg,
};
