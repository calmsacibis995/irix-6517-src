/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#include <sys/types.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <ksys/fdt.h>
#include <sys/kmem.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include "pproc_private.h"
#include <sys/schedctl.h>
#include <sys/space.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/uthread.h>
#include <sys/vnode.h>
#include <ksys/vproc.h>
#include <sys/space.h>
#include <sys/sat.h>
#include <sys/runq.h>
#ident        "$Revision: 1.16 $"
extern void gfx_shcheck(uthread_t *);
extern void gfx_shalloc(void);
extern void gfx_shfree(void);
extern void shmiq_sproc(shaddr_t *);

/*
 * share group routines
 */

static struct zone *shaddr_zone;
void
shaddr_init(void)
{
	shaddr_zone = kmem_zone_init(sizeof(shaddr_t), "shaddr");
}

/*
 * allocshaddr - allocate a shared address descriptor for process p
 * a reference count is maintained.
 * Returns: 0 if failure
 *	    1 if ok
 */
int
allocshaddr(proc_t *p)
{
	shaddr_t *sa;
	uthread_t *ut = prxy_to_thread(&p->p_proxy);
	cred_t *cr;

	ASSERT(!IS_SPROC(&p->p_proxy));

	/* not yet initialized alloc room for shaddr struct and open files */
	sa = (shaddr_t *) kmem_zone_alloc(shaddr_zone, KM_SLEEP);

	/* init detachshaddr mutex semaphore */
	init_mutex(&sa->s_detachsem, MUTEX_DEFAULT, "dts", p->p_pid);

	sv_init(&sa->s_detached, 0, "s_detached");

	/* link us on */
	/*
	 * No need to make this assignment atomic -- no other thread
	 * can/will reference sa yet.
	 */
	sa->s_refcnt = 1;
	sa->s_plink = p;
	sa->s_orig = p->p_pid;	/* we are the original */
	p->p_slink = NULL;

	/* init shadowed versions of share group resources */
	sa->s_limit = p->p_rlimit[RLIMIT_FSIZE].rlim_cur;
	sa->s_cmask = ut->ut_cmask;

	cr = pcred_access(p);
	crhold(cr);
	sa->s_cred = cr;
	pcred_unaccess(p);

	/*
	 * Move all open file descriptors to common place.
	 */
	fdt_share_alloc(sa);

	/*
	 * Grab share group's references on current/root directory.
	 */
	if (ut->ut_rdir) {
		ASSERT(ut->ut_rdir->v_count >= 1);
		VN_HOLD(ut->ut_rdir);
		sa->s_rdir = ut->ut_rdir;
	} else
		sa->s_rdir = NULL;

	if (ut->ut_cdir) {
		ASSERT(ut->ut_cdir->v_count >= 1);
		VN_HOLD(ut->ut_cdir);
		sa->s_cdir = ut->ut_cdir;
	} else
		sa->s_cdir = NULL;

	/* init lock on s_plink */
	init_mutex(&sa->s_listlock, MUTEX_DEFAULT, "shl", p->p_pid);

	/* init shared directory update sema */
	init_mutex(&sa->s_fupdsema, MUTEX_DEFAULT, "shd", p->p_pid);

	/* init misc update lock */
	init_spinlock(&sa->s_rupdlock, "shr", p->p_pid);

	/* notify shmiq device driver that we are now a sproc process */
	shmiq_sproc(sa);

	/* set defaults for scheduling parameters */
	sa->s_sched = SGS_FREE;	/* default is independent scheduling */
	sa->s_master = p->p_pid;
	sa->s_gdb = NULL;
	gang_create(sa);
	gang_link(sa, ut);
	/*
	 * The parent shares all normal attributes.
	 */
	p->p_proxy.prxy_shmask = PR_SALL;
	p->p_shaddr = sa;
	prxy_flagset(&p->p_proxy, PRXY_SPROC);

	/* need p_shaddr set */
	gfx_shalloc();

	return(1);
}

/*
 * attachshaddr - attach a process cp to the shared address descriptor of p
 */
