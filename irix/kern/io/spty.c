/* Pseudo-teletype Driver
 *	(Actually two drivers, requiring two entries in 'cdevsw')
 *
 * Copyright 1986, Silicon Graphics Inc., Mountain View, CA.
 */

#ident "$Revision: 3.62 $"

#include "sys/types.h"
#include "bstring.h"
#include "sys/param.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/cred.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/ioctl.h"
#include "sys/signal.h"
#include "sys/strsubr.h"
#include "sys/kabi.h"
#include "sys/kmem.h"
#include "sys/strids.h"
#include "sys/stropts.h"
#include "sys/strmp.h"
#include "sys/stty_ld.h"
#include "sys/sysinfo.h"
#include "sys/termio.h"
#include "sys/strtty.h"
#include "sys/major.h"
#include "sys/mac_label.h"
#include "sys/idbgactlog.h"
#include "sys/idbgentry.h"
#include "sys/cmn_err.h"


extern struct stty_ld def_stty_ld;

extern tcgeta(queue_t *, mblk_t *, struct termio *);

/* controller stream stuff
 */
static struct module_info ptcm_info = {
	STRID_PTC,			/* module ID */
	"PTC",				/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size */
	STRHIGH,			/* hi-water mark */
	STRLOW,				/* lo-water mark */
	MULTI_THREADED,			/* locking */
};

static int ptc_rsrv(queue_t *);
static int ptc_open(queue_t *, dev_t *, int, int, cred_t *);
static int ptc_close(queue_t *, int, cred_t *);

static struct qinit ptc_rinit = {
	NULL, ptc_rsrv, ptc_open, ptc_close, NULL, &ptcm_info, NULL
};

static int ptc_wput(queue_t *, mblk_t *);
static int ptc_wsrv(queue_t *);

static struct qinit ptc_winit = {
	ptc_wput, ptc_wsrv, NULL, NULL, NULL,
	&ptcm_info, NULL
};

int ptcdevflag = 0;

struct streamtab ptcinfo = {&ptc_rinit, &ptc_winit, NULL, NULL};


/* slave stream stuff
 */
static struct module_info ptsm_info = {
	STRID_PTS,			/* module ID */
	"PTS",				/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size */
	STRHIGH,			/* hi-water mark */
	STRLOW,				/* lo-water mark */
	MULTI_THREADED,			/* locking */
};

static pts_rput(queue_t *, mblk_t *);
static pts_rsrv(queue_t *);
static int pts_open(queue_t *, dev_t *, int, int, cred_t *);
static int pts_close(queue_t *, int, cred_t *);
static struct qinit pts_rinit = {
	pts_rput, pts_rsrv, pts_open, pts_close, NULL, &ptsm_info, NULL
};

static int pts_wput(queue_t *, mblk_t *);
static int pts_wsrv(queue_t *);
static struct qinit pts_winit = {
	pts_wput, pts_wsrv, NULL, NULL, NULL, &ptsm_info, NULL
};

int ptsdevflag = 0;

struct streamtab ptsinfo = {&pts_rinit, &pts_winit, NULL, NULL};

/*
 * We allocate one of these structures for every open ptc/tty pair
 */
struct pty {
	queue_t	*pts_rq, *pts_wq;	/* slave queues			*/
	queue_t	*ptc_rq, *ptc_wq;	/* controller queues		*/


	struct termio pt_termio;
#define pt_line pt_termio.c_line
#define pt_iflag pt_termio.c_iflag
#define pt_cflag pt_termio.c_cflag
#define pt_ospeed pt_termio.c_ospeed
#define	pt_cc	 pt_termio.c_cc

	uint	pt_index;		/* port number			*/

	u_char	pt_pkt;			/* pending 'packet' bits	*/

	u_char	pts_state, ptc_state;	/* current state		*/
	struct	winsize	pt_winsize;	/* size of window		*/
	mac_label *pt_mac;		/* MAC label			*/
	u_char	pts_auto_ld;		/* push line disc. onto slave   */
#if defined(DEBUG) && defined(ACTLOG)
	struct actlog pt_actlog;
#endif
	mblk_t	*pt_bufp;		/* ptr. to zero byte msg. blk. */
};


#define NUM_PTY_PER_MAJOR 200		/* minors/major */
#define MAX_MAXPTY 1000			/* cannot configure more ptys */
extern int maxpty;			/* lboot-master.d value */

/* XXXrs compatibility foo? ========== */

#define NUM_PTY_MAJOR 5			/* number of pty major numbers */
static int ptc_majors[NUM_PTY_MAJOR] = {
	14, 104, 106, 108, 110
};

/* XXXrs ============================= */

static struct pty **pty;

lock_t pty_lock;
#define INIT_PTY_LOCK()	spinlock_init(&pty_lock, "ptylock")
#define PTY_LOCK()	mutex_spinlock(&pty_lock)
#define PTY_UNLOCK(s)	mutex_spinunlock(&pty_lock, (s))


/* force cflag to this */
#define PT_CFLAG (CS8|CREAD|CLOCAL)

/* slave state bits */
#define PT_UNBORN	0x01		/* never opened */
#define	PT_DEAD		0x02		/* have lived and died already	*/
#define PTS_TXSTOP	0x04		/* output stopped by received XOFF */
#define PTS_LIT		0x08		/* have seen literal character	*/
#define	PTS_PUSHING	0x10		/* pushing module in pts_open */
#define	PTS_PUSHWAIT	0x20		/* 2nd thru Nth pts_open waiting for */
					/* initial module push */
#define PTS_LOCKED	0x40		/* slave side locked */

/* controller state bits */
#define PTC_QUEUED	0x04		/* 'queued' device		*/
#define PTC_PKT		0x08		/* 'packet mode' on		*/
#define PTC_PKT_SET	0x10		/* 'packet mode' set in head	*/

#define MAX_PKT_DATA	2048		/* maximum data in 'packet mode' */

/*
 * Called from idbg to dump a pty struct.
 */

