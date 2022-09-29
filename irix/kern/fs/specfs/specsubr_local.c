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

#ident	"$Revision: 1.31 $"

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
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pvnode.h>
#include <sys/ddi.h>
#include <sys/major.h>

#ifdef	CELL_IRIX
#include <ksys/cell.h>
#endif	/* CELL_IRIX */

#include <fs/specfs/spec_atnode.h>
#include <fs/specfs/spec_lsnode.h>


/*
 * SPECFS Global Data for the "Local" side.
 */
int	spec_fstype;

dev_t	spec_dev;

struct vfs *spec_vfsp;

struct zone *spec_ls_zone;

struct lsnode **spec_lsnode_table;		/* The lsnode hash table   */
mutex_t spec_lsnode_tablock;			/* spec_lsnode_table lock */


#ifdef	SPECFS_STATS_DEBUG
struct spec_stat_info	spec_stats;
#endif	/* SPECFS_STATS_DEBUG */


STATIC struct lsnode *spec_get(dev_t, vtype_t, vnode_t *, struct cred *);
STATIC void spec_insert(dev_t, struct lsnode *);

#ifdef	JTK_DEBUG
int spec_check_vp(vnode_t *vp);
#endif	/* JTK_DEBUG */

/* 
 * Macros to lock and to unlock lsnode hash table, used during modifications
 * and during traversal of the hash list with possibility of sleep
 */
#define SPEC_LSTAB_LOCK()	mutex_lock(&spec_lsnode_tablock, PINOD)
#define SPEC_LSTAB_UNLOCK()	mutex_unlock(&spec_lsnode_tablock)
#define SPEC_LSTAB_ISLOCKED()	mutex_mine(&spec_lsnode_tablock)


/*
 * Decide how to "attach" to a common csnode.
 *
 * The csnode is the one which is "common" to all the aliased
 * lsnodes that denote the same device.
 *
 * If we determine that the device/driver requires service
 * on some other cell, this is where we'll override the ops
 * vector with one that leads through a "dc <-> ds" transport.
 *
 * NB: lsp is passed in locked.
 */
void
spec_attach(
	lsnode_t	*lsp,
	dev_t		dev,
	vtype_t		type,
	spec_handle_t	*handle,
	spec_handle_t	*cvp_handle,
	long		*gen,
	daddr_t		*size,
	struct stdata	**stream)
{
#ifdef	CELL_IRIX
	extern spec_ls_ops_t spec_ls2dc_ops;


	cell_t		cell_to_use;
	cell_t		my_cell;
	vnode_t		*vp;
#endif	/* CELL_IRIX */


	SPEC_STATS(spec_attach);

#ifdef	CELL_IRIX

	/*
	 * During the creation of this snode
	 * the ops vector has been preset to:
	 *
	 *	lsp->ls2cs_ops = &spec_ls2cs_ops;
	 */

	vp = LSP_TO_VP(lsp);

	my_cell = cell_to_use = cellid();

	if (SPECFS_IS_LOCAL_DEVICE(vp)) {

		cell_to_use = my_cell;

	} else {
		/*
		 * For the moment, until /hwgraph knows how to tell us
		 * otherwise, we'll run all "non-local" devices on the
		 * golden_cell.
		 */
		cell_to_use = golden_cell;
	}

	handle->sh_obj.h_service.s_cell = cell_to_use;

	if (cell_to_use != my_cell) {

		lsp->ls2cs_ops = &spec_ls2dc_ops;

# ifdef	JTK_DEBUG
	printf(
	  "spec_attach[%d]: (%d/%d, %d), vp/0x%x goes to cell %d\n",
					cellid(),
					major(dev), emajor(dev), minor(dev),
					LSP_TO_VP(lsp), cell_to_use);

# endif	/* JTK_DEBUG */
	}

#endif	/* CELL_IRIX */

	VSPECFS_SUBR_ATTACH(lsp, dev, type, handle,
					cvp_handle, gen, size, stream);
}


void
spec_lock(lsnode_t *lsp)
{
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_STATS(spec_lock);

	ASSERT(lsp->ls_locktrips >= 0);

	if (mutex_mine(&lsp->ls_lock)) {
		lsp->ls_locktrips++;

		return;
	}

	mutex_lock(&lsp->ls_lock, 0);
}


void
spec_unlock(lsnode_t *lsp)
{
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_STATS(spec_unlock);

	ASSERT(lsp->ls_locktrips >= 0);

	if (lsp->ls_locktrips > 0) {
		lsp->ls_locktrips--;

		return;
	}

	mutex_unlock(&lsp->ls_lock);
}


#ifdef	SPECFS_DEBUG
int	spec_cycle_num;
#endif	/* SPECFS_DEBUG */

/*
 * Create a new, locked lsnode and its vnode.
 *
 * The caller is responsible for acquiring resources that
 * might cause a "sleep", a "fresh" vnode if needed, and
 * room for a new snode.
 */
