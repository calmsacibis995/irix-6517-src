/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * SGI implementation of select.
 */

#ident "$Revision: 1.112 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/pda.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/prctl.h>
#include <sys/systm.h>
#include <sys/ktime.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/callo.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/splock.h>
#include <sys/sat.h>
#include <sys/schedctl.h>
#include <sys/xlate.h>
#include <ksys/fdt.h>
#include <sys/vsocket.h>


static int dopoll(int, struct pollfd *, struct timeval *, int *, int);

void
initpollhead(struct pollhead *php)
{
	bzero (php, sizeof(*php));
	spinlock_init(&php->ph_lock, "ph_lock");
}

void
destroypollhead(struct pollhead *php)
{
	struct polldat *pdp;

	/*
	 * free any distributed events
	 */
	while (pdp = php->ph_next) {
		php->ph_next = pdp->pd_next;
		kmem_free(pdp, sizeof(*pdp));
	}

	spinlock_destroy(&php->ph_lock);
}

/*
 * the function to be placed in the callout table to time out a process
 * waiting on poll.  
 *
 * XXX This routine is *not* declared "static" so it may be called from the
 * XXX DCE DFS code.  This is a *really ugly* situation and should be fixed.
 * XXX See bug #550656, ``DCE DFS vs. select/poll [private] interfaces.''
 */
void
polltime(uthread_t *ut)
{
	int s = mutex_spinlock(&ut->ut_pollrotorlock);
	sv_signal(&UT_TO_KT(ut)->k_timewait);
	mutex_spinunlock(&ut->ut_pollrotorlock, s);
}

/*
 * Masks for ut_pollrotor.  We keep track of how many wakeups have been done
 * on the process thread while it was asleep.  If we have only had one, then
 * the dopoll() code will just pickup the rotor fd status and return.  This
 * helps processes like the X server that poll on a large # of fds with only
 * one or two ever active from instant to instant.
 */
#define	ROTORFDMASK	0x3FFF
#define	ROTORONCE	0x4000
#define	ROTORMULTI	0xC000


/*
 * This function initializes a polldat structure with the given data and
 * places it on the passed pollhead's list.
 */
static __inline int
_polladd(struct pollhead *php, short events, unsigned int gen,
	 void *arg, struct polldat *pdp)
{
	int s;

	ASSERT(php != NULL && pdp != NULL);
	pdp->pd_events = events;
	pdp->pd_headp = php;
	pdp->pd_arg = arg;

	s = mutex_spinlock(&php->ph_lock);
	if (POLLGEN(php) != gen) {
		#pragma mips_frequency_hint NEVER
		/*
		 * If the poll generation number is different than when we
		 * started, then a pollwakeup has fired.
		 */
		mutex_spinunlock(&php->ph_lock, s);
		return 1;
	}
	pdp->pd_next = php->ph_list;
	php->ph_list = pdp;
	php->ph_events |= events;
	mutex_spinunlock(&php->ph_lock, s);
	return 0;
}

#if CELL_IRIX
/*
 * Variation of polladd used by distributed poll implementation.
 * Called by I_ routines when they want to queue a thread on a
 * pollhead.
 */
#if DEBUG
long polladd_dynamic;
#endif

int
distributed_polladd(struct pollhead *php, 
	short events, 
	unsigned int gen)
{
	int	s;
	struct polldat *pdp;
	int	curpid = current_pid();
	int	curtid = current_utid();

	ASSERT(php != NULL);
	s = mutex_spinlock(&php->ph_lock);

	if (POLLGEN(php) != gen) {
		/*
		 * If the poll generation number is different than when we
		 * started, then a pollwakeup has fired.
		 */
		mutex_spinunlock(&php->ph_lock, s);
		return 1;
	}

	if (pdp = php->ph_next) {
		while (pdp) {
			if ((pdp->pd_pid == curpid) && 
				(pdp->pd_tid == curtid)) {
				goto found;
			}
			pdp = pdp->pd_next;
		}
	}

	mutex_spinunlock(&php->ph_lock, s);
	pdp = (struct polldat *)kmem_alloc(sizeof(*pdp), KM_SLEEP);
	s = mutex_spinlock(&php->ph_lock);
	if (POLLGEN(php) != gen) {
		/*
		 * If the poll generation number is different than 
		 * when we started, then a pollwakeup has fired.
		 */
		mutex_spinunlock(&php->ph_lock, s);
		kmem_free(pdp, sizeof(*pdp));
		return 1;
	}
	pdp->pd_next = php->ph_next;
	php->ph_next = pdp;
	pdp->pd_events = 0;
#if DEBUG
	polladd_dynamic++;
#endif

	pdp->pd_headp = php;
	pdp->pd_rotorhint = -1;
	pdp->pd_pid = curpid;
	pdp->pd_tid = curtid;

found:
	pdp->pd_events |= events;
	mutex_spinunlock(&php->ph_lock, s);
	return 0;
}
#endif

