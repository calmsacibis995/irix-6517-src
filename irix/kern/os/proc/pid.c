/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1998 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.55 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <ksys/pid.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/var.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/cmn_err.h>
#include "pproc.h"
#include "pid_private.h"
#include "pproc_private.h"
#include "vproc_private.h"
#include "proc_trace.h"

/* Currently, the pid table is organized as follows: pidtab
 * is a static array allocated of size 'pidtabsz' entries. Each
 * element represents a non-contiguous set of pids - e.g.
 * element 0 maps pid 0, pidtabsz, 2*pidtabsz... Element 1 maps
 * pids 1, pidtabsz+1, 2*pidtabsz+1...
 *
 * Pidtab slots that are not in use are chained on a
 * free list. So in the normal case, pid allocation is quick -
 * just grab the first element from the free list - the field
 * pe_pid already has the next pid to use - and return that.
 * All that needs to happen is to allocate a pid entry to describe
 * the use of the pid. There is normally only one entry chained
 * to a slot with a process in it.
 *
 * This is somewhat gobbed up by checkpoint/restart, which may
 * request a pid that is free, but the pid slod that that pid
 * maps to may be busy. Hence, we chain additional proc
 * entries off of the pidtab slot.
 *
 * Pid slots may have entries on their chains even when on the
 * free list. The free list only indicates that there are no
 * pid entries chained that have active procs in them. All 
 * pid entries with active procs are chained together on the
 * pidactive list.
 *
 *
 * Locking is a little tricky.  There is a single free-list lock
 * [pidlock], which shouldn't be a bottleneck because it is held
 * only while taking a pid_slot_t entry off the doubly-linked queue
 * on putting an entry on the queue.
 * Each pid_slot has its own lock which protects the pid-entry queue
 * and all pid-entry data [except the pid-entry active queue], and
 * all data in the pid_slot with one exception: if the pid_slot is on
 * the free list (!pidslot_isactive()), the free list protects the
 * pid_slot forw/back indices.  When not on the free list (when active),
 * the pid_slot lock protects the active/busycnt fields which are a
 * union in the same memory as forw/back, and both the pid_slot lock
 * and pidtab_lock must be held to put a pid_slot back on the free list
 * [transition from active to free].
 *
 * The order is always pid_slot lock then pidtab_lock.
 *
 * Active pid-entries [those which have been pid_associate()d] are
 * put on a pid_active_list queue, strictly for the benefit of procscan.
 * The lock order is always pid_active_lock then pid_slot_lock.  To
 * maintain this order, pid_associate sets the pe_vproc field with the
 * pid_slot lock held, then releases the lock, acquires the pid_active
 * lock and queues it on the active list.  When de-activating the entry,
 * it would be logically simplest to dequeue the entry then null pe_vproc,
 * but since this isn't convenient, procscan must be careful to check
 * pe_vproc [with pid_slot lock held] to confirm that every queued
 * entry is really active.
 *
 * For reasons that should be obvious,
 * the order is pidactive_lock then pidslot_lock.
 */

pid_t		pid_wrap;
pid_t		pid_base;
pid_slot_t *	pidtab;
int		pidtabsz;
int		pidfirstfree;
int		pidlastfree;
lock_t		pidlock;		/* pid_slot_t free list */
sv_t		pid_dealloc_sv;

pid_active_t	pid_active_list;

static struct zone *pident_zone;

#define PID_INCR(pid)	((((pid)-pid_base+pidtabsz)%pid_wrap)+pid_base)

void
pidtab_init()
{
	int		i;
	pid_slot_t	*ps;

	pidtab_lock_init();
	spinlock_init(&pid_active_list.pa_lock, "pidactive");
	kqueue_init(&pid_active_list.pa_queue);
	sv_init(&pid_dealloc_sv, SV_DEFAULT, "pid_dealloc");

	ASSERT(pidtabsz + pid_base <= MAXPID);

	pidtab = kmem_alloc(pidtabsz * sizeof(pid_slot_t), KM_SLEEP);

	/*
	 * create the free list
	 */
	pidfirstfree = 0;

	for (ps = pidtab, i = 0; i < pidtabsz; i++, ps++) {
		ps->ps_pid = i + pid_base;
		ps->ps_forw = i + 1;
		ps->ps_back = i - 1;
		ps->ps_chain = 0;
		pidslot_lock_init(ps, i);
	}

	pidlastfree = pidtabsz - 1;
	pidtab[pidlastfree].ps_forw = -1;		/* End of list */

	pident_zone = kmem_zone_init(sizeof(pid_entry_t), "pid entry");
}