void
attachshaddr(
	shaddr_t *sa,
	proc_t *cp)
{
	uint flag;

	mutex_lock(&sa->s_listlock, 0);
	/* add cp to list */
	ASSERT(sa->s_refcnt > 0);
	sa->s_refcnt++;
	ASSERT_ALWAYS(sa->s_refcnt != 0); /* catch rollover */
	cp->p_slink = sa->s_plink;
	sa->s_plink = cp;
	cp->p_shaddr = sa;
	prxy_flagset(&cp->p_proxy, PRXY_SPROC);
	mutex_unlock(&sa->s_listlock);
	gang_link(sa, prxy_to_thread(&cp->p_proxy));
	/*
	 * from the time the parent entered the sproc call to when
	 * we call attachshaddr, another process may have changed the state
	 * of the world and we wouldn't have gotton any SYNC notices
	 * since until now we weren't on the list - make sure we are
	 * synced when we exit
	 */
	flag = 0;
	if (cp->p_proxy.prxy_shmask & PR_SDIR)
		flag |= UT_UPDDIR;
	if (cp->p_proxy.prxy_shmask & PR_SUMASK)
		flag |= UT_UPDUMASK;
	if (cp->p_proxy.prxy_shmask & PR_SULIMIT)
		flag |= UT_UPDULIMIT;
	if (cp->p_proxy.prxy_shmask & PR_SID)
		flag |= UT_UPDUID;
	ut_updset(prxy_to_thread(&cp->p_proxy), flag);
}

/*
 * detachshaddr - remove p from share group
 * If p is the last process using the shaddr_t block, it is de-allocated
 *
 * NOTE: We single thread detaching so that we can guarantee that
 *	the last close of a file descriptor occurs with the last
 *	sproc to leave (this fixes problems with shmiq).
 *	We do this with a special semaphore
 */
