/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: ppgrp.c,v 1.54 1998/01/10 02:40:01 ack Exp $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/conf.h"
#include "sys/errno.h"
#include "sys/ksignal.h"
#include "sys/cred.h"
#include "pproc_private.h"
#include "ksys/vsession.h"
#include "sys/sema.h"
#include "sys/var.h"
#include "sys/debug.h"
#include "sys/sysinfo.h"
#include "sys/kmem.h"
#include "sys/cmn_err.h"
#include "ksys/pid.h"
#include "ksys/childlist.h"
#include <values.h>

#include <ksys/vproc.h>
#include <ksys/vpgrp.h>
#include "vpgrp_private.h"
#include "pproc.h"

/* prototypes for static functions */
static int proc_contributesjcl(proc_t *, pid_t, pid_t);
static int anystop(pgrp_t *, pid_t);

typedef int	(*pgrpscanfunc_t)(struct proc *, long);

/*
 * process group -  Each process in the system is a member of a process group
 *		    that is identified by a process group ID.  This grouping
 *		    permits the signaling of related processes.  New processes
 *		    join the process group of their creators.
 *
 * orphaned process group - A process group in which the parent of every 
 *			    member is either itself a member of the group or 
 *			    is not a member of the group's session.
 *			    The point being that job control status for an
 *			    orphaned process group could not be reported to
 *			    any other process in the same (or any) session.
 *
 * locking:	
 *		Each pgroup has its own mutex lock, which protects its own
 *		chain, member count and various fields.  There is also a
 *		list access mode lock, which in read mode represents the
 *		number of pending signal operations against the processesr
 *		in the process group, and in write mode represents the number
 *		of list updates in progress or oustanding. Note that in write
 *		mode, the pgrp lock must remain held while performing an
 *		update to the list; whereas, in read mode, the pgrp lock
 *		can be dropped (after switching to read mode) while the
 *		list chain is traversed.
 *
 *		A process wishing to change its (or another's) process group
 *		acquires the target's p_who mrlock in update mode -- this
 *		avoids simultaneous pgroup (or session) changes; any
 *		process may dereference p_vpgrp while holding either p_who
 *		in access mode, or the p_siglock spinlock -- p_siglock must
 *		be held when p_vpgrp, p_pgid or p_sid are changed.
 *
 * The physical pgrp structure is reached from the virtual pgrp via
 * behavior chain reference. The pgrp's behavior descriptor is embedded
 * in the pgrp structure. This behavior descriptor points to the pgrp_t
 * itself and back to the associated vpgrp_t. Vpgrp structures are hashed
 * by pgid for lookups.

    ------------
    |          |<------------------------------ 
    |          |                               |
    |          |-- VPGRP_TO_FIRST_BHV()-       |
    ------------                        |      |
      vpgrp_t                           |      |
                                        |      |
  first behavior <----------------------       |
        ...                                    |
  previous behavior                            |
         |                                     |
         v                                     |
    --------------                             |
    | pg_bhv     |-- BHV_TO_VPGRP()------------ 
    --------------
    :    |     :
    :    v     : 	-----------	-----------		-----------
    --------------	|	  |	|	  |		|	  |
    | pg_chain   |----->|p_pgroup |	|p_pgroup |		|p_pgroup |
    |------------|	|---------|	|---------|		|---------|
    | pg_sid     |	|p_pgflink|---->|p_pgflink|---->...	|p_pgflink|--
    |------------|	|---------|	|---------|		|---------|  |
    | pg_memcnt  |    --|p_pgblink|<----|p_pgblink|	...<----|p_pgblink| ---
    |------------|   |	|---------|	|---------|		|---------|
    | pg_jclcnt  |  --- |  ...	  |	|  ...	  |		|  ...	  |
    |------------|	|---------|	|---------|		|---------|
    | pg_lockmode|	|  ...	  |	|  ...	  |		|  ...	  |
    |------------|	|---------|	|---------|		|---------|
    | pg_lock    |	    proc	   proc			   proc
    --------------
       pgrp_t

*/


#define JCL_CONTRIBUTOR(_ch_pgid, _ch_sid, _p_pgid, _p_sid) \
	(_ch_pgid != _p_pgid && _ch_sid == _p_sid)	

/*
 * Return 1 if the parent process of process cp prevents
 * process group pgid in session sid from being orphaned.
 */