/* ARGSUSED */
lsnode_t *
spec_makesnode(
	vtype_t		type,
	dev_t		dev,
	vnode_t		*vp,
	vnode_t		*fsvp,
	vnode_t		*nvp,
	lsnode_t	*nlsp,
	int		line_number)
{
	extern spec_ls_ops_t spec_ls2cs_ops;

	/* REFERENCED */
	int		error;


	SPEC_STATS(spec_makesnode);

	ASSERT(nlsp);			/* Make sure the caller	*/
					/* gave us resources.	*/

	init_mutex(&nlsp->ls_lock, MUTEX_DEFAULT, "spec_lsnode", dev);
	mutex_lock(&nlsp->ls_lock, 0);

	ASSERT(nlsp->ls_locktrips == 0);

	/*
	 * Init'ing the sema is the same as calling spec_lock()
	 */
	SPEC_STATS(spec_lock);

#ifdef	SPECFS_DEBUG
	nlsp->ls_line  = line_number;
	nlsp->ls_cycle = spec_cycle_num++;
#endif	/* SPECFS_DEBUG */

	nlsp->ls_dev = dev;

	if (vp) {
		extern int	hwgfstype;

		ASSERT(nvp == NULL);		/* Make sure the caller	*/
						/* kept his resources.	*/
		/*
		 * Use the vp/vnode provided by the file system:
		 * Insert specfs as a "behavior" that happens
		 * "above" the file system code.
		 */
		VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));

		bhv_desc_init(LSP_TO_BHV(nlsp), nlsp, vp, &spec_vnodeops);

		error = vn_bhv_insert(VN_BHV_HEAD(vp), LSP_TO_BHV(nlsp));

		ASSERT(error == 0);

		VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));

		vn_trace_entry(vp, "spec_bhv_insert",
						(inst_t *)__return_address);

		nlsp->ls_flag |= SPASS;

		VN_HOLD(vp);		/* Get our "teeth" into it */

		vn_trace_entry(vp, "spec_makesnode.1",
						(inst_t *)__return_address);

		nvp = vp;
	} else {

		ASSERT(nvp);			/* Make sure the caller	*/
						/* gave us resources.	*/
		/*
		 * Use the "new" vp as a "specfs" vnode
		 */
		bhv_desc_init(LSP_TO_BHV(nlsp), nlsp, nvp, &spec_vnodeops);

		vn_bhv_insert_initial(VN_BHV_HEAD(nvp), LSP_TO_BHV(nlsp));

		VN_FLAGSET(nvp, VINACTIVE_TEARDOWN);

#ifdef	CELL_IRIX
		spec_at_insert_bhv(nvp, fsvp);	/* Insert "attr" layer	*/

		nlsp->ls_flag |= SATTR;		/* So noted		*/
#endif	/* CELL_IRIX */

		vn_trace_entry(nvp, "spec_makesnode.2",
						(inst_t *)__return_address);

	}

	if (dev_is_vertex(dev))
		hwgraph_vertex_ref(dev_to_vhdl(dev));

	nlsp->ls2cs_ops = &spec_ls2cs_ops;

#if	! ((defined(CELL_IRIX) || defined(SPECFS_TEST)))
	/*
	 * Fill in the "devsw" address now.
	 */
	if (nvp->v_type == VBLK) {
		nlsp->ls_bdevsw = get_bdevsw(dev);
	} else {
		nlsp->ls_cdevsw = get_cdevsw(dev);

		if (nlsp->ls_cdevsw && nlsp->ls_cdevsw->d_str)
			nlsp->ls_flag |= SSTREAM;
	}
#endif	/* ! ((defined(CELL_IRIX) || defined(SPECFS_TEST))) */

	return nlsp;
}


void
spec_free(lsnode_t *lsp)
{
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_STATS(spec_free);

#ifdef	SPECFS_FREE_DEBUG
	/*
	 * Make a scan of ALL of the hash chains to verify that
	 * this "lsp" is really ready to "free".
	 */
	{
		int	i;
		int	already_locked;
		lsnode_t *slsp;


		already_locked = SPEC_LSTAB_ISLOCKED();

		/*
		 * lock the hash table during modification
		 */
		if (! already_locked)
			SPEC_LSTAB_LOCK();

		for (i = 0; i < DLSPECTBSIZE; i++) {

			if ((slsp = spec_lsnode_table[i]) == NULL)
				continue;

			for (; slsp != NULL; slsp = slsp->ls_next) {

				if (lsp == slsp) {
					panic(
				"spec_free: lsp 0x%x found on hash chain %d!",
									lsp, i);
				}
			}
		}

		if (! already_locked)
			SPEC_LSTAB_UNLOCK();
	}
#endif	/* SPECFS_FREE_DEBUG */

	if (dev_is_vertex(lsp->ls_dev))
		hwgraph_vertex_unref(dev_to_vhdl(lsp->ls_dev));

	mutex_destroy(&lsp->ls_lock);

	kmem_zone_free(spec_ls_zone, lsp);
}


/*
 * Insert the "specfs" behavior into this vnode.
 *
 * If no lsnode exists for this dev create one and put it in a table
 * hashed by <dev, fsvp>.
 *
 * If the lsnode for this dev is already in the table return it
 * (ref count is incremented by spec_get).
 *
 * The fsid is inherited from the real vnode so that clones can be found.
 */
