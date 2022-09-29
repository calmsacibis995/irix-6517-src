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

#ident "$Revision: 1.37 $"

#include <sys/types.h>
#include <sys/acct.h>
#include <ksys/cred.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#include <sys/sat.h>
#include <ksys/vsession.h>
#include <ksys/vpgrp.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */

#include "pproc_private.h"

/* Service routine for BSDsetpgrp and setpgid */
int
pproc_setpgid(
	bhv_desc_t *bhv,
	pid_t pgid,
	pid_t callers_pid,
	pid_t callers_sid)
{
	proc_t *p;
	int error = 0;
	vpgrp_t *vpg_old;
	vpgrp_t *vpg_new;
	int	is_batch;

	p = BHV_TO_PROC(bhv);
	if (p == NULL)
		return(ESRCH);

	mrupdate(&p->p_who);

	/* setpgid operates only on the caller, or on a direct child
	 * of the caller. Otherwise return ESRCH.
	 */
	if (p->p_pid != callers_pid && p->p_ppid != callers_pid) {
		error = ESRCH;
		goto out;
	}

	if (pgid == 0)	/* use pid of specified process */
		pgid = p->p_pid;

	/* If the caller is operating on a child rather than itself */
	if (p->p_pid != callers_pid) {
		/* Error if child process has exec'd */
		if (p->p_flag & SEXECED) {
			error = EACCES;
			goto out;
		}

		/* Error of child process not in same session as caller. */
		if (p->p_sid != callers_sid) {
			error = EPERM;
			goto out;
		}
	}
	/* session leaders (pid == sid) may not change their pgrp */
	if (p->p_pid == p->p_sid) {
		error = EPERM;
		goto out;
	}
	
	ASSERT(p->p_vpgrp);
	vpg_old = p->p_vpgrp;
	VPGRP_GETATTR(vpg_old, NULL, NULL, &is_batch);
	if (is_batch) {
		error = EPERM;
		goto out;
	}	

	if (p->p_pgid == pgid) {
		/* already a member - easy */
		goto out;
	}

	/*
	 * if not trying to set pgrp = to pid of specified process, see
	 * if a proc exists with p_pgid == pgid which is in our session.
	 */
	if (p->p_pid == pgid) {
		vpg_new = VPGRP_CREATE(pgid, p->p_sid);
	} else {
		pid_t	sid;
		int	is_batch;
		vpg_new = VPGRP_LOOKUP(pgid);
		if (vpg_new == NULL) {
			error = EPERM;
			goto out;
		}
		VPGRP_GETATTR(vpg_new, &sid, NULL, &is_batch);
		if (sid != p->p_sid || is_batch) {
			error = EPERM;
			VPGRP_RELE(vpg_new);
			goto out;
		}
	}

	/*
	 * Moving between pgrps involves 3 steps:
	 * - commit to joining a group (VPGRP_JOIN_BEGIN);
	 * - leave the old group (VPGRP_LEAVE); and
	 * - join the new (VPGRP_JOIN_END).
	 * The first step may fail if the existing pgrp is empty
	 * and about to be torn-down. However, if the commit succeeds,
	 * the pgrp won't be destroyed and the join is guaranteed.
	 */
	error = VPGRP_JOIN_BEGIN(vpg_new);
	if (error) {
		error = EPERM;
		VPGRP_RELE(vpg_new);
		goto out;
		/*
		 * XXX - there's a vanishingly small (but non-zero)
		 * chance that we're a pgrp leader rejoining our
		 * own pgrp at a time when the last member is
		 * leaving. We shouldn't return an error here.
		 */
	}

	VPGRP_HOLD(vpg_old);		/* Hold old vpgrp for now */
	VPGRP_LEAVE(vpg_old, p, 0);
	p->p_vpgrp = NULL;
	p->p_pgid = -1;
	p->p_sid = -1;

	VPGRP_JOIN_END(vpg_new, p, 1);
	VPGRP_RELE(vpg_old);		/* Release old vpgrp (it and vsession)
					 * may now vanish */
	p->p_vpgrp = vpg_new;
	p->p_pgid = pgid;
	VPGRP_GETATTR(vpg_new, &p->p_sid, NULL, NULL);
	VPGRP_RELE(vpg_new);
out:
	mrunlock(&p->p_who);

	_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE,callers_pid,p->p_cred,error,-1);

	return error;
}

