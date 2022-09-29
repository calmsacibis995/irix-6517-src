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

#ident	"$Revision: 1.41 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/hwg.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/open.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/strmp.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ddi.h>
#include <sys/major.h>

#ifdef	CELL_IRIX
#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_spec_dc_stubs.h"
#include "I_spec_ds_stubs.h"
#include <ksys/cell/service.h>
#endif	/* CELL_IRIX */

#include <fs/specfs/spec_csnode.h>



/*
 * SPECFS Global Data for the "Common" side.
 */
#ifdef	CELL_IRIX
service_t	spec_service_id;		/* our SPECFS service id */
int		spec_cellid;
#endif	/* CELL_IRIX */

mutex_t spec_sync_lock;

bhv_desc_t	spec_bhv;

struct zone *spec_cs_zone;

csnode_t **spec_csnode_table;			/* The csnode hash table  */
mutex_t spec_csnode_tablock;			/* spec_csnode_table lock */


/* 
 * Macros to lock and to unlock csnode hash table, used during modifications
 * and during traversal of the hash list with possibility of sleep
 */
#define SPEC_CSTAB_LOCK()	mutex_lock(&spec_csnode_tablock, PINOD)
#define SPEC_CSTAB_UNLOCK()	mutex_unlock(&spec_csnode_tablock)
#define SPEC_CSTAB_ISLOCKED()	mutex_mine(&spec_csnode_tablock)


/* ARGSUSED */
int
spec_init(struct vfssw *vswp, int fstype)
{
	extern struct vfsops	spec_vfsops;
	extern vnodeops_t	spec_vnodeops;

	extern void		spec_at_init(void);
	extern void		spec_ls_init(void);

	dev_t dev;


	/*
	 * First initialize the "vfs" side of the specfs complex.
	 */
	spec_fstype = fstype;

	/*
	 * Associate the specfs VFS ops vector with this fstype.
	 */
	vswp->vsw_vfsops   = &spec_vfsops;
	vswp->vsw_vnodeops = &spec_vnodeops;

#ifdef	CELL_IRIX
						/* Make our service id */
	SERVICE_MAKE(spec_service_id, cellid(), SVC_SPECFS);

# ifdef	SPECFS_STATS_DEBUG
	spec_cellid = cellid();
# endif	/* SPECFS_STATS_DEBUG */

#endif	/* CELL_IRIX */

	if ((dev = getudev()) == -1) {
		cmn_err(CE_WARN, "specinit: can't get unique device number");
		dev = 0;
	}

	spec_dev = makedevice(dev, 0);

	if ((spec_vfsp = vfs_allocate_dummy(fstype, spec_dev)) == NULL)
		return -1;

	vfs_insertbhv(spec_vfsp, &spec_bhv, &spec_vfsops, NULL);

	mutex_init(&spec_sync_lock, MUTEX_DEFAULT, "specsync");

	vfs_simpleadd(spec_vfsp);

	/*
	 * Now initialize the "vop" side of the specfs complex.
	 */
	spec_cs_zone = kmem_zone_init(sizeof(struct csnode), "specFS CSnodes");

	spec_csnode_table =
			kern_calloc(1, DCSPECTBSIZE * sizeof(struct csnode *));

	mutex_init(&spec_csnode_tablock, MUTEX_DEFAULT, "cspectb");

	spec_at_init();			/* Init the "attribute" side	*/
	spec_ls_init();			/* Init the "local" side	*/

#ifdef	CELL_IRIX
	mesg_handler_register(spec_dc_msg_dispatcher, DCSPECFS_SUBSYSID);
	mesg_handler_register(spec_ds_msg_dispatcher, DSSPECFS_SUBSYSID);
#endif	/* CELL_IRIX */
	return 0;
}


void
spec_cs_lock(csnode_t *csp)
{
	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	SPEC_STATS(spec_cs_lock);

	ASSERT(csp->cs_locktrips >= 0);

	if (mutex_mine(&csp->cs_lock))
		csp->cs_locktrips++;
	else
		mutex_lock(&csp->cs_lock, 0);
}