/*
 * Allocate a new pid - the requested argument is used
 * only by checkpoint/restart - to restart a process with
 * a particular pid.
 */
/* ARGSUSED */
int
pid_alloc(
	pid_t	requested,
	pid_t	*newpidp)
{
	pid_slot_t	*ps;
	pid_entry_t	*pe;
	pid_entry_t	*newpe;
	int		spl;

	newpe = kmem_zone_zalloc(pident_zone, KM_SLEEP);
#ifdef why_bother
	/* kmem_zone_zalloc sets all this to null */
	newpe->pe_dealloc = 0;
	newpe->pe_nodealloc = 0;
	newpe->pe_busy = 0;	/* zeros sess, pgrp, batch and proc */
#endif
	newpe->pe_proc = 1;
	newpe->pe_pid = requested;

#ifdef CKPT
	if (requested) {
		/* sanity-check our inputs
		 * Same errno that ckpt-restart uses, although EINVAL
		 * would seem to be a better choice.
		 */
		if (requested <= 0 || requested >= MAXPID) {
			kmem_zone_free(pident_zone, newpe);
			return ESRCH;
		}

		ps = PID_SLOT(requested);
		spl = pidslot_lock(ps);

		/*
		 * see if the requested pid is in use
		 */
		for (pe = ps->ps_chain; pe; pe = pe->pe_next) {
			if (pe->pe_pid == requested) {
				pidslot_unlock(ps, spl);
				kmem_zone_free(pident_zone, newpe);
				return(EBUSY);
			}
		}

		/*
		 * if the next pid to recycle is the requested one,
		 * increment the recyled pid.
		 */
		if (ps->ps_pid == requested)
			ps->ps_pid = PID_INCR(ps->ps_pid);

		/*
		 * take the slot off the free list if it is on it
		 */
		pidtab_nested_lock();
		if (!pidslot_isactive(ps)) {
			if (ps->ps_back == -1)
				pidfirstfree = ps->ps_forw;
			else
				pidtab[ps->ps_back].ps_forw = ps->ps_forw;
			if (ps->ps_forw == -1)
				pidlastfree = ps->ps_back;
			else
				pidtab[ps->ps_forw].ps_back = ps->ps_back;
			pidslot_setactive(ps);
		} else
			ps->ps_busycnt++;
		pidtab_nested_unlock();
	} else
#endif	/* CKPT */
	{
		spl = pidtab_lock();

		if (pidfirstfree == -1) {
			/*
			 * no free slots
			 */
			pidtab_unlock(spl);
			kmem_zone_free(pident_zone, newpe);
			return(EAGAIN);
		}

		/*
		 * Take the first free slot of the free list
		 */
		ps = &pidtab[pidfirstfree];
		pidfirstfree = ps->ps_forw;

		if (pidfirstfree == -1)
			pidlastfree = -1;
		else
			pidtab[pidfirstfree].ps_back = -1;

		/*
		 * Must set active while holding pidtab_lock so a
		 * concurrent pid_alloc(requested) can't get confused.
		 */
		pidslot_setactive(ps);
		pidtab_unlock(spl);

		spl = pidslot_lock(ps);

		/*
		 * check that the pid in the slot is not being used
		 * for anything else so we can recyle it
		 */
recheck:
		for (pe = ps->ps_chain; pe; pe = pe->pe_next) {
			if (ps->ps_pid == pe->pe_pid) {
				ps->ps_pid = PID_INCR(ps->ps_pid);
				goto recheck;
			}
		}

		newpe->pe_pid = ps->ps_pid;
	}
	
	/*
	 * Add the new pid entry to the front of the slot chain
	 */
	newpe->pe_next = ps->ps_chain;
	ps->ps_chain = newpe;
	newpe->pe_vproc = VPROC_NULL;

	pidslot_unlock(ps, spl);

	*newpidp = newpe->pe_pid;
	return 0;
}

/* Associate vproc struct pointer 'vp' with pid */
void
pid_associate(
	pid_t	pid,
	vproc_t	*vp)
{
	pid_entry_t	*pe;
	pid_slot_t	*ps;
	int		spl;

	ps = PID_SLOT(pid);
	spl = pidslot_lock(ps);

	pe = ps->ps_chain;

	while (pe) {
		if (pe->pe_pid == pid) {
			/*
			 * We found the right entry. Make sure that
			 * it is not used by another proc, make it
			 * ours and add it to the active pid list
			 */
			ASSERT(pe->pe_proc);
			ASSERT(pe->pe_vproc == VPROC_NULL);
			pe->pe_vproc = vp;
			pidslot_nested_unlock(ps);
			pidactive_nested_lock();
			kqueue_enter_last(&pid_active_list.pa_queue,
						&pe->pe_queue);
			pidactive_unlock(spl);
			PROC_TRACE4(PROC_TRACE_EXISTENCE,
					"pid_associate", pid, "vproc", vp);
			return;
		}
		pe = pe->pe_next;
	}

	pidslot_unlock(ps, spl);

	/* Didn't find pid on list - this should never happen */
	panic("pid_associate did not find pid in pidtab");
}

