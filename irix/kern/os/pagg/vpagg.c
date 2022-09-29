/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Revision: 1.7 $"

#include "sys/types.h"
#include "ksys/behavior.h"
#include "sys/extacct.h"
#include "sys/kmem.h"
#include "sys/sema.h"
#include "sys/systm.h"
#include "sys/errno.h"
#include "sys/atomic_ops.h"
#include "sys/sysmacros.h"
#include "sys/cred.h"
#include "ksys/vproc.h"
#include "ksys/vpag.h"
#include "ppagg_private.h"
#include "sys/cmn_err.h"
#include "ksys/vm_pool.h"
#include "ksys/as.h"


vpagg_t	vpagg0;	/* Initial system vpagg_t */

/*
 * All active vpags are kept in a doubly linked list. vpag_lookup
 * steps through the list to find a vpagg given an id.
 * This  mechanism may be improved later if faster lookups are needed.
 */
static lock_t 	vpag_active_list_lck;	/* active vpagg list mutex */
vpagg_t 	*vpagactlist;		/* active arsess list */

static struct zone *vpagg_zone;

/*
 * min_local_paggid/max_local_paggid defines the range within which to allocate
 * paggids for a given machine. It is ORed with the global_paggid_mask to
 * make the paggid unique across a given array of machine.
 */

extern paggid_t	min_local_paggid,
		max_local_paggid; /* Paggid bounds - defined in mtune/kernel */
extern int	asmachid;	  /* default machine ID for global paggid's */
extern int	asarrayid;	  /* default array ID for global paggid's */
extern paggid_t incr_paggid;	  /* default paggid increment */

static paggid_t	next_paggid = 1LL;	/* local portion of next paggid */
static paggid_t eff_incr_paggid = 1LL;	/* effective increment for paggid's */
static int	eff_machid = 0;		/* effective machine ID */
static int	eff_arrayid = 0xFFFF;	/* effective array ID */
static paggid_t	global_paggid_mask;	/* mask for forming global paggid's */

extern	int	dflt_paggid;		/* default paggid for vpagg0, etc. */

static void 	vpag_free(vpagg_t *, int);

static int 	vpag_remove(vpagg_t *);
vpagg_t 	*vpag_lookup_nolock(paggid_t);
static int	vpag_allocid_insert(paggid_t, vpagg_t *);
static paggid_t vpag_makeid(void);

#define ARRAYID_SHIFT	32
#define	MACHID_SHIFT	48
#define VPAG_MAKE_PAGGID_MASK(A,M) ((((paggid_t)(A)) << ARRAYID_SHIFT) | \
				    (((paggid_t)(M)) << MACHID_SHIFT))

#define	VPAG_ACTIVE_LIST_LOCK()		mutex_spinlock(&vpag_active_list_lck)
#define	VPAG_ACTIVE_LIST_UNLOCK(x) mutex_spinunlock(&vpag_active_list_lck, (x))

/*
 * vpag_init:
 * Initialize the vpagg lists. Also create  vpagg0 which will be associated
 * with process 0.
 */
void
vpag_init()
{
	spinlock_init(&vpag_active_list_lck,  "vpag_act_list_lck");
	vpagactlist = NULL;

	/* Set up the various parts that make up a pagg ID */
	vpag_setidctr(min_local_paggid);
	vpag_setidincr(incr_paggid);
	vpag_setmachid(asmachid);
	vpag_setarrayid(asarrayid);

	/* Set up the pseudo-arsess for proc 0 */
	vpagactlist = &vpagg0;

	vpagg0.vpag_refcnt = 1;
	vpagg0.vpag_paggid = global_paggid_mask | dflt_paggid;
	ppag_init();
	bhv_head_init(VPAG_BHV_HEAD(&vpagg0), "vpagg");
	ppagg_create(&vpagg0,  1, 0, VPAG_ARRAY_SESSION);

	vpagg_zone = kmem_zone_init(sizeof(vpagg_t), 
					"Virtual process aggregate");

}

/*
 * vpag_create:
 * Allocate and initialize a vpagg object. It allocates a vpagg structure
 * allocates a paggid if one is not passed in and also allocates a corresponding
 * ppag structure. It also sets up the behaviour chain.
 */
