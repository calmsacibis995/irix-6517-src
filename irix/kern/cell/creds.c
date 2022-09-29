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

#ident	"$Revision: 1.12 $"

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <ksys/vhost.h>
#include <ksys/cred.h>
#include <ksys/cell/subsysid.h>
#include <ksys/cell/cell_set.h>
#include <ksys/cell/recovery.h>
#include "creds_private.h"
#include "I_creds_stubs.h"
#include "invk_creds_stubs.h"
#ifdef DEBUG
#include <sys/idbgentry.h>
#endif
#ifdef CRED_DEBUG
#include <sys/cmn_err.h>
#endif

static mrlock_t credid_lock;
static mrlock_t credid_lookup_lock;
static ushort credid_rotor;
static avltree_desc_t credid_lookup;
static zone_t *credid_zone;
static credid_cache_line_t *credid_cache;       /* credid cache */

#ifdef DEBUG
static int ccache_access;
static int ccache_miss;
#endif

static void credid_lookup_refresh(void);

#ifdef DEBUG
static void credid_lookup_dump(avltree_desc_t *lookup);
static void idbg_creds_lookup(__psint_t addr);
static void idbg_creds_cache(__psint_t addr);
#endif
/*
 * cred_getid
 *
 * Return the identifier for the input cred structure
 *
 * This is a trivial implementation to act as a placeholder until
 * the real implementation comes along.
 */

credid_t
cred_getid(cred_t *credp)
{
	credid_t credid;
	credid_t curid;
	struct credid_s *credid_s;
	credid_lookup_entry_t *centryp;

	if (credp == NULL)
		return (CREDID_NULLID);

	if (credp == sys_cred)
		return (CREDID_SYSCRED);

	ASSERT(sizeof(struct credid_s) == sizeof(credid));
	/*
	 * If this cred structure already has an id allocated on this cell,
	 * just return that.  Otherwise, allocate one and insert
	 * the cred structure exported list
	 * 
	 * If it's got an id, but not local to this cell and we're exporting
	 * it, we need to replace the id and assume responsibilty, since
	 * we can't assume the cred will continue to exist on the original cell
	 */
	curid = credp->cr_id;
	if (curid >= 0)
		return (credp->cr_id);

	centryp = (credid_lookup_entry_t *)kmem_zone_alloc(credid_zone, KM_SLEEP);

	credid_s = (struct credid_s *)&credid;
	credid_s->crid_cell = cellid();

again:
	mrlock(&credid_lock, MR_UPDATE, PZERO);
	if (credid_rotor == CREDID_MAX) {
		cred_t *was;
		/*
		 * id wrap...flush remote, drop dead enties from lookup table
		 * and wrap the rotor
		 */
#ifdef CRED_DEBUG
		cmn_err(CE_NOTE, "creid wrap on cell %d\n", cellid());
#endif
		VHOST_CREDFLUSH(cellid());

		mrlock(&credid_lookup_lock, MR_UPDATE, PZERO);
		credid_lookup_refresh();
		mrunlock(&credid_lookup_lock);
		/*
		 * Due to the fact that VHOST_CREDFLUSH_WAIT is a sync rpc,
		 * we could nest here in cred_getid, so temporarily set
		 * sys_cred to cause a hasty exit in that case
		 */
		was = get_current_cred();
		set_current_cred(sys_cred);

		VHOST_CREDFLUSH_WAIT(cellid());

		ASSERT(was->cr_ref > 0);

		set_current_cred(was);

		credid_rotor = 0;
	}
	credid_s->crid_value = credid_rotor++;
	mrunlock(&credid_lock);
	/*
	 * See if someone else assigned an id to the cred or if the id is busy
	 */
	mrlock(&credid_lookup_lock, MR_ACCESS, PZERO);
	curid = credp->cr_id;
	if ((curid > 0) && CREDID_LOCAL(curid)) {
		mrunlock(&credid_lookup_lock);
		kmem_zone_free(credid_zone, centryp);
		return (credp->cr_id);
	}
	if (credid_lookup_find(&credid_lookup, credid)) {
		/*
		 * cred id in use
		 */
		mrunlock(&credid_lookup_lock);
		goto again;
	}
	mrunlock(&credid_lookup_lock);
	/*
	 * Upgrade to a write lock to install the id in the lookup table
	 * Set cred id value in cred struct only after successfully installing
	 * the lookup mentry
	 */
	centryp->cr_id = credid;
	centryp->cr_cred = credp;

	mrlock(&credid_lookup_lock, MR_UPDATE, PZERO);
	if ((credp->cr_id > 0) && CREDID_LOCAL(credp->cr_id)) {
		mrunlock(&credid_lookup_lock);
		kmem_zone_free(credid_zone, centryp);
		return (credp->cr_id);
	}
	credid_lookup_insert(&credid_lookup, centryp);

	credp->cr_id = credid;

	mrunlock(&credid_lookup_lock);

	return (credid);
}
/*
 * credid_lookup_remove
 *
 * The ref count on a cred has grone to 0 and it has a valid credid.
 *
 * There is the issue of id reusue.  If the value of the id
 * is behind the local rotor, simply remove it.  The remote caches will be
 * flushed before the id can be reused.
 *
 * If it's ahead of the local rotor, there's a problem.  If we just free,
 * the id could be reused before remote caches are flushed potentially
 * causing a stale remote reference. 
 *
 * So, if it's ahead of the rotor, NULL the cred pointer marking the entry
 * as dead and occupying the id.  Dead entries are removed at rotor wrap/
 * cache flush time.  Note that dead entries could  be removed whenever the
 * rotor overtakes them.
 *
 * One final note, don't need to lock out id allocation, since worst thing that
 * could happen is an entry could be marked dead when it could've been freed.
 * Do have to prevent rotor wrap/remote flush.
 */