/*
 * Apply 'op' to the pid entry 'pe'.
 * Sets the pe_busy field as a side-effect.
 */
static int
apply_op(
	pid_slot_t	*ps,
	pid_entry_t	*pe,
	int		op,
	int		*spl)
{
	switch (op) {
	case P_PROC_FREE:
		/*
		 * If procscanners are looking at this pid entry,
		 * wait for them to all to finish
		 */
		while (pe->pe_nodealloc) {
			pe->pe_dealloc = 1;
			pidslot_wait(ps, *spl);
			*spl = pidslot_lock(ps);
		}

		ASSERT(pidslot_isactive(ps));
		ASSERT(ps->ps_busycnt > 0);
		ps->ps_busycnt--;	/* one fewer process in this slot */

		/*
		 * The pid tab lock will stop anyone looking while we
		 * remove this entry
		 */
		ASSERT(pe->pe_proc);
		pe->pe_proc = 0;

		/*
		 * pe_vproc may be null if the process hasn't been
		 * instantiated yet...
		 */
		if (pe->pe_vproc != VPROC_NULL) {
			pe->pe_vproc = VPROC_NULL;
			return 1;
		}

		break;

	case P_PGRP_JOIN:
		pe->pe_pgrp = 1;
		break;
	case P_PGRP_LEAVE:
		ASSERT(pe->pe_pgrp);
		pe->pe_pgrp = 0;
		break;

	case P_SESS_JOIN:
		pe->pe_sess = 1;
		break;
	case P_SESS_LEAVE:
		ASSERT(pe->pe_sess);
		pe->pe_sess = 0;
		break;

	case P_BATCH_JOIN:
		pe->pe_batch = 1;
		break;
	case P_BATCH_LEAVE:
		ASSERT(pe->pe_batch);
		pe->pe_batch = 0;
		break;

	default:
		panic("apply_op: bad op");
		/* NOTREACHED */
	}

	return 0;
}

/* Perform 'op' on pid - either remove proc/pid association, or remove
 * pid as pgrp or session leader. In any case, 'op' may result in
 * freeing the pid.
 */
void
pid_op(
	int	op,
	pid_t	pid)
{
	pid_entry_t	*pe;
	pid_slot_t	*ps;
	pid_entry_t	**pepp;
	int		spl, dq, free = 0;

	ps = PID_SLOT(pid);

	spl = pidslot_lock(ps);

	pepp = &ps->ps_chain;
	for (pe = ps->ps_chain; pe; pe = pe->pe_next) {
		if (pe->pe_pid != pid) {
			pepp = &pe->pe_next;
			continue;
		}

		dq = apply_op(ps, pe, op, &spl);

		/*
		 * If the entry isn't being used any more, take it off the
		 * slot chain
		 */
		if (!pe->pe_busy) {
			free = 1;
			*pepp = pe->pe_next;
		}

		/*
		 * If the slot has no active process in it, put it on
		 * the free list
		 */
		if (pidslot_isactive(ps) && ps->ps_busycnt == 0) {
			int	indx = PID_INDEX(ps->ps_pid);
			/*
			 * No need to recheck pidslot_isactive, etc. --
			 * the only place it can change from 'active'
			 * to free is here.
			 */
			pidtab_nested_lock();
			ps->ps_pid = PID_INCR(pid);
			ps->ps_back = pidlastfree;
			ps->ps_forw = -1;
			if (pidlastfree == -1) {
				ASSERT(pidfirstfree == -1);
				pidfirstfree = indx;
			} else
				pidtab[pidlastfree].ps_forw = indx;
			pidlastfree = indx;
			pidtab_nested_unlock();
		}
		pidslot_unlock(ps, spl);

		if (dq) {
			spl = pidactive_lock();
			kqueue_remove(&pe->pe_queue);
			pidactive_unlock(spl);
		}

		/*
		 * If the entry was not being used, we free it here after
		 * we drop the pid lock(s).
		 */
		if (free)
			kmem_zone_free(pident_zone, pe);

		return;
	}

	panic("pid_op: can't find pid");
}

/*
 * Get a reference on a process.
 * WARNING - if you ask for zombies, one still gets locked but one
 * must be aware that the process could have started exitting (set SEXIT)
 * which means it won't be prevented from exitting. 
 */
