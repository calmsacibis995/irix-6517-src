/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.19 $"
/*
 *	Posix.1b-1993 Named Semaphore Facility
 */
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/map.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysinfo.h>
#include <sys/sysmp.h>
#include <sys/systm.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/vnode.h>
#include <sys/idbgentry.h>
#include <sys/atomic_ops.h>
#include <sys/idbg.h>
#include <sys/psema_cntl.h>
#include <sys/cred.h>

typedef struct psema {
	int		ps_value;
	int		ps_open_count;
	vnode_t		*ps_vp;
	sv_t		ps_wait;
	mutex_t 	ps_lock;
	struct psema    *ps_next;
} psema_t;

static struct ps_hash {
        psema_t *psh_next;
} *ps_hash;

#define PS_HASH_SHIFT   7
#define PS_HASH(kaddr) \
        ((((__psint_t)kaddr) >> PS_HASH_SHIFT) & ps_hash_mask)

static struct zone *psema_zone;	/* zone for semaphore structs */
static mrlock_t  psema_lock;         /* global lock */
static u_int ps_hash_mask;
extern int psema_max_cnt;
static int max_psemas;
static int psema_cnt;

#define PSEMA_READ_LOCK()   (void)mrlock(&psema_lock, MR_ACCESS, PZERO)
#define PSEMA_WRITE_LOCK()  (void)mrlock(&psema_lock, MR_UPDATE, PZERO)
#define PSEMA_UNLOCK()      mrunlock(&psema_lock)

#define PSEMA_READ_LOCKED()  (ismrlocked(&psema_lock, MR_ACCESS) != 0)
#define PSEMA_WRITE_LOCKED() (ismrlocked(&psema_lock, MR_UPDATE) != 0)
#define PSEMA_READ_WRITE_LOCKED()				\
			(ismrlocked(&psema_lock, MR_ACCESS|MR_UPDATE) != 0)

extern int close(void *, rval_t *);

static void psema_dump(void);
static void insert_ps(psema_t *);
static void remove_ps(psema_t *);
static void init_ps(psema_t *, vnode_t *, unsigned int);
static psema_t * get_ps(vnode_t *);

struct closea {
        sysarg_t fdes;
};

/*	psemainit - Called by main()
 *
 *	initialize the semaphore map.
 */
void
psemainit(void)
{
	int i;

	max_psemas = psema_max_cnt;
	psema_zone = kmem_zone_init(sizeof(psema_t),"psema");
	mrinit(&psema_lock, "p_sem_lock");
        for (i = 1; i < max_psemas; i <<= 1)
                ;
        if (i >= 10)
                i /= 10;
        ps_hash_mask = i - 1;
        ps_hash = kmem_zalloc(i * sizeof (*ps_hash), KM_SLEEP);

	idbg_addfunc("psema", psema_dump);
}

/*
 *	psema_open - open/create a semaphore
 *
 *	used for opening and creating a semaphore
 *	the filename passed in as the argument is used as the
 *	anchor to the semaphore.
 *	When a semaphore is created, a kernel semaphore is
 *	allocated. 
 *
 *	Returns fd on success
 */

struct psema_opena {
	sysarg_t	opcode;
  	char		*fname;
	sysarg_t	fmode;
	usysarg_t	cmode;
	sysarg_t	ps_init_value;
};

