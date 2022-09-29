/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: vsem_mgr.c,v 1.1 1997/07/30 22:19:25 sp Exp $"

/*
 * Virtual object management for the System V semaphore subsystem
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/idbgentry.h>
#include <sys/sem.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/atomic_ops.h>

#include <ksys/vsem.h>
#include "vsem_mgr.h"
#include "vsem_private.h"

/*
 * structure of a hash table entry used to store/lookup vsem structures
 * on a cell
 * Basic locking:
 * By holding lookup_lock for UPDATE permits scanning the entire vsemtab
 *	 w/o holding any VSHMTAB locks (a corollary is that to grab VSHMTAB
 *	for UPDATE one MUST have the lookup_lock for ACCESS or UPDATE)
 *	This also implies that is you have the lookup_lock for UPDATE
 *	then you can insert/delete w/o holding any VSHMTAB locks
 *
 * Reference counting - note that the reference count in the vsem
 * is within a system call only. A reference is enough to hold off a RMID
 * i.e. RMID does nothing except mark that the ID has been removed -
 * it is destroyed when the ref count goes to 0.
 */
int		vsemtabsz;		/* size of hash table */
vsemtab_t	*vsemtab;		/* The lookup hash table */
int		semid_base;		/* The base id for this node */
int		semid_max;		/* The maximum id for this range */
static int	sem_nextid;		/* The next free id to allocate */
static mutex_t	semid_lock;		/* id allocation lock */
mrlock_t	vsem_lookup_lock;	/* lookup/create atomicity */
struct zone	*vsem_zone;
static char	*sem_idtab;		/* track in use ids */

extern int semmni;

/*#define SEMDEBUG 1 */
#if DEBUG || SEMDEBUG
static void idbg_vsem(__psint_t);
static void idbg_vsem_trace(__psint_t);
struct ktrace	*vsem_trace;
int		vsem_trace_id = -1;
#endif

/*
 * Initialize the vsem subsystem. This is called once per cell
 */
void
vsem_init(void)
{
	int		i;
	vsemtab_t	*vq;

#if DEBUG || SEMDEBUG
	idbg_addfunc("vsem", idbg_vsem);
	idbg_addfunc("vsemtrace", idbg_vsem_trace);
	vsem_trace = ktrace_alloc(1000, 0);
#endif
	ASSERT(vsemtab == 0);

	sem_nextid = 0;
	mutex_init(&semid_lock, MUTEX_DEFAULT, "semid allocation");
	mrlock_init(&vsem_lookup_lock, MRLOCK_DEFAULT, "vsl", cellid());

	vsem_mgr_init();

	/*
	 * Allocate and initialize the vsem hash table
	 */
	vsemtabsz = semmni / 6;
	vsemtab = kmem_alloc(sizeof(vsemtab_t) * vsemtabsz, KM_SLEEP);
	ASSERT(vsemtab != 0);
	for (i = 0; i < vsemtabsz; i++) {
		vq = &vsemtab[i];
		kqueue_init(&vq->vst_queue);
		mrlock_init(&vq->vst_lock, MRLOCK_DEFAULT, "vst",
			    cellid()*vsemtabsz + i);
	}

	/*
	 * allocate semidtab - keeps track of which id's are in use
	 */
	sem_idtab = kmem_zalloc(sizeof(char) * semmni, KM_SLEEP);

	vsem_zone = kmem_zone_init(sizeof(vsem_t), "Vsem");
}

void
vsem_ref(
	vsem_t		*vsem)
{
	atomicAddInt(&vsem->vsm_refcnt, 1);
}

void
vsem_rele(
	vsem_t		*vsem)
{
	ASSERT(vsem->vsm_refcnt > 0);
	if (atomicAddInt(&vsem->vsm_refcnt, -1) == 0) {
		/*
		 * Last use - tear down.
		 * At this point vsem can't be in hash and
		 * noone can be referencing it
		 */
		VSEM_DESTROY(vsem);
	}
}

/*
 * Allocate a new shared memory id. Even though there is not remote
 * communication, this id is guaranteed to be globally unique because
 * the id space has been partitioned between cells
 * Returns < 0 on error;
 */
int
vsem_allocid()
{
	int 		tries;
	int 		found;
	int		id;

	mutex_lock(&semid_lock, PZERO);
	for (tries = 0, found = 0; tries < semmni; tries++) {
		/*
		 * Allocate the next id in the cell range, wrapping if necessary
		 */
		id = sem_nextid;
		if (++sem_nextid == semid_max) {
			sem_nextid = 0;
		}
		if (sem_idtab[id % semmni] == 0) {
			sem_idtab[id % semmni] = 1;
			found = 1;
			break;
		}
	}
	mutex_unlock(&semid_lock);
	if (!found)
		return -1;

	/*
	 * Shift the id into the global range
	 */
	id += semid_base;
	return(id);
}

