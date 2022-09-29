/**************************************************************************
 *									  *
 *		Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: psem.c,v 1.7 1999/05/19 20:21:21 bcasavan Exp $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/mac_label.h>
#include <sys/idbgentry.h>
#include <ksys/vproc.h>	/* for current_pid() */

#include <sys/sem.h>
#include <ksys/vsem.h>

#include "vsem_mgr.h"
#include "vsem_private.h"
#include "psem.h"

/* tuneables */
extern int semmsl;
extern int semvmx;
extern int semaem;

#define PSEMN	(PZERO + 3)	/* sleep priority waiting for greater value */
#define PSEMZ	(PZERO + 2)	/* sleep priority waiting for zero */

int
psem_alloc(
	int		key,
	int		flags,
	int		nsems,
	struct cred	*cred,
	vsem_t		*vsem)
{
	psem_t		*psem;
	size_t		semsize;

	if (nsems <= 0 || nsems > semmsl)
		return(EINVAL);

	/*
	 * Allocate the descriptor and initialize the ipc_perm
	 * structure from the creds of process that caused this creation.
	 */
	semsize = sizeof(psem_t) + (sizeof(struct svsem) * nsems);
	psem = kmem_zalloc(semsize, KM_SLEEP);
	if (psem == NULL)
		return(ENOSPC);

	psem->psm_semarray = (svsem_t *)(psem + 1);

	ipc_init(&psem->psm_perm, key, flags, cred);

	/*
	 * initialize the descriptor.
	 */
	psem->psm_nsems = nsems;
	psem->psm_optime = 0;
	psem->psm_ctime = time;
	psem->psm_maclabel = cred->cr_mac;
	psem->psm_undo = NULL;
	init_mutex(&psem->psm_lock, MUTEX_DEFAULT, "psem", vsem->vsm_id);

	/*
	 * Initialize the behavior descriptor and put it on the chain.
	 */
	bhv_desc_init(&psem->psm_bd, psem, vsem, &psem_ops);
	bhv_insert_initial(&vsem->vsm_bh, &psem->psm_bd);

	/* Return success */
	return(0);
}

/*
 * Free a semaphore
 */
void
psem_free(
	psem_t		*psem)
{
	int		nsems;
	semundo_t	*su;
	size_t		semsize;

	nsems = psem->psm_nsems;
	while (su = psem->psm_undo) {
		psem->psm_undo = su->su_next;
		kmem_free(su, sizeof(semundo_t) + (sizeof(short) * nsems));
	}
	mutex_destroy(&psem->psm_lock);
	semsize = sizeof(psem_t) + (sizeof(struct svsem) * nsems);
	kmem_free(psem, semsize);
}

static int
psem_add_exitop(
	psem_t		*psem,	
	int		semnum,
	short		semop)
{
	semundo_t	*su;
	pid_t		curpid = current_pid();

	if (semop > semaem || semop < -semaem) 
		return(ERANGE);

	for (su = psem->psm_undo; su != NULL; su = su->su_next) {
		if (su->su_pid == curpid)
			break;
	}
	if (su == NULL) {
		su = kmem_zalloc(sizeof(semundo_t) +
				 (sizeof(short) * psem->psm_nsems), KM_SLEEP);
		su->su_entry = (short *)(su + 1);
		su->su_next = psem->psm_undo;
		psem->psm_undo = su;
		su->su_pid = curpid;
	}

	su->su_entry[semnum] -= semop;
	if (su->su_entry[semnum] > semaem || su->su_entry[semnum] < -semaem) {
		su->su_entry[semnum] += semop;
		return(ERANGE);
	}
	return(0);
}

/* psem_undo_op
 *
 * Implement undo operation for SysV semaphores.
 */

