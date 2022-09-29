/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: pshm.c,v 1.65 1998/09/14 16:14:27 jh Exp $"

/*
 * This file contains the physical object operations for system
 * V shared memory. The object in point is the pshm structure
 * which is the kernel internal structure as opposed to the shmid_ds
 * structure which is exported to the application.
 */

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/mac_label.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/shm.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/idbgentry.h>
#include <sys/cmn_err.h>

#include "vshm_mgr.h"
#include "vshm_private.h"

#ifdef _VCE_AVOIDANCE
#include <sys/fs_subr.h>
#include <sys/vfs.h>
#include <sys/vnode.h>

extern int vce_avoidance;
/*
 * vnodeops for shm dummy vnodes
 */
static int	shmnode_inactive(bhv_desc_t *, cred_t *);

vnodeops_t shmnode_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	(vop_open_t)fs_nosys,
	(vop_close_t)fs_nosys,
	(vop_read_t)fs_nosys,
	(vop_write_t)fs_nosys,
	(vop_ioctl_t)fs_nosys,
	fs_setfl,
	(vop_getattr_t)fs_nosys,
	(vop_setattr_t)fs_nosys,
	(vop_access_t)fs_nosys,
	(vop_lookup_t)fs_nosys,
	(vop_create_t)fs_nosys,
	(vop_remove_t)fs_nosys,
	(vop_link_t)fs_nosys,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	(vop_readdir_t)fs_nosys,
	(vop_symlink_t)fs_nosys,
	(vop_readlink_t)fs_nosys,
	(vop_fsync_t)fs_nosys,
	shmnode_inactive,
	(vop_fid_t)fs_nosys,
	(vop_fid2_t)fs_nosys,
	(vop_rwlock_t)fs_noval,
	(vop_rwunlock_t)fs_noval,
	(vop_seek_t)fs_nosys,
	fs_cmp,
	(vop_frlock_t)fs_nosys,
	(vop_realvp_t)fs_nosys,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_noval,
	(vop_map_t)fs_nodev,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	(vop_poll_t)fs_nosys,
	(vop_dump_t)fs_nosys,
	(vop_pathconf_t)fs_nosys,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	(vop_attr_get_t)fs_nosys,
	(vop_attr_set_t)fs_nosys,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	(vop_cover_t)fs_nosys,
	(vop_link_removed_t)fs_noval,
	(vop_vnode_change_t)fs_nosys,
	(vop_ptossvp_t)fs_noval,
	(vop_pflushinvalvp_t)fs_noval,
	(vop_pflushvp_t)fs_nosys,
	(vop_pinvalfree_t)fs_noval,
	(vop_sethole_t)fs_noval,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};
#endif

extern int vshm_count;
/* tuneables */
extern size_t	shmmax;
extern size_t	shmmin;
extern int	sshmseg;

/*
 * Allocate a shared memory desriptor and a region to go with it.
 */
