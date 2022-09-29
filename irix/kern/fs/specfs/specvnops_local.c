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

#ident	"$Revision: 1.47 $"

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
#include <sys/pvnode.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/runq.h>
#include <sys/ddi.h>
#include <sys/attributes.h>
#include <sys/major.h>
#include <fs/specfs/spec_lsnode.h>


STATIC void spec_rwlock(bhv_desc_t *, vrwlock_t);
STATIC void spec_rwunlock(bhv_desc_t *, vrwlock_t);

STATIC void spec_teardown(bhv_desc_t *bdp);


#define PCASTTYPE (int (*)(void *, short, int, short *,		\
					void **, unsigned int *))


extern mutex_t spec_lsnode_tablock;		/* spec_lsnode_table lock */

/* 
 * Macros to lock and to unlock lsnode hash table, used during modifications
 * and during traversal of the hash list with possibility of sleep
 */
#define SPEC_LSTAB_LOCK()	mutex_lock(&spec_lsnode_tablock, PINOD)
#define SPEC_LSTAB_UNLOCK()	mutex_unlock(&spec_lsnode_tablock)
#define SPEC_LSTAB_ISLOCKED()	mutex_mine(&spec_lsnode_tablock)


#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
/*
 * VN_KILL poll stub
 */
/* ARGSUSED */
STATIC int
spec_kill_poll(
	void		*a0,
	short		events,
	int		anyyet,
	short		*reventsp,
	void		**vpp,
	unsigned int	*genp)
{
	SPEC_STATS(spec_kill_poll);

	return EIO;
}


/*
 * Return default answer to poll() for those without poll handlers.
 */
/* ARGSUSED */
STATIC int
spec_dflt_poll(
	void		*a0,
	short		events,
	int		anyyet,
	short		*reventsp,
	void		**vpp,
	unsigned int	*genp)
{
	SPEC_STATS(spec_dflt_poll);

	*reventsp = events & (POLLIN|POLLRDNORM|POLLOUT);

	return 0;
}
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */


/*
 * Allow handler of special files to initialize and validate before actual I/O.
 */
int
spec_open(
	bhv_desc_t	*bdp,
	struct vnode	**vpp,
	mode_t		flag,
	struct cred	*credp)
{
#if	defined(MP) && ! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
	int		dflags;
#endif	/* defined(MP) && ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */
	int		error;
	daddr_t		vblk_size;
	dev_t		newdev;
	lsnode_t	*lsp;
	lsnode_t	*nlsp;
	/* REFERENCED */
	vnode_t		*vp;
	struct stdata	*stp;


	SPEC_STATS(spec_open);

	error = 0;

	vp  = BHV_TO_VNODE(bdp);
	lsp = BHV_TO_LSP(bdp);

						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	ASSERT(vpp != NULL);

	ASSERT(vp->v_count > 0);

	/*
	 * Deny access if the containing filesystem was mounted "nodev".
	 */
	if (vp->v_vfsp && (vp->v_vfsp->vfs_flag & VFS_NODEV))
		return EACCES;

	flag &= ~FCREAT;		/* paranoia */


	VN_HOLD(vp);			/* Bump the hold count to keep	*/
					/* things in order while we're	*/
					/* off trying to open the	*/
					/* "common" side		*/

	newdev = vp->v_rdev;		/* pre-set the dev's	*/

	vblk_size = 0;

	stp = NULL;

	/*
	 * Attempt to "open" the common side 1st.
	 */
	error = VSPECFS_VOP_OPEN(lsp, &lsp->ls2cs_handle, lsp->ls_gen,
					&newdev, &vblk_size, &stp, flag, credp);


	SPEC_LSP_LOCK(lsp);

	VN_RELE(vp);			/* drop the count that was	*/
					/* held across the "common"	*/
					/* side open.			*/
	if (error != 0) {
		SPEC_LSP_UNLOCK(lsp);

		return error;
	}

	lsp->ls_flag &= ~SINACTIVE;		/* Mark "active"	*/

	lsp->ls_opencnt++;

	/*
	 * Check to see if the "common" side has indicated
	 * that this was a "stream" open, if so note that fact
	 * in the "local" snode.
	 */
	if (stp)
		lsp->ls_flag |= SSTREAM;

	/*
	 * Now follow down pretty much the same path as the "common" side,
	 * using information returned from the "common" side to mimic cloning.
	 */
	switch (vp->v_type) {

	case VCHR:

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
		if (lsp->ls_cdevsw == NULL) {

			lsp->ls_opencnt--;

			error = ENXIO;

			break;
		}
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

		if (lsp->ls_flag & SSTREAM) {

			if (vp->v_rdev != newdev) {
#ifdef	JTK_STR_DEBUG
				printf(
"spec_open: d_str CLONING vp/0x%x *vpp/0x%x stp/0x%x dev(%d/%d, %d) ",
							vp, *vpp, stp,
							major(vp->v_rdev),
							emajor(vp->v_rdev),
							minor(vp->v_rdev));
				printf("ndev(%d/%d, %d)!\n",
							major(newdev),
							emajor(newdev),
							minor(newdev));
#endif	/* JTK_STR_DEBUG */
				/*
				 * Streams clone open.
				 *
				 * The main difference here is that we
				 * need to pass it the streams head pointer
				 * returned from the "common" open.
				 */
				nlsp = spec_make_clone(lsp, newdev,
							VCHR, vp, vpp,
							stp, flag);
				/*
				 * NOTE! "lsp" & "vp" are no longer ours,
				 *	 they were released by
				 *	 spec_make_clone(), who also
				 *	 returned with the new vp vn_held.
				 */
				vp = LSP_TO_VP(nlsp);

				vn_trace_entry(vp, "spec_open_hold.1",
						(inst_t *)__return_address);

				ASSERT(vp->v_stream == (*vpp)->v_stream);

				lsp = nlsp;

			} else {
				/*
				 * Normal open.
				 */
				(*vpp)->v_stream = stp;
			}

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
			lsp->ls_poll   = PCASTTYPE strpoll;
			lsp->ls_polla0 = (void *)((*vpp)->v_stream);
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

		} else {
			/* 
			 * It must be a loadable module.
			 */
			if (vp->v_rdev != newdev) {
#ifdef	JTK_STR_DEBUG
				printf(
		"spec_open: VCHR CLONING dev(%d/%d, %d) ndev(%d/%d, %d)!\n",
							major(vp->v_rdev),
							emajor(vp->v_rdev),
							minor(vp->v_rdev),
							major(newdev),
							emajor(newdev),
							minor(newdev));
#endif	/* JTK_STR_DEBUG */
				/*
				 * Clone open.
				 */
				nlsp = spec_make_clone(lsp, newdev,
								VCHR, vp, vpp,
								NULL, flag);
				/*
				 * NOTE! "lsp" & "vp" are no longer ours,
				 *	 they were released by
				 *	 spec_make_clone(), who also
				 *	 returned with the new vp vn_held.
				 */
				lsp = nlsp;

				vp = LSP_TO_VP(nlsp);

				vn_trace_entry(vp, "spec_open_hold.2",
						(inst_t *)__return_address);
			}

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))

#ifdef	MP
			dflags = *lsp->ls_cdevsw->d_flags;

