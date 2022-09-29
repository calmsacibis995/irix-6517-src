/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.4 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/debug.h>
#include <sys/var.h>

/* UID active hash list management */
/* This enforces the notion of maximum processes per uid */

typedef struct uid_act {
	uid_t		ua_uid;
	int		ua_cnt;
	struct uid_act	*ua_nxt;
} uid_act_t;

typedef struct {
	uid_act_t	*uhash_uidact;
#ifdef MP
	lock_t		uhash_lock;
#endif
} uid_hash_t;

static uid_hash_t *uidact_hashtab;
static int uidact_hashmask;
#define uid_hash(X)		(&uidact_hashtab[(X) & uidact_hashmask])

#define	uidact_lock(lp)		mutex_spinlock(lp)
#define	uidact_unlock(lp, x)	mutex_spinunlock(lp,x)

zone_t *uidact_zone;

#ifdef DEBUG
/* Keep track of number of uid structs currently in use - this
 * number should never exceed v.v_proc (slight fudge factor
 * due to uidact_switch.)
 */
lock_t uidact_dbg_lock;
static int uidact_alloc_count;
#endif

void
uidacthash_init(int nent)
{
	uidact_hashtab = kern_calloc(nent, sizeof(uid_hash_t));
	uidact_hashmask = nent - 1;
	uidact_zone = kmem_zone_init(sizeof(uid_act_t), "uid_actlist");

#ifdef MP
	{
	int i;

	for (i = 0; i < nent; i++)
		init_spinlock(&uidact_hashtab[i].uhash_lock, "uidact", i);
	}
#endif
#ifdef DEBUG
	spinlock_init(&uidact_dbg_lock, "uidact_dbg");
#endif
}

/* Increment active cnt for uid. Create entry if necessary. */
int
uidact_incr(uid_t uid)
{
	uid_hash_t *uhash;
	uid_act_t **uact;
	uid_act_t *nact = 0;
	int s;
	int cnt;


	uhash = uid_hash(uid);

again:
	s = uidact_lock(&uhash->uhash_lock);
	uact = &uhash->uhash_uidact;

	while (*uact != NULL) {
		if ((*uact)->ua_uid == uid)
			break;
		uact = &((*uact)->ua_nxt);
	}

	if (*uact == NULL && nact == NULL) {
#ifdef DEBUG
		atomicAddInt(&uidact_alloc_count, 1);
#endif
		uidact_unlock(&uhash->uhash_lock, s);
		nact = kmem_zone_alloc(uidact_zone, KM_SLEEP);
		goto again;
	}

	if (*uact == NULL) {
		/* Link new entry onto list */
		*uact = nact;
		nact->ua_nxt = NULL;
		nact->ua_cnt = 1;
		nact->ua_uid = uid;
		cnt = 1;

		uidact_unlock(&uhash->uhash_lock, s);
	} else {
		ASSERT((*uact)->ua_uid == uid);

		cnt = ++(*uact)->ua_cnt;

		uidact_unlock(&uhash->uhash_lock, s);

		if (nact) {
			/* found user 2nd time through */
			kmem_zone_free(uidact_zone, nact);
#ifdef DEBUG
			atomicAddInt(&uidact_alloc_count, -1);
			s = mutex_spinlock(&uidact_dbg_lock);
			ASSERT(uidact_alloc_count >= 0);
			ASSERT(uidact_alloc_count <= v.v_proc + 20);
			mutex_spinunlock(&uidact_dbg_lock, s);
#endif
		}
	}

	return cnt;
}

/* Decrement count of processes for uid. If count goes to 0,
 * free entry.
 */
void
uidact_decr(uid_t uid)
{
	uid_hash_t *uhash;
	uid_act_t **uact;
	uid_act_t *freeact = NULL;
	int s;

	uhash = uid_hash(uid);
	uact = &uhash->uhash_uidact;

	s = uidact_lock(&uhash->uhash_lock);

	while (*uact != NULL) {
		if ((*uact)->ua_uid == uid)
			break;
		uact = &((*uact)->ua_nxt);
	}

	ASSERT(*uact != NULL);
	ASSERT((*uact)->ua_cnt > 0);

	if (--(*uact)->ua_cnt <= 0) {
		freeact = *uact;
		*uact = freeact->ua_nxt;
	}

#ifdef DEBUG
	if (freeact)
		atomicAddInt(&uidact_alloc_count, -1);
#endif

	uidact_unlock(&uhash->uhash_lock, s);

	if (freeact)
		kmem_zone_free(uidact_zone, freeact);

}

/* Used by setuid calls - decrement old uid entry, increment
 * new uid entry.
 */
void
uidact_switch(uid_t old, uid_t new)
{
	if (old != new) {
		uidact_decr(old);
		(void)uidact_incr(new);
	}
}