static void
psem_undo_op(
	psem_t		*psem,	
	struct sembuf	*ops,
	int		nops)
{
	svsem_t		*sem;
	int		i;
	int		wakeup;

	/* Undo all the operations up to 'nops' */
	for (i = 0; i < nops; i++, ops++) {
		if (ops->sem_op == 0)
			continue;
		sem = &psem->psm_semarray[ops->sem_num];
		sem->svs_val -= ops->sem_op;	/* Undo the operation */
		if (ops->sem_op < 0) {
			/* Just incremented the semaphore during the undo */
			/* so let some other waiter take its chance. */
			if(sem->svs_flags & SVSEM_WAITMULTI) {
				/* Wake up all threads if anyone is sleeping */
				/* on a value of 2 or more */
				sem->svs_flags &= ~((ushort_t) SVSEM_WAITMULTI);
				sv_broadcast(&sem->svs_nwait);
			} else {
				/* Only wake up as many threads as can      */
				/* consume the semaphore if all threads are */
				/* sleeping on a value of 1. This is an     */
				/* optimization for the typical case.       */
				for (wakeup = 0; wakeup < -ops->sem_op; wakeup++) {
					sv_signal(&sem->svs_nwait);
				}
			}
			/* If the value is now zero wake up threads waiting */
			/* on a value of zero. */
			if (sem->svs_val == 0) {
				sv_broadcast(&sem->svs_zwait);
			}
		}
		/* The undo-at-exit option was specified, so undo the */
		/* undo-at-exit operation which was set in psem_op in */
		/* case the thread is killed before coming out of the */
		/* wait. Confusing, eh? */
		if (ops->sem_flg & SEM_UNDO)
			(void)psem_add_exitop(psem, ops->sem_num, -ops->sem_op);
	}
}

static void
psem_remove_exitops(
	psem_t		*psem,		/* semaphore being changed */
	ushort		low,		/* lowest semaphore being changed */
	ushort		high)		/* highest semaphore being changed */
{
	semundo_t	*undotab;
	int		i;

	for (undotab = psem->psm_undo;
	     undotab != NULL;
	     undotab = undotab->su_next) {
		for (i = low; i <= high; i++)
			undotab->su_entry[i] = 0;
	}
}

int
psem_keycheck(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	int		flags,
	int		nsems)
{
	psem_t		*psem;
	int		error = 0;

	psem = BHV_TO_PSEM(bdp);

	/*
	 * Check for exclusive access
	 */
	if ((flags & (IPC_CREAT | IPC_EXCL)) ==
	    (IPC_CREAT | IPC_EXCL)) {
		return(EEXIST);
	}

	/*
	 * check for access mode
	 */
	mutex_lock(&psem->psm_lock, PZERO);
	if ((flags & 0777) & ~(psem->psm_perm.mode)) {
		error = EACCES;
		goto out;
	}

	/*
	 * Check the callers creds
	 */
	if (error = _MAC_ACCESS(psem->psm_maclabel, cred, MACWRITE)) {
		error = EACCES;
		goto out;
	}

	/*
	 * check for nsems
	 */
	if (nsems < 0 || (nsems > 0 && psem->psm_nsems < nsems))
		error = EINVAL;

out:
	ASSERT(mutex_mine(&psem->psm_lock));
	mutex_unlock(&psem->psm_lock);
	return(error);
}

int
psem_rmid(
	bhv_desc_t	*bdp,
	struct cred	*cred)
{
	psem_t		*psem;
	vsem_t		*vsem;
	svsem_t		*sem;
	int		i;
	int		error = 0;

	psem = BHV_TO_PSEM(bdp);
	vsem = BHV_TO_VSEM(bdp);

	mutex_lock(&psem->psm_lock, PZERO);

	/* check permissions */
	if (cred->cr_uid != psem->psm_perm.uid &&
	    cred->cr_uid != psem->psm_perm.cuid &&
	    !_CAP_CRABLE(cred, CAP_FOWNER)) {
		error = EPERM;
		mutex_unlock(&psem->psm_lock);
	} else if (psem->psm_perm.mode & IPC_ALLOC) {
		psem->psm_perm.mode &= ~IPC_ALLOC;
		for (i = 0; i < psem->psm_nsems; i++) {
			sem = &psem->psm_semarray[i];
			sem->svs_val = 0;
			sem->svs_pid = 0;
			sem->svs_flags = 0;
			sv_broadcast(&sem->svs_nwait);
			sv_broadcast(&sem->svs_zwait);
		}
		mutex_unlock(&psem->psm_lock);
		/*
		 * Remove vsem from lookup tables
		 */
		vsem_remove(vsem, 1);
	} else
		mutex_unlock(&psem->psm_lock);
	return(error);
}