static int
proc_contributesjcl(proc_t *cp, pid_t pgid, pid_t sid)
{
	vp_get_attr_t	pattr;
	vproc_t		*pvp;

	pvp = VPROC_LOOKUP_STATE(cp->p_ppid, ZNO);
	if (pvp == NULL)
		return 0;

	VPROC_GET_ATTR(pvp, VGATTR_JCL, &pattr);
	VPROC_RELE(pvp);

	/*
	 * Now the acid test:
	 * is our parent in the same session but in a different process group.
	 */
        return JCL_CONTRIBUTOR(pgid, sid, pattr.va_pgid, pattr.va_sid);
}

/*
 * pgroupscan - examine every element on the process group chain,
 *	and call the named routine for each one.  if the routine
 *	returns non-zero, stop the scan and return.  returns non-zero
 *	if aborted by the called routine, zero otherwise.
 *
 * The caller has the specific pgrp held.
 */
int
pgroupscan(
	pgrp_t *pg,
	pgrpscanfunc_t pfunc,
	long arg)
{
	proc_t	*tp;
	int tmp = 0;
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(&pg->pg_bhv);

	ASSERT(PGRPLIST_IS_READLOCKED(pg));

	for (tp = pg->pg_chain; tp != NULL; tp = tp->p_pgflink) {
		if (tmp = (*pfunc)(tp, arg))
			break;
	}

	return(tmp);
}

/* ARGSUSED */
static int
anyustop(uthread_t *ut, void *annoy)
{
	return (((ut->ut_flags & UT_STOP) && (ut->ut_whystop == JOBCONTROL))
	  ||	sigsetismember(&ut->ut_sig, &stopdefault));
}

/*
 * anystop - any members of passed process group stopped?
 *
 * Called with pgroup list lock held.
 */
static int
anystop(
	pgrp_t *pg,
	pid_t exclude_pid)
{
	proc_t *mp;

 	for (mp = pg->pg_chain; mp; mp = mp->p_pgflink) {
		if (mp->p_pid == exclude_pid)
			continue;
		if ((mp->p_flag & SJSTOP) ||
		    uthread_apply(&mp->p_proxy, UT_ID_NULL, anyustop, 0))
			return 1;
	}
	return(0);
}

sequence_num_t
ppgrp_sigseq(
	bhv_desc_t	*bdp)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	return pg->pg_sigseq;
}

void
ppgrp_set_sigseq(
	bhv_desc_t	*bdp,
	sequence_num_t	sequence)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	pg->pg_sigseq = sequence;
}

int
ppgrp_sig_wait(
	bhv_desc_t	*bdp,
	sequence_num_t	sequence)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	int	retval = 1;

	PGRP_LOCK(pg);
	ASSERT(pg->pg_memcnt > 0);

	/*
	 * XXX - punt for PGRP_SIGSEQ_LT, returning non-zero is functionally
	 * correct. This op is on the verge of extinction anyhow.
	 * If the list is in read mode, we'll assume that a pgrp signal
	 * is being delivered and so we will return non-zero and force
	 * waitsys to retry.
	 */
	if (!PGRPLIST_IS_READLOCKED(pg)) {
		if (PGRP_SIGSEQ_GT(pg,sequence))
			retval = -1;
		else if (PGRP_SIGSEQ_GEQ(pg,sequence))
			retval = 0;
	}

	PGRP_UNLOCK(pg);
	return retval;
}


/*
 * Foreach child, update child's pgrp's orphan status as a result
 * of parent's new state.
 */
void
link_children(
	proc_t	*p,
	pid_t	pgid,
	pid_t	sid)
{
	child_pidlist_t **list;
	child_pidlist_t *cpid;
	vproc_t		*vpr;

	mutex_lock(&p->p_childlock, PZERO);
	list = &p->p_childpids;

	while (cpid = *list) {
		vpr = VPROC_LOOKUP_STATE(cpid->cp_pid, ZYES);
		ASSERT(vpr != NULL);
		VPROC_PGRP_LINKAGE(vpr, pgid, sid);
		list = &cpid->cp_next;
		VPROC_RELE(vpr);
	}

	mutex_unlock(&p->p_childlock);
}

void
ppgrp_setattr(
	bhv_desc_t	*bdp,
	int		*is_batchp)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);

	if (is_batchp != NULL) 
		pg->pg_batch = *is_batchp;
}

