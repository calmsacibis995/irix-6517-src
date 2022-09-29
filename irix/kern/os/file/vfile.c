/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Id: vfile.c,v 1.34 1998/09/15 00:01:39 nrs Exp $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/prctl.h>
#include <sys/flock.h>
#include <sys/resource.h>
#include <sys/sema.h>
#include <sys/splock.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/uthread.h>
#include <sys/vnode.h>
#include <sys/vsocket.h>
#include <sys/fs_subr.h>
#include <ksys/vfile.h>
#include <limits.h>
#include <sys/atomic_ops.h>
#include <sys/psema_cntl.h>

static struct zone 	*file_zone;
/*
 * Count and list of inuse file structures.
 * Only updated for DEBUG configuration.
 * activefiles read by netstat.
 */
struct vfile 	*activefiles = NULL;
#ifdef	DEBUG
lock_t 		filelock;
#endif

#ifdef DEBUG
int vfile_outstanding;
#endif

static void 	vfile_cleanlock(vnode_t *);
static int	vfile_close_common(vfile_t *, int);
extern void	pfile_init(void);


void
vfile_init(void)
{

	vfile_cell_init();

	file_zone = kmem_zone_init(sizeof(struct vfile), "File table");
	activefiles = NULL;
#ifdef	DEBUG
	spinlock_init(&filelock, "filock");
#endif
	pfile_init();
}

/*
 * Allocate a user file descriptor and a file structure,
 * initializing the descriptor to point at the file structure.
 * 
 * The file descriptor/file structure are unavailable for use
 * until the caller calls vfile_ready.
 *
 * To undo the effects of this call there are two cases:
 * - vfile_ready hasn't been called: use vfile_alloc_undo
 * - vfile_ready has been called: use vfile_closed/vfile_close combo
 *   (needed to synchronize with other sprocs/threads accessing the fd)
 */
int
vfile_alloc(int flag, struct vfile **fpp, int *fdp)
{
	vfile_t		*fp;
	int 		fd, error;

	/*
	 * Allocate a fd and set the file pointer.
	 *
	 * Note that open file won't be accessible through
	 * the file descriptor table until vfile_ready() is called.
	 */
	fp = vfile_create(flag | FINPROGRESS);
	error = fdt_alloc(fp, &fd);
	if (error) {
		int _fdrop_s = VFLOCK(fp); 
		--(fp)->vf_count;
		VFUNLOCK(fp, _fdrop_s);
		vfile_destroy(fp);
		return error;
	}
	/*
	 * vfile_alloc always creates a V/P object pair - 
	 * no sharing is possible until after they have been created
	 * (and then only via rfork, rexec, migrate)
	 */
	pfile_create(fp);

	*fpp = fp;
	*fdp = fd;
	return 0;
}

/*
 * Mark a file structure as active and available for use.
 */
void
vfile_ready(vfile_t *fp, void *vp_or_vs)
{
	ASSERT(fp->vf_flag & FINPROGRESS);

	VF_SET_DATA(fp, vp_or_vs);
	fp->vf_flag &= ~FINPROGRESS;
}

/*
 * Undo effects of vfile_alloc() for a file that hasn't had vfile_ready() 
 * called.  Deals with both the file descriptor and the file structure.
 * 
 * Return the vnode that the file struct was referring to.
 */
void
vfile_alloc_undo(int fd, vfile_t *fp)
{
	/* REFERENCED */
	vfile_t		*temp_fp;

	/*
	 * Remove from fdt first.  Sanity check that the fp in
	 * the fdt is the same as the one passed in by the caller.
	 */
	temp_fp = fdt_alloc_undo(fd);
	ASSERT(fp == temp_fp);		
	ASSERT(fp->vf_flag & FINPROGRESS);
	ASSERT(fp->vf_count == 1);

	--fp->vf_count;
	VFILE_TEARDOWN(fp);	/* XXX could just call pfile_teardown */
	vfile_destroy(fp);
}

/*
 * Clean locks associated with a file being closed.
 */
static void
vfile_cleanlock(
        vnode_t		*vp)
{
	flid_t 		fl;

	get_current_flid(&fl);

	fs_cleanlock(vp, &fl, get_current_cred());
}

/*
 * Internal form of close.  Decrement reference count on file
 * structure.  Decrement reference count on the vnode following
 * removal of the referencing file structure.
 * This code assumes that no other process can hold a file that
 * has one reference, because file references are added only by
 * dup and fork.
 */