vnode_t *
spec_vp(vnode_t *vp, dev_t dev, vtype_t type, struct cred *credp)
{ 
	extern int efs_fstype;
	extern vnode_t *fifovp(struct vnode *, struct cred *);

	bhv_desc_t	*bdp;
	dev_t		newdev;
	lsnode_t	*lsp;
	vnode_t		*nvp;

	SPEC_STATS(spec_vp);

	if (vp == NULL)
		return NULL;

	ASSERT(vp->v_count > 0);

	if (vp->v_type == VFIFO)
		return fifovp(vp, credp);

	ASSERT(vp->v_type == type);
	ASSERT(vp->v_rdev == dev);


	/*
	 * 1st check to see if specfs already exists in this vnode's
	 * behavior chain.
	 *
	 * If it does, even for EFS, we can't allow ourselves to use the
	 * "IS_HWGRAPH_STRING_DEV" & hwgraph_vnode_to_dev() tests,
	 * as hwgraph_vnode_to_dev() does a VOP_GET_ATTR on the vnode,
	 * and we'll give back a different/false answer the 2nd time
	 * around, causing us to translate the "dev" incorrectly.
	 */
	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &spec_vnodeops);

	if (bdp) {

		lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
						/* what we think they are */
		ASSERT((lsp->ls_flag & SCOMMON) == 0);

#ifdef	JTK_DEBUG
		if (dev != lsp->ls_dev)
			printf(
		    "spec_vp: vp/0x%x, dev(%d/%d, %d) lspdev(%d/%d, %d)\n",
							vp,
							major(dev), emajor(dev),
							minor(dev),
							major(lsp->ls_dev),
							emajor(lsp->ls_dev),
							minor(lsp->ls_dev));
#endif	/* JTK_DEBUG */
						/* Find out what "dev"	*/
		dev = lsp->ls_dev;		/* we hash'd it under.	*/

		/*
		 * Now go get it again, properly, the hard way, with locks
		 * and races and vn_gets and all that stuff.
		 * Most all of the get/inactivate race code centers around
		 * using the hash table lock, so we can't just assume at this
		 * point that since we're already in the behavior chain that
		 * we could just "return", i.e. we might be coming back from
		 * vnode cache...
		 */
		lsp = spec_get(dev, type, vp, credp);

	} else if (   (vp->v_vfsp->vfs_fstype == efs_fstype)
		   && (IS_HWGRAPH_STRING_DEV(dev))
		   && (newdev = hwgraph_vnode_to_dev(vp, credp)) != NODEV) {

		/*
		 * This is an EFS "Hardware Graph" kind of device, we need to go
		 * talk to /hwgraph to find out what the "real" rdev should be.
		 */
		lsp = spec_get(newdev, type, vp, credp);

		hwgraph_vertex_unref(dev_to_vhdl(newdev));

		dev = newdev;
	} else {
		lsp = spec_get(dev, type, vp, credp);
	}

					/* Make sure we have one!	*/
	ASSERT(SPEC_HANDLE_TO_OBJID(lsp->ls2cs_handle));

	SPEC_LSP_UNLOCK(lsp);

	nvp = LSP_TO_VP(lsp);

	vn_trace_entry(nvp, "spec_vp", (inst_t *)__return_address);

	return nvp;
}


/*
 * Return the lsnode for a special vnode for the given dev;
 *
 * No vnode is supplied for it to shadow, so one will be allocated.
 *
 * Always create a new lsnode and put it in the table hashed by <dev, NULL>.
 */
STATIC lsnode_t *
make_specvp_locked(
	dev_t		dev,
	vtype_t		type,
	vnode_t		*fsvp,
	lsnode_t	*spare_lsp,
	vnode_t		*spare_vp,
	int		line_number)
{
	lsnode_t	*lsp;
	timespec_t	tv;

	SPEC_STATS(make_specvp_locked);


	ASSERT(spare_lsp != NULL);
	ASSERT(spare_vp  != NULL);

	lsp  = spec_makesnode(type, dev, NULL, fsvp,
					spare_vp, spare_lsp, line_number);

	/*
	 * Fill it in, and "attach" to the common-side.
	 */
	nanotime_syscall(&tv);

	lsp->ls_atime = tv.tv_sec;
	lsp->ls_mtime = tv.tv_sec;
	lsp->ls_ctime = tv.tv_sec;

	lsp->ls_fsid = spec_dev;

	/*
	 * The "attach" routine will update:
	 *	lsp->ls2cs_handle
	 *	lsp->ls_gen
	 *	lsp->ls_size
	 *	lsp->ls_cvp_handle
	 *	vp->v_stream
	 */
	spec_attach(lsp, dev, type,
				&lsp->ls2cs_handle,
				&lsp->ls_cvp_handle,
				&lsp->ls_gen,
				&lsp->ls_size,
				&LSP_TO_VP(lsp)->v_stream);

	spec_insert(dev, lsp);

					/* Make sure we have one!	*/
	ASSERT(SPEC_HANDLE_TO_OBJID(lsp->ls2cs_handle));

	return lsp;
}