int
vpag_create(paggid_t paggid, pid_t pid, int nice, vpagg_t **vpagp, vpag_type_t type)
{
	vpagg_t	*vpag;
	int	error;

        if (paggid >= min_local_paggid  &&  paggid <= max_local_paggid)
            return EINVAL;


	vpag = kmem_zone_zalloc(vpagg_zone, KM_SLEEP);
	ASSERT(vpag);
	bhv_head_init(VPAG_BHV_HEAD(vpag), "vpagg");
	error = ppagg_create(vpag, pid, nice, type);
	if (error) {
		bhv_head_destroy(&vpag->vpag_bhvh);
		kmem_zone_free(vpagg_zone, vpag);
		return error;
	}
	if (vpag_allocid_insert(paggid, vpag) == -1)  {
		vpag_free(vpag, 0);
		return EINVAL;
	}
	*vpagp = vpag;
	return 0;
}

/*
 * vpag_free:
 * Free a vpagg object. It frees up the entire behaviour chain including the
 * ppag.
 */
void
vpag_free(vpagg_t *vpagg, int remove)
{
	ASSERT(vpagg != &vpagg0);
	if (remove)
		vpag_remove(vpagg);
	VPAG_DESTROY(vpagg);
	bhv_head_destroy(&vpagg->vpag_bhvh);
	kmem_zone_free(vpagg_zone, vpagg);
}

/*
 * vpag_join:
 * Operation to add a process to a process aggregate.
 */
/* ARGSUSED */
void
vpag_join(vpagg_t *vpag, pid_t pid)
{
	AddToVpagRefCnt(vpag, 1);
	ASSERT(vpag->vpag_refcnt > 0);
}

/*
 * vpag_leave:
 * Operation used to disassociate a process from a process aggregate.
 */
/* ARGSUSED */
void
vpag_leave(vpagg_t *vpag, pid_t pid)
{
	int cnt = AddToVpagRefCnt(vpag, -1);
	ASSERT(cnt >= 0);
	if (cnt == 0) 
		vpag_free(vpag, 1);
}


/*
 * vpag_remove:
 * Remove a vpag into the active list.
 */

static int
vpag_remove(vpagg_t *vpag)
{
	int s = VPAG_ACTIVE_LIST_LOCK();

        /* Remove the arsess from the active list. */
        if (vpag->vpag_prev) {
                vpag->vpag_prev->vpag_next = vpag->vpag_next;
        } else {
                vpagactlist = vpag->vpag_next;
        }
        if (vpag->vpag_next)
                vpag->vpag_next->vpag_prev = vpag->vpag_prev;

        /* Safe to unlock the active list */
	VPAG_ACTIVE_LIST_UNLOCK(s);
	return 0;
}

	
/*
 * vpag_lookup_nolock:
 *	return a pointer to the vpagg_t with the specified vpaggid 
 *	or NULL if the specified process aggregate id is not in
 *	use. Assumes that the vpag active list lock is held
 */
vpagg_t *
vpag_lookup_nolock(paggid_t paggid)
{
	vpagg_t *curr;

	for (curr = vpagactlist;  curr != NULL;  curr = curr->vpag_next)
	    if (curr->vpag_paggid == paggid)
		return curr;

	return NULL;
}

/*
 * vpag_lookup:
 * Looks up a paggid and returns a vpag. Calls vpag_lookup_nolock after holding
 * active list lock. Also gets a reference on the vpagg.
 */
vpagg_t *
vpag_lookup(paggid_t paggid)
{
	vpagg_t *vpag;

	int s = VPAG_ACTIVE_LIST_LOCK();

	vpag = vpag_lookup_nolock(paggid);
	if (vpag && (AddToVpagRefCnt(vpag, 1) == 1)) {
		/*
		 * Somebody raced with us and decremented the reference count.
		 * Release the reference and return a failure.
		 */
		AddToVpagRefCnt(vpag, -1);
		VPAG_ACTIVE_LIST_UNLOCK(s);
		return NULL;
	}
	VPAG_ACTIVE_LIST_UNLOCK(s);
	return vpag;
}