/* Physical behavior for local process (sub-)groups. */

/*
 * Get process group attributes.
 * A NULL pointer can be supplied if a particular attribute is not needed.
 */
void
ppgrp_getattr(
	bhv_desc_t	*bdp,
	pid_t		*sidp,
	int		*is_orphanedp, 
	int		*is_batchp)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	if (is_orphanedp != NULL)
		*is_orphanedp = (pg->pg_jclcnt == 0);
	if (sidp != NULL) 
		*sidp = vpg->vpg_sid;
	if (is_batchp != NULL) 
		*is_batchp = pg->pg_batch;
}

/*
 * Perform process group orphan action.
 * This is called only on the server cell.
 */
void
ppgrp_orphan(
	bhv_desc_t	*bdp,
	int		is_orphaned,
	int		is_exit)
{
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);
	int	is_stopped;

	ASSERT(BHV_POSITION(VPGRP_TO_FIRST_BHV(vpg)) != VPGRP_POSITION_DC);

	if (is_orphaned && is_exit) {
		VPGRP_ANYSTOP(vpg, 0, &is_stopped);
		if (is_stopped)
			VPGRP_SENDSIG(vpg, SIGHUP, VPG_SENDCONT, 0, NULL, NULL);
	}

	return;
}

int
ppgrp_join_begin(
	bhv_desc_t	*bdp)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	PGRP_LOCK(pg);

	/*
	 * May have lost a race with a leaving member and the pgrp
	 * is now marked for destruction.
	 */
	if (!VPGRP_IS_VALID(vpg)) {
		PGRP_UNLOCK(pg);
		return 1;
	}

	if (++pg->pg_memcnt == 1)
		VPGRP_MEMBERSHIP(vpg, 1, vpg->vpg_sid);

	PGRP_UNLOCK(pg);
	return 0;
}

/*
 * Add a (local) process to a process group
 */
void
ppgrp_join_end(
	bhv_desc_t	*bdp,
	proc_t		*p,
	int		attach)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	PGRP_LOCK(pg);

	/*
	 * Get the pgrp list for update.
	 * If some action's currently directed at the entire process group,
	 * wait until the action's over.
	 */
	PGRPLIST_WRITELOCK(pg);

	if (pg->pg_chain == NULL) {
		pg->pg_jclcnt = 0;
	}

	p->p_pgblink = NULL;
	p->p_pgflink = pg->pg_chain;
	if (pg->pg_chain)
		pg->pg_chain->p_pgblink = p;
	pg->pg_chain = p;
 
	/*
	 * Release the list "lock". We still have the pgrp lock, though.
	 */
	PGRPLIST_UNLOCK(pg);

	if (attach) {
		/*
		 * A process joins a new process group (or makes
		 * new group unto itself).
		 */
		if (proc_contributesjcl(p, vpg->vpg_pgid, vpg->vpg_sid)) {
			p_flagset(p, SPGJCL);

			/* 
			 * If creating a new process group or the existing group 
			 * is detached, this process may be attaching the group.
			 * Need to hold local membership and drop the pgrp lock
			 * while doing this, though.
			 */
			if (++pg->pg_jclcnt == 1) {
				PGRPLIST_READLOCK(pg);	/* Get read */
				PGRP_UNLOCK(pg);
				VPGRP_ORPHAN(vpg, 0, 0);
				PGRP_LOCK(pg);
				ASSERT(PGRPLIST_IS_READLOCKED(pg));
				PGRPLIST_UNLOCK(pg);
			}
		}
		PGRP_UNLOCK(pg);
		/*
		 * If the process in question is a parent, its new pgrp can
		 * change the orphan status of its children pgrps.
		 */
		link_children(p, vpg->vpg_pgid, vpg->vpg_sid);
	} else
		PGRP_UNLOCK(pg);
}

/*
 * Remove a process from a process group
 * If it is the last member of the process group, deallocate the pgroup.
 * If it is not the last member of the process group, and this process
 *      will cause the process group to be orphaned, detach the process group
 */