void
dump_pty_info(__psint_t n)
{
	struct pty *ptyp;

	if (n < 0) {
		ptyp = (struct pty *)n;
	}
	else if (n >= maxpty) {
		qprintf("Pty %d out of range.  Max is %d\n", n, maxpty-1);
		return;
	} else {
		ptyp = pty[n];
	}
	qprintf("&pty[%d] 0x%x pty[%d] 0x%x lock 0x%x:\n",
		n, &pty[n], n, ptyp, pty_lock);
	if (!ptyp) {
		qprintf("\tno pty allocated.\n");
		return;
	}
	if (ptyp == (struct pty *)1L) {
		qprintf("\tpty being allocated.\n");
		return;
	}
	qprintf("\tpts_rq 0x%x pts_wq 0x%x ptc_rq 0x%x ptc_wq 0x%x\n",
		ptyp->pts_rq, ptyp->pts_wq, ptyp->ptc_rq, ptyp->ptc_wq);
	qprintf("\ttermio 0x%x index %d pkt 0x%x\n",
		&ptyp->pt_termio, ptyp->pt_index, ptyp->pt_pkt);
	qprintf("\tpts_state 0x%x : %s%s%s%s%s%s%s\n",
		ptyp->pts_state,
		ptyp->pts_state & PT_UNBORN ? "UNBORN " : "",
		ptyp->pts_state & PT_DEAD ? "DEAD " : "",
		ptyp->pts_state & PTS_TXSTOP ? "TXSTOP " : "",
		ptyp->pts_state & PTS_LIT ? "LIT " : "",
		ptyp->pts_state & PTS_PUSHING ? "PUSHING " : "",
		ptyp->pts_state & PTS_PUSHWAIT ? "PUSHWAIT " : "",
		ptyp->pts_state & PTS_LOCKED ? "LOCKED " : "");
	qprintf("\tptc_state 0x%x : %s%s%s%s%s\n",
		ptyp->ptc_state,
		ptyp->ptc_state & PT_UNBORN ? "UNBORN " : "",
		ptyp->ptc_state & PT_DEAD ? "DEAD " : "",
		ptyp->ptc_state & PTC_QUEUED ? "QUEUED " : "",
		ptyp->ptc_state & PTC_PKT ? "PKT " : "",
		ptyp->ptc_state & PTC_PKT_SET ? "PKT_SET" : "");
	qprintf("\twinsize 0x%x label 0x%x svr4_open 0x%x\n",
		ptyp->pt_winsize, ptyp->pt_mac, ptyp->pts_auto_ld);
}

#if defined(DEBUG) && defined(ACTLOG)

#define PTY_AL_NULL		0

#define PTY_AL_PTS_OPEN		10
#define PTY_AL_PTS_CLOSE	11
#define PTY_AL_PTS_RSRV		12
#define PTY_AL_PTS_WPUT		13
#define PTY_AL_PTS_WSRV		14

#define PTY_AL_PTC_OPEN		20
#define PTY_AL_PTC_CLOSE	21
#define PTY_AL_PTC_RSRV		22
#define PTY_AL_PTC_WPUT		23
#define PTY_AL_PTC_WSRV		24

#define PTY_AL_PTS_NOENAB	30
#define PTY_AL_PTS_ENABOK	31
#define PTY_AL_PTC_NOENAB	32

void
dump_pty_actlog(int n)
{
	register int i, j, act;
	struct actlog *alp;
	struct actlogentry *aep;

	if (pty[n] == NULL) {
		qprintf("pty[%d] not allocated.\n", n);
		return;
	}
	alp = &pty[n]->pt_actlog;
        for (i = 0, j = alp->al_wind % ACTLOG_SIZE;
             i < ACTLOG_SIZE;
             i++, j = ++j % ACTLOG_SIZE) {
                aep = &alp->al_log[j];
                act = aep->ae_act;
                qprintf(
"ptyactlog[%d]: %s sec %d usec %d cpu %d 0x%x 0x%x 0x%x\n",
			j,
			act == PTY_AL_NULL ?		"EMPTY     " :
			act == PTY_AL_PTS_OPEN ?	"PTS OPEN  " :
			act == PTY_AL_PTS_CLOSE ?	"PTS CLOSE " :
			act == PTY_AL_PTS_RSRV ?	"PTS RSRV  " :
			act == PTY_AL_PTS_WPUT ?	"PTS WPUT  " :
			act == PTY_AL_PTS_WSRV ?	"PTS WSRV  " :
			act == PTY_AL_PTC_OPEN ?	"PTC OPEN  " :
			act == PTY_AL_PTC_CLOSE ?	"PTC CLOSE " :
			act == PTY_AL_PTC_RSRV ?	"PTC RSRV  " :
			act == PTY_AL_PTC_WPUT ?	"PTC WPUT  " :
			act == PTY_AL_PTC_WSRV ?	"PTC WSRV  " :
			act == PTY_AL_PTS_NOENAB ?	"PTS NOENAB" :
			act == PTY_AL_PTC_NOENAB ?	"PTC NOENAB" :
			act == PTY_AL_PTS_ENABOK ?	"PTC ENABOK" :
							"?!?!?!?!? ",
			aep->ae_sec,
			aep->ae_usec,
			aep->ae_cpu,
			aep->ae_info1,
			aep->ae_info2,
			aep->ae_info3);
	}
}
#endif /* DEBUG && ACTLOG */

/* initialize this module
 */
void
ptcinit()
{
	if (maxpty > MAX_MAXPTY) {
		printf("ptc: too many pty\'s: %d > %d\n",
			   maxpty, MAX_MAXPTY);
		maxpty = MAX_MAXPTY;
	}
	if (pty == 0) {
		pty = (struct pty**)kern_malloc(sizeof(*pty)*maxpty);
		bzero(pty, sizeof(*pty)*maxpty);
		INIT_PTY_LOCK();
	}
}

/*
 * Allocate a pty structure for the given dev if its not already allocated.
 * Pty lock must be held by caller.
 * 
 * called from both ptc_open and pts_close. Note that this means that
 * there can be two process both trying to allocate the pty.
 */
#define CONTROLLER	0
#define SLAVE		1