int
psema_open(struct psema_opena *uap, rval_t *rvp)
{
	int error;
	int fmode;
	vfile_t *fp;
	psema_t *ps;
	struct closea closea;
	struct vnode *vp;

	extern int copen(char *, int, mode_t, rval_t *);

	fmode = uap->fmode - FOPEN;
	/*
	 * clear all flags other than FREAD/FWRITE/O_EXCL/O_CREATE;
	 * done at user level
	 * uap->ps_opena.fmode &=  (O_RDONLY | O_WRONLY | O_RDWR |
	 *						O_CREAT | O_EXCL);
	 */
	error = copen(uap->fname, uap->fmode-FOPEN, (mode_t) uap->cmode, rvp);
	if (error) {
		/*
		 * POSIX compliance error re-mappings
		 */
		if ((error == ENOENT) && (uap->fmode & O_CREAT))
			return EINVAL;

		if ((error != EACCES) && (error != EEXIST) &&
			(error != ENAMETOOLONG) && (error != EINTR) &&
			(error != EINVAL) && (error != EMFILE) &&
			(error != ENFILE) && (error != ENOENT) &&
			(error != ENOSPC)) {
			error = EINVAL;
		}

		return (error);
	}

	if (getf(rvp->r_val1, &fp)) {
		/*
		 * This really should not happen, because the file was
		 * just opened
		 */
		return EINVAL;
	}

	if (!VF_IS_VNODE(fp)) {
                fdt_release();			/* XXX why? */
		closea.fdes = rvp->r_val1;
		close((void *) &closea, rvp);
		return EINVAL;
	}

	vp = VF_TO_VNODE(fp);

	/*
	 * XXX
	 *	Make sure that a regular file is opened
	 *	Is this check necessary???
	 */
	if (vp->v_type != VREG) {
                fdt_release();			/* XXX why? */
		closea.fdes = rvp->r_val1;
		close((void *) &closea, rvp);
		return ENOENT;
	}

	/*
	 * When (O_EXCL & O_CREAT) are specified both the file and the
	 * semaphore should be created atomically, or if the semaphore
	 * exists, EEXIST should be returned as the error value.
	 *
	 * The psema_lock is held across the search for the semaphore
	 * and creation of a new one, if it doesn't exist. This takes care
	 * of the race condition that exists when two or more procs are
	 * trying to create the semaphore simultaneously.
	 */
	PSEMA_WRITE_LOCK();
	ps = get_ps(vp);

	if (ps == NULL) {
		if ((fmode & FCREAT) == 0) {
			/*
			 * semaphore doesn't exist and is not being created
			 */
			PSEMA_UNLOCK();
			fdt_release();
			closea.fdes = rvp->r_val1;
			close((void *) &closea, rvp);
			return ENOENT;
		}

		/*
		 * create and initialize the new semaphore
		 */
		if (psema_cnt >= max_psemas) {
			PSEMA_UNLOCK();
			fdt_release();
			closea.fdes = rvp->r_val1;
			(void) close((void *) &closea, rvp);
			return ENOSPC;
		}

		ps = kmem_zone_zalloc(psema_zone, KM_SLEEP);
		init_ps(ps, vp, uap->ps_init_value);
		atomicAddInt(&psema_cnt, 1);
		insert_ps(ps);

		/*
		 * Set the vnode semaphore flag and grab a reference
		 * (dropped during unlink).
		 */
		VN_FLAGSET(vp, VSEMAPHORE);
		VN_HOLD(vp);
	} else {
		/*
		 * Because the psema_lock is not held across the call to
		 * open(), someone else might have created the semaphore;
		 * return EEXIST in this case
		 */
		if ((fmode & FCREAT) && (fmode & FEXCL)) {
                	fdt_release();
			closea.fdes = rvp->r_val1;
			close((void *) &closea, rvp);
			mutex_unlock(&ps->ps_lock);
			PSEMA_UNLOCK();
			return EEXIST;
		}

		ps->ps_open_count++;
		mutex_unlock(&ps->ps_lock);
	}

	PSEMA_UNLOCK();
	return 0;
}

/*
 *	psema_close - close semaphore
 *
 *	Returns 0 on success
 */

struct psema_opa {
	sysarg_t	opcode;
	sysarg_t	fdes;
	sysarg_t	userpri;
};

int
psema_close(struct psema_opa *uap, rval_t *rvp)
{
	vfile_t *fp;
	struct closea closea;

	if (getf(uap->fdes, &fp))
		return EINVAL;

	/*
	 * Verify whether the file descriptor names a Posix semaphore
	 */
	if (!VF_IS_VNODE(fp) || !(VF_TO_VNODE(fp)->v_flag & VSEMAPHORE))
		return EINVAL;

	fdt_release();		      /* XXX why not wait for syscall exit? */

	closea.fdes = uap->fdes;
	if (close((void *) &closea, rvp))
		return EINVAL;

	return 0;
}

