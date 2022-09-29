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
#ident "$Id: vshm_mgr_dp.c,v 1.27 1997/08/27 21:39:02 sp Exp $"

/*
 * vshm manager routines - CELL versions.
 * Each of these functions has a twin in vshm_mgr_lp.c which is for the
 * local case
 */

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/capability.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/kmem.h>
#include <sys/mac_label.h>
#include <sys/sema.h>
#include <sys/shm.h>
#include <sys/systm.h>
#include <sys/idbgentry.h>

#include <ksys/cell.h>
#include <ksys/cell/wp.h>
#include <ksys/vshm.h>

#include "dshm.h"
#include "vshm_mgr.h"
#include "vshm_private.h"
#include "vshm_migrate.h"

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dcshm_stubs.h"
#include "I_dsshm_stubs.h"

static int vshm_keyregister(key_t, int *);

static service_t svipc_service;	/* name of the service on this node */
static wp_domain_t shm_wp_keydomain; /* wp database for keys */
wp_domain_t shm_wp_iddomain;	/* wp database for ids */

/*#define IDTEST 1*/
#if IDTEST
#undef SHM_IDMAX
#define SHM_IDMAX (1024 * 2000)
#endif

void
vshm_mgr_init(void)
{
	int ids_per_cell;

	ids_per_cell = SHM_IDMAX / MAX_CELLS;
	shmid_base = ids_per_cell * cellid();
	shmid_max = ids_per_cell;
	/*
	 * Find the wp database names
	 */
	wp_domain_create(SHM_WP_KEYDOMAIN, &shm_wp_keydomain);
	wp_domain_create(SHM_WP_IDDOMAIN, &shm_wp_iddomain);

	SERVICE_MAKE(svipc_service, cellid(), SVC_SVIPC);

	/*
	 * Register our id range
	 */
	{
		ulong_t		oldbase;
		ulong_t		oldmax;
		wp_value_t	oldvalue;
		int		error;
	
		error = wp_register(shm_wp_iddomain, shmid_base, shmid_max,
			SERVICE_TO_WP_VALUE(svipc_service), &oldbase, &oldmax,
			&oldvalue);
		if (error)
			panic("vshm_mgr_init: bad registration");
	}

	vshm_obj_init();

	mesg_handler_register(dcshm_msg_dispatcher, DCSHM_SUBSYSID);
	mesg_handler_register(dsshm_msg_dispatcher, DSSHM_SUBSYSID);
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
	key_t		key,
	int		flags,
	size_t		size,
	pid_t		pid,
	struct cred	*cred,
	vshm_t		**vshmp,
	int		id)
{
	int		error;
	int		newid;

	/*
	 * must have lookup_lock locked somehow if we are going to
	 * try for a VSHMTAB_LOCK for UPDATE
	 * If we're not going to create then we could just have lookup lock
	 * for ACCESS
	 */
	if (key == IPC_PRIVATE)
		mrlock(&vshm_lookup_lock, MR_ACCESS, PZERO);
	ASSERT(ismrlocked(&vshm_lookup_lock, MR_ACCESS|MR_UPDATE));
	ASSERT((key != IPC_PRIVATE) || (flags & IPC_CREAT));

	/*
	 * if we're not going to create - just lookup to see
	 * if someone else has already registered key
	 * Note that IPC_PRIVATE always has the IPC_CREAT flag set
	 */
	if (!(flags & IPC_CREAT)) {
		wp_value_t value;
		if (wp_lookup(shm_wp_keydomain, (ulong_t)key, &value)) {
			mrunlock(&vshm_lookup_lock);
			return ENOENT;
		}
		id = (int)value;
	} else {
		/* Get a new id */
		id = newid = vshm_allocid(id);
		if (id < 0) {
			mrunlock(&vshm_lookup_lock);
			return ENOSPC;
		}
	}

	/*
	 * Determine if the key should be managed on this cell
	 */
	if ((key == IPC_PRIVATE) ||
			((flags & IPC_CREAT) && vshm_keyregister(key, &id))) {
		ASSERT(id == newid);
		/*
		 * If the register succeeds we know that not only
		 * must we create the structure on this cell but
		 * also that only this thread will do it (the
		 * registration will fail for other threads on this cell too
		 */
		VSHM_TRACE4("vshm_create", id, "key", key);
		/*
		 * now create the vshm for the new id. If all goes well,
		 * add a ref to the new vshm, and
		 * add the new object into the hash list
		 */
		error = vshm_create_id(key, id, flags, size, pid, cred, vshmp);
		if (!error) {
			vshm_enter(*vshmp, B_FALSE);
		} else {
			if (key != IPC_PRIVATE)
				/* error creating - remove key */
				vshm_idclear(key);
			vshm_freeid(newid);
		}
		mrunlock(&vshm_lookup_lock);
		return(error);
	}
	if (flags & IPC_CREAT) {
		/* don't need id */
		ASSERT(newid != id);
		vshm_freeid(newid);
	}

	/*
	 * either:
	 * a) key currently being managed by another server
	 * b) key currently being set up by another thread on this cell
	 * But another thread on this server would have the lookup_lock
	 * for UPDATE so there is no way we could get here.
	 */
	ASSERT(vshmid_to_server_cell(id) != cellid());
	/*
	 * managed by another cell ..
	 * This means that we simply need to do a lookup based on the id
	 * It seems fair to release the lookup_lock here - we don't
	 * want to hold it across a long operation and this is
	 * no longer an atomic operation.
	 * 2 things to remember:
	 * a) once we release the lock the id could go away and lookup_id
	 *	might return an error.
	 * b) once we get the vshm we need to verify the permissions.
	 */
	mrunlock(&vshm_lookup_lock);
	error = vshm_lookup_id(id, vshmp);
	if (error == 0) {
		/*
		 * we now have a DC, we need vshm_lookup_key to
		 * check permissions
		 * We don't really want the reference - vshm_lookup_key
		 * will get one for us if all works out
		 */
		vshm_rele(*vshmp);
		return EAGAIN;
	}
	/*
	 * failed - likely due to the server having done an RMID
	 * This had better be the ONLY reason ...
	 * Ok then, start over and try again to atomically add this key
	 */
	return EAGAIN;
}