void
spec_cs_unlock(csnode_t *csp)
{
	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	SPEC_STATS(spec_cs_unlock);

	ASSERT(mutex_mine(&csp->cs_lock));
	ASSERT(csp->cs_locktrips >= 0);

	if (csp->cs_locktrips > 0)
		csp->cs_locktrips--;
	else
		mutex_unlock(&csp->cs_lock);
}


/*
 * Create a new, locked csnode and its vnode.
 *
 * Note! We're called under SPEC_CSTAB_LOCK, and require
 *	 that the caller already got us a skeleton vnode
 *	 to fill in outside of SPEC_CSTAB_LOCK to prevent
 *	 potential deadlocks that can occur when vn_alloc()
 *	 calls vn_reclaim() against a local-side vnode.
 */
STATIC csnode_t *
spec_cs_makesnode(dev_t dev, vnode_t *nvp, csnode_t *ncsp)
{
	SPEC_STATS(spec_cs_makesnode);

	ASSERT(ncsp);			/* Make sure the caller	*/
	ASSERT(nvp);			/* gave us resources.	*/

	init_mutex(&ncsp->cs_lock, MUTEX_DEFAULT, "spec_csnode", dev);
	mutex_lock(&ncsp->cs_lock, 0);

	ASSERT(ncsp->cs_locktrips == 0);

	/*
	 * Init'ing the sema is the same as calling spec_cs_lock()
	 */
	SPEC_STATS(spec_cs_lock);

	bhv_desc_init(CSP_TO_BHV(ncsp), ncsp, nvp, &spec_cs_vnodeops);

	vn_bhv_insert_initial(VN_BHV_HEAD(nvp), CSP_TO_BHV(ncsp));

	VN_FLAGSET(nvp, VINACTIVE_TEARDOWN);

	/*
	 * Initialize the close vs clone synchronizers.
	 */
	ncsp->cs_close_flag = 0;

	sv_init(&ncsp->cs_close_sync, SV_DEFAULT, "specclose");

	if (dev_is_vertex(dev))
		hwgraph_vertex_ref(dev_to_vhdl(dev));

	/*
	 * Fill in the "devsw" address now.
	 */
	if (nvp->v_type == VBLK) {
		ncsp->cs_bdevsw = get_bdevsw(dev);
	} else {
		ncsp->cs_cdevsw = get_cdevsw(dev);

		if (ncsp->cs_cdevsw && ncsp->cs_cdevsw->d_str)
			ncsp->cs_flag |= SSTREAM;
	}

	return ncsp;
}


void
spec_cs_free(csnode_t *csp)
{
	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */

	SPEC_STATS(spec_cs_free);

#ifdef	SPECFS_FREE_DEBUG
	/*
	 * Make a scan of ALL of the hash chains to verify that
	 * this "csp" is really ready to "free".
	 */
	{
		int	i;
		int	already_locked;
		csnode_t *scsp;


		already_locked = SPEC_CSTAB_ISLOCKED();

		/*
		 * lock the hash table during modification
		 */
		if (! already_locked)
			SPEC_CSTAB_LOCK();

		for (i = 0; i < DCSPECTBSIZE; i++) {

			if ((scsp = spec_csnode_table[i]) == NULL)
				continue;

			for (; scsp != NULL; scsp = scsp->cs_next) {

				if (csp == scsp) {
					panic(
			    "spec_cs_free: csp 0x%x found on hash chain %d!",
									csp, i);
				}
			}
		}

		if (! already_locked)
			SPEC_CSTAB_UNLOCK();
	}
#endif	/* SPECFS_FREE_DEBUG */

	if (dev_is_vertex(csp->cs_dev))
		hwgraph_vertex_unref(dev_to_vhdl(csp->cs_dev));

	mutex_destroy(&csp->cs_lock);

	sv_destroy(&csp->cs_close_sync);

	kmem_zone_free(spec_cs_zone, csp);
}


/*
 * Find-or-Create a common csnode for the given dev.
 *
 * The csnode is the one which is "common" to all the aliased
 * lsnodes that denote the same device.
 *
 * NB: asp is passed in locked.
 *
 * "spec_cs_attach" is called through the spec_ls2cs_ops vector.
 */