			if (lsp->ls_cdevsw->d_poll != NULL) {
				lsp->ls_poll = PCASTTYPE lsp->ls_cdevsw->d_poll;

				if ((dflags & DLOCK_MASK) != D_MP)
				    lsp->ls_poll = NULL;

			} else {
				lsp->ls_poll = spec_dflt_poll;
			}
#else	/* ! MP */
			if (lsp->ls_cdevsw->d_poll != NULL)
				lsp->ls_poll = PCASTTYPE lsp->ls_cdevsw->d_poll;
			else 
				lsp->ls_poll = spec_dflt_poll;
#endif	/* ! MP */
			lsp->ls_polla0 = (void *)(__psunsigned_t)newdev;
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */
		}

		break;

	case VBLK:

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
		if (lsp->ls_bdevsw == NULL) {

			lsp->ls_opencnt--;

			error = ENXIO;

			break;
		}
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

		lsp->ls_size = vblk_size;

		if (vp->v_rdev != newdev) {
#ifdef	JTK_STR_DEBUG
			printf(
		"spec_open: VBLK CLONING dev(%d/%d, %d) ndev(%d/%d, %d)!\n",
							major(vp->v_rdev),
							emajor(vp->v_rdev),
							minor(vp->v_rdev),
							major(newdev),
							emajor(newdev),
							minor(newdev));
#endif	/* JTK_STR_DEBUG */

			/*
			 * Clone open.
			 */
			nlsp = spec_make_clone(lsp, newdev,
							VBLK, vp, vpp,
							NULL, flag);
			/*
			 * NOTE! "lsp" & "vp" are no longer ours,
			 *	 they were released by spec_make_clone(),
			 *	 who also  returned with the new vp vn_held.
			 */
			lsp = nlsp;

			vp = LSP_TO_VP(nlsp);

			vn_trace_entry(vp, "spec_open_hold.3",
						(inst_t *)__return_address);
		}

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
		lsp->ls_poll   = spec_dflt_poll;
		lsp->ls_polla0 = (void *)(__psunsigned_t)newdev;
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

		break;

	default:
		cmn_err_tag(85,CE_PANIC, "spec_open: type not VCHR or VBLK\n");
		break;
	}

	SPEC_LSP_UNLOCK(lsp);

	return error;
}


/* ARGSUSED */
STATIC int
spec_close(
	bhv_desc_t	*bdp,
	int		flag,
	lastclose_t	lastclose,
	struct cred	*credp)
{
	int		error;
	lsnode_t	*lsp;
	/* REFERENCED */
	vnode_t		*vp;


	SPEC_STATS(spec_close);

	error = 0;

	vp  = BHV_TO_VNODE(bdp);
	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */
	ASSERT((lsp->ls_flag & SINACTIVE) == 0);

	ASSERT(vp->v_count > 0);

	/*
	 * Attempt to "close" the common side 1st.
	 */
	error = VSPECFS_VOP_CLOSE(lsp, &lsp->ls2cs_handle,
					flag, lastclose, credp);
	if (error)
		return error;

	SPEC_LSP_LOCK(lsp);

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

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
		/*
		 * When we obsolete the lsnode, change the poll function
		 * pointer to a stub that returns EIO.
		 *
		 * This saves the overhead of comparing the lsnode generation
		 * numbers in the spec_poll() routine on every call.
		 */
		lsp->ls_poll = spec_kill_poll;
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

		SPEC_LSP_UNLOCK(lsp);

		return 0;
	}

	if (lastclose == L_FALSE) {

		SPEC_LSP_UNLOCK(lsp);

		return 0;
	}

	/*
	 * Drop references added by open.
	 * NOTE! There are still some strange export/import
	 *	 interactions that can allow a process to
	 *	 open a device (most notably the ctty) on
	 *	 one lsnode reference, then throught export/
	 *	 import/re-export/re-import cause the close
	 *	 to occur on a different lsnode reference....
	 */
	lsp->ls_opencnt--;

	SPEC_LSP_UNLOCK(lsp);

	return error;
}


#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
/*
 * Do the right flushing for the read case when the IO_RSYNC flag is set.
 */
STATIC void
spec_read_rsync(
	bhv_desc_t	*bdp,
	lsnode_t	*lsp,
	int		ioflag,
	dev_t		dev,	 
	cred_t		*credp)		
{
	int		error;
	bhv_desc_t	*nbdp;
	vnode_t		*fsvp;
	struct vattr	va, vatmp;
	/* REFERENCED */ 	
	int unused; 			


	SPEC_STATS(spec_read_rsync);

	if (ioflag & (IO_SYNC | IO_DSYNC))
		bflush(dev);

	if (!(ioflag & IO_SYNC))
		return;

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	/*
	 * If times didn't change, don't flush anything.
	 */
	if (   ((lsp->ls_flag & (SACC|SUPD|SCHG)) != 0)
	    || (LSP_TO_VP(lsp)->v_type == VBLK)) {
		
		lsp->ls_flag &= ~(SACC|SUPD|SCHG);

		/*
		 * If no real vnode to update, don't flush anything.
		 */
		if ((fsvp = lsp->ls_fsvp) != NULL) {

			vatmp.va_mask = AT_ATIME|AT_MTIME;

			if (lsp->ls_flag & SPASS) {

				/* prevent behavior insert/remove */
				VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

				PV_NEXT(bdp, nbdp, vop_getattr);
				PVOP_GETATTR(nbdp, &vatmp, 0, credp, error);

				VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

			} else {
				VOP_GETATTR(fsvp, &vatmp, 0, credp, error);
			}

			if (!error) {
				if (vatmp.va_atime.tv_sec > lsp->ls_atime)
					va.va_atime = vatmp.va_atime;
				else {
					va.va_atime.tv_sec = lsp->ls_atime;
					va.va_atime.tv_nsec = 0;
				}

				if (vatmp.va_mtime.tv_sec > lsp->ls_mtime)
					va.va_mtime = vatmp.va_mtime;
				else {
					va.va_mtime.tv_sec = lsp->ls_mtime;
					va.va_mtime.tv_nsec = 0;
				}

				va.va_mask = AT_ATIME|AT_MTIME;

				if (lsp->ls_flag & SPASS) {

					/* prevent behavior insert/remove */
					VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

					PV_NEXT(bdp, nbdp, vop_setattr);
					PVOP_SETATTR(nbdp, &va, 0,
								credp, unused);

					VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

				} else {
					VOP_SETATTR(fsvp, &va, 0,
								credp, unused);
				}
			}

			if (lsp->ls_flag & SPASS) {

				/* prevent behavior insert/remove */
				VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

				PV_NEXT(bdp, nbdp, vop_fsync);
				PVOP_FSYNC(nbdp,
					   (ioflag & IO_DSYNC)
						? (FSYNC_WAIT | FSYNC_DATA)
						: FSYNC_WAIT,
					   credp, (off_t)0, (off_t)-1, unused); 

				VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

			} else {
				VOP_FSYNC(fsvp,
					  (ioflag & IO_DSYNC)
						? (FSYNC_WAIT | FSYNC_DATA)
						: FSYNC_WAIT,
					  credp, (off_t)0, (off_t)-1, unused); 
			}
		}
	}
}
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

	   
/*
 * VOP_RWLOCK has been done (if IO_ISLOCKED flag is set)
 */
