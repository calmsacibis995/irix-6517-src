/*
 * Copyright (c) 1997 Silicon Graphics, Inc.
 */
#ident "$Revision: 1.98 $"

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/driver.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kabi.h>
#include <sys/ksignal.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <ksys/vpgrp.h>
#include <ksys/vsession.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/vnode.h>
#include <ksys/vfile.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/strstat.h>
#include <sys/poll.h>
#include <sys/sema.h>

#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/cmn_err.h>
#include <sys/strmp.h>
#include <limits.h>
#include <fifofs/fifonode.h>
#include <sys/ddi.h>
#include <sys/runq.h>
#include <sys/idbgactlog.h>
#include <sys/mload.h>
#include <sys/kthread.h>

#define	szlong_m1		(sizeof(long)-1)
#define SALIGN(p)               (char *)(((__psint_t)p+1) & ~1)
#define IALIGN(p)               (char *)(((__psint_t)p+3) & ~3)
#define LALIGN(p)               (char *)(((__psint_t)p+szlong_m1) & ~szlong_m1)
#define SNEXT(p)                (char *)((__psint_t)p + sizeof (short))
#define INEXT(p)                (char *)((__psint_t)p + sizeof (int))
#define LNEXT(p)                (char *)((__psint_t)p + sizeof (long))

#ifndef MAXCPUS
/* If a maximum isn't defined, define one ourselves. */
#define MAXCPUS	128
#endif

#ifdef DEBUG
#define SAVE_STRALLOC_ADDRESS(sp, addr) \
	((struct shinfo *)(sp))->sh_alloc_func = (void (*)())(addr)
#define SAVE_STRFREE_ADDRESS(sp, addr) \
	((struct shinfo *)(sp))->sh_freeb_func = (void (*)())(addr)
#else
#define SAVE_STRALLOC_ADDRESS(sp, addr)
#define SAVE_STRFREE_ADDRESS(sp, addr)
#endif /* DEBUG */

mutex_t mux_mutex;

/*
 * WARNING:
 * The variables and routines in this file are private, belonging
 * to the STREAMS subsystem.  These should not be used by modules
 * or drivers.  Compatibility will not be guaranteed. 
 */
#define ncopyin(A, B, C, D)	copyin(A, B, C)		/* temporary */
#define ncopyout(A, B, C, D)	copyout(A, B, C)	/* temporary */

/*
 * Global Variables
 * ================
 *
 * All of the following global variables are protected on MP systems by the
 * global resource spinlock (see str_resource_lock in strmp.h/strmp.c).
 * "!" marks the protected fields.
 *
 * Id value used to distinguish between different multiplexor links.
 */
STATIC int lnk_id;		/* ! */

/*
 * Queue scheduling control variables.
 */
char qrunflag;			/* ! set iff queues are enabled */
				/* XXXrs - all races appear benign? */
char queueflag;			/* ! set iff inside queuerun() */
				/* XXXrs there's some ugly code in tpisocket.c
				 * that tries to use this flag.  It's currently
				 * ifdef-ed out.  It should stay that way!
				 */
struct queue *qhead = NULL;	/* ! head of queues to run */
struct queue **qtailp = &qhead; /* !  pointer to tail of queues to run */

char strbcwait;			/* ! bufcall functions waiting */
char strbcflag;			/* ! bufcall functions ready to go */
				/* XXXrs - all races appear benign? */
struct bclist strbcalls;	/* ! list of waiting bufcalls */

/*
 * STRHOLD feature control variables
 * Protected by the global Streams resource lock
 */
struct stdata *strholdhead = NULL;		/* !streams with held msgs */
struct stdata **strholdtailp = &strholdhead;	/* !pointer to tail */
int strholdflag;				/* !strhold timeout pending */

STATIC struct mux_node *mux_nodes;	/* ! mux info for cycle checking */

#define SECACHE 10

STATIC struct seinfo *sefreelist; 	/* ! */
STATIC struct seinfo Secache[SECACHE];	/* ! emergency cache of seinfo's */
					/*   in case memory runs out */
STATIC struct seinfo *secachep;		/* ! cache list head */

struct strinfo Strinfo[NDYNAMIC];	/* ! dynamic Streams resource info */

struct queue *cpu_cur_q[MAXCPUS];

/*
 * Forward referenced procedures
 */
static void mux_rmvedge(stdata_t *, int);
static void strunbcall(int, struct kthread *);
static void strqbuf(struct stdata *);
static void adjfmtp(char **, mblk_t *, int);
static void freeband(qband_t *);

/*
 * External referenced procedures
 */
extern void str_stats_ops(cpuid_t, int, int, int);
     
/*
 * Global and statics declarations
 */
static int strinited = 0;

/*
 * Init routine run from main at boot time.
 * This contains some machine dependent code.
 * spl's probably are not necessary, so they are not used.
 */
void
strinit(void)
{
	int i;

	if (strinited++ != 0) {
		return;
	}
	str_mp_init();

	/* obtain newly created monitor */
	STREAMS_LOCK();

	/*
	 * set up seinfo cache (if memory runs out, we need a few of
	 * these so things have a chance to recover)
	 */
	sefreelist = NULL;
	for (i = 0; i < SECACHE; ++i) {
		Secache[i].s_next = secachep;
		secachep = &Secache[i];
	}
	/*
	 * Set up mux_node structures.
	 */
	if ((mux_nodes = kmem_alloc((sizeof(struct mux_node) * cdevmax),
				    KM_NOSLEEP)) == NULL) {
		cmn_err_tag(158,CE_PANIC, "Could not allocate space for mux_nodes\n");
	}
	for (i = 0; i < cdevmax; i++) {
		mux_nodes[i].mn_ddt = 0;
		mux_nodes[i].mn_indegree = 0;
		mux_nodes[i].mn_originp = NULL;
		mux_nodes[i].mn_startp = mux_nodes[i].mn_outp = NULL;
		mux_nodes[i].mn_flags = 0;
	}
	mutex_init(&mux_mutex, MUTEX_DEFAULT, "strmux");
	STREAMS_UNLOCK();
	/*
	 * Duart code may call strinit() "early", before the callout
	 * tables are ready. So we must ensure the call to setup the
	 * Stream giveback timeout gets called later from main.
	 */
	return;
}

/*
 * Send SIGPOLL signal to all processes registered on the given signal
 * list that want a signal for the specified event.  Always called at
 * splstr().
 */
void
strsendsig(register struct strevent *siglist, register int event, long data)
{
	struct strevent *sep;
	k_siginfo_t info, *sigp = NULL;

	info.si_signo = SIGPOLL;
	info.si_errno = 0;
	for (sep = siglist; sep; sep = sep->se_next) {
		if (sep->se_events & event) {
			switch (event) {
			case S_INPUT:
				info.si_code = POLL_IN;
				info.si_band = data;
				sigp = &info;
				goto addq;

			case S_OUTPUT:
				info.si_code = POLL_OUT;
				info.si_band = data;
				sigp = &info;
				goto addq;

			case S_HIPRI:
				info.si_code = POLL_PRI;
				info.si_band = 0;
				sigp = &info;
				goto addq;

			case S_MSG:
				info.si_code = POLL_MSG;
				info.si_band = data;
				sigp = &info;
				goto addq;

			case S_ERROR:
				info.si_code = POLL_ERR;
				info.si_band = 0;
				info.si_errno = (int)data;
				sigp = &info;
				goto addq;

			case S_HANGUP:
				info.si_code = POLL_HUP;
				info.si_band = 0;
				sigp = &info;

			case S_RDBAND:
addq:
				if (sep->se_events & S_BANDURG) {
					sigtopid(sep->se_pid, SIGURG,
						SIG_ISKERN, 0, 0, sigp);
					break;
				}
				/* FALLTHROUGH */

			case S_RDNORM:
			case S_WRBAND:
				sigtopid(sep->se_pid, SIGPOLL, SIG_ISKERN,
					0, 0, sigp);
				break;

			default:
				cmn_err_tag(159,CE_PANIC,
				    "strsendsig: unknown event %x\n", event);
			}
		}
	}
}

/*
 * Attach a stream device or module.
 *
 * qp is a read queue; the new queue pair goes in "below" (downstream
 * from) qp so that the new queue's next read ptr points to qp, and the
 * next write ptr of the write queue associated with qp points to the
 * write queue of the new queue pair.
 *
 * Return 0 on success, or a non-zero errno on failure.
 */
