/* Copyright 1986-1996, Silicon Graphics Inc., Mountain View, CA. */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.52 $"

/*
 * File that implements the top level shm system calls
 */

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/capability.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/kabi.h>
#include <sys/kipc.h>
#include <sys/lock.h>
#include <sys/prctl.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/shm.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/xlate.h>
#include <ksys/vshm.h>
#include <ksys/vm_pool.h>
#include <ksys/vpag.h>

#include "shm/vshm_private.h"

extern time_t	time;		/* system idea of date */

#if _MIPS_SIM == _ABI64
static int shmid_to_irix5(void *, int, xlate_info_t *);
static int irix5_to_shmid(enum xlate_mode, void *, int, xlate_info_t *);
#endif


/*
 * Argument vectors for the various flavors of shmsys().
 */

#define	SHMAT	0
#define	SHMCTL	1
#define	SHMDT	2
#define	SHMGET	3
#define SHMSTAT	4
#define	SHMGETID 5

/*
 * Shmat (attach shared segment) system call.
 */
struct shmata {
	sysarg_t shmid;
	caddr_t	addr;
	sysarg_t flag;
};

int
shmat(struct shmata *uap, rval_t *rvp)
{
	vshm_t			*vshm;
	int			error;
	caddr_t			vaddr;

	if ((error = vshm_lookup_id(uap->shmid, &vshm)) != 0)
		return(error);

	VSHM_ATTACH(vshm, uap->flag, curuthread->ut_asid, current_pid(),
				get_current_cred(), uap->addr, &vaddr, error);
	vshm_rele(vshm);

	rvp->r_val1 = (__psint_t) vaddr;
	return error;
}

/*
 * Shmctl system call.
 */
struct shmctla {
	sysarg_t	shmid;
	sysarg_t	cmd;
	struct shmid_ds	*arg;
};

int
shmctl(struct shmctla *uap)
{
	vshm_t		*vshm;
	int		error;
	cred_t		*cred = get_current_cred();
	vasid_t		vasid;
	as_setrangeattr_t attr;
	as_mohandle_t mo;
#if _MIPS_SIM == _ABI64
	int		abi = get_current_abi();
#endif

	if ((error = vshm_lookup_id(uap->shmid, &vshm)) != 0)
		return(error);

	VSHM_MAC_ACCESS(vshm, cred, error);
	if (error) {
		vshm_rele(vshm);
		return(EACCES);
	}

	switch(uap->cmd) {

	/*
	 * Remove shared memory identifier
	 */
	case	IPC_RMID:
		VSHM_RMID(vshm, cred, 0, error);
		/* returns with one last reference */
		break;

	/*
	 * Set ownership and permissions
	 */
	case	IPC_SET:
	    {
		struct shmid_ds	ds;

		if (COPYIN_XLATE(uap->arg, &ds, sizeof(ds),
				irix5_to_shmid, abi, 1)) {
			error = EFAULT;
			break;
		}

		VSHM_IPCSET(vshm, &ds.shm_perm, cred, error);
		break;
	    }

	/*
	 * Get shared memory data structure
	 */
	case	IPC_STAT:
	    {
		struct shmid_ds	sp;
		cell_t cell;

		VSHM_GETSTAT(vshm, cred, &sp, &cell, error);

		if (error == 0 && XLATE_COPYOUT(&sp, uap->arg, sizeof(sp),
				  shmid_to_irix5, abi, 1))
			error = EFAULT;
		break;
	    }

	/*
	 * Lock segment in memory
	 */
	case	SHM_LOCK:
		if (!_CAP_ABLE(CAP_MEMORY_MGT)) {
			error = EPERM;
			break;
		}
		VSHM_GETMO(vshm, &mo);

		as_lookup_current(&vasid);

		/*
		 * Batch processes cannot pin memory. So suspend them until they
		 * go critical. If we let them lock pages and they later on wait
		 * for ever to go critical, it is possible that we locked a 
		 * piece memory for a very long time.
		 */

		while (kt_basepri(curthreadp) == PBATCH) {
			as_getasattr_t as_vpag_attr;
			VAS_GETASATTR(vasid, AS_VPAGG, &as_vpag_attr);
			if (as_vpag_attr.as_vpagg)
				VPAG_SUSPEND(as_vpag_attr.as_vpagg);
		}

		attr.as_op = AS_LOCK_BY_ATTR;
		attr.as_lock_attr = AS_SHMLOCK;
		attr.as_lock_handle = mo;
		error = VAS_SETRANGEATTR(vasid, 0, 0, &attr, NULL);
		if (error == EAGAIN)
			nomemmsg("shmctl while locking memory segment");

		break;

	/*
	 * Unlock segment
	 */
	case	SHM_UNLOCK:
		if (!_CAP_ABLE(CAP_MEMORY_MGT)) {
			error = EPERM;
			break;
		}

		VSHM_GETMO(vshm, &mo);

		as_lookup_current(&vasid);
		attr.as_op = AS_UNLOCK_BY_ATTR;
		attr.as_lock_attr = AS_SHMLOCK;
		attr.as_lock_handle = mo;
		error = VAS_SETRANGEATTR(vasid, 0, 0, &attr, NULL);
		break;

	default:
		error = EINVAL;
		break;
	}

	vshm_rele(vshm);
	return(error);
}