vnode_t *
make_specvp(dev_t dev, vtype_t type)
{
	lsnode_t	*lsp;
	lsnode_t	*spare_lsp;
	vnode_t		*lsvp;
	vnode_t		*spare_vp;


	SPEC_STATS(make_specvp);

	spare_lsp = kmem_zone_zalloc(spec_ls_zone, KM_SLEEP);

	spare_vp  = vn_alloc(spec_vfsp, type, dev);

	lsp = make_specvp_locked(dev, type, NULL,
					spare_lsp, spare_vp, __LINE__);

	lsvp = LSP_TO_VP(lsp);

	SPEC_LSP_UNLOCK(lsp);

	return lsvp;
}


/*
 * Find a special vnode that refers to the given device of the given type.
 *
 * Return NULL if a special vnode does not exist.
 *
 * HOLD the vnode before returning it.
 *
 * NOTE! This subroutine only used by stropen() in io/streams/streamio.c
 */
vnode_t *
spec_find(dev_t dev, vtype_t type)
{
	lsnode_t	*lsp;
	vnode_t		*nvp;

	SPEC_STATS(spec_find);

	SPEC_LSTAB_LOCK();

	lsp = spec_lsnode_table[DLSPECTBHASH(dev)];

	while (lsp != NULL) {
		if (lsp->ls_dev == dev) {
			nvp = LSP_TO_VP(lsp);

			if (nvp->v_type == type) {

				VN_HOLD(nvp);

				vn_trace_entry(nvp, "spec_find",
						(inst_t *)__return_address);

				SPEC_LSTAB_UNLOCK();

				return nvp;
			}
		}

		lsp = lsp->ls_next;
	}

	SPEC_LSTAB_UNLOCK();

	return NULL;
}


/*
 * Find a special vnode that refers to the given device.
 *
 * Return True if the special device is "open", indicating in use.
 *
 * NOTE! This subroutine only used by dkscioctl() in io/dksc.c
 */
int
spec_dev_is_inuse(dev_t dev)
{
	int		opencnt;
	lsnode_t	*lsp;


	SPEC_STATS(spec_dev_is_inuse);

	SPEC_LSTAB_LOCK();

	lsp = spec_lsnode_table[DLSPECTBHASH(dev)];

	while (lsp != NULL) {

		if (lsp->ls_dev == dev) {

			opencnt = VSPECFS_SUBR_GET_OPENCNT(lsp,
							  &lsp->ls2cs_handle);
			SPEC_LSTAB_UNLOCK();

			if (opencnt > 0)
				return 1;
			else
				return 0;
		}

		lsp = lsp->ls_next;
	}

	SPEC_LSTAB_UNLOCK();

	return 0;
}


/*
 * Snode lookup stuff.
 *
 * These routines maintain a table of snodes hashed by dev so
 * that the lsnode for an dev can be found if it already exists.
 */

/*
 * Put an lsnode in the table.
 */
STATIC void
spec_insert(dev_t dev, lsnode_t *lsp)
{
	int	already_locked;
	u_int	hash;

						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_STATS(spec_insert);

	already_locked = SPEC_LSTAB_ISLOCKED();

	/*
	 * lock the hash table during modification
	 */
	if (! already_locked)
		SPEC_LSTAB_LOCK();

	hash = DLSPECTBHASH(dev);

	lsp->ls_next = spec_lsnode_table[hash];

	spec_lsnode_table[hash] = lsp;

	if (! already_locked)
		SPEC_LSTAB_UNLOCK();
}


/*
 * Remove a lsnode from the hash table.
 *
 * The fsvp is not released here because spec_inactive() may
 * need it to do a spec_fsync().
 */
void
spec_delete(lsnode_t *lsp)
{
	int		already_locked;
	int		found_it = 0;
	u_int		hash;
	lsnode_t	*st;
	lsnode_t	*stprev = NULL;

						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_STATS(spec_delete);

	already_locked = SPEC_LSTAB_ISLOCKED();

	/*
	 * lock hash table during modification
	 */
	if (! already_locked)
		SPEC_LSTAB_LOCK();

	hash = DLSPECTBHASH(lsp->ls_dev);

	st = spec_lsnode_table[hash];

	while (st != NULL) {
		if (st == lsp) {
			found_it = 1;

			if (stprev == NULL)
				spec_lsnode_table[hash] = st->ls_next;
			else
				stprev->ls_next = st->ls_next;
			break;
		}
		stprev = st;
		st = st->ls_next;
	}

	if (! found_it)
		panic("spec_delete: Missed lsp/0x%x on chain %d!", lsp, hash);

	if (! already_locked)
		SPEC_LSTAB_UNLOCK();
}


/*
 * Lookup an lsnode by <dev, type, vp>.
 *
 * If no lsnode exists, create one and hash it.
 */