int
detachshaddr(proc_t *p, int flags)
{
	shaddr_t *sa = p->p_shaddr;
	uthread_t *ut = prxy_to_thread(&p->p_proxy);
	proc_t **pp;
	proc_t *lp;
	int rv = 0;
	int nfdsharing;
	int s;
#ifdef DEBUG
	int found = 0;
#endif

	ASSERT(sa->s_refcnt > 0);

	/*
	 * If we are the creator of the share group - don't detach
	 * our stack. This lets argv's and environ variables still work
	 */
	if (sa->s_orig == current_pid())
		sa->s_orig = NULL;
	else
		rv = 1;

	/* Single thread from here on down */
	mutex_lock(&sa->s_detachsem, PZERO);

	/* remove p from shared list */
	mutex_lock(&sa->s_listlock, 0);
	gang_endrun(ut, sa->s_gdb);
	pp = &sa->s_plink;
	nfdsharing = 0;
	while (*pp) {
		if ((*pp)->p_proxy.prxy_shmask & PR_SFDS)
			nfdsharing++;
		if (*pp == p) {
			*pp = p->p_slink;
#ifdef DEBUG
			found++;
#endif
		} else
			pp = &((*pp)->p_slink);
	}
	ASSERT(found == 1);
	p->p_slink = NULL;
	sa->s_refcnt--;
	gang_exit(sa);
	mutex_unlock(&sa->s_listlock);

	if (sa->s_refcnt == 0) {

		gang_free(sa);
		sv_destroy(&sa->s_detached);

		/*
		 * Can release detachsem here; since we sucessfully did
		 * a lock operation and this is last reference,
		 * all others who found non-zero ref-count would have
		 * completed their execution of detachshaddr.
		 */
		mutex_unlock(&sa->s_detachsem);

		/*
		 * give back share group's references on things like
		 * current directory
		 */
		if (sa->s_rdir) {
			ASSERT(sa->s_rdir->v_count >= 1);
			VN_RELE(sa->s_rdir);
		}
		if (sa->s_cdir) {
			ASSERT(sa->s_cdir->v_count >= 1);
			VN_RELE(sa->s_cdir);
		}
		if (sa->s_gdb)
			gang_free(sa);

		if (p->p_proxy.prxy_shmask & PR_SFDS) {
			/*
			 * if execing, just move fd's back to user struct.
			 * if exiting, then close all fds.
			 */
			fdt_share_dealloc(p, DETACH_REASON(flags));
		} 

		ut_updclr(ut, UTSYNCFLAGS);

		gfx_shfree();	/* doit before clearing p_shaddr */

		p->p_proxy.prxy_shmask = 0;
		prxy_flagclr(&p->p_proxy, PRXY_SPROC);
		p->p_shaddr = NULL;

		crfree(sa->s_cred);
		ASSERT(mutex_owner(&sa->s_fupdsema) == NULL);
		mutex_destroy(&sa->s_detachsem);
		mutex_destroy(&sa->s_fupdsema);
		spinlock_destroy(&sa->s_rupdlock);
		mutex_destroy(&sa->s_listlock);
		kmem_zone_free(shaddr_zone, sa);

		return rv;
	}

	/*
	 * don't release detachsem; so no other member of the
	 * sharegroup may go in and start nuking the address space
	 * from under us --- we may need the address space to
	 * be intact, as we may be doing close() down below.
	 */
	/* 
	 * if we aren't the last one and aren't sharing fds - we
	 * shouldn't touch the share group's fds
	 */
	if (p->p_proxy.prxy_shmask & PR_SFDS) {
		/*
		 * if execing, copy fds from shared area to upage,
		 * taking additional file struct references.
		 * This applies to ALL fds since FCLOSEXEC ones at
		 * least need to call vfile_close to release file locks.
		 * 
		 * if exiting, do close processing on open files.
		 * 
		 * detachsema protects us from someone tearing down 
		 * the entire shaddr struct.
		 */
		ASSERT(nfdsharing > 0);
		p->p_proxy.prxy_shmask &= ~PR_SFDS;
		fdt_share_detach(p, DETACH_REASON(flags), nfdsharing == 1);
	}

	if (UT_TO_KT(ut)->k_runcond & RQF_GFX)
		gfx_shcheck(ut);

	/*
	 * We're not the last one -- if requested, send signal to all survivors.
	 */
	if (s = p->p_exitsig) {
		k_siginfo_t si;

		bzero((caddr_t)&si, sizeof(si));
		si.si_signo = s;
		si.si_code = EXIT_DECODE(flags);

		mutex_lock(&sa->s_listlock, 0);
		for (lp = sa->s_plink; lp; lp = lp->p_slink)
			sigtopid(lp->p_pid, s, SIG_ISKERN, 0, 0, &si);
		mutex_unlock(&sa->s_listlock);
	}

	ut_updclr(ut, UTSYNCFLAGS);

	p->p_proxy.prxy_shmask = 0;
	prxy_flagclr(&p->p_proxy, PRXY_SPROC);
	p->p_shaddr = NULL;

	/*
	 * now release detachsem --- we're all done.
	 */
	mutex_unlock(&sa->s_detachsem);

	return rv;
}

/*
 * getshdpids - get share group pids
 */
int
getshdpids(proc_t *p, pid_t *buf, int bufsize)
{
	shaddr_t *sa = p->p_shaddr;
	register proc_t *pp;
	int count = 0;

	if (!IS_SPROC(&p->p_proxy))
		return(0);

	mutex_lock(&sa->s_listlock, 0);
	for (pp = sa->s_plink; pp && bufsize; pp = pp->p_slink) {
		*buf++ = pp->p_pid;
		bufsize -= sizeof(pid_t);
		count++;
	}
	mutex_unlock(&sa->s_listlock);

	return (count);
}

/*
 * setshdsync - arrange for all share group processes to sync up
 */
void
setshdsync(
	shaddr_t *sa,
	proc_t *notpp,		/* don't update caller */
	int sres,		/* which shared resource */
	int sflag)		/* which flag in ut_flags to set */
{
	proc_t *pp;

	mutex_lock(&sa->s_listlock, 0);

	for (pp = sa->s_plink; pp; pp = pp->p_slink) {
		if (pp != notpp && pp->p_proxy.prxy_shmask & sres) {
			ut_updset(prxy_to_thread(&pp->p_proxy), sflag);
		}
	}

	mutex_unlock(&sa->s_listlock);
}

static void
syncdir(vnode_t **vpp,		/* pointer to process's c/r dir */
	vnode_t *svp)		/* vnode to share */
{
	if (svp && svp != *vpp) {
		if (*vpp) {
			/* remove old one */
			VN_RELE(*vpp);
		}
		VN_HOLD(svp);
		*vpp = svp;
	}
}

