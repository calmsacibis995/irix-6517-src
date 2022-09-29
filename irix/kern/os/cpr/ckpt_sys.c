/*
 * ckpt_sys.c
 *
 * 	Implements s checkpoint syssgi system calls.
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.88 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <ksys/as.h>
#include <sys/debug.h>
#include <sys/sema.h>
#include <ksys/vfile.h>
#include <sys/mode.h>
#include <ksys/fdt.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <ksys/vproc.h>
#include <ksys/childlist.h>
#include <sys/proc.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include <sys/exec.h>
#include <sys/kucontext.h>
#include <sys/ckpt.h>
#include <sys/ckpt_sys.h>
#include <sys/prctl.h>
#include <pipefs/pipenode.h>
#include <sys/var.h>
#include <sys/kmem.h>
#include <sys/sysmacros.h>
#include "ckpt_procfs.h"
#ifdef DEBUG
#include <sys/sema_private.h>
#endif
#include <sys/wait.h>
#include <fs/procfs/prdata.h>  /* prstop() */
#include <sys/cred.h>
#include <os/numa/pm.h>
#include <sys/hwgraph.h>
#include <sys/mmci.h>

extern int fdaccess(int, int);
static int ckpt_diropen(int, int, int *);

static int
ckpt_diropen(int fd, int filemode, int *rvp)
{
	struct vfile *fp;
	vnode_t *vp;
	vnode_t *dvp;
	int error;
	fid_t *fidp;

	filemode -= FOPEN;

	if (error = getf(fd, &fp))
		return error;

	if (!VF_IS_VNODE(fp) || fp->vf_ckpt == -1)
		return EINVAL;

	vp = VF_TO_VNODE(fp);

	if ((fidp = ckpt_lookup_get(vp, fp->vf_ckpt)) == NULL)
		return EINVAL;

	VFS_VGET(vp->v_vfsp, &dvp, fidp, error);
	if (error)
		return error;

	if (dvp == NULL)
		return ENOENT;

	if (error = ckpt_vp_open(dvp, filemode, -1, rvp)) {
		VN_RELE(dvp);
		return error;
	}
	return 0;
}

static int
ckpt_execmap(caddr_t uap)
{
	ckpt_execargs_t args;
	struct vfile *fp;
	int error;
	vasid_t vasid;

	if (copyin(uap, (caddr_t)&args, sizeof(args)))
		return EFAULT;

	if (error = getf(args.fd, &fp))
		return error;
	if (!VF_IS_VNODE(fp))
		return EINVAL;

	as_lookup_current(&vasid);
	VAS_LOCK(vasid, AS_EXCL);
	error = execmap(VF_TO_VNODE(fp),
			args.vaddr,
			args.len,
			args.zfodlen,
			args.off,
			args.prot,
			args.flags,
			vasid,
			fp->vf_ckpt);
	VAS_UNLOCK(vasid);
	return error;
}
/*
 * Set the exec vnode for this process.  Targets destined to become Zombies
 * pass in an fd of -1. For them, release any urrent vnode and set exec vnode
 * to null.
 */
static int
ckpt_setexec(int execfd)
{
	struct vfile *execfp;
	vnode_t *vp;
	struct vattr vattr;
	int error;

	if (execfd >= 0) {
		if ((execfp = fdt_getref(curprocp, execfd)) == NULL)
			return EBADF;
	
		if (!VF_IS_VNODE(execfp)) {
			VFILE_REF_RELEASE(execfp);
			return EINVAL;
		}
		vp = VF_TO_VNODE(execfp);
	
		vattr.va_mask = AT_TYPE;
		VOP_GETATTR(vp, &vattr, 0, get_current_cred(), error);
		if (error) {
			VFILE_REF_RELEASE(execfp);
			return error;
		}
		if (VTTOIF(vattr.va_type) != S_IFREG) {
			VFILE_REF_RELEASE(execfp);
			return EINVAL;
		}
	} else
		vp = NULL;

	mrlock(&curprocp->p_who, MR_UPDATE, PZERO);
	if (curprocp->p_exec) {
		VN_RELE(curprocp->p_exec);
		curprocp->p_exec = NULL;
	}
	if (vp) {
		curprocp->p_exec = vp;
		VN_HOLD(curprocp->p_exec);
	}
	mrunlock(&curprocp->p_who);

	if (vp)
		VFILE_REF_RELEASE(execfp);

	return 0;
}

