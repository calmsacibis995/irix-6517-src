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

#ident	"$Revision: 1.12 $"

#include <sys/types.h>
#include <ksys/cred.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include "pproc_private.h"
#include <sys/sat.h>
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/uthread.h>
#include <ksys/vproc.h>

/*
 * canblock - check if the running process has permission to block process 'p'
 *
 * If process is member of share group, and is sharing uids,
 * then check permissions against current settings in share group.
 * (The process in question may not have had a chance to synchronize).
 * The process could however be updating its uid, but that seems
 * to be an OK race.
 */
static int
canblock(
	proc_t	*p,
	cred_t	*cr)
{
	shaddr_t *sa;
	uid_t uid;
	uid_t suid;
	int s;

	if (IS_SPROC(&p->p_proxy) && (p->p_proxy.prxy_shmask & PR_SID)) {
		/*
		 * prevent s_cred from changing by another 
		 * share group process doing an set[ug]id 
		 */
		sa = p->p_shaddr;
		s = mutex_spinlock(&sa->s_rupdlock);
		uid = p->p_shaddr->s_cred->cr_uid;
		suid = p->p_shaddr->s_cred->cr_suid;
		mutex_spinunlock(&sa->s_rupdlock, s);
	} else  {
		/*
		 * pcred_access(p) to prevent us from using a stale cred 
		 * pointer if p does a setuid
		 */
		cred_t *cr = pcred_access(p);
		uid = cr->cr_uid;
		suid = cr->cr_suid;
		pcred_unaccess(p);
	}

	if (! ( cr->cr_uid == uid || cr->cr_ruid == uid ||
		cr->cr_uid == suid || cr->cr_ruid == suid ||
	        _CAP_CRABLE(cr, CAP_PROC_MGT) )) {
			return 0;
	} else {
		return 1;
	}
}

#define BLOCK	0
#define UNBLOCK	1
#define SET	2

/* 3 4 5 are used to refer to the entire share group */

static int
doprocblk(
	proc_t	*p,
	int	isself,
	cred_t	*cr,
	int	action,
	int	count)
{
	ssleep_t *st;
	int s;
	uthread_t *ut;

	/* share group member might be exiting - ignore */
	if (p->p_stat == SZOMB)
		return 0;

	if (!isself && !canblock(p, cr))
		return EPERM;

	/* This only operates on single-threaded processes, so
	 * just grabbing prxy_threads is ok.
	 */
	ut = prxy_to_thread(&p->p_proxy);
	st = &ut->ut_pblock;

	s = ut_lock(ut);
	if (action == BLOCK) {
		/* if not already minimum value, block */
		if (st->s_waitcnt < PR_MAXBLOCKCNT) {
			blockset(ut, -1, 1, s);
			return 0;
		}
	} else if (action == UNBLOCK) {
		/* if not already maximum value, unblock */
		if (st->s_slpcnt < PR_MAXBLOCKCNT) {
			blockset(ut, 1, 1, s);
			return 0;
		}
	} else { /* (action == SET) */
		/* blockset unlocks ut. */
		blockset(ut, count, 0, s);
		return 0;
	}

	ut_unlock(ut, s);

	return 0;
}

/* pproc layer for the procblk system call */
int
pproc_procblk(
	bhv_desc_t	*bhv,
	int		action,
	int		count,
	cred_t		*cr,		/* Callers credentials */
	int		isself)
{
	proc_t	*p;
	proc_t	*sp;
	int	blkerr;
	int	error;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	if (p->p_proxy.prxy_shmask & PR_THREADS)
		return(EPERM);

	/* Various permission checks */
	if (!isself) {
		cred_t *pcr = pcred_access(p);
		if (_MAC_ACCESS(pcr->cr_mac, cr, MACWRITE)
		      && !_CAP_CRABLE(cr, CAP_KILL)) {
			pcred_unaccess(p);
			return(EPERM);
		 }
		pcred_unaccess(p);
	}

	if (action > 2 && IS_SPROC(&p->p_proxy)) {
		mutex_lock(&p->p_shaddr->s_listlock, 0);

		for (sp = p->p_shaddr->s_plink; sp; sp = sp->p_slink) {
			if (blkerr = doprocblk(sp, 0, cr, action-3, count))
				error = blkerr;
		}
		mutex_unlock(&p->p_shaddr->s_listlock);
	} else
		error = doprocblk(p, isself, cr, action, count);

	_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE, p->p_pid, p->p_cred, error, -1);
	return(error);
}