int
qattach(register queue_t *qp,
	dev_t *devp,
	int flag,
	int table,
	void *tabentry, /* either a cdevsw or fmodsw entry */
	cred_t *crp)
{
	register queue_t *rq;
	struct streamtab *qinfop;
	int sflg, error = 0;

	if ((rq = allocq()) == NULL)
		return (ENXIO);

	if (table == CDEVSW) {
		struct cdevsw *my_cdevsw = tabentry;

		if (*my_cdevsw->d_flags & D_OBSOLD)
			return ENOTSUP;
		/* If its a loadable module, increment the refcnt. */
		cdhold(my_cdevsw);
		qinfop = my_cdevsw->d_str;
		sflg = 0;
	} else {
		struct fmodsw *my_fmodsw = tabentry;

		ASSERT(table == FMODSW);
		if (*my_fmodsw->f_flags & D_OBSOLD)
			return ENOTSUP;
		/* If its a loadable module, increment the refcnt. */
		fmhold(my_fmodsw-fmodsw);
		qinfop = my_fmodsw->f_str;
		sflg = MODOPEN;
	}

	rq->q_next = qp;
	WR(rq)->q_next = WR(qp)->q_next;
	if (WR(qp)->q_next)
		OTHERQ(WR(qp)->q_next)->q_next = rq;
	WR(qp)->q_next = WR(rq);
	rq->q_monpp = WR(rq)->q_monpp = qp->q_monpp;
	ASSERT(qp->q_strh);
	rq->q_strh  = WR(rq)->q_strh  = qp->q_strh;
	setq(rq, qinfop->st_rdinit, qinfop->st_wrinit);
 	rq->q_flag |= QWANTR;
	WR(rq)->q_flag |= QWANTR;

	/*
	 * Open the attached module or driver.
	 */
	if (error = (*rq->q_qinfo->qi_qopen)(rq, devp, flag, sflg, crp)) {
		qdetach(rq, 0, 0, crp);
		return (error);
	}
	/*
	 * Was the device modified on open?  If so, re-validate the
	 * major device number, get the "new" streamtab, and reset
	 * the queue information.
	 */ 
	if (table == CDEVSW) {
		struct cdevsw *my_cdevsw = get_cdevsw(*devp);

		if ((void *)my_cdevsw != tabentry) {
			if (!cdvalid(my_cdevsw)||!(qinfop = my_cdevsw->d_str)){
				qdetach(rq, 0, 0, crp);
				return (ENXIO);
			}
			setq(rq, qinfop->st_rdinit, qinfop->st_wrinit);
		}
	}
	return (0);
}

/*
 * Detach a stream module or device.
 * If clmode == 1 then the module or driver was opened and its
 * close routine must be called.  If clmode == 0, the module
 * or driver was never opened or the <open failed, and so its close
 * should not be called.
 */
void
qdetach(register queue_t *rq, int clmode, int flag, cred_t *crp)
{
	register int s2;
	register queue_t *wq;

        ASSERT(rq->q_flag&QREADR);
        wq = WR(rq);

	if (clmode) {
#ifdef _EAGER_RUNQUEUES
		if (qready())
			runqueues();
#endif /* _EAGER_RUNQUEUES */
		(*rq->q_qinfo->qi_qclose)(rq, flag, crp);
		flushq(rq, FLUSHALL);
		flushq(wq, FLUSHALL);
	}

	/* If this is a loadable module, decrement the refcnt */
	mlqdetach (rq);

	LOCK_STR_RESOURCE(s2);
        /*
         * If this queue has held messages, remove its associated
         * stream head from the list of streams with held messages.
         */
        if (wq->q_flag & QHLIST) {
                register struct stdata *sp = strholdhead;
                register struct stdata **prevp = &strholdhead;
                struct stdata *stp = (struct stdata *)wq->q_ptr;

                while (sp) {
                        if (sp == stp) {
                                *prevp = sp->sd_hnext;
                                wq->q_flag &= ~QHLIST;
                        }
                        else
                                prevp = &sp->sd_hnext;
                        sp = sp->sd_hnext;
                }
                strholdtailp = prevp;

                ASSERT(!(wq->q_flag & QHLIST));
        }

	UNLOCK_STR_RESOURCE(s2);

	if (wq->q_next)
		backq(rq)->q_next = rq->q_next;
	if (rq->q_next)
		backq(wq)->q_next = wq->q_next;

	streams_mpdetach(rq);
	freeq(rq);
}

/*
 * This function is placed in the callout table when messages have been
 * held in strwrite, in case the hope of being able to add more data is
 * unrealized.
 */
void
strrelease()
{
	int s;

	LOCK_STR_RESOURCE(s);
	setqsched();
	strholdflag = 0;
	UNLOCK_STR_RESOURCE(s);
	return;
}

/*
 * This function is placed in the callout table to wake up a process
 * waiting to close a stream that has not completely drained.
 */
void
strtime(struct stdata *stp)
{
	if (stp->sd_flag & STRTIME) {
		stp->sd_flag &= ~STRTIME;
		wakeup(stp->sd_wrq);
	}
	return;
}

/*
 * This function is placed in the callout table to wake up all
 * processes waiting to send an ioctl down a particular stream,
 * as well as the process whose ioctl is still outstanding.  The
 * process placing this function in the callout table will remove
 * it if he gets control of the ioctl mechanism for the stream -
 * this should only run if there is a failure.  This wakes up
 * the same processes as str3time below.
 */
void
str2time(struct stdata *stp)
{
	if (stp->sd_flag & STR2TIME) {
		stp->sd_flag &= ~STR2TIME;
		wakeup(&stp->sd_iocwait);
	}
	return;
}

/*
 * This function is placed in the callout table to wake up the
 * process that has an outstanding ioctl waiting acknowledgement
 * on a stream, as well as any processes waiting to send their
 * own ioctl messages.  It should be removed from the callout table
 * when the acknowledgement arrives.  If this function runs, it
 * is the result of a failure.  This wakes up the same processes
 * as str2time above.
 */
void
str3time(struct stdata *stp)
{
	if (stp->sd_flag & STR3TIME) {
		stp->sd_flag &= ~STR3TIME;
		wakeup(stp);
	}
	return;
}

/*
 * Put ioctl data from user land to ioctl buffers.  Return non-zero
 * errno for failure, 1 for success.
 */
int
putiocd(register mblk_t *bp,
	mblk_t *ebp,
	caddr_t arg,
	int copymode,
	int flag,
	char *fmt)
{
	register mblk_t *tmp;
	register int count, n;
	mblk_t *obp = bp;
	int error = 0;

	/* XXXrs - needed? ASSERT(STREAMS_LOCKED); */

	if (bp->b_datap->db_type == M_IOCTL)
		count = ((struct iocblk *)bp->b_rptr)->ioc_count;
	else {
		ASSERT(bp->b_datap->db_type == M_COPYIN);
		count = ((struct copyreq *)bp->b_rptr)->cq_size;
	}
	/*
	 * strdoioctl validates ioc_count, so if this assert fails it
	 * cannot be due to user error.
	 */
	ASSERT(count >= 0);

	while (count) {
		n = MIN(MAXIOCBSZ, count);
		if (flag == SE_SLEEP) {
			while (!(tmp = allocb(n, BPRI_HI))) {
				if (error = strwaitbuf(n, BPRI_HI))
					return (error);
			}
		} else if (!(tmp = allocb(n, BPRI_HI)))
			return (EAGAIN);
		error = STREAMS_COPYIN((caddr_t)arg, (caddr_t)tmp->b_wptr,
				       n, fmt, copymode);
		if (error) {
			freeb(tmp);
			return (error);
		}
		if (fmt && (count > MAXIOCBSZ) && (copymode == U_TO_K))
			adjfmtp(&fmt, tmp, n);
		arg += n;
		tmp->b_datap->db_type = M_DATA;
		tmp->b_wptr += n;
		count -= n;
		bp = (bp->b_cont = tmp);
	}

	/*
	 * If ebp was supplied, place it between the
	 * M_IOCTL block and the (optional) M_DATA blocks.
	 */
	if (ebp) {
		ebp->b_cont = obp->b_cont;
		obp->b_cont = ebp;
	}
	return (0);
}

/*
 * Copy ioctl data to user-land.  Return non-zero errno on failure,
 * 0 for success.
 */
int
getiocd(register mblk_t *bp, caddr_t arg, int copymode, char *fmt)
{
	register int count, n;
	int error;

	/* XXXrs - needed? ASSERT(STREAMS_LOCKED); */

	if (bp->b_datap->db_type == M_IOCACK)
		count = ((struct iocblk *)bp->b_rptr)->ioc_count;
	else {
		ASSERT(bp->b_datap->db_type == M_COPYOUT);
		count = ((struct copyreq *)bp->b_rptr)->cq_size;
	}
	ASSERT(count >= 0);

	for (bp = bp->b_cont; bp && count; count -= n, bp = bp->b_cont, arg += n) {
		n = MIN(count, bp->b_wptr - bp->b_rptr);
		error = STREAMS_COPYOUT((caddr_t)bp->b_rptr, (caddr_t)arg,
					n, fmt, copymode);
		if (error)
			return (error);
		if (fmt && bp->b_cont && (copymode == U_TO_K))
			adjfmtp(&fmt, bp, n);
	}
	ASSERT(count == 0);
	return (0);
}

/* 
 * Allocate a linkinfo table entry given the write queue of the
 * bottom module of the top stream and the write queue of the
 * stream head of the bottom stream.
 *
 * linkinfo table entries are freed by nulling the li_lblk.l_qbot field.
 */