/*
 * This function removes a polldat structure from a pollhead.
 * Recalculate the events after removal of the specified polldat.
 *
 * Note that we don't hold off interrupts in this routine on a
 * single cpu machine.  The reason is that we only update the
 * linked list on a pollhead in user context (never in interrupt
 * context).  When we delete ourselves, we do it with a single
 * store on the linked list and then update the event masks at
 * the very end.  This insures that anyone sharing this pollhead
 * with us will still get the events they want during the delete.
 */
static __inline void
_polldel(struct polldat *pdp)
{
	struct polldat *p, *p2, **pp;
	struct pollhead *php;
	int events = 0, s;

	ASSERT(pdp != NULL);
	ASSERT(pdp->pd_headp != NULL);
	ASSERT(private.p_kstackflag != PDA_CURINTSTK);
	php = pdp->pd_headp;
	s = mp_mutex_spinlock(&php->ph_lock);
	pp = &php->ph_list;
	if ((p = php->ph_list) == NULL) {
		/* XXX this should never happen -- remove at start of 6.5.4 */
		goto out;
	}
	do {
		p2 = p->pd_next;
		if (p == pdp) {
			*pp = p2;
		} else {
			events |= p->pd_events;
			pp = &p->pd_next;
		}
		p = p2;
	} while (p);
	php->ph_events = events;
	pdp->pd_headp = NULL;					/* DEBUG */
 out:
	mp_mutex_spinunlock(&php->ph_lock, s);
}

/*
 * XXX We export an out-of-line version of polladd() and polldel() so it may
 * XXX be called from the DCE DFS code.  This is a *really ugly* situation
 * XXX and should be fixed.  See bug #550656, ``DCE DFS vs. select/poll
 * XXX [private] interfaces.''
 */
int
polladd(struct pollhead *php, short events, unsigned int gen,
	void *arg, struct polldat *pdp)
{
	return _polladd(php, events, gen, arg, pdp);
}

void
polldel(struct polldat *pdp)
{
	_polldel(pdp);
}

/*
 * This function applies polldel() to an array of polldat pointers().  The
 * start pointer points to the first polldat struct to be deleted.  The
 * end pointer points one past the last polldat struct to be deleted.
 */
static void
polldelvec(struct polldat *start, struct polldat *end)
{
	while (start <= --end)
		_polldel(end);
}

/*
 * Poll file descriptors for interesting events.
 */

struct polla {
	struct pollfd *fdp;
	usysarg_t nfds;
	sysarg_t timo;
};

