/* Copyright 1986-1997, Silicon Graphics Inc., Mountain View, CA. */
/*	Copyright (c) 1984 AT&T	*/
/*	All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <sys/xlate.h>
#include <sys/sat.h>
#include <sys/kmem.h>
#include <sys/ksa.h>
#include <sys/proc.h>
#include <sys/uthread.h>
#include <ksys/vproc.h>
#include <sys/kipc.h>

#include <sys/sem.h>
#include <ksys/vsem.h>

extern int semopm;

#define	NSEMUNDO_IDS	10
typedef struct semprocundo {
	struct semprocundo	*pu_next;
	int			pu_nids;
	int			pu_uids[NSEMUNDO_IDS];
} semprocundo_t;

struct semctla {
	sysarg_t	semid;
	usysarg_t	semnum;
	sysarg_t	cmd;
	sysarg_t	arg;
};

#if _MIPS_SIM == _ABI64
struct semctla_n32 {
	sysarg_t	semid;
	usysarg_t	semnum;
	sysarg_t	cmd;
	__uint32_t	arghi;
	__uint32_t	arglo;
};

/* ARGSUSED */
static int
irix5_to_semid(
	enum xlate_mode	mode,
	void		*to,
	int		count,
	xlate_info_t	*info)
{
	struct semid_ds *ds;
	struct irix5_semid_ds *i5_ds;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_semid_ds) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
					    sizeof(struct irix5_semid_ds));
		info->copysize = sizeof(struct irix5_semid_ds);
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_semid_ds));
	ASSERT(info->copybuf != NULL);

	ds = to;
	i5_ds = info->copybuf;

	irix5_to_ipc_perm(&i5_ds->sem_perm, &ds->sem_perm);
	ds->sem_base = (void *)(__psunsigned_t)i5_ds->sem_base;
	ds->sem_nsems = i5_ds->sem_nsems;
	ds->sem_otime = i5_ds->sem_otime;
	ds->sem_ctime = i5_ds->sem_ctime;

	return 0;
}


/* ARGSUSED */
static int
semid_to_irix5(
	void		*from,
	int		count,
	xlate_info_t	*info)
{
	struct irix5_semid_ds *i5_ds;
	struct semid_ds	*ds = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_semid_ds) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_semid_ds));
	info->copysize = sizeof(struct irix5_semid_ds);

	i5_ds = info->copybuf;

	ipc_perm_to_irix5(&ds->sem_perm, &i5_ds->sem_perm);
	i5_ds->sem_base = (app32_ptr_t)(__psint_t)ds->sem_base;
	i5_ds->sem_nsems = ds->sem_nsems;
	i5_ds->sem_otime = ds->sem_otime;
	i5_ds->sem_ctime = ds->sem_ctime;

	return 0;
}

#endif /* _MIPS_SIM == _ABI64 */

/* ARGSUSED */
static void
semexit(
	void		*arg)
{
	vsem_t		*vsem;
	proc_proxy_t	*prxy;
	semprocundo_t	*pu;
	int		i;
	int		error;

	prxy = curuthread->ut_pproxy;

	while (prxy->prxy_semundo) {
		pu = prxy->prxy_semundo;
		for (i = 0; i < pu->pu_nids; i++) {
			error = vsem_lookup_id(pu->pu_uids[i], &vsem);
			if (error)
				continue;
			VSEM_EXIT(vsem, current_pid());
			vsem_rele(vsem);
		}
		prxy->prxy_semundo = pu->pu_next;
		kmem_free(pu, sizeof(semprocundo_t));
		
	}
}

void
sem_register_undo(
	int		semid)
{
	proc_proxy_t	*prxy;
	semprocundo_t	*pu;
	int		i;

	prxy = curuthread->ut_pproxy;

	mutex_lock(&prxy->prxy_semlock, PZERO);
	if (prxy->prxy_semundo == NULL) {
		prxy->prxy_semundo = kmem_zalloc(sizeof(semprocundo_t),
					KM_SLEEP);
		add_exit_callback(current_pid(), 0, semexit, 0);
	}

	for (pu = prxy->prxy_semundo; ; pu = pu->pu_next) {
		for (i = 0; i < pu->pu_nids; i++) {
			if (pu->pu_uids[i] == semid) {
				mutex_unlock(&prxy->prxy_semlock);
				return;
			}
		}
		if (pu->pu_nids != NSEMUNDO_IDS) {
			pu->pu_uids[pu->pu_nids] = semid;
			pu->pu_nids++;
			break;
		}
		if (pu->pu_next == NULL)
			pu->pu_next = kmem_zalloc(sizeof(semprocundo_t),
					KM_SLEEP);
	}
	mutex_unlock(&prxy->prxy_semlock);
}