void
vsem_freeid_local(
	int		id)
{
	int 		idx;

	ASSERT(id >= semid_base);
	idx = (id - semid_base) % semmni;
	ASSERT(sem_idtab[idx]);
	sem_idtab[idx] = 0;
}

/*
 * Create a new shared memory structure for a given id.
 */
int
vsem_create_id(
	int		key,
	int		id,
	int		flags,
	int		nsems,
	struct cred	*cred,
	vsem_t		**vsemp)
{
	vsem_t 		*vsem;
	int 		error;

	ASSERT(flags & IPC_CREAT);

	/*
	 * Allocate the virtual object for the semaphore for this cell
	 */
	vsem = kmem_zone_alloc(vsem_zone, KM_SLEEP);

	/*
	 * Create the physical object. This is the local case.
	 * Initialize behavior head before calling psem_alloc.
	 */
	bhv_head_init(&vsem->vsm_bh, "vsem");

	/*
	 * Initialize the virtual object structure
	 * We start with a reference of 1. rmid drops the count
	 * and in vsem_rele if it goes to 0 we know that
	 * a) it's not in any hash
	 * b) no one is in progress of referencing it on this cell
	 */
	vsem->vsm_id = id;
	vsem->vsm_key = key;
	vsem->vsm_refcnt = 1;
	vsem->vsm_nsems = nsems;

	error = psem_alloc(key, flags, nsems, cred, vsem);
	if (error) {
		VSEM_TRACE4("vsem_create_id", id, "FAILED key", key);
		bhv_head_destroy(&vsem->vsm_bh);
		kmem_zone_free(vsem_zone, vsem);
		return(error);
	}

	/*
	 * return success and a pointer to the new virtual shared memory
	 * object 
	 */
	*vsemp = vsem;
	VSEM_TRACE6("vsem_create_id", vsem->vsm_id, "key", key, "vsem", vsem);
	return(0);
}

/*
 * Look for a vsem structure with the given id in a hash table queue
 * The caller must have the hash table entry locked. The vsem returned
 * is referenced
 */
void
vsem_qlookup(
	vsemtab_t	*vq,
	int		id,
	vsem_t		**vsemp)
{
	vsem_t		*vsem;
	kqueue_t	*kq;

	/* Get the hash queue */
	kq = &vq->vst_queue;
	/*
	 * Step through the hash queue looking for the given id
	 */
	for (vsem = (vsem_t *)kqueue_first(kq);
	     vsem != (vsem_t *)kqueue_end(kq);
	     vsem = (vsem_t *)kqueue_next(&vsem->vsm_queue)) {
		if (id == vsem->vsm_id) {
			/*
			 * Got it. Add reference to vsem and return
			 */
			VSEM_TRACE6("vsem_qlookup", vsem->vsm_id,
				    "key", vsem->vsm_key, "vsem", vsem);
			vsem_ref(vsem);
			*vsemp = vsem;
			return;
		}
	}
	/*
	 * Can't find it, return NULL
	 */
	VSEM_TRACE2("vsem_qlookup failed", id);
	*vsemp = NULL;
}

/*
 * Enter a vsem into the hash table.
 */
void
vsem_enter(
	vsem_t		*vsem,
	int		lock)
{
	vsemtab_t	*vq;

	/* Initial reference */
	vsem_ref(vsem);

	vq = &vsemtab[vsem->vsm_id%vsemtabsz];
	if (lock)
		mrlock(&vsem_lookup_lock, MR_ACCESS, PZERO);
	VSEMTAB_LOCK(vq, MR_UPDATE);
	ASSERT(!vsem_onkqueue(&vq->vst_queue, vsem, vsem->vsm_id));
	kqueue_enter(&vq->vst_queue, &vsem->vsm_queue);
	VSEMTAB_UNLOCK(vq);
	if (lock)
		mrunlock(&vsem_lookup_lock);
}

/*
 * remove a semaphore from the lookup table.
 * This is used when and RMID occurs
 * This calls needs to be serialized so that only one thread does the clearid
 * since once the key is removed, another thread could be creating another
 * semaphore with the same key.
 */