void
spec_cs_attach(
	dev_t		dev,
	vtype_t		type,
	spec_handle_t	*handle,
	spec_handle_t	*cvp_handle,
	long		*gen,
	daddr_t		*size,
	struct stdata	**stream)
{
	csnode_t	*csp;
#ifdef	CELL_IRIX
	service_t	svc;
#endif	/* CELL_IRIX */
	vnode_t		*cvp;


	SPEC_STATS(spec_cs_attach);

	csp = spec_cs_get(dev, type);

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	cvp = CSP_TO_VP(csp);

	ASSERT(cvp);

#ifdef	CELL_IRIX
	SERVICE_MAKE(svc, cellid(), SVC_SPECFS);

	SPEC_HANDLE_MAKE(*cvp_handle, svc, cvp, cvp->v_rdev, cvp->v_type);
	SPEC_HANDLE_MAKE(    *handle, svc, csp, cvp->v_rdev, cvp->v_type);
#else	/* ! CELL_IRIX */
	handle->objid     = (void *)csp;
	cvp_handle->objid = (void *)cvp;
#endif	/* ! CELL_IRIX */

	*gen = csp->cs_gen;

	*stream = cvp->v_stream;

	if (type == VBLK)
		*size = csp->cs_size;

	SPEC_CSP_UNLOCK(csp);
}


/*
 * Csnode lookup stuff.
 *
 * These routines maintain a table of csnodes hashed by dev so
 * that the csnode for an dev can be found if it already exists.
 */

/*
 * Remove an csnode from the hash table.
 */
void
spec_cs_delete(csnode_t *csp)
{
	int		already_locked;
	int		found_it = 0;
	u_int		hash;
	csnode_t	*st;
	csnode_t	*stprev = NULL;


	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	SPEC_STATS(spec_cs_delete);

	already_locked = SPEC_CSTAB_ISLOCKED();

	/*
	 * lock hash table during modification
	 */
	if (! already_locked)
		SPEC_CSTAB_LOCK();

	hash = DCSPECTBHASH(csp->cs_dev);

	st = spec_csnode_table[hash];

	while (st != NULL) {
		if (st == csp) {
			found_it = 1;

			if (stprev == NULL)
				spec_csnode_table[hash] = st->cs_next;
			else
				stprev->cs_next = st->cs_next;
			break;
		}

		stprev = st;
		st = st->cs_next;
	}

	if (! found_it)
		panic("spec_cs_delete: Missed csp/0x%x on chain %d", csp, hash);

	if (! already_locked)
		SPEC_CSTAB_UNLOCK();
}


/*
 * Lookup a csnode by <dev, type>.
 *
 * If no csnode exists, create one and hash it.
 */