/*
 * Find a vshm structure with a given id.
 * Return unlocked and referenced vshm
 * This function will search all cells
 */

int
vshm_lookup_id(int id, vshm_t **vshmp)
{
	int error;
	wp_value_t value;
	service_t svc;
	vshm_t *vshm;
	int retries = 0;

retry:
	error = vshm_lookup_id_local(id, vshmp);
	if (error != ENOENT)
		return error;

	/*
	 * don't have it - if we're the server for the id then
	 * that is the authoritive answer else ask the appropriate server
	 */
	if (wp_lookup(shm_wp_iddomain, (ulong_t)id, &value))
		return EINVAL;

	SERVICE_FROMWP_VALUE(svc, value);
	if (SERVICE_TO_CELL(svc) == cellid()) {
		/* its us! */
		return EINVAL;
	}

	/*
	 * we basically simply attempt to create a client version
	 * this will fail if the server doesn't know about such an id
	 * This will also fail if the server believes we already have
	 * this client set up - this occurs in the case where 2 threads
	 * on one cell go after the same id at the same time.
	 */
	error = dcshm_create(id, svc, vshmp);
	if (error == EAGAIN) {
		cell_backoff(++retries);
		goto retry;
	} else if (error) {
		return error;
	}

	/* got it - link into our table */
	vshm = *vshmp;
	vshm_enter(vshm, B_TRUE);

	return 0;
}

/*
 * used by ipcs to swing through all id's
 * In this case we don't want to instantiate DCs for all valid
 * id's - so we function ship the entire operation in the cell case.
 */