/* Implement process-specific portions of the prctl system call */
int
pproc_prctl(
	bhv_desc_t	*bhv,
	int		option,
	sysarg_t	arg,
	int		isself,
	cred_t		*cr,			/* Callers credentials */
	pid_t		callers_pid,
	rval_t		*rvp)
{
	proc_t		*p;
	uthread_t	*ut;
	ssleep_t	*st;
	shaddr_t	*sa;
	proc_t		*lp;
	int		s;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	switch (option) {
	default:
		ASSERT(0);
		return(EINVAL);

	case PR_ISBLOCKED:
		/*
		 * return  true   (1) if named process is blocked,
		 * 	   false  (0) if process is not blocked,
		 * 	   error (-1) if process does not exist
		 * we protect the contents of ut_block with ut_lock.
		 */

		if (!isself && !canblock(p, cr))
			return(EPERM);

		rvp->r_val1 = 0;

		/* procblock operations are not defined on multi-threaded
		 * processes, so just return 0 if this is a multi-threaded
		 * process.
		 */
		if (p->p_proxy.prxy_shmask & PR_THREADS)
			return(0);

		/* Not a multi-threaded app, so this is ok. */
		ut = prxy_to_thread(&p->p_proxy);
		s = ut_lock(ut);
		st = &ut->ut_pblock;
		if (st->s_waitcnt > st->s_slpcnt || sv_waitq(&st->s_wait))
			rvp->r_val1 = 1;
		ut_unlock(ut, s);
		return(0);

	case PR_UNBLKONEXEC:
		ASSERT(!isself);

		if (!canblock(p, cr))
			return(EPERM);
		return(0);

	case PR_GETSHMASK:
		ASSERT(!isself);

		if (!IS_SPROC(&p->p_proxy))
			return(EINVAL);

		if (!canblock(p, cr))
			return(EPERM);

		/* The semantics of this call require that the caller
		 * be a member of the same share group as the target
		 * process. Walk our shaddr list to determine that.
		 */
		sa = p->p_shaddr;
		mutex_lock(&sa->s_listlock, 0);
		for (lp = sa->s_plink; lp; lp = lp->p_slink) {
			if (lp->p_pid == callers_pid)
				break;
		}
		mutex_unlock(&sa->s_listlock);

		if (lp == NULL)
			return(EINVAL);

		/* Ok, return the shmask of the target process. */
		rvp->r_val1 = p->p_proxy.prxy_shmask;
		return(0);

	case PR_COREPID:
		if (!isself && !canblock(p, cr))
			return(EPERM);

		if (arg == 0)
			p_flagclr(p, SCOREPID);
		else
			p_flagset(p, SCOREPID);
		return(0);

	case PR_SETEXITSIG:
		if (!IS_SPROC(&p->p_proxy)) {
			p_flagclr(p, SABORTSIG);
			p->p_exitsig = arg;
			return(0);
		}

		/*
		 * Propagate the exit signal to all members of the share
		 * group.  This turns off sig-on-abort.
		 */
		sa = p->p_shaddr;
		mutex_lock(&sa->s_listlock, 0);
		for (p = sa->s_plink; p; p = p->p_slink) {
			p->p_exitsig = arg;
			p_flagclr(p, SABORTSIG);
		}
		mutex_unlock(&sa->s_listlock);

		return(0);

	case PR_SETABORTSIG:
		if (!IS_SPROC(&p->p_proxy)) {
			p->p_exitsig = arg;
			if (arg)
				p_flagset(p, SABORTSIG);
			return(0);
		}

		/*
		 * Propagate the exit signal to all members of the share
		 * group.  This turns off sig-on-exit.
		 */
		sa = p->p_shaddr;
		mutex_lock(&sa->s_listlock, 0);
		for (p = sa->s_plink; p; p = p->p_slink) {
			p->p_exitsig = arg;
			if (arg)
				p_flagset(p, SABORTSIG);
		}
		mutex_unlock(&sa->s_listlock);

		return(0);

	case PR_TERMCHILD:
		p_flagset(p, SCEXIT);

		return(0);

	case PR_GETNSHARE:
		if (!IS_SPROC(&p->p_proxy))
			return(0);
		rvp->r_val1 = p->p_shaddr->s_refcnt;
		return(0);

	case PR_LASTSHEXIT:
		if (!IS_SPROC(&p->p_proxy)) {
			rvp->r_val1 = 1;
		} else {
			sa = p->p_shaddr;
			mutex_lock(&sa->s_listlock, 0);
			p_flagset(p, SWILLEXIT);
			rvp->r_val1 = 1;
			for (p = sa->s_plink; p; p = p->p_slink) {
				if ((p->p_flag & SWILLEXIT) == 0) {
					rvp->r_val1 = 0;
					break;
				}
			}
			mutex_unlock(&sa->s_listlock);
		}

		return(0);
	}
}

int
pproc_set_unblkonexecpid(
	bhv_desc_t	*bhv,
	pid_t		unblkpid)
{
	proc_t		*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* if process already has an unblock pid then error */
	if (p->p_unblkonexecpid)
		return(EINVAL);
	p->p_unblkonexecpid = unblkpid;
	return(0);
}

void
pproc_unblkpid(
	bhv_desc_t	*bhv)
{
	proc_t		*p;
	uthread_t	*ut;
	int		s;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	if (p->p_proxy.prxy_shmask & PR_THREADS)
		return;

	ut = prxy_to_thread(&p->p_proxy);

	s = ut_lock(ut);
	blockset(ut, 1, 1, s);		/* blockset unlocks ut. */
}