lsnode_t *
spec_get(dev_t dev, vtype_t type, vnode_t *vp, struct cred *credp)
{
	int		backoff = 1;
	int		error;
	int		restart;
	u_int		hash;
	bhv_desc_t	*bdp;
	bhv_desc_t	*nbdp;
	lsnode_t	*lsp;
	lsnode_t	*nlsp;
	lsnode_t	*spare_lsp;
	vattr_t		va;
	vmap_t		vmap;
	vnode_t		*lsp_vp;
	vnode_t		*spare_vp;


	ASSERT(vp != NULL);

	SPEC_STATS(spec_get);

#ifdef	JTK_DEBUG
	spec_check_vp(NULL);
#endif	/* JTK_DEBUG */

	spare_vp  = NULL;			/* Nobody	*/
	spare_lsp = NULL;			/*	on deck	*/

	hash = DLSPECTBHASH(dev);

loop:
	SPEC_LSTAB_LOCK();

	lsp = spec_lsnode_table[hash];

	while (lsp != NULL) {
		lsp_vp = LSP_TO_VP(lsp);

		if (   lsp->ls_dev    == dev
		    && lsp_vp->v_type == type
		    && lsp->ls_fsvp   == vp) {

			/*
			 * if we're looking for a fsvp match and its
			 * generation # is different than its common vnode
			 * (due to a vn_kill), make a new one, unless we
			 * can see that the one we've found is in an
			 * inactive state, hanging around due to vnode
			 * caching.
			 *
			 * We must have lsp locked to avoid races with
			 * multiple simultaneous lookups ... we won't get our
			 * common vnode until we have completed unlocking
			 * lsp.
			 *
			 * All spec vnodes get a common vnode so if we don't
			 * have one, we just need to wait a bit.
			 *
			 * No lsnode can go away since we hold the hash table
			 * lock
			 */
			restart = 0;

			if (SPEC_HANDLE_TO_OBJID(lsp->ls2cs_handle) == NULL) {
				/*
				 * The lsnode doesn't have a common snode yet
				 *
				 * All we need to do is acquire the
				 * SPEC_LSP_LOCK - by then it will
				 * have a common vnode - but we need to
				 * go through all the stuff below to
				 * atomically grab the lock
				 */
				restart = 1;

			} else if (! VSPECFS_SUBR_CMP_GEN(lsp,
							  &lsp->ls2cs_handle,
							  lsp->ls_gen)) {

				/*
				 * The "generation" #'s in the lsnode and
				 * csnode don't agree...
				 * Check to see if the lsnode is currently
				 * "inactive", being kept around due to
				 * vnode caching, in which case we can
				 * safely "revive" the lsnode and re-use it.
				 */
				if (lsp->ls_flag & SINACTIVE) {
#ifdef	JTK_DEBUG
					printf(
		"spec_get: Gen # UPGRADING dev/(%d/%d, %d) vp/0x%x ",
						major(dev), emajor(dev),
						minor(dev), vp);
					printf(
		"v_count/%d ls_flag/0x%x ls_opencnt/%d ls_gen/%d cs_gen/%d!\n",
						vp->v_count, lsp->ls_flag,
						lsp->ls_opencnt, lsp->ls_gen,
						VSPECFS_SUBR_GET_GEN(lsp,
							&lsp->ls2cs_handle));
#endif	/* JTK_DEBUG */

					lsp->ls_gen = VSPECFS_SUBR_GET_GEN(lsp,
							&lsp->ls2cs_handle);

				} else {
					/*
					 * Check to see if we have the
					 * resources to do this now,
					 * if not, break out, get them
					 * & start over.
					 */
					if (   spare_lsp == NULL
					    || spare_vp  == NULL)
						break;

					/*
					 * We need to leave the existing vnode
					 * alone, somebody's still possibly
					 * using it.  Manufacture a specfs
					 * vnode (non-cached) for this case.
					 * Return it locked..
					 */
					nlsp = make_specvp_locked(dev,
								  type,
								  vp,
								  spare_lsp,
								  spare_vp,
								  __LINE__);

					spare_vp  =  NULL;	/* Used */
					spare_lsp = NULL;	/*   up */
#ifdef	JTK_DEBUG
					printf(
		"spec_get: Gen # REPLICATING dev/(%d/%d, %d) vp/0x%x nvp/0x%x ",
							major(dev),
							emajor(dev),
							minor(dev),
							vp, LSP_TO_VP(nlsp));
					printf(
		"v_count/%d ls_flag/0x%x ls_opencnt/%d ls_gen/%d cs_gen/%d!\n",
						vp->v_count, lsp->ls_flag,
						lsp->ls_opencnt, lsp->ls_gen,
						VSPECFS_SUBR_GET_GEN(lsp,
							&lsp->ls2cs_handle));
#endif	/* JTK_DEBUG */
					/*
					 * Fill in the ls_fsvp field under
					 * protection of the SPEC_LSTAB_LOCK,
					 * and take a reference on it, as
					 * there'll be a VN_RELE() done in
					 * spec_teardown_fsync() if ls_fsvp
					 * != NULL.
					 */
					nlsp->ls_fsvp = lsp->ls_fsvp;

					if (nlsp->ls_fsvp) {

						VN_HOLD(nlsp->ls_fsvp);

						vn_trace_entry(nlsp->ls_fsvp,
							"spec_get.0",
							(inst_t *)
							    __return_address);
					}

					lsp = nlsp;

					vp = LSP_TO_VP(lsp);

					goto fill_in_a_new_one;
				}
			}

			VMAP(lsp_vp, vmap);

			SPEC_LSTAB_UNLOCK();

			if ( ! (lsp_vp = vn_get(lsp_vp, &vmap, 0))) {

				/*
				 * We may be blocking the thread that
				 * needs to make progress in order to
				 * get us the proper setup, delay a bit.
				 */
				delay(backoff);
				backoff *= 2;

				goto loop;
			}

			ASSERT(LSP_TO_VP(lsp) == lsp_vp);
			
			SPEC_LSP_LOCK(lsp);

			if (restart) {
				SPEC_LSP_UNLOCK(lsp);

				vn_trace_entry(lsp_vp, "spec_get",
						(inst_t *)__return_address);
				VN_RELE(lsp_vp);

				/*
				 * We may be blocking the thread that
				 * needs to make progress in order to
				 * get us the proper setup, delay a bit.
				 */
				delay(backoff);
				backoff *= 2;

				goto loop;
			}

			lsp->ls_flag &= ~SINACTIVE;	/* Mark "active" */

			if (spare_lsp) {
				kmem_zone_free(spec_ls_zone, spare_lsp);

				spare_lsp = NULL;
			}

			if (spare_vp) {
				vn_free(spare_vp);

				spare_vp = NULL;
			}

			return lsp;
		}

		lsp = lsp->ls_next;
	}

	/*
	 * To avoid (witnessed) deadlocks between spec_makesnode() and
	 * spec_delete() when called out of spec_teardown(); due to
	 * kmem_zone_zalloc() possibly sleeping during memory allocation,
	 * we need to obtain a skeleton snode to be used by spec_makesnode()
	 * without the protection of SPEC_LSTAB_LOCK.  So we move the obtaining
	 * of resources out of spec_makesnode(), and make it the caller's
	 * responsibility to obtain the proper resources.
	 *
	 * This exposes us to a race condition where some other thread could
	 * insert dev/type while we're off waiting on kmem_zone_zalloc(), so
	 * we'll * need to re-chase the hash chain after we get back from
	 * kmem_zone_zalloc().
	 */
	if (spare_lsp == NULL || spare_vp == NULL) {

		SPEC_LSTAB_UNLOCK();

		if (spare_lsp == NULL)
			spare_lsp = kmem_zone_zalloc(spec_ls_zone, KM_SLEEP);

		if (spare_vp == NULL)
			spare_vp  = vn_alloc(spec_vfsp, type, dev);

		ASSERT(spare_lsp != NULL);
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
	 * If we drop off the end of the "while" loop, we couldn't
	 * find a match on dev/type/vp in the hash table.
	 * Arrange to create a "new" entry.
	 */

#ifdef	JTK_DEBUG
	spec_check_vp(vp);
#endif	/* JTK_DEBUG */

	/*
	 * Free up unused resources.
	 */
	if (spare_vp) {
		vn_free(spare_vp);

		spare_vp = NULL;
	}

	/*
	 * spec_makesnode() creates lsp locked.
	 */
	lsp = spec_makesnode(type, dev, vp, NULL, NULL, spare_lsp, __LINE__);

	/*
	 * Do what spec_insert() would normally do
	 * to insert a new lsnode into the hash chain.
	 */
	lsp->ls_next = spec_lsnode_table[hash];

	spec_lsnode_table[hash] = lsp;

	/*
	 * Fill in the ls_fsvp field under protection of the SPEC_LSTAB_LOCK.
	 */
	lsp->ls_fsvp = vp;

fill_in_a_new_one:

	SPEC_LSTAB_UNLOCK();

	/*
	 * Init the times in the lsnode to those in the vnode.
	 */
	va.va_mask = AT_TIMES|AT_FSID;

	if (lsp->ls_flag & SPASS) {

		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &spec_vnodeops);

		ASSERT(bdp);
				/* prevent behavior insert/remove */
		VN_BHV_READ_LOCK(VN_BHV_HEAD(vp));

		PV_NEXT(bdp, nbdp, vop_getattr);
		PVOP_GETATTR(nbdp, &va, 0, credp, error);

		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	} else {
		VOP_GETATTR(vp, &va, 0, credp, error);
	}

	if (error == 0) {
		lsp->ls_atime = va.va_atime.tv_sec;
		lsp->ls_mtime = va.va_mtime.tv_sec;
		lsp->ls_ctime = va.va_ctime.tv_sec;
		lsp->ls_fsid  = va.va_fsid;
	} else {
		lsp->ls_fsid  = spec_dev;
	}

	/*
	 * For VBLK & VCHR types, attach to a "common"
	 * snode, referred to in the code as a "csnode".
	 */
	if (type == VBLK || type == VCHR) {
		/*
		 * The "attach" routine will update:
		 *	lsp->ls2cs_handle
		 *	lsp->ls_cvp_handle
		 *	lsp->ls_gen
		 *	lsp->ls_size
		 *	vp->v_stream
		 */
		spec_attach(lsp, dev, type,
					  &lsp->ls2cs_handle,
					  &lsp->ls_cvp_handle,
					  &lsp->ls_gen,
					  &lsp->ls_size,
					  &vp->v_stream);
	}

	return lsp;
}