struct linkinfo *
alloclink(queue_t *qup, queue_t *qdown, vfile_t *fpdown)
{
	register struct linkinfo *linkp, *lp;
	register int s;

	if ((linkp = kmem_zalloc(sizeof(struct linkinfo), KM_NOSLEEP))== NULL){

		str_stats_ops(cpuid(), 0, STR_LINK_FAILOPS, -1);
		return NULL;
	}
	linkp->li_lblk.l_qtop = qup;
	linkp->li_lblk.l_qbot = qdown;

	/*
	 * Assign link id, being careful to watch out
	 * for wrap-around leading to id clashes.  Unlike
	 * ioctl ids, link ids can be long-lived.
	 * If we're concerned about covert channels, start the id search
	 * at a random place rather than where the previous search stopped.
	 * If the random() routine is stubbed out, it will return 0,
	 * in which case we want to revert to the sequential method.
	 */
	LOCK_STR_RESOURCE(s);
	do {
		linkp->li_lblk.l_index = ++lnk_id;
		if (lnk_id <= 0)
			linkp->li_lblk.l_index = lnk_id = 1;
		for (lp = (struct linkinfo *)Strinfo[DYN_LINKBLK].sd_head;
		     lp; lp = lp->li_next)
			if (lp->li_lblk.l_index == lnk_id)
				break;
	} while (lp);

	linkp->li_fpdown = fpdown;
	Strinfo[DYN_LINKBLK].sd_cnt++;
	linkp->li_next = (struct linkinfo *) Strinfo[DYN_LINKBLK].sd_head;
	if (linkp->li_next)
		linkp->li_next->li_prev = linkp;
	linkp->li_prev = NULL;
	Strinfo[DYN_LINKBLK].sd_head = (void *)linkp;
	UNLOCK_STR_RESOURCE(s);

	str_stats_ops(cpuid(), 0, STR_LINK_OPS, 1);
	return(linkp);
}

/*
 * Free a linkinfo entry.
 */
void
lbfree(register struct linkinfo *linkp)
{
	register int s;

	LOCK_STR_RESOURCE(s);
	if (linkp->li_prev == NULL) {
		if (linkp->li_next)
			linkp->li_next->li_prev = NULL;
		Strinfo[DYN_LINKBLK].sd_head = (void *) linkp->li_next;
	}
	else {
		if (linkp->li_next)
			linkp->li_next->li_prev = linkp->li_prev;
		linkp->li_prev->li_next = linkp->li_next;
	}
	UNLOCK_STR_RESOURCE(s);
	kmem_free(linkp, sizeof(struct linkinfo));

	atomicAddInt(&(Strinfo[DYN_LINKBLK].sd_cnt), -1);

	str_stats_ops(cpuid(), 0, STR_LINK_OPS, -1);
	return;
}

/*
 * Check for a potential linking cycle.
 * Return 1 if a link will result in a cycle,
 * and 0 otherwise.
 */
int
linkcycle(stdata_t *upstp, stdata_t *lostp)
{
	register struct mux_node *np;
	register struct mux_edge *ep;
	register int i;
	device_driver_t loddt, upddt;

	/*
	 * if the lower stream is a pipe/FIFO, return, since link
	 * cycles can not happen on pipes/FIFOs
	 */
	if (lostp->sd_vnode->v_type == VFIFO)
		return(0);

	mutex_lock(&mux_mutex, PZERO - 1);
	for (i = 0; i < cdevmax; i++) {
		np = &mux_nodes[i];
		MUX_CLEAR(np);
	}
	loddt = device_driver_getbydev(lostp->sd_vnode->v_rdev);
	upddt = device_driver_getbydev(upstp->sd_vnode->v_rdev);
	np = str_find_mux_node(loddt, 0);
	if (np == 0) {
		/*
		 * linkcycle() was called prior to any nodes being inserted so
		 * there can't be a cycle; return 0.
		 */
		mutex_unlock(&mux_mutex);
		return 0;
	}
	for ( ; ; ) {
		if (!MUX_DIDVISIT(np)) {
			if (np->mn_ddt == upddt) {
				mutex_unlock(&mux_mutex);
				return (1);
			}
			if (np->mn_outp == NULL) {
				MUX_VISIT(np);
				if (np->mn_originp == NULL) {
					mutex_unlock(&mux_mutex);
					return (0);
				}
				np = np->mn_originp;
				continue;
			}
			MUX_VISIT(np);
			np->mn_startp = np->mn_outp;
		} else {
			if (np->mn_startp == NULL) {
				if (np->mn_originp == NULL) {
					mutex_unlock(&mux_mutex);
					return (0);
				}
				else {
					np = np->mn_originp;
					continue;
				}
			}
			ep = np->mn_startp;
			np->mn_startp = ep->me_nextp;
			ep->me_nodep->mn_originp = np;
			np = ep->me_nodep;
		}
	}
	/* NOTREACHED */
}

/* 
 * Find linkinfo table entry corresponding to the parameters.
 */
struct linkinfo *
findlinks(stdata_t *stp, int index, int type)
{
	register struct linkinfo *linkp;
	register struct mux_edge *mep;
	register int s;
	struct mux_node *mnp;
	queue_t *qup;

	if ((type & LINKTYPEMASK) == LINKNORMAL) {
		qup = getendq(stp->sd_wrq);
		LOCK_STR_RESOURCE(s);
		for (linkp = (struct linkinfo *) Strinfo[DYN_LINKBLK].sd_head;
		     linkp; linkp = linkp->li_next) {
			if ((qup == linkp->li_lblk.l_qtop) &&
		    	    (!index || (index == linkp->li_lblk.l_index))) {
				UNLOCK_STR_RESOURCE(s);
				return (linkp);
			}
		}
		UNLOCK_STR_RESOURCE(s);
	} else {
		ASSERT((type & LINKTYPEMASK) == LINKPERSIST);
		mutex_lock(&mux_mutex, PZERO - 1);
		mnp = str_find_mux_node(device_driver_getbydev(stp->sd_vnode->v_rdev), 0);
		ASSERT(mnp);
		mep = mnp->mn_outp;
		while (mep) {
			if ((index == 0) || (index == mep->me_muxid))
				break;
			mep = mep->me_nextp;
		}
		if (!mep) {
			mutex_unlock(&mux_mutex);
			return (NULL);
		}
		LOCK_STR_RESOURCE(s);
		for (linkp = (struct linkinfo *) Strinfo[DYN_LINKBLK].sd_head;
		     linkp; linkp = linkp->li_next) {
			if ( (!linkp->li_lblk.l_qtop) &&
			    (mep->me_muxid == linkp->li_lblk.l_index)) {
				UNLOCK_STR_RESOURCE(s);
				mutex_unlock(&mux_mutex);
				return (linkp);
			}
		}
		UNLOCK_STR_RESOURCE(s);
		mutex_unlock(&mux_mutex);
	}
	return (NULL);
}

/* 
 * Given a queue ptr, follow the chain of q_next pointers until you reach the
 * last queue on the chain and return it.
 */
queue_t *
getendq(register queue_t *q)
{
	ASSERT( q!= NULL);
	while (SAMESTR(q))
		q = q->q_next;
	return (q);
}

/*
 * Unlink a multiplexor link.  Stp is the controlling stream for the
 * link, fpdown is the file pointer for the lower stream, and
 * linkp points to the link's entry in the linkinfo table.
 */
int
munlink(struct stdata **stpp,
	struct linkinfo *linkp,
	int flag,
	cred_t *crp,
	int *rvalp)
{
	struct strioctl strioc;
	struct stdata *stp, *stpdown;
	queue_t *rq;
	int error = 0;
	struct vfile *fp;

	stp = *stpp; /* safe since we're locked */
	ASSERT(stp);
	if ((flag & LINKTYPEMASK) == LINKNORMAL)
		strioc.ic_cmd = I_UNLINK;
	else
		strioc.ic_cmd = I_PUNLINK;
	strioc.ic_timout = 0;
	strioc.ic_len = sizeof(struct linkblk);
	strioc.ic_dp = (char *)&linkp->li_lblk;
	
	error = strdoioctl(stp, &strioc, NULL, K_TO_K, STRLINK, crp, rvalp);

	/*
	 * If there was an error and this is not called via strclose, 
	 * return to the user.  Otherwise, pretend there was no error 
	 * and close the link.  
	 */
	if (error) {
		if (flag & LINKCLOSE) {
			cmn_err(CE_CONT, "KERNEL: munlink: could not perform unlink ioctl, closing anyway\n");
			stp->sd_flag &= ~(STRDERR|STWRERR); /* allows strdoioctl() to work */
		} else
			return (error);
	}

	stpdown = VF_TO_VNODE(linkp->li_fpdown)->v_stream;
	stpdown->sd_flag &= ~STPLEX;
	mux_rmvedge(stp, linkp->li_lblk.l_index);
	rq = RD(stpdown->sd_wrq);
	setq(rq, &strdata, &stwdata);
	rq->q_ptr = WR(rq)->q_ptr = (caddr_t)stpdown;

	fp = linkp->li_fpdown;
	lbfree(linkp);

	streams_mpdetach(rq);

	STRHEAD_UNLOCK(stp);
	vfile_close(fp); /* XXXrs is this really safe? */
	STRHEAD_LOCK(stpp, stp);
	ASSERT(stp); /* XXXrs really? */
	return (0);
}