static int
pt_alloc(uint whocalled, uint d, struct pty **ptypp, int *ospl, cred_t *crp)
{ 
	struct pty *pt;


	for (;;) {
		pt = pty[d];
		if (!pt) {
		    	extern int baud_to_cbaud(speed_t);
			/*
			 * ptc_open searches for a null pty entry before calling
			 * pt_alloc. Since, the PTY_LOCK is asserted, ptc_open
			 * will always take the pty[d] == 0 branch.
			 */
			/*
			 * SVR4 semantics require that the controller side
			 * be open and that the slave side be unlocked from
			 * the controller side before the slave can be opened.
			 * This effectively means that the slave side open
			 * can't allocate a pty struct.  Only the controller
			 * can do that.
			 */
			if (whocalled == SLAVE)
				return(EACCES);

			/*
			 * mark the slot "in use". we might sleep in
			 * kmem_zalloc so we have to unlock the structure.
			 */
			pty[d] = (struct pty *)1L;
			PTY_UNLOCK(*ospl);
			pt = (struct pty *)kmem_zalloc(sizeof(struct pty),
						       KM_SLEEP);
			*ospl = PTY_LOCK();

			if (!pt) {
				/* should never happen */
				pty[d] = 0;
				wakeup((caddr_t)&pty[d]);
				return(ENOMEM);
			}
			pt->pt_termio = def_stty_ld.st_termio;
			pt->pt_cflag &= ~CBAUD;
			pt->pt_cflag |= PT_CFLAG|baud_to_cbaud(B19200);
			pt->pt_ospeed = B19200;
			pt->pt_index = d;
#ifdef DEBUG_PTY_MAC
			cmn_err(CE_NOTE,
			    "pty mac initialize: %s, proc=%c%c",
			    get_current_name(),
			    crp->cr_mac->ml_msen_type,
			    crp->cr_mac->ml_mint_type);
#endif /* DEBUG_PTY_MAC */
			pt->pt_mac = crp->cr_mac;
			pt->ptc_state = PT_UNBORN;
			pt->pts_state = PT_UNBORN;
			pt->pts_state |= PTS_LOCKED;
			pty[d] = pt;
			wakeup((caddr_t)&pty[d]);
		} else if (pt == (struct pty *)1L) {
			/*
			 * another process is trying to allocate the
			 * pty structure. Note that PTY_LOCK is off
			 * during call to kmem_zalloc. This allows other
			 * process to sneak in.  Two situations:
			 * 1. ptc_open already in progress and a pts_open
			 *    is attempted. Note that there can be several
			 *    pts_opens here.
			 * 2. pts_open already in progress and a second
			 *    pts_open being attempted.
			 * Note that we will fail the pts_open if the
			 * pseudotty has not been locked. This can only
			 * be done from the master side since only there
			 * will the open succeed.
			 */
			PTY_UNLOCK(*ospl);
			if (sleep((caddr_t)&pty[d], STIPRI)) {
				*ospl = PTY_LOCK();	
				return(EINTR);
			}
			*ospl = PTY_LOCK();	
		} else {
			if (_MAC_ACCESS(pt->pt_mac, crp, MACWRITE)) {
#ifdef	DEBUG_PTY_MAC
				cmn_err(CE_NOTE,
				    "pty access: %s, MAC=%c%c, proc=%c%c",
				    get_current_name(),
				    pt->pt_mac->ml_msen_type,
				    pt->pt_mac->ml_mint_type,
				    crp->cr_mac->ml_msen_type,
				    crp->cr_mac->ml_mint_type);
#endif	/* DEBUG_PTY_MAC */
				return(EACCES);
			}
			*ptypp = pt;
			return(0);
		}
	}
}



/* do 'XON' */
static void
pt_resume(register struct pty *pt)
{
	pt->pts_state &= ~PTS_TXSTOP;
	qenable(pt->pts_wq);
}


/* set 'tty' parameters
 */
static void
pt_tcset(register struct pty *pt, register mblk_t *bp)
{
	register struct iocblk *iocp;
	register struct termio *tp;

	iocp = (struct iocblk*)bp->b_rptr;
	tp = STERMIO(bp);

	if (pt->ptc_state & PTC_PKT) {	/* do packet stuff */
		register ushort iflag = (tp->c_iflag & IXON);
		if ((iflag ^ pt->pt_iflag) & IXON) {
			register int s;

			STR_LOCK_SPL(s);
			pt->pt_pkt &= ~(TIOCPKT_DOSTOP|TIOCPKT_NOSTOP);
			if (iflag == IXON) {
				pt->pt_pkt |= TIOCPKT_DOSTOP;
			} else {
				pt->pt_pkt |= TIOCPKT_NOSTOP;
			}
			STR_UNLOCK_SPL(s);
		}
	}

	if (tp->c_ospeed == __INVALID_BAUD) {
	    tp->c_ospeed = pt->pt_termio.c_ospeed;
	}
	tp->c_ispeed = tp->c_ospeed;
	pt->pt_termio = *tp;

	if (PTS_TXSTOP & pt->pts_state
	    && !(pt->pt_iflag & IXON))
		pt_resume(pt);

	iocp->ioc_count = 0;
	bp->b_datap->db_type = M_IOCACK;
}



#define SWINSIZE(bp) ((struct winsize *)(bp)->b_cont->b_rptr)
#define WIN_IOCP(bp) ((struct iocblk*)bp->b_rptr)

/* get current window size
 */
static void
pt_gwinsz(register queue_t *wq, register mblk_t *bp, register struct pty *pt)
{
	ASSERT(WIN_IOCP(bp)->ioc_count == sizeof(struct winsize));

	*SWINSIZE(bp) = pt->pt_winsize;
	bp->b_datap->db_type = M_IOCACK;
	qreply(wq,bp);
}


/* set window size
 */
static void
pt_swinsz(register queue_t *wq, register mblk_t *bp, register struct pty *pt)
{
	register u_char df;
	ASSERT(WIN_IOCP(bp)->ioc_count == sizeof(struct winsize));

	df = bcmp((char*)&pt->pt_winsize, (char*)SWINSIZE(bp),
		  sizeof(struct winsize));
	if (0 != df)
		pt->pt_winsize = *SWINSIZE(bp);
	bp->b_datap->db_type = M_IOCACK;
	WIN_IOCP(bp)->ioc_count = 0;
	qreply(wq,bp);

	if (0 != df && pt->pts_rq)	/* signal size change to all slaves */
		(void)putctl1(pt->pts_rq->q_next, M_PCSIG, SIGWINCH);
}


/* slave side open
 */