/*
 * Clone a device, usually a stream.
 *
 * Code was originally moved here to prevent the streams complex from
 * having to have detailed knowledge of specfs internals...  sigh...
 *
 * Wound up obviating the need to export the make_specvp_locked()
 * interface.
 */
lsnode_t *
spec_make_clone(
	lsnode_t	*lsp,
	dev_t		dev,
	vtype_t		type,
	vnode_t		*vp,
	vnode_t		**vpp,
	struct stdata	*stp,
	int		flag)
{
	lsnode_t	*nlsp;
	lsnode_t	*spare_lsp;
	vnode_t		*nvp;
	vnode_t		*spare_vp;


	SPEC_STATS(spec_make_clone);

	spare_lsp = kmem_zone_zalloc(spec_ls_zone, KM_SLEEP);

	spare_vp  = vn_alloc(spec_vfsp, type, dev);

	nlsp = make_specvp_locked(dev, type, NULL,
					spare_lsp, spare_vp, __LINE__);

	nvp = LSP_TO_VP(nlsp);

#ifdef	JTK_DEBUG
	if (lsp->ls_flag & SINACTIVE) {
		panic("spec_make_clone: nvp/0x%x{%d} SINACTIVE!\n",
							nvp, nvp->v_count);
	}
#else	/* ! JTK_DEBUG */
	ASSERT((lsp->ls_flag & SINACTIVE) == 0);
#endif	/* ! JTK_DEBUG */

	/*
	 * Note that nlsp's lsnode is locked but the common vnode's lsnode
	 * is NOT.
	 * "Theory" says this should be okay, as the "common" vnode/snode
	 * shouldn't be able to disappear on their own, only through some
	 * action vectored through the locked "local" vnode/snode.
	 *
	 * Clones inherit ls_fsid and v_vfsp..
	 */
	nlsp->ls_fsid = lsp->ls_fsid;
	nvp->v_vfsp   = vp->v_vfsp;


	/*
	 * Link off to the "common" side to tie up loose ends
	 * like "v_stream", and "sd_vnode", and finalizing the
	 * streams open, if that's what this is.
	 *
	 * Note: that this is about the only time we "actually"
	 *	 need ls_cvp_handle as information, in this case, it's
	 *	 the "old vp" used in the original stropen() call.
	 */
	VSPECFS_SUBR_CLONE(nlsp, &nlsp->ls2cs_handle, stp, &lsp->ls_cvp_handle,
							flag, &nvp->v_stream);
	/*
	 * Relinquish our "hold" on the "old" lsp, transition to "nlsp".
	 */
	lsp->ls_opencnt--;

	SPEC_LSP_UNLOCK(lsp);

	vn_trace_entry(vp, "spec_make_clone-", (inst_t *)__return_address);

	VN_RELE(vp);


	/*
	 * "nlsp" is already locked since it was created
	 * by make_specvp_locked() that way.
	 */
	nlsp->ls_opencnt++;

	*vpp = nvp;

	vn_trace_entry(nvp, "spec_make_clone+", (inst_t *)__return_address);

	return nlsp;
}