/*
 *	psema_indirect_close
 *
 *	Close the semaphore, called indirectly from close()
 *
 *	Returns 0 on success
 */
int
psema_indirect_close(vnode_t *vp)
{
	psema_t *ps;
	struct vattr va;
	int error;

	PSEMA_WRITE_LOCK();
	ps = get_ps(vp);
	if (ps == NULL) {
		PSEMA_UNLOCK();
		return EINVAL;
	}

	ps->ps_open_count--;

	va.va_mask = AT_NLINK;
	VOP_GETATTR(vp, &va, 0, get_current_cred(), error);
	if (error)
		return EINVAL;

	if ((ps->ps_open_count == 0) &&	(va.va_nlink == 0)) {
		remove_ps(ps);
		VN_FLAGCLR(vp, VSEMAPHORE);
		VN_RELE(vp);
	} else
		mutex_unlock(&ps->ps_lock);

	PSEMA_UNLOCK();

	return 0;
}

/*
 *	psema_unlink - unlink semaphore
 *
 *	Returns 0 on success
 */

struct psema_unlinka {
	sysarg_t	opcode;
	char *		name;
};

int
psema_unlink(struct psema_unlinka *uap, rval_t *rvp)
{
	int rval;
	extern int unlink(void *, rval_t *);

	rval = unlink((void *) &uap->name, rvp);

	switch (rval) {
	case 0:
		return 0;
	case EACCES:
	case ENAMETOOLONG:
	case ENOENT:
		return rval;
	}	  

	return EINVAL;
}

/*
 *	psema_indirect_unlink
 *
 *	Unlink the semaphore, called indirectly from unlink.
 *
 *	Returns 0 on success
 */
int
psema_indirect_unlink(vnode_t *vp)
{
	psema_t *ps;

	PSEMA_WRITE_LOCK();
	ps = get_ps(vp);
	if (ps == NULL) {
		PSEMA_UNLOCK();
		return ENOENT;
	}

	if (ps->ps_open_count == 0) {
		remove_ps(ps);
		VN_FLAGCLR(vp, VSEMAPHORE);
		VN_RELE(vp);
	} else
		mutex_unlock(&ps->ps_lock);

	PSEMA_UNLOCK();

	return 0;
}

/*
 *	psema_post - unlock a semaphore: wake up waiter, if any
 *
 *	Returns 0 on success
 */
/* ARGSUSED */
int
psema_post(struct psema_opa *uap, rval_t *rvp)
{
	vfile_t *fp;
	psema_t *ps;

	if (getf(uap->fdes, &fp))
		return EINVAL;

	if (!VF_IS_VNODE(fp))
		return EINVAL;

	PSEMA_READ_LOCK();
	ps = get_ps(VF_TO_VNODE(fp));
	if (ps == NULL) {
		PSEMA_UNLOCK();
		return EINVAL;
	}
	PSEMA_UNLOCK();

	ps->ps_value++;
	if (ps->ps_value <= 0)
		sv_signal(&ps->ps_wait);

	mutex_unlock(&ps->ps_lock);

	return 0;
}

/*
 *	psema_broadcast - wake up all waiters
 *
 *	Return the number of processes woken-up
 */
int
psema_broadcast(struct psema_opa *uap, rval_t *rvp)
{
	vfile_t *fp;
	psema_t *ps;

	if (getf(uap->fdes, &fp))
		return EINVAL;

	if (!VF_IS_VNODE(fp))
		return EINVAL;

	PSEMA_READ_LOCK();
	ps = get_ps(VF_TO_VNODE(fp));
	if (ps == NULL) {
		PSEMA_UNLOCK();
		return EINVAL;
	}
	PSEMA_UNLOCK();

	if (ps->ps_value < 0) {
		rvp->r_val1 = sv_broadcast(&ps->ps_wait);
		ps->ps_value = 0;
	}

	mutex_unlock(&ps->ps_lock);

	return 0;
}

/*
 *	psema_wait - lock a semaphore, block if already locked
 *
 *	returns 0 on success
 */