static int				/* return minor # or failure reason */
pts_open(register queue_t *rq, dev_t *devp, int flag, int sflag, cred_t *crp)
{
	struct pty *pt;
	queue_t *wq = WR(rq);
	uint d;
	int error, s;
	mblk_t *mp;

	if (sflag)			/* do not do slave 'clone' open */
		return(ENODEV);
	d = minor(*devp);
	if (d >= NUM_PTY_PER_MAJOR)
		return(ENODEV);
	switch (emajor(*devp)) {
	case PTS1_MAJOR:
		d += NUM_PTY_PER_MAJOR*1;
		break;
	case PTS2_MAJOR:
		d += NUM_PTY_PER_MAJOR*2;
		break;
	case PTS3_MAJOR:
		d += NUM_PTY_PER_MAJOR*3;
		break;
	case PTS4_MAJOR:
		d += NUM_PTY_PER_MAJOR*4;
		break;
	}
	if (d >= maxpty)
		return(ENODEV);

	s = PTY_LOCK();
	if ((error = pt_alloc(SLAVE, d, &pt, &s, crp))) {
		PTY_UNLOCK(s);
		return(error);
	}

	/*
	 * we are a second process trying to open this pty. The
	 * first process is pushing STREAMS modules and has gone
	 * to sleep. Note that the PTY_LOCK has been freed during
	 * that time. We will wait until the first process completes
	 * the push sequence.
	 */
	while (pt->pts_state & PTS_PUSHING) {
		pt->pts_state |= PTS_PUSHWAIT;
		PTY_UNLOCK(s);
		if (sleep((caddr_t) &pt->pts_state, STIPRI))
			return(EINTR);
		s = PTY_LOCK();
	}

	if (!pt->pts_rq) {		/* connect new device to stream */
		if (pt->ptc_state & PT_DEAD) { /* Controller died on us */
			pty[d] = 0;
			PTY_UNLOCK(s);
			if (pt->pt_bufp)	/* from previous process */
				freemsg(pt->pt_bufp);
			kmem_free(pt, sizeof(struct pty));
			return(EIO);
		}
		/*
		 * pty entry is created locked in pt_alloc(). The only way
		 * to unlock the pty is via an ioctl on the master side.
		 * See UNLKPT case in ptc_wput. Thus a pts_open will only
		 * succeed after a ptc_open and the pty has been explicitly
		 * unlocked by the master side process.
		 */
		if (pt->pts_state & PTS_LOCKED) {
			PTY_UNLOCK(s);
			return(EACCES);
		}

		/*
		 * indicate we are pushing a module, since we have to
		 * release the pty lock during the push operation (we
		 * might sleep). Thus prevent racing pts_opens...
		 */
		pt->pts_state |= PTS_PUSHING;
		if (pt->pts_auto_ld) {
			PTY_UNLOCK(s);
			error = strdrv_push(rq, "stty_ld", devp, crp);
			s = PTY_LOCK();
		}
		/*
		 * a second process tryed to do an open while we
		 * were pushing the STREAMS modules.
		 */
		if (pt->pts_state & PTS_PUSHWAIT) {
			pt->pts_state &= ~PTS_PUSHWAIT;
			wakeup((caddr_t) &pt->pts_state);
		}
		/* done with module push */
		pt->pts_state &= ~PTS_PUSHING;

		/*
		 * Could have slept pushing.  The master may have done
		 * a close. If so, then we're done.
		 */

		if (pt->ptc_state & PT_DEAD) { /* Controller died on us. */
			pty[d] = 0;
			PTY_UNLOCK(s);
			if (pt->pt_bufp)
				freemsg(pt->pt_bufp);
			kmem_free(pt, sizeof(struct pty));
			if (error)
				return(error);
			qdetach(rq->q_next, 1, flag, crp);
			return(EIO);
		}

		if (error) {
			PTY_UNLOCK(s);
			return(error);
		}

		/*
		 * allocate a 0 length mblock if we don't have one
		 */
		if (!pt->pt_bufp) {
			if (! (mp = allocb(0, 0))) {
				PTY_UNLOCK(s);
				return EAGAIN;
			}
			pt->pt_bufp = mp;
		}

		pt->pts_state &= ~(PT_UNBORN|PT_DEAD);

		PTY_UNLOCK(s);	/* JOINSTREAMS() can sleep */
		/* XXXrs error return ? */
		JOINSTREAMS(rq, &pt->ptc_rq);
		s = PTY_LOCK();

		/*
		 * Could have slept joining streams.  The master may have done
		 * a close. If so, then we're done.
		 */

		if (pt->ptc_state & PT_DEAD) { /* Controller died on us. */
			pty[d] = 0;
			PTY_UNLOCK(s);
			if (pt->pt_bufp)
				freemsg(pt->pt_bufp);
			kmem_free(pt, sizeof(struct pty));
			qdetach(rq->q_next, 1, flag, crp);
			return(EIO);
		}

		PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTS_OPEN, rq, wq, 0);
		rq->q_ptr = (caddr_t)pt;
		wq->q_ptr = (caddr_t)pt;
		pt->pts_rq = rq;
		pt->pts_wq = wq;

		/*
		 * wait for controller if asked. Need to wait if
		 * 1. master no yet up and
		 * 2. open flags FNONBLK and FNDELAY not set.
		 */
		while (pt->ptc_state & PT_UNBORN &&
		       !(flag & (FNONBLK|FNDELAY))) {
			PTY_UNLOCK(s);
			if (sleep((caddr_t)pt, STIPRI)) {
				/* signal during sleep */
				s = PTY_LOCK();
				pt->pts_rq = 0;
				pt->pts_wq = 0;
				/* master ain't there or died on us? */
				if (pt->ptc_state & (PT_UNBORN|PT_DEAD)) {
					pty[d] = 0;
					PTY_UNLOCK(s);
					qdetach(rq->q_next, 1, flag, crp);
					if (pt->pt_bufp)
						freemsg(pt->pt_bufp);
					kmem_free(pt, sizeof(struct pty));
				} else {	/* send HUP to controller */
					pt->pts_state |= PT_DEAD;
					PTY_UNLOCK(s);
/* do we send a HUP when slave sleep aborted? */
					(void)putctl(pt->ptc_rq->q_next,
						     M_HANGUP);
				}
				return(EINTR);
			}
			s = PTY_LOCK();
			/*
			 * Could have slept joining streams.  The master may
			 * have done a close. If so, then we're done.
			 */
			if (pt->ptc_state & PT_DEAD) {
				pty[d] = 0;
				PTY_UNLOCK(s);
				qdetach(rq->q_next, 1, flag, crp);
				if (pt->pt_bufp)
					freemsg(pt->pt_bufp);
				kmem_free(pt, sizeof(struct pty));
				return(EIO);
			}
			ASSERT(!(pt->ptc_state & PT_UNBORN));
			ASSERT(!(pt->pts_state & PTS_LOCKED));
			if (pt->pts_state & PTS_LOCKED) {
				/* HOW do we get here? */
				pt->pts_rq = 0;
				pt->pts_wq = 0;
				pt->pts_state |= PT_UNBORN;
				PTY_UNLOCK(s);
				qdetach(rq->q_next, 1, flag, crp);
				return(EACCES);
			}
		}
		PTY_UNLOCK(s);
	} else {
		ASSERT((struct pty*)pt->pts_rq->q_ptr == pt);
		ASSERT((struct pty*)pt->pts_wq->q_ptr == pt);
		ASSERT(rq == pt->pts_rq);
		PTY_UNLOCK(s);
	}

	return 0;
}



/* close down a pty, for the slave
 *	- close this end down and adjust state so that controller
 *	  will find out on next operation
 */
/* ARGSUSED */
static int
pts_close(queue_t *rq, int flag, cred_t *crp)
{
	int s;
	struct pty *pt = (struct pty*)rq->q_ptr;
	uint d = pt->pt_index;
	mblk_t *bp;
	queue_t *q;

	ASSERT(d < maxpty && pty[d] == pt);

	str_unbcall(rq);		/* stop waiting for buffers */

	s = PTY_LOCK();
	if (pt->ptc_state & (PT_UNBORN|PT_DEAD)) {
		pty[d] = 0;
		PTY_UNLOCK(s);
		if (pt->pt_bufp)
			freemsg(pt->pt_bufp);
		kmem_free(pt, sizeof(struct pty));
	} else {			/* let controlling tty know */
		PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTS_CLOSE,
			     pt->pts_rq, pt->pts_wq, 0);
		pt->pts_state |= PT_DEAD;
		pt->pts_rq = 0;
		pt->pts_wq = 0;
		pt->pt_winsize.ws_row = pt->pt_winsize.ws_col = 0;
		pt->pt_winsize.ws_xpixel = pt->pt_winsize.ws_ypixel = 0;
		PTY_UNLOCK(s);
		ASSERT(pt->pt_bufp != NULL);
		if ((bp = pt->pt_bufp) != NULL) {
			q = pt->ptc_rq->q_next;
			(q->q_qinfo->qi_putp)(q, bp);
			pt->pt_bufp = NULL;
		}
		/*
		 * clear out all master control messages destined for this
		 * slave. I'm disconnected and going away. Another slave
		 * may open the pseudo tty and should not get anything 
		 * that was going to go to me.
		 */
		flushq(pt->ptc_wq, FLUSHALL);
	}
	return 0;
}