/*
 * Leave the old session and old process group and make a new
 * session and a new process group. Called by setsid.
 */
static int
setpg(proc_t *p)
{
	vsession_t	*vsp;
	vpgrp_t		*vpg_old;
	vpgrp_t		*vpg_new;
	int		is_batch;

	ASSERT(ismrlocked(&p->p_who, MR_UPDATE));
	
	/*
	 * Locking is a little funny here.  The p_who mrlock protects
	 * others (other processes or other threads within the process)
	 * from changing p_vpgrp.  But holding p_lock gives any process
	 * the right to dereference p_vpgrp just to query the structure.
	 * Long-term operations against the entire process group are allowed
	 * within pgrp_hold()/pgrp_rele() -- pgrp_hold blocks
	 * further insertions/deletions to/from the process group.
	 *
	 * Note that p_vsession is only changed by the process itself,
	 */

	ASSERT(p->p_vpgrp);
	VPGRP_GETATTR(p->p_vpgrp, NULL, NULL, &is_batch);
	if (is_batch) {
		return EPERM;
	}
			
	vsp = VSESSION_CREATE(p->p_pid);
	ASSERT(vsp);
	vpg_new = VPGRP_CREATE(p->p_pid, vsp->vs_sid);
	ASSERT(vpg_new);
	vpg_old = p->p_vpgrp;

#ifdef DEBUG
	ASSERT( VPGRP_JOIN_BEGIN(vpg_new) == 0 );
#else
	VPGRP_JOIN_BEGIN(vpg_new);
#endif
	VPGRP_HOLD(vpg_old);
	VPGRP_LEAVE(vpg_old, p, 0);
	p->p_vpgrp = NULL;
	p->p_pgid = -1;
	p->p_sid = -1;
	p_flagclr(p, SNOCTTY);

	VPGRP_JOIN_END(vpg_new, p, 0);
	VPGRP_RELE(vpg_new);
	VPGRP_RELE(vpg_old);
	p->p_vpgrp = vpg_new;
	p->p_pgid = p->p_pid;

	p->p_sid = vsp->vs_sid;
	VSESSION_RELE(vsp);

	return 0; 
}

/* pproc_setsid implements Posix setsid */
int
pproc_setsid(
	bhv_desc_t	*bhv,
	pid_t		*new_pgid)
{
	proc_t		*p;
	vpgrp_t		*vpg;
	int 		error;
	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* process group leaders may NOT execute setsid() */
	if (p->p_pid == p->p_pgid) {
		_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE, p->p_pid, p->p_cred,
		    EPERM, -1);
		return EPERM;
	}

	/*
	 * mrlock p_who protects p's p_vsession and p_pgroup pointers
	 */
	mrupdate(&p->p_who);

	/* check for duplicate pgrp amongst active processes */
	vpg = VPGRP_LOOKUP(p->p_pid);
	if (vpg != NULL) {
		VPGRP_RELE(vpg);
		mrunlock(&p->p_who);
		_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE, p->p_pid,
			 p->p_cred, EPERM, -1);
                return EPERM;
        }  
	/*
	 * Now make this process a session leader and group leader.
	 * Sets rvp->r_val1 to new process group (== sid).
	 * Also, since procp->p_pid != procp->p_pgid on entry,
	 * relinquishes controlling tty.
	 */
	error = setpg(p);
	mrunlock(&p->p_who);

	*new_pgid = p->p_pgid;

	_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE, p->p_pid, p->p_cred, 0, -1);
	return error;
}

/* pproc_setpgrp implements SysV setpgrp, and a special case
 * of BSDsetpgrp
 */