void
vsem_remove(
	vsem_t	*vsem,
	int	clearid)
{
	vsemtab_t	*vq;

	vq = &vsemtab[vsem->vsm_id%vsemtabsz];
	/*
	 * Lock out global scans and semgets
	 */
	mrlock(&vsem_lookup_lock, MR_ACCESS, PZERO);
	/*
	 * Indicate the key is no longer here
	 * We do this first so that new semget's won't find a key but not
	 * the entry.
	 */
	if (clearid && (vsem->vsm_key != IPC_PRIVATE))
		vsem_idclear(vsem->vsm_key);
	/*
	 * lock out lookup scans and create/destroys
	 */
	VSEMTAB_LOCK(vq, MR_UPDATE);
	VSEM_TRACE4("vsem_remove", vsem->vsm_id, "vsem", vsem);
	ASSERT(!kqueue_isnull(&vsem->vsm_queue));
	kqueue_remove(&vsem->vsm_queue);
	kqueue_null(&vsem->vsm_queue);
	VSEMTAB_UNLOCK(vq);
	mrunlock(&vsem_lookup_lock);

	/* release initial reference */
	vsem_rele(vsem);
}

int
vsem_is_removed(
	vsem_t		*vsem)
{
	return(kqueue_isnull(&vsem->vsm_queue));
}

/*
 * Find a vsem structure with a given key.
 * The tricky/ugly part is that atomic lookup/create may be required
 * so we must effectively single thread all key-lookup/creates (semget).
 * flags comes from semget - consists of mode and IPC_CREAT
 */
int
vsem_lookup_key(
	key_t		key,
	int		flags,
	int		nsems,
	struct cred	*cred,
	vsem_t		**vsemp)
{
	vsem_t		*vsem;
	kqueue_t	*kq;
	int		i;
	int		error;
	vsemtab_t	*vq;

	ASSERT(key != IPC_PRIVATE);
again:
	mrlock(&vsem_lookup_lock, MR_UPDATE, PZERO);

	/*
	 * Step through the vsem table looking for an existing 
	 * vsem with the correct key
	 */
	for (i = 0, vq = &vsemtab[0]; i < vsemtabsz; i++, vq++) {
		kq = &vq->vst_queue;
		/*
		 * Step through each entry in the hash queue looking
		 * for the key
		 */
		for (vsem = (vsem_t *)kqueue_first(kq);
		     vsem != (vsem_t *)kqueue_end(kq);
		     vsem = (vsem_t *)kqueue_next(&vsem->vsm_queue)) {
			if (vsem == 0) {
				printf("vsemq corrupt, idx %i, kq 0x%x\n",
						i, kq);
				panic("vsem_lookup_key");
			}
			if (key == vsem->vsm_key) {

				vsem_ref(vsem);
				mrunlock(&vsem_lookup_lock);

				/*
				 * Got an active key. Check it is what we want
				 */
				error = VSEM_KEYCHECK(vsem, cred, flags, nsems);

				if (error) {
					vsem_rele(vsem);
					return(error);
				}

				/* Got it. The vsem is returned */
				*vsemp = vsem;
				return(0);
			}
		}
	}

	/*
	 * There is no existing vsem (on this cell) 
	 * Create one if required. As the lookup
	 * lock is held we can do this without a race with another
	 * lookup for the same key
	 */
	error = vsem_create(key, flags, nsems, cred, vsemp);
	/* vsem_create unlocks lookup_lock always. */
	if (error == EAGAIN) {
		/* we instantiated a DC for an existing semaphore
		 * we should loop back (and find it this time) and
		 * check requisite permissions
		 */
		goto again;
	}
	return(error);
}

/*
 * Find a vsem structure with a given id.
 * Return unlocked and referenced vsem
 * This function only searches the local cell's vsemtab
 */
int
vsem_lookup_id_local(
	int		id,
	vsem_t		**vsemp)
{
	vsemtab_t	*vq;
	vsem_t		*vsem;

	*vsemp = NULL;

	if (id < 0)
		return(EINVAL);

	mrlock(&vsem_lookup_lock, MR_ACCESS, PZERO);

	/*
	 * Look for the id in the appropriate hash queue.
	 * Returns with the vsem referenced (or NULL)
	 */
	vq = &vsemtab[id%vsemtabsz];
	VSEMTAB_LOCK(vq, MR_ACCESS);
	vsem_qlookup(vq, id, &vsem);
	VSEMTAB_UNLOCK(vq);
	mrunlock(&vsem_lookup_lock);

	if (vsem == NULL)
		return(ENOENT);

	*vsemp = vsem;
	return(0);
}

