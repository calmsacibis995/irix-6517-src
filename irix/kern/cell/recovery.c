/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <ksys/cell.h>
#include <ksys/cell/cell_set.h>
#include <sys/sema.h>
#include <sys/kmem.h>
#include <ksys/sthread.h>
#include <ksys/cell/mesg.h>
#include <ksys/cell/recovery.h>
#include <ksys/cell/membership.h>
#include <ksys/partition.h>
#include <sys/cmn_err.h>

static lock_t crs_thrd_lock;		/* recovery thread exit */
static int crs_active;
static int crs_nested;
static cell_set_t crs_nested_fail_set;

typedef struct crs_callout_entry {
	void				(*crs_callout)();
	void				*crs_arg;
	struct crs_callout_entry	*crs_next;
} crs_callout_entry_t;

static crs_callout_entry_t *crs_callout;

static void crs_end_recovery(cell_set_t *);

/*
 * External interfaces
 */
/*
 * Register a function to be called upon cell down notification
 *
 * Added with no locking.  List remains consistent while being updated.
 */
void
crs_register_callout(void (*callout)(cell_set_t *, void *), void *arg)
{
	crs_callout_entry_t *co;

	co = (crs_callout_entry_t *)kmem_alloc(sizeof(crs_callout_entry_t),
						      KM_SLEEP);
	co->crs_callout = callout;
	co->crs_arg = arg;
	co->crs_next = crs_callout;
	crs_callout = co;
}

/*
 * crs_recovery_active
 *
 * Return non-zero if recovery is active on the cell, 0 if not
 */
int
crs_recovery_active(void)
{
	return (crs_active);
}
/*
 * crs_nested_failure
 *
 * Returns non-zero if there's a nested failure, 0 if not
 */
crs_nested_failure(void)
{
	return (crs_nested);
}
/*
 * Internal interfaces
 */
/*
 * The recovery thread itself.  Launched on celldown.  Does all cell
 * recovery processing including (and especially) calling subsytem 
 * specific callbacks that doe all the 'real work'
 */
void
crs_recovery_thread(void *arg)
{
	int s;
	cell_set_t fail_set;
	cell_set_t master_fail_set;
	crs_callout_entry_t *co;

	fail_set = (cell_set_t)arg;
	set_assign(&master_fail_set, &fail_set);
restart:
	/*
	 * Probably need lots of stuff here, like logical volume switchover,
	 * root disk issues, etc., etc.
	 */
	for (co = crs_callout; co && !crs_nested; co = co->crs_next) {
		set_assign(&fail_set, &master_fail_set);
		(*co->crs_callout)(&fail_set, co->crs_arg);
	}
	/*
	 * Getting ready to terminate.  Check nested failure.
	 */
	s = mutex_spinlock(&crs_thrd_lock);
	if (crs_nested) {
		set_assign(&master_fail_set, &crs_nested_fail_set);
		set_init(&crs_nested_fail_set);
		crs_nested = 0;
		mutex_spinunlock(&crs_thrd_lock, s);
		goto restart;
	}
	crs_active = 0;
	mutex_spinunlock(&crs_thrd_lock, s);
	crs_end_recovery(&master_fail_set);
	sthread_exit();
}

void
crs_initiate_recovery(cell_set_t fail_set)
{
	int s;
	extern int recovery_pri;

	s = mutex_spinlock(&crs_thrd_lock);
	if (crs_active) {
		set_union(&crs_nested_fail_set, &fail_set);
		crs_nested = 1;
		mutex_spinunlock(&crs_thrd_lock, s);
		return;
	}
	crs_active = 1;
	mutex_spinunlock(&crs_thrd_lock, s);

	s = sthread_create("Recovery", 0, 2 * KTHREAD_DEF_STACKSZ,
			   0, recovery_pri, KT_PS,
		           (st_func_t *)crs_recovery_thread,
		           (void *)fail_set, 0, 0, 0);

	if (s < 0)
		cmn_err(CE_PANIC, "Recovery thread launch");
}

void
crs_init()
{
	spinlock_init(&crs_thrd_lock, "crs_thrd_lock");
}



/*
 * crs_end_recovery:
 * Clean up the message channels.
 */
static void
crs_end_recovery(cell_set_t *cell_set)
{
	cell_t	cell;

	delay(HZ*75*cellid());
	for (cell = 0; cell < MAX_CELLS; cell++) {
		if (!set_is_member(cell_set, cell))
			continue;
		mesg_channel_set_state(cell, MESG_CHANNEL_MAIN, 
							MESG_CHAN_CLOSED);
		mesg_cleanup(cell, MESG_CHANNEL_MAIN);
		cell_disconnect(cell);
		part_deactivate(CELLID_TO_PARTID(cell));
	}
	cmn_err(CE_NOTE, "Signaling end of recovery cell %d\n", cellid());
	cms_mark_recovery_complete();
}