/* ARGSUSED */
int
psema_wait(struct psema_opa *uap, int upri, rval_t *rvp)
{
	vfile_t *fp;
	psema_t *ps;
	int ret;

	if (getf(uap->fdes, &fp))
		return EINVAL;

	if (!VF_IS_VNODE(fp))
		return EINVAL;

	PSEMA_READ_LOCK();
	ps = get_ps(VF_TO_VNODE(fp));
	if (ps == NULL) {
		PSEMA_UNLOCK();
		return EINVAL;
	}
	PSEMA_UNLOCK();

	ps->ps_value--;

	if (ps->ps_value < 0) {
		kthread_t *kt = private.p_curkthread;

		if (upri)
			kt->k_qkey = uap->userpri;
		else
			kt->k_qkey = kt->k_pri;
		
		ret = sv_wait_sig(&ps->ps_wait, PZERO + 1, &ps->ps_lock, 0);
		if (ret == -1) {
			/*
			 * interrupted by signal
			 */ 
			mutex_lock(&ps->ps_lock, PZERO);
			ps->ps_value++;
			mutex_unlock(&ps->ps_lock);
			return EINTR;
		}
	} else
		mutex_unlock(&ps->ps_lock);

	return 0;
}

/*
 *	psema_trywait - try to lock a semaphore
 *			do not block if already locked
 *
 *	returns 0 on success
 */
/* ARGSUSED */
int
psema_trywait(struct psema_opa *uap, rval_t *rvp)
{
	int rval = 0;
	vfile_t *fp;
	psema_t *ps;

	if (getf(uap->fdes, &fp))
		return EINVAL;

	if (!VF_IS_VNODE(fp))
		return EINVAL;

	PSEMA_READ_LOCK();
	ps = get_ps(VF_TO_VNODE(fp));
	if (ps == NULL) {
		PSEMA_UNLOCK();
		return EINVAL;
	}
	PSEMA_UNLOCK();

	if (ps->ps_value > 0) {
		ps->ps_value--;
		rval = 0;
	} else
		rval = EAGAIN;

	mutex_unlock(&ps->ps_lock);

	return (rval);
}

/*
 *	psema_getvalue - return the value of the semaphore
 *
 *	returns 0 on success
 */

struct psema_geta {
	sysarg_t	opcode;
	sysarg_t	fdes;
	sysarg_t	intp;
};

/* ARGSUSED */
int
psema_getvalue(struct psema_geta *uap, rval_t *rvp)
{
	int rval = 0;
	vfile_t *fp;
	psema_t *ps;

	if (getf(uap->fdes, &fp))
		return EINVAL;

	if (!VF_IS_VNODE(fp))
		return EINVAL;

	PSEMA_READ_LOCK();
	ps = get_ps(VF_TO_VNODE(fp));
	if (ps == NULL) {
		PSEMA_UNLOCK();
		return EINVAL;
	}
	PSEMA_UNLOCK();

	if (suword((caddr_t)uap->intp, ps->ps_value))
		rval = EINVAL;

	mutex_unlock(&ps->ps_lock);

	return (rval);
}

/*
 *	psema_cntl
 *
 *	System entry point for Posix.1b named semaphore calls
 */

struct psema_sysa {
        sysarg_t        opcode;
};

