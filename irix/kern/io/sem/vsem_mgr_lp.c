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
#ident "$Id: vsem_mgr_lp.c,v 1.1 1997/07/30 22:19:29 sp Exp $"

/*
 * vsem manager routines - LOCAL versions.
 * Each of these functions has a twin in vsem_mgr_dp.c which is for the
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
#include <sys/sem.h>
#include <sys/systm.h>
#include <ksys/vsem.h>

#include "vsem_mgr.h"
#include "vsem_private.h"

void
vsem_mgr_init(void)
{
	semid_base = 0;
	semid_max = SEM_IDMAX;
}

/*
 * Create a new vsem structure for a given key.
 * It assumed that either:
 * a) it's an IPC_PRIVATE key and we should create a new vsem regardless
 *	 in this case the lookup_lock doesn't have to be held
 * b) it's a key that doesn't exist yet and we want to atomically lookup/create
 *	it. In this case the lookup_lock must be held for UPDATE.
 * Note that for case b) that the key may already be existent on another
 * cell, so this can turn into a lookup.
 *
 * Always returns with lookup_lock unlocked if it was locked.
 */
int
vsem_create(
	int		key,
	int		flags,
	int		nsems,
	struct cred	*cred,
	vsem_t		**vsemp)
{
	int		error;
	int		id;

	/*
	 * Check that it should actually be created
	 */
	if (!(flags & IPC_CREAT)) {
		if (key != IPC_PRIVATE)
			mrunlock(&vsem_lookup_lock);
		return ENOENT;
	}
	if ((id = vsem_allocid()) < 0) {
		if (key != IPC_PRIVATE)
			mrunlock(&vsem_lookup_lock);
		return ENOSPC;
	}

	/*
	 * must have lookup_lock locked somehow it we are going to
	 * try for a VSEMTAB_LOCK for UPDATE
	 */
	if (key == IPC_PRIVATE) {
		mrlock(&vsem_lookup_lock, MR_ACCESS, PZERO);
	} else
		ASSERT(ismrlocked(&vsem_lookup_lock, MR_UPDATE));

	VSEM_TRACE4("vsem_create", id, "key", key);
	/*
	 * now create the vsem for the new id. If all goes well,
	 * lock the new vsem to give the caller exclusive access,
	 * add the new object into the hash list, unlock the hash
	 * queue and return.
	 */
	error = vsem_create_id(key, id, flags, nsems, cred, vsemp);
	if (!error)
		vsem_enter(*vsemp, B_FALSE);
	else
		vsem_freeid(id);
	mrunlock(&vsem_lookup_lock);
	return(error);
}

/*
 * Find a vsem structure with a given id.
 * Return unlocked and referenced vsem
 */
int
vsem_lookup_id(
	int		id,
	vsem_t		**vsemp)
{
	int		error;

	error = vsem_lookup_id_local(id, vsemp);
	if (error == ENOENT)
		error = EINVAL;
	return error;
}

/*
 * used by ipcs to swing through all id's
 */
int
vsem_getstatus(
	struct semstat	*stat,
	struct cred	*cred)
{
	int 		error;

	/*
	 * Skip over sem that we have no access to.
	 */
	while (error = vsem_getstatus_local(stat, cred)) {
		if (error != EACCES)
			break;
	}

	return(error);
}

/* ARGSUSED */
int
vsem_idclear(
	key_t		key)
{
	return(0);
}

void
vsem_freeid(
	int		id)
{
	vsem_freeid_local(id);
}

int
vsem_islocal(
	bhv_desc_t	*bdp)
{
	return (BHV_OPS(bdp) == &psem_ops);
}

#if DEBUG || SEMDEBUG
void
idbg_vsem_bhv_print(
	vsem_t		*vsp)
{
	bhv_desc_t	*bdp;

	bdp = VSEM_TO_FIRST_BHV(vsp);
	if (BHV_POSITION(bdp) == VSEM_POSITION_PSEM)
		idbg_psem_print(BHV_PDATA(bdp));
	else
		qprintf("Unknown behavior\n");
}
#endif