/*
 * Unlink all multiplexor links for which stp is the controlling stream.
 * Return 0, or a non-zero errno on failure.
 */
int
munlinkall(struct stdata **stpp, int flag, cred_t *crp, int *rvalp)
{
	struct stdata *stp;
	struct linkinfo *linkp;
	int error = 0;

	stp = *stpp; /* safe since we're locked */
	ASSERT(stp);
	while (linkp = findlinks(stp, 0, flag)) {
		if (error = munlink(stpp, linkp, flag, crp, rvalp))
			return (error);
	}
	return (0);
}

/*
 * A multiplexor link has been made.  Add an
 * edge to the directed graph.  Returns 0 on success
 * and an error number on failure.
 */
int
mux_addedge(stdata_t *upstp, stdata_t *lostp, int muxid)
{
	register struct mux_node *np;
	register struct mux_edge *ep;
	device_driver_t upddt, loddt;

	upddt = device_driver_getbydev(upstp->sd_vnode->v_rdev);
	loddt = device_driver_getbydev(lostp->sd_vnode->v_rdev);
	mutex_lock(&mux_mutex, PZERO - 1);
	np = str_find_mux_node(upddt, 1);
	ASSERT(np);
	if (np->mn_outp) {
		ep = np->mn_outp;
		while (ep->me_nextp)
			ep = ep->me_nextp;
		if ((ep->me_nextp = (struct mux_edge *)kmem_alloc(
			sizeof(struct mux_edge), 0)) == NULL) {
			cmn_err_tag(160,CE_WARN,
				"Can not allocate memory for mux_edge\n");
			mutex_unlock(&mux_mutex);
			return (EAGAIN);
		}
		ep = ep->me_nextp;
	} else {
		if ((np->mn_outp = (struct mux_edge *)kmem_alloc(
			sizeof(struct mux_edge), 0)) == NULL) {
			cmn_err_tag(161,CE_WARN,
				"Can not allocate memory for mux_edge\n");
			mutex_unlock(&mux_mutex);
			return (EAGAIN);
		}
		ep = np->mn_outp;
	}
	ep->me_nextp = NULL;
	ep->me_muxid = muxid;
	ep->me_nodep = str_find_mux_node(loddt, 1);
	ASSERT(ep->me_nodep);
	mutex_unlock(&mux_mutex);
	return (0);
}

/*
 * A multiplexor link has been removed.  Remove the
 * edge in the directed graph.
 */
static void
mux_rmvedge(stdata_t *upstp, int muxid)
{
	register struct mux_node *np;
	register struct mux_edge *ep;
	register struct mux_edge *pep = NULL;
	device_driver_t upddt;

	upddt = device_driver_getbydev(upstp->sd_vnode->v_rdev);
	mutex_lock(&mux_mutex, PZERO - 1);
	np = str_find_mux_node(upddt, 0);
	ASSERT(np);
	ASSERT (np->mn_outp != NULL);
	ep = np->mn_outp;
	while (ep) {
		if (ep->me_muxid == muxid) {
			if (pep)
				pep->me_nextp = ep->me_nextp;
			else
				np->mn_outp = ep->me_nextp;
			mutex_unlock(&mux_mutex);
			kmem_free(ep, sizeof(struct mux_edge));
			return;
		}
		pep = ep;
		ep = ep->me_nextp;
	}
	ASSERT(0);	/* should not reach here */
	mutex_unlock(&mux_mutex); 	/* but in case we do */
}

/*
 * Set the interface values for a pair of queues (qinit structure,
 * packet sizes, water marks).
 */
void
setq(queue_t *rq, struct qinit *rinit, struct qinit *winit)
{
	register queue_t  *wq;

	ASSERT(STREAM_LOCKED(rq));

	wq = WR(rq);
	rq->q_qinfo = rinit;
	rq->q_hiwat = rinit->qi_minfo->mi_hiwat;
	rq->q_lowat = rinit->qi_minfo->mi_lowat;
	rq->q_minpsz = rinit->qi_minfo->mi_minpsz;
	rq->q_maxpsz = rinit->qi_minfo->mi_maxpsz;
	wq->q_qinfo = winit;
	wq->q_hiwat = winit->qi_minfo->mi_hiwat;
	wq->q_lowat = winit->qi_minfo->mi_lowat;
	wq->q_minpsz = winit->qi_minfo->mi_minpsz;
	wq->q_maxpsz = winit->qi_minfo->mi_maxpsz;

	return;
}

/*
 * Make a protocol message given control and data buffers.
 *
 * Caller should be holding the streams monitor.
 */
int
strmakemsg(register struct strbuf *mctl,
	   int count,
	   struct uio *uiop,
	   struct stdata *stp,
	   long flag,
	   mblk_t **mpp)
{
	register mblk_t *mp = NULL;
	register mblk_t *bp;
	caddr_t base;
	int pri, msgtype;
	int wroff = (int)stp->sd_wroff;
	int offlg = 0;
	int error = 0;

	ASSERT(STREAM_LOCKED(RD(stp->sd_wrq)));

	*mpp = NULL;
	pri = (flag & RS_HIPRI) ? BPRI_MED : BPRI_LO;

	/*
	 * Create control part of message, if any.
	 */
	if ((mctl != NULL) && (mctl->len >= 0)) {
		register int ctlcount;
		int allocsz;

		msgtype = (flag & RS_HIPRI) ? M_PCPROTO : M_PROTO;
		ctlcount = mctl->len;
		base = mctl->buf;

		/*
		 * Give modules a better chance to reuse M_PROTO/M_PCPROTO
		 * blocks by increasing the size to something more usable.
		 */
		allocsz = MAX(ctlcount, 64);

		/*
		 * Range checking has already been done; simply try
		 * to allocate a message block for the ctl part.
		 */
		while (!(bp = allocb(allocsz, pri))) {
			if (uiop->uio_fmode  & (FNDELAY|FNONBLOCK))
				return (EAGAIN);
			if (error = strwaitbuf(allocsz, pri))
				return (error);
		}

		bp->b_datap->db_type = msgtype;
		if (STREAMS_COPYIN(base, (caddr_t)bp->b_wptr, ctlcount,
				   "XXXrs fmt", U_TO_K)) {
			freeb(bp);
			return (EFAULT);
		}

		/*
		 * We could have slept copying in user pages.
		 * Recheck the stream head state (the other end
		 * of a pipe could have gone away).
		 */
		if (stp->sd_flag & (STRHUP|STWRERR|STPLEX)) {
			error = ((stp->sd_flag & STPLEX) ? EINVAL :
			    stp->sd_werror);
			freeb(bp);
			return (error);
		}
		bp->b_wptr += ctlcount;
		mp = bp;
	}

	/*
	 * Create data part of message, if any.
	 */
	if (count >= 0) {
		register int size;

		size = count + (offlg ? 0 : wroff);
		while ((bp = allocb(size, pri)) == NULL) {
			if (uiop->uio_fmode  & (FNDELAY|FNONBLOCK))
				return (EAGAIN);
			if (error = strwaitbuf(size, pri)) {
				freemsg(mp);
				return (error);
			}
		}
		if (wroff && !offlg++ &&
		    (wroff < bp->b_datap->db_lim - bp->b_wptr)) {
			bp->b_rptr += wroff;
			bp->b_wptr += wroff;
		}
		if ((size = MIN(count, bp->b_datap->db_lim - bp->b_wptr)) &&
		    (error = STREAMS_UIOMOVE((caddr_t)bp->b_wptr, size,
					     UIO_WRITE, uiop))) {
			freeb(bp);
			freemsg(mp);
			return (error);
		}

		/*
		 * We could have slept copying in user pages.
		 * Recheck the stream head state (the other end
		 * of a pipe could have gone away).
		 */
		if (stp->sd_flag & (STRHUP|STWRERR|STPLEX)) {
			error = ((stp->sd_flag & STPLEX) ? EINVAL :
			    stp->sd_werror);
			freeb(bp);
			freemsg(mp);
			return (error);
		}
		bp->b_wptr += size;
		count -= size;
		if (!mp)
			mp = bp;
		else
			linkb(mp, bp);

	}
	*mpp = mp;
	return (0);
}

/*
 * Wait for a buffer to become available.  Return non-zero errno
 * if not able to wait, 0 if buffer is probably there.
 */
int
strwaitbuf(int size, int pri)
{
	if (!bufcall(size,pri,(void(*)(long))wakeup,(long)curthreadp)) {
		return (ENOSR);
	}
	if (sleep((caddr_t)curthreadp, STOPRI)) {
		strunbcall(size, curthreadp);
		return (EINTR);
	}
	strunbcall(size, curthreadp);
	return (0);
}

/*
 * Remove a wakeup for the given process from the bufcall list for
 * the given buffer size.  'size' can be -1 for externally-supplied buffers.
 */
