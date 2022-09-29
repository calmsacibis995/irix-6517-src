/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/atomic_ops.h>
#include <sys/idbgentry.h>
#include <sys/systm.h>

#include "frame_barrier.h"

syncbar_t*
syncbar_create(int nclients)
{
	syncbar_t* syncbar;

	syncbar = (syncbar_t*) kmem_zalloc(sizeof(syncbar_t),
					   KM_CACHEALIGN | KM_SLEEP);
	if (syncbar == NULL)
		return (NULL);

	syncbar->in_counter = 0;
	syncbar->registered = 1;
	syncbar->nclients = 1;

	if (nclients > SYNCBAR_CUTOFF) {
		int next_nclients = (nclients - SYNCBAR_CUTOFF) + 1;
		syncbar->next = syncbar_create(next_nclients);
		if (syncbar->next == NULL) {
			kmem_free(syncbar, sizeof(syncbar_t));
			return (NULL);
		}
	} else {
		syncbar->next = NULL;
	}

	return (syncbar);
}
		
syncbar_t*
syncbar_register(syncbar_t* syncbar)
{
	int registered;

	registered = atomicAddInt((int*)&syncbar->registered, 1);
	if (registered <= SYNCBAR_CUTOFF) {
		ASSERT (syncbar->nclients < SYNCBAR_CUTOFF);
		atomicAddInt((int*)&syncbar->nclients, 1);
		return (syncbar);
	}

	ASSERT (syncbar->next);
	return (syncbar_register(syncbar->next));
}

void
syncbar_unregister(syncbar_t* syncbar)
{
	int client;

	/*
	 * "nclients" is affected here instead of "registered", since it
	 * is nclients which truely represents the number of cpus
	 * synchronizing on this barrier.  "Registered" is only used to
	 * determine which cpu uses which syncbar in the syncbar chain.
	 */

	ASSERT (syncbar->nclients > 0);
	client = atomicAddInt((int*)&syncbar->nclients, -1);
	if (client == 1)
		syncbar->in_counter = 0;
}

void
syncbar_wait(syncbar_t* syncbar)
{
	int in_counter;

	in_counter = atomicAddInt((int*)&syncbar->in_counter, 1);
	ASSERT (in_counter > 0);

	if (in_counter == syncbar->nclients) {
		if (syncbar->next)
			syncbar_wait(syncbar->next);
		syncbar->in_counter = 0;
		return;
	}

	while (in_counter != 0) {
		in_counter = syncbar->in_counter;
	}
}

void
syncbar_kick(syncbar_t* syncbar)
{
	int in_counter;

	in_counter = atomicAddInt((int*)&syncbar->in_counter, 1);
	ASSERT (in_counter > 0);

	if (in_counter == syncbar->nclients) {
		if (syncbar->next)
			syncbar_wait(syncbar->next);
		syncbar->in_counter = 0;
		return;
	}
}

void
syncbar_destroy(syncbar_t* syncbar)
{
	ASSERT (syncbar->nclients);
	ASSERT (syncbar->nclients <= SYNCBAR_CUTOFF);
	ASSERT (syncbar->registered);
	ASSERT (syncbar->in_counter == 0);

	if (syncbar->next)
		syncbar_destroy(syncbar->next);

	kmem_free(syncbar, sizeof(syncbar_t));
}

void
syncbar_print(syncbar_t* syncbar)
{
	qprintf("  Syncbar: 0x%x Clients %d Registered %d Incount %d\n",
	       syncbar, syncbar->nclients, syncbar->registered,
		syncbar->in_counter);

	if (syncbar->next)
		syncbar_print(syncbar->next);
}