static int
vfile_close_common(
	vfile_t 	*fp,
	int		lastclose_info)
{
	extern int	grio_remove_reservation_with_fp(vfile_t *);
	vnode_t 	*vp;
	vsock_t		*vs;
        cred_t 		*credp;
	lastclose_t 	lastclose;
	int 		flag;
	int 		error = 0;
	int 		tmperr;
	
	if (VF_IS_VNODE(fp))
		vp = VF_TO_VNODE(fp);
	else
		vs = VF_TO_VSOCK(fp);

	flag = fp->vf_flag;
	credp = fp->vf_cred;

	/*
	 * First call to VFILE_CLOSE done by caller.
	 */
	lastclose = (lastclose_info & VFILE_ISLASTCLOSE) ? L_TRUE : L_FALSE;

	if (VF_IS_VNODE(fp)) {
		/*
		 * Do the VOP_CLOSE, unless told to skip it.
		 * First, cleanlocks if they exist, and this is not
		 * a reference release.
		 */
		if (vp->v_flag & VFRLOCKS)
			if ((lastclose_info & VFILE_REFREL) == 0)
				vfile_cleanlock(vp);

		if (!(lastclose_info & VFILE_SKIPCLOSE)) {
			VOP_CLOSE(vp, flag, lastclose, credp, error);
		} else {
			/*
			 * When VFILE_SKIPCLOSE is set this is a 
			 * last close for a DC, which may not actually
			 * be a last close for the distributed vfile.
			 * The DS will call VOP_CLOSE with lastclose
			 * equal to true if required when the existence
			 * token is returned.
			 */
			VOP_CLOSE(vp, flag, L_FALSE, credp, error);
		}

	} else if (lastclose == L_TRUE)
		VSOP_CLOSE(vs, lastclose, flag, error);


	if (lastclose == L_FALSE) {
		/*
		 * Must call vfile_close again.
		 */
		VFILE_CLOSE(fp, VFILE_SECONDCLOSE, lastclose_info);
		lastclose = (lastclose_info & VFILE_ISLASTCLOSE) ? 
			L_TRUE : L_FALSE;
		if (lastclose == L_TRUE) {
		       /*
		 	* Multiple closes were racing.  We didn't think we were
		 	* going to be the last close when we checked above, but
		 	* now we know we really are.  Do the lastclose
			* processing at this time.  This introduces an extra
			* call to VOP_CLOSE, but it is harmless.
		 	*/
			if (VF_IS_VNODE(fp)) {
				if (!(lastclose_info & VFILE_SKIPCLOSE)) {
					VOP_CLOSE(vp, flag, lastclose, 
						  credp, error);
				} else {
					/*
					 * When VFILE_SKIPCLOSE is set this is a 
					 * last close for a DC, which may not actually
					 * be a last close for the distributed vfile.
					 * The DS will call VOP_CLOSE with lastclose
					 * equal to true if required when the existence
					 * token is returned.
					 */
					VOP_CLOSE(vp, flag, L_FALSE, credp, error);
				}
			} else
				VSOP_CLOSE(vs, lastclose, flag, error);
		}

	}

	if (lastclose == L_FALSE)
		return error;

	/*
	 * If the cache flush & invalidate or flush flag is set, do so.  But 
	 * first, make sure we verify they have write access to the file if
	 * they are invalidating.
	 */
	if (VF_IS_VNODE(fp)) {
		if (fp->vf_flag & FLCINVAL) {
			VOP_ACCESS(vp, VWRITE, get_current_cred(), tmperr);
			if (!tmperr) {
				VOP_FSYNC(vp, FSYNC_WAIT | FSYNC_INVAL | FSYNC_DATA, get_current_cred(),
					(off_t)0, (off_t)-1, tmperr);
			}
		} else if (fp->vf_flag & FLCFLUSH) {
			VOP_FSYNC(vp, FSYNC_WAIT | FSYNC_DATA, get_current_cred(), (off_t)0,
				(off_t)-1, tmperr);
		}
	}
                                                       
	/*
	 * Was the last close - just teardown.
	 */
	if (VF_IS_VNODE(fp)) {
		if (vp->v_flag & VSEMAPHORE) {
			/*
			 * If the vnode represents a Posix named
			 * semaphore, drop the semaphore reference.
			 */
			(void) psema_indirect_close(vp);
		}
		VN_RELE(vp);
	}

	/* remove any grio reservations associated with this file */
	if (fp->vf_flag & FPRIORITY) {
#ifdef GRIO_DEBUG
		printf("Calling grio_remove_reservation_with_fp from vfile_close\n");
#endif /* GRIO_DEBUG */
		grio_remove_reservation_with_fp(fp);
	}


	VFILE_TEARDOWN(fp);
	vfile_destroy(fp);

	return error;
}

int
vfile_close(vfile_t *fp)
{
	int 		lastclose_info;
	int 		error;

	/*
	 * Sanity check.
	 */
	if (fp == NULL || fp->vf_count <= 0)
		return 0;

	/*
	 * This call to VFILE_CLOSE will decrement the vfile reference
	 * count iff the count is currently 1.  Otherewise, the decrement
	 * won't happen until the call to VFILE_CLOSE in vfile_close_common.  
	 * This prevents the race where a "lastclose" thread can get ahead 
	 * of a "not lastclose" thread and tear the vfile down.
	 *
	 * The reason for not just skipping the VFILE_CLOSE call here
	 * is that we need to determine if it's the last close.
	 */
	VFILE_CLOSE(fp, VFILE_FIRSTCLOSE, lastclose_info);

	/*
	 * Always complete the close - shared with vfile_ref_release
	 */
	error = vfile_close_common (fp, lastclose_info);
	return (error);
}

