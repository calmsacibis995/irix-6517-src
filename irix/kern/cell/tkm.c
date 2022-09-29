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
#ident "$Id: tkm.c,v 1.2 1997/02/21 19:58:08 cp Exp $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <ksys/cell/tkm.h>
#include "tk_private.h"

/*
 * Initialize token migration synchronizer.
 */
void
tkm_init(tkm_state_t *ms)
{
	mutex_init(&ms->tkm_lock, MUTEX_DEFAULT, "migrate");
	ms->tkmi_state = NULL;
}

/*
 * Tear-down token migration synchronizer.
 */
void
tkm_destroy(tkm_state_t *ms)
{
	mutex_destroy(&ms->tkm_lock);
	ASSERT(ms->tkmi_state == NULL);
}

/*
 * Request commencement of object migration. Permission is
 * granted only to the first request. Runers-up in this race
 * receive an error and must failback to the requsting client.
 */
int
tkm_begin(
	tkm_state_t	*tkm,
	cell_t		destination,
	tk_set_t	existence,
	tk_set_t	service)
{
	tkmi_state_t	*mi;

	/*
	 * Under a lock, check if a migration is already
	 * underway. If so return.
	 */
	mutex_lock(&(tkm->tkm_lock), PZERO);
	if (tkm->tkmi_state != NULL) {
		/* Another attempt beat us */
		mutex_unlock(&(tkm->tkm_lock));
		return EMIGRATING;
	}

	/*
	 * No migration is in progress.
	 * Create a migration management structure to exist
	 * only during migration.
	 */
	mi = (void *) kern_malloc(sizeof(tkmi_state_t));
	ASSERT(mi);
	tkm->tkmi_state = mi;
	mi->tkmi_migration_state = TKM_ARRESTING;
	mi->tkmi_to_cell = destination;
	mi->tkmi_existence = existence;
	mi->tkmi_service = service;

	mutex_unlock(&(tkm->tkm_lock));
	return 0;
}

/*
 * Obtain USE tokens for an object. The EXISTENCE token is mandatorily
 * granted but the SERVICE token depends on whether the object
 * is undergoing arrest prior to migration.
 */
void
tkm_obtain_use(
	tkm_state_t	*tkm,		/* migration manager */
	tks_state_t	*tks,		/* object containing token */
	tks_ch_t	client,		/* handle for client making request */
	tk_set_t	wanted,		/* tokens wanted */
	tk_set_t	*granted,	/* tokens granted */
	tk_set_t	*refused,	/* tokens refused */
	tk_set_t	*held)		/* tokens already held by client */
{
	tkmi_state_t	*mi = (tkmi_state_t *) tkm->tkmi_state;

	/*
	 * Check the service state. If the object is being arrested
	 * we cannot grant the SERVICE token at this time. If the
	 * object is undergoing migration, we could not be here because
	 * this routine is called with the access lock on the vshm's
	 * behavior chain and migration requires the update lock.
	 */
	mutex_lock(&(tkm->tkm_lock), PZERO);
	if (tkm->tkmi_state != NULL) {
		ASSERT(mi->tkmi_migration_state == TKM_ARRESTING);
		TK_DIFF_SET(wanted, mi->tkmi_service);  
		tks_obtain(tks, client, wanted, granted, refused, held);
		TK_COMBINE_SET(*refused, mi->tkmi_service);  
	} else {
		tks_obtain(tks, client, wanted, granted, refused, held);
	}
	mutex_unlock(&(tkm->tkm_lock));
}

/*
 * Update the migration state of an object from ARRESTING to ARRESTED.
 */
void
tkm_arrested(tkm_state_t *tkm)
{
	tkmi_state_t	*mi = (tkmi_state_t *) tkm->tkmi_state;

	mutex_lock(&(tkm->tkm_lock), PZERO);
	ASSERT(mi);
	ASSERT(mi->tkmi_migration_state == TKM_ARRESTING);
	mi->tkmi_migration_state = TKM_ARRESTED;
	mutex_unlock(&(tkm->tkm_lock));
}

/*
 * Free a migration management control structure upon completion
 * of migration.
 */
void
tkm_end(tkm_state_t *tkm)
{
	tkmi_state_t	*mi = (tkmi_state_t *) tkm->tkmi_state;

	mutex_lock(&(tkm->tkm_lock), PZERO);
	ASSERT(mi);
	ASSERT(mi->tkmi_migration_state == TKM_ARRESTED);
	tkm->tkmi_state = NULL;
	kern_free(mi);
	mutex_unlock(&(tkm->tkm_lock));
}
