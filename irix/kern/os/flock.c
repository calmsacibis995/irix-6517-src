/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/flock.c	1.9"*/
#ident	"$Revision: 3.60 $"

/*
 * This module has the older implementation of flock which is available
 * under the systune option, use_old_flock.  All external symbols 
 * have been suffixed with "_old".  Stubs in flock_new.c call either
 * the "_old" or the "_new" routines based on the systune variable.
 */

/*
 * WARNING - this file relies heavily on the fact that it cannot get preempted
 * (for single processor machines) - almost all locks here are
 * app* semas that aren't in UP systems. Most important
 * here is that kmem_alloc NOT be preemptible - if it is this
 * code will break..
 */
/*
 * This file contains all of the file/record locking specific routines.
 * 
 * All record lock lists (referenced by a pointer in the vnode) are
 * ordered by starting position relative to the beginning of the file.
 * 
 * In this file the name "l_end" is a macro and is used in place of
 * "l_len" because the end, not the length, of the record lock is
 * stored internally.
 */

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <sys/flock.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/sysinfo.h>
#include "flock_private.h"

#define	WAKEUP(ptr)		if (ptr->stat.wakeflg) \
					fiwakeup(ptr);

/*
 * compare two flock structures to see if the owners are the same
 * match both process ID and sysid
 * if either flock structure has a pid IGN_PID, consider the pids to match
 */
#define SAMEOWNER(a, b)	((((a)->l_pid == IGN_PID) || ((b)->l_pid == IGN_PID) \
	|| ((a)->l_pid == (b)->l_pid)) && ((a)->l_sysid == (b)->l_sysid))

#define SAMERANGE(a, b)	\
	(((a)->l_start == (b)->l_start) && ((a)->l_len == (b)->l_len))

/* region types */
#define	S_BEFORE	010
#define	S_START		020
#define	S_MIDDLE	030
#define	S_END		040
#define	S_AFTER		050
#define	E_BEFORE	001
#define	E_START		002
#define	E_MIDDLE	003
#define	E_END		004
#define	E_AFTER		005

static zone_t 	*frlock_zone;		/* zone for frlock data structures */

static struct	filock	*sleeplcks;	/* head of chain of sleeping locks */
static mutex_t	slplck_sem;		/* protects sleeplcks */


static int deadflck(struct filock *flp, struct flock *lckdat);
static struct filock *insflck(struct filock **, struct flock *, struct filock *);
static void delflck(struct filock  **lck_list, struct filock  *fl);
static int regflck(struct flock *ld, struct filock *flp);
static int flckadj(struct filock **, struct filock *, struct flock *);
static struct filock *blocked(struct filock *, struct flock *, struct filock **);
#if DEBUG
static int isonlist(struct filock  *lck_list, struct filock  *fl);
#endif
static void fiwakeup(struct filock *);

static flid_t __sys_flid;

/*
 * Initialize file locking data structures.
 */
void
flckinit_old(void)
{
	frlock_zone = kmem_zone_init(sizeof(struct filock), "frlock");
	mutex_init(&slplck_sem, MUTEX_DEFAULT, "slplck");
	sys_flid = &__sys_flid;
	__sys_flid.fl_pid = 0;
	__sys_flid.fl_sysid = 0L;
}

/*
 * Insert lock (lckdat) after given lock (fl); If fl is NULL place the
 * new lock at the beginning of the list and update the head ptr to
 * list which is stored at the address given by lck_list. 
 *
 * Note that it is the caller's responsibility to have locked 'lck_list'.
 */
static struct filock *
insflck(struct filock **lck_list, struct flock *lckdat, struct filock *fl)
{
	register struct filock *new;

	/*
	 * Allocate a filock data structure.
	 */
	new = kmem_zone_alloc(frlock_zone, KM_SLEEP);
	ASSERT(new != NULL);
	++SYSINFO.reccnt;

	sv_init(&new->wakesem, SV_DEFAULT, "frlock");
	new->set = *lckdat;
	new->stat.wakeflg = 0;
	new->set.l_pid = lckdat->l_pid;
	new->set.l_sysid = lckdat->l_sysid;
	if (fl == NULL) {
		new->next = *lck_list;
		if (new->next != NULL)
			new->next->prev = new;
		*lck_list = new;
	} else {
		new->next = fl->next;
		if (fl->next != NULL)
			fl->next->prev = new;
		fl->next = new;
	}
	new->prev = fl;

	return new;
}