csnode_t *
spec_cs_get(dev_t dev, vtype_t type)
{
	int		backoff = 1;
	u_int		hash;
	csnode_t	*csp;
	csnode_t	*spare_csp;
	vnode_t		*csvp;
	vmap_t		vmap;
	vnode_t		*spare_vp;


	SPEC_STATS(spec_cs_get);

	spare_vp  = NULL;		/* Nobody	*/
	spare_csp = NULL;		/*	on deck	*/

	hash = DCSPECTBHASH(dev);

loop:
	SPEC_CSTAB_LOCK();

	csp = spec_csnode_table[hash];

	while (csp != NULL) {
		csvp = CSP_TO_VP(csp);

		if (   csp->cs_dev  == dev
		    && csvp->v_type == type) {

			/*
			 * Found it in the hash chain, now go "vn_get" it.
			 */
			VMAP(csvp, vmap);

			SPEC_CSTAB_UNLOCK();

			if ( ! (csvp = vn_get(csvp, &vmap, 0))) {

				/*
				 * We may be blocking the thread that
				 * needs to make progress in order to
				 * get us the proper setup, delay a bit.
				 */
				delay(backoff);
				backoff *= 2;

				goto loop;
			}

			ASSERT(CSP_TO_VP(csp) == csvp);
			
			/*
			 * If this snode is in the process of being closed
			 * by another thread, we need to wait until that's
			 * done before we let somebody else look at it.
			 */
			SPEC_CSP_LOCK(csp);

			while (csp->cs_close_flag & SCLOSING) {

				csp->cs_close_flag |= SWAITING;

				sv_wait(&csp->cs_close_sync, PZERO,
							&csp->cs_lock, 0);
				SPEC_CSP_LOCK(csp);
			}

			/*
			 * Free up unused resources and return.
			 */
			if (spare_csp) {
				kmem_zone_free(spec_cs_zone, spare_csp);

				spare_csp = NULL;
			}

			if (spare_vp) {
				vn_free(spare_vp);

				spare_vp = NULL;
			}

			return csp;
		}

		csp = csp->cs_next;
	}

	/*
	 * To avoid (witnessed) deadlocks between spec_cs_makesnode() and
	 * spec_delete() when called out of spec_reclaim(); due to vn_alloc()
	 * possibly attempting to reclaim a vnode from the "local" side, we
	 * need to obtain skeleton resources to be used by spec_cs_makesnode()
	 * without the protection of SPEC_CSTAB_LOCK.  So we move the obtaining
	 * of resources out of spec_cs_makesnode(), and make it the caller's
	 * responsibility to obtain the proper resources.
	 *
	 * This exposes us to a race condition where some other thread could
	 * insert dev/type while we're off waiting on vn_alloc(), so we'll
	 * need to re-chase the hash chain after we get back from allocating.
	 */
	if (spare_csp == NULL || spare_vp == NULL) {

		SPEC_CSTAB_UNLOCK();

		if (spare_csp == NULL)
			spare_csp = kmem_zone_zalloc(spec_cs_zone, KM_SLEEP);

		if (spare_vp == NULL)
			spare_vp  = vn_alloc(spec_vfsp, type, dev);

		ASSERT(spare_csp != NULL);
		ASSERT(spare_vp  != NULL);

		/*
		 * We don't need to "delay", as we're not waiting on
		 * something another thread of control was setting
		 * up for us, we just had to "drop locks" and get
		 * some potentially needed resources.
		 */
		goto loop;		/* Try, try, again	*/
	}

	/*
	 * Okay, we didn't find it, let go of SPEC_CSTAB_LOCK, allocated
	 * skeleton resources, re-obtained SPEC_CSTAB_LOCK, tried to find
	 * it again, and didn't, so now we're all set to fill it in and
	 * insert the new dev/type into the hash chains.
	 *
	 * Makesnode creates csp locked.
	 */
	csp = spec_cs_makesnode(dev, spare_vp, spare_csp);

	csp->cs_flag  |= SCOMMON;

	csp->cs_dev    = dev;
	csp->cs_fsid   = spec_dev;
	csp->cs_size   = UNKNOWN_SIZE;

	/*
	 * Insert the new csnode into the hash chain.
	 */
	csp->cs_next		= spec_csnode_table[hash];
	spec_csnode_table[hash] = csp;

	SPEC_CSTAB_UNLOCK();

	return csp;
}


#ifdef	JTK_DEBUG
/*
 * Given a specfs vnode, let the caller know if it's a "common" one.
 */
int
spec_vp_is_common(vnode_t *vp)
{
	bhv_desc_t	*bdp;
	/* REFERENCED */
	csnode_t	*csp;

	SPEC_STATS(spec_vp_is_common);

	if ((vp->v_type != VBLK) && (vp->v_type != VCHR)) {
#ifdef	JTK_DEBUG
printf("spec_vp_is_common: vp/0x%x not VBLK or VCHR!\n", vp);
#endif	/* JTK_DEBUG */
		return 0;
	}

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &spec_cs_vnodeops);

	if (bdp == NULL) {
#ifdef	JTK_DEBUG
printf("spec_vp_is_common: vp/0x%x not spec_cs!\n", vp);
#endif	/* JTK_DEBUG */
		return 0;
	}

	csp = BHV_TO_CSP(bdp);

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	return 1;
}
#endif	/* JTK_DEBUG */