int
psem_ipcset(
	bhv_desc_t	*bdp,
	struct ipc_perm *perm,
	struct cred	*cred)
{
	psem_t		*psem;
	int		error = 0;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	/* check permissions */
	if (cred->cr_uid != psem->psm_perm.uid &&
	    cred->cr_uid != psem->psm_perm.cuid &&
	    !_CAP_CRABLE(cred, CAP_FOWNER)) {
		error = EPERM;
	} else {
		psem->psm_perm.uid = perm->uid;
		psem->psm_perm.gid = perm->gid;
		psem->psm_perm.mode = (perm->mode & 0777) |
				    (psem->psm_perm.mode & ~0777);
		psem->psm_ctime = time;
	}
	ASSERT(mutex_mine(&psem->psm_lock));
	mutex_unlock(&psem->psm_lock);
	return(error);
}

int
psem_getstat(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	struct semid_ds	*semds,
	cell_t		*cell)
{
	psem_t		*psem;
	int		error;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	if (error = ipcaccess(&psem->psm_perm, cred, SEM_R)) {
		mutex_unlock(&psem->psm_lock);
		return(error);
	}

	semds->sem_perm = psem->psm_perm;
	semds->sem_base = psem->psm_semarray;
        semds->sem_nsems = psem->psm_nsems;
        semds->sem_otime = psem->psm_optime;
        semds->sem_ctime = psem->psm_ctime;
	mutex_unlock(&psem->psm_lock);

	*cell = cellid();

	return(0);
}

int
psem_getncnt(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	int		semnum,
	int		*cnt)
{
	psem_t		*psem;
	svsem_t		*sem;
	int		error;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	if (error = ipcaccess(&psem->psm_perm, cred, SEM_R)) {
		mutex_unlock(&psem->psm_lock);
		return(error);
	}

	sem = &psem->psm_semarray[semnum];
	*cnt = sv_waitq(&sem->svs_nwait);
	mutex_unlock(&psem->psm_lock);
	return(0);
}

int
psem_getpid(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	int		semnum,
	pid_t		*pid)
{
	psem_t		*psem;
	svsem_t		*sem;
	int		error;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	if (error = ipcaccess(&psem->psm_perm, cred, SEM_R)) {
		mutex_unlock(&psem->psm_lock);
		return(error);
	}

	sem = &psem->psm_semarray[semnum];
	*pid = sem->svs_pid;
	mutex_unlock(&psem->psm_lock);
	return(0);
}

int
psem_getval(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	int		semnum,
	ushort_t	*semval)
{
	psem_t		*psem;
	svsem_t		*sem;
	int		error;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	if (error = ipcaccess(&psem->psm_perm, cred, SEM_R)) {
		mutex_unlock(&psem->psm_lock);
		return(error);
	}

	sem = &psem->psm_semarray[semnum];
	*semval = sem->svs_val;
	mutex_unlock(&psem->psm_lock);
	return(0);
}

int
psem_getzcnt(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	int		semnum,
	int		*zcnt)
{
	psem_t		*psem;
	svsem_t		*sem;
	int		error;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	if (error = ipcaccess(&psem->psm_perm, cred, SEM_R)) {
		mutex_unlock(&psem->psm_lock);
		return(error);
	}

	sem = &psem->psm_semarray[semnum];
	*zcnt = sv_waitq(&sem->svs_zwait);
	mutex_unlock(&psem->psm_lock);
	return(0);
}