vproc_t *
pid_to_vproc(
	pid_t	pid,
	int	flags)
{
	vproc_t		*vp;
	pid_slot_t	*ps;
	pid_entry_t	*pe;
	int		spl;

	if (pid < 0 || pid > MAXPID)
		return 0;

	ps = PID_SLOT(pid);
	spl = pidslot_lock(ps);

	if (!pidslot_isactive(ps)) {
		pidslot_unlock(ps, spl);
		return(VPROC_NULL);
	}

	for (pe = ps->ps_chain; pe; pe = pe->pe_next) {
		if (pe->pe_pid == pid) {
			int	held;

			if ((vp = pe->pe_vproc) == VPROC_NULL) {
				/*
				 * The pid happens to be in the pid table,
				 * but no proc associated with it. No need
				 * to look further.
				 */
				break;
			}

			VPROC_NESTED_REFCNT_LOCK(vp);
			pidslot_nested_unlock(ps);

			held = vproc_reference(vp, flags);

			VPROC_REFCNT_UNLOCK(vp, spl);

			if (held) {
				PROC_TRACE10(PROC_TRACE_REFERENCE,
				      "pid_to_vproc", vp->vp_pid,
				      "by", current_pid(),
				      "refcnt", vp->vp_refcnt,
				      "\n\tcalled from", __return_address,
				      "flag", flags);
				return(vp);
			} else
				return(VPROC_NULL);
		}
	}

	pidslot_unlock(ps, spl);
	return(VPROC_NULL);
}

/* Interface for procfs readdir */
int
pid_getlist_local(
        pidord_t	start,
        size_t		*count,
        pid_t		*pidbuf,
        pidord_t	*ordbuf)
{
        size_t 		sofar = 0;
        size_t 		left = *count;
        int 		index;
        pid_t 		pmin;

	ASSERT(left > 0);
	index = PID_INDEX(start);

	/* start - 1 because the loop below tests for > pmin */
	pmin = start - 1;

	for ( ; index < pidtabsz; index++, pmin = 0) {
		pid_slot_t	*ps;

	        ps = &pidtab[index];
		if (!pidslot_isactive(ps)) 
                	continue;
                while (1) {
		        int		spl;
			pid_t   	pmax = MAXPID;
			pid_entry_t	*pe;
			int     	found = 0;

		        spl = pidslot_lock(ps);
			if (pidslot_isactive(ps))
				for (pe = ps->ps_chain; pe; pe = pe->pe_next) {
					vproc_t *vpr;

					if ((vpr = pe->pe_vproc) && 
					    vpr->vp_state != 0 &&
					    vpr->vp_pid > pmin &&
					    vpr->vp_pid < pmax) {
						pmax = vpr->vp_pid;
						found = 1;
					}
				}
			pidslot_unlock(ps, spl);
			if (!found)
				break;
			pmin = pmax;
			pidbuf[sofar] = pmin;

			/* If the pid wraps at this index, go to the
			 * next index in the pid table.
			 */
			if ((ordbuf[sofar] = PID_INCR(pmin)) < pmin) {
				/* If the next index is beyond the range
				 * of the pid table, then we have hit the
				 * end of the pid table - set ord to
				 * MAXPID, to end search.
				 */
				if (index + 1 >= pidtabsz)
					ordbuf[sofar] = MAXPID;
				else
					ordbuf[sofar] = pid_base + index + 1;
			}
			sofar++;
			left--;
			if (left == 0) {
				*count = sofar;
			 	return (0);
			}
		}
	}
	*count = sofar;
	return (1);
}
		  
