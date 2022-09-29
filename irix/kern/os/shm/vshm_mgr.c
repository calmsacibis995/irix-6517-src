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
#ident "$Id: vshm_mgr.c,v 1.52 1998/10/08 21:13:03 steiner Exp $"

/*
 * Virtual object management for the System V shared memory subsystem
 */

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/capability.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/idbgentry.h>
#include <sys/kmem.h>
#include <ksys/vproc.h>
#include <sys/sema.h>
#include <sys/shm.h>
#include <sys/systm.h>

#include <ksys/vshm.h>
#include "vshm_mgr.h"
#include "vshm_private.h"

/*
 * structure of a hash table entry used to store/lookup vshm structures
 * on a cell
 * Basic locking:
 * By holding lookup_lock for UPDATE permits scanning the entire vshmtab
 *	 w/o holding any VSHMTAB locks (a corollary is that to grab VSHMTAB
 *	for UPDATE one MUST have the lookup_lock for ACCESS or UPDATE)
 *	This also implies that is you have the lookup_lock for UPDATE
 *	then you can insert/delete w/o holding any VSHMTAB locks
 *
 * Reference counting - note that the reference count in the vshm
 * is within a system call only - it has nothing to do with
 * whether anyone has the shm segment mapped - the VM system has
 * that reference count. A reference is enough to hold off a RMID
 * i.e. RMID does nothing except mark that the ID has been removed -
 * it is destroyed when the ref count goes to 0.
 */
int vshmtabsz;			/* size of hash table */
vshmtab_t *vshmtab;		/* The lookup hash table */
int shmid_base;			/* The base id for this node */
int shmid_max;			/* The maximum id for this range */
static int shm_nextid;		/* The next free id to allocate */
static mutex_t shmid_lock;	/* id allocation lock */
mrlock_t vshm_lookup_lock;	/* lookup/create atomicity */
struct zone *vshm_zone;
static char *shm_idtab;		/* track in use ids */
sv_t vshm_syncinit;		/* central sync for in-progress init */

extern int shmmni;

/*#define SHMDEBUG 1 */
#if DEBUG || SHMDEBUG
static void idbg_vshm(__psint_t);
static void idbg_vshm_trace(__psint_t);
int vshm_count;
struct ktrace	*vshm_trace;
int		vshm_trace_id = -1;
#endif

/*
 * Initialize the vshm subsystem. This is called once per cell
 */
void
vshm_init(void)
{
	int		i;
	vshmtab_t	*vq;
	int		shm_idtab_size;

#if DEBUG || SHMDEBUG
	idbg_addfunc("vshm", idbg_vshm);
	idbg_addfunc("vshmtrace", idbg_vshm_trace);
	vshm_trace = ktrace_alloc(1000, 0);
#endif
	ASSERT(vshmtab == 0);

	shm_nextid = 0;
	mutex_init(&shmid_lock, MUTEX_DEFAULT, "shmid allocation");
	mrlock_init(&vshm_lookup_lock, MRLOCK_DEFAULT, "vsl", cellid());

	vshm_mgr_init();

	/*
	 * Allocate and initialize the vshm hash table
	 */
	vshmtabsz = shmmni / 6;
	vshmtab = (vshmtab_t *)kern_malloc(sizeof(vshmtab_t) * vshmtabsz);
	ASSERT(vshmtab != 0);
	for (i = 0; i < vshmtabsz; i++) {
		vq = &vshmtab[i];
		kqueue_init(&vq->vst_queue);
		mrlock_init(&vq->vst_lock, MRLOCK_DEFAULT, "vst",
			    cellid()*vshmtabsz + i);
	}

	/*
	 * allocate shmidtab - keeps track of which id's are in use
	 */
	shm_idtab_size = roundup(((shmmni+7)/8), sizeof(long));
	shm_idtab = kmem_zalloc(shm_idtab_size, KM_SLEEP);

	vshm_zone = kmem_zone_init(sizeof(vshm_t), "Vshm");
	sv_init(&vshm_syncinit, SV_DEFAULT, "vshmsync");
}

/*
 * Allocate a specific shared memory id.  For cell case, need to forward this
 * request to cell owning the id range TBD.
 *
 * Since this is a performing a random access into the table, not
 * going to update shm_nextid.  The worst that could happen is that
 * a collicsion bewteen requested id and vshm_nextid causes one additional
 * iteration in vshm_allocid.
 */