static int
pts_dopktset(struct pty *pt, queue_t *wq, queue_t *crq)
{
	mblk_t *bp;
	register struct stroptions *sop;

	if (!(pt->ptc_state & PTC_PKT_SET))
		return 0;

	bp = str_allocb(sizeof(struct stroptions), wq, BPRI_LO);
	if (!bp)
		return -1;
	bp->b_datap->db_type = M_SETOPTS;
	sop = (struct stroptions*)bp->b_rptr;
	bp->b_wptr += sizeof(struct stroptions);
	sop->so_flags = SO_READOPT;
	sop->so_readopt = ((pt->ptc_state & PTC_PKT) ? RMSGD : RNORM);
	putnext(crq, bp);
	pt->ptc_state &= ~PTC_PKT_SET;
	return 0;
}

static int
pts_dopktdata(struct pty *pt, queue_t *wq, queue_t *crq)
{
	mblk_t *bp;

	if (TIOCPKT_DATA == pt->pt_pkt)
		return 0;

	bp = str_allocb(1,wq,BPRI_LO);
	if (!bp)
		return -1;
	*bp->b_wptr++ = pt->pt_pkt;
	putnext(crq, bp);
	pt->pt_pkt = TIOCPKT_DATA;
	return 0;
}

#define PUT_PROC	0
#define SERV_PROC	1

#define STASH(q,b) { \
	ASSERT(callfrom == PUT_PROC || callfrom == SERV_PROC); \
	if (callfrom == SERV_PROC) \
		putbq(q, b); \
	else \
		putq(q, b); \
}

static int
pts_senddata(struct pty *pt, queue_t *wq, queue_t *crq,
	     mblk_t *bp, int callfrom)
{
	if (pt->ptc_state & PTC_PKT) {	/* need header in */
		int len;
		register mblk_t *hbp;	/* packet mode */
		hbp = str_allocb(1,wq,BPRI_LO);
		if (!hbp) {
			STASH(wq, bp);
			return -1;
		}
		*hbp->b_wptr++ = TIOCPKT_DATA;
		hbp->b_cont = bp;

		if ((len = msgdsize(hbp)) > MAX_PKT_DATA) {
                        /*
			 * size of message is too large. So copy
			 * the message and adjust it so that we
			 * can send up MAX_PKT_DATA with the control
			 * byte. Put the remaining data into the
			 * queue for the service routine to handle it.
			 */
			register mblk_t *nmp;

			/* if we run out of resources */
			if ((nmp = copymsg(bp)) == NULL) {
				hbp->b_cont = NULL;	/* safety */
				freeb(hbp);
				STASH(wq, bp);
				return -1;
			}

                        adjmsg(bp, (MAX_PKT_DATA - 1));
                        STASH(wq, bp);

			adjmsg(nmp, -(len - (MAX_PKT_DATA)));
                        hbp->b_cont = nmp;
                }
		bp = hbp;
	}
	putnext(crq, bp);	/* send data up */
	return 0;
}

/* slave output 'put' function
 */
static int
pts_wput(register queue_t *wq, register mblk_t *bp)
{
	struct pty *pt = (struct pty*)wq->q_ptr;
	struct iocblk *iocp;
	register mblk_t *mctlp;		/* Pointer to a M_CTL message */
	queue_t *crq;

	ASSERT(pt->pt_index < maxpty && pty[pt->pt_index] == pt);

	PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTS_WPUT, 0, 0, 0);

	switch (bp->b_datap->db_type) {

	case M_FLUSH:
		if (pt->ptc_state & PTC_PKT) {	/* handle packet mode */
			if (*bp->b_rptr & FLUSHW)
				pt->pt_pkt |= TIOCPKT_FLUSHWRITE;
			if (*bp->b_rptr & FLUSHR)
				pt->pt_pkt |= TIOCPKT_FLUSHREAD;
		}
		if (*bp->b_rptr & FLUSHW)
			pt_resume(pt);
		sdrv_flush(wq,bp);
		break;

	case M_DATA:
		SYSINFO.pts += msgdsize(bp);

		if (pt->pts_state & PTS_TXSTOP) {
			putq(wq, bp);
			break;
		}

		crq = pt->ptc_rq;
		if (!crq || !canput(crq->q_next) || wq->q_first) {
			putq(wq, bp);
			break;
		}
		if (pts_dopktset(pt, wq, crq) < 0 ||
		    pts_dopktdata(pt, wq, crq) < 0) {
			putq(wq, bp);
			break;
		}

		(void) pts_senddata(pt, wq, crq, bp, PUT_PROC);
		break;

	case M_DELAY:			/* ignore timing requests */
		freemsg(bp);
		break;

	case M_IOCTL:
		if (pt->ptc_state & PTC_PKT) {	/* do packet stuff */
			pt->pt_pkt |= TIOCPKT_IOCTL;
		}
		iocp = (struct iocblk*)bp->b_rptr;
		switch (iocp->ioc_cmd) {
		case TCXONC:
			ASSERT(iocp->ioc_count == sizeof(int));
			switch (*(int*)(bp->b_cont->b_rptr)) {
			case 0:		/* stop output */
				pt->pts_state |= PTS_TXSTOP;
				break;
			case 1:		/* resume output */
				pt_resume(pt);
				break;
			default:
				iocp->ioc_error = EINVAL;
				break;
			}
			bp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = 0;
			qreply(wq, bp);
			break;

		case TCSETA:
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			pt_tcset(pt,bp);
			qreply(wq,bp);
			break;

		case TCSETAW:
		case TCSETAF:
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			putq(wq, bp);
			break;

		case TCGETA:
			tcgeta(wq, bp, &pt->pt_termio);
			break;

		case TCSBRK:
			putq(wq, bp);
			break;

		case TIOCGWINSZ:	/* get window size */
			pt_gwinsz(wq,bp,pt);
			break;

		case TIOCSWINSZ:		/* set window size */
			pt_swinsz(wq,bp,pt);
			break;
		case TIOCREMOTE:
			/*
			 * Send M_CTL up using the iocblk format
			 */
			if (( mctlp = allocb( sizeof( struct iocblk), BPRI_MED)) == (mblk_t *)NULL) {
				iocp->ioc_count = 0;
				iocp->ioc_error = EAGAIN;
				bp->b_datap->db_type = M_IOCNAK;
				qreply( wq, bp);
				break;
			}

			mctlp->b_wptr = mctlp->b_rptr + sizeof( struct iocblk);
			mctlp->b_datap->db_type = M_CTL;
			iocp = ( struct iocblk *)mctlp->b_rptr;
			if ( *(int *)bp->b_cont->b_rptr)
				iocp->ioc_cmd = MC_NO_CANON;
			else
				iocp->ioc_cmd = MC_DO_CANON;
			/*
			 * Send the M_CTL upstream to STTY_LD
			 */
			putnext( RD(wq), mctlp);

			/*
			 * Format IOCACK message and pass back to
			 * the master
			 */
			iocp = (struct iocblk *)bp->b_rptr;
			iocp->ioc_count = 0;
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;

			bp->b_datap->db_type = M_IOCACK;
			qreply(wq, bp);
			break;
		case TCSETLABEL:
#ifdef DEBUG_PTY_MAC
			{
			mac_label *mlp = (mac_label *)bp->b_cont->b_rptr;
			cmn_err(CE_NOTE,
			    "pts_wput - TCSETLABEL uid=%d new=%c%c old=%c%c",
			    iocp->ioc_uid,
			    mlp->ml_msen_type, mlp->ml_mint_type,
			    pt->pt_mac->ml_msen_type,
			    pt->pt_mac->ml_mint_type);
			}
#endif /* DEBUG_PTY_MAC */
			/*
			 * XXX: Capability work required here.
			 */
			if (iocp->ioc_uid != 0)
				iocp->ioc_error = EPERM;
			else if (iocp->ioc_count != sizeof(mac_label *))
				iocp->ioc_error = EINVAL;
			else
				bcopy(bp->b_cont->b_rptr, (char *)&pt->pt_mac,
				    sizeof(pt->pt_mac));
			bp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = 0;
			qreply(wq, bp);
			break;
		default:
			bp->b_datap->db_type = M_IOCNAK;
			qreply(wq,bp);
			break;
		}
		break;


	default:
		sdrv_error(wq,bp);
	}
	return 0;
}