#ifdef DEBUG
static int
awaited(struct filock  *fl)
{
	struct filock *fi;
	mutex_lock(&slplck_sem, PZERO);
	for (fi = sleeplcks; fi; fi = fi->next) {
		if (fi->stat.blk.blocked == fl) {
			mutex_unlock(&slplck_sem);
			return 1;
		}
	}
	mutex_unlock(&slplck_sem);
	return 0;
}
#endif /* DEBUG */

/*
 * Delete lock (fl) from the record lock list. If fl is the first lock
 * in the list, remove it and update the head ptr to the list which is
 * stored at the address given by lck_list.
 *
 * Note that it is the caller's responsibility to have locked 'lck_list'.
 */
static void
delflck(struct filock  **lck_list, struct filock  *fl)
{
	ASSERT(isonlist(*lck_list, fl));
	if (fl->prev != NULL)
		fl->prev->next = fl->next;
	else
		*lck_list = fl->next;
	if (fl->next != NULL)
		fl->next->prev = fl->prev;
	if (lck_list != &sleeplcks) {
		ASSERT(fl->stat.wakeflg || !awaited(fl));
		WAKEUP(fl);
		ASSERT(!awaited(fl));
	}
	SYSINFO.reccnt--;

	/*
	 * Free the filock data structure.
	 */
	sv_destroy(&fl->wakesem);
	kmem_zone_free(frlock_zone, fl);
}

/*
 * regflck sets the type of span of this (un)lock relative to the specified
 * already existing locked section.
 * There are five regions:
 *
 *  S_BEFORE        S_START         S_MIDDLE         S_END          S_AFTER
 *     010            020             030             040             050
 *  E_BEFORE        E_START         E_MIDDLE         E_END          E_AFTER
 *      01             02              03              04              05
 * 			|-------------------------------|
 *
 * relative to the already locked section.  The type is two octal digits,
 * the 8's digit is the start type and the 1's digit is the end type.
 */
static int
regflck(struct flock *ld, struct filock *flp)
{
	register int regntype;

	if (ld->l_start > flp->set.l_start) {
		if (ld->l_start-1 == flp->set.l_end)
			return S_END|E_AFTER;
		if (ld->l_start > flp->set.l_end)
			return S_AFTER|E_AFTER;
		regntype = S_MIDDLE;
	} else if (ld->l_start == flp->set.l_start)
		regntype = S_START;
	else
		regntype = S_BEFORE;

	if (ld->l_end < flp->set.l_end) {
		if (ld->l_end == flp->set.l_start-1)
			regntype |= E_START;
		else if (ld->l_end < flp->set.l_start)
			regntype |= E_BEFORE;
		else
			regntype |= E_MIDDLE;
	} else if (ld->l_end == flp->set.l_end)
		regntype |= E_END;
	else
		regntype |= E_AFTER;

	return  regntype;
}

/*
 * Adjust file lock from region specified by 'ld', in the record
 * lock list indicated by the head ptr stored at the address given
 * by lck_list. Start updates at the lock given by 'insrtp'.  It is 
 * assumed the list is ordered on starting position, relative to 
 * the beginning of the file, and no updating is required on any
 * locks in the list previous to the one pointed to by insrtp.
 * Insrtp is a result from the routine blocked().  Flckadj() scans
 * the list looking for locks owned by the process requesting the
 * new (un)lock :
 *
 * 	- If the new record (un)lock overlays an existing lock of
 * 	  a different type, the region overlaid is released.
 *
 * 	- If the new record (un)lock overlays or adjoins an exist-
 * 	  ing lock of the same type, the existing lock is deleted
 * 	  and its region is coalesced into the new (un)lock.
 *
 * When the list is sufficiently scanned and the new lock is not 
 * an unlock, the new lock is inserted into the appropriate
 * position in the list.
 */
static int
flckadj(struct filock **lck_list,
	register struct filock *insrtp,
	struct flock *ld)
{
	register struct	filock	*flp, *nflp;
	int regtyp;

	nflp = (insrtp == NULL) ? *lck_list : insrtp;