int
vshm_getstatus(struct shmstat *stat, struct cred *cred)
{
	int error;
	service_t service;
	wp_value_t value;
	ulong_t base, range;

	for (;;) {
		if (wp_lookup(shm_wp_iddomain, (ulong_t)stat->sh_id, &value) == 0) {
			SERVICE_FROMWP_VALUE(service, value);
			if (SERVICE_TO_CELL(service) == cellid())
				error = vshm_getstatus_local(stat, cred);
			else
				error = vshm_getstatus_remote(service, cred, stat);

			if (error == 0)
				return 0;

			/*
			 * Special handling for the case where we created the 
			 * shm segment but have no access to it.
			 */
			if ((error == EACCES) && (stat->sh_shmds.shm_cpid != NOPID))
				return(EACCES);
		}

		/*
		 * id's in this range aren't handled or range
		 * has been completed - ask for next range
		 */
		base = stat->sh_id;
		range = 1;
		error = wp_getnext(shm_wp_iddomain, &base, &range, &value);
		if (error)
			return ESRCH;

		stat->sh_id = base;
		stat->sh_location = -1LL;
	}
	/* NOTREACHED */
}

/*
 * Try and register that this cell will be the server for a key. If this
 * succeeds the caller knows that no other cell will attempt to create 
 * a physical object for this key. If it fails then return the name of
 * the service that is managing the segment already.
 */
static int
vshm_keyregister(key_t key, int *id)
{
	ulong_t		base;
	ulong_t		range;
	wp_value_t	value;

	/*
	 * Try and register with white pages
	 */
	if (wp_register(shm_wp_keydomain, (ulong_t)key, 1, (ulong_t)*id,
			&base, &range, &value)) {
		VSHM_TRACE6("vshm_keyregister", *id, "failed key", key,
			    "existing id", value);
		*id = (int)value;
		return 0;
	} else {
		VSHM_TRACE4("vshm_keyregister", *id, "key", key);
		return 1;
	}
}

int
vshm_idclear(key_t key)
{
	int	error;

	error = wp_clear(shm_wp_keydomain, (ulong_t)key, 1);
	ASSERT(error == 0);
	return(error);
}

void
vshm_freeid(int id)
{ 
	/* REFERENCED */
	int		error;
	wp_value_t	value;
	service_t	svc;

	error = wp_lookup(shm_wp_iddomain, (ulong_t)id, &value);
	ASSERT(error == 0);

	SERVICE_FROMWP_VALUE(svc, value);
	if (SERVICE_TO_CELL(svc) == cellid())
		vshm_freeid_local(id);
	else
		vshm_freeid_remote(svc, id);
}

/*
 * translate an id to the cell that serves it.
 * returns cell_t(-1) if there is noone registered to handle this id
 */
cell_t
vshmid_to_server_cell(int id)
{
	wp_value_t value;
	service_t svc;

	if (wp_lookup(shm_wp_iddomain, (ulong_t)id, &value))
		return((cell_t)-1);
	SERVICE_FROMWP_VALUE(svc, value);
	return SERVICE_TO_CELL(svc);
}

int
vshm_islocal(bhv_desc_t *bdp)
{
	return (BHV_OPS(bdp) == &pshm_ops) || (BHV_OPS(bdp) == &dsshm_ops);
}

#if defined(DEBUG) || defined(SHMDEBUG)
void
idbg_vshm_bhv_print(
	vshm_t	*vsp)
{
	bhv_desc_t	*bdp = VSHM_TO_FIRST_BHV(vsp);

	switch (BHV_POSITION(bdp)) {
	case VSHM_POSITION_DS:
		idbg_dsshm_print(BHV_PDATA(bdp));
		return;
	case VSHM_POSITION_DC:
		idbg_dcshm_print(BHV_PDATA(bdp));
		return;
	case VSHM_POSITION_PSHM:
		idbg_pshm_print(BHV_PDATA(bdp));
		return;
	default:
		qprintf("Unknown behavior position %d\n", BHV_POSITION(bdp));
	}
}
#endif