/*
 * dodsync - sync up current directories
 */
static void
sproc_dodsync(proc_t *p)
{
	shaddr_t *sa = p->p_shaddr;
	uthread_t *ut = prxy_to_thread(&p->p_proxy);

	mutex_lock(&sa->s_fupdsema, PZERO);

	syncdir(&ut->ut_rdir, sa->s_rdir);
	syncdir(&ut->ut_cdir, sa->s_cdir);
	mutex_unlock(&sa->s_fupdsema);
}

/*
 * sync umask
 */
static void
sproc_doumasksync(proc_t *p)
{
	uthread_t *ut = prxy_to_thread(&p->p_proxy);
	shaddr_t *sa = p->p_shaddr;
	int s;

	s = mutex_spinlock(&sa->s_rupdlock);
	ut->ut_cmask = sa->s_cmask;
	mutex_spinunlock(&sa->s_rupdlock, s);
}

/*
 * sync ulimit
 */
static void
sproc_doulimitsync(proc_t *p)
{
	shaddr_t *sa = p->p_shaddr;
	int s;

	s = mutex_spinlock(&sa->s_rupdlock);
	p->p_rlimit[RLIMIT_FSIZE].rlim_cur = sa->s_limit;
	mutex_spinunlock(&sa->s_rupdlock, s);
}

/*
 * sync uid/gid
 */
static void
sproc_doidsync(proc_t *p)
{
	shaddr_t *sa = p->p_shaddr;
	uthread_t *ut = curuthread;	/* is p's only uthread */
	int s;
	struct cred *cr;
	uid_t old_ruid;

	/*
	 * Hold pcred_lock() to synchronize with others trying to check our
	 * credentials.  Locking order is pcred_lock first, then s_rupdlock.
	 * Call crcopy first in case it sleeps allocating a new cred.
	 */
	cr = pcred_lock(p);
	crfree(ut->ut_cred);	/* before crcopy, to optimize crcopy */
	old_ruid = cr->cr_ruid;

	if (!_CAP_CRABLE(cr, CAP_PROC_MGT)) {
		/* if not privileged just update effective id */
		cr = crcopy(p);
		s = mutex_spinlock(&sa->s_rupdlock);
		cr->cr_uid = sa->s_cred->cr_uid;
		cr->cr_gid = sa->s_cred->cr_gid;
	} else {
		crfree(cr);
		s = mutex_spinlock(&sa->s_rupdlock);
		cr = p->p_cred = sa->s_cred;
		crhold(cr);
	}
	mutex_spinunlock(&sa->s_rupdlock, s);

	/*
	 * update uthread's copy of process credential
	 * before releasing process credential lock
	 */
	ut->ut_cred = cr;
	crhold(cr);
	pcred_unlock(p);

	/* Update count of this uid's active processes */
	uidact_switch(old_ruid, cr->cr_ruid);
}

/*
 * dosync - handle synchronization requests for various resources
 */
void
dosync(int tflag)
{
	proc_t	*p = curprocp;

	ASSERT(IS_SPROC(&p->p_proxy));

	if (tflag & UT_UPDDIR) {
		sproc_dodsync(p);			/* cur/root dir */
		_SAT_PN_BOOK(-1, curuthread);		/* update directory */
		if (tflag & UT_SAT_CWD)			/* for audit trail */
			_SAT_UPDATE_RWDIR(SAT_CHDIR,curuthread);
		if (tflag & UT_SAT_CRD)
			_SAT_UPDATE_RWDIR(SAT_CHROOT,curuthread);
	}

	/*
	 * Must update uid/gid before uthread's credentials pointer --
	 * process credential pointer could change in sproc_doidsync.
	 */
	if (tflag & UT_UPDUID)
		sproc_doidsync(p);		/* uid/gid */
	if (tflag & UT_UPDCRED)
		credsync();			/* ut_cred */
	if (tflag & UT_UPDULIMIT)
		sproc_doulimitsync(p);		/* ulimit */
	if (tflag & UT_UPDUMASK)
		sproc_doumasksync(p);		/* umask */
}