int
semctl(
	struct semctla	*uap,
	rval_t		*rvp)
{
	vsem_t		*vsem;
	int		error;
	cred_t		*cred = get_current_cred();
#if _MIPS_SIM == _ABI64
	int		abi = get_current_abi();
#endif
	sysarg_t	uaparg;

	if ((error = vsem_lookup_id(uap->semid, &vsem)) != 0)
		return(error);

#if THISISNECESSARY
	error = VSEM_MAC_ACCESS(vsem, cred);
	if (error) {
		vsem_rele(vsem);
		return(EACCES);
	}
#endif

#if _MIPS_SIM == _ABI64
	if (ABI_IS_IRIX5_N32(abi))
		uaparg = ((struct semctla_n32 *)uap)->arghi;
	else
#endif
		uaparg = uap->arg;


	switch(uap->cmd) {
	case IPC_RMID:		/* Remove semaphore set. */
		error = VSEM_RMID(vsem, cred);
		/* returns with one last reference */
		break;

	case	IPC_SET:	/* Set ownership and permissions */
	    {
		struct semid_ds	ds;

		if (COPYIN_XLATE((void *)uaparg, &ds, sizeof(ds),
				irix5_to_semid, abi, 1)) {
			error = EFAULT;
			break;
		}

		error = VSEM_IPCSET(vsem, &ds.sem_perm, cred);
		break;
	    }

	case	IPC_STAT:	/* Get semaphore data structure. */
	    {
		struct semid_ds	sp;
		cell_t	cell;

		error = VSEM_GETSTAT(vsem, cred, &sp, &cell);

		if (error == 0 && XLATE_COPYOUT(&sp, (void *)uaparg,
					sizeof(sp), semid_to_irix5, abi, 1))
			error = EFAULT;
		break;
	    }

	case	GETNCNT:	/* Get # of procs sleeping for greater semval */
	    {
		int	cnt;

		if (uap->semnum >= vsem->vsm_nsems) {
			error = EINVAL;
			break;
		}

		error = VSEM_GETNCNT(vsem, cred, uap->semnum, &cnt);

		rvp->r_val1 = cnt;
		break;
	    }

	case	GETPID:		/* Get pid of last proc to do an op */
	    {
		pid_t	pid;

		if (uap->semnum >= vsem->vsm_nsems) {
			error = EINVAL;
			break;
		}

		error = VSEM_GETPID(vsem, cred, uap->semnum, &pid);

		rvp->r_val1 = pid;
		break;
	    }

	case	GETVAL:		/* Get semval of one semaphore. */
	    {
		ushort_t	semval;

		if (uap->semnum >= vsem->vsm_nsems) {
			error = EINVAL;
			break;
		}

		error = VSEM_GETVAL(vsem, cred, uap->semnum, &semval);

		rvp->r_val1 = semval;
		break;
	    }

	case	GETZCNT:	/* # of procs sleeping for semval to goto 0 */
	    {
		int	zcnt;

		if (uap->semnum >= vsem->vsm_nsems) {
			error = EINVAL;
			break;
		}

		error = VSEM_GETZCNT(vsem, cred, uap->semnum, &zcnt);

		rvp->r_val1 = zcnt;
		break;
	    }

	case	SETVAL:		/* Set semval of one semaphore. */
	    {
		int	semval;

#if _MIPS_SIM == _ABI64
		/* Unions of ptrs and ints are tricky in the LP64 model */
		if (ABI_IS_IRIX5_N32(abi) ||
		    ABI_IS_IRIX5_64(abi))
			semval = ((union semun *)&uap->arg)->val;
		else
#endif
			semval = uap->arg;

		error = VSEM_SETVAL(vsem, cred, uap->semnum, semval);
		break;
	    }

	case	GETALL:
	    {
		ushort	*semvals;
		size_t	semvalsz;

		semvalsz = sizeof(ushort) * vsem->vsm_nsems;
		semvals = kmem_alloc(semvalsz, KM_SLEEP);

		error = VSEM_GETALL(vsem, cred, semvals);

		if (!error)
			if (copyout(semvals, (caddr_t)uaparg, semvalsz))
				error = EFAULT;

		kmem_free(semvals, semvalsz);
		break;
	    }

	case	SETALL:		/* Set semvals of all semaphores in set. */
	    {
		ushort	*semvals;
		size_t	semvalsz;

		semvalsz = sizeof(ushort) * vsem->vsm_nsems;
		semvals = kmem_alloc(semvalsz, KM_SLEEP);

		if (copyin((caddr_t)uaparg, semvals, semvalsz)) {
			error = EFAULT;
		} else
			error = VSEM_SETALL(vsem, cred, semvals);

		kmem_free(semvals, semvalsz);
		break;
	    }

	default:
		error = EINVAL;
		break;
	}

	vsem_rele(vsem);
	return(error);
}
/*
 * semget - Semget system call.
 */