int
procscan(
	int	(*pfunc)(proc_t *, void *, int),
	void	*arg)
{
	pid_slot_t	*ps;
	pid_entry_t	*pe;
	vproc_t		*vp;
	proc_t		*p;
	int		stop_scan = 0;
	int		do_broadcast = 0;
	int		spl;

	/* protect against early clock ticks */
	if (npalloc == 0)
		return 0;
	if ((*pfunc)(0, arg, 0))
		return(0);

	spl = pidactive_lock();
	for (pe = (pid_entry_t *)kqueue_first(&pid_active_list.pa_queue);
	     pe != (pid_entry_t *)kqueue_end(&pid_active_list.pa_queue);
	     pe = (pid_entry_t *)kqueue_next(&pe->pe_queue)) {

		ps = PID_SLOT(pe->pe_pid);
		pidslot_nested_lock(ps);

		if ((vp = pe->pe_vproc) == VPROC_NULL) {
			pidslot_nested_unlock(ps);
			continue;
		}

		/*
		 * Stop the entry from being deallocated
		 */
		pe->pe_nodealloc++;
		pidslot_nested_unlock(ps);
		pidactive_unlock(spl);

		/*
		 * If someone is waiting to deallocate the
		 * previous entry, wake them up now
		 */
		if (do_broadcast) {
			do_broadcast = 0;
			sv_broadcast(&pid_dealloc_sv);
		}
		VPROC_GET_PROC_LOCAL(vp, &p);

		/* Call the scanner if the process is local */
		if (p != NULL)
			stop_scan = (*pfunc)(p, arg, 1);

		/*
		 * Must hold pidactive_lock because as soon as pe_nodealloc
		 * goes to null, it could get pulled from the active queue.
		 * Must hold pidslot_lock to change pe_nodealloc.
		 */
		spl = pidactive_lock();
		pidslot_nested_lock(ps);
		if ((--pe->pe_nodealloc == 0) && pe->pe_dealloc)
			do_broadcast = 1;
		pidslot_nested_unlock(ps);

		if (stop_scan) {
			/*
			 * The function returned a non-zero value so
			 * we stop the scan
			 */
			pidactive_unlock(spl);

			/*
			 * If someone is waiting to deallocate the 
			 * previous entry, wake them up now
			 */
			if (do_broadcast)
				sv_broadcast(&pid_dealloc_sv);
			return(stop_scan);
		}
	}
	pidactive_unlock(spl);

	/*
	 * If someone is waiting to deallocate the last entry,
	 * wake them up now
	 */
	if (do_broadcast)
		sv_broadcast(&pid_dealloc_sv);

	/*
	 * Now scan through all the procs here for which we are not
	 * the origin.
	 */
	stop_scan = remote_procscan(pfunc, arg);
	if (stop_scan)
		return(stop_scan); 
	
	return((*pfunc)(0, arg, 2));
}

/*
 * This is the same function as procscan but without the locking for
 * idbg functions that want to do something to every active process.
 */
int
idbg_procscan(
	int	(*pfunc)(proc_t *, void *, int),
	void	*arg)
{
	pid_entry_t	*pe;
	vproc_t		*vp;
	proc_t		*p;
	int		stop_scan;
	extern int idbg_remote_procscan(int (*)(proc_t *, void *, int), void *);

	/* protect against early usage */
	if (npalloc == 0)
		return 0;
	if ((*pfunc)(0, arg, 0))
		return(0);

	/*
	 * First scan all the procs that are still in their
	 * origin cell
	 */
	for (pe = (pid_entry_t *)kqueue_first(&pid_active_list.pa_queue);
	     pe != (pid_entry_t *)kqueue_end(&pid_active_list.pa_queue);
	     pe = (pid_entry_t *)kqueue_next(&pe->pe_queue)) {
		if (!(vp = pe->pe_vproc))
			continue;
		IDBG_VPROC_GET_PROC(vp, &p);
		if (p != NULL) {
			stop_scan = (*pfunc)(p, arg, 1);
			if (stop_scan)
				return(stop_scan);
		}
	}

	/*
	 * Now scan through all the procs here for which we are not
	 * the origin.
	 */
	stop_scan = idbg_remote_procscan(pfunc, arg);
	if (stop_scan)
		return(stop_scan); 

	return((*pfunc)(0, arg, 2));
}



/* Given pid, return pointer to vproc struct using that
 * pid, if any.
 */
/* Warning: This is used without locks */
vproc_t *
idbg_pid_to_vproc(
	pid_t	pid)
{
	pid_slot_t	*ps;
	pid_entry_t	*pe;
	vproc_t		*vp;

	if (pid < 0 || pid > MAXPID)
		return 0;

	/*
	 * following check is needed to allow SYMMON to display "eframe"s
	 * if you happen to take an exception before pidtab[] is init-ed.
	 */
	if (pidtab == 0)
		return 0;

	ps = PID_SLOT(pid);

	if (!pidslot_isactive(ps))
		return(0);

	vp = VPROC_NULL;
	for (pe = ps->ps_chain; pe; pe = pe->pe_next) {
		if (pe->pe_pid == pid) {
			vp = pe->pe_vproc;
			break;
		}
	}

	return(vp);
}

/* Given pid, return pointer to proc struct using that
 * pid, if any.
 */
/* Warning: This is used without locks */
proc_t *
idbg_pid_to_proc(
	pid_t	pid)
{
	vproc_t	*vp;

	vp = idbg_vproc_lookup(pid);

	if (vp) {
		struct proc *p;

		IDBG_VPROC_GET_PROC(vp, &p);
		return(p);
	}

	return(0);
}