static void
strunbcall(int size, struct kthread *kt)
{
	register int s;
	register struct strevent *sep, *prevsep;
	
	LOCK_STR_RESOURCE(s);
	sep = strbcalls.bc_head;
	prevsep = NULL;
	while (sep) {
		if ((sep->se_arg == (long)kt) &&
		    (sep->se_func == (void (*)(long))wakeup) &&
		    (sep->se_size == size)) {
			if (prevsep == NULL)
				strbcalls.bc_head = sep->se_next;
			else
				prevsep->se_next = sep->se_next;
			if (sep == strbcalls.bc_tail)
				strbcalls.bc_tail = prevsep;
			sefree(sep);
			UNLOCK_STR_RESOURCE(s);
			return;
		}
		prevsep = sep;
		sep = sep->se_next;
	}
	UNLOCK_STR_RESOURCE(s);
	return;
}

static int
strwaitsleep(struct stdata *stp, queue_t *q, int flag, int pri)
{
        vnode_t *vp;
	bhv_desc_t *bdp;
	fifonode_t *fnp;
	sv_t *svp;

        vp = stp->sd_vnode;
        if (vp->v_type == VFIFO) {
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &fifo_vnodeops);
		ASSERT(bdp != NULL);
		fnp = BHVTOF(bdp);
		svp = (flag & READWAIT || flag & GETWAIT) ?
				&(fnp->fn_empty) : &(fnp->fn_full);
		
		if ((pri & PMASK) > PZERO)
			return sv_wait_sig(svp, pri|TIMER_SHIFT(AS_STRMON_WAIT), 0, 0);
		sv_wait(svp, pri|TIMER_SHIFT(AS_STRMON_WAIT), 0, 0);
		return 0;
        } else {
		return(sleep(q, pri));
        }
}

extern void strwakeq(struct stdata *stp, queue_t *q, int flag);
/*
 * This function waits for a read or write event to happen on a stream.
 */
int
strwaitq(register struct stdata *stp,
	 int flag,
	 off_t count,
	 int fmode,
#ifndef _IRIX_LATER
	 int vtime,
#endif /* !_IRIX_LATER */
	 int *done)
{
	int bid, s2, slpflg, slppri, errs;
	int error = 0;
	queue_t *slpq;
	mblk_t *mp;
	long *rd_count;

#ifndef _IRIX_LATER
	int id;
#endif /* !_IRIX_LATER */

#ifdef _IRIX_LATER
	if (fmode & (FNDELAY|FNONBLOCK)) {
		if (!(flag & NOINTR))
			error = EAGAIN;
		*done = 1;
		return (error);
	}
#else /* !_IRIX_LATER */
	/* if POSIX no-delay and it's a tty, return error */
	if ((fmode & FNONBLK) && (stp->sd_flag & STRISTTY)) {
		*done = 1;
                return(EAGAIN);
        }

	if (((fmode&(FNONBLK|FNDELAY))          /* if no delay on, */
	    && (!(stp->sd_flag & STRISTTY)     /* and not a tty or reading */
		 || (flag & (READWAIT|NOINTR))))
	    || (stp->sd_flag & STFIONBIO)) {    /* or 4.2 non-delay on */
		if (flag & NOINTR) {    /* quit */
			;               /* without error if 2nd time around */

		} else if (stp->sd_flag & STFIONBIO) {
			error = EWOULDBLOCK;        /* with 4.2 error */

		} else if (!(stp->sd_flag & STRISTTY)) {
			error = EAGAIN;     /* no error if tty */
		}
		*done = 1;
                return(error);
	}
#endif /* _IRIX_LATER */

	if ((flag & READWAIT) || (flag & GETWAIT)) {
		slpflg = RSLEEP;
		slpq = RD(stp->sd_wrq);
		slppri = STIPRI;
		errs = STRDERR|STPLEX;

		/*
		 * If any module downstream has requested read notification
		 * by setting SNDMREAD flag using M_SETOPTS, send a message
		 * down stream.
		 */
		if ((flag & READWAIT) && (stp->sd_flag & SNDMREAD)) {
			while ((mp = allocb(sizeof(long), BPRI_MED)) == NULL) {
				bid = bufcall(sizeof(long), BPRI_MED,
					      (void(*)(long))strqbuf,
					      (long)stp);
				if (bid == 0) {
					*done = 1;
					return (EAGAIN);
				}
				stp->sd_flag |= RDBUFWAIT;
				if (strwaitsleep(stp, slpq,
						 flag, slppri)) {
					if (stp->sd_flag & RDBUFWAIT) {
						/* interrupted sleep */
						stp->sd_flag &= ~RDBUFWAIT;
						unbufcall(bid);
						*done = 1;
						return (EINTR);
					}
				}
			}
			mp->b_datap->db_type = M_READ;
			rd_count = (long *)mp->b_wptr;
			*rd_count = count;
			mp->b_wptr += sizeof(long);
			/*
			 * Send the number of bytes requested by the
			 * read as the argument to M_READ.
			 */
			putnext(stp->sd_wrq, mp);
#ifdef _EAGER_RUNQUEUES
			if (qready())
				runqueues();
#endif /* _EAGER_RUNQUEUES */
			/*
			 * If any data arrived due to inline processing
			 * of putnext(), don't sleep.
			 */
			mp = RD(stp->sd_wrq)->q_first;
			while (mp) {
				if (!(mp->b_flag & MSGNOGET))
					break;
				mp = mp->b_next;
			}
			if (mp != NULL) {
				*done = 0;
				return (error);
			}
		}
	} else {
		slpflg = WSLEEP;
		slpq = stp->sd_wrq;
		slppri = STOPRI;
		errs = STWRERR|STRHUP|STPLEX;
	}
	
#ifndef _IRIX_LATER
	if (vtime)
		id = MP_STREAMS_TIMEOUT2(stp->sd_wrq->q_monpp, strwakeq, stp,
					 vtime*(short)(HZ/10), slpq,
					 (void *)(__psunsigned_t)flag);
#endif /* !_IRIX_LATER */
	LOCK_STR_RESOURCE(s2);
	stp->sd_pflag |= slpflg;
	UNLOCK_STR_RESOURCE(s2);
	if (strwaitsleep(stp, slpq, flag, slppri)) {
		LOCK_STR_RESOURCE(s2);
		stp->sd_pflag &= ~slpflg;
		UNLOCK_STR_RESOURCE(s2);

		strwakeq(stp, slpq, flag);
#ifndef _IRIX_LATER
		if (vtime)
			untimeout(id);
#endif /* !_IRIX_LATER */
		if (!(flag & NOINTR))
			error = EINTR;
		*done = 1;
		return (error);
	}

#ifndef _IRIX_LATER
	if (vtime)
		untimeout(id);
#endif /* !_IRIX_LATER */
	if (stp->sd_flag & errs) {
		*done = 1;
		return ((stp->sd_flag & STPLEX) ? EINVAL :
		    ((errs & STWRERR) ? stp->sd_werror : stp->sd_rerror));
	}
	*done = 0;
	return (error);
}

static int
str2num(register char **str)
{
	register int n;

	n = 0;
	for (; **str >= '0' && **str <= '9'; (*str)++)
		n = 10 * n + **str - '0';
	return (n);
}

/*
 * Update canon format pointer with "bytes" worth of data.
 *
 * Note that adjfmtp is only called when copymode == U_TO_K.
 * Hence it is OK for adjfmtp to access get_current_abi
 */
static void
adjfmtp(register char **str, register mblk_t *bp, int bytes)
{
	register caddr_t addr;
	register caddr_t lim;
	long num;
#if _MIPS_SIM == _ABI64
	/* This depends on being called only when copymode is U_TO_K */
	register int abi32 = ! ABI_IS_IRIX5_64(get_current_abi());
#endif

	addr = (caddr_t)bp->b_rptr;
	lim = addr + bytes;
	while (addr < lim) {
		switch (*(*str)++) {
		case 's':			/* short */
			addr = SALIGN(addr);
			addr = SNEXT(addr);
			break;
		case 'i':			/* integer */
			addr = IALIGN(addr);
			addr = INEXT(addr);
			break;
		case 'l':			/* long */
#if _MIPS_SIM == _ABI64
			if (abi32) {
				addr = IALIGN(addr);
				addr = INEXT(addr);
				break;
			}
#endif
			addr = LALIGN(addr);
			addr = LNEXT(addr);
			break;
		case 'b':			/* byte */
			addr++;
			break;
		case 'c':			/* character */
			if ((num = str2num(str)) == 0) {
				while (*addr++)
					;
			} else
				addr += num;
			break;
		case 0:
			return;
		default:
			break;
		}
	}
}

static void
strqbuf(register struct stdata *stp)
{
	if (stp->sd_flag & RDBUFWAIT) {
		stp->sd_flag &= ~RDBUFWAIT;
		wakeup(RD(stp->sd_wrq));
	}
	return;
}

/*
 * Allocate a stream head.
 */