void
ppgrp_leave(
	bhv_desc_t	*bdp,
	proc_t		*p,
	int		exitting)
{
	/* REFERENCED */
	pid_t		pgid;
	pgrp_t		*pg = BHV_TO_PPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	ASSERT(ismrlocked(&p->p_who, MR_UPDATE));

	/*
	 * The p_who mrlock stops others (other processes or other threads
	 * within the p
	 */

	pgid = p->p_pgid;

	ASSERT(pgid != -1);
	ASSERT(pgid == vpg->vpg_pgid);

	/*
	 * Must have pgrp_lock to dequeue pg from hash list.
	 */
	PGRP_LOCK(pg);
	PGRPLIST_WRITELOCK(pg);

	/*
	 * Remove p from this process group
	 */
	if (p->p_pgblink == NULL) {
		/*
		 * p is first on q
		 */
		pg->pg_chain = p->p_pgflink;
	} else {
		p->p_pgblink->p_pgflink = p->p_pgflink;
	}
	if (p->p_pgflink != NULL)
		p->p_pgflink->p_pgblink = p->p_pgblink;

	p->p_pgflink = p->p_pgblink = NULL;

        if (--pg->pg_memcnt == 0)
		VPGRP_MEMBERSHIP(vpg, 0, p->p_sid);

	PGRPLIST_UNLOCK(pg);

	if (p->p_flag & SPGJCL) {
		p_flagclr(p, SPGJCL);
		ASSERT(pg->pg_jclcnt > 0);
		if (--pg->pg_jclcnt == 0) {
			/*
			 * The pgrp previously not (locally) orphaned
			 * but this process leaving changes that.
			 * Hold the membership and invoke another
			 * vop to check global state and do the
			 * right POSIX stuff.
			 */
			if (exitting) {
				PGRPLIST_READLOCK(pg);
				PGRP_UNLOCK(pg);
				VPGRP_ORPHAN(vpg, 1, exitting);
				PGRP_LOCK(pg);
				PGRPLIST_UNLOCK(pg);
			} else {
				VPGRP_ORPHAN(vpg, 1, exitting);
			}
		}
	}

	PGRP_UNLOCK(pg);
}

/*
 * Re-evaluate the orphan status of a process group for which
 * a parent process of a given member is exiting.
 */
void
ppgrp_detach(
	bhv_desc_t	*bdp,
	proc_t		*p)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	ASSERT(pg->pg_memcnt > 0);

	/*
	 * Fast check for pgrp already orphaned - if the local
	 * pgrp is orphaned, there will be no change resulting
	 * from this detach.
	 * Otherwise, we must use the VPGRP operation to perform
	 * an orphan check.
	 */
	PGRP_LOCK(pg);
	if (pg->pg_jclcnt == 0) {
		ASSERT(!(p->p_flag & SPGJCL));
		PGRP_UNLOCK(pg);
		return;
	}

	if (p->p_flag & SPGJCL) {
		p_flagclr(p, SPGJCL);
		ASSERT(pg->pg_jclcnt > 0);
		if (--pg->pg_jclcnt == 0) {
			/*
			 * The pgrp previously not (locally) orphaned
			 * but this process leaving changes that.
			 * Hold the membership and invoke another
			 * vop to check global state and do the
			 * right POSIX stuff.
			 */
			PGRPLIST_READLOCK(pg);
			PGRP_UNLOCK(pg);
			VPGRP_ORPHAN(vpg, 1, 1);
			PGRP_LOCK(pg);
			PGRPLIST_UNLOCK(pg);
		}
	}
	PGRP_UNLOCK(pg);
}

/*
 * Re-evaluate the orphan status of a process group for which
 * the parent of a member has changed pgid/sid. This may result
 * in either orphaning or non-orphaning the group. Note that
 * if the group is orphaned in this case we don't go doing the
 * POSIX SIGHUP/SIGCONT stuff.
 */
void
ppgrp_linkage(
	bhv_desc_t	*bdp,
	proc_t		*p,
	pid_t		parent_pgid,
	pid_t		parent_sid)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);
	int	contribution_change;

	ASSERT(pg->pg_memcnt > 0);
	contribution_change = (JCL_CONTRIBUTOR(p->p_pgid, p->p_sid,
					       parent_pgid, parent_sid) ? 1 : 0)
				- ((p->p_flag & SPGJCL) ? 1 : 0);
	switch (contribution_change) {
	case 0:
		break;
	case 1:
		/* We're now a contributor and may non-orphan the group */
		PGRP_LOCK(pg);
		p_flagset(p, SPGJCL);
		if (++pg->pg_jclcnt == 1) {
			PGRPLIST_READLOCK(pg);
			PGRP_UNLOCK(pg);
			VPGRP_ORPHAN(vpg, 0, 0);
			PGRP_LOCK(pg);
			PGRPLIST_UNLOCK(pg);
		}
		PGRP_UNLOCK(pg);
		break;
	case -1:
		/* We're no longer a contributor and may orphan the group */
		PGRP_LOCK(pg);
		p_flagclr(p, SPGJCL);
		ASSERT(pg->pg_jclcnt > 0);
		if (--pg->pg_jclcnt == 0) {
			PGRPLIST_READLOCK(pg);
			PGRP_UNLOCK(pg);
			VPGRP_ORPHAN(vpg, 1, 0);
			PGRP_LOCK(pg);
			PGRPLIST_UNLOCK(pg);
		}
		PGRP_UNLOCK(pg);
		break;
	}
}