/* ARGSUSED */
STATIC int
spec_read(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl)
{
		 lsnode_t	*lsp;
		 int		error;
#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
		 int		nbmv;
		 int		oresid;
	register unsigned	n;
	register dev_t		dev;
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */
		 vnode_t	*vp;
#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
		 struct bmapval	bmv[2];
		 struct buf	*bp;
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */


	SPEC_STATS(spec_read);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */
	ASSERT((lsp->ls_flag & SINACTIVE) == 0);

	vp  = BHV_TO_VNODE(bdp);

	ASSERT(vp->v_count > 0);

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))

	/*
	 * All Character specials will be shuttled off to the
	 * "common" side of the specfs world.
	 *
	 * This also catches all the "streams" devices.
	 */
	if (vp->v_type == VCHR) {
		error = VSPECFS_VOP_READ(lsp, &lsp->ls2cs_handle, lsp->ls_gen,
						vp, uiop, ioflag, credp, fl);
		if (! error) {
			if (WRITEALLOWED(vp, credp))
				spec_mark(lsp, SACC);
		}

		return error;
	}


	/*
	 * If we're here, it MUST be a VBLK device as all VCHR devices
	 * are handled by the "common" side code.
	 */
	ASSERT(vp->v_type == VBLK);

	if (! (ioflag & IO_ISLOCKED))
		spec_rwlock(bdp, VRWLOCK_READ);

	/*
	 * 1st check for null (0 bytes) transfers.
	 */
	if (uiop->uio_resid == 0) {
		error = 0;
		goto out;
	}

	/*
	 * Don't allow negative offsets into the block device
	 * code.  It causes problems.  Some devices, most likely
	 * /dev/mem, do actually use negative offsets.  This
	 * check is only up here so that we don't do the spec_mark()
	 * call on a write that is going to fail.
	 */
	if (uiop->uio_offset < 0) {
		error = EINVAL;
		goto out;
	}

	if (WRITEALLOWED(vp, credp))
		spec_mark(lsp, SACC);

	dev = lsp->ls_dev;

	/*
	 * Lock lsp to keep the common csnode/vnode from being killed.
	 */
	SPEC_LSP_LOCK(lsp);

	if (   (ioflag & IO_DIRECT) == 0
	    && VSPECFS_SUBR_GET_ISMOUNTED(lsp, &lsp->ls2cs_handle)) {

		ioflag |= IO_DIRECT;
	}

	if (ioflag & IO_DIRECT) {

		SPEC_LSP_UNLOCK(lsp);

		ASSERT(lsp->ls_bdevsw != NULL);

		error = biophysio(lsp->ls_bdevsw->d_strategy, 0, dev,
				  B_READ, OFFTOBB(uiop->uio_offset), uiop);
		goto out;
	}

	error = 0;
	oresid = uiop->uio_resid;
	
	if (ioflag & IO_RSYNC)
		spec_read_rsync(bdp, lsp, ioflag, dev, credp);
	
	do {
		nbmv = 2;

		/*
		 * Pass this off to the "common" snode side of the world.
		 */
		error = VSPECFS_VOP_BMAP(lsp, &lsp->ls2cs_handle,
					 uiop->uio_offset, uiop->uio_resid,
					 B_READ, credp, &bmv[0], &nbmv);

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

	SPEC_LSP_UNLOCK(lsp);

	if (oresid != uiop->uio_resid)		/* partial read */
		error = 0;

out:
	if (! (ioflag & IO_ISLOCKED))
		spec_rwunlock(bdp, VRWLOCK_READ);

	return error;

#else	/* ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

	/*
	 * On "CELL_IRIX" systems, shuttle all reads off to the
	 * "common" side of the specfs world.
	 */
	error = VSPECFS_VOP_READ(lsp, &lsp->ls2cs_handle, lsp->ls_gen,
						vp, uiop, ioflag, credp, fl);
	if (! error) {
		if (WRITEALLOWED(vp, credp))
			spec_mark(lsp, SACC);
	}

	return error;

#endif	/* ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */
}


/*
 * VOP_RWLOCK has been done (if IO_ISLOCKED flag is set)
 */
/* ARGSUSED */
STATIC int
spec_write(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl)
{
		 lsnode_t	*lsp;
		 int		error;
#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
		 int		nbmv;
		 int		oresid;
	register unsigned	n;
	register dev_t		dev;
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */
		 vnode_t	*vp;
#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
		 struct bmapval	bmv;
		 struct buf	*bp;
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */


	SPEC_STATS(spec_write);

	ASSERT(credp != NULL);			/* Catch an old problem? */

	lsp = BHV_TO_LSP(bdp);

						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */
	ASSERT((lsp->ls_flag & SINACTIVE) == 0);

	vp  = BHV_TO_VNODE(bdp);

	ASSERT(vp->v_count > 0);

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))

	/*
	 * All Character specials will be shuttled off to the
	 * "common" side of the specfs world.
	 *
	 * This also includes all the "streams" devices.
	 */
	if (vp->v_type == VCHR) {
		error = VSPECFS_VOP_WRITE(lsp, &lsp->ls2cs_handle, lsp->ls_gen,
						vp, uiop, ioflag, credp, fl);
		if (! error) {
			if (WRITEALLOWED(vp, credp))
				spec_mark(lsp, SACC|SUPD|SCHG);
		}

		return error;
	}


	/*
	 * If we're here, it MUST be a VBLK device as all VCHR devices
	 * are handled by the "common" side code.
	 */
	ASSERT(vp->v_type == VBLK);

	if (! (ioflag & IO_ISLOCKED))
		spec_rwlock(bdp, VRWLOCK_WRITE);

	/*
	 * 1st check for null (0 bytes) transfers.
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

	SPEC_LSP_LOCK(lsp);

	if (   (ioflag & IO_DIRECT) == 0
	    && VSPECFS_SUBR_GET_ISMOUNTED(lsp, &lsp->ls2cs_handle)) {

		ioflag |= IO_DIRECT;
	}

	dev = lsp->ls_dev;

	if (ioflag & IO_DIRECT) {

		SPEC_LSP_UNLOCK(lsp);

		ASSERT(lsp->ls_bdevsw != NULL);

		error = biophysio(lsp->ls_bdevsw->d_strategy, 0, dev,
				  B_WRITE, OFFTOBB(uiop->uio_offset), uiop);
		goto out;
	}

	oresid = uiop->uio_resid;
	do {
		nbmv = 1;

		/*
		 * Pass this off to the "common" snode side of the world.
		 */
		error = VSPECFS_VOP_BMAP(lsp, &lsp->ls2cs_handle,
					 uiop->uio_offset, uiop->uio_resid,
					 B_WRITE, credp, &bmv, &nbmv);
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

		/* XXXbe svr4 does spec_mark here */
	} while (error == 0 && uiop->uio_resid > 0 && n != 0);

	/*
	 * SPEC_LSP_LOCK/SPEC_LSP_UNLOCK are double-trippable,
	 * so order spec_mark()/SPEC_LSP_UNLOCK() to take advantage
	 * of faster double-tripping route.
	 */
	spec_mark(lsp, SUPD|SCHG);

	SPEC_LSP_UNLOCK(lsp);

	if (oresid != uiop->uio_resid)		/* partial write */
		error = 0;

out:
	if (! (ioflag & IO_ISLOCKED))
		spec_rwunlock(bdp, VRWLOCK_WRITE);

	return error;

