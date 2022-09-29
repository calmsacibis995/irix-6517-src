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
#ident "$Id: behavior.c,v 1.17 1998/04/29 20:57:43 dnoveck Exp $"

/*
 * Source file used to associate/disassociate behaviors with virtualized 
 * objects.  See ksys/behavior.h for more information about behaviors, etc.
 *
 * The implementation is split between functions in this file and macros
 * in behavior.h.
 */

#include <sys/types.h>
#include <sys/cpumask.h>
#include <ksys/behavior.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/idbgentry.h>


zone_t	*bhv_global_zone;

/*
 * Global initialization function called out of main.
 */
void
bhv_global_init(void)
{
	/*
	 * Initialize a behavior zone used by subsystems using behaviors 
         * but without any private data.  In the UNIKERNEL case, this zone 
         * is used only for behaviors that are not yet isolated to a single 
         * cell.  The only such user is in pshm.c in which a dummy vnode is 
         * obtained in support of vce avoidance logic.
	 */
	bhv_global_zone = kmem_zone_init(sizeof(bhv_desc_t), "bhv_global_zone");
}

/*
 * Insert a new behavior descriptor into a behavior chain.  The act of
 * modifying the chain is done atomically w.r.t. ops-in-progress
 * (see comment at top of behavior.h for more info on synchronization).
 *
 * If BHV_SYNCH is defined, then must be called with the behavior chain 
 * write locked.  This both synchronizes with ops-in-progress as well
 * as multiple concurrent threads inserting.
 *
 * If BHV_SYNCH is not defined, then it's the callers' responsibility
 * to synchronize appropriately.  Imon, for instance, relies on the
 * atomic nature of insertion to synchronize with ops-in-progress, and
 * implements its own lock to synchronize multiple concurrent threads
 * inserting.
 *
 * The behavior chain is ordered based on the 'position' number which
 * lives in the first field of the ops vector (higher numbers first).
 *
 * Attemps to insert duplicate ops result in an EINVAL return code.
 * Otherwise, return 0 to indicate success.
 */
int 
bhv_insert(bhv_head_t *bhp, bhv_desc_t *bdp)
{
	bhv_desc_t 	*curdesc, *prev;
	int		position;

	ASSERT(bdp->bd_next == NULL);
	ASSERT(BHV_IS_WRITE_LOCKED(bhp));

	/*
	 * Validate the position value of the new behavior.
	 */
	position = BHV_POSITION(bdp);
	ASSERT(position >= BHV_POSITION_BASE && position <= BHV_POSITION_TOP);

	/* 
	 * Find location to insert behavior.  Check for duplicates.
	 */
	prev = NULL;
	for (curdesc = bhp->bh_first;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		/* Check for duplication. */
		if (curdesc->bd_ops == bdp->bd_ops)
			return EINVAL;

		/* Find correct position */
		if (position >= BHV_POSITION(curdesc)) {
			ASSERT(position != BHV_POSITION(curdesc));
			break;		/* found it */
		}

		prev = curdesc;
	} 

	if (prev == NULL) {
		/* insert at front of chain */
		bdp->bd_next = bhp->bh_first;
		bhp->bh_first = bdp;		/* atomic wrt oip's */
	} else {
		/* insert after prev */
		bdp->bd_next = prev->bd_next;
		prev->bd_next = bdp;		/* atomic wrt oip's */
	}

	return 0;
}

/*
 * Same as bhv_insert except it operates on a bhv2_head_t.
 */