int
pproc_setpgrp(
	bhv_desc_t	*bhv,
	int		flags,
	pid_t		*new_pgid)
{
	proc_t		*p;
	sigvec_t	*sigvp;
	int 		error;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* If this process is already a session leader, then don't
	 * allow this call - setpg() will happily create a new session
	 * for this process. If this process's current session (of
	 * which it is the leader) has more than just itself as a
	 * member, then, after the call to setpg(), there would be 2
	 * sessions with this sid - which makes no sense, and causes
	 * havoc with the pid table code, if nothing else.
	 *
	 * Similarly we do not allow this call for a process group
	 * leader.  Otherwise, we could end up with the various
	 * members of a process group being in different sessions.
	 * This also does not make sense.
	 */
	mrupdate(&p->p_who);
	if (p->p_sid == p->p_pid || p->p_pgid == p->p_pid) {
		mrunlock(&p->p_who);
		*new_pgid = p->p_pgid;
		return 0;
	}

	error = setpg(p);
	mrunlock(&p->p_who);

	if (flags & VSPGRP_SYSV) {
		/* set bit SNOCLDSTOP */
		VPROC_GET_SIGVEC(PROC_TO_VPROC(p), VSIGVEC_UPDATE, sigvp);
		sigvp->sv_flags |= SNOCLDSTOP;
		VPROC_PUT_SIGVEC(PROC_TO_VPROC(p));
	}

	*new_pgid = p->p_pgid;

	_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE, p->p_pid, p->p_cred, 0, -1);
	return error;
}

/*
 * Parent has changed pgid/sid, check pgrp orphan status of child.
 */
void
pproc_pgrp_linkage(
	bhv_desc_t	*bhv,
	pid_t		parent_pgid,
	pid_t		parent_sid)
{
	proc_t		*p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* Let the vpgrp do the determination */
	mrupdate(&p->p_who);
	VPGRP_LINKAGE(p->p_vpgrp, p, parent_pgid, parent_sid);
	mrunlock(&p->p_who);
}

/* pproc_setuid implements setuid (sysV) and setreuid(BSD) */
int
pproc_setuid(
	bhv_desc_t *bhv,
	uid_t ruid,
	uid_t euid,
	int flags)
{
	proc_t *p;
	uid_t old_ruid;
	cred_t *cr;
	int ispriv;

	p = BHV_TO_PROC(bhv);
	if (p == NULL)
		return(ESRCH);

	/*
	 * Hold pcred_lock while changing cred pointer.  This synchronizes
	 * with other processes who might be inspecting our ID's and
	 * makes sure that they'll either see our old or new credentials
	 * atomically.  After that the user ID's are safe to change at
	 * will because the process only changes these values itself.
	 * If somebody was scanning on these ID's (say kill, for instance)
	 * the net effect is the same as normal UNIX: depending on who gets
	 * there first he may or may not find him.
	 */

	/*
	 * Save the uid information in the audit record,
	 * should that be appropriate. This will save real, effective, and
	 * saved uids.
	 */
	_SAT_SAVE_ATTR(SAT_UID_LIST_TOKEN, curuthread);

	cr = pcred_lock(p);
	old_ruid = cr->cr_ruid;

	if (flags & VSUID_SYSV) {
		/*
		 * setuid semantics: change both real and effective uids.
		 */
		cr = crcopy(p);
		ispriv = cap_able_cred(cr, CAP_SETUID);

		if (euid != cr->cr_ruid && euid != cr->cr_suid && !ispriv) {
			pcred_unlock(p);
			_SAT_SETREUID(euid, euid, EPERM);
			return EPERM;
		}

		/* always ok to set this */
		cr->cr_uid = euid;

		if (ispriv) {
			cr->cr_suid = euid;
			cr->cr_ruid = euid;
#ifdef _SHAREII
			/*
			 * set ruid to euid for the upcoming call to
			 * _SAT_SETREUID()
			 */
			ruid = euid;
			/*
			 *	Where the real uid is changing ShareII needs
			 *	change the lnode unless setuid suppression
			 *	was requested.
			 */	
			SHR_SETRUID(euid);
#endif /* _SHAREII */
		} else {
			ruid = -1;
#ifdef _SHAREII
			/*
			 *	If we are only changing the euid, then we
			 *	call SHseteuid which currently only clears
			 *	the setuid suppression bit.
			 */
			SHR_SETEUID(euid);
#endif /* _SHAREII */
		}
	} else if (flags & VSUID_CKPT) {
		/*
		 * just wants to set suid
		 */
		if (!cap_able_cred(cr, CAP_SETUID)) {
			pcred_unlock(p);
			_SAT_SETREUID(ruid, euid, EPERM);
			return EPERM;
		}
		cr->cr_suid = ruid;
	} else {
		/*
		 * 4.3BSD setreuid semantics: can set real and effective uids
		 * separately.
		 *
		 * If super-user, then can set the real and effective uids to
		 * any value. Otherwise, the caller can only swap ruid and euid
		 * or make both the same.
		 */
		if ((cr->cr_ruid != ruid && cr->cr_uid != ruid ||
		     cr->cr_ruid != euid && cr->cr_uid != euid &&
		     cr->cr_suid != euid)
		    && !cap_able_cred(cr, CAP_SETUID)) {
			pcred_unlock(p);
			_SAT_SETREUID(ruid, euid, EPERM);
			return EPERM;
		}

		cr = crcopy(p);

		cr->cr_uid  = euid;
		cr->cr_ruid = ruid;
#ifdef _SHAREII
		/*
		 *	Where the real uid is changing ShareII needs
		 *	change the lnode unless setuid suppression
		 *	was requested.
		 *	If the real uid is the same as the old,
		 *	this merely clears the setuid suppression bit.
		 */
		SHR_SETRUID(ruid);
#endif /* _SHAREII */
	}

	/*
	 * if no errors, then update all share group processes except p
	 * XXX doesn't work for setreuid ...
	 */
	if (IS_SPROC(&p->p_proxy) && (p->p_proxy.prxy_shmask & PR_SID)) {
		shaddr_t *sa = p->p_shaddr;
		int s;

		s = mutex_spinlock(&sa->s_rupdlock);
		crfree(sa->s_cred);
		sa->s_cred = cr;
		crhold(cr);
		setshdsync(sa, p, PR_SID, UT_UPDUID);
		mutex_spinunlock(&sa->s_rupdlock, s);
	}

	/* see comment in exec code */
	if (cap_able_any(cr))
		p->p_acflag |= ASU;

	/*
	 * Inform all uthreads attached to this process that they need
	 * to get updated versions of process credentials, and unlock.
	 */
	pcred_push(p);

	/*
	 * Audit with the pcred unlocked as cttydev() respects the lock
	 */
	_SAT_SETREUID(ruid, euid, 0);

	uidact_switch(old_ruid, cr->cr_ruid);

	return 0;
}