/* send characters to the controller, from the slave
 *	We come here only if the controller got behind, or if we encountered
 *	an IOCTL that needed to be done in sequence.
 */
static int
pts_wsrv(register queue_t *wq)
{
	struct pty *pt = (struct pty*)wq->q_ptr;
	mblk_t *bp;
	queue_t *crq = pt->ptc_rq;

	ASSERT(pt->pt_index < maxpty && pty[pt->pt_index] == pt);
	PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTS_WSRV, 0, 0, 0);
	ASSERT(STREAM_LOCKED(wq));

	if (!crq)			/* forget it if no controller */
		return 0;

	if (pts_dopktset(pt, wq, crq) < 0)
		return 0;

	for (;;) {
		if (pts_dopktdata(pt, wq, crq) < 0)
			break;

		if (!(bp = getq(wq))) {	/* quit after last message */
			enableok(wq);
			break;
		}

		switch (bp->b_datap->db_type) {
		case M_IOCTL:
			{
				register struct iocblk *iocp;
				iocp = (struct iocblk*)bp->b_rptr;

				if (pt->ptc_state & PTC_PKT) {	/* do packet stuff */
					pt->pt_pkt |= TIOCPKT_IOCTL;
				}
				switch (iocp->ioc_cmd) {
				case TCSETAW:
					pt_tcset(pt,bp);
					break;
				case TCSETAF:
					(void)putctl1(RD(wq)->q_next,
						      M_FLUSH,FLUSHR);
					if (pt->ptc_state & PTC_PKT)
						pt->pt_pkt|=TIOCPKT_FLUSHREAD;
					pt_tcset(pt,bp);
					break;
				case TCSBRK:	/* let output empty */
					bp->b_datap->db_type = M_IOCACK;
					iocp->ioc_count = 0;
					break;
				default:
					panic("???");
				}
				qreply(wq,bp);
			}
			break;


		case M_DATA:
			if ((pt->pts_state & PTS_TXSTOP)
			    || !canput(crq->q_next)) {
				putbq(wq, bp);
				noenable(wq);
				goto for_exit;
			}

			if (pts_senddata(pt, wq, crq, bp, SERV_PROC) < 0)
				goto for_exit;
			break;
#ifdef DEBUG
		default:		/* the put fnc checked all msgs */
			panic("?");
#endif
		}
	}
for_exit:;

	return 0;
}


static int
pts_doreadproc(queue_t *rq, mblk_t **bpp, struct pty *pt, int callfrom)
{
	register mblk_t *obp, *ibp;
	register u_char *cp, *lim;

	ibp = *bpp;
	ASSERT(M_DATA == ibp->b_datap->db_type);

	if (!(obp = dupb(ibp))) {	/* quit if no msg blocks */
		STASH(rq, ibp);
		(void)bufcall(0, BPRI_LO, (void(*)())qenable,(long)rq);
		return(-1);
	}

	cp = obp->b_rptr;
	lim = obp->b_wptr;
	while (cp < lim) {
		/* start or stop output (if permitted) when we get
		 * XOFF or XON */
		if (pt->pt_iflag & IXON) {
			register u_char cs = *cp;

			if (pt->pt_iflag & ISTRIP)
				cs &= 0x7f;

			if ((PTS_TXSTOP & pt->pts_state)
			    && (cs == pt->pt_cc[VSTART]
				|| ((IXANY & pt->pt_iflag)
				    && (cs != pt->pt_cc[VSTOP]
					|| pt->pt_line == LDISC0)))) {
				pt_resume(pt);
				if (cs == pt->pt_cc[VSTART]) {
					ibp->b_rptr++;
					break;
				}
			} else if (PTS_LIT & pt->pts_state) {
				pt->pts_state &= ~PTS_LIT;
			} else if (cs == pt->pt_cc[VSTOP]) {
				pt->pts_state |= PTS_TXSTOP;
				ibp->b_rptr++;
				break;
			} else if (cs == pt->pt_cc[VSTART]) {
				ibp->b_rptr++;
				break;	/* ignore extra control-Qs */
			} else if (cs == pt->pt_cc[VLNEXT]
				   && LDISC0 != pt->pt_line) {
				pt->pts_state |= PTS_LIT;
			}
		}

		cp++;
	}

	ibp->b_rptr += (cp - obp->b_rptr);
	if (ibp->b_rptr >= ibp->b_wptr) {
		register mblk_t *nbp;
		nbp = rmvb(ibp,ibp);
		freemsg(ibp);
		*bpp = nbp;
	}

	obp->b_wptr = cp;
	if (cp > obp->b_rptr) {	/* send the data up stream */
		putnext(rq, obp);
	} else {
		freemsg(obp);
	}

	return(0);
}

static
pts_rput(queue_t *rq, mblk_t *bp)
{

	mblk_t *ibp;

	if (rq->q_first || !canput(rq->q_next)) {
		putq(rq, bp);
		return(0);
	}
	ibp = bp;
	while(ibp)
		pts_doreadproc(rq, &ibp, (struct pty*)rq->q_ptr, PUT_PROC);
	return(0);
}