static int
ckpt_setbrk(uvaddr_t vaddr)
{
	vasid_t vasid;
	as_setattr_t asattr;

	as_lookup_current(&vasid);

	asattr.as_op = AS_SET_CKPTBRK;
	asattr.as_set_brkbase = vaddr;
	asattr.as_set_brksize = 0;

	return VAS_SETATTR(vasid, NULL, &asattr);
}

static int
ckpt_fpaddr(int fd, rval_t *rvp)
{
	int error;
	struct vfile *fp;

	if (error = getf(fd, &fp))
		return error;

	rvp->r_val1 = (__int64_t)(__psint_t)fp;

	return (0);
}

static int
ckpt_sockmate(int fd, rval_t *rvp)
{
	int error;
	struct vfile *fp;

	if (error = getf(fd, &fp))
	      return error;

	rvp->r_val1 = (__int64_t)(__psint_t)fp->vf_cpr.cu_mate;

	return (0);
}

static int
ckpt_svr3pipe(caddr_t cmaddr, rval_t *rvp)
{
	int *pipefd = (int *)cmaddr;
	int error;

	error = oldpipe(NULL, rvp);
	if (!error) {
		if (suword(pipefd++, rvp->r_val1))
			return (EFAULT);
		if (suword(pipefd, rvp->r_val2))
			return (EFAULT);
	}
	return (error);
}

/*
 * Support routines for discovering process hierarchy
 */
typedef struct {
	int	maxpids;
	int	pidcnt;
	pid_t	*pid;
} ckpt_pid_t;

static void
ckpt_childpid(proc_t *p, ckpt_pid_t *pidarg)
{
	child_pidlist_t *cpid;
	int i;

	mutex_lock(&p->p_childlock, PZERO);

	for (cpid = p->p_childpids; cpid != NULL; cpid = cpid->cp_next) {
		i = pidarg->pidcnt++;

		/*
		 * If we're just counting or overflowed, loop now
		 * User can detect overflow by returned pid count > number
		 * passed as argument.
		 */
		if (pidarg->maxpids == 0 || i >= pidarg->maxpids)
			continue;
		pidarg->pid[i] = cpid->cp_pid;
	}

	mutex_unlock(&p->p_childlock);
}


static int
ckpt_getchildpids(pid_t pid, int cnt, caddr_t pidbuf, rval_t *rvp)
{
	vproc_t *vpr;
	proc_t *p;
	ckpt_pid_t pidarg;
	int error = 0;
	/*
	 * check for absurd cnt values
	 */
	if ((cnt < 0)||(cnt >= nproc))
		return EINVAL;

	if ((vpr = VPROC_LOOKUP(pid)) == NULL)
		return ENOENT;
	VPROC_GET_PROC(vpr, &p);

	if (cnt == 0) {
		pidarg.maxpids = 0;
		pidarg.pidcnt = 0;
		pidarg.pid = NULL;
	} else {
		pidarg.maxpids = cnt;
		pidarg.pidcnt = 0;
		pidarg.pid = (pid_t *)kmem_alloc(cnt*sizeof(pid_t), KM_SLEEP);
	}

	ckpt_childpid(p, &pidarg);

	VPROC_RELE(vpr);

	if (cnt) {
		if (copyout(pidarg.pid, pidbuf, cnt * sizeof(pid_t)))
			error = EFAULT;
		kmem_free(pidarg.pid, cnt * sizeof(pid_t));
	}
	rvp->r_val1 = pidarg.pidcnt;

	return (error);
}