void
credid_lookup_remove(cred_t *credp)
{
	credid_lookup_entry_t *centryp;
	struct credid_s  *credid_s;

	ASSERT(CREDID_VALID(credp));
	ASSERT(CREDID_CELL(credp->cr_id) == cellid());

	mrlock(&credid_lookup_lock, MR_UPDATE, PZERO);
	centryp = credid_lookup_find(&credid_lookup, credp->cr_id);

	ASSERT(centryp);
	ASSERT(centryp->cr_id == credp->cr_id);

	credid_s = (struct credid_s *)&credp->cr_id;

	if (credid_s->crid_value >= credid_rotor) {
		/*
		 * Dead
		 */
		centryp->cr_cred = NULL;
		mrunlock(&credid_lookup_lock);
	} else {
		credid_lookup_entry_remove(&credid_lookup, centryp);
		mrunlock(&credid_lookup_lock);
		kmem_zone_free(credid_zone, centryp);
	}

}

#define	CREDID_FIRST(C)	((credid_lookup_entry_t *)((C).avl_firstino))
#define	CREDID_NEXT(E)	((credid_lookup_entry_t *)((E)->cr_avlnode.avl_nextino))

#ifdef DEBUG
static void 
credid_lookup_dump(avltree_desc_t *lookup)
{
	credid_lookup_entry_t *centryp;

	qprintf("CELL %d, dump lookup table @ %lx:\n", cellid(), lookup);

	centryp =  CREDID_FIRST(*lookup);
	while (centryp) {
		qprintf("centryp %lx id %x cred %lx ref %d id %x\n",
			centryp,
			centryp->cr_id,
			centryp->cr_cred,
			(centryp->cr_cred)? centryp->cr_cred->cr_ref : -1,
			(centryp->cr_cred)? centryp->cr_cred->cr_id : -1);
		centryp = CREDID_NEXT(centryp);
	}
}
#endif

static void
credid_lookup_refresh(void)
{
	credid_lookup_entry_t *centryp;
	credid_lookup_entry_t *next;
	/*
	 * Pull "first" off the list until find one that is active,
	 * freeing all "dead" ones encountered
	 */
	do {
		centryp = CREDID_FIRST(credid_lookup);
		if (centryp == NULL)
			return;

		if (centryp->cr_cred == NULL) {
			/*
			 * First lookup entry is dead.  Remove it and
			 * fetch new first
			 */
			credid_lookup_entry_remove(&credid_lookup, centryp);
			kmem_zone_free(credid_zone, centryp);
			centryp = NULL;
		}
	} while (centryp == NULL);

	while (next = CREDID_NEXT(centryp)) {

		if (next->cr_cred == NULL) {
			/*
			 * dead entry
			 */
			credid_lookup_entry_remove(&credid_lookup, next);
			kmem_zone_free(credid_zone, next);
		} else
			centryp = next;
	}
}