int
spec_cs_device_hold(csnode_t *csp)
{
	vnode_t	*vp;


	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */

	SPEC_STATS(spec_cs_device_hold);

	vp = CSP_TO_VP(csp);

	if (vp->v_type == VCHR) {
		ASSERT(csp->cs_cdevsw != NULL);

		return cdhold(csp->cs_cdevsw);
	} else {
		ASSERT(csp->cs_bdevsw != NULL);

		return bdhold(csp->cs_bdevsw);
	}
}


void
spec_cs_device_rele(csnode_t *csp)
{
	vnode_t	*vp;


	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */

	SPEC_STATS(spec_cs_device_rele);

	vp = CSP_TO_VP(csp);

	if (vp->v_type == VCHR) {
		ASSERT(csp->cs_cdevsw != NULL);

		cdrele(csp->cs_cdevsw);
	} else {
		ASSERT(csp->cs_bdevsw != NULL);

		bdrele(csp->cs_bdevsw);
	}
}


/*
 * Called with "csp" locked! & returns with it re-locked
 * after calling the driver close routine.
 */
int
spec_cs_device_close(csnode_t *csp, int flag, struct cred *credp)
{
	int		error;
	dev_t		dev;
	enum vtype	type;
	vnode_t		*vp;


	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */

	SPEC_STATS(spec_cs_device_close);

	ASSERT(mutex_mine(&csp->cs_lock));

	vp   = CSP_TO_VP(csp);
	dev  = csp->cs_dev;
	type = vp->v_type;

	/*
	 * We need to "unlock" in order to protect against
	 * possible deadlocks (and other aberrant behavior)
	 * whilst off in the device-driver's close code.
	 * Grab a reference on the vnodes to make sure
	 * nothing goes away during this time.
	 */
	VN_HOLD(vp);

	csp->cs_close_flag |= SCLOSING;		/* Let spec_cs_get &	  */
						/* spec_cs_open_hndl know */
	SPEC_CSP_UNLOCK(csp);

	switch (type) {

	case VCHR:
		if (csp->cs_flag & SSTREAM) {

			error = strclose(vp, flag, credp);

			/*
			 * NOTE! v_stream was cleared in strclose.
			 */
		} else {
			ASSERT(csp->cs_cdevsw != NULL);

			error = cdclose(csp->cs_cdevsw, dev, flag,
							OTYP_CHR, credp);
		}

		break;

	case VBLK:
		/*
		 * On last close of a block device we must invalidate any
		 * in-core blocks so that we can, for example, change
		 * floppy disks.
		 */
		bflush(dev);
		binval(dev);

		ASSERT(csp->cs_bdevsw != NULL);

		error = bdclose(csp->cs_bdevsw, dev, flag, OTYP_BLK, credp);

		break;
	}

	SPEC_CSP_LOCK(csp);

	csp->cs_close_flag &= ~SCLOSING;

	if (csp->cs_close_flag & SWAITING) {

		csp->cs_close_flag &= ~SWAITING;

		/*
		 * Wakeup any waiting-on-close processes.
		 */
		sv_broadcast(&csp->cs_close_sync);
	}

	VN_RELE(vp);

	return error;
}


/*
 * "spec_cs_ismounted" is a "common" subroutine that returns a value
 * showing whether SMOUNTED is in "cs_flag" in the "common" snode.
 *
 * called through the spec_ls2cs_ops vector.
 */
int
spec_cs_ismounted(spec_handle_t *handle)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_ismounted);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	return (csp->cs_flag & SMOUNTED);
}


/*
 * "spec_cs_get_gen" is a "common" subroutine that returns the
 * current "cs_gen" value from the "common" snode.
 *
 * called through the spec_ls2cs_ops vector.
 */
int
spec_cs_get_gen(spec_handle_t *handle)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_get_gen);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	return csp->cs_gen;
}


/*
 * "spec_cs_cmp_gen" is a "common" subroutine that compares the
 * current "ls_gen" & "cs_gen" values.
 *
 * called through the spec_ls2cs_ops vector.
 */
int
spec_cs_cmp_gen(spec_handle_t *handle, long asp_gen)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_cmp_gen);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp != NULL);			/* Make sure things are   */
	ASSERT(csp->cs_flag & SCOMMON);		/* what we think they are */

	return (asp_gen == csp->cs_gen);
}