	while (flp = nflp) {
		nflp = flp->next;
		if( SAMEOWNER(&(flp->set), ld) ) {

			/* Release already locked region if necessary */

			switch (regtyp = regflck(ld, flp)) {
			case S_BEFORE|E_BEFORE:
				nflp = NULL;
				break;
			case S_BEFORE|E_START:
				if (ld->l_type == flp->set.l_type) {
					ld->l_end = flp->set.l_end;
					delflck(lck_list, flp);
				}
				nflp = NULL;
				break;
			case S_START|E_END:
				/*
				 * Don't bother if this is in the middle of
				 * an already similarly set section.
				 */
				if (ld->l_type == flp->set.l_type)
					return 0;
			case S_START|E_AFTER:
				insrtp = flp->prev;
				delflck(lck_list, flp);
				break;
			case S_BEFORE|E_END:
				if (ld->l_type == flp->set.l_type)
					nflp = NULL;
			case S_BEFORE|E_AFTER:
				delflck(lck_list, flp);
				break;
			case S_START|E_MIDDLE:
				insrtp = flp->prev;
			case S_MIDDLE|E_MIDDLE:
				/*
				 * Don't bother if this is in the middle of
				 * an already similarly set section.
				 */
				if (ld->l_type == flp->set.l_type)
					return 0;
			case S_BEFORE|E_MIDDLE:
				if (ld->l_type == flp->set.l_type)
					ld->l_end = flp->set.l_end;
				else {
					/* setup piece after end of (un)lock */
					register struct	filock *tdi, *tdp;
					struct flock td;

					td = flp->set;
					td.l_start = ld->l_end + 1;
					tdp = tdi = flp;
					do {
						if (tdp->set.l_start < td.l_start)
							tdi = tdp;
						else
							break;
					} while (tdp = tdp->next);
					if (insflck(lck_list, &td, tdi) == NULL)
						return ENOLCK;
				}
				if (regtyp == (S_MIDDLE|E_MIDDLE)) {
					/* setup piece before (un)lock */
					flp->set.l_end = ld->l_start - 1;
					WAKEUP(flp);
					insrtp = flp;
				} else
					delflck(lck_list, flp);
				nflp = NULL;
				break;
			case S_MIDDLE|E_END:
				/*
				 * Don't bother if this is in the middle of
				 * an already similarly set section.
				 */
				if (ld->l_type == flp->set.l_type)
					return 0;
				flp->set.l_end = ld->l_start - 1;
				WAKEUP(flp);
				insrtp = flp;
				break;
			case S_MIDDLE|E_AFTER:
				if (ld->l_type == flp->set.l_type) {
					ld->l_start = flp->set.l_start;
					insrtp = flp->prev;
					delflck(lck_list, flp);
				} else {
					flp->set.l_end = ld->l_start - 1;
					WAKEUP(flp);
					insrtp = flp;
				}
				break;
			case S_END|E_AFTER:
				if (ld->l_type == flp->set.l_type) {
					ld->l_start = flp->set.l_start;
					insrtp = flp->prev;
					delflck(lck_list, flp);
				}
				break;
			case S_AFTER|E_AFTER:
				insrtp = flp;
				break;
			}
		}
	}

	if (ld->l_type != F_UNLCK) {
		if (flp = insrtp) {
			do {
				if (flp->set.l_start < ld->l_start)
					insrtp = flp;
				else
					break;
			} while (flp = flp->next);
		}
		if (insflck(lck_list, ld, insrtp) == NULL)
			return ENOLCK;
	}

	return 0;
}

/*
 * blocked() checks whether a new lock (lckdat) would be
 * blocked by a previously set lock owned by another process.
 * Insrt is set to point to the lock where lock list updating
 * should begin to place the new lock.
 */ 
static struct filock *
blocked(struct filock *flp,
	struct flock  *lckdat,
	struct filock **insrt)
{
	register struct filock *f;

	*insrt = NULL;
	for (f = flp; f != NULL; f = f->next) {
		if (f->set.l_start < lckdat->l_start)
			*insrt = f;
		else
			break;
		if( SAMEOWNER(&(f->set), lckdat) ) {
			if ((lckdat->l_start-1) <= f->set.l_end)
				break;
		} else if (lckdat->l_start <= f->set.l_end
		  && (f->set.l_type == F_WRLCK
		    || (f->set.l_type == F_RDLCK && lckdat->l_type == F_WRLCK)))
			return f;
	}

	for (; f != NULL; f = f->next) {
		if (lckdat->l_end < f->set.l_start)
			break;
		if (lckdat->l_start <= f->set.l_end
		  && ( !SAMEOWNER(&(f->set), lckdat) )
		  && (f->set.l_type == F_WRLCK
		    || (f->set.l_type == F_RDLCK
		            && lckdat->l_type == F_WRLCK)))
			return f;
	}

	return NULL;
}

#ifdef DEBUG
static int
good_sleep_list(void)
{
	struct filock *sf;

	for (sf = sleeplcks; sf != NULL; sf = sf->next) {
		if (sf->stat.blk.blocked && !sf->stat.blk.blocked->stat.wakeflg) {
			return(0);
		}
	}
	return(1);
}
#endif /* DEBUG */