static int
vshm_allocid_requested(int id)
{
	int local_id;
	/*
	 * Check id range here and forward...TBD
	 */
	ASSERT(id >= shmid_base);

	/*
	 * bring id in local range
	 */
	local_id = id - shmid_base;

	mutex_lock(&shmid_lock, PZERO);

	if (!btst(shm_idtab, local_id % shmmni)) {
		bset(shm_idtab, local_id % shmmni);
		mutex_unlock(&shmid_lock);
		return (id);
	}
	mutex_unlock(&shmid_lock);
	return (-1);
}

/*
 * Allocate a new shared memory id. Even though there is not remote
 * communication, this id is guaranteed to be globally unique because
 * the id space has been partitioned between cells
 * Returns < 0 on error;
 */
int
vshm_allocid(int id)
{
	int tries, found;

	if (id >= 0)
		return (vshm_allocid_requested(id));

	mutex_lock(&shmid_lock, PZERO);
	for (tries = 0, found = 0; tries < shmmni; tries++) {
		/*
		 * Allocate the next id in the cell range, wrapping if necessary
		 */
		id = shm_nextid;
		if (++shm_nextid == shmid_max) {
			shm_nextid = 0;
		}
		if (!btst(shm_idtab, id % shmmni)) {
			bset(shm_idtab, id % shmmni);
			found = 1;
			break;
		}
	}
	mutex_unlock(&shmid_lock);
	if (!found)
		return -1;

	/*
	 * Shift the id into the global range
	 */
	id += shmid_base;
	return(id);
}

void
vshm_freeid_local(int id)
{
	int idx;

	ASSERT(id >= shmid_base);
	idx = (id - shmid_base) % shmmni;

	mutex_lock(&shmid_lock, PZERO);
	ASSERT(btst(shm_idtab,idx));
	bclr(shm_idtab,idx);
	mutex_unlock(&shmid_lock);
}

/*
 * Create a new shared memory structure for a given id.
 */
int
vshm_create_id(
	int		key,
	int		id,
	int		flags,
	size_t		size,
	pid_t		pid,
	struct cred	*cred,
	vshm_t		**vshmp)
{
	vshm_t 		*vshm;
	int 		error;

	ASSERT(flags & IPC_CREAT);

	/*
	 * Allocate the virtual object for the shared memory segment
	 * for this cell
	 */
	vshm = kmem_zone_alloc(vshm_zone, KM_SLEEP);
	ADDVSHM();

	/*
	 * Create the physical object. This is the local case.
	 * Initialize behavior head before calling pshm_alloc.
	 */
	bhv_head_init(&vshm->vsm_bh, "vshm");

	/*
	 * Initialize the virtual object structure
	 * We start with a reference of 1. rmid drops the count
	 * and in vshm_rele if it goes to 0 we know that
	 * a) it's not in any hash
	 * b) no one is in progress of referencing it on this cell
	 */
	vshm->vsm_id = id;
	vshm->vsm_key = key;
	vshm->vsm_refcnt = 1;

	error = pshm_alloc(key, flags, size, pid, cred, vshm);
	if (error) {
		VSHM_TRACE4("vshm_create_id", id, "FAILED key", key);
		bhv_head_destroy(&vshm->vsm_bh);
		kmem_zone_free(vshm_zone, vshm);
		DELETEVSHM();
		return(error);
	}

	/*
	 * return success and a pointer to the new virtual shared memory
	 * object 
	 */
	*vshmp = vshm;
	VSHM_TRACE6("vshm_create_id", vshm->vsm_id, "key", key, "vshm", vshm);
	return(0);
}

/*
 * Look for a vshm structure with the given id in a hash table queue
 * The caller must have the hash table entry locked. The vshm returned
 * is referenced
 */
void
vshm_qlookup(
	vshmtab_t	*vq,
	int		id,
	vshm_t		**vshmp)
{
	vshm_t *vshm;
	kqueue_t *kq;

	/* Get the hash queue */
	kq = &vq->vst_queue;
	/*
	 * Step through the hash queue looking for the given id
	 */
	for (vshm = (vshm_t *)kqueue_first(kq);
	     vshm != (vshm_t *)kqueue_end(kq);
	     vshm = (vshm_t *)kqueue_next(&vshm->vsm_queue)) {
		if (id == vshm->vsm_id) {
			/*
			 * Got it. Add reference to vshm and return
			 */
			VSHM_TRACE6("vshm_qlookup", vshm->vsm_id,
				    "key", vshm->vsm_key, "vshm", vshm);
			vshm_ref(vshm);
			*vshmp = vshm;
			return;
		}
	}
	/*
	 * Can't find it, return NULL
	 */
	VSHM_TRACE2("vshm_qlookup failed", id);
	*vshmp = NULL;
}