int 
bhv2_insert(bhv2_head_t *bhp, bhv_desc_t *bdp)
{
	bhv_desc_t 	*curdesc, *prev;
	int		position;

	ASSERT(bdp->bd_next == NULL);
	ASSERT(BHV2_IS_WRITE_LOCKED(bhp));

	/*
	 * Validate the position value of the new behavior.
	 */
	position = BHV_POSITION(bdp);
	ASSERT(position >= BHV_POSITION_BASE && position <= BHV_POSITION_TOP);

	/* 
	 * Find location to insert behavior.  Check for duplicates.
	 */
	prev = NULL;
	for (curdesc = bhp->bh2_first;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		/* Check for duplication. */
		if (curdesc->bd_ops == bdp->bd_ops)
			return EINVAL;

		/* Find correct position */
		if (position >= BHV_POSITION(curdesc)) {
			ASSERT(position != BHV_POSITION(curdesc));
			break;		/* found it */
		}

		prev = curdesc;
	} 

	if (prev == NULL) {
		/* insert at front of chain */
		bdp->bd_next = bhp->bh2_first;
		bhp->bh2_first = bdp;		/* atomic wrt oip's */
	} else {
		/* insert after prev */
		bdp->bd_next = prev->bd_next;
		prev->bd_next = bdp;		/* atomic wrt oip's */
	}

	return 0;
}

/*
 * Remove a behavior descriptor from a position in a behavior chain;
 * the postition is guaranteed not to be the first position.  
 * Should only be called by the bhv_remove() macro.
 *
 * The act of modifying the chain is done atomically w.r.t. ops-in-progress
 * (see comment at top of behavior.h for more info on synchronization).
 */
void 
bhv_remove_not_first(bhv_head_t *bhp, bhv_desc_t *bdp)
{
	bhv_desc_t 	*curdesc, *prev;

	ASSERT(bhp->bh_first != NULL);
	ASSERT(bhp->bh_first->bd_next != NULL);

	prev = bhp->bh_first;
	for (curdesc = bhp->bh_first->bd_next;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc == bdp)
			break;		/* found it */
		prev = curdesc;
	}

	ASSERT(curdesc == bdp);
	prev->bd_next = bdp->bd_next;	/* remove from after prev */
	                                /* atomic wrt oip's */
}

/*
 * Same as bhv_remove_not_first except it operates on a bhv2_head_t.
 */
void 
bhv2_remove_not_first(bhv2_head_t *bhp, bhv_desc_t *bdp)
{
	bhv_desc_t 	*curdesc, *prev;

	ASSERT(bhp->bh2_first != NULL);
	ASSERT(bhp->bh2_first->bd_next != NULL);

	prev = bhp->bh2_first;
	for (curdesc = bhp->bh2_first->bd_next;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc == bdp)
			break;		/* found it */
		prev = curdesc;
	}

	ASSERT(curdesc == bdp);
	prev->bd_next = bdp->bd_next;	/* remove from after prev */
	                                /* atomic wrt oip's */
}

/* 
 * Look for a specific ops vector on the specified behavior chain.
 * Return the associated behavior descriptor.  Or NULL, if not found.
 */
bhv_desc_t *
bhv_lookup(bhv_head_t *bhp, void *ops)
{
	bhv_desc_t	*curdesc;

	for (curdesc = bhp->bh_first;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc->bd_ops == ops)
			return curdesc;
	}

	return NULL;
}

/*
 * Same as bhv_lookup except it operates on a bhv2_head_t.
 */
bhv_desc_t *
bhv2_lookup(bhv2_head_t *bhp, void *ops)
{
	bhv_desc_t	*curdesc;

	for (curdesc = bhp->bh2_first;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc->bd_ops == ops)
			return curdesc;
	}

	return NULL;
}

/* 
 * Look for a specific ops vector on the specified behavior chain.
 * Return the associated behavior descriptor.  Or NULL, if not found.
 * 
 * The caller has not read locked the behavior chain, so aucquire the 
 * lock before traversing the chain.
 */
bhv_desc_t *
bhv_lookup_unlocked(bhv_head_t *bhp, void *ops)
{
	bhv_desc_t	*bdp;

	BHV_READ_LOCK(bhp);
	bdp = bhv_lookup(bhp, ops);
	BHV_READ_UNLOCK(bhp);

	return bdp;
}