/*
 * vpag_allocid:
 *	Generate a fresh paggid. The allocated ID is guaranteed not to
 *	be in use on the local machine.
 */
paggid_t
vpag_allocid(void)
{
	paggid_t paggid;
	int s;

	s = VPAG_ACTIVE_LIST_LOCK();
	do {
		paggid = vpag_makeid();
	} while (vpag_lookup_nolock(paggid));
	VPAG_ACTIVE_LIST_UNLOCK(s);

	return paggid;
}

/*
 * vpag_allocid_insert:
 * If paggid passed is invalid it allocates a paggid and inserts the vpag
 * into the active list. If paggid passed down is valid and it has not been 
 * previously allocated it is allocated and the vpag is put on the active list.
 */
int
vpag_allocid_insert(paggid_t paggid, vpagg_t *vpag)
{
        int s = VPAG_ACTIVE_LIST_LOCK();
	if (paggid == INVALID_PAGGID) { /* Allocate a new paggid */
		do {
			vpag->vpag_paggid = vpag_makeid();
		} while (vpag_lookup_nolock(vpag->vpag_paggid));
	} else {
		if (vpag_lookup_nolock(paggid) != NULL)  {
			VPAG_ACTIVE_LIST_UNLOCK(s);
			vpag->vpag_paggid = INVALID_PAGGID;
			return -1;
		} else 
			vpag->vpag_paggid = paggid;
	}

	vpag->vpag_next = vpagactlist;
	vpag->vpag_prev = NULL;
	if (vpagactlist)
		vpagactlist->vpag_prev = vpag;
	vpagactlist = vpag;
	vpag->vpag_refcnt = 1;
        VPAG_ACTIVE_LIST_UNLOCK(s);
	return 0;
}

/*
 * vpag_makeid:
 *	Generate a new paggid_t value from the current paggid parameters.
 *	No test is made to determine if the ID is unique or not. The
 *	vpag active list lock is assumed to be held or else untold horrors
 *	may occur while updating next_paggid, etc. 
 */
paggid_t
vpag_makeid(void)
{
	paggid_t paggid = global_paggid_mask | next_paggid;

	next_paggid += eff_incr_paggid;
	if (next_paggid > max_local_paggid) {	  /* positive increment */
		next_paggid = (min_local_paggid - 1
			       + (next_paggid - max_local_paggid));
	}
	else if (next_paggid < min_local_paggid) { /* negative increment */
		next_paggid = (max_local_paggid + 1
			       - (min_local_paggid - next_paggid));
	}

	return paggid;
}

/*
 * vpag_set_paggid:
 * Change the paggid of a given vpagg. First ensure that the new paggid is
 * not allocated already.
 */
int
vpag_set_paggid(vpagg_t *vpag, paggid_t paggid)
{
	int s;


        /* Pagg id must not be in the same range as the */
        /* automatically assigned paggids and must not */
        /* be negative.                                              */

        if (paggid >= min_local_paggid  &&  paggid <= max_local_paggid)
            return EINVAL;

	s = VPAG_ACTIVE_LIST_LOCK();

	/* Ensure new id is unique (exception: don't     */
	/* worry about non-unique if it's the same handle we started with) */

	if (paggid != vpag->vpag_paggid && vpag_lookup_nolock(paggid)) {
		VPAG_ACTIVE_LIST_UNLOCK(s);
		return EINVAL;
	}

	vpag->vpag_paggid = paggid;
	VPAG_ACTIVE_LIST_UNLOCK(s);

	return 0;
}

/*
 * vpagg_enumashs
 *	Stores all of the currently active paggids in the
 *	specified array, up to some maximum number of entries. Returns
 *	the number of entries stored, or -1 if there wasn't enough
 *	space to store the entire list.
 */