/*
 * Mark the accessed, updated, or changed times in a lsnode
 * with the current time.
 */
void
spec_mark(lsnode_t *lsp, int flag)
{
	int		locked;
	timespec_t	tv;

						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_STATS(spec_mark);

	locked = 0;

	nanotime_syscall(&tv);

	if ((flag & lsp->ls_flag) == 0) {
		SPEC_LSP_LOCK(lsp);

		locked = 1;

		lsp->ls_flag |= flag;
	}

	if (flag & SACC)
		lsp->ls_atime = tv.tv_sec;

	if (flag & SUPD)
		lsp->ls_mtime = tv.tv_sec;

	if (flag & SCHG)
		lsp->ls_ctime = tv.tv_sec;

	if (locked)
		SPEC_LSP_UNLOCK(lsp);
}


void
spec_mountedflag(vnode_t *vp, int set)
{
	bhv_desc_t	*bdp;
	lsnode_t	*lsp;

	SPEC_STATS(spec_mountedflag);

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &spec_vnodeops);

	if (bdp == NULL)
		return;

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	SPEC_LSP_LOCK(lsp);

	VSPECFS_SUBR_MOUNTEDFLAG(lsp, &lsp->ls2cs_handle, set);

	SPEC_LSP_UNLOCK(lsp);
}


int
spec_ismounted(vnode_t *vp)
{
	bhv_desc_t	*bdp;
	lsnode_t	*lsp;

	SPEC_STATS(spec_ismounted);

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &spec_vnodeops);

	if (bdp == NULL)
		return -1;

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	return VSPECFS_SUBR_GET_ISMOUNTED(lsp, &lsp->ls2cs_handle);
}