struct stdata *
shalloc(queue_t *qp)
{
	register int s;
	register stdata_t *stp;
	register struct shinfo *shp;

	if ((shp = kmem_zalloc(sizeof(struct shinfo), KM_SLEEP)) == NULL) {
		str_stats_ops(cpuid(), 0, STR_STREAM_FAILOPS, 1);
		return NULL;
	}
	SAVE_STRALLOC_ADDRESS(shp, __return_address);
	stp = (stdata_t *)shp;
	stp->sd_wrq = WR(qp);
	initpollhead(&stp->sd_pollist);

	LOCK_STR_RESOURCE(s);
	Strinfo[DYN_STREAM].sd_cnt++;
	shp->sh_next = (struct shinfo *)Strinfo[DYN_STREAM].sd_head;
	if (shp->sh_next) {
		shp->sh_next->sh_prev = shp;
	}
	shp->sh_prev = NULL;
	Strinfo[DYN_STREAM].sd_head = (void *)shp;
	UNLOCK_STR_RESOURCE(s);

	str_stats_ops(cpuid(), 0, STR_STREAM_OPS, 1);
	return(stp);
}

/*
 * Free a stream head.
 */
void
shfree(register struct stdata *stp)
{
	register int s;
	register struct shinfo *shp;

	while (stp->sd_pollist.ph_list != NULL) {
		/* 
		 * allow the processes that slep on stp->sd_pollist
		 * to clean up polldat
 		 */
		delay(10);
	}
	destroypollhead(&stp->sd_pollist);

	LOCK_STR_RESOURCE(s);
	shp = (struct shinfo *)stp;
	SAVE_STRFREE_ADDRESS(shp, __return_address);

	if (shp->sh_prev == NULL) {
		if (shp->sh_next)
			shp->sh_next->sh_prev = NULL;
		Strinfo[DYN_STREAM].sd_head = (void *) shp->sh_next;
	} else {
		if (shp->sh_next)
			shp->sh_next->sh_prev = shp->sh_prev;
		shp->sh_prev->sh_next = shp->sh_next;
	}
	Strinfo[DYN_STREAM].sd_cnt--;
	UNLOCK_STR_RESOURCE(s);

	str_stats_ops(cpuid(), 0, STR_STREAM_OPS, -1);
	kmem_free(shp, sizeof(struct shinfo));
	return;
}

/*
 * Allocate a pair of queues
 */
queue_t *
allocq(void)
{
	register int s2;
	register queue_t *qp;
	register struct queinfo *qip;
	cpuid_t cpu_id = cpuid();

	/* allocate a queinfo struct (which contains two queues) */
	if ((qip = kmem_zalloc(sizeof(struct queinfo), KM_SLEEP)) == NULL) {
		str_stats_ops(cpu_id, 0, STR_QUEUE_FAILOPS, 1);
		return NULL;
	}
	qp = (queue_t *) qip;
	qp->q_qinfo = NULL;
	qp->q_first = NULL;
	qp->q_last = NULL;
	qp->q_next = NULL;
	qp->q_ptr = NULL;
	qp->q_count = 0;
	qp->q_flag = QUSE | QREADR;
	qp->q_minpsz = 0;
	qp->q_maxpsz = 0;
	qp->q_hiwat = 0;
	qp->q_lowat = 0;
	qp->q_link = NULL;
	qp->q_bandp = NULL;
	qp->q_nband = 0;
#ifdef _STRQ_TRACING
	qp->q_trace = ktrace_alloc(QUEUE_TRACE_SIZE, 0);
#endif
	WR(qp)->q_qinfo = NULL;
	WR(qp)->q_first = NULL;
	WR(qp)->q_last = NULL;
	WR(qp)->q_next = NULL;
	WR(qp)->q_ptr = NULL;
	WR(qp)->q_count = 0;
	WR(qp)->q_flag = QUSE;
	WR(qp)->q_minpsz = 0;
	WR(qp)->q_maxpsz = 0;
	WR(qp)->q_hiwat = 0;
	WR(qp)->q_lowat = 0;
	WR(qp)->q_link = NULL;
	WR(qp)->q_bandp = NULL;
	WR(qp)->q_nband = 0;
#ifdef _STRQ_TRACING
	WR(qp)->q_trace = ktrace_alloc(QUEUE_TRACE_SIZE, 0);
#endif
	/* for accounting purposes, count as 2 */
	str_stats_ops(cpu_id, 0, STR_QUEUE_OPS, 2);

	LOCK_STR_RESOURCE(s2);
	Strinfo[DYN_QUEUE].sd_cnt += 2;

	qip->qu_next = (struct queinfo *) Strinfo[DYN_QUEUE].sd_head;
	if (qip->qu_next)
		qip->qu_next->qu_prev = qip;
	qip->qu_prev = NULL;
	Strinfo[DYN_QUEUE].sd_head = (void *) qip;
	UNLOCK_STR_RESOURCE(s2);

	return(qp);
}

/*
 * Free a pair of queues.
 */
void
freeq(register queue_t *qp)
{
	register int s2;
	register struct queinfo *qip;
	register qband_t *qbp;
	register qband_t *nqbp;

#ifdef DEBUG
	qp->q_flag &= ~QUSE;
	WR(qp)->q_flag &= ~QUSE;
#endif /* DEBUG */

#ifdef _STRQ_TRACING
	ktrace_free(qp->q_trace);
	ktrace_free(WR(qp)->q_trace);
#endif
	qbp = qp->q_bandp;
	while (qbp) {
		nqbp = qbp->qb_next;
		freeband(qbp);
		qbp = nqbp;
	}
	qbp = WR(qp)->q_bandp;
	while (qbp) {
		nqbp = qbp->qb_next;
		freeband(qbp);
		qbp = nqbp;
	}
	qip = (struct queinfo *) qp;

	LOCK_STR_RESOURCE(s2);
	if (qip->qu_prev == NULL) {
		if (qip->qu_next)
			qip->qu_next->qu_prev = NULL;
		Strinfo[DYN_QUEUE].sd_head = (void *)qip->qu_next;
	} else {
		if (qip->qu_next)
			qip->qu_next->qu_prev = qip->qu_prev;
		qip->qu_prev->qu_next = qip->qu_next;
	}
	Strinfo[DYN_QUEUE].sd_cnt -= 2;
	UNLOCK_STR_RESOURCE(s2);

	/* for accounting purposes, count individually */
	str_stats_ops(cpuid(), 0, STR_QUEUE_OPS, -2);

	kmem_free(qip, sizeof(struct queinfo));
	return;
}

/*
 * Allocate a qband structure.
 */
qband_t *
allocband(void)
{
	register int s2;
	register struct qbinfo *qbip;

	if ((qbip = kmem_zalloc(sizeof(struct qbinfo), KM_NOSLEEP)) == NULL) {
		str_stats_ops(cpuid(), 0, STR_QBAND_FAILOPS, 1);
		return NULL;
	}

	LOCK_STR_RESOURCE(s2);
	Strinfo[DYN_QBAND].sd_cnt++;
	qbip->qbi_next = (struct qbinfo *) Strinfo[DYN_QBAND].sd_head;
	if (qbip->qbi_next)
		qbip->qbi_next->qbi_prev = qbip;
	qbip->qbi_prev = NULL;
	Strinfo[DYN_QBAND].sd_head = (void *)qbip;
	UNLOCK_STR_RESOURCE(s2);

	str_stats_ops(cpuid(), 0, STR_QBAND_OPS, 1);
	return((qband_t *) qbip);
}

/*
 * Free a qband structure.
 */
static void
freeband(register qband_t *qbp)
{
	register int s2;
	register struct qbinfo *qbip;

	qbip = (struct qbinfo *) qbp;
	if (qbip->qbi_prev == NULL) {
		if (qbip->qbi_next)
			qbip->qbi_next->qbi_prev = NULL;

		LOCK_STR_RESOURCE(s2);
		Strinfo[DYN_QBAND].sd_head = (void *)qbip->qbi_next;
		UNLOCK_STR_RESOURCE(s2);
	} else {
		if (qbip->qbi_next)
			qbip->qbi_next->qbi_prev = qbip->qbi_prev;
		qbip->qbi_prev->qbi_next = qbip->qbi_next;
	}
	atomicAddInt(&(Strinfo[DYN_QBAND].sd_cnt), -1);
	str_stats_ops(cpuid(), 0, STR_QBAND_OPS, -1);

	kmem_free(qbip, sizeof(struct qbinfo));
	return;
}

/*
 * Allocate a stream event cell.
 */
struct strevent *
sealloc(int slpflag)
{
	register int s2;
	register struct seinfo *sep;
	cpuid_t cpu_id = cpuid();

	LOCK_STR_RESOURCE(s2);
	if (sefreelist) {
		sep = sefreelist;
		sefreelist = sep->s_next;
		sep->s_strevent.se_pid = -1;
		sep->s_strevent.se_events = 0;
		sep->s_strevent.se_next = NULL;
	} else {
		UNLOCK_STR_RESOURCE(s2);

		if (sep = kmem_zalloc(sizeof(struct seinfo),
			(slpflag == SE_SLEEP) ? KM_SLEEP : KM_NOSLEEP)) {

			LOCK_STR_RESOURCE(s2);
			Strinfo[DYN_STREVENT].sd_cnt++;
		} else {
			LOCK_STR_RESOURCE(s2);
			if (slpflag == SE_NOSLP) {
				/* use the cache for these */
				if (secachep) {
					sep = secachep;
					secachep = secachep->s_next;
					sep->s_strevent.se_pid = -1;
					sep->s_strevent.se_events = 0;
					sep->s_strevent.se_next = NULL;
				}
			}
			if (sep == NULL) { /* failed anyhow */

				UNLOCK_STR_RESOURCE(s2);
				str_stats_ops(cpu_id,0,STR_STREVENT_FAILOPS,1);
				return NULL;
			}
		}
	}
	sep->s_next = (struct seinfo *) Strinfo[DYN_STREVENT].sd_head;
	if (sep->s_next)
		sep->s_next->s_prev = sep;
	sep->s_prev = NULL;
	Strinfo[DYN_STREVENT].sd_head = (void *) sep;
	UNLOCK_STR_RESOURCE(s2);

	str_stats_ops(cpu_id, 0, STR_STREVENT_OPS, 1);
	return((struct strevent *) sep);
}