/*
 * get and set file/record locks
 *
 * cmd & SETFLCK indicates setting a lock.
 * cmd & SLPFLCK indicates waiting if there is a blocking lock.
 */
int
reclock_old(struct vnode *vp,
	struct flock *lckdat,
	int cmd,
	int flag,
	off_t offset,
	cred_t *cr)
{
	register struct filock **lock_list, *sf;
	struct	filock *found, *insrt = NULL;
	int retval;
	int contflg;
	int locks_were_held;

	/* check access permissions */
	if ((cmd & SETFLCK) 
	  && ((lckdat->l_type == F_RDLCK && (flag & FREAD) == 0)
	    || (lckdat->l_type == F_WRLCK && (flag & FWRITE) == 0)))
		return EBADF;
	
	/* Convert start to be relative to beginning of file */
	/* whence = lckdat->l_whence; XXX see below */
	if (retval = convoff_old(vp, lckdat, 0, offset, SEEKLIMIT, cr))
		return retval;

	/* Convert l_len to be the end of the rec lock l_end */
	if (lckdat->l_len < 0)
		return EINVAL;
	if (lckdat->l_len == 0)
		lckdat->l_end = MAXEND;
	else
		lckdat->l_end += (lckdat->l_start - 1);

	/* check for arithmetic overflow */
	if (lckdat->l_start > lckdat->l_end)
		return EINVAL;

	mp_mutex_lock(&vp->v_filocksem, PZERO);
	lock_list = &vp->v_filocks;
	locks_were_held = (vp->v_filocks != NULL) ? 1 : 0;

	do {
		contflg = 0;
		switch (lckdat->l_type) {
		case F_RDLCK:
		case F_WRLCK:
			/*
			 * See if we can get the lock.
			 */
			found = blocked(*lock_list, lckdat, &insrt);
			if (found == NULL) {
				/*
				 * Can get it.
				 */
				if (cmd & (SETFLCK | SETBSDFLCK)) {
					/*
					 * Get it.
					 */
					retval = flckadj(lock_list, insrt, 
							 lckdat);
				} else {
					/*
					 * Just return an indication that
					 * the lock isn't held.
					 */
					lckdat->l_type = F_UNLCK;
				}

				/*
				 * All done, unless called by Net Lock Manager.
				 */
				if ((cmd & (RCMDLCK|SLPFLCK)) != 
				    (RCMDLCK|SLPFLCK))
					break;
			} else if (cmd & SLPFLCK) {
				/*
				 * Can't get it and need to sleep.
				 */
				mutex_lock(&slplck_sem, PZERO);
				if (lock_pending(vp, lckdat)) {
					mutex_unlock(&slplck_sem);
					retval = EBUSY;
				/* do deadlock detection here */
				} else if (deadflck(found, lckdat)) {
					mutex_unlock(&slplck_sem);
					retval = EDEADLK;
				} else if ((sf=insflck(&sleeplcks, lckdat, NULL)
						) == NULL) {
					mutex_unlock(&slplck_sem);
					retval = ENOLCK;
				} else {
					sf->stat.blk.pid = found->set.l_pid;
					sf->stat.blk.sysid = found->set.l_sysid;
					ASSERT(!isonlist(sleeplcks, found));
					sf->stat.blk.blocked = found;
					ASSERT(vp);
					sf->stat.blk.cancel = 0;
					found->stat.wakeflg = 1;
					ASSERT(good_sleep_list());

					/*
					 * Unlock while we sleep.
					 */
					mp_mutex_unlock(&vp->v_filocksem);
					if (cmd & INOFLCK) {
						VOP_RWUNLOCK(vp, 
						  (cmd & INOFLCK_READ) ?
						  VRWLOCK_READ : VRWLOCK_WRITE);
					}
					if (sv_wait_sig(&found->wakesem,
							PZERO+1, &slplck_sem, 
							0))
							retval = EINTR;
					else {
						retval = 0;
						contflg = 1;
					}
					ASSERT(!retval || !contflg);

					/*
					 * Relock now what we're awake.
					 */
					if (cmd & INOFLCK) {
						VOP_RWLOCK(vp, 
						  (cmd & INOFLCK_READ) ?
						  VRWLOCK_READ : VRWLOCK_WRITE);
					}
					mp_mutex_lock(&vp->v_filocksem, PZERO);

					/*
					 * Reset the locks_held flag now
					 * that we've relocked.
					 */
					locks_were_held = 
					     (vp->v_filocks != NULL) ? 1 : 0;

					/*
					 * We must check for cancellation 
					 * while holding the sleep lock sema.
					 */
					mutex_lock(&slplck_sem, PZERO);
					if (sf->stat.blk.cancel) {
						retval = EINTR;
						contflg = 0;
					}
					ASSERT(isonlist(sleeplcks, sf));
					sf->stat.blk.pid = 0;
					sf->stat.blk.sysid = 0;
					delflck(&sleeplcks, sf);
					mutex_unlock(&slplck_sem);
				}
			} else if (cmd & (SETFLCK | SETBSDFLCK)) {
				retval = EAGAIN;
				if (cmd & RCMDLCK) {
					mutex_lock(&slplck_sem, PZERO);
					if (deadflck(found, lckdat))
						retval = EDEADLK;
					mutex_unlock(&slplck_sem);
				}
			} else {
				*lckdat = found->set;
			}
			break;
		case F_UNLCK:
			/* removing a file record lock */
			if (cmd & (SETFLCK | SETBSDFLCK))
				retval = flckadj(lock_list, *lock_list, lckdat);

			break;
		default:
			/* invalid lock type */
			retval = EINVAL;
			break;
		}
	} while (contflg);

	/*
	 * Call VOP_VNODE_CHANGE if the vnode has transitioned from either:
	 * - having no locks to having at least one lock
	 * - having at least one lock to having no locks
	 */
	if (locks_were_held) {
		if (vp->v_filocks == NULL)
			VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 0);
	} else {
		if (vp->v_filocks != NULL)
			VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 1);
	}
		
	mp_mutex_unlock(&vp->v_filocksem);

	/* Restore l_len */
	if (lckdat->l_end == MAXEND)
		lckdat->l_len = 0;
	else
		lckdat->l_len -= (lckdat->l_start-1);
	/*
	 * POSIX compatibility requires that we disable this perfectly
	 * nice behavior.  The returned lock description will be normalized
	 * relative to SEEK_SET (0) instead of being converted back to the
	 * format that was passed in.
	 *
	 * (void) convoff_old(vp, lckdat, whence, offset, SEEKLIMIT);
	 */

	return retval;
}