static __psunsigned_t
credid_avl_start(avlnode_t *np)
{
	return ((__psunsigned_t ) (((credid_lookup_entry_t *)np)->cr_id));
}

static __psunsigned_t
credid_avl_end(avlnode_t *np)
{
	return ((__psunsigned_t ) (((credid_lookup_entry_t *)np)->cr_id) + 1);
}

avlops_t credid_avlops = {
	credid_avl_start,
	credid_avl_end
};
/*
 * Client side support
 */
/*
 * credid_getcred
 *
 * Map a cred id to a credential structure
 *
 * This is a trivial implementation to act as a placeholder until
 * the real implementation comes along.
 */
/* ARGSUSED */
cred_t *
credid_getcred(credid_t credid)
{
	int i, bkt;
	cred_t *cred;
	cred_t *newcred;
	gid_t *groups;
	size_t count;
	cell_t server;
	int error;
	service_t svc;
	/* REFERENCED */
	int msgerr;
#ifdef DEBUG
	atomicAddInt(&ccache_access, 1);
#endif
	/*
	 * Check for "special" credids
	 */
	if (credid < 0) {
		if (credid == CREDID_NULLID)
			return (NULL);
	
		if (credid == CREDID_SYSCRED) {
			crhold(sys_cred);
			return (sys_cred);
		}
		ASSERT(0);
		return (NULL);
	}
	bkt = CREDID_BUCKET(credid);
	mrlock(&credid_cache[bkt].ccl_lock, MR_ACCESS, PZERO);

	i = credid_cache[bkt].ccl_hint;

	if (credid_cache[bkt].ccl_id[i] == credid) {

		cred = credid_cache[bkt].ccl_cred[i];
		ASSERT(cred);

		crhold(cred);

		mrunlock(&credid_cache[bkt].ccl_lock);

		return (cred);
	}
	for (i = 0; i < MAX_CCL_ENTRY; i++) {
		if (credid_cache[bkt].ccl_id[i] == credid) {

			cred = credid_cache[bkt].ccl_cred[i];
			ASSERT(cred);

			crhold(cred);

			credid_cache[bkt].ccl_hint = i;
			mrunlock(&credid_cache[bkt].ccl_lock);

			return (cred);
		}
	}
	mrunlock(&credid_cache[bkt].ccl_lock);
	/*
	 * Cache miss
	 */
#ifdef DEBUG
	atomicAddInt(&ccache_miss, 1);
#endif
	server = CREDID_CELL(credid);

	if (server == cellid()) {
		credid_lookup_entry_t *centryp;

		mrlock(&credid_lookup_lock, MR_ACCESS, PZERO);
		centryp = credid_lookup_find(&credid_lookup, credid);
	
		if (centryp && centryp->cr_cred) {
			cred = centryp->cr_cred;
			crhold(cred);
		} else {
#ifdef CRED_DEBUG
			printf("failed to locate %x\n", credid);
			credid_lookup_dump(&credid_lookup);
#else
			ASSERT(0);	/* should not happen */
#endif
			cred = NULL;
		}
		mrunlock(&credid_lookup_lock);
		return (cred);
	}
	/*
	 * rpc to server to get cred struct then lock cache for update
	 * It's possible to be racing with another thread trying to install
	 * the same credentials, so before inserting check for another
	 * copy.  If there is, toss these.
	 */
	/*
	 * Contact the remote server for the creds
	 */
	newcred = creds_crget();
	groups = newcred->cr_groups;
	count = ngroups_max;

	ASSERT(server != cellid());

	SERVICE_MAKE(svc, server, SVC_CREDID);

	msgerr = invk_creds_get_cred(svc, credid, newcred, &groups, &count, &error);
	ASSERT(!msgerr);

	/*
	 * Temporary hack to prevent downstream addressing errors until
	 * mac label and multi-group support is added
	 */
	newcred->cr_mac = NULL;
	/*
	 * We've now got a copy of the original cred struct
	 * The ref count is set to the count on the serving cell,
	 * which is irrelevant here and should be reset to 1 for the 
	 * ccred cache ref and reset the credid in the cred struct itself.
	 */
	newcred->cr_ref = 1;
	newcred->cr_id = CREDID_NULLID;
	if (error) {
		crfree(newcred);
		return (NULL);
	}
	/*
	 * Install in local cache
	 */
	mrlock(&credid_cache[bkt].ccl_lock, MR_UPDATE, PZERO);
	for (i = 0; i < MAX_CCL_ENTRY; i++) {
		if (credid_cache[bkt].ccl_id[i] == credid) {
			/*
			 * somone else installed while we were busy
			 */
			cred = credid_cache[bkt].ccl_cred[i];
			ASSERT(cred);
			crhold(cred);

			credid_cache[bkt].ccl_hint = i;

			mrunlock(&credid_cache[bkt].ccl_lock);

			crfree(newcred);
			return (cred);
		}
	}
	/*
	 * Put it in the cache
	 */
	i = credid_cache[bkt].ccl_hint;
	if (i == 0)
		i = MAX_CCL_ENTRY;
	i--;

	ASSERT(((credid_cache[bkt].ccl_id[i] == CREDID_NULLID)&&
	       (credid_cache[bkt].ccl_cred[i] == NULL)) ||
	       (credid_cache[bkt].ccl_cred[i]));

	if (credid_cache[bkt].ccl_id[i] != CREDID_NULLID)
		crfree(credid_cache[bkt].ccl_cred[i]);

	credid_cache[bkt].ccl_cred[i] = newcred;
	credid_cache[bkt].ccl_id[i] = credid;
	credid_cache[bkt].ccl_hint = i;

	crhold(newcred);

	mrunlock(&credid_cache[bkt].ccl_lock);

	return (newcred);
}
/*
 * Remote interfaces
 */
