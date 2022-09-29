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
#ident "$Id: vshm_mgr_lp.c,v 1.16 1997/04/04 18:40:37 len Exp $"

/*
 * vshm manager routines - LOCAL versions.
 * Each of these functions has a twin in vshm_mgr_dp.c which is for the
 * cell case
 */

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/capability.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/idbgentry.h>
#include <sys/ipc.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/shm.h>
#include <sys/systm.h>
#include <ksys/vshm.h>

#include "vshm_mgr.h"
#include "vshm_private.h"

void
vshm_mgr_init(void)
{
	shmid_base = 0;
	shmid_max = SHM_IDMAX;
}

/*
 * Create a new vshm structure for a given key.
 * It assumed that either:
 * a) it's an IPC_PRIVATE key and we should create a new vshm regardless
 *	 in this case the lookup_lock doesn't have to be held
 * b) it's a key that doesn't exist yet and we want to atomically lookup/create
 *	it. In this case the lookup_lock must be held for UPDATE.
 * Note that for case b) that the key may already be existent on another
 * cell, so this can turn into a lookup.
 *
 * Always returns with lookup_lock unlocked if it was locked.
 */
int
vshm_create(
	int		key,
	int		flags,
	size_t		size,
	pid_t		pid,
	struct cred	*cred,
	vshm_t		**vshmp,
	int		id)
{
	int		error;

	/*
	 * Check that it should actually be created
	 */
	if (!(flags & IPC_CREAT)) {
		if (key != IPC_PRIVATE)
			mrunlock(&vshm_lookup_lock);
		return ENOENT;
	}
	if ((id = vshm_allocid(id)) < 0) {
		if (key != IPC_PRIVATE)
			mrunlock(&vshm_lookup_lock);
		return ENOSPC;
	}

	/*
	 * must have lookup_lock locked somehow it we are going to
	 * try for a VSHMTAB_LOCK for UPDATE
	 */
	if (key == IPC_PRIVATE) {
		mrlock(&vshm_lookup_lock, MR_ACCESS, PZERO);
	} else
		ASSERT(ismrlocked(&vshm_lookup_lock, MR_UPDATE));

	VSHM_TRACE4("vshm_create", id, "key", key);
	/*
	 * now create the vshm for the new id. If all goes well,
	 * lock the new vshm to give the caller exclusive access,
	 * add the new object into the hash list, unlock the hash
	 * queue and return.
	 */
	error = vshm_create_id(key, id, flags, size, pid, cred, vshmp);
	if (!error)
		vshm_enter(*vshmp, B_FALSE);
	else
		vshm_freeid(id);
	mrunlock(&vshm_lookup_lock);
	return(error);
}

/*
 * Find a vshm structure with a given id.
 * Return unlocked and referenced vshm
 */
int
vshm_lookup_id(int id, vshm_t **vshmp)
{
	int error;

	error = vshm_lookup_id_local(id, vshmp);
	if (error == ENOENT)
		error = EINVAL;
	return error;
}

/*
 * used by ipcs to swing through all id's
 */
int
vshm_getstatus(struct shmstat *stat, struct cred *cred)
{
	int error;

	/*
	 * Skip over shm segments that we have no access to.
	 * If cpid has been set, it means that we have no
	 * access but that we are the creator of this segment.
	 */
	while (error = vshm_getstatus_local(stat, cred)) {
		if ((error != EACCES) ||
	           (stat->sh_shmds.shm_cpid != NOPID))
			break;
	}

	return(error);
}

/* ARGSUSED */
int
vshm_idclear(key_t key)
{
	return(0);
}

void
vshm_freeid(int id)
{
	vshm_freeid_local(id);
}

int
vshm_islocal(bhv_desc_t *bdp)
{
	return (BHV_OPS(bdp) == &pshm_ops);
}

/* ARGSUSED */
int
pshm_push(bhv_desc_t *bdp, cell_t to)
{
	return 0;
}

#if DEBUG || SHMDEBUG
void
idbg_vshm_bhv_print(vshm_t *vsp)
{
	bhv_desc_t	*bdp = VSHM_TO_FIRST_BHV(vsp);
	if (BHV_POSITION(bdp) == VSHM_POSITION_PSHM)
		idbg_pshm_print(BHV_PDATA(bdp));
	else
		qprintf("Unknown behavior\n");
}
#endif