int
vsem_getstatus_local(
	struct semstat	*stat,
	struct cred	*cred)
{
	kqueue_t	*kq;
	int		found;
	unsigned	loc = 0;
	vsemtab_t	*vq;
	vsem_t		*vsem;
	bhv_desc_t	*bdp;
	int		error;

	mrlock(&vsem_lookup_lock, MR_UPDATE, PZERO);

	/*
	 * start by looking for 'id' in the location passed to us.
	 * If we don't find it, start at first of chain, else return
	 * next in line
	 * For the first search in each cell, sm_id is the first id
	 * to look for and sm_location is -1.
	 */
	if (stat->sm_location != -1LL) {
		/*
		 * start where we left off
		 * The 'sm_location' field is the index into our vsemtab table,
		 * and sm_id is the last id we gave back.
		 */
		loc = (unsigned)stat->sm_location;
		if (loc >= vsemtabsz)
			loc = 0;
		vq = &vsemtab[loc];
		kq = &vq->vst_queue;
		for (found = 0, vsem = (vsem_t *)kqueue_first(kq);
		     vsem != (vsem_t *)kqueue_end(kq);
		     vsem = (vsem_t *)kqueue_next(&vsem->vsm_queue)) {
			if (found) {
				bdp = VSEM_TO_FIRST_BHV(vsem);
				if (vsem_islocal(bdp))
					goto ret;
			}
			if (vsem->vsm_id == stat->sm_id) {
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

	for (; loc < vsemtabsz; loc++) {
		vq = &vsemtab[loc];
		kq = &vq->vst_queue;
		/*
		 * for every element on this hash queue
		 */
		for (vsem = (vsem_t *)kqueue_first(kq);
		     vsem != (vsem_t *)kqueue_end(kq);
		     vsem = (vsem_t *)kqueue_next(&vsem->vsm_queue)) {
			bdp = VSEM_TO_FIRST_BHV(vsem);
			if (vsem_islocal(bdp)) {
ret:
				vsem_ref(vsem);
				mrunlock(&vsem_lookup_lock);

				/* might get error if non-SU is trying to stat */
				error = VSEM_GETSTAT(vsem, cred,
					     &stat->sm_semds, &stat->sm_cell);
				stat->sm_id = vsem->vsm_id;
				stat->sm_location = loc;
				vsem_rele(vsem);
				return(error);
			}
		}
	}
	mrunlock(&vsem_lookup_lock);
	return(ESRCH);
}

#if DEBUG || SEMDEBUG
int
vsem_onkqueue(
	kqueue_t	*kq,
	vsem_t		*tv,
	int		id)
{
	vsem_t		*vsem;

	for (vsem = (vsem_t *)kqueue_first(kq);
	     vsem != (vsem_t *)kqueue_end(kq);
	     vsem = (vsem_t *)kqueue_next(&vsem->vsm_queue)) {
		if (vsem == tv || vsem->vsm_id == id)
			return(1);
	}
	return(0);
}

static void
vsemprint(
	vsem_t		*vsem,
	vsemtab_t	*vq)
{
	qprintf("vsem @0x%x:\n", vsem);
	qprintf("    id %d key 0x%w32x ref %d nsems",
		vsem->vsm_id, vsem->vsm_key, vsem->vsm_refcnt, vsem->vsm_nsems);
	if (vq)
		qprintf(" vq 0x%x", vq);
	qprintf("\n    behavior head 0x%x:\n", &vsem->vsm_bh);
	idbg_vsem_bhv_print(vsem);
}

static void
idbg_vsem(
	__psint_t	x)
{
	kqueue_t	*kq;
	int		i;
	vsemtab_t	*vq;
	vsem_t		*vsem;

	if (x == -1) {
#if CELL
		qprintf("Dumping vsem table for cell %d\n", cellid());
#endif
		for (i = 0; i < vsemtabsz; i++) {
			vq = &vsemtab[i];
			kq = &vq->vst_queue;
			/*
			 * for every element on this hash queue
			 */
			for (vsem = (vsem_t *)kqueue_first(kq);
			     vsem != (vsem_t *)kqueue_end(kq);
			     vsem = (vsem_t *)kqueue_next(&vsem->vsm_queue)) {
				vsemprint(vsem, vq);
			}
		}
	} else if (x < 0) {
		vsemprint((vsem_t *)x, NULL);
	}
}

static void
idbg_vsem_trace(
	__psint_t	x)
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
		
	ktrace_print_buffer(vsem_trace, id, idx, count);
}
#endif /* DEBUG || SEMDEBUG */