/* slave read service function
 *	Take data from the controller, and send it up to the slave.
 *	We have to scan the data, looking for XON and XOFF.  Worse, we must
 *	discard XON and XOFF when we find them.
 */
static int
pts_rsrv(queue_t *rq)
{
	struct pty *pt = (struct pty*)rq->q_ptr;
	mblk_t *ibp;

	ASSERT(pt->pt_index < maxpty && pty[pt->pt_index] == pt);

	ibp = NULL;
	PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTS_RSRV, 0, 0, 0);
	for (;;) {
		if (!ibp) {
			if (!canput(rq->q_next))
				break;	/* quit if slave constipated */
			if (!(ibp = getq(rq))) {	/* quit if no more, */
				register queue_t *cwq;	/* after awakening */
				cwq = pt->ptc_wq;	/* controller */
				if (0 != cwq
				    && 0 != cwq->q_first)
					qenable(cwq);
				break;
			}
		}
		if (pts_doreadproc(rq, &ibp, pt, SERV_PROC) < 0)
			break;
	}
	return 0;
}

/* controller open
 */
/*ARGSUSED*/
static int				/* return minor # or failure reason */
ptc_open(register queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *crp)
{
	struct pty *pt;
	queue_t *wq = WR(rq);
	uint d, base, lim;
	int error, s;

	/* refuse to do anything but a "clone" open, to keep the security
	 * hole of controlling nodes closed.
	 */
	if (!sflag)
		return(ENODEV);
	switch (emajor(*dev)) {
	case PTC_MAJOR:
	default:
		base = NUM_PTY_PER_MAJOR*0;
		lim = maxpty;
		break;
	case PTC1_MAJOR:
		base = NUM_PTY_PER_MAJOR*1;
		lim = NUM_PTY_PER_MAJOR*2;
		break;
	case PTC2_MAJOR:
		base = NUM_PTY_PER_MAJOR*2;
		lim = NUM_PTY_PER_MAJOR*3;
		break;
	case PTC3_MAJOR:
		base = NUM_PTY_PER_MAJOR*3;
		lim = NUM_PTY_PER_MAJOR*4;
		break;
	case PTC4_MAJOR:
		base = NUM_PTY_PER_MAJOR*4;
		lim = NUM_PTY_PER_MAJOR*5;
		break;
	}
	if (lim > maxpty)
		lim = maxpty;

	d = base;
	s = PTY_LOCK();
	while (pty[d]) {
		d++;
		if (d >= lim) {
			PTY_UNLOCK(s);
			return(ENODEV);
		}
	}

	if ((error = pt_alloc(CONTROLLER, d, &pt, &s, crp))) {
		PTY_UNLOCK(s);
		return(error);
	}

	ASSERT (!(pt->pts_state & PT_DEAD));

	if (!(pt->ptc_state & PT_UNBORN)) {    /* controller already open */
		/*
		 * XXXrs - If already open, why would anyone be sleeping
		 * on the pty entry?  The wakeup seems useless.
		 */
		wakeup((caddr_t)pt);	/* awaken slave */
		PTY_UNLOCK(s);
		return(EIO);
	}

	PTY_UNLOCK(s);
	/* XXXrs - error return? */
	JOINSTREAMS(rq, &pt->pts_rq);
	s = PTY_LOCK();

	PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTC_OPEN, rq, wq, 0);

	rq->q_ptr = (caddr_t)pt;	/* connect new device to stream */
	wq->q_ptr = (caddr_t)pt;
	pt->ptc_rq = rq;
	pt->ptc_wq = wq;
	pt->pts_auto_ld = 1;
	pt->ptc_state &= ~(PT_UNBORN|PT_DEAD);
	PTY_UNLOCK(s);

	wakeup((caddr_t)pt);		/* awaken slave */

	*dev = makedev(ptc_majors[d/NUM_PTY_PER_MAJOR], d%NUM_PTY_PER_MAJOR);
	return(0);
}



/* close down a pty, for the controller
 *	- close this end down and adjust state so that slave
 *	  will find out on next operation
 */
/* ARGSUSED */
static int
ptc_close(queue_t *rq, int flag, cred_t *crp)
{
	int s;
	struct pty *pt = (struct pty*)rq->q_ptr;
	uint d = pt->pt_index;

	ASSERT(d < maxpty && pty[d] == pt);

	s = PTY_LOCK();
	if (pt->pts_state & (PT_UNBORN|PT_DEAD)
	    && !(pt->pts_state & PTS_PUSHING)) {
		pty[d] = 0;
		PTY_UNLOCK(s);
		if (pt->pt_bufp)
			freemsg(pt->pt_bufp);
		kmem_free(pt, sizeof(struct pty));
	} else {			/* let slave know */
		PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTC_CLOSE,
			     pt->ptc_rq, pt->ptc_wq, 0);
		pt->ptc_state |= PT_DEAD;
		pt->ptc_state &= ~PTC_QUEUED;
		pt->ptc_rq = 0;
		pt->ptc_wq = 0;
		pt->pt_winsize.ws_row = pt->pt_winsize.ws_col = 0;
		pt->pt_winsize.ws_xpixel = pt->pt_winsize.ws_ypixel = 0;
		PTY_UNLOCK(s);
		if (0 != pt->pts_rq)
			(void)putctl(pt->pts_rq->q_next, M_HANGUP);
	}
	return 0;
}



/* controller output 'put' function
 *	queue data for the slave to 'input'
 */