#else	/* ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

	/*
	 * On "CELL_IRIX" systems, shuttle all writes off to the
	 * "common" side of the specfs world.
	 */
	error = VSPECFS_VOP_WRITE(lsp, &lsp->ls2cs_handle, lsp->ls_gen,
						vp, uiop, ioflag, credp, fl);
	if (! error) {
		if (WRITEALLOWED(vp, credp))
			spec_mark(lsp, SACC|SUPD|SCHG);
	}

	return error;

#endif	/* ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */
}


/* ARGSUSED */
STATIC int
spec_ioctl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		mode,
	struct cred	*credp,
	int		*rvalp,
	struct vopbd	*vbd)
{
	int		error;
	lsnode_t	*lsp;
	vnode_t		*vp;


	SPEC_STATS(spec_ioctl);

	vp  = BHV_TO_VNODE(bdp);
	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */
	ASSERT((lsp->ls_flag & SINACTIVE) == 0);

	ASSERT(vp->v_count > 0);

	if (vp->v_type != VCHR)
		return ENOTTY;

	/*
	 * Pass this off to the "common" snode side of the world.
	 */
	error = VSPECFS_VOP_IOCTL(lsp, &lsp->ls2cs_handle, lsp->ls_gen,
					cmd, arg, mode, credp, rvalp, vbd);

	return error;
}


/* ARGSUSED */
STATIC int
spec_setfl(
	bhv_desc_t	*bdp,
	int		oflags,
	int		nflags,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_setfl);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_setfl);
			PVOP_SETFL(nbdp, oflags, nflags, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_SETFL(fsvp, oflags, nflags, credp, error);
		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return fs_setfl(bdp, oflags, nflags, credp);
}


/* ARGSUSED */
STATIC int
spec_getattr(
		 bhv_desc_t	*bdp,
	register struct vattr	*vap,
		 int		flags,
		 struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*vp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_getattr);

	vp = BHV_TO_VNODE(bdp);

	ASSERT((flags & ATTR_COMM) == 0);


	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	SPEC_LSP_LOCK(lsp);

	error = 0;

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_getattr);
			PVOP_GETATTR(nbdp, vap, flags, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_GETATTR(fsvp, vap, flags, credp, error);
		}

	} else {
		/*
		 * No fs vnode behind this one.
		 * Fill in the fields from the lsnode.
		 *
		 * This code should be refined to return only the
		 * attributes asked for instead of all of them.
		 */
		vap->va_type    = vp->v_type;
		vap->va_mode    = 0;
		vap->va_uid     = vap->va_gid = 0;
		vap->va_fsid    = lsp->ls_fsid;
		vap->va_nodeid  = (long)lsp & 0xFFFF;	/* XXX -- must fix */
		vap->va_nlink   = 0;
		vap->va_size    = BBTOB(lsp->ls_size);
		vap->va_rdev    = lsp->ls_dev;
		vap->va_blksize = MAXBSIZE;
		vap->va_nblocks = btod(vap->va_size);

		vap->va_atime.tv_sec  = lsp->ls_atime;
		vap->va_atime.tv_nsec = 0;
		vap->va_ctime.tv_sec  = lsp->ls_ctime;
		vap->va_ctime.tv_nsec = 0;
		vap->va_mtime.tv_sec  = lsp->ls_mtime;
		vap->va_mtime.tv_nsec = 0;

		vap->va_vcode = 0;

		error = 0;
	}

	if (!error) {
		vap->va_atime.tv_sec  = lsp->ls_atime;
		vap->va_atime.tv_nsec = 0;

		vap->va_ctime.tv_sec  = lsp->ls_ctime;
		vap->va_ctime.tv_nsec = 0;

		vap->va_mtime.tv_sec  = lsp->ls_mtime;
		vap->va_mtime.tv_nsec = 0;

		vap->va_vcode     = 0;
		vap->va_xflags    = 0;
		vap->va_extsize   = 0;
		vap->va_nextents  = 0;
		vap->va_anextents = 0;
		vap->va_projid    = 0;
		vap->va_gencount  = 0;
	}


	SPEC_LSP_UNLOCK(lsp);

	return error;
}


STATIC int
spec_setattr(
		 bhv_desc_t	*bdp,
	register struct vattr	*vap,
		 int		flags,
		 struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;
	timespec_t	tv;


	SPEC_STATS(spec_setattr);

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) == NULL) {
		error = 0;	/* no fs vnode to update */
	} else {
		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_setattr);
			PVOP_SETATTR(nbdp, vap, flags, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_SETATTR(fsvp, vap, flags, credp, error);
		}
	}

	if (error == 0) {
		/*
		 * If times were changed, update lsnode.
		 */
		if (vap->va_mask & AT_ATIME)
			lsp->ls_atime = vap->va_atime.tv_sec;

		if (vap->va_mask & AT_MTIME) {
			lsp->ls_mtime = vap->va_mtime.tv_sec;

			nanotime_syscall(&tv);

			lsp->ls_ctime = tv.tv_sec;
		}
	}

	return error;
}


STATIC int
spec_access(
	bhv_desc_t	*bdp,
	int		mode,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_access);

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		ASSERT(fsvp->v_count > 0);

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_access);
			PVOP_ACCESS(nbdp, mode, credp, error);	

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_ACCESS(fsvp, mode, credp, error);	
		}

		return(error); 
	}

	return 0;
}