/*
 * Free a stream event cell.
 * Caller must hold resource lock.
 */
void
sefree(struct strevent *sep)
{
	register struct seinfo *seip;

	/* already at splstr due to the spinlock being held */
	ASSERT(spinlock_islocked(&str_resource_lock));

	seip = (struct seinfo *) sep;
	if (seip->s_prev == NULL) {
		if (seip->s_next)
			seip->s_next->s_prev = NULL;
		Strinfo[DYN_STREVENT].sd_head = (void *) seip->s_next;
	}
	else {
		if (seip->s_next)
			seip->s_next->s_prev = seip->s_prev;
		seip->s_prev->s_next = seip->s_next;
	}
	if (seip >= &Secache[0] && seip <= &Secache[SECACHE-1]) {
		/* it's from the cache */
		seip->s_next = secachep;
		secachep = seip;
	}
	else {
		seip->s_next = sefreelist;
		sefreelist = seip;
	}
	str_stats_ops(cpuid(), 0, STR_STREVENT_OPS, -1);
	return;
}

/*
 * Check linked list of strhead write queues with held messages;
 * put downstream those that have been held long enough.
 *
 * Run the service procedures of each enabled queue
 *	-- must not be reentered
 *
 * Called by service mechanism (processor dependent) if there
 * are queues to run.  The mechanism is reset.
 */

/* XXXrs */
static int queuerun_gen = 0;
/* XXXrs - kludge */
extern int strwsrv(queue_t *);
/* XXXrs - kludge */

void
_queuerun(void)
{
	register queue_t *q;
	register int s2, nevent;
	register ulong count;
	register struct strevent *sep;
	register struct stdata *stp;
	mblk_t *mp;

	qqrun = 0;
	qrunflag = 0;
	/*
	 * Clear 'qrunflag' at top.  Nothing prevents it from getting
	 * set while queuerun runs.  Worst that could happen
	 * is that someone calls setqsched() between now and
	 * when we run the queue, and that should only result
	 * in an unnecessary pass through here.  Since here
	 * is the only place it gets cleared, and since we are
	 * about to run the queue, we should never miss a
	 * queue run.
	 */

	LOCK_STR_RESOURCE(s2);
        while ((stp = strholdhead) && (stp->sd_rtime <= lbolt)) {

		if ((strholdhead = stp->sd_hnext) == NULL)
			strholdtailp = &strholdhead;

		q = stp->sd_wrq;
		ASSERT(q->q_flag & QHLIST);
		q->q_flag &= ~QHLIST;

		if (mp = q->q_first)  {
			q->q_first = NULL;
			if (stp->sd_flag & (STRHUP|STWRERR))  {
				UNLOCK_STR_RESOURCE(s2);
				freemsg(mp);
				LOCK_STR_RESOURCE(s2);
			} else {
				UNLOCK_STR_RESOURCE(s2);

				ASSERT(q->q_next);
				MP_STREAMS_INTERRUPT(q->q_monpp, putnext,
					(void *)q, (void *)mp, 0);

				LOCK_STR_RESOURCE(s2);
			}
		}
	}
	do {
		if (strbcflag) {
			strbcwait = strbcflag = 0;

			/*
			 * count how many events are on the list
			 * now so we can check to avoid looping
			 * in low memory situations
			 */
			nevent = 0;
			for (sep = strbcalls.bc_head; sep; sep = sep->se_next)
				nevent++;
			/*
			 * get estimate of available memory from kmem_avail().
			 * awake all bufcall functions waiting for
			 * memory whose request could be satisfied 
			 * by 'count' memory and let 'em fight for it.
			 */
			count = kmem_avail();
			queuerun_gen++;
			while ((sep = strbcalls.bc_head) && nevent) {
				--nevent;
				if (sep->se_size <= count) {
				    strbcalls.bc_head = sep->se_next;

				    UNLOCK_STR_RESOURCE(s2);

				    MP_STREAMS_INTERRUPT(sep->se_monpp,
							 sep->se_func,
							 (void *)sep->se_arg,
							 0, 0);

				    LOCK_STR_RESOURCE(s2);
				    sefree(sep);
				} else {
				    /*
				     * too big, try again later - note that
				     * nevent was decremented above so we
				     * won't retry this one on this iteration
				     * of the loop
				     */
				    if (sep->se_next == NULL) {
					ASSERT(strbcalls.bc_tail == sep);
					strbcalls.bc_head = sep;
				    } else {
					ASSERT(strbcalls.bc_tail != sep);
					ASSERT(!strbcalls.bc_tail->se_next);
					strbcalls.bc_head = sep->se_next;
					sep->se_next = NULL;
					strbcalls.bc_tail->se_next = sep;
					strbcalls.bc_tail = sep;
				    }
				}
			}
			if (strbcalls.bc_head) {
				/*
				 * still some bufcalls we couldn't do
				 * let kmem_free know
				 */
				strbcwait = 1;
			} else {
				strbcalls.bc_tail = NULL;
			}
		}

		while (q = qhead) {
			/*
			 * XXXrs - we aren't holding the queue's monitor
			 *         here so it's not safe to fiddle with
			 *         the flags.  To make things safe means
			 *	   holding some common lock.  Most other
			 *	   q_flag manipulations are done under the
			 *	   monitor, so perhaps it would be possible
			 *	   to just attempt to grab the monitor
			 *	   semaphore spinlock???  Either that, or all
			 *	   other q_flag manipulations will need to
			 *	   be under the resource lock.  Ugh.
			 *	   Could make special cases of the QENAB and
			 *	   QBACK flags and move them to their own
			 *	   flags fields?  That probably violates some
			 *	   stinking ABI?
			 */
			ASSERT(q->q_pflag & QENAB);
			if (!(qhead = q->q_link))
				qtailp = &qhead;

			if (q->q_qinfo->qi_srvp) {
				UNLOCK_STR_RESOURCE(s2);
				STREAMS_ENAB_INTERRUPT(q->q_monpp,
						       q->q_qinfo->qi_srvp,
						       q, 0, 0);
				LOCK_STR_RESOURCE(s2);
				/*
				 * XXXrs - QBACK/QB_BACK are now cleared
				 *         as part of the new
				 *	   STREAMS_ENAB_INTERRUPT mechanism.
				 *         See the comments in
				 *         streams_service().
				 */
			}
		}
	} while (strbcflag);

        if (strholdhead && !strholdflag)  {
                strholdflag++;
                MP_STREAMS_TIMEOUT(&streams_monp, strrelease, 0, STRHOLDTIME);
        }

	UNLOCK_STR_RESOURCE(s2);
}

#ifdef _EAGER_RUNQUEUES
/*
 * Function to kick off queue scheduling for those system calls
 * that cause queues to be enabled (read, recv, write, send, ioctl).
 */
void
runqueues(void)
{
	register int s2;

	LOCK_STR_RESOURCE(s2);
	if (qrunflag && !queueflag) {
		queueflag = 1;
		UNLOCK_STR_RESOURCE(s2);
		queuerun();
		LOCK_STR_RESOURCE(s2);
		queueflag = 0;
		UNLOCK_STR_RESOURCE(s2);
		return;
	}
	UNLOCK_STR_RESOURCE(s2);
}
#else
/*
 * Otherwise, this function is handled by the macro setqsched().
 * See the definition in sys/strsubr.h
 */
#endif /* _EAGER_RUNQUEUES */

/* 
 * Find module
 * 
 * return index into fmodsw
 * or -1 if not found
 */
int
findmod(register char *name)
{
	register int i, j;

	for (i = 0; i < fmodmax; i++) {
		if (fmhold(i))		/* invalid fmodsw index */
			continue;
		for (j = 0; j < FMNAMESZ + 1; j++) {
			if (fmodsw[i].f_name[j] != name[j]) 
				break;
			if (name[j] == '\0') {
				fmrele(i);
				return(i);
			}
		}
		fmrele(i);
	}
	return(-1);
}

/* 
 * Same as findmod() but holds a reference to the module so it can't
 * be unloaded.  Reference can be given up later by calling fmrele().
 */
int
fmfind(register char *name)
{
	register int i, j;

	for (i = 0; i < fmodmax; i++) {
		if (fmhold(i))		/* invalid fmodsw index */
			continue;
		for (j = 0; j < FMNAMESZ + 1; j++) {
			if (fmodsw[i].f_name[j] != name[j]) 
				break;
			if (name[j] == '\0')
				return(i);
		}
		fmrele(i);
	}
	return(-1);
}