/*
 * Given and process ID (possibly IGN_PID) and a sysid, determine whether
 * or not any locks are held on the given vnode.  Returns 1 (locks held)
 * or 0 (no locks held).
 */
int
haslocks_old(register struct vnode *vp, pid_t pid, sysid_t sysid)
{
	register struct filock *f;
	int locks_held = 0;

	/*
	 * If there are no locks at all, we can skip this since asking whether
	 * you have a lock held is an inherently racy action ...
	 */
	if (vp->v_filocks) {
		mp_mutex_lock(&vp->v_filocksem, PZERO);
		for (f = vp->v_filocks; f != NULL; f = f->next) {
			if ((sysid == f->set.l_sysid) &&
			    ((pid == IGN_PID) || (pid == f->set.l_pid))) {
					locks_held = 1;
					break;
			}
		}
		mp_mutex_unlock(&vp->v_filocksem);
	}
	return(locks_held);
}

/*
 * This function answers the question "Is there a lock with the
 * same owner as the lock described by ld which spans the lock
 * described by ld?"
 */
int
haslock_old(struct vnode *vp, flock_t *ld)
{
	register struct filock *f;
	int lock_held = 0;
	int reg;

	/* Convert l_len to be the end of the rec lock l_end */
	if (ld->l_len == 0)
		ld->l_end = MAXEND;
	else
		ld->l_end += (ld->l_start - 1);

	/*
	 * If there are no locks at all, we can skip this since asking whether
	 * you have a lock held is an inherently racy action ...
	 */
	if (vp->v_filocks) {
		mp_mutex_lock(&vp->v_filocksem, PZERO);
		for (f = vp->v_filocks; !lock_held && f; f = f->next) {
			if (SAMEOWNER(ld, &(f->set)) &&
			    (ld->l_type == f->set.l_type)) {
				switch (reg = regflck(ld, f)) {
					case S_MIDDLE|E_MIDDLE:
					case S_START|E_END:
					case S_START|E_MIDDLE:
					case S_MIDDLE|E_END:
						lock_held = 1;
						break;
					default:
						break;
				}
			}
		}
		mp_mutex_unlock(&vp->v_filocksem);
	}
	/* Restore l_len */
	if (ld->l_end == MAXEND)
		ld->l_len = 0;
	else
		ld->l_len -= (ld->l_start-1);
	return(lock_held);
}