int
vpagg_enumpaggids(paggid_t *paggid_list, int max_paggid)
{
	vpagg_t *curr;
	int	 numpaggids = 0;
	int	s;

	s = VPAG_ACTIVE_LIST_LOCK();
	for (curr = vpagactlist;  curr != NULL;  curr = curr->vpag_next) {
		if (numpaggids == max_paggid) {
			numpaggids = -1;
			break;
		}
		paggid_list[numpaggids] = curr->vpag_paggid;
		++numpaggids;
	}
	VPAG_ACTIVE_LIST_UNLOCK(s);

	return numpaggids;
}

/*
 * vpag_setarrayid:
 *	set the pagg array ID, which is used for such things as generating
 *	global vpaggids. Specifying -1 resets the original boot-time array
 *	ID. Note that if the machine ID is 0, then the array ID is ignored.
 *	Returns 0 if successful, or an error code (suitable for errno) if not.
 */
int
vpag_setarrayid(int newarrayid)
{
	int s;

	if (newarrayid < -1  ||  newarrayid > 0xffff) {
		return EINVAL;
	}

	s = VPAG_ACTIVE_LIST_LOCK();

	if (newarrayid == -1) {
		eff_arrayid = asarrayid;
	}
	else {
		eff_arrayid = newarrayid;
	}

	if (eff_machid != 0) {
		global_paggid_mask = VPAG_MAKE_PAGGID_MASK(eff_arrayid,
							   eff_machid);
	}

	VPAG_ACTIVE_LIST_UNLOCK(s);

	return 0;
}

/*
 * vpag_setmachid:
 *	set the array session machine ID, which is used for such things
 *	as generating global vpaggids. Specifying a machine ID of 0 effectively
 *	unsets the machine ID, in which case all paggids generated by the
 *	kernel will be local. Specifying -1 resets the original boot-time
 *	machine ID. Returns 0 if successful, or an error code (suitable
 *	for errno) if not.
 */
int
vpag_setmachid(int newmachid)
{
	int s;

	if (newmachid < -1  ||  newmachid > 0x7fff) {
		return EINVAL;
	}

	s = VPAG_ACTIVE_LIST_LOCK();

	if (newmachid == -1) {
		eff_machid = asmachid;
	}
	else {
		eff_machid = newmachid;
	}

	if (eff_machid == 0) {
		global_paggid_mask = 0LL;
	}
	else {
		global_paggid_mask = VPAG_MAKE_PAGGID_MASK(eff_arrayid,
							   eff_machid);
	}

	VPAG_ACTIVE_LIST_UNLOCK(s);

	return 0;
}

/*
 * vpag_setidctr:
 *	change the value of the paggid counter, which is used for
 *	determining the local portion of the next paggid. This would
 *	ordinarily only be done early during system initialization,
 *	in order to resume the counter at or near its value after a
 *	previous orderly shutdown. Returns 0 if successful, or an
 *	error code (suitable for errno) if not.
 */
int
vpag_setidctr(paggid_t newctr)
{
	int s;

	if (newctr < min_local_paggid  ||  newctr > max_local_paggid) {
		return EINVAL;
	}

	s = VPAG_ACTIVE_LIST_LOCK();

	next_paggid = newctr;

	VPAG_ACTIVE_LIST_UNLOCK(s);

	return 0;
}

/*
 * vpag_setidincr:
 *	change the value of the paggid counter increment, which is
 *	used when determining the local portion of the next paggid.
 *	Returns 0 if successful, or an error code (suitable for errno)
 *	if not.
 */
int
vpag_setidincr(paggid_t newincr)
{
	int s;

	if (newincr < (min_local_paggid - max_local_paggid)  ||
	    newincr > (max_local_paggid - min_local_paggid)  ||
	    newincr == 0)
	{
		return EINVAL;
	}

	s = VPAG_ACTIVE_LIST_LOCK();

	eff_incr_paggid = newincr;

	VPAG_ACTIVE_LIST_UNLOCK(s);

	return 0;
}

/*
 * vpag_getarrayid:
 *	return the current array session array ID, performing any
 *	necessary translations and massaging.
 */
int
vpag_getarrayid(void)
{
	return eff_arrayid;
}

/*
 * vpag_getmachid:
 *	return the current array session machine ID, performing any
 *	necessary translations and massaging.
 */
int
vpag_getmachid(void)
{
	return eff_machid;
}