int
pshm_alloc(
	int		key,
	int		flags,
	size_t		size,
	pid_t		pid,
	struct cred	*cred,
        vshm_t		*vshm)
{
	pshm_t		*ps;	/* shared memory descriptor */
	as_mohandle_t	mo;
	int		error;
#ifdef _VCE_AVOIDANCE
	struct vnode	*vp = NULL;
#define VCE_VP		(vp)
	bhv_desc_t	*bdp;	/* behavior descriptor for the vnode */
#else
#define VCE_VP		NULL
#endif

	/*
	 * Check size limits of the request
	 */
	if (size < shmmin || size > shmmax)
		return(EINVAL);

	/*
	 * Allocate the descriptor from node private memory and
	 * initialize the ipc_perm structure from the creds of
	 * process that caused this creation.
	 */
	ps = kern_malloc(sizeof(pshm_t));
	if (ps == (pshm_t *)0)
		return(ENOSPC);
	ipc_init(&ps->psm_perm, key, flags, cred);

#ifdef _VCE_AVOIDANCE
	if (vce_avoidance) {
		VCE_VP = vn_alloc(NULL, VNON, NODEV);
		bdp = (bhv_desc_t *)kmem_zone_alloc(bhv_global_zone, 0);
		bhv_desc_init(bdp, ps, VCE_VP, &shmnode_vnodeops);
		vn_bhv_insert_initial(VN_BHV_HEAD(VCE_VP), bdp);
		VN_FLAGSET(VCE_VP, VINACTIVE_TEARDOWN);
	}
#endif
	/*
	 * Allocate the memory object for this descriptor
	 */
	error = as_allocshmmo(VCE_VP, vshm->vsm_id, &mo);
	if (error) {
#ifdef _VCE_AVOIDANCE
		if (VCE_VP)
			VN_RELE(VCE_VP);
#endif
		kern_free(ps);
		return(error);
	}

	/*
	 * Finally now that everything has been created, initialize
	 * the descriptor.
	 */
	ps->psm_perm.mode |= SHM_INIT;  /* grow on first attach */
	ps->psm_segsz = size;
	ps->psm_mo = mo;
	ps->psm_atime = ps->psm_dtime = 0;
	ps->psm_ctime = time;
	ps->psm_lpid = 0;
	ps->psm_cpid = pid;
	ps->psm_maclabel = cred->cr_mac;
	mutex_init(&ps->psm_lock, MUTEX_DEFAULT, "pshmlk");

	/*
	 * Use a proc exit callback to remove AUTORMID segments.
	 */
	if (flags & IPC_AUTORMID) {
		ps->psm_perm.mode |= IPC_AUTORMID;
		error = add_exit_callback(pid, 0, shm_rmid_callback,
				(void*)(__psint_t)(vshm->vsm_id));
		if (error) {
			cmn_err(CE_WARN,
				"pshm_alloc: add_exit_callback(%d,0,shm_rmid_callback,%d) returned %d\n",
				pid, vshm->vsm_id, error);
#ifdef _VCE_AVOIDANCE
			if (VCE_VP)
				VN_RELE(VCE_VP);
#endif
			kern_free(ps);
			return(error);
		}
	}
	
	/* Initialize the behavior descriptor and put it on the chain. */
	bhv_desc_init(&ps->psm_bd, ps, vshm, &pshm_ops);
	bhv_insert_initial(&vshm->vsm_bh, &ps->psm_bd);

#ifdef _VCE_AVOIDANCE
	/*
	 * The as_allocshmmo() call took a reference on the vnode, so
	 * drop ours.  The vnode will go inactive and be freed
	 * when the memory object is destroyed.
	 */
	if (VCE_VP)
		VN_RELE(VCE_VP);
#endif
	/* Return success */
	return(0);
}

/*
 * Free a shared memory descriptor
 */
void
pshm_free(pshm_t *ps)
{
	if ((ps->psm_perm.mode & IPC_ALLOC) == 0) {
		/* shmid was removed */
		as_freeshmmo(ps->psm_mo);
	}
	mutex_destroy(&ps->psm_lock);
	kern_free(ps);
}

shmops_t pshm_ops = {
	BHV_IDENTITY_INIT_POSITION(VSHM_POSITION_PSHM),
	pshm_getmo,
	pshm_keycheck,
	pshm_attach,
	pshm_setdtime,
	pshm_rmid,
	pshm_ipcset,
	pshm_getstat,
	pshm_mac_access,
	pshm_destroy,
};

/*
 * Get the memory object handle from the shared memory descriptor
 * No locking necessary since it doesn't change.
 */
void
pshm_getmo(bhv_desc_t *bdp, as_mohandle_t *mo)
{
	*mo = BHV_TO_PSHM(bdp)->psm_mo;
}

/*
 * Check access to a key
 */
int
pshm_keycheck(bhv_desc_t *bdp, struct cred *cred, int flags, size_t size)
{
	pshm_t *ps = BHV_TO_PSHM(bdp);
	int error = 0;

	/*
	 * Check for exclusive access
	 */
	if ((flags & (IPC_CREAT | IPC_EXCL)) ==
	    (IPC_CREAT | IPC_EXCL)) {
		return(EEXIST);
	}

	/*
	 * check for access mode
	 */
	mutex_lock(&ps->psm_lock, PZERO);
	if ((flags & 0777) & ~(ps->psm_perm.mode)) {
		error = EACCES;
		goto out;
	}

	/*
	 * Check the callers creds
	 */
	if (error = _MAC_ACCESS(ps->psm_maclabel, cred, MACWRITE)) {
		error = EACCES;
		goto out;
	}

	/*
	 * check for size
	 */
	if (size && (size > ps->psm_segsz)) {
		error = EINVAL;
		goto out;
	}

out:
	ASSERT(mutex_mine(&ps->psm_lock));
	mutex_unlock(&ps->psm_lock);
	return error;
}