#ifdef DEBUG
void
print_locks_old(struct vnode *vp, pid_t pid, __uint32_t sysid)
{
	register struct filock *f;

	if (vp->v_filocks) {
		printf("Locks held, vp 0x%p pid %d sysid 0x%x\n", vp, pid, sysid);
		mp_mutex_lock(&vp->v_filocksem, PZERO);
		for (f = vp->v_filocks; f != NULL; f = f->next) {
			if ((sysid == f->set.l_sysid) &&
				((pid == IGN_PID) || (pid == f->set.l_pid))) {
					printf("\tpid %d: %lld, %lld, type %d\n", f->set.l_pid,
						f->set.l_start, f->set.l_end, f->set.l_type);
			}
		}
		mp_mutex_unlock(&vp->v_filocksem);
	}
}

void
print_blocked_locks_old(__uint32_t sysid)
{
	register struct filock *f;

	printf("Blocked locks for sysid 0x%x\n", sysid);
	mutex_lock(&slplck_sem, PZERO);
	for (f = sleeplcks; f != NULL; f = f->next) {
		if (f->set.l_sysid == sysid) {
			printf("\tpid %d: %lld %lld type %d\n",
				f->set.l_pid, f->set.l_start, f->set.l_end, f->set.l_type);
		}
	}
	mutex_unlock(&slplck_sem);
}
#endif /* DEBUG */

/*
 * Determine whether or not any remote locks are held on the given vnode.
 * Returns 1 (locks held) or 0 (no locks held).
 */
int
remotelocks_old(register struct vnode *vp)
{
	int locks_held = 0;
	register struct filock *f;

#if MP
	/*
	 * This assert is only valid on MP systems because the mutex is
	 * acquired with mp_mutex_lock which is only defined for MP.
	 */
	ASSERT(mutex_mine(&vp->v_filocksem));
#endif /* MP */
	for (f = vp->v_filocks; f != NULL; f = f->next) {
		/*
		 * Local locks will have a sysid of 0, remote ones will be
		 * non-zero, so return 1 when the first lock with a non-zero
		 * sysid is found.
		 */
		if (f->set.l_sysid != 0) {
			locks_held = 1;
			break;
		}
	}
	return(locks_held);
}

void
release_remote_locks_old(vnode_t *vp)
{
	register struct filock *f;
	struct filock *tmp;

	/*
	 * If there are no locks at all, we can skip this ...
	 */
	if (vp->v_filocks) {
		mp_mutex_lock(&vp->v_filocksem, PZERO);
		for (f = vp->v_filocks; f != NULL; ) {
			/*
			 * Local locks will have a sysid of 0, remote ones will be
			 * non-zero, so return 1 when the first lock with a non-zero
			 * sysid is found.
			 */
			if (f->set.l_sysid != 0) {
				/*
				 * get the next pointer before we delete the entry
				 */
				tmp = f;
				f = f->next;
				delflck(&vp->v_filocks, tmp);
			} else {
				f = f->next;
			}
		}
		mp_mutex_unlock(&vp->v_filocksem);
	}
}

/*
 * Determine whether or not there is a pending (blocked) lock request
 * matching the given vnode and lock description.
 */
/* ARGSUSED */
int
lock_pending_old(vnode_t *vp, flock_t *flp)
{
	register struct filock *f;
	int found = 0;

	for (f = sleeplcks; f != NULL; f = f->next) {
		/*
		 * We check only ownership here.  SAMEOWNER checks sysid and pid.
		 * It is not possible for a process to block for one request and
		 * send another request for a different lock.
		 * Ignore cancelled reuests.
		 */
		if (!f->stat.blk.cancel && SAMEOWNER(&f->set, flp)) {
				found++;
#ifndef DEBUG
				break;
#endif /* DEBUG */
		}
	}
	ASSERT((found == 0) || (found == 1));
	return(found);
}

#ifdef DEBUG
int
locks_pending_old(pid_t pid, __uint32_t sysid)
{
	register struct filock *f;
	int found = 0;

	mutex_lock(&slplck_sem, PZERO);
	for (f = sleeplcks; f != NULL; f = f->next) {
		/*
		 * We check only ownership here.  SAMEOWNER checks sysid and pid.
		 * It is not possible for a process to block for one request and
		 * send another request for a different lock.
		 */
		if (!f->stat.blk.cancel && (f->set.l_sysid == sysid) &&
			((pid == IGN_PID) || (pid == f->set.l_pid))) {
				found++;
		}
	}
	mutex_unlock(&slplck_sem);
	return(found);
}
#endif /* DEBUG */

/*
 * Cancel a pending (blocked) lock request.
 */
