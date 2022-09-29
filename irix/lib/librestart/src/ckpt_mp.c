/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/prctl.h>
#include <ckpt.h>
#include <ckpt_internal.h>
#include "librestart.h"

/*
 * The text and data here are compiled to a reserved area (0x300000 - 0x400000)
 * to avoid address conflicts with the target process.
 */

ckpt_pl_t *proclist = (ckpt_pl_t *)(CKPT_PLRESERVE+4*(sizeof (ulong *)));
static unsigned long *creates_started = (unsigned long *)(CKPT_PLRESERVE);

/*
 * Convert virtual pid to physical pid
 */
pid_t
vtoppid_low(pid_t vpid)
{
        ckpt_pl_t *pl = proclist;
	int i;

        for (i = 0; i < *creates_started; i++) {
                if (pl->pl_opid == vpid)
                        return (pl->pl_pid);
                pl++;
        }
	ckpt_perror("Failed to translate pid from VTOP", ENOENT);
	return (-1);
}

void
ckpt_update_proclist_states(unsigned long pindex, uint_t update)
{
	proclist[pindex].pl_states |= update;
	if (update == CKPT_PL_ERR)
                proclist[pindex].pl_errno = ERRNO;
}

/*
 * Convert a pid to its address in a list
 */
pid_t *
pidtoaddr(pid_t pid, pid_t *pidlist, unsigned long limit)
{
	pid_t *ppid;

	for (ppid = pidlist; ppid < &pidlist[limit]; ppid++)
		if (*ppid == pid)
			return (ppid);
	/*
	 * should never happen
	 */
	return (NULL);
}
/*
 * Low level sync primitives
 */
int
ckpt_sem_wait(void *vaddr)
{
	usync_arg_t uarg;

	uarg.ua_version = USYNC_VERSION_2;
	uarg.ua_addr = (__uint64_t)vaddr;
	uarg.ua_policy = USYNC_POLICY_FIFO;
	uarg.ua_flags = 0;

	return (usync_cntl(USYNC_INTR_BLOCK, &uarg));
}
int
ckpt_sem_post(void *vaddr)
{
	usync_arg_t uarg;

	uarg.ua_version = USYNC_VERSION_2;
	uarg.ua_addr = (__uint64_t)vaddr;
	uarg.ua_policy = USYNC_POLICY_FIFO;
	uarg.ua_flags = 0;

	return (usync_cntl(USYNC_UNBLOCK, &uarg));
}
/*
 * Barrier for sprocs.  Insure all have hit the barrier before any continue
 */
int
ckpt_sproc_block(pid_t client, pid_t *list, unsigned long count, unsigned long *sync)
{
	unsigned long index;

	index = ckpt_add_then_test(sync, 1);

	if (index != count)
		return (ckpt_sem_wait(pidtoaddr(client, list, count)));

	*sync = 0;

	return (1);
}

int
ckpt_sproc_unblock(pid_t ignore, unsigned long count, pid_t *list)
{
	pid_t *client;
	unsigned long index;

	for (index = 0; index < count; index++) {

		client = &list[index];

		if (*client == ignore)
			continue;

		if (ckpt_sem_post(client) < 0)
			return (-1);

	}
	return (0);
}
/*
 * Let caller know if they are last one haring the address space through
 * here.  This will need work if/when pthreads run in parrallel during restart
 */
int
ckpt_exit_barrier(void)
{
	if (saddrlist) {
		unsigned long index;
		unsigned long count;

		count = saddrlist->saddr_count;

		index = ckpt_add_then_test(&saddrlist->saddr_sync, 1);
		/*
		 * Don't reference sddrlist here!
		 */
		return (index == count);
	} else
		return (1);
	/* 
	 * NOTREACHED
	 */
}
/*
 * barrier for shared id sprocs
 */
int
ckpt_sid_barrier(ckpt_pi_t *pip, pid_t client)
{
	int rc;
	/*
	 * If this is a candidate for changing creds, log it's
	 * pid.  Don't care about race.  Any elligible member will
	 * do
	 */
	if (pip->cp_psi.ps_shflags & CKPT_PS_CRED)
		sidlist->sid_assigned = client;

	rc = ckpt_sproc_block(	client,
				sidlist->sid_pid,
				sidlist->sid_count,
				&sidlist->sid_sync);

	if (rc < 0)
		/*
		 * Error
		 */
		return (rc);

	if ((rc > 0)&&(sidlist->sid_assigned == NULL))
		/*
		 * no elligible cred changers...default
		 * to current sproc
		 */
		sidlist->sid_assigned = client;

	if ((rc > 0)&&(client != sidlist->sid_assigned)) {
		/*
		 * everyone is here, but this member isn't
		 * elligible
		 */
		pid_t *list = sidlist->sid_pid;
		unsigned long count = sidlist->sid_count;

		if (ckpt_sem_post(pidtoaddr(sidlist->sid_assigned, list, count)) < 0)
			return (-1);

		if (ckpt_sem_wait(pidtoaddr(client, list, count)) < 0)
			return (-1);

	}
	if (client != sidlist->sid_assigned)
		/*
		 * One of our share buddies took care of ids
		 */
		return (0);

	/*
	 * it's our job
	 */
	return (1);
}

int
ckpt_mutex_acquire(ckpt_mutex_t *mutex)
{
	ckpt_spinlock(&mutex->lock);
	while (mutex->busy != 0) {
		mutex->waiters++;
		ckpt_spinunlock(&mutex->lock);
		ckpt_sem_wait(mutex);
		ckpt_spinlock(&mutex->lock);
		mutex->waiters--;
	}
	mutex->busy = 1;
	ckpt_spinunlock(&mutex->lock);
	return (0);
}

int
ckpt_mutex_rele(ckpt_mutex_t *mutex)
{
	ckpt_spinlock(&mutex->lock);
	mutex->busy = 0;
	if (mutex->waiters != 0)
		ckpt_sem_post(mutex);
	ckpt_spinunlock(&mutex->lock);
	return (0);
}