int
pshm_attach(bhv_desc_t *bdp,
	int flag,
	asid_t asid,
	pid_t pid,
	struct cred *cred,
	caddr_t addr,
	caddr_t *aaddr)
{
	pshm_t *ps = BHV_TO_PSHM(bdp);
	int error, doinit = 0;
	vasid_t vasid;
	as_addspace_t as;
	as_addspaceres_t asres;

	/* make access checks atomically */
	mutex_lock(&ps->psm_lock, PZERO);
	if (error = _MAC_ACCESS(ps->psm_maclabel, cred, MACWRITE)) {
		mutex_unlock(&ps->psm_lock);
		return EACCES;
	}

	/* check permissions */
	if (error = ipcaccess(&ps->psm_perm, cred, SHM_R)) {
		mutex_unlock(&ps->psm_lock);
		return(error);
	}
	if (((flag & SHM_RDONLY) == 0) &&
	    (error = ipcaccess(&ps->psm_perm, cred, SHM_W))) {
		mutex_unlock(&ps->psm_lock);
		return(error);
	}
	if (addr) {
		if (addr < (caddr_t) LOWUSRATTACH) {
			mutex_unlock(&ps->psm_lock);
			return EINVAL;
		}
		if (flag & SHM_RND)
			addr = (caddr_t) ((__psint_t)addr & ~(SHMLBA - 1));
	} else {
		/* SPT: allow shared page tables
		 * only if address is selected
		 */
		if (flag & SHM_SHATTR) {
			mutex_unlock(&ps->psm_lock);
			return EINVAL;
		}
	}

	/*
	 * SPT
	 * Shared PTs require RDWR mapping
	 */
	if ((flag & (SHM_SHATTR | SHM_RDONLY)) == (SHM_SHATTR | SHM_RDONLY)) {
		mutex_unlock(&ps->psm_lock);
		return EACCES;
	}

	while (ps->psm_perm.mode & SHM_INPROG) {
		sv_wait(&vshm_syncinit, PZERO, &ps->psm_lock, 0);
		mutex_lock(&ps->psm_lock, PZERO);
		if (ps->psm_perm.mode & SHM_INIT)
			/* INPROG'er failed */
			break;
	}

	if (ps->psm_perm.mode & SHM_INIT) {
		doinit = 1;
		ps->psm_perm.mode &= ~SHM_INIT;
		ps->psm_perm.mode |= SHM_INPROG;
	}
	mutex_unlock(&ps->psm_lock);

	if (as_lookup(asid, &vasid)) {
		error = EINVAL;
		goto failinit;
	}
	as.as_op = AS_ADD_SHM;
	as.as_addr = addr;
	as.as_length = ps->psm_segsz;
	as.as_prot = (flag & SHM_RDONLY) ? PROT_READ : PROT_READ|PROT_WRITE;
	/* Permit mprotect to set executable by setting maxprot R?X */
	as.as_maxprot = as.as_prot | PROT_EXEC;
	as.as_shm_maxsegs = sshmseg;
	as.as_shm_mo = ps->psm_mo;
	as.as_shm_flags = (doinit ? AS_SHM_INIT : 0) | 
				(flag & SHM_SHATTR ? AS_SHM_SHATTR : 0);

	if (error = VAS_ADDSPACE(vasid, &as, &asres))
		goto at_out;

	*aaddr = asres.as_addr;

	mutex_lock(&ps->psm_lock, PZERO);

	if (doinit) {
		/* all done - signal any waiters */
		ASSERT(ps->psm_perm.mode & SHM_INPROG);
		ps->psm_perm.mode &= ~SHM_INPROG;
		sv_broadcast(&vshm_syncinit);
	}
	ASSERT(!(ps->psm_perm.mode & SHM_INIT));

	/* set attach time */
	ps->psm_atime = time;
	ps->psm_lpid = pid;
	ASSERT(mutex_mine(&ps->psm_lock));
	mutex_unlock(&ps->psm_lock);
	as_rele(vasid);
	return 0;

at_out:
	as_rele(vasid);
failinit:
	if (doinit) {
		mutex_lock(&ps->psm_lock, PZERO);
		ASSERT(ps->psm_perm.mode & SHM_INPROG);
		ASSERT(!(ps->psm_perm.mode & SHM_INIT));
		ps->psm_perm.mode &= ~SHM_INPROG;
		ps->psm_perm.mode |= SHM_INIT;
		sv_broadcast(&vshm_syncinit);
		mutex_unlock(&ps->psm_lock);
	}
	ASSERT(error);
	return(error);
}

