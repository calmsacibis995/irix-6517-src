#include "sys/types.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/immu.h"
#include "sys/sema.h"
#include "bsd/sys/lf.h"

/*
 * element struct on global list of bristles
 */
struct global_elt {
	struct global_elt *next, *remain;
};

extern struct lf_listvec lf_globalvec[];	/* global free list vector */
extern struct lf_elt *lf_reclaim_list;	/* global reclaimed page list */
extern u_int lf_nreclaimed;	/* number of elements on reclaimed list */
extern lock_t lf_reclaim_lock;	/* lock for updating reclaim list */

extern int splimp();

/*
 * Procedure Description:
 *   Lock free get procedure to return a node from a lf_vec list structure
 *
 */
struct lf_elt *
lf_get(local_lfvecp, canwait)
	register struct lf_listvec *local_lfvecp;
	register int canwait;
{
	register struct lf_listvec *global_lfvecp;
	register struct global_elt *e;
	register int s_global, s_lfvec;

	s_lfvec = mutex_spinlock(&local_lfvecp->lock);

	if (local_lfvecp->get) {	/* first try getting from local list */

		e = (struct global_elt *)local_lfvecp->get;
		local_lfvecp->get = (char *)e->next;
		e->next = 0;
	} else {	/* local list empty so try getting from global list */

		global_lfvecp = (struct lf_listvec *)((char *)lf_globalvec
			+ local_lfvecp->index);

		nested_spinlock(&global_lfvecp->lock);

		e = (struct global_elt *)global_lfvecp->get;
		if (e) {
			local_lfvecp->get = (char *)e->next;
			e->next = 0;
			global_lfvecp->get = (char *)e->remain;
		}
		nested_spinunlock(global_lfvecp->lock);
	}

	mutex_spinunlock(&local_lfvecp->lock, s_lfvec);
	return ((struct lf_elt *)e);
}

/*
 * Procedure Description:
 *   Lock-free free procedure to return a node to a lf_vec list structure
 *
 */
void
lf_free(local_lfvecp, e)
	register struct lf_listvec *local_lfvecp;
	register struct lf_elt *e;
{
	register struct lf_listvec *global_lfvecp;
	register int s_global, s_lfvec, s_reclaim;
	register char *pn;

	if (!e) {	/* return if null */
		return;
	}

	global_lfvecp = (struct lf_listvec *)((char *)lf_globalvec
			+ local_lfvecp->index);
	pn = (char *)((int)e & -NBPP);

	s_global = mutex_spinlock(&global_lfvecp->lock);
	/* 
	 * Set reclaim page number (pn) if it is null, then return.
	 */
	if (!global_lfvecp->reclaim_page) {

		global_lfvecp->reclaim_page = pn;
		global_lfvecp->nelts = 1;
		mutex_spinunlock(&global_lfvecp->lock, s_global);

		return;
	}
	/*
	 * IF incoming pn matches reclaim page
	 * THEN
	 *   bump count of number of entries reclaimed for this page
	 *   IF all elements of page found
	 *   THEN
	 *     put page on reclaim_list
	 *   FI
	 * FI
	 * return
	 */
	if (global_lfvecp->reclaim_page == pn) {

		global_lfvecp->nelts++;

		if (global_lfvecp->nelts == global_lfvecp->e_per_page) {
			/*
			 * add to reclaim list
			 */
			nested_spinlock(&lf_reclaim_lock);

			e = (struct lf_elt *)pn;
			e->next = lf_reclaim_list;
			lf_reclaim_list = e;
			lf_nreclaimed++;

			nested_spinunlock(&lf_reclaim_lock);

			global_lfvecp->nelts = 0;
			global_lfvecp->reclaim_page = 0;
		}
		mutex_spinunlock(&global_lfvecp->lock, s_global);
		return;
	}

	/* release global list lock */
	mutex_spinunlock(&global_lfvecp->lock, s_global);

	/* 
	 * Put element on list. Bump bristle count and move entire
	 * bristle to global list when threshold is reached.
	 */
	s_lfvec = mutex_spinlock(&local_lfvecp->lock);

	e->next = (struct lf_elt *)local_lfvecp->get;
	local_lfvecp->get = (char *)e;
	local_lfvecp->count++;

	if (local_lfvecp->count >= local_lfvecp->threshold) {

		local_lfvecp->get = 0;
		local_lfvecp->count = 0;
		mutex_spinunlock(&local_lfvecp->lock, s_lfvec);

		s_global = mutex_spinlock(&global_lfvecp->lock);
		((struct global_elt *)e)->remain =
			(struct global_elt *)global_lfvecp->get;
		global_lfvecp->get = (char *)e;
		mutex_spinunlock(&global_lfvecp->lock, s_global);

	} else {
		mutex_spinunlock(&local_lfvecp->lock, s_lfvec);
	}
	return;
}

/*
 * Procedure Description:
 *   Lock-free lock procedure to obtain a spin lock
 *
 */
int
lf_lock(
	register lock_t *lockp,
	register int canwait)
{
	return mutex_spinlock_spl(lockp, splimp);
}

/*
 * Procedure Description:
 *   Lock-free unlock procedure to release a spin lock
 *
 */
int
lf_unlock(lockp, s)
	register lock_t *lockp;
	register int s;
{
	mutex_spinunlock(lockp, s);
	return;
}

/*
 * Procedure Description:
 *   Lock-free procedure to atomically decrement a statistics variable
 *   used ONLY in debug kernels to keep procedure call statistics
 *
 */
int lf_fetch_dec(lock_t *lockp, u_int *statp)
{
	register int s;

	s = mutex_spinlock(lockp);
	(*statp)--;
	mutex_spinunlock(lockp, s);
	return;
}

/*
 * Procedure Description:
 *   Lock-free procedure to atomically increment a statistics variable
 */
int lf_fetch_inc(lock_t *lockp, u_int *statp)
{
	register int s;

	s = mutex_spinlock(lockp);
	(*statp)++;
	mutex_spinunlock(lockp, s);
	return;
}