/* ARGSUSED */
STATIC int
spec_lookup(
	bhv_desc_t	*bdp,
	char		*nm,
	struct vnode	**vpp,
	struct pathname	*pnp,
	int		flags,
	struct vnode	*rdir,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_lookup);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_lookup);
			PVOP_LOOKUP(nbdp, nm, vpp, pnp, flags, rdir,
								credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_LOOKUP(fsvp, nm, vpp, pnp, flags, rdir,
								credp, error);
		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_create(
	bhv_desc_t	*bdp,
	char		*name,
	struct vattr	*vap,
	int		flags,
	int		mode,
	struct vnode	**vpp,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_create);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_create);
			PVOP_CREATE(nbdp, name, vap, flags, mode,
							vpp, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_CREATE(fsvp, name, vap, flags, mode,
							vpp, credp, error);

		}

		return error;
	}

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}
	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_remove(
	bhv_desc_t	*bdp,
	char		*nm,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_remove);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		vn_trace_entry(fsvp, "spec_remove", (inst_t *)__return_address);

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_remove);
			PVOP_REMOVE(nbdp, nm, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_REMOVE(fsvp, nm, credp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_link(
	bhv_desc_t	*bdp,
	struct vnode	*svp,
	char		*tnm,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_link);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_link);
			PVOP_LINK(nbdp, svp, tnm, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_LINK(fsvp, svp, tnm, credp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_rename(
	bhv_desc_t	*bdp,
	char		*snm,
	struct vnode	*tdvp,
	char		*tnm,
	struct pathname	*tpnp,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_rename);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_rename);
			PVOP_RENAME(nbdp, snm, tdvp, tnm, tpnp, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_RENAME(fsvp, snm, tdvp, tnm, tpnp, credp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_mkdir(
	bhv_desc_t	*bdp,
	char		*dirname,
	struct vattr	*vap,
	struct vnode	**vpp,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_mkdir);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_mkdir);
			PVOP_MKDIR(nbdp, dirname, vap, vpp, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_MKDIR(fsvp, dirname, vap, vpp, credp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_rmdir(
	bhv_desc_t	*bdp,
	char		*nm,
	struct vnode	*cdir,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_rmdir);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_rmdir);
			PVOP_RMDIR(nbdp, nm, cdir, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_RMDIR(fsvp, nm, cdir, credp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_readdir(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	struct cred	*credp,
	int		*eofp)

{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_readdir);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_readdir);
			PVOP_READDIR(nbdp, uiop, credp, eofp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_READDIR(fsvp, uiop, credp, eofp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_symlink(
	bhv_desc_t	*bdp,
	char		*linkname,
	struct vattr	*vap,
	char		*target,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_symlink);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_symlink);
			PVOP_SYMLINK(nbdp, linkname, vap, target, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_SYMLINK(fsvp, linkname, vap, target, credp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


STATIC int
spec_readlink(
	bhv_desc_t	*bdp,
	struct uio	*uiop,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_readlink);

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_readlink);
			PVOP_READLINK(nbdp, uiop, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_READLINK(fsvp, uiop, credp, error);
		}

		return (error);
	}

	return 0;
}



/*
 * In order to sync out the lsnode times without multi-client problems,
 * make sure the times written out are never earlier than the times
 * already set in the vnode.
 */
STATIC int
spec_fsync(
	bhv_desc_t	*bdp,
	int		flag,
	struct cred	*credp,
	off_t		start,
	off_t		stop)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;
	vnode_t		*vp;
	struct vattr	va, vatmp;


	SPEC_STATS(spec_fsync);

	vp  = BHV_TO_VNODE(bdp);
	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	error = 0;

	SPEC_LSP_LOCK(lsp);

	/*
	 * If times didn't change, don't flush anything.
	 */
	if ((lsp->ls_flag & (SACC|SUPD|SCHG)) == 0) {

		SPEC_LSP_UNLOCK(lsp);

		return 0;
	}

	lsp->ls_flag &= ~(SACC|SUPD|SCHG);

	SPEC_LSP_UNLOCK(lsp);

	/*
	 * We only need to call bflush on block devices
	 */
	if (vp->v_type == VBLK)
		bflush(lsp->ls_dev);
	
	/*
	 * If no fs vnode to update, don't flush anything.
	 */
	if ((fsvp = lsp->ls_fsvp) == NULL)
		return 0;

	vatmp.va_mask = AT_ATIME|AT_MTIME;

	if (lsp->ls_flag & SPASS) {

		/* prevent behavior insert/remove */
		VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

		PV_NEXT(bdp, nbdp, vop_getattr);
		PVOP_GETATTR(nbdp, &vatmp, 0, credp, error);

		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

	} else {
		VOP_GETATTR(fsvp, &vatmp, 0, credp, error);
	}

	if (!error) {
		if (vatmp.va_atime.tv_sec > lsp->ls_atime)
			va.va_atime = vatmp.va_atime;
		else {
			va.va_atime.tv_sec = lsp->ls_atime;
			va.va_atime.tv_nsec = 0;
		}

		if (vatmp.va_mtime.tv_sec > lsp->ls_mtime)
			va.va_mtime = vatmp.va_mtime;
		else {
			va.va_mtime.tv_sec = lsp->ls_mtime;
			va.va_mtime.tv_nsec = 0;
		}

		va.va_mask = AT_ATIME|AT_MTIME;

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_setattr);
			PVOP_SETATTR(nbdp, &va, 0, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_SETATTR(fsvp, &va, 0, credp, error);
		}
	}

	if (lsp->ls_flag & SPASS) {

		/* prevent behavior insert/remove */
		VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

		PV_NEXT(bdp, nbdp, vop_fsync);
		PVOP_FSYNC(nbdp, flag, credp, start, stop, error);

		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

	} else {
		VOP_FSYNC(fsvp, flag, credp, start, stop, error);
	}

	/*
	 * Ref. pv #614378.
	 * Most of the time the "underlying file system" is the hardware
	 * graph, which doesn't have any "fsync" code, and returns ENOSYS
	 * as well.
	 * For VBLK devices, we've flushed the caches, VCHR devices don't
	 * cache, and we've given the underlying file system a "shot" at it.
	 *
	 * Revert back to the 6.[234] behavior of always returning "0" status.
	 *
	 * return error;
	 */

	return 0;
}


STATIC int
spec_fid(
	bhv_desc_t	*bdp,
	struct fid	**fidpp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_fid);

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_fid);
			PVOP_FID(nbdp, fidpp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_FID(fsvp, fidpp, error);
		}

		return (error);
	} else {
		return EINVAL;
	}
}


STATIC int
spec_fid2(
	bhv_desc_t	*bdp,
	struct fid	*fidp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_fid2);

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_fid2);
			PVOP_FID2(nbdp, fidp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_FID2(fsvp, fidp, error);
		}

		return(error);
	} else {
		return EINVAL;
	}
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
spec_rwlock(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	lsnode_t	*lsp;


	SPEC_STATS(spec_rwlock);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_LSP_LOCK(lsp);
}


/* ARGSUSED */
STATIC void
spec_rwunlock(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock)
{
	lsnode_t	*lsp;


	SPEC_STATS(spec_rwunlock);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_LSP_UNLOCK(lsp);
}


/* ARGSUSED */
STATIC int
spec_seek(
	bhv_desc_t	*bdp,
	off_t		ooff,
	off_t		*noffp)
{
	SPEC_STATS(spec_seek);

	return 0;
}


/* ARGSUSED */
STATIC int
spec_cmp(
	bhv_desc_t	*bdp,
	vnode_t		*vp)
{
	int		equal;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_cmp);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_cmp);
			PVOP_CMP(nbdp, vp, equal);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_CMP(fsvp, vp, equal);
		}

		return equal;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return fs_cmp(bdp, vp);
}


/* ARGSUSED */
STATIC int
spec_frlock(
	bhv_desc_t	*bdp,
	int		cmd,
	struct flock	*bfp,
	int		flag,
	off_t		offset,
	vrwlock_t	vrwlock,
	struct cred	*credp)
{
	int		error;
	lsnode_t	*lsp;


	SPEC_STATS(spec_frlock);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if (vrwlock == VRWLOCK_NONE)
		SPEC_LSP_LOCK(lsp);

	error = fs_frlock(bdp, cmd, bfp, flag, offset, VRWLOCK_WRITE, credp);

	if (vrwlock == VRWLOCK_NONE)
		SPEC_LSP_UNLOCK(lsp);

	return error;
}


/* ARGSUSED */
STATIC int
spec_realvp(
	register bhv_desc_t	*bdp,
	register struct vnode	**vpp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;
	struct vnode	*rvp;


	SPEC_STATS(spec_realvp);

	lsp = BHV_TO_LSP(bdp);

	fsvp = lsp->ls_fsvp;

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	error = 0;

	if (fsvp != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_realvp);
			PVOP_REALVP(nbdp, &rvp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_REALVP(fsvp, &rvp, error);
		}

		if (!error)
			fsvp = rvp;
	}

	*vpp = fsvp;

	return error;
}


#if	((defined(CELL_IRIX) || defined(SPECFS_TEST)))