int
psem_getall(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	ushort		*semvals)
{
	psem_t		*psem;
	svsem_t		*sem;
	int		error;
	int		i;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	if (error = ipcaccess(&psem->psm_perm, cred, SEM_R)) {
		mutex_unlock(&psem->psm_lock);
		return(error);
	}
	for (i = 0; i < psem->psm_nsems; i++) {
		sem = &psem->psm_semarray[i];
		semvals[i] = sem->svs_val;
	}
	mutex_unlock(&psem->psm_lock);
	return(0);
}

int
psem_setval(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	int		semnum,
	ushort_t	semval)
{
	psem_t		*psem;
	int		error;
	svsem_t		*sem;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	if (error = ipcaccess(&psem->psm_perm, cred, SEM_A)) {
		mutex_unlock(&psem->psm_lock);
		return(error);
	}

	if (semnum >= psem->psm_nsems) {
		mutex_unlock(&psem->psm_lock);
		return(EINVAL);
	}

	if ((unsigned)semval > semvmx) {
		mutex_unlock(&psem->psm_lock);
		return(ERANGE);
	}

	sem = &psem->psm_semarray[semnum];
	sem->svs_val = semval;
	sem->svs_pid = current_pid();
	psem_remove_exitops(psem, semnum, semnum);
	mutex_unlock(&psem->psm_lock);
	if (semval)
		sv_broadcast(&sem->svs_nwait);
	else
		sv_broadcast(&sem->svs_zwait);
	return(0);
}

int
psem_setall(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	ushort		*semvals)
{
	psem_t		*psem;
	svsem_t		*sem;
	int		error;
	int		i;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	if (error = ipcaccess(&psem->psm_perm, cred, SEM_A)) {
		mutex_unlock(&psem->psm_lock);
		return(error);
	}

	for (i = 0; i < psem->psm_nsems; i++) {
		if (semvals[i] > semvmx) {
			mutex_unlock(&psem->psm_lock);
			return(ERANGE);
		}
	}
	psem_remove_exitops(psem, 0, psem->psm_nsems);
	for (i = 0; i < psem->psm_nsems; i++) {
		sem = &psem->psm_semarray[i];
		sem->svs_val = semvals[i];
		if (sem->svs_val)
			sv_broadcast(&sem->svs_nwait);
		else
			sv_broadcast(&sem->svs_zwait);
	}
	mutex_unlock(&psem->psm_lock);
	return(0);
}

/* psem_op
 *
 * Implement SysV semaphore increments, decrements, and value waits.
 */