int
cancel_lock_old(vnode_t *vp, flock_t *flp)
{
	register struct filock *f;
	register struct filock *next;
	register struct filock *fi;
	register int error = EAGAIN; /* must map to nlm_denied/NLM4_DENIED */
#ifdef DEBUG
	flock_t tfl;

	ASSERT(flp->l_whence == 0);
	tfl = *flp;
	if (tfl.l_len == 0)
		tfl.l_end = MAXEND;
	else
		tfl.l_end += (tfl.l_start - 1);
	ASSERT(tfl.l_start <= tfl.l_end);
#endif /* DEBUG */

	mp_mutex_lock(&vp->v_filocksem, PZERO);
	mutex_lock(&slplck_sem, PZERO);
	/*
	 * Mark all of the sleeping locks with the same owner, range and
	 * vnode as cancelled.  This must be done before waking them up.
	 */
	for (f = sleeplcks, next = NULL; f != NULL; f = next) {
		next = f->next;
		ASSERT(!f->stat.blk.blocked ||
			!isonlist(sleeplcks, f->stat.blk.blocked));
		if (SAMEOWNER(&f->set, flp) && SAMERANGE(&f->set, flp)) {
			/*
			 * This blocked lock request is to be cancelled.
			 * Mark it as such and wake up all processes
			 * waiting for it.  It may happen that we are
			 * racing with an unlock which has already woken
			 * this sleeping file lock.  In that case,
			 * stat.blk.blocked will be NULL.  It is sufficient
			 * to simply set stat.blk.cancel to 1 if this
			 * happens.  The awakened processes will notice
			 * this and to the right thing.
			 */
			f->stat.blk.cancel = 1;
			f->stat.blk.pid = 0;
			f->stat.blk.sysid = 0;
			fi = f->stat.blk.blocked;
			f->stat.blk.blocked = NULL;
			if (fi) {
				fi->stat.wakeflg = 0;
#ifdef DEBUG
				for (f = sleeplcks; f != NULL; f = f->next) {
					if (f->stat.blk.blocked == fi) {
						f->stat.blk.blocked = NULL;
					}
				}
#endif /* DEBUG */
				sv_broadcast(&fi->wakesem);
			}
			error = 0; /* matching blocked lock has been found */
		}
	}
	ASSERT(good_sleep_list());
	mutex_unlock(&slplck_sem);
	mp_mutex_unlock(&vp->v_filocksem);
	return(error);
}

/*
 * Cancel all pending (blocked) lock requests for the given sysid.
 */
void
cancel_blocked_locks_old(__uint32_t sysid)
{
	register struct filock *f;
	register struct filock *fi;
	register struct filock *next;

	mutex_lock(&slplck_sem, PZERO);
	for (f = sleeplcks, next = NULL; f != NULL; f = next) {
		next = f->next;
		if (f->set.l_sysid == sysid) {
			ASSERT(!f->stat.blk.blocked ||
				!isonlist(sleeplcks, f->stat.blk.blocked));
			f->stat.blk.cancel = 1;
			f->stat.blk.pid = 0;
			f->stat.blk.sysid = 0;
			fi = f->stat.blk.blocked;
			f->stat.blk.blocked = NULL;
			if (fi) {
				fi->stat.wakeflg = 0;
				sv_broadcast(&fi->wakesem);
			}
		}
	}
	ASSERT(good_sleep_list());
	mutex_unlock(&slplck_sem);
}

/*
 * convoff - converts the given data (start, whence) to the
 * given whence.
 * The limit given as a parameter specifies the maximum offset the
 * caller is willing to allow.
 */
int
convoff_old(struct vnode *vp,
	struct flock *lckdat,
	int whence,
	off_t offset,
	off_t limit,
	cred_t *cr)
{
	int error;
	off_t llen;
	struct vattr vattr;

	vattr.va_mask = AT_SIZE;
	VOP_GETATTR(vp, &vattr, 0, cr, error);
	if (error)
		return error;
	if (lckdat->l_whence == 1)
		lckdat->l_start += offset;
	else if (lckdat->l_whence == 2)
		lckdat->l_start += vattr.va_size;
	else if (lckdat->l_whence != 0)
		return EINVAL;

	/*
	 * Bug/feature alert.  The following test allows locking
	 * starting at 'limit' with length of one to succeed
	 * even though there can't be any file space _beginning_
	 * at 'limit' in most cases.  But C-ISAM (and other products?)
	 * relies on the ability to lock here at the end of the world...
	 *
	 * Note that it is up to the calling file systems to enforce any
	 * 32 bit limits that they might have.
	 */
	llen = lckdat->l_len > 0 ? lckdat->l_len - 1 : lckdat->l_len;

	if ((lckdat->l_start < 0) ||
	    (lckdat->l_start > limit) ||
	    (lckdat->l_start + llen < 0) ||
	    (lckdat->l_start + llen > limit)) {
		return EINVAL;
	}

	if (whence == 1)
		lckdat->l_start -= offset;
	else if (whence == 2)
		lckdat->l_start -= vattr.va_size;
	else if (whence != 0)
		return EINVAL;
	lckdat->l_whence = (short)whence;
	return 0;
}