/*
 * Set the detach time of a process of a shared memory segment
 * This also records which process did the detach.
 */
void
pshm_setdtime(bhv_desc_t *bdp, int dtime, pid_t pid)
{
	pshm_t *ps = BHV_TO_PSHM(bdp);

	mutex_lock(&ps->psm_lock, PZERO);
	ps->psm_dtime = dtime;
	ps->psm_lpid = pid;
	ASSERT(mutex_mine(&ps->psm_lock));
	mutex_unlock(&ps->psm_lock);
}

/*
 * Remove a shared memory identifier
 * Very little is done here except to:
 * a) mark pshm as no longer allocated
 * b) remove from lookup tables
 * c) release our 'initial' reference.
 * When the reference count goes to zero we will actually
 * release things.
 *
 * Called with bdp ref'd and returns with same reference
 * It's important that the vshm doesn't go away until we return
 * since we're invoked via an interposer that needs the bdp struct
 * to do locking
 * Note also that 2 threads (from different cells) could be in here at once,
 * we need to be sure that only one really does all the work.
 *
 * zap some stuff so that we know whether to free things
 * when we free the actual shm
 */
int
pshm_rmid(bhv_desc_t *bdp, struct cred *cred, int flags)
{
	pshm_t *ps = BHV_TO_PSHM(bdp);
	vshm_t *vshm = BHV_TO_VSHM(bdp);
	int error = 0;
	int rmid_error;

	/* check permissions */
	mutex_lock(&ps->psm_lock, PZERO);
	if (cred->cr_uid != ps->psm_perm.uid &&
	    cred->cr_uid != ps->psm_perm.cuid &&
	    !_CAP_CRABLE(cred, CAP_FOWNER)) {
		error = EPERM;
		mutex_unlock(&ps->psm_lock);
	} else if (ps->psm_perm.mode & IPC_ALLOC) {
		ps->psm_perm.mode &= ~IPC_ALLOC;
		mutex_unlock(&ps->psm_lock);

		/* If we're getting called via the IPC_AUTORMID callback,
		 * flags contains IPC_AUTORMID.  No need to remove the
		 * callback if we're being called via the callback.
		 */
		if (ps->psm_perm.mode & IPC_AUTORMID &&
		    !(flags & IPC_AUTORMID)) {
			/*
			 * Remove shm_rmid_callback from creating pid.
			 */
			rmid_error = add_exit_callback(ps->psm_cpid,
					ADDEXIT_REMOVE,
					shm_rmid_callback,
					(void*)(__psint_t)(vshm->vsm_id));
			if (rmid_error) {
				cmn_err(CE_WARN,
					"pshm_rmid: add_exit_callback(%d,ADDEXIT_REMOVE,shm_rmid_callback,%d) returned %d\n",
					ps->psm_cpid, vshm->vsm_id, rmid_error);
			}
		}

		/*
		 * Remove vshm from lookup tables
		 */
		vshm_remove(vshm, 1);
	} else
		mutex_unlock(&ps->psm_lock);
	return error;
}

/*
 * Set uid, gid and mode in the struct ipc_perm of a shared memory
 * descriptor. This also modifies the ctime.
 */
int
pshm_ipcset(bhv_desc_t *bdp, struct ipc_perm *perm, struct cred *cred)
{
	pshm_t *ps = BHV_TO_PSHM(bdp);
	int error = 0;

	/* check permissions */
	mutex_lock(&ps->psm_lock, PZERO);
	if (cred->cr_uid != ps->psm_perm.uid &&
	    cred->cr_uid != ps->psm_perm.cuid &&
	    !_CAP_CRABLE(cred, CAP_FOWNER)) {
		error = EPERM;
	} else {
		ps->psm_perm.uid = perm->uid;
		ps->psm_perm.gid = perm->gid;
		ps->psm_perm.mode = (perm->mode & 0777) |
				    (ps->psm_perm.mode & ~0777);
		ps->psm_ctime = time;
	}
	ASSERT(mutex_mine(&ps->psm_lock));
	mutex_unlock(&ps->psm_lock);
	return error;
}

/*
 * Get the status of a shared memory segment
 */