int
psem_op(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	struct sembuf	*sembuf,
	int		nops)
{
	psem_t		*psem;
	struct sembuf	*op;
	svsem_t		*svsem;
	int		i;
	int		error = 0;
	pid_t		curpid;
	int		wakeup;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	/* Check the access permissions for all the semaphores in the */
	/* list of operations. */
	for (i = 0; i < nops; i++) {
		op = &sembuf[i];
                if (error = ipcaccess(&psem->psm_perm, cred,
					op->sem_op? SEM_A : SEM_R))
			goto op_out;
	}

restart:
	/* Do all operations specified in the set */
	for (i = 0; i < nops; i++) {
		op = &sembuf[i];
		svsem = &psem->psm_semarray[op->sem_num];
		if (op->sem_op > 0) {	/* Increment semaphore */
			/* Make sure we don't overflow the max value */
			if (op->sem_op + svsem->svs_val > semvmx) {
				error = ERANGE;
				if (i)
					psem_undo_op(psem, sembuf, i);
				goto op_out;
			}
			if (op->sem_flg & SEM_UNDO) {
				/* Set up the undo-at-exit operation. */
				error = psem_add_exitop(psem, op->sem_num,
							op->sem_op);
				if (error) {
					/*
					 * undo all the operations done
					 * so far
					 */
					if (i)
						psem_undo_op(psem, sembuf, i);
					goto op_out;
				}
			}

			svsem->svs_val += op->sem_op;
			if(svsem->svs_flags & SVSEM_WAITMULTI) {
				/* Wake up all threads if anyone is sleeping */
				/* on a value of 2 or more */
				svsem->svs_flags &= ~((ushort_t) SVSEM_WAITMULTI);
				sv_broadcast(&svsem->svs_nwait);
			} else {
				/* Only wake up as many threads as     */
				/* necessary to consume the semaphore. */
				/* This only happens if all sleeping   */
				/* threads are waiting on a value of 1 */
				/* or 0. This is an optimization for   */
				/* the typical case. */
                        	for (wakeup = 0; wakeup < op->sem_op; wakeup++)
                               		sv_signal(&svsem->svs_nwait);
			}

			/* If this operation made the semaphore value zero   */
			/* then wake any threads waiting on a value of zero. */

			/* XXX - bcasavan: The semaphore can never be  */
			/* negative, and we just incremented it, so is */
			/* this situation even possible? */
			if (svsem->svs_val == 0)
				sv_broadcast(&svsem->svs_zwait);
			continue;
		}
		if (op->sem_op < 0) {	/* Wait operation */
			/* Ready to proceed on this semaphore */
			if (svsem->svs_val >= -op->sem_op) {
				if (op->sem_flg & SEM_UNDO) {
					/* Set up the undo-at-exit operation. */
					error = psem_add_exitop(psem,
								op->sem_num,
								op->sem_op);
					if (error) {
						/*
						 * undo all the operations done
						 * so far
						 */
						if (i)
							psem_undo_op(psem,
								sembuf, i);
						goto op_out;
					}
				}

				/* If this operation made the semaphore     */
				/* value zero then wake any threads waiting */
				/* on a value of zero. */
				svsem->svs_val += op->sem_op;
				if (svsem->svs_val == 0)
					sv_broadcast(&svsem->svs_zwait);
				continue;
			}
			/* If we are going to wait on this semaphore, but */
			/* have already succeeded in other operations in  */
			/* this set then undo those operations before     */
			/* going to sleep. */
			if (i)
				psem_undo_op(psem, sembuf, i);
			/* If the thread can't wait then return an error */
			if (op->sem_flg & IPC_NOWAIT) {
				error = EAGAIN;
				goto op_out;
			}

			/* If this thread is going to wait on a value of  */
			/* op->sem_op which is 2 or more, then we need to */
			/* set the SVSEM_WAITMULTI flag. When this flag   */
			/* is set the incrementing thread will broadcast  */
			/* to all waiting threads instead of waking only  */
			/* a few. This provides high performance for the  */
			/* typical case (all waiting threads waiting on a */
			/* value of 1), and provides correct (but slower) */
			/* behavior in the atypical case.		  */
			if(op->sem_op <= -2)
				svsem->svs_flags |= SVSEM_WAITMULTI;

			/* Thread needs to wait for this semaphore, so */
			/* put it to sleep. */
			if (sv_wait_sig(&svsem->svs_nwait, PSEMN,
					&psem->psm_lock, 0) == -1)
				return(EINTR);

			/* Reaquire semaphore lock */
			mutex_lock(&psem->psm_lock, PZERO);
			if (!(psem->psm_perm.mode & IPC_ALLOC)) {
				mutex_unlock(&psem->psm_lock);
				return(EIDRM);
			}
			goto restart;	/* Start the lock operations again */
		}
		/* Wait on a value of zero */
		if (svsem->svs_val) {
			/* Value is non-zero, so thread will need to sleep. */

			/* If this isn't the first semaphore in the list */
			/* release any semaphores the thread has obtained */
			/* before getting to this point. */
			if (i)
				psem_undo_op(psem, sembuf, i);
			/* If thread can't wait return an error */
			if (op->sem_flg & IPC_NOWAIT) {
				error = EAGAIN;
				goto op_out;
			}
			/* Wait on the value of zero */
			if (sv_wait_sig(&svsem->svs_zwait, PSEMZ,
					&psem->psm_lock, 0) == -1)
				return(EINTR);

			/* Reacquire the semaphore lock */
			mutex_lock(&psem->psm_lock, PZERO);
			if (!(psem->psm_perm.mode & IPC_ALLOC)) {
				mutex_unlock(&psem->psm_lock);
				return(EIDRM);
			}
			goto restart;	/* Start the lock operations again */
		}
	}

	/* All operations succeeded.  Update sempid for accessed semaphores. */
	curpid = current_pid();
	for (i = 0; i < nops; i++) {
		svsem = &psem->psm_semarray[sembuf[i].sem_num];
		svsem->svs_pid = curpid;
	}
	/* Update semaphore access time */
	psem->psm_optime = time;

op_out:
	mutex_unlock(&psem->psm_lock);	/* Release the semaphore lock */
	return(error);
}