static int
ckpt_getshdpids(pid_t pid, caddr_t buf, int bufsize, rval_t *rvp)
{
	vproc_t *vpr;
	proc_t *p;
	int pidcnt;
	int error = 0;
	pid_t *pidbuf;

	if ((vpr = VPROC_LOOKUP(pid)) == NULL)
		return ENOENT;
	VPROC_GET_PROC(vpr, &p);

	if (!IS_SPROC(&p->p_proxy)) {
		rvp->r_val1 = 0;
		VPROC_RELE(vpr);
		return (0);
	}
	if (buf == NULL) {
		/*
		 * just get a count
		 */
		rvp->r_val1 = p->p_shaddr->s_refcnt;
		VPROC_RELE(vpr);
		return (0);
	}
	pidbuf = (pid_t *)kmem_alloc(bufsize, KM_SLEEP);
	/*
	 * Get as many pids as will fit in the buffer, return the count
	 */
	pidcnt = getshdpids(p, pidbuf, bufsize);

	rvp->r_val1 = pidcnt;

	if (copyout(pidbuf, buf, pidcnt * sizeof(pid_t)))
		error = EFAULT;

	VPROC_RELE(vpr);

	kmem_free(pidbuf, bufsize);

	return (error);
}

static int
ckpt_unmapprda(void)
{
	int error;
	vasid_t vasid;
        as_deletespace_t asd;

	/* if the prda is locked - there is no safe way to unlock it
	 * since it means that the kernel has pointers into it
	 */
	if (curuthread->ut_prda)
		return EBUSY;

	as_lookup_current(&vasid);
        asd.as_op = AS_DEL_MUNMAP;
        asd.as_munmap_start = (uvaddr_t)PRDAADDR;
        asd.as_munmap_len = PRDASIZE;
        asd.as_munmap_flags = 0;
        error = VAS_DELETESPACE(vasid, &asd, NULL);
	ASSERT(error != EBUSY);
	return (error);
}

/*
 * Set accessd on mod times for file by fd
 *
 * Can't set change time...that gets updated as a result of changing
 * the other times.
 *
 * This is used rather than utime() so we can set tv_nsec
 */
static int
ckpt_fsettimes(int fd, timespec_t *timep)
{
	timespec_t times[2];
	struct vattr vattr;

	if (copyin(timep, times, sizeof(times)))
		return EFAULT;
	/*
	 * We ask to set access, mod and change times, although most (all?)
	 * fs implementations don't do change time.
	 */
	vattr.va_mask = AT_TIMES;
	vattr.va_atime = times[0];
	vattr.va_mtime = times[1];

	return (fdsetattr(fd, &vattr, ATTR_UTIME));
}
/*
 * Set accessd on mod times for file by pathname
 *
 * Can't set change time...that gets updated as a result of changing
 * the other times.
 *
 * This is used rather than utime() so we can set tv_nsec
 */
static int
ckpt_settimes(char *fname, timespec_t *timep)
{
	timespec_t times[2];
	struct vattr vattr;

	if (copyin(timep, times, sizeof(times)))
		return EFAULT;

	vattr.va_mask = AT_TIMES;
	vattr.va_atime = times[0];
	vattr.va_mtime = times[1];

	return (namesetattr(fname, FOLLOW, &vattr, ATTR_UTIME));
}

static int
ckpt_setsuid(uid_t suid)
{
	int error;

	if (suid < 0 || suid > MAXUID)
		return EINVAL;

	CURVPROC_SETUID(suid, -1, VSUID_CKPT, error);

	return (error);
}

static int
_ckpt_pmogetnext(pmo_type_t pmo_type, pmo_handle_t pmo_handle, int *rval)
{
	uthread_t *ut = curuthread;

	return (pmo_ckpt_pmo_nexthandle(1, ut->ut_asid, pmo_handle, pmo_type, rval));
}

static int
ckpt_munmap_local(caddr_t addr, size_t len)
{
	vasid_t vasid;
	as_deletespace_t asd;

	if (len <= 0)
		return EINVAL;
	if (poff(addr))
		return EINVAL;
	if (chk_kuseg_abi(get_current_abi(), addr, len))
		return EINVAL;

	as_lookup_current(&vasid);
	asd.as_op = AS_DEL_MUNMAP;
	asd.as_munmap_start = addr;
	asd.as_munmap_len = len;
	asd.as_munmap_flags = DEL_MUNMAP_LOCAL;
	return VAS_DELETESPACE(vasid, &asd, NULL);
}