/*
 * Check for any stopped members locally.
 */
void
ppgrp_anystop(
	bhv_desc_t	*bdp,
	pid_t		exclude_pid,
	int		*is_stopped)
{
	pgrp_t	*pg = BHV_TO_PPGRP(bdp);
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	if (pg->pg_chain == NULL) {
		*is_stopped = 0;
		return;
	}

	PGRP_LOCK(pg);
	*is_stopped = anystop(pg, exclude_pid);
	PGRP_UNLOCK(pg);

}

/*
 * Send signal (at last!) to process, if not no-ctty.
 */
static int
sigpgrpf(
	proc_t	*p,
	long	arg)
{
	sig_arg_t	*argp = (sig_arg_t *) arg;
	int		error;

	if (!(p->p_flag & SNOCTTY)) {
		/* Hold to prevent reap */
		if (VPROC_HOLD_STATE(PROC_TO_VPROC(p), ZYES))
			return(0);		/* just missed it! */
		error = pproc_sendsig(&p->p_bhv, argp->sig, argp->opts,
				      argp->sid, argp->credp, argp->infop);	
		if (error)
			argp->error = error;
		VPROC_RELE(PROC_TO_VPROC(p));
	}
	return(0);
}

/* ARGSUSED */
static int
sendcont(
	proc_t	*p,
	long	arg)
{
	sig_arg_t	*argp = (sig_arg_t *) arg;
	int		error;

	/* Hold to prevent reap */
	if (VPROC_HOLD_STATE(PROC_TO_VPROC(p), ZYES))
		return(0);			/* missed it! */
	error = pproc_sendsig(&p->p_bhv, SIGHUP, SIG_ISKERN, 0, NULL, NULL);
	error = pproc_sendsig(&p->p_bhv, SIGCONT, SIG_ISKERN, 0, NULL, NULL);
	if (error)
		argp->error = error;
	VPROC_RELE(PROC_TO_VPROC(p));
	return(0);
}

/*
 * Signal members of a process group
 */
int
ppgrp_sendsig(
	bhv_desc_t	*bdp,
	int		sig,
	int		options,
	pid_t		sid,
	cred_t		*credp,
	k_siginfo_t	*infop)
{
	pgrp_t		*pg = BHV_TO_PPGRP(bdp);
	pgrpscanfunc_t	sigfunc;
	sig_arg_t	arg;

	ASSERT(pg);
	ASSERT(sig > 0);

	if (pg->pg_chain == NULL)
		return 0;

	/* Initialize pgroupscan function argument block */
	arg.sig   = sig;
	arg.opts  = options;
	arg.sid   = sid;
	arg.credp = credp;
	arg.infop = infop;
	arg.error = 0;

	/*
	 * Here we must set the hold-bit in the process group
	 * structure, so no process group members can exit,
	 * and no prospective members can join --
	 * release hold via pgrp_rele, below.
	 *
	 * This prevents the race condition between
	 * sigtoproc and waitsys, and others.
	 */
	PGRP_LOCK(pg);
	PGRPLIST_READLOCK(pg);
	PGRP_SIGSEQ_INC(pg);		/* increment signal sequence */
	PGRP_UNLOCK(pg);

	if (options & VPG_SENDCONT)
		sigfunc = (pgrpscanfunc_t) sendcont;
	else
		sigfunc = (pgrpscanfunc_t) sigpgrpf;

	/*
	 * Scan through members of the process group.
	 */
	pgroupscan(pg, sigfunc, (long) &arg);

	PGRP_LOCK(pg);
	ASSERT(PGRPLIST_IS_READLOCKED(pg));
	PGRPLIST_UNLOCK(pg);
	PGRP_UNLOCK(pg);

	return arg.error;
}