daddr_t
spec_devsize(vnode_t *vp)
{
	bhv_desc_t	*bdp;
	lsnode_t	*lsp;

	SPEC_STATS(spec_devsize);

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &spec_vnodeops);

	if (bdp == NULL)
		return -1;

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	return VSPECFS_SUBR_GET_SIZE(lsp, &lsp->ls2cs_handle);
}


/*
 * routine used by console driver to keep its vnode up-to-date
 * Caller better have a vnode reference!!
 * We are only called after getting an EIO so to help
 * debugging strange problems we return an error if gen #'s match
 *
 * NOTE! This subroutine used by cnread(), cnwrite(), cnioctl() & cnpoll()
 * in io/cn.c
 */
int
spec_fixgen(vnode_t *vp)
{
	long		cgen;
	bhv_desc_t	*bdp;
	lsnode_t	*lsp;

	SPEC_STATS(spec_fixgen);

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &spec_vnodeops);

	if (bdp == NULL)
		cmn_err_tag(86,CE_PANIC, "spec_fix_gen: bad vnode 0x%x\n", vp);

	lsp = BHV_TO_LSP(bdp);
						/* Make sure things are   */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);	/* what we think they are */

	cgen = VSPECFS_SUBR_GET_GEN(lsp, &lsp->ls2cs_handle);

#ifdef	JTK_DEBUG
	if (lsp->ls_gen == cgen) {
		printf(
		    "spec_fixgen: fixup for vp/0x%x, ls_gen==cs_gen %d/%d\n",
						vp, lsp->ls_gen, cgen);
	} else {
		printf(
		    "spec_fixgen: fixup for vp/0x%x, ls_gen!=cs_gen %d/%d\n",
						vp, lsp->ls_gen, cgen);
	}
#endif	/* JTK_DEBUG */

	if (lsp->ls_gen == cgen)
		return 1;

	lsp->ls_gen = cgen;

	return 0;
}


/*
 * this routine is called from spec_sync, while traversing the hash list,
 * the routine can go to sleep, it therefore locks the hash table before
 * initiating the traversal 
 */
void
spec_flush(void)
{
	lsnode_t **spp, *sp;
	vnode_t *vp;
	
	SPEC_STATS(spec_flush);

	SPEC_LSTAB_LOCK();

	for (spp = spec_lsnode_table;
			spp < &spec_lsnode_table[DLSPECTBSIZE];
					spp++) {

		for (sp = *spp; sp != NULL; sp = sp->ls_next) {
			vp = LSP_TO_VP(sp);

			if (vp->v_type == VBLK)
				bflush(vp->v_rdev);
		}
	}

	SPEC_LSTAB_UNLOCK();
}


void
spec_ls_init()
{
	spec_ls_zone = kmem_zone_init(sizeof(struct lsnode), "specFS LSnodes");

	spec_lsnode_table =
			kern_calloc(1, DLSPECTBSIZE * sizeof(struct lsnode *));

	mutex_init(&spec_lsnode_tablock, MUTEX_DEFAULT, "lspectb");
}


#ifdef	JTK_DEBUG

int
spec_check_vp(vnode_t *vp)
{
	int		i;
	int		already_locked;
	u_int		hash;
	lsnode_t	*lsp;
	vnode_t		*lsp_vp;

	SPEC_STATS(spec_check_vp);

	already_locked = SPEC_LSTAB_ISLOCKED();

	/*
	 * lock hash table during modification
	 */
	if (! already_locked)
		SPEC_LSTAB_LOCK();

	for (i = 0; i < DLSPECTBSIZE; i++) {
		lsp = spec_lsnode_table[i];

		while (lsp != NULL) {
			lsp_vp = LSP_TO_VP(lsp);

			if (lsp_vp == NULL) {
				printf(
			    "spec_check_vp: i/%d lsp/0x%x vp == NULL\n",
								i, lsp);

				panic("spec_check_vp: lsp_vp == NULL!");
			}

			if (vp) {
				if (lsp_vp == vp) {
					printf(
			    "spec_check_vp: i/%d lsp/0x%x vp == 0x%x\n",
								i, lsp, vp);

					panic("spec_check_vp: lsp_vp == vp!");
				}
			}

			if (lsp->ls_dev != lsp_vp->v_rdev) {
				printf(
	"spec_check_vp: i/%d lsp/0x%x lsp_vp/0x%x ls_dev/0x%x != v_rdev/0x%x\n",
								i, lsp, lsp_vp,
								lsp->ls_dev,
								lsp_vp->v_rdev);

				panic("spec_check_vp: rdev mismatch error!");
			}

			hash = DLSPECTBHASH(lsp->ls_dev);

			if (i != hash) {
				printf(
			"spec_check_vp: lsp/0x%x lsp_vp/0x%x i/%d != hash/%d\n",
								lsp, lsp_vp,
								i, hash);

				panic("spec_check_vp: hash mismatch error!");
			}

			lsp = lsp->ls_next;
		}
	}
	
	if (! already_locked)
		SPEC_LSTAB_UNLOCK();

	return 0;
}

#endif	/* JTK_DEBUG */