/*
 * "spec_cs_get_opencnt" is a "common" subroutine that returns the
 * current "cs_opencnt" value from the "common" snode.
 *
 * called through the spec_ls2cs_ops vector.
 */
int
spec_cs_get_opencnt(spec_handle_t *handle)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_get_opencnt);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	return csp->cs_opencnt;
}


/*
 * "spec_cs_get_size" is a "common" subroutine that returns the
 * current "cs_size" value from the "common" snode.
 *
 * called through the spec_ls2cs_ops vector.
 */
daddr_t
spec_cs_get_size(spec_handle_t *handle)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_get_size);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	return csp->cs_size;
}


/*
 * "spec_cs_mountedflag" is the "common" extension to "spec_mountedflag"
 *
 * called through the spec_ls2cs_ops vector.
 */
void
spec_cs_mountedflag(spec_handle_t *handle, int set)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_mountedflag);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */
	SPEC_CSP_LOCK(csp);

	if (set)
		csp->cs_flag |= SMOUNTED;
	else
		csp->cs_flag &= ~SMOUNTED;

	SPEC_CSP_UNLOCK(csp);
}


/*
 * Clone a streams device.
 *
 * Code moved to here to prevent the streams complex from having
 * to have detailed knowledge of specfs internals...  sigh...
 */
void
spec_cs_clone(
	spec_handle_t	*handle,
	struct stdata	*stp,
	spec_handle_t	*ovp_handle,
	int		flag,
	struct stdata	**stream)
{
	int		s2;
	csnode_t	*csp;
	vnode_t		*cvp;
	vnode_t		*oldvp;


	SPEC_STATS(spec_cs_clone);

	csp = HANDLE_TO_CSP(handle);

	cvp = CSP_TO_VP(csp);

	ASSERT(cvp != NULL);

	SPEC_CSP_LOCK(csp);

	csp->cs_opencnt++;		/* Implicit "open"	*/

	csp->cs_flag |= SWANTCLOSE;

	SPEC_CSP_UNLOCK(csp);

	if (stp) {
		LOCK_STRHEAD_MONP(s2);

		*stream = cvp->v_stream = stp;

		stp->sd_vnode = cvp;

		UNLOCK_STRHEAD_MONP(s2);

		oldvp = (vnode_t *)SPEC_HANDLE_TO_OBJID(*ovp_handle);

		/*
		 * Call back into streams/streamio.c to actually finish
		 * the original stropen() sequence.
		 */
		stropen_clone_finish(oldvp, stp, flag);

	} else {
		*stream = cvp->v_stream = NULL;
	}

	/*
	 * During the creation of a clone, two references were taken on the
	 * common vnode, the 1st during the open sequence, where the local
	 * side called the common side with a "clone master", and the common
	 * side obtained, and referenced, a "clone slave", returning the
	 * "newdev".  Later, as the local side matched the cloning sequence,
	 * it called spec_make_clone, which created a local "clone slave"
	 * using make_specvp_locked, which called spec_attach, who called
	 * spec_cs_attach to cement the link between the local and common
	 * "clone slaves", spec_cs_attach winds up generating a 2nd reference
	 * when it uses spec_cs_get to "find" the common "clone slave"
	 * created earlier.
	 * All this to explain a potentially confusing, yet important VN_RELE.
	 */
	VN_RELE(cvp);
}


/*
 * Mark the accessed, updated, or changed times in a csnode
 * with the current time.
 */
void
spec_cs_mark(csnode_t *csp, int flag)
{
	int		lock;
	timespec_t	tv;

	ASSERT(csp->cs_flag & SCOMMON);		/* Make sure things are   */
						/* what we think they are */

	SPEC_STATS(spec_cs_mark);

	lock = 0;

	nanotime_syscall(&tv);

	if ((flag & csp->cs_flag) == 0) {
		SPEC_CSP_LOCK(csp);

		lock++;

		csp->cs_flag |= flag;
	}

	if (flag & SACC)
		csp->cs_atime = tv.tv_sec;

	if (flag & SUPD)
		csp->cs_mtime = tv.tv_sec;

	if (flag & SCHG)
		csp->cs_ctime = tv.tv_sec;

	if (lock)
		SPEC_CSP_UNLOCK(csp);
}