/*
 * This routine is not is the ops table. It is called by the
 * distribution layer to obtain physical state information,
 */ 
void
ppgrp_getstate(
	bhv_desc_t	*bdp,
	int		*nmembers,
	int		*is_orphaned)
{
	pgrp_t		*pg = BHV_TO_PPGRP(bdp);

	PGRP_LOCK(pg);
	*nmembers = pg->pg_memcnt;
	*is_orphaned = (pg->pg_jclcnt == 0);
	PGRP_UNLOCK(pg);
}

/* Communication structure between ppgrp_nice and pgrp_nice_f,
 * since pgroupscan allows only 1 argument.
 */
typedef struct {
	int	flags;			/* GET_NICE or SET_NICE */
	int	nice;			/* Nice value to set, or, for
					 * getpriority, the lowest nice
					 * value of the pgroup. */
	int	cnt;			/* number of procs found */
	cred_t	*scred;			/* Credentials of caller */
} pg_nice_s;

static int
pgrp_nice_f(
	proc_t		*p,
	long		arg)
{
	int error;
	int nice;
	pg_nice_s *ns = (pg_nice_s *)arg;

	if (ns->flags & VPG_GET_NICE) {
		error = proc_get_nice(p, &nice, ns->scred);
		if (error == 0) {
			if (nice < ns->nice)
				ns->nice = nice;
			ns->cnt++;
		}
		return error;
	}

	/* Set priority
	 * proc_set_nice rewrites the input nice value with the
	 * old nice value - hence we pass a temp, which has to be
	 * reinitialized for each proc_set_nice call.
	 */
	nice = ns->nice;

	/* XXX sfc - this is now wrong - need to iterate over
	 * all cells with threads for this proc to set nice.
	 */
	error = proc_set_nice(p, &nice, ns->scred, 0);
	if (error == 0)
		ns->cnt++;
	return error;
}

/* Support for the BSD setpriority/getpriority system calls */
int
ppgrp_nice(
	bhv_desc_t	*bdp,
	int		flags,
	int		*nice,
	int		*count,
	cred_t		*scred)
{
	pgrp_t		*pg = BHV_TO_PPGRP(bdp);
	pg_nice_s	nice_s;
	int		error;

	ASSERT( (flags & (VPG_GET_NICE|VPG_SET_NICE)) == VPG_GET_NICE || \
		(flags & (VPG_GET_NICE|VPG_SET_NICE)) == VPG_SET_NICE);

	nice_s.flags = flags;
	nice_s.cnt = 0;
	nice_s.scred = scred;

	/* getpriority returns the highest priority (lowest numerical
	 * nice	value) enjoyed by any of the specified processes.
	 * To implement this:
	 *	initialize 'nice' to MAXINT - pgrp_nice_f will keep track
	 *	of highest value, and return that in nice.
	 */
	if (flags & VPG_GET_NICE)
		nice_s.nice = MAXINT;
	else
		nice_s.nice = *nice;

	/* prevent our proc list from changing while we scan */
	PGRP_LOCK(pg);
	PGRPLIST_READLOCK(pg);
	PGRP_UNLOCK(pg);

	error = pgroupscan(pg, pgrp_nice_f, (long)&nice_s);

	PGRP_LOCK(pg);
	PGRPLIST_UNLOCK(pg);
	PGRP_UNLOCK(pg);

	if (error)
		return error;

	if (flags & VPG_GET_NICE)
		/* No error - return 'best' priority */
		*nice = nice_s.nice;
	
	/* Number of processes found - so caller can determine if ESRCH. */
	*count = nice_s.cnt;

	/* No errors */
	return 0;

}

int
ppgrp_setbatch(
	bhv_desc_t	*bdp)
{
	pgrp_t 		*pg = BHV_TO_PPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	/* Last minute check that we're the pgrp leader alone in our pgrp */
	if (!(pg->pg_memcnt == 1 && vpg->vpg_pgid == current_pid()))
		return EPERM;
	pg->pg_batch = 1;
	return 0;
}

int
ppgrp_clearbatch(
	bhv_desc_t 	*bdp)
{
	pgrp_t          *pg = BHV_TO_PPGRP(bdp);

        pg->pg_batch = 0;
	return 0;
}