STATIC int
spec_poll(
	bhv_desc_t	*bdp,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead	**phpp,
	unsigned int	*genp)
{
	int		error;
	dev_t		dev;
	lsnode_t	*lsp;
	/* REFERENCED */
	vnode_t		*vp;

	SPEC_STATS(spec_poll_cell);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	vp = BHV_TO_VNODE(bdp);

	dev = lsp->ls_dev;

	ASSERT(dev == vp->v_rdev);

	/*
	 * Pass this off to the "common" snode side of the world.
	 */
	error = VSPECFS_VOP_POLL(lsp, &lsp->ls2cs_handle, lsp->ls_gen,
					events, anyyet, reventsp, phpp, genp);
	return error;
}

#else	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

STATIC int
spec_poll(
	bhv_desc_t	*bdp,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead	**phpp,
	unsigned int	*genp)
{
	int		error;
	lsnode_t	*lsp;
	/* REFERENCED */
	vnode_t		*vp;

#ifdef	MP
	SPEC_STATS(spec_poll_noncell);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	vp = BHV_TO_VNODE(bdp);

	if (lsp->ls_poll) {
		error = lsp->ls_poll(lsp->ls_polla0, events, anyyet,
					reventsp, (void **)phpp, genp);
	} else {
		ASSERT(vp->v_type == VCHR);

		ASSERT(lsp->ls_cdevsw != NULL);

		error = cdpoll(lsp->ls_cdevsw, lsp->ls_dev,
						events, anyyet, reventsp,
						phpp, genp);
	}
#else	/* ! MP */

	SPEC_STATS(spec_poll_noncell);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	vp = BHV_TO_VNODE(bdp);

	error = lsp->ls_poll(lsp->ls_polla0, events, anyyet, reventsp,
							(void **)phpp, genp);
#endif	/* ! MP */

	return error;
}

#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */


/* ARGSUSED */
STATIC int
spec_dump(
	bhv_desc_t	*bdp,
	caddr_t		addr,
	daddr_t		daddr,
	u_int		flag)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_dump);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_dump);
			PVOP_DUMP(nbdp, addr, daddr, flag, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_DUMP(fsvp, addr, daddr, flag, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_pathconf(
	bhv_desc_t	*bdp,
	int		cmd,
	long		*valp,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_pathconf);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_pathconf);
			PVOP_PATHCONF(nbdp, cmd, valp, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_PATHCONF(fsvp, cmd, valp, credp, error);
		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return fs_pathconf(bdp, cmd, valp, credp);
}


/* ARGSUSED */
STATIC int
spec_allocstore(
	bhv_desc_t	*bdp,
	off_t		off,
	size_t		len,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_allocstore);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_allocstore);
			PVOP_ALLOCSTORE(nbdp, off, len, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_ALLOCSTORE(fsvp, off, len, credp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return 0;
}


/* ARGSUSED */
STATIC int
spec_fcntl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flags,
	off_t		offset,
	cred_t		*credp,
	rval_t		*rvp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_fcntl);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_fcntl);
			PVOP_FCNTL(nbdp, cmd, arg, flags, offset,
							credp, rvp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_FCNTL(fsvp, cmd, arg, flags, offset,
							credp, rvp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC int
spec_bmap(
	register bhv_desc_t	*bdp,
		 off_t		offset,
		 ssize_t	count,
		 int		rw,
		 struct cred	*credp,
	register struct bmapval	*bmap,
		 int		*nbmap)
{
	int		error;
	lsnode_t	*lsp;


	ASSERT(BHV_TO_VNODE(bdp)->v_type == VBLK);

	SPEC_STATS(spec_bmap);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	/*
	 * Pass this off to the "common" snode side of the world.
	 */
	error = VSPECFS_VOP_BMAP(lsp, &lsp->ls2cs_handle,
					offset, count, rw, credp, bmap, nbmap);
	return error;
}


/* ARGSUSED */
STATIC void
spec_strategy(
	bhv_desc_t	*bdp,
	struct buf	*bp)
{
	lsnode_t	*lsp;


	SPEC_STATS(spec_strategy);

	lsp = BHV_TO_LSP(bdp);

	/*
	 * Pass this off to the "common" snode side of the world.
	 */
	VSPECFS_VOP_STRATEGY(lsp, &lsp->ls2cs_handle, bp);
}


/* ARGSUSED */
STATIC int
spec_map(
	bhv_desc_t	*bdp,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	u_int		flags,
	struct cred	*credp,
	vnode_t		**nvp)
{
	long		csgen;
	lsnode_t	*lsp;


	SPEC_STATS(spec_map);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	/*
	 * Handle this entirely on the "local" side.
	 */
	csgen = VSPECFS_SUBR_GET_GEN(lsp, &lsp->ls2cs_handle);

	if (lsp->ls_gen != csgen)		/* was it "vn_kill'd" ?   */
		return EIO;

	return 0;
}


/* ARGSUSED */
STATIC int
spec_addmap(
	bhv_desc_t	*bdp,
	vaddmap_t	op,
	vhandl_t	*vt,
	pgno_t		*pgno,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	struct cred	*credp)
{
	int		error;
	lsnode_t	*lsp;


	SPEC_STATS(spec_addmap);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	/*
	 * Pass this off to the "common" snode side of the world.
	 */
	error = VSPECFS_VOP_ADDMAP(lsp, &lsp->ls2cs_handle,
					op, vt, off, len, prot, credp, pgno);
	return error;
}


/* ARGSUSED */
STATIC int
spec_delmap(
	bhv_desc_t	*bdp,
	vhandl_t	*vt,
	size_t		len,
	struct cred	*credp)
{
	int		error;
	lsnode_t	*lsp;


	SPEC_STATS(spec_delmap);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	/*
	 * Pass this off to the "common" snode side of the world.
	 */
	error = VSPECFS_VOP_DELMAP(lsp, &lsp->ls2cs_handle, vt, len, credp);

	return error;
}


/*
 * Return the canonical name for a device.
 */
/* ARGSUSED */
STATIC int
spec_canonical_name_get(
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
	lsnode_t	*lsp;


	SPEC_STATS(spec_canonical_name_get);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	/*
	 * 1st find out what the actual name & length is.
	 *
	 * Note:  dev_to_name may return a different address
	 *	  than "devname" if it's an "UnKnowDevice".
	 */
	canonical_name = dev_to_name(lsp->ls_dev, devname, MAXDEVNAME);

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
STATIC int
spec_mac_pointer_get(
	bhv_desc_t	*bdp, 
	char		*name, 
	char		*value, 
	int		*valuelenp, 
	int		flags, 
	struct cred	*credp)
{
	
	SPEC_STATS(spec_mac_pointer_get);

	bcopy(&mac_low_high_lp, value, sizeof(mac_label *));

	*valuelenp = sizeof(mac_label *);

	return 0;
}


/* ARGSUSED */
STATIC int
spec_mac_label_get(
	bhv_desc_t	*bdp, 
	char		*name, 
	char		*value, 
	int		*valuelenp, 
	int		flags, 
	struct cred	*credp)
{
	int i;


	SPEC_STATS(spec_mac_label_get);

	i = (int) mac_size(mac_low_high_lp);

	bcopy(mac_low_high_lp, value, i);

	*valuelenp = i;

	return 0;
}


/*
 * The spec_attr_to_fn_table defines which function should be called for
 * a given attribute.  Only attributes handled locally by specfs are listed.
 */
typedef int (*attr_fn_t)
	(bhv_desc_t *, char *, char *, int *, int, struct cred *);

STATIC
struct spec_attr_to_fn {
	char		*saf_name;
	attr_fn_t	saf_func;
} spec_attr_to_fn_table[] = {
	{_DEVNAME_ATTR,		spec_canonical_name_get},
	{SGI_MAC_POINTER,	spec_mac_pointer_get},
	{SGI_MAC_FILE,		spec_mac_label_get},
};

int num_spec_attr_fn = sizeof(spec_attr_to_fn_table)
					/ sizeof(struct spec_attr_to_fn);

/*
 * Given an attribute name, return a local function that handles that
 * attribute name, or NULL if the attribute is not supposed to be
 * handled locally by specfs.
 */
STATIC attr_fn_t
spec_attr_func(char *name)
{
	int	i;

	SPEC_STATS(spec_attr_func);

	for (i = 0; i < num_spec_attr_fn; i++)
		if (!strcmp(spec_attr_to_fn_table[i].saf_name, name))
			return(spec_attr_to_fn_table[i].saf_func);

	return(NULL);
}


STATIC int
spec_attr_get(
	bhv_desc_t	*bdp,
	char		*name,
	char		*value,
	int		*valuelenp,
	int		flags,
	struct cred	*credp)
{
	int		error;
	int		fs_error;
	attr_fn_t	func;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_attr_get);

	fs_error = error = 0;

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	fsvp = lsp->ls_fsvp;

	/*
	 * If the "fsvp" vnode belongs to some other file system,
	 * try just passing the vop_attr_get() on down.
	 */
	if (fsvp != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_attr_get);
			PVOP_ATTR_GET(nbdp, name, value, valuelenp,
							flags, credp, fs_error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

			if (   (fs_error == 0)
			    || (fs_error != ENOATTR) ) {

				return fs_error;
			}
		} else {
			/*
			 * At this point we know that lsp->ls_flag indicates
			 * that we're not in SPASS (pass through) mode.
			 *
			 * We have a "hidden" file system vp, use it.
			 */
			VOP_ATTR_GET(fsvp, name, value, valuelenp,
							flags, credp, fs_error);

			if (   (fs_error == 0)
			    || (fs_error != ENOATTR) ) {

				return fs_error;
			}
		}
	}


	/*
	 * At this point we know that no other "underlying" file system
	 * has been able to provide the requested attributes.
	 *
	 * If the specified attribute is one that's supposed to be handled
	 * directly by specfs, then handle it.  Otherwise, can't help here.
	 */
	func = spec_attr_func(name);

	if (func != NULL) {

		error = func(bdp, name, value, valuelenp, flags, credp);

		return error;
	}
		
	/*
	 * If we're here, then we didn't find a way of handling it via
	 * spec_attr_func(), just return the error and information that
	 * we got on the 1st trip through VOP_ATTR_GET() on the fsvp.
	 */
	if (fsvp != NULL) {

		return fs_error;
	}

	/*
	 * Otherwise, it's all for nought.
	 */
	return EINVAL;
}


STATIC int
spec_attr_set(
	bhv_desc_t	*bdp,
	char		*name,
	char		*value,
	int		valuelen,
	int		flags,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_attr_set);

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_attr_set);
			PVOP_ATTR_SET(nbdp, name, value, valuelen,
							flags, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_ATTR_SET(fsvp, name, value, valuelen,
							flags, credp, error);
		}

		return error;
	}

	return EINVAL;
}