void
psem_exit(
	bhv_desc_t	*bdp,
	pid_t		pid)
{
	psem_t		*psem;
	semundo_t	*su;
	semundo_t	**prev_suptr;
	int		nsems;
	int		i;
	svsem_t		*sem;
	short		val;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);
	nsems = psem->psm_nsems;

	prev_suptr = &psem->psm_undo;
	for (su = psem->psm_undo; su != NULL; su = su->su_next) {
		if (su->su_pid == pid) {
			*prev_suptr = su->su_next;
			break;
		}
		prev_suptr = &su->su_next;	
	}
	if (su == NULL) {
		mutex_unlock(&psem->psm_lock);
		return;
	}
	for (i = 0; i < nsems; i++) {
		if (su->su_entry[i] == 0)
			continue;
		sem = &psem->psm_semarray[i];
		val = sem->svs_val + su->su_entry[i];
		if (val < 0 || val > semvmx)
			continue;
		sem->svs_val = val;
		if (val == 0)
			sv_broadcast(&sem->svs_zwait);
		if (su->su_entry[i] > 0)
			sv_broadcast(&sem->svs_nwait);
	}
	mutex_unlock(&psem->psm_lock);
	kmem_free(su, sizeof(semundo_t) + (sizeof(short) * nsems));
}

int
psem_mac_access(
	bhv_desc_t	*bdp,
	struct cred	*cred)
{
	psem_t		*psem;
	int		error;

	psem = BHV_TO_PSEM(bdp);
	mutex_lock(&psem->psm_lock, PZERO);

	error = _MAC_ACCESS(psem->psm_maclabel, cred, MACWRITE);

	mutex_unlock(&psem->psm_lock);
	return(error);
}

void
psem_destroy(
	bhv_desc_t	*bdp)
{
	psem_t		*psem;
	vsem_t		*vsem;

	psem = BHV_TO_PSEM(bdp);
	vsem = BHV_TO_VSEM(bdp);

	ASSERT(vsem->vsm_refcnt == 0);
	vsem_freeid(vsem->vsm_id);

	bhv_remove(&vsem->vsm_bh, &psem->psm_bd);
	psem_free(psem);
	bhv_head_destroy(&vsem->vsm_bh);
	kmem_zone_free(vsem_zone, vsem);
}

semops_t psem_ops = {
	BHV_IDENTITY_INIT_POSITION(VSEM_POSITION_PSEM),
	psem_keycheck,
	psem_rmid,
	psem_ipcset,
	psem_getstat,
	psem_getncnt,
	psem_getpid,
	psem_getval,
	psem_getzcnt,
	psem_getall,
	psem_setval,
	psem_setall,
	psem_op,
	psem_exit,
	psem_mac_access,
	psem_destroy,
};

#if DEBUG || SEMDEBUG
void
idbg_psem_print(psem_t *psem)
{
	qprintf("psem 0x%x:\n", psem);
	qprintf("    mode 0%o uid %d gid %d cuid %d cgid %d\n",
		psem->psm_perm.mode, psem->psm_perm.uid, psem->psm_perm.gid,
		psem->psm_perm.cuid, psem->psm_perm.cgid);
	qprintf("    psm_nsems 0x%x psm_semarray 0x%x\n",
		psem->psm_nsems, psem->psm_semarray);
	qprintf("    lock 0x%x undo 0x%x\n", &psem->psm_lock, psem->psm_undo);
}
#endif