/*
 * Enter a vshm into the hash table.
 */
void
vshm_enter(vshm_t *vshm, int lock)
{
	vshmtab_t	*vq;

	/* Initial reference */
	vshm_ref(vshm);

	vq = &vshmtab[vshm->vsm_id%vshmtabsz];
	if (lock)
		mrlock(&vshm_lookup_lock, MR_ACCESS, PZERO);
	VSHMTAB_LOCK(vq, MR_UPDATE);
	ASSERT(!vshm_onkqueue(&vq->vst_queue, vshm, vshm->vsm_id));
	kqueue_enter(&vq->vst_queue, &vshm->vsm_queue);
	VSHMTAB_UNLOCK(vq);
	if (lock)
		mrunlock(&vshm_lookup_lock);
}

/*
 * remove a shared memory segment from the lookup table.
 * This is used when and RMID occurs
 * This calls needs to be serialized so that only one thread does the clearid
 * since once the key is removed, another thread could be creating another
 * shm segment with the same key.
 */
/* ARGSUSED */
void
vshm_remove(vshm_t *vshm, int clearid)
{
	vshmtab_t	*vq;

	vq = &vshmtab[vshm->vsm_id%vshmtabsz];
	/*
	 * Lock out global scans and shmgets
	 */
	mrlock(&vshm_lookup_lock, MR_ACCESS, PZERO);
	/*
	 * Indicate the key is no longer here
	 * We do this first so that new shmget's won't find a key but not
	 * the entry.
	 */
	if (clearid && (vshm->vsm_key != IPC_PRIVATE))
		vshm_idclear(vshm->vsm_key);
	/*
	 * lock out lookup scans and create/destroys
	 */
	VSHMTAB_LOCK(vq, MR_UPDATE);
	VSHM_TRACE4("vshm_remove", vshm->vsm_id, "vshm", vshm);
	ASSERT(!kqueue_isnull(&vshm->vsm_queue));
	kqueue_remove(&vshm->vsm_queue);
	kqueue_null(&vshm->vsm_queue);
	VSHMTAB_UNLOCK(vq);
	mrunlock(&vshm_lookup_lock);

	/* release initial reference */
	vshm_rele(vshm);
}

int
vshm_is_removed(vshm_t *vshm)
{
	return kqueue_isnull(&vshm->vsm_queue);
}

/*
 * Find a vshm structure with a given key.
 * The tricky/ugly part is that atomic lookup/create may be required
 * so we must effectively single thread all key-lookup/creates (shmget).
 * flags comes from shmget - consists of mode and IPC_CREAT
 */
int
vshm_lookup_key(
	key_t		key,
	int		flags,
	size_t		size,
	pid_t		pid,
	struct cred	*cred,
	vshm_t		**vshmp,
	int		id)
{
	vshm_t		*vshm;
	kqueue_t	*kq;
	int		i;
	int		error;
	vshmtab_t	*vq;

	ASSERT(key != IPC_PRIVATE);
again:
	mrlock(&vshm_lookup_lock, MR_UPDATE, PZERO);

	/*
	 * Step through the vshm table looking for an existing 
	 * vshm with the correct key
	 */
	for (i = 0, vq = &vshmtab[0]; i < vshmtabsz; i++, vq++) {
		kq = &vq->vst_queue;
		/*
		 * Step through each entry in the hash queue looking
		 * for the key
		 */
		for (vshm = (vshm_t *)kqueue_first(kq);
		     vshm != (vshm_t *)kqueue_end(kq);
		     vshm = (vshm_t *)kqueue_next(&vshm->vsm_queue)) {
			if (vshm == 0) {
				printf("vshmq corrupt, idx %i, kq 0x%x\n",
						i, kq);
				panic("vshm_lookup_key");
			}
			if (key == vshm->vsm_key) {

				vshm_ref(vshm);
				mrunlock(&vshm_lookup_lock);

				/*
				 * Got an active key. Check it is what we want
				 */
				VSHM_KEYCHECK(vshm, cred, flags, size, error);

				if (error) {
					vshm_rele(vshm);
					return(error);
				}

				/* Got it. The vshm is returned */
				*vshmp = vshm;
				return(0);
			}
		}
	}

	/*
	 * There is no existing vshm (on this cell) 
	 * Create one if required. As the lookup
	 * lock is held we can do this without a race with another
	 * lookup for the same key
	 */
	error = vshm_create(key, flags, size, pid, cred, vshmp, id);
	/* vshm_create unlocks lookup_lock always. */
	if (error == EAGAIN) {
		/* we instantiated a DC for an existing shm segment
		 * we should loop back (and find it this time) and
		 * check requisite permissions
		 */
		goto again;
	}
	return(error);
}