/* pproc_setgid implements setgid (sysV) and setregid(BSD) */

int
pproc_setgid(
	bhv_desc_t *bhv,
	gid_t rgid,
	gid_t egid,
	int flags)
{
	proc_t *p;
	cred_t *cr;
	int ispriv;

	p = BHV_TO_PROC(bhv);
	if (p == NULL)
		return(ESRCH);

	/*
	 * Hold p_lock while changing cred pointer.  This synchronizes
	 * with other processes who might be inspecting our ID's and
	 * makes sure that they'll either see our old or new credentials
	 * atomically.  After that the group ID's are safe to change at
	 * will because the process only changes and tests these values itself.
	 * Other processes only look our effective and saved user ID, never
	 * at our group ID.
	 */

	/*
	 * Save the uid information in the audit record,
	 * should that be appropriate. This will save [re]gid.
	 */
	_SAT_SAVE_ATTR(SAT_GID_LIST_TOKEN, curuthread);

	cr = pcred_lock(p);

	if (flags & VSUID_SYSV) {
		/*
		 * setgid semantics: change both real and effective gids.
		 */
		cr = crcopy(p);
		ispriv = cap_able_cred(cr, CAP_SETGID);

		if (egid != cr->cr_rgid && egid != cr->cr_sgid && !ispriv) {
			pcred_unlock(p);
			_SAT_SETREGID(egid, egid, EPERM);
			return EPERM;
		}

		/* always ok to set this */
		cr->cr_gid = egid;

		/* set rgid for terminal auditing */
		if (ispriv) {
			cr->cr_sgid = egid;
			cr->cr_rgid = egid;
			rgid = egid;
		} else {
			rgid = -1;
		}
	} else {
		/*
		 * 4.3BSD setregid semantics: can set real and effective gids
		 * separately.
		 *
		 * If super-user, then can set the real and effective gids to
		 * any value. Otherwise, the caller can set the real gid
		 * to the effective gid, or set the effective gid to the
		 * real gid, or to the saved-set-gid value.
		 *
		 * XPG4 semantics - just slightly different: If not
		 * priveleged, the caller can set the real gid to
		 * the saved-set-gid or can set the effective gid to
		 * either the real gid or to the saved-set-gid.
		 */

		if (flags & VSUID_XPG4) {
			if ((cr->cr_rgid != rgid && cr->cr_sgid != rgid ||
			     cr->cr_gid != egid && cr->cr_rgid != egid &&
			     cr->cr_sgid != egid)
			    && !cap_able_cred(cr, CAP_SETGID)) {
				pcred_unlock(p);
				_SAT_SETREGID(rgid, egid, EPERM);
				return EPERM;
			}
		} else {
			if ((cr->cr_rgid != rgid && cr->cr_gid != rgid ||
			     cr->cr_rgid != egid && cr->cr_gid != egid &&
			     cr->cr_sgid != egid)
			    && !cap_able_cred(cr, CAP_SETGID)) {
				pcred_unlock(p);
				_SAT_SETREGID(rgid, egid, EPERM);
				return EPERM;
			}
		}

		cr = crcopy(p);

		cr->cr_gid = egid;
		cr->cr_rgid = rgid;
	}

	/* if no errors, then update all share group processes (except p) */
	if (IS_SPROC(&p->p_proxy) && (p->p_proxy.prxy_shmask & PR_SID)) {
		shaddr_t *sa = p->p_shaddr;
		int s;

		s = mutex_spinlock(&sa->s_rupdlock);
		crfree(sa->s_cred);
		sa->s_cred = cr;
		crhold(cr);
		setshdsync(sa, p, PR_SID, UT_UPDUID);
		mutex_spinunlock(&sa->s_rupdlock, s);
	}

	/*
	 * Inform all uthreads attached to this process that they need
	 * to get updated versions of process credentials, and unlock.
	 */
	pcred_push(p);
	/*
	 * Audit with the pcred unlocked as cttydev() respects the lock
	 */
	_SAT_SETREGID(rgid, egid, 0);
	return 0;
}