/*
 * Same as bhv_lookup_unlocked except it operates on a bhv2_head_t.
 */
bhv_desc_t *
bhv2_lookup_unlocked(bhv2_head_t *bhp, void *ops)
{
	bhv_desc_t	*bdp;

	BHV2_READ_LOCK(bhp);
	bdp = bhv2_lookup(bhp, ops);
	BHV2_READ_UNLOCK(bhp);

	return bdp;
}

/* 
 * Return the base behavior in the chain, or NULL if the chain
 * is empty.  
 * 
 * The caller has not read locked the behavior chain, so aucquire the 
 * lock before traversing the chain.
 */
bhv_desc_t *
bhv_base_unlocked(bhv_head_t *bhp)
{
	bhv_desc_t	*curdesc;

	BHV_READ_LOCK(bhp);
	for (curdesc = bhp->bh_first;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc->bd_next == NULL) {
			BHV_READ_UNLOCK(bhp);
			return curdesc;
		}
	}

	BHV_READ_UNLOCK(bhp);
	return NULL;
}

/*
 * Same as bhv_base_unlocked except it operates on a bhv2_head_t.
 */
bhv_desc_t *
bhv2_base_unlocked(bhv2_head_t *bhp)
{
	bhv_desc_t	*curdesc;

	BHV2_READ_LOCK(bhp);
	for (curdesc = bhp->bh2_first;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc->bd_next == NULL) {
			BHV2_READ_UNLOCK(bhp);
			return curdesc;
		}
	}

	BHV2_READ_UNLOCK(bhp);
	return NULL;
}

#ifdef BHV_SYNCH

/*
 * Private structure for queueing update callout requests on a behavior head.
  */
typedef struct bhv_ucq {
	struct bhv_ucq	*bhc_next;
	bhv_ucallout_t	*bhc_func;
	void		*bhc_arg1;
	void		*bhc_arg2;
	void		*bhc_arg3;
	void		*bhc_arg4;
} bhv_ucalloutq_t;

/*
 * This function queues an update callout request for a behavior head lock.
 * The possible return values being:
 *	-1 => the lock is currently locked for update;
 *	0  => the lock was obtained for update and the callout was taken;
 *	>0 => indicates the number of callout requests now queued.  
 */
int
bhv_queue_ucallout(
	bhv_head_t	*bhp,
	bhv_ucallout_t	*func,
	void		*arg1,
	void		*arg2,
	void		*arg3,
	void		*arg4)
{
	bhv_ucalloutq_t	*bhcp;
	bhv_ucalloutq_t	*bhucqp;
	int		s;
	int		count = 0;

	/*
	 * If we can't get the lock for access immediately,
	 * it must be update-locked, so fail.
	 */
	if (!BHV_MRTRYACCESS(&bhp->bh_mrlock))
		return -1;

	BHV_BLA_PUSH(&bhp->bh_mrlock,0);

	bhcp = (bhv_ucalloutq_t *) kmem_alloc(sizeof(bhv_ucalloutq_t),KM_SLEEP);
	bhcp->bhc_func = func;
	bhcp->bhc_arg1 = arg1;
	bhcp->bhc_arg2 = arg2;
	bhcp->bhc_arg3 = arg3;
	bhcp->bhc_arg4 = arg4;
	bhcp->bhc_next = NULL;

	/*
	 * Add new request to (end) of list under lock to serialize additions.
	 * The list cannot be dismantled since we hold the behavior lock
	 * for access.
	 */
	s = mutex_spinlock(&bhp->bh_ucqlock);
	/* Find list end and count queued requests */
	if ((bhucqp = bhp->bh_ucallout) == NULL) {
		bhp->bh_ucallout = bhcp;
	} else {
		while (bhucqp->bhc_next) {
			bhucqp = bhucqp->bhc_next;
			count++;
		}
		bhucqp->bhc_next = bhucqp;
	}
	count++;
	mutex_spinunlock(&bhp->bh_ucqlock, s);

	/*
	 * Now release the access lock - but first try to
	 * promote to update and if so the callouts must be made.
	 */
	BHV_BLA_POP(&bhp->bh_mrlock,0);
	if (BHV_MRTRYPROMOTE(&bhp->bh_mrlock)) {
		BHV_BLA_PUSH(&bhp->bh_mrlock,1);
		bhv_do_ucallout(bhp);
		BHV_BLA_POP(&bhp->bh_mrlock,1);
		BHV_MRUNLOCK(&bhp->bh_mrlock);
		count = 0;
	} else {
		BHV_MRACCUNLOCK(&bhp->bh_mrlock);
	}

	return count;
}