void
spec_cs_teardown_subr(spec_handle_t *handle)
{
	csnode_t	*csp;


	SPEC_STATS(spec_cs_teardown_subr);

	csp = HANDLE_TO_CSP(handle);

	ASSERT(csp);

	ASSERT(csp->cs_flag & SCOMMON);	/* Make sure things are   */
					/* what we think they are */

	vn_trace_entry(CSP_TO_VP(csp), "spec_cs_teardown_subr",
						(inst_t *)__return_address);
	VN_RELE(CSP_TO_VP(csp));
}



struct __vhandl_s;

int spec_cs_bmap_subr(spec_handle_t *, off_t, ssize_t, int, struct cred *,
					struct bmapval *, int *);

int spec_cs_open_hndl(spec_handle_t *, int, dev_t *, daddr_t *,
					struct stdata **, mode_t,
					struct cred *);

int spec_cs_read_hndl(spec_handle_t *, int, vnode_t *, struct uio *, int,
					struct cred *, struct flid *);

int spec_cs_write_hndl(spec_handle_t *, int, vnode_t *, struct uio *, int,
					struct cred *, struct flid *);

int spec_cs_ioctl_hndl(spec_handle_t *, int, int, void *, int ,
					struct cred *, int *, struct vopbd *);

int spec_cs_strgetmsg_hndl(spec_handle_t *, struct strbuf *, struct strbuf *,
					unsigned char *, int *, int,
					union rval *);

int spec_cs_strputmsg_hndl(spec_handle_t *, struct strbuf *, struct strbuf *,
					unsigned char, int, int);

int spec_cs_poll_hndl(spec_handle_t *, long, short, int, short *,
					struct pollhead **, unsigned int *);

void spec_cs_strategy_subr(spec_handle_t *, struct buf *);

int spec_cs_addmap_subr(spec_handle_t *, vaddmap_t, struct __vhandl_s *,
					off_t, size_t, mprot_t, struct cred *,
					pgno_t *);

int spec_cs_delmap_subr(spec_handle_t *, struct __vhandl_s *, size_t,
					struct cred *);

int spec_cs_close_hndl(spec_handle_t *, int, lastclose_t, struct cred *);

spec_ls_ops_t spec_ls2cs_ops = {
	spec_cs_attach,			/* spec_ops_cs_attach		*/
	spec_cs_cmp_gen,		/* spec_ops_cs_cmp_gen		*/
	spec_cs_get_gen,		/* spec_ops_cs_get_gen		*/
	spec_cs_get_opencnt,		/* spec_ops_cs_get_opencnt	*/
	spec_cs_get_size,		/* spec_ops_cs_get_size		*/
	spec_cs_ismounted,		/* spec_ops_cs_ismounted	*/
	spec_cs_mountedflag,		/* spec_ops_cs_mountedflag	*/
	spec_cs_clone,			/* spec_ops_cs_clone		*/
	spec_cs_bmap_subr,		/* spec_ops_bmap		*/
	spec_cs_open_hndl,		/* spec_ops_open		*/
	spec_cs_read_hndl,		/* spec_ops_read		*/
	spec_cs_write_hndl,		/* spec_ops_write		*/
	spec_cs_ioctl_hndl,		/* spec_ops_ioctl		*/
	spec_cs_strgetmsg_hndl,		/* spec_ops_strgetmsg		*/
	spec_cs_strputmsg_hndl,		/* spec_ops_strputmsg		*/
	spec_cs_poll_hndl,		/* spec_ops_poll		*/
	spec_cs_strategy_subr,		/* spec_ops_strategy		*/
	spec_cs_addmap_subr,		/* spec_ops_addmap		*/
	spec_cs_delmap_subr,		/* spec_ops_delmap		*/
	spec_cs_close_hndl,		/* spec_ops_close		*/
	spec_cs_teardown_subr,		/* spec_ops_teardown		*/
};