/*
 * Dump the contents of a cache for a given server.
 */
/* ARGSUSED */
void
credid_flush_cache(cell_t server)
{
	int bkt;
	int i, j;
#ifdef CRED_DEBUG
	cmn_err(CE_DEBUG, "creds_flush_cache from %d\n", server);
#endif
	if (server == cellid())
		/*
		 * Nothing cached from ourselves
		 */
		return;

	bkt = server * MAX_CCL_PER_CELL; 

	for (i = 0; i < MAX_CCL_PER_CELL; i++) {
		mrlock(&credid_cache[bkt+i].ccl_lock, MR_UPDATE, PZERO);
		for (j = 0; j < MAX_CCL_ENTRY; j++) {

			if ((credid_cache[bkt+i].ccl_id[j] != CREDID_NULLID)&&
			    (CREDID_CELL(credid_cache[bkt+i].ccl_id[j]) == server)) {
				ASSERT(credid_cache[bkt+i].ccl_cred[j]);
				crfree(credid_cache[bkt+i].ccl_cred[j]);
				credid_cache[bkt+i].ccl_id[j] = CREDID_NULLID;
			}
		}
		mrunlock(&credid_cache[bkt+i].ccl_lock);
	}
}
#ifdef DEBUG
static void
credid_cache_dump(credid_cache_line_t *credid_cache, cell_t cell)
{
	int bkt, limit, i;

	if (cell < 0) {
		qprintf("Dumping cred cache for all servers\n");
		bkt = 0;
		limit = MAX_CELLS * MAX_CCL_PER_CELL;
	} else if (cell >= 0 && cell < MAX_CELLS) {
		qprintf("Dumping cred cache for server cell %d\n", cell);
		bkt = cell * MAX_CCL_PER_CELL;
		limit = bkt + MAX_CCL_PER_CELL;
	} else {
		qprintf("Invalid cell %d\n", cell);
		return;
	}
	qprintf("\t[bucket,index] credid cred_ptr\n");
	for (; bkt < limit; bkt++) {
		for (i = 0; i < MAX_CCL_ENTRY; i++) {
			if (credid_cache[bkt].ccl_id[i] != CREDID_NULLID) {
				qprintf("\t[%d,%d] %x %lx\n",
					bkt, i,
					credid_cache[bkt].ccl_id[i],
					credid_cache[bkt].ccl_cred[i]
				);
			}
		}
	}
}
#endif
/*	
 * Remote request to look up a cred on this cell.
 *
 * Currently passing back only the basic cred structure.  Needs to be 
 * enhanced ro support bsd multi-group and mac
 * TBD
 */