/* ARGSUSED */
int
poll(struct polla *uap, rval_t *rvp)
{
	struct pollfd p[NPOLLFILE], *pollp;
	int psize;
	int error;
	int fdcnt = 0;
	struct rlimit rlim;
	struct timeval timeval, *tv;

	curuthread->ut_acct.ua_syscps++;
	VPROC_GETRLIMIT(curvprocp, RLIMIT_NOFILE, &rlim);
	if (uap->nfds > rlim.rlim_cur)
		return EINVAL;

	/*
	 * Allocate space for the pollfd array.  Then copy in
	 * the pollfd array from user space.
	 */
	if (uap->nfds != 0) {
		psize = uap->nfds * sizeof(struct pollfd);
		if (uap->nfds > NPOLLFILE) {
			if ((pollp = kmem_zalloc(psize, KM_SLEEP)) == NULL)
				return EAGAIN;
		} else {
			pollp = p;
		}
		if (copyin((caddr_t)uap->fdp, (caddr_t)pollp, psize)) {
			if (uap->nfds > NPOLLFILE)
				kmem_free((caddr_t)pollp, psize);
			return EFAULT;
		}
	}

	if (uap->timo < 0)
		tv = NULL;
	else {
		/* get timeval.tv_sec first to avoid overflow */
		timeval.tv_sec = uap->timo / 1000;	/* msecs to secs */
		timeval.tv_usec = (uap->timo - timeval.tv_sec * 1000) * 1000;
		tv = &timeval;
	}

	error = dopoll(uap->nfds, pollp, tv, &fdcnt, 0);

	/*
	 * Don't return EBADF since poll(2) just uses the POLLNVAL flag.
	 */
	if (error == EBADF) {
		error = 0;
	}

	if (error == 0) {
		/*
		 * Copy out the events and return the fdcnt to the user.
		 */
		rvp->r_val1 = fdcnt;
		if (uap->nfds != 0)
			if (copyout((caddr_t)pollp, (caddr_t)uap->fdp, psize))
				error = EFAULT;
	}
	_SAT_PFD_READ2(pollp, uap->nfds, error);
	if (uap->nfds > NPOLLFILE)
		kmem_free((caddr_t)pollp, psize);
	return error;
}