struct semgeta {
	sysarg_t	key;
	sysarg_t	nsems;
	sysarg_t	semflg;
};

static int
semget(
	struct semgeta	*uap,
	rval_t		*rvp)
{
	vsem_t		*vsem;
	int		error;

	if (uap->key == IPC_PRIVATE) {
		error = vsem_create(uap->key, uap->semflg|IPC_CREAT, uap->nsems,
				get_current_cred(), &vsem);
	} else {
		error = vsem_lookup_key(uap->key, uap->semflg, uap->nsems,
				get_current_cred(), &vsem);
	}
	if (error)
		return(error);

	rvp->r_val1 = vsem->vsm_id;

	vsem_rele(vsem);

	return(0);
}

struct semopa {
	sysarg_t	semid;
	struct sembuf	*sops;
	usysarg_t	nsops;
};

/*
 * semop - Semop system call.
 */
/* ARGSUSED */
static int
semop(
	struct semopa	*uap,
	rval_t		*rvp)
{
	int		error;
	vsem_t		*vsem;
#define	SEMOPS_BUFSZ	20
	struct sembuf	semops_buf[SEMOPS_BUFSZ];
	struct sembuf	*semops;
	struct sembuf	*op;
	int		i;
	int		register_undo = 0;

	SYSINFO.sema++;			/* bump semaphore operation count */

	if (uap->nsops > semopm)
		return(E2BIG);

	if ((error = vsem_lookup_id(uap->semid, &vsem)) != 0)
		return(error);

	if (uap->nsops > SEMOPS_BUFSZ)
		semops = kmem_alloc(sizeof(*semops) * uap->nsops, KM_SLEEP);
	else
		semops = semops_buf;

	if (copyin((caddr_t)uap->sops, (caddr_t)semops,
			uap->nsops * sizeof(*semops))) {
		error = EFAULT;
		goto op_out;
	}

	/* Verify that sem #s are in range */
	for (i = 0, op = semops; i++ < uap->nsops; op++) {
		if (op->sem_num >= vsem->vsm_nsems) {
			error = EFBIG;
			goto op_out;
		}

		if (op->sem_flg & SEM_UNDO)
			register_undo++;
	}

	error = VSEM_OP(vsem, get_current_cred(), semops, uap->nsops);

	if (!error && register_undo)
		sem_register_undo(uap->semid);

op_out:
	vsem_rele(vsem);
	if (uap->nsops > SEMOPS_BUFSZ)
		kmem_free(semops, sizeof(*semops) * uap->nsops);
	return(error);
}

struct semstatusa {
	struct semstat	*ustat;
};

int
semstatus(
	struct semstatusa *uap)
{
	struct semstat	*ustat;
	struct semstat	sstat;
	int		error;

	ustat = uap->ustat;

	if (copyin(&ustat->sm_id, &sstat.sm_id, sizeof(int)))
		return(EFAULT);
	if (copyin(&ustat->sm_location, &sstat.sm_location,
						sizeof(sstat.sm_location)))
		return(EFAULT);

	error = vsem_getstatus(&sstat, get_current_cred());

	if (error != ESRCH) {
		/* on most 'errors' still update location information */
		if (copyout(&sstat.sm_id, &ustat->sm_id, sizeof(int)))
			return(EFAULT);
		if (copyout(&sstat.sm_location, &ustat->sm_location,
						sizeof(sstat.sm_location)))
			return(EFAULT);
		if (copyout(&sstat.sm_cell, &ustat->sm_cell,
						sizeof(sstat.sm_cell)))
			return(EFAULT);
	}
	if (!error) {
		if (XLATE_COPYOUT(&sstat.sm_semds, &ustat->sm_semds,
				  sizeof(struct semid_ds),
				  semid_to_irix5, get_current_abi(), 1))
			return(EFAULT);
	}

	return(error);
}

/*
 * semsys - System entry point for semctl, semget, and semop system calls.
 */
struct semsysa {
	sysarg_t	opcode;
	void		*data;
};

#define SEMCTL	0
#define SEMGET	1
#define SEMOP	2
#define SEMSTAT	3

int
semsys(
	struct semsysa	*uap,
	rval_t		*rvp)
{
        int		error;

	_SAT_SET_SUBSYSNUM(uap->opcode);

	switch (uap->opcode) {
	case SEMCTL:
		error = semctl((struct semctla *)&uap->data, rvp);
		break;
	case SEMGET:
		error = semget((struct semgeta *)&uap->data, rvp);
		break;
	case SEMOP:
		error = semop((struct semopa *)&uap->data, rvp);
		break;

	case SEMSTAT:
		error = semstatus((struct semstatusa *)&uap->data);
		break;

	default:
		error = EINVAL;
		break;
	}

	return(error);
}