/*
 * establish groups
 * A separate function so AFS can call
 */
void
estgroups(proc_t *p, cred_t *newcr)
{
	cred_t *cr;

	/*
	 * lock pcred_lock to make sure that anyone else who might be
	 * looking at our current credentials is done before we
	 * replace them with the new ones.
	 */

	_SAT_SETGROUPS(newcr->cr_ngroups, newcr->cr_groups, 0);

	cr = pcred_lock(p);
	p->p_cred = newcr;

	/* update all share group processes (except p) */
	if (IS_SPROC(&p->p_proxy) && (p->p_proxy.prxy_shmask & PR_SID)) {
		shaddr_t *sa = p->p_shaddr;
		int s;

		s = mutex_spinlock(&sa->s_rupdlock);
		crfree(sa->s_cred);
		sa->s_cred = newcr;
		crhold(newcr);
		setshdsync(sa, p, PR_SID, UT_UPDUID);
		mutex_spinunlock(&sa->s_rupdlock, s);
	}

	pcred_push(p);

	crfree(cr);
}

void
pproc_setgroups(
	bhv_desc_t *bhv,
	int setsize,
	gid_t *newgids)
{
	proc_t *p;
	cred_t *cr;
	cred_t *newcr;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/*
	 * Generate the audit record BEFORE the credential is locked.
	 * This avoids a locking problem in cttydev()
	 */
	_SAT_SETGROUPS(setsize, newgids, 0);

	cr = pcred_lock(p);
	newcr = crdup(cr);

	bcopy(newgids, newcr->cr_groups, setsize * sizeof(gid_t));
	newcr->cr_ngroups = setsize;

	p->p_cred = newcr;

	/* update all share group processes */
	if (IS_SPROC(&p->p_proxy) && (p->p_proxy.prxy_shmask & PR_SID)) {
		shaddr_t *sa = p->p_shaddr;
		int s;

		s = mutex_spinlock(&sa->s_rupdlock);
		crfree(sa->s_cred);
		sa->s_cred = newcr;
		crhold(newcr);
		setshdsync(sa, p, PR_SID, UT_UPDUID);
		mutex_spinunlock(&sa->s_rupdlock, s);
	}

	pcred_push(p);

	crfree(cr);
}
