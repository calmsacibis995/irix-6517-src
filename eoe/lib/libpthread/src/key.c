/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * This module implements per-thread data as defined by POSIX.
 *
 * Key points:
	* Data is stored in an array created for each pthread when it
	  attempts to set a value for a given key.
	* The get/set specific interfaces are reentrant.
	* Keys may wrap and require all pthread's values be flushed before
	  it can be used.
 */

#include "common.h"
#include "key.h"
#include "mtx.h"
#include "sys.h"
#include "vp.h"
#include "pt.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>


/*
 * List of all keys.  If the table entry is NULL, the key is available.
 * Otherwise it contains a pointer to a destructor function (which could be
 * the NULL_DESTRUCTOR).
 */
typedef void			(*key_destructor_t)(void *);
#define NULL_DESTRUCTOR		((key_destructor_t)(1L))

static key_destructor_t	keys[PT_KEYS_MAX];	/* Keys table. */
static slock_t		key_lock;		/* Lock to protect table */
static int		key_hint = 0;		/* Key table hint. */
static int		key_table_wrapped = 0;	/* TRUE if all entries have
						   been allocated before. */

static void		key_clear(pt_t *, void *);


/*
 * pthread_key_create(key, destructor)
 *
 * Return a key to be used for thread specific data.
 */
int
pthread_key_create(pthread_key_t *key, key_destructor_t destructor)
{
	int	i;
	int	k;

	ASSERT(key != NULL);

	sched_enter();
	lock_enter(&key_lock);

	/*
	 * Search through the keys table looking for an empty slot.  When
	 * we find one, mark it as busy by placing NULL_DESTRUCTOR in it.
	 * We can't set the final value until we've cleared any old key
	 * data associated with the old entry.  This avoids a race with
	 * key_data_cleanup().
	 *
	 * Use key_hint as a starting point.
	 */
	for (i = 0, k = key_hint; i < PT_KEYS_MAX; i++, k++) {
		if (keys[k % PT_KEYS_MAX] == NULL) {
			*key = k % PT_KEYS_MAX;
			keys[*key] = NULL_DESTRUCTOR;	/* Hold our place. */
			key_hint = *key + 1;
			break;
		}
	}

	/*
	 * No more keys available.
	 */
	if (i == PT_KEYS_MAX) {
		lock_leave(&key_lock);
		sched_leave();
		return (EAGAIN);
	}

	if (key_table_wrapped) {
		lock_leave(&key_lock);
		sched_leave();

		/*
		 * We're reusing a slot.  pt_data[*key] might not be NULL
		 * for all pthreads so clear it for everybody.
		 */
		pt_foreach(key_clear, key);
	} else {
		/*
		 * Check to see if we've wrapped yet.
		 */
		if (key_hint == PT_KEYS_MAX) {
			key_table_wrapped = TRUE;
		}

		lock_leave(&key_lock);
		sched_leave();
	}

	/*
	 * Set destructor function, if any.
	 */
	if (destructor) {
		keys[*key] = destructor;
	}

	return (0);
}

/* Specialised macro to avoid duplication
 */
#define KEY_DATA_INIT(ret) \
	if (pt_self->pt_data == NULL) { \
		if (!(pt_self->pt_data = _malloc(PT_KEYS_MAX*sizeof(void*)))) {\
			return (ret); \
		} \
		memset(pt_self->pt_data, 0, PT_KEYS_MAX*sizeof(void *)); \
	}

/*
 * pthread_setspecific(key, value)
 *
 * Set this thread's data value for this key to value.
 *
 * If this is the first time pthread_setspecific has been called for this
 * thread, allocate pt_data.  This memory will be freed when the pt_t is
 * fully unreferenced.
 *
 * Since a pthread calls this on its own behalf and it only modifies data
 * that it references, no locks need be held.
 */
int
pthread_setspecific(pthread_key_t key, const void *value)
{
	register pt_t	*pt_self = PT;

	if (key < 0 || key >= PT_KEYS_MAX || keys[key] == NULL) {
		return (EINVAL);
	}

	KEY_DATA_INIT(ENOMEM);

	pt_self->pt_data[key] = (void *)value;

	return (0);
}


/*
 * __pthread_getspecific_addr(key)
 *
 * Return address of TSD value.  Used to implement TSD that requires
 * an lvalue, e.g. errno.
 */
void *
__pthread_getspecific_addr(pthread_key_t key)
{
	register pt_t	*pt_self = PT;

	KEY_DATA_INIT(0);

	return (pt_self->pt_data + key);
}


/*
 * pthread_getspecific(key)
 *
 * Retrieve the value pointed to by key.
 *
 * No error checking is performed.
 *
 * Since a pthread calls this on its own behalf and it only modifies data
 * that it references, no locks need be held.
 */
void *
pthread_getspecific(pthread_key_t key)
{
	register pt_t	*pt_self = PT;

	ASSERT(key >= 0 && key < PT_KEYS_MAX);

	if (pt_self->pt_data) {
		return (pt_self->pt_data[key]);
	}

	return (NULL);
}


/*
 * pthread_key_delete(key)
 *
 * Invalidate key.
 *
 * If we've already gone through the table once, then use this key as a
 * hint for the next pthread_key_create().
 *
 * Takes no locks since keys[key] store is atomic and key_hint is only
 * that, a hint.
 */
int
pthread_key_delete(pthread_key_t key)
{
	if (key < 0 || key >= PT_KEYS_MAX) {
		return (EINVAL);
	}

	keys[key] = NULL;

	if (key_table_wrapped) {
		key_hint = key;
	}

	return (0);
}


/*
 * pt_key_clear(pt, key)
 *
 * Clear any data pt has associated with key.
 */
void
key_clear(pt_t *pt, void *arg)
{
	pthread_key_t	key = *(pthread_key_t *)arg;

	if (pt->pt_data) {
		pt->pt_data[key] = NULL;
	}
}


/*
 * key_data_cleanup()
 *
 * Call the destructor functions for each data value.  Could loop forever,
 * but that's okay.
 */
void
key_data_cleanup(void)
{
	int			i;
	int			loop_again;
	register pt_t		*pt_self = PT;
	key_destructor_t	destructor;
	void			*data;

	if (pt_self->pt_data == NULL) {
		return;
	}

	do {
		loop_again = 0;

		/*
		 * Copy destructor and data to avoid race with
		 * pthread_key_delete.
		 */
		for (i = 0; i < PT_KEYS_MAX; i++) {
			if ((destructor = keys[i])
			    && destructor != NULL_DESTRUCTOR
			    && (data = pt_self->pt_data[i]) != NULL) {
				pt_self->pt_data[i] = 0;
				destructor(data);
				loop_again = 1;
			}
		}
	} while (loop_again);
}