static int
dopoll(int nfds, 
	struct pollfd *pollp, 
	struct timeval *atv,
	int *fdcnt,
	int is_xpg4)
{
	uthread_t *ut = curuthread;
	struct polldat *darray, *curdat;
	struct pollvobj *pollvobj, *pvobj;
	toid_t tid, (*callback)(void (*)(), void *, long, ...);
	struct pollfd *pfd;
	int size, dsize, vsize;
	int s, i, rotor, rwonce, vcnt, rv, mayblock;
	unsigned int selticks, gen;
	int error;

	if (atv) {
		if (timerisset(atv)) {
			/* check to make sure all criteras met,
			 * before calling fast_timeout
			 */
			if (kt_needs_fastimer(atv, curthreadp)) {
				/* compute interval in fast tick
				 * need to add 1 to keep from returning early
				 */
				selticks = fasthzto(atv) + 1;
				callback = fast_timeout;
			} else {	
				/* compute interval in normal ticks 
				 * need to add 1 to keep from returning early
				 */
				selticks = hzto(atv) + 1;
				callback = timeout_nothrd;
			}
			if (selticks == 0) 
				selticks = 1;
		} else
			selticks = 0;
	}

	/*
	 * Create and initialize the file ptr and polldat data structures.
	 */
	if (nfds > NPOLLFILE) {
		dsize = nfds * sizeof(struct polldat);
		vsize = nfds * sizeof(struct pollvobj);
		size = dsize + vsize;	/* for kmem_free below */
		darray = (struct polldat *) kmem_zalloc(size, KM_SLEEP);
		ASSERT(darray);
	} else {
		dsize = NPOLLFILE * sizeof(struct polldat);
		vsize = NPOLLFILE * sizeof(struct pollvobj);
		size = 0;		/* no kmem_free below */
		if (ut->ut_polldat == NULL)
			ut->ut_polldat = (struct polldat *)
				kern_calloc(1, dsize + vsize);
		darray = ut->ut_polldat;
	}

	/*
	 * Pointer to array of pollvobj structures. 
	 */
	pollvobj = (struct pollvobj *)((char *)darray + dsize);

	/*
	 * Convert fd's to vnode or vocket pointers, and perform fd 
	 * error checking.
	 */
	error = fdt_select_convert(pollp, nfds, pollvobj);

	/*
	 * If select syscall and bad arguments passed in, get out now
	 */
	if (error && ut->ut_syscallno == SYS_select - SYSVoffset) {
		#pragma mips_frequency_hint NEVER
		if (size)
			kmem_free((caddr_t)darray, size);
		*fdcnt = 0;
		return error;
	}

	/*
	 * Retry scan of fds until an event is found or until the
	 * timeout is reached.
	 */
	vcnt = 0;
	ut->ut_pollrotor = 0;

retry:
	/*
	 * Initialize polladd() polldat struct cursor.  We hook successive
	 * polldat's onto pollhead chains as we pass through the array of
	 * objects to poll.  We stop doing this if we ever get a successful
	 * poll response from an object.  We also don't bother doing this if
	 * a timeout has been specified and there are 0 selticks left since
	 * we know we aren't going to block.
	 */
	curdat = darray;
	mayblock = !atv || selticks;

	/*
	 * We use a rotor here to cut down on the amount of time spent
	 * re-adding & deleteing the poll entries on wakeups.  It is likely
	 * that the rotor points to an FD with revents for us.  Note that
	 * when we wakeup, rotor equals 0 if no pollwakeup() has happened,
	 * or non-zero with a pollp index plus 1.  Note that we don't need
	 * to take the poll rotor lock here because we can not be the target
	 * of any pollwakeup() or polltime() call at this point.
	 */
	rotor = ut->ut_pollrotor & ROTORFDMASK;
	ASSERT(rotor <= nfds);
	rwonce = ut->ut_pollrotor & ROTORMULTI;
	ut->ut_pollrotor = 0;
	if (rotor)
		rotor--;

	pfd = &pollp[rotor];
	pvobj = &pollvobj[rotor];
	for (rv = 0, i = 0; i < nfds; i++) {
		struct pollhead *php;
quicktry:
		php = NULL;
		if (pvobj->pv_vobj != NULL) {
			/*
			 * pvobj refers to either a vnode or a vsocket as
			 * indicated by the vtype (set by fdt_select_convert):
			 *	1 => vnode
			 *	2 => vsocket
			 */
			ASSERT(pvobj->pv_vtype == 1 || pvobj->pv_vtype == 2);
			if (pvobj->pv_vtype == 1) {
				vnode_t *vp = (vnode_t *)pvobj->pv_vobj;
				static int pollheadalert = 0;
				gen = 0;
				VOP_POLL(vp, pfd->events, vcnt,
					 &pfd->revents, &php, &gen, error);
				/*
				 * A little bit of logic to try to catch device
				 * driver poll() routines which aren't snap-
				 * shotting the pollhead generation number.
				 * This portion of the poll() interface was
				 * added in IRIX 6.5 and this code can probably
				 * be safely removed in a later version of the
				 * OS.  We don't do it for the VSOP_SELECT()
				 * below because there's really only one socket
				 * implementation so far and the entire vsocket
				 * interface is new with IRIX 6.5.
				 */
				if (gen == 0 && php != NULL
				    && POLLGEN(php) > 1000
				    && pollheadalert == 0) {
					#pragma mips_frequency_hint NEVER
					pollheadalert = 1;
					cmn_err_tag(320,CE_ALERT, "dopoll: pollhead"
						    " generation not set by driver"
						    " %v?\n", vp->v_rdev);
				}

				/*
				 * Hack for xpg4: xopen specifies that a poll
				 * for exceptional condition on a regular file
				 * always returns true (as does select for
				 * READ/WRITE)
				 */
				if (is_xpg4) {
					#pragma mips_frequency_hint NEVER
					if ((pfd->events & (POLLPRI|POLLRDBAND))
					    && vp->v_type == VREG)
						pfd->revents |= pfd->events &
							(POLLPRI|POLLRDBAND);
				}
			} else {
				vsock_t *vso = (vsock_t *)pvobj->pv_vobj;
				VSOP_SELECT(vso, pfd->events, vcnt,
					    &pfd->revents, &php, &gen, error);
			}
			if (error)
				goto pollout;
		}
		if (pfd->revents) {
			vcnt++;
			mayblock = 0;
			rv |= pfd->revents & POLLNVAL;
			if (rwonce == ROTORONCE)
				goto pollout;
		} else if (php == NULL) {
			/* if no pollhead, disable ROTORONCE opt */
			rwonce = -1;
		} else if (mayblock) {
			curdat->pd_rotorhint = rotor;
			if (_polladd(php, pfd->events, gen,
				     (void *)ut, curdat) != 0) {
				/*
				 * A pollwakeup() was done after the poll probe.
				 * We need to retry to recover from this race
				 * between pollwakeup() and polladd().
				 */
				goto quicktry;
			}
			curdat++;
		}
		if (++rotor >= nfds) {
			pfd = pollp;
			pvobj = pollvobj;
			rotor = 0;
		} else {
			++pfd;
			++pvobj;
		}
	}
	if (!mayblock)
		goto pollout;

	s = mutex_spinlock(&ut->ut_pollrotorlock);
	/*
	 * If we're going to block and any of our objects didn't give us a
	 * pollhead to attach to we have to disable the ROTORONCE optimization
	 * since we'll need to scan the entire array again when we unblock.
	 */
	if (rwonce < 0)
		ut->ut_pollrotor |= ROTORMULTI;

	/*
	 * Since we hold the lock, anybody checking to see if we are on the
	 * poll synchronization variable will wait until we get there.
	 */
	if ((ut->ut_pollrotor & ROTORFDMASK) != 0) {
		/* a pollwakeup we care about has happened */
		mutex_spinunlock(&ut->ut_pollrotorlock, s);
		polldelvec(darray, curdat);
		goto retry;
	}

	/*
	 * If we get here, the poll of fds was unsuccessful.  Set a timeout,
	 * if specified, and block until some fd becomes readable,
	 * writeable, or gets an exception.
	 */
	if (atv) {
		ASSERT(selticks);
		tid = callback(polltime, ut, selticks);
	}
	rv = sv_wait_sig(&UT_TO_KT(ut)->k_timewait,
		(PZERO+1)|TIMER_SHIFT(AS_SELECT_WAIT),
		&ut->ut_pollrotorlock, s);

	/*
	 * Delete the timeout if we posted one.  If it went off
	 * while we were asleep, we get back a selticks value of
	 * zero.  In that case, just exit dopoll().  Otherwise,
	 * save the time tick value returned for the next loop.
	 */
	if (atv && (selticks = untimeout(tid)) == 0)
		goto pollout;
	if (rv != -1) {
		polldelvec(darray, curdat);
		goto retry;
	}
	error = EINTR;

pollout:
	/*
	 * If select syscall and bad arguments passed in, return EBADF
	 */
	if (rv == POLLNVAL && ut->ut_syscallno == SYS_select - SYSVoffset) {
		#pragma mips_frequency_hint NEVER
		error = EBADF;
	}

	/*
	 * Poll cleanup code.
	 */
	if (curdat > darray)
		polldelvec(darray, curdat);
	if (size)
		kmem_free((caddr_t)darray, size);

	*fdcnt = vcnt;

	return error;
}