/*
 * vfile_ref_release is called when a reference to a vfile is being dropped.
 * If this is the last reference, all the
 * normal (last) close processing must be done.  If it is NOT the last
 * reference then only the reference count needs to be decremented - the
 * rest of close processing must NOT be performed (because the process still
 * exists - we are only removing an old reference).
 */
void
vfile_ref_release(vfile_t *fp)
{
	lastclose_t 	lastclose;
	int 		lastclose_info;
	/* REFERENCED */
	int		error;

	/*
	 * Sanity check.
	 */
	if (fp == NULL || fp->vf_count <= 0)
		return;

	/*
	 * Using VFILE_SECONDCLOSE here ... we need to drop a reference
	 * in all cases.  If this is not the last reference we are done.
	 * If it is the last reference then we need to do normal
	 * close processing.
	 */
	VFILE_CLOSE(fp, VFILE_SECONDCLOSE, lastclose_info);
	lastclose = (lastclose_info & VFILE_ISLASTCLOSE) ? L_TRUE : L_FALSE;

	/*
	 * Only complete the close if this is the last close
	 */
	if (lastclose == L_TRUE) {
		error = vfile_close_common (fp, lastclose_info|VFILE_REFREL);
		ASSERT(error == 0);
	}

	return;
}

/*
 * Allocate a file descriptor and assign it to the vnode "*vpp",
 * performing the usual open protocol upon it and returning the
 * file descriptor allocated.  It is the responsibility of the
 * caller to dispose of "*vpp" if any error occurs.
 */
int
vfile_assign(struct vnode **vpp, int mode, int *fdp)
{
	auto struct vfile *fp;
	register int error;
	auto int fd;
	struct vnode *vp;

	error = vfile_alloc(mode & FMASK, &fp, &fd);
	if (error)
		return error;
	vp = *vpp;
	VOP_OPEN(vp, vpp, mode, get_current_cred(), error);
	if (error) {
		vfile_alloc_undo(fd, fp);
		return error;
	}
	vfile_ready(fp, *vpp);
	*fdp = fd;
	return 0;
}

/*
 * create a vfile
 * flags passed in, fdt_alloc already done
 */
vfile_t *
vfile_create(
	int 	flag)
{
	cred_t	*cr;

	vfile_t	*vfp;

	vfp = kmem_zone_alloc(file_zone, KM_SLEEP);
	
        spinlock_init(&vfp->vf_lock, "vfile");
        vfp->vf_count = 1;
        vfp->vf_flag = flag;
	cr = get_current_cred();
	crhold(cr);
        vfp->vf_cred = cr;

        vfp->vf_msgcount = 0;
#ifdef CKPT
        vfp->vf_ckpt = -1;
#endif
#ifdef	DEBUG
	/* 
	 * Add to open file list.
	 *
	 * Be careful here if this list is ever maintained for non-debug.
	 * These vfile's may have the FINPROGRESS flag set.  It might
	 * make more sense to add to the list in vfile_ready.
	 */
	{
	int 	s;
	s = mp_mutex_spinlock(&filelock);
	if (activefiles)
		activefiles->vf_prev = vfp;
	vfp->vf_prev = NULL;
	vfp->vf_next = activefiles;
	activefiles = vfp;
	mp_mutex_spinunlock(&filelock, s);
	}
#endif

	SYSINFO.filecnt++;
	bhv_head_init(&vfp->vf_bh, "vfile");
	VF_SET_DATA(vfp, NULL);		/* sanity */

#ifdef DEBUG
	atomicAddInt(&vfile_outstanding, 1);
#endif
	return vfp;
}

void 
vfile_destroy(
	vfile_t *vfp)
{
	ASSERT(vfp->vf_count == 0);
	crfree(vfp->vf_cred);
#ifdef	DEBUG
	{
	int s;
	s = mp_mutex_spinlock(&filelock);
	if (vfp->vf_prev)
		vfp->vf_prev->vf_next = vfp->vf_next;
	else
		activefiles = vfp->vf_next;
	if (vfp->vf_next)
		vfp->vf_next->vf_prev = vfp->vf_prev;
	mp_mutex_spinunlock(&filelock, s);
	}
#endif
	SYSINFO.filecnt--;
	spinlock_destroy(&vfp->vf_lock);
	bhv_head_destroy(&vfp->vf_bh);
	kmem_zone_free(file_zone, vfp);
#ifdef DEBUG
	atomicAddInt(&vfile_outstanding, -1);
#endif
}