/*
 * Detach shared memory segment
 */
struct shmdta {
	caddr_t	addr;
};

static int
shmdt(struct shmdta *uap)
{
	vshm_t *vshm;
	vasid_t vasid;
	as_deletespace_t as;
	as_deletespaceres_t asres;
	int error;

	as_lookup_current(&vasid);
	as.as_op = AS_DEL_SHM;
	as.as_shm_start = uap->addr;

	if (error = VAS_DELETESPACE(vasid, &as, &asres))
		return error;
	/*
	 * Find vshm, update detach time
	 * and pid, and free if appropriate
	 */
	if (vshm_lookup_id(asres.as_shmid, &vshm) == 0) {
		VSHM_SETDTIME(vshm, time, current_pid());
		vshm_rele(vshm);
	}

	return(0);
}

/*
 * Shmget (create new shmem) system call.
 */
struct shmgeta {
	sysarg_t	key;
	usysarg_t	size;
	usysarg_t	shmflg;
};

static int
shmget(struct shmgeta *uap, rval_t *rvp)
{
	int		error;
	vshm_t		*vshm;

	if (uap->key == IPC_PRIVATE) {
		error = vshm_create(uap->key, uap->shmflg|IPC_CREAT, uap->size,
				current_pid(), get_current_cred(), &vshm, -1);
	} else {
		error = vshm_lookup_key(uap->key, uap->shmflg, uap->size,
				current_pid(), get_current_cred(), &vshm, -1);
	}
	if (error) {
		return(error);
	}
#ifdef CKPT
	CURVPROC_CKPT_SHMGET(vshm->vsm_id);
#endif
	rvp->r_val1 = vshm->vsm_id;

	vshm_rele(vshm);

	return(0);
}

/*
 * Shmget_id (create new shmem with requested id) system call.
 */
struct shmgetida {
	sysarg_t	key;
	usysarg_t	size;
	usysarg_t	shmflg;
	sysarg_t	id;
};

static int
shmget_id(struct shmgetida *uap, rval_t *rvp)
{
	int		error;
	vshm_t		*vshm;

	if (uap->key == IPC_PRIVATE) {
		error = vshm_create(uap->key, uap->shmflg|IPC_CREAT, uap->size,
				current_pid(), get_current_cred(), &vshm,
				uap->id);
	} else {
		error = vshm_lookup_key(uap->key, uap->shmflg, uap->size,
				current_pid(), get_current_cred(), &vshm, 
				uap->id);
	}
	if (error) {
		return(error);
	}
#ifdef CKPT
	CURVPROC_CKPT_SHMGET(vshm->vsm_id);
#endif
	rvp->r_val1 = vshm->vsm_id;

	vshm_rele(vshm);

	return(0);
}

struct shmstatusa {
	struct shmstat	*ustat;
};

int
shmstatus(
	struct shmstatusa *uap)
{
	struct shmstat	*ustat;
	struct shmstat	sstat;
	int		error;

	ustat = uap->ustat;

	if (copyin(&ustat->sh_id, &sstat.sh_id, sizeof(int)))
		return(EFAULT);
	if (copyin(&ustat->sh_location, &sstat.sh_location,
						sizeof(sstat.sh_location)))
		return(EFAULT);

	error = vshm_getstatus(&sstat, get_current_cred());