/*
 * Select system call.
 *
 */
struct selecta {
	usysarg_t	nd;
	fd_set		*in, *ou, *ex;
	struct timeval	*tv;
};

static int
k_select(struct selecta *uap, rval_t *rvp, int is_xpg4)
{
	struct pollfd po[NPOLLFILE], *pollp;
	struct pollfd *pfd;
	int nfds = 0, nd = uap->nd, maxnd = fdt_nofiles();
	int fdcnt = 0, psize = 0, error = 0;
	struct timeval atv;

	curuthread->ut_acct.ua_syscps++;

	/*
	 * clamp the max # of fds to the max # open fildes,
	 * forgiving, if slightly wrong
	 */
	if (nd > maxnd)
		nd = maxnd;

	/*
	 * timeval structure processing
	 */
	if (uap->tv) {
		if (COPYIN_XLATE(uap->tv, &atv, sizeof atv,
				irix5_to_timeval_xlate, get_current_abi(), 1)) {
			#pragma mips_frequency_hint NEVER
			error = EFAULT;
			goto out;
		}
		if (itimerfix(&atv)) {
			#pragma mips_frequency_hint NEVER
			error = EINVAL;
			goto out;
		}

		/* block indefinitely */
		if (atv.tv_sec > 1000000) {
			#pragma mips_frequency_hint NEVER
			uap->tv = 0;
		}
	}

	/*
 	 * For each fd, if any bits are set convert them into
	 * the appropriate pollfd struct.
	 */
	{
	    ufd_mask_t all, in, ou, ex, bit, *inp, *oup, *exp;
	    int events, i, j;

	    /*
	     * Load the mask pointers
	     */
	    inp = (ufd_mask_t *)uap->in;
	    oup = (ufd_mask_t *)uap->ou;
	    exp = (ufd_mask_t *)uap->ex;

	    for (pfd = pollp = po, i = 0; i < nd; i += NFDBITS) {

	        /* note: sfu32 sets error to EFAULT on fault */
		#define	getbits(ptr, var) \
			if (ptr) { \
				var = sfu32(&error, ptr++); \
			}

		    /*
		     * Get next set of bits and clamp (mask) down to uap->nd
		     */
		    in = ou = ex = 0;
		    getbits(inp, in);
		    getbits(oup, ou);
		    getbits(exp, ex);
		    all = in | ou | ex;
		    if ((nd - i) < NFDBITS) {
			all &= (1 << (nd - i)) - 1;
		    }
		#undef	getbits

		/*
		 * Loop until we run out of set bits
		 */
		for (bit = 1, j = i; all != 0; all >>= 1, bit <<= 1, j++) {
			if (all & 1) {
				/*
				 * If we run out of local pfd's, allocate a
				 * max sized buffer to hold all of them.
				 *
				 * We postpone this based on the idea that
				 * a lot of select calls have holey bit args,
				 * which is the case with the X server.
				 */
				if (nfds == NPOLLFILE) {
					psize = nd * sizeof(struct pollfd);
					pollp = kmem_alloc(psize, KM_SLEEP);
					bcopy(po, pollp, sizeof po);
					pfd = &pollp[NPOLLFILE];
				}

				/*
				 * Create new pfd, fill in all fields
				 */
				pfd->fd = j;
				pfd->revents = events = 0;
				if (bit & in)
					events |= POLLRDNORM;
				if (bit & ou)
					events |= POLLWRNORM;
				if (bit & ex)
					events |= POLLPRI|POLLRDBAND;
				pfd->events = events;
				pfd++;
				nfds++;
			}
		}
	    }
	}

	/* If a copy error happened, exit out now */
	if (error)
		goto out;
	
	/* Call dopoll to check the pollfds for events */
	error = dopoll(nfds, pollp, uap->tv ? &atv : NULL, &fdcnt, is_xpg4);

	/* Reverse pollfd info back into bit strings (time critical!) */
	if (!error) {
	    ufd_mask_t in = 0, ou = 0, ex = 0, bit;
	    fd_set *inp = uap->in, *oup = uap->ou, *exp = uap->ex;
	    int revents, n, j, i;

	    /* note: spu32 sets error to EFAULT on fault */
	    #define putbits(ptr, var) \
		    if (ptr) { \
			(void) spu32(&error, &ptr->fds_bits[j], var); \
		    }

		/* this code assumes fd's are always increasing!!! */
		for (pfd = pollp, n = 0, j = -1; n < nfds; n++, pfd++) {
		    ASSERT(pfd->fd >= 0 && pfd->fd <= nd);
		    i = (unsigned int)pfd->fd / NFDBITS;
		    bit = 1 << ((unsigned int)pfd->fd % NFDBITS);
		    if (i != j) {
			/* save previous set of bits */
			if (j != -1) {
			    putbits(inp, in);
			    putbits(oup, ou);
			    putbits(exp, ex);
			    in = ou = ex = 0;
			}

			/* setup for next set */
			j = i;
		    }
		    if ((revents = pfd->revents) != 0) {
			if (revents & (POLLHUP|POLLERR))
			    revents |= pfd->events;
			if (revents & POLLRDNORM)
			    in |= bit;
			if (revents & POLLWRNORM)
			    ou |= bit;
			if (revents & (POLLPRI|POLLRDBAND)) 
			    ex |= bit;
		    }
		}

		/* save last set on the way out */
		if (nfds) {
		    ASSERT(j >= 0);
		    putbits(inp, in);
		    putbits(oup, ou);
		    putbits(exp, ex);
		}
		rvp->r_val1 = fdcnt;

	    #undef putbits
	}

out:
	_SAT_PFD_READ2(pollp, nfds, error);

	if (psize) {
		kmem_free((caddr_t)pollp, psize);
	}

	return error;
}