/*
 * vpag_getidctr:
 *	return the current paggid counter
 */
paggid_t
vpag_getidctr(void)
{
	return next_paggid;
}

/*
 * vpag_getidincr:
 *	return the current paggid increment
 */
paggid_t
vpag_getidincr(void)
{
	return eff_incr_paggid;
}


/*
 * vpag_transfer_vm_pool:
 * Transfer a process aggregate from one VM pool to another.
 * Currently this is used by miser to transfer the process agg. from
 * the global pool to the miser pool when the process goes critical.
 */
int	
vpag_transfer_vm_pool(vpagg_t *vpag, vm_pool_t *vm_pool)
{
	int	error;
	int	s;
	pgno_t	rss_limit;

	ASSERT(vpag);

	error = VPAG_TRANSFER_VM_POOL(vpag, vm_pool);

	if (vm_pool != GLOBAL_POOL) {
		s = VM_POOL_LOCK(GLOBAL_POOL);
		vm_pool_wakeup(GLOBAL_POOL, 1);
		VM_POOL_UNLOCK(GLOBAL_POOL, s);
	}

	VPAG_GET_VM_RESOURCE_LIMITS(vpag, &rss_limit);
	vm_pool_update_guaranteed_mem(vm_pool, rss_limit);
	return error;
}

/*
 * vpag_miser_create:
 * Create a process aggregate object for miser. It is called when a batch job 
 * is submitted to miser. rss_mem_limit is the max. physical memory that the 
 * process agg. can consume. Returns a vpag with a reference. By default
 * the vpagg enters the global pool. To start with its RSS is zero.  RSS
 * accounting does not start with the current process. exec and fork 
 * enable RSS accounting for the processes. Typically the process which
 * does vpag_miser_create execs and hence starts RSS accounting for the
 * new address space and its children. A miser process cannot create
 * another miser process.
 */
vpagg_t *
vpag_miser_create(size_t rss_mem_limit)
{
	vpagg_t *oldvpag;
	vpagg_t *newvpag;
	vproc_t *vpr;
	as_setattr_t as_attr;
	int	error;
	int	nice;
	vasid_t	vasid;
	pid_t pid;


	pid = current_pid();
	vpr = curvprocp;

	VPROC_GETVPAGG(vpr, &oldvpag);

	/*
 	 * Make sure that the process did belong to an array session.
	 */
	if (oldvpag && (VPAG_GET_TYPE(oldvpag) == VPAG_MISER))
		return NULL;

	VPROC_GET_NICE(curvprocp, &nice, get_current_cred(), error);
	error = vpag_create(INVALID_PAGGID, pid, nice, &newvpag, VPAG_MISER);
	if (error) 
		return NULL;

	if (oldvpag) {
                prid_t  prid;

                prid = VPAG_GETPRID(oldvpag);
                VPAG_SETPRID(newvpag, prid);

                if (!VPAG_SPINFO_ISDFLT(oldvpag)) {
                        char    *spibuf;
                        int     oldspilen;

                        oldspilen = VPAG_GETSPILEN(oldvpag);
                        spibuf = kern_malloc(oldspilen);
                        ASSERT(spibuf != NULL);

                        VPAG_GETSPINFO(oldvpag, spibuf, oldspilen);
                        VPAG_SETSPINFO(newvpag, spibuf, oldspilen);

                        kern_free(spibuf);
                }
        }


	/* Release the old process aggregate to which the process belongs */
	if (oldvpag)
		VPAG_LEAVE(oldvpag, pid);
	VPROC_SETVPAGG(vpr, newvpag);

	as_lookup_current(&vasid);
	as_attr.as_op = AS_SET_VPAGG;
	as_attr.as_set_vpagg = newvpag;
	VPAG_HOLD(newvpag);
	VAS_SETATTR(vasid, NULL, &as_attr);

	VPAG_SET_VM_RESOURCE_LIMITS(newvpag, btoc(rss_mem_limit));
	pag_printf("Created a miser pool pid 0x%d rsslimit %d paggid %d\n",
			pid, rss_mem_limit, VPAG_GETPAGGID(newvpag));
	return newvpag;
}