STATIC int
spec_attr_remove(
	bhv_desc_t	*bdp,
	char		*name,
	int		flags,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_attr_remove);

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_attr_remove);
			PVOP_ATTR_REMOVE(nbdp, name, flags, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_ATTR_REMOVE(fsvp, name, flags, credp, error);
		}

		return error;
	}

	return EINVAL;
}


STATIC int
spec_attr_list(
	bhv_desc_t		*bdp,
	char			*buffer,
	int			bufsize,
	int			flags,
	attrlist_cursor_kern_t	*cursor,
	struct cred		*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_attr_list);

	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_attr_list);
			PVOP_ATTR_LIST(nbdp, buffer, bufsize,
						flags, cursor, credp, error);	

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_ATTR_LIST(fsvp, buffer, bufsize,
						flags, cursor, credp, error);	
		}

		return error;
	}

	return EINVAL;
}


/* ARGSUSED */
STATIC int
spec_cover(
	bhv_desc_t	*bdp,
	struct mounta	*uap,
	char		*attrs,
	struct cred	*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_cover);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_cover);
			PVOP_COVER(nbdp, uap, attrs, credp, error);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_COVER(fsvp, uap, attrs, credp, error);

		}

		return error;
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
	return ENOSYS;
}


/* ARGSUSED */
STATIC void
spec_link_removed(
	bhv_desc_t	*bdp,
	vnode_t		*dvp,
	int		linkzero)
{
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_link_removed);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_link_removed);
			PVOP_LINK_REMOVED(nbdp, dvp, linkzero);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

			/*
			 * If the link count has gone to zero, set a flag
			 * so we'll know better what to do at spec_inactive
			 * time.
			 */
			if (linkzero) {

				SPEC_LSP_LOCK(lsp);

				lsp->ls_flag |= SLINKREMOVED;

				SPEC_LSP_UNLOCK(lsp);
			}

		} else {
			VOP_LINK_REMOVED(fsvp, dvp, linkzero);
		}
	}

	/*
	 * There's nobody "below" us, use the "default" action.
	 */
}


/* ARGSUSED */
STATIC void
spec_vnode_change(
	bhv_desc_t	*bdp,
	vchange_t	cmd,
	__psint_t	val)
{
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	vnode_t		*fsvp;


	SPEC_STATS(spec_vnode_change);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	/*
	 * If ls_fsvp is non-null, then there's another behavior associated
	 * with this vnode.  That other behavior is either stacked below
	 * us (if SPASS set) or is associated with another vnode.  For the
	 * latter case, we need to make sure that the vnode_change op is
	 * applied to both vnodes.
	 */
	if ((fsvp = lsp->ls_fsvp) != NULL) {

		if (lsp->ls_flag & SPASS) {

			/* prevent behavior insert/remove */
			VN_BHV_READ_LOCK(VN_BHV_HEAD(fsvp));

			PV_NEXT(bdp, nbdp, vop_vnode_change);
			PVOP_VNODE_CHANGE(nbdp, cmd, val);

			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(fsvp));

		} else {
			VOP_VNODE_CHANGE(fsvp, cmd, val);

			fs_vnode_change(bdp, cmd, val);
		}
	} else
		fs_vnode_change(bdp, cmd, val);
}



/* ARGSUSED */
STATIC int
spec_strgetmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	int		error;
	lsnode_t	*lsp;


	SPEC_STATS(spec_strgetmsg);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	/*
	 * Pass this off to the "common" snode side of the world.
	 */
	error = VSPECFS_VOP_STRGETMSG(lsp, &lsp->ls2cs_handle,
					mctl, mdata, prip, flagsp, fmode, rvp);

	return error;
}


/* ARGSUSED */
STATIC int
spec_strputmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	int		error;
	lsnode_t	*lsp;


	SPEC_STATS(spec_strputmsg);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	/*
	 * Pass this off to the "common" snode side of the world.
	 */
	error = VSPECFS_VOP_STRPUTMSG(lsp, &lsp->ls2cs_handle,
					mctl, mdata, pri, flag, fmode);

	return error;
}