/*
 * Find a vshm structure with a given id.
 * Return unlocked and referenced vshm
 * This function only searches the local cell's vshmtab
 */
int
vshm_lookup_id_local(int id, vshm_t **vshmp)
{
	vshmtab_t	*vq;
	vshm_t		*vshm;

	*vshmp = NULL;

	if (id < 0)
		return(EINVAL);

	mrlock(&vshm_lookup_lock, MR_ACCESS, PZERO);

	/*
	 * Look for the id in the appropriate hash queue.
	 * Returns with the vshm referenced (or NULL)
	 */
	vq = &vshmtab[id%vshmtabsz];
	VSHMTAB_LOCK(vq, MR_ACCESS);
	vshm_qlookup(vq, id, &vshm);
	VSHMTAB_UNLOCK(vq);
	mrunlock(&vshm_lookup_lock);

	if (vshm == NULL)
		return(ENOENT);

	*vshmp = vshm;
	return(0);
}

void
vshm_ref(vshm_t *vshm)
{
	atomicAddInt(&vshm->vsm_refcnt, 1);
}

void
vshm_rele(vshm_t *vshm)
{
	ASSERT(vshm->vsm_refcnt > 0);
	if (atomicAddInt(&vshm->vsm_refcnt, -1) == 0) {
		/*
		 * Last use - tear down.
		 * At this point vshm can't be in hash and
		 * noone can be referencing it
		 */
		VSHM_DESTROY(vshm);
	}
}

/*
 * Iterate through all the shared memory descriptors know about on this
 * cell calling function 'func(vshm, arg)' with each.
 * This isn't great - holds lookup_lock across the
 * function call ...
 * XXX careful when calling this - we can't grab the vshm lock
 * since we need to hold onto the lookup_lock - we do grab
 * a reference to the vshm, so it won't go away. However if the
 * arbitrary 'func' calls some VSHM operations, they will be
 * called w/o the vshm locked... (currently only GETREGION is called).
 */
void
vshm_iterate(
	void (*func)(vshm_t *, void *),
	void *	arg)
{
	kqueue_t	*kq;
	int		i;
	vshmtab_t	*vq;
	vshm_t		*vshm;

	mrlock(&vshm_lookup_lock, MR_UPDATE, PZERO);

	/*
	 * for every element in the hash table
	 */
	for (i = 0; i < vshmtabsz; i++) {
		vq = &vshmtab[i];
		kq = &vq->vst_queue;
		/*
		 * for every element on this hash queue
		 */
		for (vshm = (vshm_t *)kqueue_first(kq);
		     vshm != (vshm_t *)kqueue_end(kq);
		     vshm = (vshm_t *)kqueue_next(&vshm->vsm_queue)) {
			vshm_ref(vshm);
			(*func)(vshm, arg);
			vshm_rele(vshm);
		}
	}
	mrunlock(&vshm_lookup_lock);
}