int
I_creds_get_cred(credid_t credid,
		 cred_t *out_cred,
		 gid_t **groups,
		 size_t *groups_count,
		 int *error,
		 void **bufdesc)
{
	credid_lookup_entry_t *centryp;
	cred_t *cred;
#ifdef CRED_DEBUG
	cmn_err(CE_DEBUG, "I_creds_get_cred credid %x\n", credid);
#endif
	if (!CREDID_LOCAL(credid)) {
		ASSERT(0);
		*error = EINVAL;
		return (-1);
	}
	mrlock(&credid_lookup_lock, MR_ACCESS, PZERO);
	centryp = credid_lookup_find(&credid_lookup, credid);

	if (centryp && centryp->cr_cred) {
		cred = centryp->cr_cred;
		crhold(cred);
	} else {
#ifdef CRED_DEBUG
		printf("failed to locate %x\n", credid);
		credid_lookup_dump(&credid_lookup);
#else
		ASSERT(0);	/* should not happen */
#endif
		cred = NULL;
	}
	mrunlock(&credid_lookup_lock);

	*bufdesc = (void *)cred;

	if (cred == NULL) {
#ifndef CRED_DEBUG
		ASSERT(0);
#endif
		*error = ENOENT;
		return (-1);
	}
	/*
	 * Still missing mac_label XXX TBD
	 */
	bcopy(cred, out_cred, sizeof(cred_t));
	*groups = cred->cr_groups;
	*groups_count = ngroups_max;

	out_cred->cr_ref = 0;

	*error = 0;

	return (0);
}
	 
/* ARGSUSED */
void
I_creds_get_cred_done(gid_t *groups, size_t groups_count, void *bufdesc)
{
	if (bufdesc)
		crfree((cred_t *)bufdesc);
}
/*
 * creds_recovery
 *
 * Some cell or set of cells failed.  Not much to do but flush cache of 
 * any entries
 *
 * Not going to worry about nested failures since we don't communicate with
 * anyone
 */
/* ARGSUSED */
void
creds_recovery(cell_set_t *fail_set, void *arg)
{
	cell_t failed_cell;

	while (!set_is_empty(fail_set)) {
		failed_cell = set_member_min(fail_set);
		set_del_member(fail_set, failed_cell);
		credid_flush_cache(failed_cell);
	}
}

void
creds_init(void)
{
	int i, j;

	credid_rotor = 0;

	credid_cache = (credid_cache_line_t *)
	    kmem_zalloc(sizeof(credid_cache_line_t)*MAX_CCL, KM_SLEEP|KM_CACHEALIGN);
	for (i = 0; i < MAX_CCL; i++) {
		mrinit(&credid_cache[i].ccl_lock, "credidcache");

		for (j = 0; j < MAX_CCL_ENTRY; j++)
			credid_cache[i].ccl_id[j] = CREDID_NULLID;
	}
	mrinit(&credid_lock, "credid");
	mrinit(&credid_lookup_lock, "credidlookup");

	credid_zone = kmem_zone_init(sizeof(credid_lookup_entry_t), "Credid");

	avl_init_tree(&credid_lookup, &credid_avlops);

	mesg_handler_register(creds_msg_dispatcher, CRED_SVC_SUBSYSID);

	crs_register_callout(creds_recovery, NULL);

#ifdef DEBUG
	idbg_addfunc("creds_lookup", idbg_creds_lookup);
	idbg_addfunc("creds_cache", idbg_creds_cache);
#endif

}
#ifdef DEBUG
/*
 * Debugging support
 */
/* ARGSUSED */
void
idbg_creds_lookup(__psint_t addr)
{
	credid_lookup_dump(&credid_lookup);

}

void
idbg_creds_cache(__psint_t addr)
{
	cell_t server;

	if (addr < 0)
		server = -1;

	else
		server = (cell_t)addr;
	
	credid_cache_dump(credid_cache, server);
}
#endif