void
bhv_do_ucallout(
	bhv_head_t *bhp)
{
	bhv_ucalloutq_t	*bhucqp;

	ASSERT(BHV_IS_WRITE_LOCKED(bhp));
	ASSERT(bhp->bh_ucallout);

	/*
	 * Locking is provided by having the behavior head locked for update
	 * and new callouts are added holding the lock for access.
	 */ 
	while (bhucqp = bhp->bh_ucallout) {
		(*bhucqp->bhc_func)(bhp,
				   bhucqp->bhc_arg1,
				   bhucqp->bhc_arg2,
				   bhucqp->bhc_arg3,
				   bhucqp->bhc_arg4);
		bhp->bh_ucallout = bhucqp->bhc_next;
		kmem_free(bhucqp, sizeof(bhv_ucalloutq_t));
	}
}

#ifdef DEBUG
void
bhv_print_ucallout(
	bhv_head_t *bhp)
{
	bhv_ucalloutq_t	*bhucqp;

	/*
	 * Locking is provided by having the behavior head locked for update
	 * and new callouts are added holding the lock for access.
	 */ 
	bhucqp = bhp->bh_ucallout;
	while (bhucqp) {
		qprintf("  update callout request @0x%x for 0x%x\n"
			"  with args (0x%x, 0x%x, 0x%x, 0x%x)\n",
			bhucqp,
			bhucqp->bhc_func,
			bhucqp->bhc_arg1,
			bhucqp->bhc_arg2,
			bhucqp->bhc_arg3,
			bhucqp->bhc_arg4);
		bhucqp = bhucqp->bhc_next;
	}
}
#endif

#endif /* BHV_SYNCH */

#if CELL_PREPARE

/* ARGSUSAED */
void
bhv_head_init(
	bhv_head_t *bhp,
        char *name)
{
	bhp->bh_first = NULL;
	bhp->bh_mrlspace = 0xf00d;
}

void
bhv_head_reinit(
	bhv_head_t *bhp)
{
	ASSERT_ALWAYS(bhp->bh_first == NULL &&
		      bhp->bh_mrlspace == 0xf00d);
}

void
bhv_insert_initial(
	bhv_head_t *bhp,
	bhv_desc_t *bdp)
{
	ASSERT_ALWAYS(bhp->bh_first == NULL &&
		      bhp->bh_mrlspace == 0xf00d);
	bhp->bh_first = bdp;
}

void
bhv_head_destroy(
	bhv_head_t *bhp)
{
	ASSERT_ALWAYS(bhp->bh_first == NULL &&
		      bhp->bh_mrlspace == 0xf00d);
	bhp->bh_mrlspace = 0;
}




/* ARGSUSED */
void
bhv_read_lock(
	__int64_t *lp)
{
        return;
}

/* ARGSUSED */
void
bhv_read_unlock(
	__int64_t *lp)
{
        return;
}

/* ARGSUSED */
void
bhv_write_lock(
	__int64_t *lp)
{
        return;
}

/* ARGSUSED */
void
bhv_write_unlock(
	__int64_t *lp)
{
        return;
}



#endif /* CELL_PREPARE */