/*
 * deadflck does deadlock detection for a given record.
 */
static int
deadflck(
	struct filock *flp,
	struct flock *lckdat)
{
	register struct filock *blck, *sf;
	pid_t blckpid;
	sysid_t blcksysid;

	blck = flp;	/* current blocking lock pointer */
	blckpid = blck->set.l_pid;
	blcksysid = blck->set.l_sysid;
	do {
		if (blckpid == lckdat->l_pid
		  && blcksysid == lckdat->l_sysid)
			return 1;
		/*
		 * If the blocking process is sleeping on a locked region,
		 * change the blocked lock to this one.
		 */
		for (sf = sleeplcks; sf != NULL; sf = sf->next) {
			if (blckpid == sf->set.l_pid
			  && blcksysid == sf->set.l_sysid) {
				blckpid = sf->stat.blk.pid;
				blcksysid = sf->stat.blk.sysid;
				break;
			}
		}
		blck = sf;
	} while (blck != NULL);
	return 0;
}

/*
 * Clean up record locks left around by process.
 */
void
cleanlocks_old(struct vnode *vp, pid_t pid, sysid_t sysid)
{
	register struct filock *flp, *nflp, **lock_list;
	int locks_were_held;

	if (!vp->v_filocks) {
		ASSERT(!(vp->v_flag & VFRLOCKS));
		return;
	}

	mp_mutex_lock(&vp->v_filocksem, PZERO);

	locks_were_held = (vp->v_filocks != NULL) ? 1 : 0;

	lock_list = &vp->v_filocks;
	nflp = (struct filock *)0;
	for (flp = *lock_list; flp != NULL; flp = nflp) {
		nflp = flp->next;
		if (((flp->set.l_pid == pid) || (pid == IGN_PID))
		  && flp->set.l_sysid == sysid)
			delflck(lock_list, flp);
	}

	/*
	 * Call VOP_VNODE_CHANGE if the vnode has transitioned from
	 * having at least one lock to having no locks
	 */
	if (locks_were_held && vp->v_filocks == NULL)
		VOP_VNODE_CHANGE(vp, VCHANGE_FLAGS_FRLOCKS, 0);
		
	mp_mutex_unlock(&vp->v_filocksem);
}

#if DEBUG
static int
isonlist(struct filock  *lck_list, struct filock  *fl)
{
	struct filock *fi;
	for (fi = lck_list; fi; fi = fi->next)
		if (fi == fl)
			return 1;
	return 0;
}
#endif

/*
 * wakeup all folks waiting on 'fi'
 * In MP, its possible that we have the following scenario:
 * Proc A locks 0
 * Proc B locks 0 - goes to sleep, sleeplk points to A as locker
 * Proc C locks 0 - goes to sleep, sleeplk points to A as locker
 * Proc A unlocks 0 - wakeups up B & C (but they don't run yet)
 * Proc C runs, gets lock, and is running on 1 processor
 * Proc A locks 0 - it will get a bogus EDEADLCK back since B hasn't
 *	run yet as so still has a sleeplk record pointing at A.
 * POSIX states that EDEADLCK isn't absolute, so it seems better
 * 	to err on the side of too few EDEADLCKs returned rather
 * 	than too many.
 * We grab the sleep_sem to avoid races
 */
static void
fiwakeup(struct filock *fi)
{
	struct filock *sf;

	if (fi->stat.wakeflg) {
		mutex_lock(&slplck_sem, PZERO);
		for (sf = sleeplcks; sf != NULL; sf = sf->next) {
			if (sf->stat.blk.blocked == fi) {
				sf->stat.blk.pid = 0;
				sf->stat.blk.sysid = 0;
				sf->stat.blk.blocked = NULL;
			}
		}
		sv_broadcast(&fi->wakesem);
		fi->stat.wakeflg = 0;
		ASSERT(good_sleep_list());
		mutex_unlock(&slplck_sem);
	}
}