int
psema_cntl(struct psema_sysa *uap, rval_t *rvp)
{
	int error;

	switch (uap->opcode) {
	case PSEMA_OPEN:
		error = psema_open((struct psema_opena *) uap, rvp);
		break;
	case PSEMA_CLOSE:
		error = psema_close((struct psema_opa *) uap, rvp);
		break;
	case PSEMA_UNLINK:
		error = psema_unlink((struct psema_unlinka *) uap, rvp);
		break;
	case PSEMA_POST:
		error = psema_post((struct psema_opa *) uap, rvp);
		break;
	case PSEMA_BROADCAST:
		error = psema_broadcast((struct psema_opa *) uap, rvp);
		break;
	case PSEMA_WAIT:
		error = psema_wait((struct psema_opa *) uap, 0, rvp);
		break;
	case PSEMA_WAIT_USERPRI:
		error = psema_wait((struct psema_opa *) uap, 1, rvp);
		break;
	case PSEMA_TRYWAIT:
		error = psema_trywait((struct psema_opa *) uap, rvp);
		break;
	case PSEMA_GETVALUE:
		error = psema_getvalue((struct psema_geta *) uap, rvp);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

/*
 *	init_ps
 *
 *	initialize the semaphore
 */
static void
init_ps(psema_t *ps, vnode_t *vp, unsigned int init_value)
{
	ps->ps_vp = vp;
	ps->ps_value = init_value;
	ps->ps_open_count = 1;
	mutex_init(&ps->ps_lock, MUTEX_DEFAULT, "psema_lock");
	sv_init(&ps->ps_wait, SV_KEYED, "psema_wait");
	return;
}

/*
 *	get_ps
 *
 *	return the locked psema struct correspnding to the vnode
 *	assumes global psema_lock is held
 */
static psema_t *
get_ps(vnode_t *vp)
{
	psema_t *psp;
	struct  ps_hash *psh;

	ASSERT(PSEMA_READ_WRITE_LOCKED());
        /*
         * lookup the hash table
         */
        psh = &ps_hash[PS_HASH(vp)];
        for (psp = psh->psh_next; psp; psp = psp->ps_next)
                if (psp->ps_vp == vp)
                        break;
	if (psp)
		mutex_lock(&psp->ps_lock, PZERO);
	return (psp);
}

static void
insert_ps(psema_t *ps)
{
	struct  ps_hash *psh;

	ASSERT(PSEMA_WRITE_LOCKED());

        psh = &ps_hash[PS_HASH(ps->ps_vp)];
	ps->ps_next = psh->psh_next;
	psh->psh_next = ps;
	return;
}

/*
 *	remove_ps
 *
 *	remove the Posix semaphore from the hash list
 *	assumes psema_lock is already held
 */
static void
remove_ps(psema_t *ps)
{
	psema_t *psp;
	psema_t *psp_prev = NULL;
	struct  ps_hash *psh;

	ASSERT(PSEMA_WRITE_LOCKED());
	ASSERT(mutex_mine(&ps->ps_lock));

        psh = &ps_hash[PS_HASH(ps->ps_vp)];
	psp = psh->psh_next;
	while (psp != ps) {
		psp_prev = psp;
		psp = psp->ps_next;
		ASSERT(psp);
	}

	if (psp_prev)
		psp_prev->ps_next = ps->ps_next;
	else
		psh->psh_next = ps->ps_next;

	mutex_destroy(&ps->ps_lock);
	sv_destroy(&ps->ps_wait);

	kmem_zone_free(psema_zone, (void *) ps);
	atomicAddInt(&psema_cnt, -1);
	return;
}

/*
 *	print psema
 */
static void
psema_print(psema_t *psp)
{
	qprintf("open_cnt = %d value = %d\n",
		psp->ps_open_count, psp->ps_value);
#if _K64U64
        qprintf("vnode = 0x%llx wait = 0x%llx lock = 0x%llx\n",
                psp->ps_vp, &psp->ps_wait, &psp->ps_lock);
#else
        qprintf("vnode = 0x%x wait = 0x%x lock = 0x%x\n",
                psp->ps_vp, &psp->ps_wait, &psp->ps_lock);
#endif
}

/*
 *	psema_dump
 *
 *	Dump list of active semaphores in psema hash table
 */
static void
psema_dump(void)
{
	psema_t *psp;
	int i, x = 0;

        qprintf("psema_cnt = %d\n", psema_cnt);
	qprintf("hash_table_size = %d\n", ps_hash_mask + 1);
        for (i=0; i <= ps_hash_mask; i++) {
                psp = ps_hash[i].psh_next;
		if (psp) {
			qprintf("HASH INDEX %d\n", i);
			while (psp) {
				qprintf("[%d] ", x++);
				psema_print(psp);
				psp = psp->ps_next;
			}
		}
        }
}