int
select(struct selecta *uap, rval_t *rvp)
{
	return k_select(uap, rvp, 0);
}

int
xpg4_select(struct selecta *uap, rval_t *rvp)
{
	/* flag arg of 1 indicates xpg4 calling */
	return k_select(uap, rvp, 1);
}

/*
 * Wake up processes sleeping in poll/select
 * We offer some protection from simultaneous poll and pollwakeup's
 * by threading them with polllock 
 *
 * Handles both local cell threads and remote cell threads.  The local
 * case optimized.
 */
void
pollwakeup(struct pollhead *php, short event)
{
	struct polldat *pdp;
	int alwaysrun = (event & (POLLHUP|POLLERR));
	int s = mutex_spinlock(&php->ph_lock);

	/*
	 * First process local cell threads
	 */
	for (pdp = php->ph_list; pdp; pdp = pdp->pd_next) {
		if (alwaysrun || (pdp->pd_events & event)) {
			pollwakeup_thread((uthread_t *)pdp->pd_arg,
					  pdp->pd_rotorhint);
			if (event & POLLWAKEUPONE)
				break;
		}
	}

	php->ph_gen++;

#if CELL_IRIX
	if (pdp = php->ph_next) {
		struct polldat *pdp2, **spdp;
		vproc_t *vpr;
		struct polldat *pl_next;
		/*
		 * Now process any distributed threads
		 *
		 */
		
		pl_next = NULL;
		spdp = &php->ph_next;
		while (pdp) {
			pdp2 = pdp->pd_next;
			if (alwaysrun || (pdp->pd_events & event)) {
				*spdp = pdp->pd_next;
				pdp->pd_next = pl_next;
				pl_next = pdp;
			} else {
				spdp = &pdp->pd_next;
			}
			pdp = pdp2;
		}

		mutex_spinunlock(&php->ph_lock, s);

		/*
		 * Now deliver accumulated poll events
		 * No longer holding lock because we can sleep
		 */
		if (pdp = pl_next) {
			while (pdp) {
				vpr = VPROC_LOOKUP(pdp->pd_pid);
				if (vpr) {
					VPROC_POLL_WAKEUP (vpr, pdp->pd_tid, 
						0);
					VPROC_RELE(vpr);
				}
				pdp2 = pdp->pd_next;
				kmem_free(pdp, sizeof(*pdp));
				pdp = pdp2;
			}
		}
	} else {
		mutex_spinunlock(&php->ph_lock, s);
	}
#else
	mutex_spinunlock(&php->ph_lock, s);
#endif
}

/*
 * The client side of pollwakeup
 */
void
pollwakeup_thread(uthread_t *ut, short pd_rotorhint)
{
	int s;
	/*
	 * Inform a process thread that an event being
	 * polled for has occurred.
	 */
	s=mutex_spinlock(&ut->ut_pollrotorlock);

	/*
	 * Have we recorded a rotor wakeup hint? (or disabled?)
	 */
	if ((ut->ut_pollrotor & ROTORFDMASK) != 0)
		ut->ut_pollrotor |= ROTORMULTI;
	else
		/*
		 * Record hint of first file descriptor in
                 * rotor.
		 */
		ut->ut_pollrotor = (ROTORONCE |
				    (pd_rotorhint + 1));

	sv_signal(&UT_TO_KT(ut)->k_timewait);
	mutex_spinunlock(&ut->ut_pollrotorlock, s);
}

#ifdef CELL_IRIX
/*
 * Wakeup any threads in a manner to cause them to retry polling.
 * This is used to relocate a pollhead.
 */
void
pollrestart(struct pollhead *php)
{
	pollwakeup(php, POLLERR);
}
#endif