int
vshm_getstatus_local(struct shmstat *stat, struct cred *cred)
{
	kqueue_t	*kq;
	int		found;
	unsigned	loc = 0;
	vshmtab_t	*vq;
	vshm_t		*vshm;
	bhv_desc_t	*bdp;
	int		error;

	mrlock(&vshm_lookup_lock, MR_UPDATE, PZERO);

	/*
	 * start by looking for 'id' in the location passed to us.
	 * If we don't find it, start at first of chain, else return
	 * next in line
	 * For the first search in each cell, sh_id is the first id
	 * to look for and sh_location is -1.
	 */
	if (stat->sh_location != -1LL) {
		/*
		 * start where we left off
		 * The 'sh_location' field is the index into our vshmtab table,
		 * and sh_id is the last id we gave back.
		 */
		loc = (unsigned)stat->sh_location;
		if (loc >= vshmtabsz)
			loc = 0;
		vq = &vshmtab[loc];
		kq = &vq->vst_queue;
		for (found = 0, vshm = (vshm_t *)kqueue_first(kq);
		     vshm != (vshm_t *)kqueue_end(kq);
		     vshm = (vshm_t *)kqueue_next(&vshm->vsm_queue)) {
			if (found) {
				bdp = VSHM_TO_FIRST_BHV(vshm);
				if (vshm_islocal(bdp))
					goto ret;
			}
			if (vshm->vsm_id == stat->sh_id) {
				/* found last returned - return next one */
				found = 1;
			}
		}
		/*
		 * either didn't find passed 'id' or 'id' was last on previous
		 * chain
		 */
		if (found) {
			/* we found the id but ran out of the last chain - go
			 * onto next chain
			 */
			loc++;
		}
	}

	for (; loc < vshmtabsz; loc++) {
		vq = &vshmtab[loc];
		kq = &vq->vst_queue;
		/*
		 * for every element on this hash queue
		 */
		for (vshm = (vshm_t *)kqueue_first(kq);
		     vshm != (vshm_t *)kqueue_end(kq);
		     vshm = (vshm_t *)kqueue_next(&vshm->vsm_queue)) {
			bdp = VSHM_TO_FIRST_BHV(vshm);
			if (vshm_islocal(bdp)) {
ret:
				vshm_ref(vshm);
				mrunlock(&vshm_lookup_lock);

				/* might get error if non-SU is trying to stat */
				VSHM_GETSTAT(vshm, cred,
					     &stat->sh_shmds, &stat->sh_cell,
					     error);
				stat->sh_id = vshm->vsm_id;
				stat->sh_location = loc;
				vshm_rele(vshm);
				return(error);
			}
		}
	}
	mrunlock(&vshm_lookup_lock);
	return(ESRCH);
}

#if DEBUG || SHMDEBUG
int
vshm_onkqueue(kqueue_t *kq, vshm_t *tv, int id)
{
	vshm_t *vshm;
	for (vshm = (vshm_t *)kqueue_first(kq);
	     vshm != (vshm_t *)kqueue_end(kq);
	     vshm = (vshm_t *)kqueue_next(&vshm->vsm_queue)) {
		if (vshm == tv || vshm->vsm_id == id)
			return 1;
	}
	return 0;
}

static void
vshmprint(vshm_t *vshm, vshmtab_t *vq)
{
	qprintf("vshm @0x%x:\n", vshm);
	qprintf("    id %d key 0x%w32x ref %d vq 0x%x\n",
		vshm->vsm_id, vshm->vsm_key, vshm->vsm_refcnt, vq);
	qprintf("    behavior head 0x%x:\n", &vshm->vsm_bh);
	idbg_vshm_bhv_print(vshm);
}

static void
idbg_vshm(__psint_t x)
{
	kqueue_t	*kq;
	int		i;
	vshmtab_t	*vq;
	vshm_t		*vshm;

	if (x == -1) {
#if CELL
		qprintf("Dumping vshm table for cell %d\n", cellid());
#endif
		for (i = 0; i < vshmtabsz; i++) {
			vq = &vshmtab[i];
			kq = &vq->vst_queue;
			/*
			 * for every element on this hash queue
			 */
			for (vshm = (vshm_t *)kqueue_first(kq);
			     vshm != (vshm_t *)kqueue_end(kq);
			     vshm = (vshm_t *)kqueue_next(&vshm->vsm_queue)) {
				vshmprint(vshm, vq);
			}
		}
	} else if (x < 0) {
		vshmprint((vshm_t *)x, NULL);
	}
}

static void
idbg_vshm_trace(__psint_t x)
{
	__psint_t	id;
	int		idx;
	int		count;

	if (x == -1) {
		qprintf("Displaying all entries\n");
		idx = -1;
		count = 0;
	} else if (x < 0) {
		idx = -1;
		count = (int)-x;
		qprintf("Displaying last %d entries\n", count);
	} else {
		qprintf("Displaying entries for id %d\n", x);
		idx = 1;
		id = x;
		count = 0;
	}
		
	ktrace_print_buffer(vshm_trace, id, idx, count);
}
#endif /* DEBUG || SHMDEBUG */