#ifdef CKPT_DEBUG
static int
ckpt_slowsys(int flag, int cookie, rval_t *rvalp)
{
	sema_t *sp;
	sv_t *svp;
	int s;
	int err;

	sp = sema_alloc(0, -1, NULL);
	svp = sv_alloc(0,0, NULL);

	if (flag & SLOWSYS_SEMAWAIT) {
		s = sema_lock(sp);
		--sp->s_un.s_st.count;
		err = semawait(sp, PSLEP, s, (inst_t *)__return_address);
	} else
		err = sv_wait_sig(svp, 0, NULL, 0);

	ASSERT(err);

	if (flag & SLOWSYS_EINTR)
		err = EINTR;
	else {
		err = 0;
		rvalp->r_val1 = cookie;
	}
	sema_dealloc(sp);
	sv_destroy(svp);

	return err;
}
#endif /* CKPT_DEBUG */
/*ARGSUSED*/
int
ckpt_sys(sysarg_t sysnum, sysarg_t arg1, sysarg_t arg2, sysarg_t arg3,
		sysarg_t arg4, rval_t *rvalp)
{
	/*
	 * we may use struct forka if needed. The casting is good enough for now.
	 */
	extern int pidfork(sysarg_t *, rval_t *);
	int error = 0;
	int rval;
	int s;

	switch (sysnum) {
	case CKPT_OPENDIR:
		/*
		 * Open the directory containing fd
		 */
		error = ckpt_diropen(arg1, arg2, &rval);
		if (!error)
			rvalp->r_val1 = rval;
		break;
	case CKPT_EXECMAP:
		/*
		 * Map an executible
		 */
		error = ckpt_execmap((caddr_t)arg1);
		break;
	case CKPT_SETEXEC:
		/*
		 * Set p_exec
		 */
		error = ckpt_setexec((int)arg1);
		break;
	case CKPT_SETBRK:
		/*
		 * Set brkbase
		 */
		error = ckpt_setbrk((caddr_t)arg1);
		break;

	case CKPT_PIDFORK:
		error = pidfork(&arg1, rvalp);
		break;

	case CKPT_FPADDR:
		error = ckpt_fpaddr(arg1, rvalp);
		break;
	case CKPT_SOCKMATE:
		error = ckpt_sockmate(arg1, rvalp);
		break;
	case CKPT_SVR3PIPE:
		error = ckpt_svr3pipe((caddr_t)arg1, rvalp);
		break;
	case CKPT_GETCPIDS:
		error = ckpt_getchildpids((pid_t)arg1, arg2, (caddr_t)arg3, rvalp);
		break;
	case CKPT_GETSGPIDS:
		error = ckpt_getshdpids((pid_t)arg1, (caddr_t)arg2, (int)arg3, rvalp);
		break;
	case CKPT_UNMAPPRDA:
		error = ckpt_unmapprda();
		break;
	case CKPT_FSETTIMES:
		error = ckpt_fsettimes((int)arg1, (timespec_t *)arg2);
		break;
	case CKPT_SETTIMES:
		error = ckpt_settimes((char *)arg1, (timespec_t *)arg2);
		break;
	case CKPT_FDACCESS:
		error = fdaccess((int)arg1, (int)arg2);
		break;
	case CKPT_SETSUID:
		error = ckpt_setsuid((uid_t)arg1);
		break;
	case CKPT_PMOGETNEXT:
		error = _ckpt_pmogetnext((pmo_type_t)arg1, (pmo_handle_t)arg2, &rval);
		if (!error)
			rvalp->r_val1 = rval;
		break;
	case CKPT_PM_CREATE:
		/*
		 * simulate the exec behavior on aspm
		 */
		aspm_ckpt_swap();
		s = p_lock(curprocp);
        	curprocp->p_flag |= SEXECED;           /* for setpgid() */
        	p_unlock(curprocp, s);

		break;
	case CKPT_MUNMAP_LOCAL:
		error = ckpt_munmap_local((caddr_t)arg1, (size_t)arg2);
		break;
#ifdef CKPT_DEBUG
	case CKPT_SLOWSYS:
		error = ckpt_slowsys(arg1, arg2, rvalp);
		break;
#endif
	case CKPT_TESTSTUB:
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

struct procblka {
	usysarg_t       id;
	sysarg_t        pid;
	sysarg_t        count;
};

#define BLOCK   0
#define UNBLOCK 1
#define SET     2

extern int procblk(struct procblka *);

struct restartreturna {
	sysarg_t	blkcnt;
	sysarg_t	wstat;
	sysarg_t	flags;
};

struct prctla {
        sysarg_t        option;
        sysarg_t        value;
        sysarg_t        value2;
        sysarg_t        value3; /* perm arg to PR_ATTACHADDRPERM */
        sysarg_t        value4; /* "hint" attachaddr to PR_ATTACHADDRPERM */
};

extern int prctl(struct prctla *, rval_t *);

int
ckpt_restartreturn(struct restartreturna *uap)
{
	uthread_t *ut = curuthread;
	struct procblka blkarg;
	struct prctla prctl_arg;
	int why, what;
	rval_t rval;

	if (uap->flags) {
		/*
		 * we're supposed to exit
		 */
		ckpt_wstat_decode(uap->wstat, &why, &what);
		exit(why, what);
		/* no return */
		ASSERT(0);
	}
	/*
	 * Gross hack to prevent us from leaving the kernel
	 * Need to make sure that cpr has completely closed this proc
	 * and dropped all tracing flags so subsequent "block" processing
	 * does not ignore fatal signals.
	 * stop ourselves here, so run-on-last-close, will
	 * run us from here.    If cpr posts SIGKILL to us, it's going to
	 * have the unfortuante side-effect uf unstopping us.  If that
	 * happens, stop again.
	 * Grab uscan lock for access to insure all /proc flags are gone.
	 *
	 * At least this disgusting code is encapsulated here.
	 */
stopme:
	ut_flagset(ut, UT_CKPT);
        stop(ut, CHECKPOINT, 0, 0);
	ut_flagclr(ut, UT_CKPT);
	uscan_access(&UT_TO_PROC(ut)->p_proxy);
	uscan_unlock(&UT_TO_PROC(ut)->p_proxy);
	/*
	 * If we're going to block, there is a signal "ready" and we're still
	 * propen'd, stop again.
	 */
	if ((uap->blkcnt < 0)&&sigisready()&&(UT_TO_PROC(ut)->p_flag & SPROPEN))
		goto stopme;

	if (uap->blkcnt) {
		/* 
		 * we're supposed to block
		 */
		if (ut->ut_pproxy->prxy_shmask & PR_THREADS) {

			ASSERT((uap->blkcnt == -1)||(uap->blkcnt > 0));

			if (uap->blkcnt == -1) {
				prctl_arg.option = PR_THREAD_CTL;
				prctl_arg.value = PR_THREAD_BLOCK;
	
				(void)prctl(&prctl_arg, &rval);
			} else {
				prctl_arg.option = PR_THREAD_CTL;
				prctl_arg.value = PR_THREAD_UNBLOCK;
				prctl_arg.value2 = ut->ut_id;

				while (uap->blkcnt-- > 0)
					(void)prctl(&prctl_arg, &rval);
			}
		} else {
			int i;
			int blkcnt;

			blkarg.pid = current_pid();
			if (uap->blkcnt < 0) {
				blkarg.id = BLOCK;
				blkcnt = -uap->blkcnt;
			} else {
				blkarg.id = UNBLOCK;
				blkcnt = uap->blkcnt;
			}
			blkarg.count = 1;
		
			for (i = 0; i < blkcnt; i++)
				(void)procblk(&blkarg);
		}
	}
	return (0);
}

void ckpt_wstat_decode(int wstat, int *why, int *what)
{
	if (WIFEXITED(wstat)) {
		*why = CLD_EXITED;
		*what = WEXITSTATUS(wstat);

	} else if (WIFSIGNALED(wstat)) {
		*why = (wstat & WCOREFLAG)? CLD_DUMPED : CLD_KILLED;
		*what = WTERMSIG(wstat);

	} else if (WIFSTOPPED(wstat)) {
		*why = CLD_STOPPED;
		*what = WSTOPSIG(wstat);

	} else if (WIFCONTINUED(wstat)) {
		*why = CLD_CONTINUED;
		*what = 0;
	} else
		ASSERT(0);
}