static int
ptc_wput(register queue_t *wq, register mblk_t *bp)
{
	register struct pty *pt;
	register struct iocblk *iocp;
	register queue_t *srq;		/* slave read queue */
	register mblk_t *mctlp;		/* Pointer to a M_CTL message */

	pt = (struct pty*)wq->q_ptr;
	srq = pt->pts_rq;

	PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTC_WPUT, 0, 0, 0);

	ASSERT(pt->pt_index < maxpty && pty[pt->pt_index] == pt);

	switch (bp->b_datap->db_type) {

	case M_FLUSH:
		sdrv_flush(wq,bp);
		if (srq)
			qenable(srq);
		break;

	case M_DATA:			/* send data to slave */
		SYSINFO.ptc += msgdsize(bp);
		if (!srq) {
			freemsg(bp);
		} else {
			if (!wq->q_first && canput(srq)){
				put(srq, bp);   /* XXXrs */
			} else {		/* if slave is constipated, */
				noenable(wq);	/* queue it here to */
				putq(wq, bp);	/* throttle controller head */
			}
		}
		break;

	case M_DELAY:			/* ignore timing requests */
		freemsg(bp);
		break;

	case M_IOCTL:
		if (pt->ptc_state & PTC_PKT) {	/* do packet stuff */
			pt->pt_pkt |= TIOCPKT_IOCTL;
		}
		iocp = (struct iocblk*)bp->b_rptr;
		/*
		 * XXX The (unsigned int) cast is a workaround for a MIPS
		 * XXX compiler bug.  With 1.21 compilers, you never get
		 * XXX to the TIOCSWINSZ case without the cast.
		 */
		switch ((unsigned int)iocp->ioc_cmd) {	/* XXX */

		case TIOCGWINSZ:		/* get window size */
			pt_gwinsz(wq,bp,pt);
			break;

		case TIOCSWINSZ:		/* set window size */
			pt_swinsz(wq,bp,pt);
			break;

		case TCSETAF:
			flushq(wq,FLUSHDATA);	/* flush and fall thru */
		case TCSETA:
		case TCSETAW:
			ASSERT(iocp->ioc_count == sizeof(struct termio));
			pt_tcset(pt,bp);
			/* This is a bit of a kludge.  To tell the line
			 * discipline code of the new mode, we send an ACK up
			 * the other stream. */
			if (NULL != srq) {
				register mblk_t *obp = dupmsg(bp);
				if (NULL != obp)
					putnext(srq,obp);
			}
			qreply(wq,bp);
			break;

		case TCGETA:
			tcgeta(wq,bp, &pt->pt_termio);
			break;

		case TCSBRK:		/* similute break to slave */
			if (0 == *(int*)bp->b_cont->b_rptr
			    && BRKINT == (pt->pt_iflag & (IGNBRK|BRKINT))
			    && srq != NULL) {
				flushq(srq, FLUSHDATA);
				(void)putctl1(srq->q_next, M_FLUSH, FLUSHRW);
				(void)putctl1(srq->q_next, M_PCSIG, SIGINT);
			}
			bp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = 0;
			qreply(wq,bp);
			break;

		case TIOCPKT:
			if (0 != *(int*)bp->b_cont->b_rptr) {
				pt->ptc_state |= PTC_PKT;
				pt->pt_pkt = TIOCPKT_DATA;
			} else {
				pt->ptc_state &= ~PTC_PKT;
			}
			pt->ptc_state |= PTC_PKT_SET;
			if (srq)
				qenable(srq);
			bp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = 0;
			qreply(wq,bp);
			break;

		case TIOCREMOTE:
			/*
			 * Send M_CTL up using the iocblk format
			 */
			if (( mctlp = allocb( sizeof( struct iocblk), BPRI_MED)) == (mblk_t *)NULL) {
				iocp->ioc_count = 0;
				iocp->ioc_error = EAGAIN;
				bp->b_datap->db_type = M_IOCNAK;
				qreply( wq, bp);
				break;
			}

			mctlp->b_wptr = mctlp->b_rptr + sizeof( struct iocblk);
			mctlp->b_datap->db_type = M_CTL;
			iocp = ( struct iocblk *)mctlp->b_rptr;
			if ( *(int *)bp->b_cont->b_rptr)
				iocp->ioc_cmd = MC_NO_CANON;
			else
				iocp->ioc_cmd = MC_DO_CANON;
			/*
			 * Send the M_CTL upstream to STTY_LD
			 */
			putnext( RD(wq), mctlp);

			/*
			 * Format IOCACK message and pass back to
			 * the master
			 */
			iocp = (struct iocblk *)bp->b_rptr;
			iocp->ioc_count = 0;
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;

			bp->b_datap->db_type = M_IOCACK;
			qreply(wq, bp);
			break;

		case SVR4SOPEN:
			pt->pts_auto_ld = 0;
			bp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = 0;
			qreply(wq,bp);
			break;

		case UNLKPT:
			pt->pts_state &= ~PTS_LOCKED;
			/* fall through to ack... */
		case ISPTM:
			bp->b_datap->db_type = M_IOCACK;
			iocp->ioc_count = 0;
			qreply(wq,bp);
			break;

		default:
			bp->b_datap->db_type = M_IOCNAK;
			qreply(wq,bp);
			break;
		}
		break;


	default:
		sdrv_error(wq,bp);
	}
	return 0;
}



/* service controller output queue
 *	The controller enqueues its output directly up toward the slave stream
 *	head.  Therefore, this function is awakened only when the controller
 *	used canput() and was turned down, and things have now drained.
 *	It is never enabled by a putq(), and so never needs enableok().
 */
static int
ptc_wsrv(register queue_t *rq)
{
	register mblk_t *bp;
	register struct pty *pt = (struct pty*)rq->q_ptr;
	register queue_t *wq, *srq;
	register int s;

	STR_LOCK_SPL(s);

	ASSERT(STREAM_LOCKED(rq));
	ASSERT(pt->pt_index < maxpty && pty[pt->pt_index] == pt);

	wq = pt->ptc_wq;
	srq = pt->pts_rq;
	PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTC_WSRV, 0, 0, 0);
	if (!srq) {
		flushq(wq, FLUSHALL);
		STR_UNLOCK_SPL(s);
		return 0;
	}

	if (canput(srq)) {		/* send all we have, in 1 burst */
		while (0 != (bp = getq(wq)))
			putq(srq, bp);
	}
	STR_UNLOCK_SPL(s);
	return 0;
}



/* service controller input queue
 *	The slave enqueues its output directly on the controller stream head.
 *	Therefore, this function is awakened only when the slave used canput()
 *	on the contoller head and was turned down.  The controller stream
 *	has now drained and has 'back-enabled' the controller queue, awakening
 *	this function.
 */
static int
ptc_rsrv(register queue_t *rq)
{
	register struct pty *pt = (struct pty*)rq->q_ptr;
	register queue_t *swq;

	ASSERT(STREAM_LOCKED(rq));
	ASSERT(pt->pt_index < maxpty && pty[pt->pt_index] == pt);

	swq = pt->pts_wq;
	PRIV_LOG_ACT(&pt->pt_actlog, PTY_AL_PTC_RSRV, swq, 0, 0);
	if (swq)			/* just activate slave output */
		qenable(swq);
	return 0;
}


/*
 * This function is special for kernel printfs to come out when the graphics
 * console is enabled. It is called only by prom_putc() in the sprom driver
 * when its time to flush a kernel printf out to the graphics port.
 * The ptc rq is given to this routine and must be turned into the pts wq.
 */
cons_pts_wput(register queue_t *cwq, register mblk_t *bp)
{
	register struct pty *pt = (struct pty*)cwq->q_ptr;
	register queue_t *srq;
	ASSERT(pt->ptc_wq == cwq);

	/*
	 * If the slave write q pntr is null then no one has the slave open.
	 * Return -1 in this case.
	 * This should be a temporary transient condition which would occur
	 * in between the time wsh goes away and grcond has time to respond
	 * to it.
	 */
	if (pt->pts_wq == NULL)
		return(-1);

	/*
	 * Find the stream head via read queue links
	 */
	for (srq = pt->pts_rq; srq->q_next; srq = srq->q_next)
		continue;

	/*
	 * Turn it around and send the mblk on down through the
	 * line discipline module.
	 */
	putq(WR(srq)->q_next, bp);
	return(0);
}