/*
 * Set the QBACK or QB_BACK flag in the given queue for
 * the given priority band.
 */
void
setqback(register queue_t *q, register unsigned char pri)
{
	register int i;
	qband_t *qbp;
	qband_t **qbpp;

	if (pri != 0) {
		if (pri > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (pri > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					cmn_err_tag(162,CE_WARN,
					  "setqback: can't allocate qband\n");
					return;
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = pri;
		while (--i)
			qbp = qbp->qb_next;

		qbp->qb_flag |= QB_BACK;
	} else {
		q->q_flag |= QBACK;
	}
}

/* ARGSUSED */
int
strcopyin(caddr_t from, caddr_t to, unsigned int len, char *fmt, int copyflag)
{
	if (copyflag == U_TO_K) {
		if (ncopyin(from, to, len, fmt))
			return (EFAULT);
	} else {
		ASSERT(copyflag == K_TO_K);
		bcopy(from, to, len);
	}
	return (0);
}

/* ARGSUSED */
int
strcopyout(
	caddr_t from,
	caddr_t to,
	unsigned int len,
	char *fmt,
	int copyflag)
{
	if (copyflag == U_TO_K) {
		if (ncopyout(from, to, len, fmt))
			return (EFAULT);
	} else {
		ASSERT(copyflag == K_TO_K);
		bcopy(from, to, len);
	}
	return (0);
}

void
strsignal(struct stdata *stp, int sig, long band)
{
	switch (sig) {
	case SIGPOLL:				
		if (stp->sd_sigflags & S_MSG) 
			strsendsig(stp->sd_siglist, S_MSG, band);
		break;

	default:
		if (stp->sd_pgid)
			signal(stp->sd_pgid, sig);
		break;
	}
}

void
strhup(struct stdata *stp)
{
	if (stp->sd_sigflags & S_HANGUP) 
		strsendsig(stp->sd_siglist, S_HANGUP, 0L);
	pollwakeup(&stp->sd_pollist, POLLHUP);
}

/*
 * Called by module open routines that want to become the
 * controlling tty of the process.
 */
void
strctltty(queue_t *rq)
{

	register struct stdata *stp;

	/*
	 * Make sure it's a read queue.
	 */
	if (!(rq->q_flag & QREADR))
		rq = RD(rq);

	/*
	 * Chain up to the stream head.
	 */
	while (rq->q_next != NULL)
		rq = rq->q_next;

	stp = (struct stdata *) rq->q_ptr;
	ASSERT(stp);

	stp->sd_flag |= STRISTTY;
}

/*
 * Session management call-back function.  Used to tear down
 * the association of the tty with a session.  Will likely be
 * called twice: once to grab the monitor and signal the
 * foreground process group, and once to clear the session
 * data structures and release the monitor.
 */
#define VALID_ACTIONS \
(TTYCB_GETMONITOR|TTYCB_SIGFGPGRP|TTYCB_RELSESS|TTYCB_RELMONITOR)

void
strsesscb(void *arg, int actions)
{
	vpgrp_t *vpg;
	struct stdata *stp = (struct stdata *)arg;

	ASSERT((actions & ~VALID_ACTIONS) == 0);

	/*
	 * Locking and releasing the monitor must be the first and
	 * last if statements, respectively since the interior operations
	 * may depend on holding the monitor.
	 */
	if (actions & TTYCB_GETMONITOR) {
		ASSERT(stp);
		STRHEAD_LOCK(&stp, stp);
		ASSERT(stp);
	}
	if (actions & TTYCB_SIGFGPGRP && stp->sd_pgid) {
		if ((vpg = VPGRP_LOOKUP(stp->sd_pgid)) != NULL) {
			VPGRP_SENDSIG(vpg, SIGHUP, SIG_ISKERN, 0, NULL, NULL);
			VPGRP_RELE(vpg);
		}
		stp->sd_pgid = 0;
	}
	if (actions & TTYCB_RELSESS) {
		VSESSION_RELE(stp->sd_vsession);
		stp->sd_vsession = NULL;
		stp->sd_pgid = 0;
	}
	if (actions & TTYCB_RELMONITOR) {
		ASSERT(stp);
		STRHEAD_UNLOCK(stp);
	}
}

void
stralloctty(struct vnode *vp, struct stdata *stp)
{
	int error;
	vproc_t *vpr;
	vp_get_attr_t attr;
	vsession_t *vsp;

	ASSERT(stp);
	ASSERT(STREAM_LOCKED(stp->sd_wrq));

	if (!(stp->sd_flag & STRISTTY) ||  /* Have we requested to be ctty? */
	    vp->v_type != VCHR)		   /* No pipes allowed! */
		return;

	/*
	 * Wait for an in-progress ctty alloc and then return
	 * if it was successful
	 */
	while (stp->sd_flag & STRCTTYALLOC) {
		stp->sd_flag |= STRCTTYWAIT;
		sleep((caddr_t)&stp->sd_vsession, PZERO);
	}
		
	/*
	 * If the device is already a controlling tty, return.
	 */
	if (stp->sd_vsession)	/* is already a controlling tty */
		return;
	/*
	 * Stop any attempts to allocate this tty as a
	 * controlling tty for a session.
	 */
	stp->sd_flag |= STRCTTYALLOC;

	/*
	 * Get the caller's session.
	 */
	vpr = VPROC_LOOKUP(current_pid());
	ASSERT(vpr != NULL);
	VPROC_GET_ATTR(vpr, VGATTR_PGID|VGATTR_SID, &attr);
	if (attr.va_sid == 0
	    || vpr->vp_pid != attr.va_sid
	    || (vsp = VSESSION_LOOKUP(attr.va_sid)) == NULL) {
		VPROC_RELE(vpr);
		goto alloc_done;
	}
	VPROC_RELE(vpr);

	/*
	 * Attempt to set the tty as the controlling tty.
	 */
	VSESSION_CTTY_ALLOC(vsp, vp, strsesscb, (void *)stp, error);
	if (error) {
		VSESSION_RELE(vsp);
	} else {
		ASSERT(stp->sd_vsession == NULL);
		stp->sd_vsession = vsp;
		stp->sd_pgid = attr.va_pgid;
		/* don't VSESSION_RELE, keep reference for sd_vsession */
	}

alloc_done:
	/*
	 * Unblock waiting calls to this function.
	 */
	stp->sd_flag &= ~STRCTTYALLOC;
	if (stp->sd_flag & STRCTTYWAIT) {
		stp->sd_flag &= ~STRCTTYWAIT;
		wakeup((caddr_t)&stp->sd_vsession);
	}
	return;
}

/*
 * Unlink "all" persistant links.
 * Currently called only from spec_sync on final system shutdown
 */
void
strpunlink(struct cred *crp)
{
	struct shinfo  *shp;
	int rval, s;

	/*
	 * for each allocated stream head, call munlinkall() with flag of
	 * LINKPERSIST to unlink any/all persistant links for the device.
	 */
	STREAMS_LOCK();
	LOCK_STR_RESOURCE(s);
	shp = (struct shinfo *)Strinfo[DYN_STREAM].sd_head;
	UNLOCK_STR_RESOURCE(s);

	while(shp) {
	    (void)munlinkall((struct stdata **)&(Strinfo[DYN_STREAM].sd_head),
			     LINKIOCTL|LINKPERSIST, crp, &rval);
	    LOCK_STR_RESOURCE(s);
	    shp = shp->sh_next;
	    UNLOCK_STR_RESOURCE(s);
	}
	STREAMS_UNLOCK();
	return;
}

/*
 * Find a mux_node entry that matches the device_driver_t requested; if
 * not found but 'insert' specified, fill in the first available slot.
 * Return 0 if not found and can't insert.
 *
 * expects to be called holding mux_mutex
 */
struct mux_node *
str_find_mux_node(device_driver_t ddt, int insert)
{
	int first_avail = -1;
	int i;

	ASSERT(ddt);

	ASSERT(mutex_mine(&mux_mutex));

	for (i = 0; i < cdevmax; i++) {
		if (mux_nodes[i].mn_ddt == ddt) {
			return &mux_nodes[i];
		} else if ((first_avail == -1) && 
			   (mux_nodes[i].mn_ddt == 0) &&
			   insert) {
			first_avail = i;
		}
	}
	if (first_avail != -1) {
		/* inserting && free slot */
		mux_nodes[first_avail].mn_ddt = ddt;
		return &mux_nodes[first_avail];
	} else {
		/* not inserting or not found */
		return (struct mux_node *)0;
	}
}

/*
 * takes mux_mutex itself so that devsupport code doesn't know about our
 * internal locking
 */
void
str_free_mux_node(device_driver_t ddt)
{
	int i;

	ASSERT(ddt);

	mutex_lock(&mux_mutex, PZERO - 1);
	for (i = 0; i < cdevmax; i++) {
		if (mux_nodes[i].mn_ddt == ddt) {
			mux_nodes[i].mn_ddt = 0;
		}
	}
	mutex_unlock(&mux_mutex);
}