STATIC void
spec_teardown(bhv_desc_t *bdp)
{
	int		already_locked;
	lsnode_t	*lsp;
	vnode_t		*vp;


	SPEC_STATS(spec_teardown);

	vp  = BHV_TO_VNODE(bdp);
	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	ASSERT(lsp->ls_flag & SPASS);		/* spec_teardown() only	*/
						/* used for pass-thru's	*/

	vn_trace_entry(vp, "spec_teardown", (inst_t *)__return_address);

	already_locked = SPEC_LSTAB_ISLOCKED();

	/*
	 * lock the hash table while we tear things down.
	 * spec_delete will understand..
	 */
	if (! already_locked)
		SPEC_LSTAB_LOCK();

	spec_delete(lsp);

	lsp->ls_fsvp = NULL;

	/*
	 * Now, drop our reference to the "common" side.
	 */
	VSPECFS_SUBR_TEARDOWN(lsp, &lsp->ls2cs_handle);

	/*
	 * Pull ourselves out of the behavior chain for this vnode.
	 */
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);

	spec_free(lsp);

	if (! already_locked)
		SPEC_LSTAB_UNLOCK();
}


STATIC void
spec_teardown_fsync(
	bhv_desc_t	*bdp,
	struct cred	*credp)
{
	int		already_locked;
	lsnode_t	*lsp;
	vnode_t		*vp;


	SPEC_STATS(spec_teardown_fsync);

	vp  = BHV_TO_VNODE(bdp);
	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	ASSERT((lsp->ls_flag & SPASS) == 0);

	vn_trace_entry(vp, "spec_teardown_fsync", (inst_t *)__return_address);

	already_locked = SPEC_LSTAB_ISLOCKED();

	/*
	 * lock the hash table while we tear things down.
	 * spec_delete will understand..
	 */
	if (! already_locked)
		SPEC_LSTAB_LOCK();

	/*
	 * Must delete 1st to prevent a race when spec_fsync() sleeps.
	 */
	spec_delete(lsp);

	/*
	 * In Non-PassThru mode, ls_fsvp points to the file
	 * system vnode, which is NOT the one that is going
	 * inactive, not yet anyway, so it's okay to "fsync"
	 * the ATIME/MTIME's.
	 */
	if (lsp->ls_fsvp) {
		(void) spec_fsync(bdp, FSYNC_NOWAIT, credp,
						(off_t)0, (off_t)-1);

		vn_trace_entry(lsp->ls_fsvp, "spec_teardown_fsync",
						(inst_t *)__return_address);
		VN_RELE(lsp->ls_fsvp);

		lsp->ls_fsvp = NULL;
	}

	/*
	 * Now, drop our reference to the "common" side.
	 */
	VSPECFS_SUBR_TEARDOWN(lsp, &lsp->ls2cs_handle);

	/*
	 * Pull ourselves out of the behavior chain for this
	 * vnode.
	 */
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);

	spec_free(lsp);

	if (! already_locked)
		SPEC_LSTAB_UNLOCK();
}


STATIC int
spec_inactive(
	bhv_desc_t	*bdp,
	struct cred	*credp)
{
	int		cache;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	/* REFERENCED */
	vnode_t		*vp;


	SPEC_STATS(spec_inactive);

	vp  = BHV_TO_VNODE(bdp);
	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	vn_trace_entry(vp, "spec_inactive", (inst_t *)__return_address);

	if ((lsp->ls_flag & SPASS) == 0) {

#ifdef	CELL_IRIX
		/*
		 * If we have our "attribute" layer under us,
		 * give it a chance to clean up.
		 */
		if (lsp->ls_flag & SATTR) {

			PV_NEXT(bdp, nbdp, vop_inactive);
			PVOP_INACTIVE(nbdp, credp, cache);

			ASSERT(cache == VN_INACTIVE_NOCACHE);
		}

		lsp->ls_flag &= ~SATTR;
#endif	/* CELL_IRIX */

		spec_teardown_fsync(bdp, credp);

		ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

		return VN_INACTIVE_NOCACHE;
	}

	ASSERT(lsp->ls_flag & SPASS);
	ASSERT(vp == lsp->ls_fsvp);

	/*
	 * We don't do a specific fsync in the PassThru mode.
	 * There's only one vnode, and it's in the midst of being
	 * inactivated.  Can't do stuff like setattr's against it
	 * (happens in fsync).
	 *
	 * Besides, the ATIME/MTIME stuff is already up to date.
	 */
	PV_NEXT(bdp, nbdp, vop_inactive);
	PVOP_INACTIVE(nbdp, credp, cache);

	if (cache == VN_INACTIVE_NOCACHE) {
		/*
		 * If the underlying file system(s) aren't caching,
		 * then we can't either.
		 */
		spec_teardown(bdp);

	} else if (lsp->ls_flag & SLINKREMOVED) {
		/*
		 * If the underlying file system has previously
		 * indicated that this vnode/inode has been removed,
		 * and is going onto the "free" list, then we need
		 * to "reclaim" our own resources.
		 */
		spec_teardown(bdp);
	} else {
		lsp->ls_flag |= SINACTIVE;	/* Mark "inactive"	*/
	}


	return cache;
}


STATIC int
spec_reclaim(
	bhv_desc_t	*bdp,
	int		flag)
{
	int		error;
	bhv_desc_t	*nbdp;
	/* REFERENCED */
	lsnode_t	*lsp;
	/* REFERENCED */
	vnode_t		*vp;


	SPEC_STATS(spec_reclaim);

	vp  = BHV_TO_VNODE(bdp);
	lsp = BHV_TO_LSP(bdp);

	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* Make sure things are   */
						/* what we think they are */

	ASSERT(lsp->ls_flag & SPASS);		/* spec_reclaim() only	*/
	ASSERT(vp == lsp->ls_fsvp);		/* used for pass-thru's	*/

	vn_trace_entry(vp, "spec_reclaim", (inst_t *)__return_address);

	PV_NEXT(bdp, nbdp, vop_reclaim);
	PVOP_RECLAIM(nbdp, flag, error);

	if (error)
		return error;

	spec_teardown(bdp);

	return 0;
}


vnodeops_t spec_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_SPECFS_DC),
	spec_open,
	spec_close,
	spec_read,
	spec_write,
	spec_ioctl,
	spec_setfl,
	spec_getattr,
	spec_setattr,
	spec_access,
	spec_lookup,
	spec_create,
	spec_remove,
	spec_link,
	spec_rename,
	spec_mkdir,
	spec_rmdir,
	spec_readdir,
	spec_symlink,
	spec_readlink,
	spec_fsync,
	spec_inactive,
	spec_fid,
	spec_fid2,
	spec_rwlock,
	spec_rwunlock,
	spec_seek,
	spec_cmp,
	spec_frlock,
	spec_realvp,
	spec_bmap,
	spec_strategy,
	spec_map,
	spec_addmap,
	spec_delmap,
	spec_poll,
	spec_dump,
	spec_pathconf,
	spec_allocstore,
	spec_fcntl,
	spec_reclaim,
	spec_attr_get,
	spec_attr_set,
	spec_attr_remove,
	spec_attr_list,
	spec_cover,
	spec_link_removed,
	spec_vnode_change,
	fs_tosspages,
	fs_flushinval_pages,
	fs_flush_pages,
	fs_invalfree_pages,
	fs_pages_sethole,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	spec_strgetmsg,
	spec_strputmsg,
};