	if (error != ESRCH) {
		/* on most 'errors' still update location information */
		if (copyout(&sstat.sh_id, &ustat->sh_id, sizeof(int)))
			return(EFAULT);
		if (copyout(&sstat.sh_location, &ustat->sh_location,
						sizeof(sstat.sh_location)))
			return(EFAULT);
		if (copyout(&sstat.sh_cell, &ustat->sh_cell,
						sizeof(sstat.sh_cell)))
			return(EFAULT);
	}
	if (!error ||
	   ((error == EACCES) && (sstat.sh_shmds.shm_cpid != NOPID))) {
		if (XLATE_COPYOUT(&sstat.sh_shmds, &ustat->sh_shmds,
				  sizeof(struct shmid_ds),
				  shmid_to_irix5, get_current_abi(), 1))
			return(EFAULT);
	}

	return(error);
}

/*
 * System entry point for shmat, shmctl, shmdt, shmget, and shmstatus
 * system calls.
 */

struct shmsysa {
	sysarg_t	opcode;
	void		*data;
};

int
shmsys(register struct shmsysa *uap, rval_t *rvp)
{
	register int error;

	switch (uap->opcode) {
	case SHMAT:
		error = shmat((struct shmata *)&uap->data, rvp);
		break;
	case SHMCTL:
		error = shmctl((struct shmctla *)&uap->data);
		break;
	case SHMDT:
		error = shmdt((struct shmdta *)&uap->data);
		break;
	case SHMGET:
		error = shmget((struct shmgeta *)&uap->data, rvp);
		break;
	case SHMGETID:
		error = shmget_id((struct shmgetida *)&uap->data, rvp);
		break;
	case SHMSTAT:
		error = shmstatus((struct shmstatusa *)&uap->data);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int
shmid_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register struct irix5_shmid_ds *i5_ds;
	register struct shmid_ds *ds = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_shmid_ds) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_shmid_ds));
	info->copysize = sizeof(struct irix5_shmid_ds);

	i5_ds = info->copybuf;

	ipc_perm_to_irix5(&ds->shm_perm, &i5_ds->shm_perm);
	i5_ds->shm_segsz = (int)ds->shm_segsz;	/* XXX potentially trucates */
	i5_ds->shm_amp = NULL;
	i5_ds->shm_lpid = ds->shm_lpid;
	i5_ds->shm_cpid = ds->shm_cpid;
	i5_ds->shm_nattch = ds->shm_nattch;
	i5_ds->shm_cnattch = ds->shm_cnattch;
	i5_ds->shm_atime = ds->shm_atime;
	i5_ds->shm_dtime = ds->shm_dtime;
	i5_ds->shm_ctime = ds->shm_ctime;

	return 0;
}

/*ARGSUSED*/
static int
irix5_to_shmid(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	register struct shmid_ds *ds;
	register struct irix5_shmid_ds *i5_ds;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_shmid_ds) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
					    sizeof(struct irix5_shmid_ds));
		info->copysize = sizeof(struct irix5_shmid_ds);
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_shmid_ds));
	ASSERT(info->copybuf != NULL);

	ds = to;
	i5_ds = info->copybuf;

	irix5_to_ipc_perm(&i5_ds->shm_perm, &ds->shm_perm);
	/* user can't set anything else */

	return 0;
}
#endif	/* _ABI64 */


/*
 * shm_rmid_callback isn't a syscall, but is very similar to
 * shmctl(shmid, IPC_RMID).
 */
void
shm_rmid_callback(void *arg)
{
	vshm_t		*vshm;
	int		error;
	cred_t		*cred = get_current_cred();

	error = vshm_lookup_id((__psint_t)arg, &vshm);
	if (error) {
		/*
		 * Another process could have done a rmid while
		 * we were doing the callback.
		 */
#ifdef DEBUG
		cmn_err(CE_NOTE, "shm_rmid_callback: vshm_lookup_id(%d,...) returned %d, probable legit race condition\n", arg, error);
#endif
		return;
	}

	/*
	 * Remove shared memory identifier
	 */
	VSHM_RMID(vshm, cred, IPC_AUTORMID, error);
#ifdef DEBUG
	if (error) {
		cmn_err(CE_NOTE, "shm_rmid_callback: VSHM_RMID returned %d, probable legit race condition\n", error);
	}
#endif

	vshm_rele(vshm);
	return;
}