int
pshm_getstat(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	struct shmid_ds	*shmds,
	cell_t		*cell)
{
	pshm_t *ps = BHV_TO_PSHM(bdp);
	int error;

	mutex_lock(&ps->psm_lock, PZERO);
	if (error = ipcaccess(&ps->psm_perm, cred, SHM_R)) {
		if ((error == EACCES) && (cred->cr_uid != ps->psm_perm.cuid)) {
			mutex_unlock(&ps->psm_lock);
			shmds->shm_cpid = NOPID;
			return error;
		}
		/* 
		 * We created this segment, but have no read access.
		 * This is a pretty unusual situation!  Fill in just
		 * the shm_perm.uid field and return EACCES.  ipcs
		 * will know what to do; but other programs will just
		 * see this as an error (which is correct according 
		 * to XPG).  We clear the other fields just for cleanliness
		 * sake.  We don't want to copy total junk back into the
		 * user's buffer.
		 */
		mutex_unlock(&ps->psm_lock);
		shmds->shm_perm.uid = 0;
		shmds->shm_perm.gid = 0;
		shmds->shm_perm.cuid = ps->psm_perm.cuid;
		shmds->shm_perm.cuid = ps->psm_perm.cgid;
		shmds->shm_perm.mode = 0;
		shmds->shm_perm.seq = 0;
		shmds->shm_perm.key = 0;
        	shmds->shm_segsz = 0;
		shmds->shm_lpid = 0;
		shmds->shm_cpid = ps->psm_cpid;
		shmds->shm_atime = 0;
		shmds->shm_dtime = 0;
		shmds->shm_ctime = 0;
		shmds->shm_nattch = 0;
		shmds->shm_cnattch = 0;
#if _MIPS_SIM == _ABI64
		shmds->shm_osegsz = 0;
#endif
		return(EACCES);
	}

        shmds->shm_perm = ps->psm_perm;
        shmds->shm_segsz = ps->psm_segsz;
	shmds->shm_lpid = ps->psm_lpid;
	shmds->shm_cpid = ps->psm_cpid;
	shmds->shm_atime = ps->psm_atime;
	shmds->shm_dtime = ps->psm_dtime;
	shmds->shm_ctime = ps->psm_ctime;

	*cell = cellid();

	shmds->shm_nattch = as_shmmo_refcnt(ps->psm_mo);
	shmds->shm_cnattch = shmds->shm_nattch;
#if _MIPS_SIM == _ABI64
	shmds->shm_osegsz = (int)ps->psm_segsz;
#endif
	ASSERT(mutex_mine(&ps->psm_lock));
	mutex_unlock(&ps->psm_lock);
	return error;
}

int
pshm_mac_access(bhv_desc_t *bdp, struct cred *cred)
{
	return _MAC_ACCESS(BHV_TO_PSHM(bdp)->psm_maclabel, cred, MACWRITE);
}

/*
 * final destruction of a local vshm
 */
void
pshm_destroy(bhv_desc_t *bdp)
{
	pshm_t *ps = BHV_TO_PSHM(bdp);
	vshm_t *vshm = BHV_TO_VSHM(bdp);

	ASSERT(vshm->vsm_refcnt == 0);
	vshm_freeid(vshm->vsm_id);

	bhv_remove(&vshm->vsm_bh, &ps->psm_bd);
	pshm_free(ps);
	bhv_head_destroy(&vshm->vsm_bh);
	kmem_zone_free(vshm_zone, vshm);

	DELETEVSHM();
}


#ifdef _VCE_AVOIDANCE
/*
 * Just pull our behavior from the vnode and free it.
 * Return that we're doing no caching in case anyone
 * cares.
 */
/*ARGSUSED*/
static int
shmnode_inactive(
	bhv_desc_t	*bdp,
	cred_t		*crp)
{
	vnode_t	*vp;

	vp = BHV_TO_VNODE(bdp);
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
	kmem_zone_free(bhv_global_zone, bdp);

	return VN_INACTIVE_NOCACHE;
}
#endif

#if DEBUG || SHMDEBUG
void
idbg_pshm_print(pshm_t *psp)
{
	qprintf("pshm 0x%x:\n", psp);
	qprintf("    psm_mode 0%o psm_segsz 0x%x psm_mo 0x%x\n",
		psp->psm_perm.mode, psp->psm_segsz, psp->psm_mo.as_mohandle);
	qprintf("    psm_lpid %d psm_cpid %d psm_lock 0x%x\n",
		psp->psm_lpid, psp->psm_cpid, &psp->psm_lock);
}
#endif
