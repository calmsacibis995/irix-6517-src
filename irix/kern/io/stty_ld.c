/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA.
 *
 * streams 'line discipline'
 *
 * $Revision: 3.84 $
 */
#include <limits.h>
#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/strids.h>
#include <sys/stream.h>
#include <sys/strmp.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/stty_ld.h>
#include <sys/strtty.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/termio.h>
#include <sys/ddi.h>
#include "string.h"

#define MAX_LLEN (MAX_CANON+1)		/* limit on canonical type ahead */
#define MAX_AHEAD 1024			/* limit on raw type ahead */
#define MAX_RETYPE (MAX_CANON*5)	/* max needed for retyping */

#define BEL 0x7


extern int posix_tty_default;
/* Default/initial streams 'line discipline' settings */

/*
 * WARNING: There's a parallel structure in cmd/init/init.c that should
 *          mirror this structure.  If the default settings change here,
 *          they should probably change there as well.
 *
 * WARNING: There's some nasty-hacky code in main() that will overwrite
 *          some of the default settings below when posix_tty_default is
 *          set.  This was done to pass certain brain-dead standards
 *          verification tests.
 */
struct stty_ld def_stty_ld = {
	{				/* st_termio			*/
		(ICRNL|IXON|IXANY	/* st_iflag: input modes	*/
		 |BRKINT|IGNPAR|ISTRIP),
		OPOST|ONLCR|TAB3,	/* st_oflag: output modes	*/
		__OLD_SSPEED|CS8|HUPCL|CLOCAL,	/* st_cflag: control modes	*/
		(ISIG|ICANON|ECHO	/* st_lflag: line discipline modes */
		 |ECHOE|ECHOK|ECHOKE|ECHOCTL
#ifdef IEXTEN
		 |IEXTEN
#endif
		 ),
		SSPEED,			/* st_ospeed: output speed	*/
		0,			/* st_ispeed: input speed	*/
		LDISC1,			/* st_line:  line discipline	*/

					/* st_cc: special characters	*/
		{CINTR,			/* 0 */
		 CQUIT,
		 CERASE,
		 CKILL,
		 CEOF,
		 0,			/* 5 */
		 0,
		 CNSWTCH,
		 CSTART,
		 CSTOP,
		 CNSWTCH,		/* 10 */
		 CNUL,
		 CRPRNT,
		 CFLUSH,
		 CWERASE,
		 CLNEXT,		/* 15 */
		 0,
		 0,
		 0,
		 0,
		 0,			/* 20 */
		 0,
		 0},			/* 22 */
	},
};

static struct module_info stm_info = {
	STRID_STTY_LD,			/* module ID */
	"STTY_LD",			/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size	*/
	1,				/* hi-water mark */
	0,				/* lo-water mark */
        MULTI_THREADED,			/* locking */
};



static int st_retype(struct stty_ld*, int, u_char, int);
static int st_wdelay(struct stty_ld*, mblk_t*, u_char*, int, char*);

int st_rput(queue_t*, mblk_t*);
int st_rsrv(queue_t*);
int st_open(queue_t*, dev_t *, int, int, cred_t *);
int st_close(queue_t*, int, cred_t *);

static int  st_nonstuff(struct stty_ld *, u_char, mblk_t *);
static void st_rflush(struct stty_ld *, u_char, queue_t *);

static struct qinit st_rinit = {
	st_rput, st_rsrv, st_open, st_close, NULL, &stm_info, NULL
};

int st_wput(queue_t *, mblk_t *);
void st_wsrv(queue_t *);

void st_write_common(queue_t *, struct stty_ld *, mblk_t *);

static struct qinit st_winit = {
	st_wput, (int (*)())st_wsrv, NULL, NULL, NULL, &stm_info, NULL
};

int stty_lddevflag = 0;
struct streamtab stty_ldinfo = {&st_rinit, &st_winit, NULL, NULL};



#define ECHO_EOF "^D\b\b"		/* echo this for EOF */



/* types of characters
 */
#define NP	1			/* non-printing, so do not space   */
#define SC	2			/* simple, spacing character	   */
#define LC	3			/* lower case character		   */
#define CT	4			/* print as '^' followed by letter */
#define BS	5			/* backspace			   */
#define LF	6			/* line feed/newline		   */
#define CR	7			/* carriage return		   */
#define HT	8			/* tab				   */
#define VT	9			/* vertical tab			   */
#define FF	10			/* form feed			   */

#define SH_DELTA 16
#define SH(c)	(SH_DELTA+(c))		/* print as \ followed by letter   */


/* the common case when we do not care about case
 */
static u_char st_types[256] = {
	CT, CT, CT, CT,			/* NUL, SOH, STX, ETX */
	CT, CT, CT, CT,			/* EOT, ENQ, ACK, BEL */
	BS, HT, LF, VT,			/* BS, HT, LF, VT */
	FF, CR, CT, CT,			/* FF, CR, SO, SI */
	CT, CT, CT, CT,			/* DLE, DC1, DC2, DC3 */
	CT, CT, CT, CT,			/* DC4, NAK, SYN, ETB */
	CT, CT, CT, CT,			/* CAN, EM, SUB, ESC */
	CT, CT, CT, CT,			/* FS, GS, RS, US */

	SC, SC, SC, SC, SC, SC, SC, SC,	/* space thru ? */
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,

	SC, SC, SC, SC, SC, SC, SC, SC,	/* @ thru _ */
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,

	SC, SC, SC, SC, SC, SC, SC, SC,	/* ` thru  DEL */
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, CT,


	NP, NP, NP, NP, NP, NP, NP, NP,	/* 0x80 */
	NP, NP, NP, NP, NP, NP, NP, NP,
	NP, NP, NP, NP, NP, NP, NP, NP,
	NP, NP, NP, NP, NP, NP, NP, NP,

	SC, SC, SC, SC, SC, SC, SC, SC,	/* 0xa0 */
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,

	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,

	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
};

/* case matters
 */
static u_char st_types_c[256] = {
	CT, CT, CT, CT,			/* NUL, SOH, STX, ETX */
	CT, CT, CT, CT,			/* EOT, ENQ, ACK, BEL */
	BS, HT, LF, VT,			/* BS, HT, LF, VT */
	FF, CR, CT, CT,			/* FF, CR, SO, SI */
	CT, CT, CT, CT,			/* DLE, DC1, DC2, DC3 */
	CT, CT, CT, CT,			/* DC4, NAK, SYN, ETB */
	CT, CT, CT, CT,			/* CAN, EM, SUB, ESC */
	CT, CT, CT, CT,			/* FS, GS, RS, US */

	SC, SC, SC, SC, SC, SC, SC, SC,	/* space thru ? */
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,

	SC,      SH('A'), SH('B'), SH('C'),	/* @ABC */
	SH('D'), SH('E'), SH('F'), SH('G'),	/* DEFG */
	SH('H'), SH('I'), SH('J'), SH('K'),	/* HIJK */
	SH('L'), SH('M'), SH('N'), SH('O'),	/* LMNO */
	SH('P'), SH('Q'), SH('R'), SH('S'),	/* PQRS */
	SH('T'), SH('U'), SH('V'), SH('W'),	/* TUVW */
	SH('X'), SH('Y'), SH('Z'),   SC,	/* XYZ[ */
	SH(CESC),  SC,      SC,      SC,	/*  ]^_ */

	SH(0x27), LC, LC, LC,		/* `abc */
	LC, LC, LC, LC,			/* defg */
	LC, LC, LC, LC,			/* hijk */
	LC, LC, LC, LC,			/* lmno */
	LC, LC, LC, LC,			/* pqrs */
	LC, LC, LC, LC,			/* tuvw */
	LC, LC, LC, SH('('),		/* xyz{ */
	SH('!'), SH(')'), SH('^'), CT,	/* |}~  */


	NP, NP, NP, NP, NP, NP, NP, NP,	/* 0x80 */
	NP, NP, NP, NP, NP, NP, NP, NP,
	NP, NP, NP, NP, NP, NP, NP, NP,
	NP, NP, NP, NP, NP, NP, NP, NP,

	SC, SC, SC, SC, SC, SC, SC, SC,	/* 0xa0 */
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,

	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,

	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
	SC, SC, SC, SC, SC, SC, SC, SC,
};


/*
 * override certain default stty_ld settings if we are in POSIX conformance mode
 */
void
stty_ld_init(void)
{
        if (posix_tty_default) {

		def_stty_ld.st_iflag &= ~IXANY;
		def_stty_ld.st_iflag &= ~IGNPAR;
		def_stty_ld.st_oflag = OPOST|ONLCR|TAB3;
		/* XXXrs - Can't mess with cflag!! */
		def_stty_ld.st_lflag = ISIG|ICANON|ECHO|ECHOK;

                def_stty_ld.st_cc[VSWTCH] = _POSIX_VDISABLE;
                def_stty_ld.st_cc[VDSUSP] = _POSIX_VDISABLE;
                def_stty_ld.st_cc[VREPRINT] = _POSIX_VDISABLE;
                def_stty_ld.st_cc[VDISCARD] = _POSIX_VDISABLE;
	/*
	 * XXXrs - Disabling WERASE is necessary to make POSIX.os/
	 *	   devclass/i_spchars 19 pass because that test is semi-
	 *	   brain-dead, but it breaks GABI.os/devclass/termio 3
	 *	   because that test is major-brain-dead.
	 *
	 *	   Continue to disable it and ignore the busted GABI test
	 *	   failure.
	 */
                def_stty_ld.st_cc[VWERASE] = _POSIX_VDISABLE;
                def_stty_ld.st_cc[VLNEXT] = _POSIX_VDISABLE;
        }
}

/* make a high priority buffer request */
#define BCALL_HI(l,q) (void)bufcall((l), BPRI_HI, (void(*)())qenable,(long)(q))

#define CHAR_IS_DISABLED(c) ((((unsigned int) (c)) & 0xFF) == _POSIX_VDISABLE)

/* pass a data message to the next queue
 */
static int				/* 1=success, 0=cannot send it now */
st_pass(register queue_t *q, register mblk_t *bp)
{
	register struct stty_ld *stp = (struct stty_ld*)q->q_ptr;

	ASSERT(M_DATA == bp->b_datap->db_type);

	if (!(stp->st_state & ST_CLOSE_DRAIN) && !canput(q->q_next)) {
		noenable(q);
		return 0;
	}

	putnext(q, bp);
	return 1;
}

static void st_untime(struct stty_ld *);

/* process a M_FLUSH message
 *	We must flush from the service routines because:
 *	  (1) the put() function of a queue can be called from an interrupt
 *		routine
 *	  (2) we need to do more than flush the queues themselves.  We must
 *		change our private data structures
 *	  (3) therefore we must either have all the service functions run
 *		at splstr() or we must queue flush requests.  We might spend a
 *		long time working on a single, big data block.
 *	  (4) therefore, we should queue flush requests.
 */
static void
st_rflush(register struct stty_ld *stp, register u_char flag,
	  register queue_t *rq)
{
	if (flag & FLUSHR) {
		flushq(rq, FLUSHDATA);

		freemsg(stp->st_imsg);
		stp->st_imsg = 0;
		stp->st_ahead = 0;

		freemsg(stp->st_lmsg);
		stp->st_lmsg = 0;
		stp->st_llen = 0;

		st_untime(stp);
		qenable(rq);
	}
}


static void
st_wflush(register struct stty_ld *stp, register u_char flag,
	  register queue_t *wq)
{
	if (flag & FLUSHW) {
		flushq(wq, FLUSHDATA);

		freemsg(stp->st_emsg);
		stp->st_emsg = NULL;

		if (stp->st_state & ST_TABWAIT) {
			stp->st_state &= ~ST_TABWAIT;
			qenable(stp->st_rq);
		}

		qenable(wq);
	}
}

/* open a new stream
 */
/* ARGSUSED */
int
st_open(queue_t *rq,			/* our read queue */
	dev_t *devp,
	int flag,
	int sflag,
	cred_t *crp)
{
	register struct stty_ld *stp;

	if (MODOPEN != sflag)
		return ENXIO;

	if (!(stp = (struct stty_ld*)rq->q_ptr)) {
		register queue_t *wq;	/* initialize on the 1st open() */

		if (!(stp = (struct stty_ld*)malloc(sizeof(*stp))))
			return ENOMEM;
		/* choose an initial state */
		bcopy((char*)&def_stty_ld, (char*)stp, sizeof(*stp));
		wq = WR(rq);
		rq->q_ptr = (caddr_t)stp;
		wq->q_ptr = (caddr_t)stp;
		stp->st_rq = rq;
		stp->st_wq = wq;

		qenable(rq);		/* must tell stream head about tty */
	}

	strctltty(rq);			/* make it a controlling tty if poss */

	return 0;
}



/* close a stream
 *	This is called when the stream is being dismantled or when this
 *	module is being popped.
 */
/* ARGSUSED */
int
st_close(queue_t *rq, int flag, cred_t *crp)
{
	register struct stty_ld *stp = (struct stty_ld*)rq->q_ptr;
	int err = 0;

	ASSERT(stp->st_rq == rq);

	/*
	 * If there is data waiting to go downstream, force it rather
	 * than lose it if possible (ignore downstream flow control for
	 * the buffers already on our write queue).  Note that the service
	 * procedure gets one pass to move all the data downstream.  If
	 * a low memory situation prevents the service procedure from
	 * clearing the write queue, no heroic effort is made.  Any
	 * remaining data is discarded and the module is closed as before.
	 */
	if (0 != stp->st_wq->q_count) {
		stp->st_state |= ST_CLOSE_DRAIN;
		qenable(stp->st_wq);
		if (sleep((caddr_t)stp->st_wq, STIPRI))
			err = EINTR;
		stp->st_state &= ~ST_CLOSE_DRAIN;
	}

	st_untime(stp);			/* forget cannonical timer */

	freemsg(stp->st_imsg);		/* release all of our buffers */
	freemsg(stp->st_lmsg);
	freemsg(stp->st_emsg);

	str_unbcall(rq);		/* stop waiting for buffers */
	str_unbcall(stp->st_wq);

	kmem_free(stp, sizeof(struct stty_ld));	/* all finished */
	return(err);
}


/* accept a new output message
 */
int
st_wput(register queue_t *wq, register mblk_t *bp)
{
	register struct stty_ld *stp = (struct stty_ld*)wq->q_ptr;

	ASSERT(stp->st_wq == wq);

	switch (bp->b_datap->db_type) {
	case M_DATA:
		if ((bp->b_rptr >= bp->b_wptr	/* discard empty messages */
		     && !msgdsize(bp))
		    || (stp->st_lflag & FLUSHO)) {	/* or flushed output */
			freemsg(bp);
			break;
		}
		if (0 == wq->q_first && OPOST & stp->st_oflag)
			st_write_common(wq, stp, bp);
		else if ((OPOST & stp->st_oflag)	/* if nothing to do, */
		         || 0 != wq->q_first
		         || !st_pass(wq, bp))	/* pass it along */
			putq(wq,bp);	/* unless we must look at it */
		break;

	case M_FLUSH:
		enableok(wq);
		putq(wq, bp);
		break;

	case M_IOCTL:
		switch (((struct iocblk*)bp->b_rptr)->ioc_cmd) {
		case TCSETAW:		/* must let output drain before	*/
		case TCSETAF:		/* some IOCTLs */
		case TCSBRK:
		case TIOCSTI:
			putq(wq,bp);
			break;

		default:		/* send other msgs down */
			putnext(wq,bp);
			break;
		}
		break;

	default:
		putnext(wq,bp);
		break;
	}
	return(0);
}



/* send whatever output we have
 */
static int				/* 0=failed, 1=state complete */
st_wsend(struct stty_ld *stp,
	 mblk_t *wbp,			/* what we have done so far */
	 u_char *cp)			/* bytes up to, but excluding, this */
{
	 mblk_t *bp1;

	if (cp > wbp->b_rptr) {		/* work only if necessary */
		if (!(stp->st_state & ST_CLOSE_DRAIN) &&
		    !canput(stp->st_wq->q_next)) {
			noenable(stp->st_wq);
			return 0;	/* quit if constipated */
		}

		if (!(bp1 = dupb(wbp))) {
			BCALL_HI(0, stp->st_wq);
			return 0;
		}

		bp1->b_wptr = cp;
		putnext(stp->st_wq, bp1);

		wbp->b_rptr = cp;
	}

	stp->st_ocol = stp->st_ncol;
	return 1;
}



/* send what we have, and allocate a new output block
 */
static mblk_t*				/* 0=failed */
st_wnew(struct stty_ld *stp,
	int sz,				/* this size block */
	mblk_t *wbp,			/* what we have so far */
	u_char *cp)			/* up to, but excluding this byte */
{
	if (!st_wsend(stp,wbp,cp))	/* send what we have so far */
		return 0;

	return str_allocb(sz,stp->st_wq,BPRI_LO);
}



/* send a delay
 */
static int				/* 0=nothing done, 1=all done */
st_wdelay(struct stty_ld *stp,
	  mblk_t *wbp,			/* what we have done so far */
	  u_char *cp,			/* send thru this target byte */
	  int	del,			/* and then delay this many "ticks" */
	  char*	sub)			/* replace target with this string */
{
	mblk_t *bp1, *bp0;

	/*
	 * If downstream congested, disable ourself and fail so as not
	 * to flood the driver.
	 */
	if (!(stp->st_state & ST_CLOSE_DRAIN) && !canput(stp->st_wq->q_next)) {
		noenable(stp->st_wq);
		return 0;
	}

	if (!sub) bp0 = 0;
	else {
		/* we never need >2 bytes */
		bp0 = str_allocb(2, stp->st_wq, BPRI_LO);
		if (!bp0)
			return 0;
		*bp0->b_wptr++ = *sub++;
		if (*sub != '\0')
			*bp0->b_wptr++ = *sub;
		cp--;
	}

	if (!del)
		bp1 = 0;
	else {				/* get msg for delay */
		bp1 = str_allocb(sizeof(int), stp->st_wq, BPRI_LO);
		if (!bp1) {
			if (bp0)
				freeb(bp0);
			return 0;
		}

		/*
		 * This mapping between fill characters and "ticks" seems
		 * like nonsense, but it is compatible with SV.
		 */
		if (del < 32		/* use fill characters if asked */
		    && stp->st_oflag & OFILL) {
			register char c;
			c = ((stp->st_oflag & OFDEL) ? 0177 : 0);
			*bp1->b_wptr++ = c;
			if (del > 3)
				*bp1->b_wptr++ = c;

		} else {
			bp1->b_datap->db_type = M_DELAY;
			*(int*)(bp1->b_wptr) = del;
			bp1->b_wptr += sizeof(int);
		}
	}

	if (!st_wsend(stp, wbp, cp+1)) { /* send up thru target */
		if (bp0) freeb(bp0);
		if (bp1) freeb(bp1);
		return 0;
	}

	if (bp0) putnext(stp->st_wq, bp0);
	if (bp1) putnext(stp->st_wq, bp1);
	if (sub) wbp->b_rptr++;
	return 1;
}



/* do CR/LF delays
 */
static int				/* 0=failed */
st_cr(struct stty_ld *stp,
      mblk_t	*wbp,			/* what we have done so far */
      u_char	*cp,
      char*	sub)			/* replace target with this string */
{
	int del;

	switch (stp->st_oflag & CRDLY) {
	case CR0:
		del = 0;
		break;
	case CR1:
		del = (stp->st_ncol >> 4) + 3;
		if (del < 6) del = 6;
		break;
	case CR2:
		del = 5;
		break;
	case CR3:
		del = 9;
		break;
	}

	stp->st_ncol = 0;
	return st_wdelay(stp,wbp,cp,del,sub);
}



/* (try to) send a message toward the output
 */
static mblk_t*				/* return unfinished part */
st_mout(struct stty_ld *stp,
	mblk_t *wbp)			/* write this message */
{
	u_char *cp, *lim;
	u_char *ctype;
	mblk_t *bp1;
	u_char c, type;
	int n;


	if (wbp->b_rptr == wbp->b_wptr	/* discard empty messages */
	    && !msgdsize(wbp)) {
		freemsg(wbp);
		return 0;
	}

	if (!(OPOST & stp->st_oflag)) {	/* quick in raw mode */
		if (!st_pass(stp->st_wq, wbp))
			return wbp;
		return NULL;
	}

	if (!(stp->st_lflag & XCASE)	/* in usual situation */
	    && !(stp->st_oflag & OLCUC))    /* upper case is ok */
		ctype = &st_types[0];
	else
		ctype = &st_types_c[0];

	stp->st_ncol = stp->st_ocol;
	do {
		cp = wbp->b_rptr;
		lim = wbp->b_wptr;
		while (cp < lim) {
			switch (type = ctype[c = *cp]) {

			case NP:
				break;

			case SC:
				stp->st_ncol++;
				break;

			case CT:
				break;

			case BS:
				if (stp->st_ncol) stp->st_ncol--;
				if ((stp->st_oflag & BSDLY)
				    && !st_wdelay(stp,wbp,cp,2,0))
					return wbp;
				break;

			case LF:
				if (stp->st_oflag & ONLRET) {
					if (!st_cr(stp,wbp,cp,0))
						return wbp;
				} else if (stp->st_oflag & ONLCR) {
					if ((stp->st_oflag & ONOCR)
					    && 0 == stp->st_ncol) {
						if (!st_cr(stp,wbp,cp,0))
							return wbp;
					} else {
						if (!st_cr(stp,wbp,cp,"\r\n"))
							return wbp;
					}
				} else if ((stp->st_oflag & NLDLY)
					   && !st_wdelay(stp,wbp,cp,5,0)) {
					return wbp;
				}
				break;

			case CR:
				if (stp->st_oflag & OCRNL) {
					if (!st_wdelay(stp,wbp,cp,
						       ((stp->st_oflag&NLDLY)
							? 5 : 0),
						       "\n"))
						return wbp;
				} else if ((stp->st_oflag & ONOCR) && 
					   0 == stp->st_ncol) {
		                	n = (lim  - cp)<<1;
                                	if (n > 64) n = 64;
                                	if (!(bp1 = st_wnew(stp,n,wbp,cp)))
                                        	return wbp;
                                	n = (bp1->b_datap->db_lim
                                     	- bp1->b_datap->db_base);
					cp++;
                                	do {
                                        	*bp1->b_wptr++ = *cp;
                                        	stp->st_ncol++;
                                	} while (++cp < lim
                                         	&& --n > 0);
                                	putnext(stp->st_wq,bp1);
                                	wbp->b_rptr = cp;
                                	cp--;
                                	stp->st_ocol = stp->st_ncol;
					break;
				} else {
					if (!st_cr(stp,wbp,cp,0))
						return wbp;
				}
				break;

			case HT:
				n = 8 - (stp->st_ncol & 7);
				switch (stp->st_oflag & TABDLY) {
				case TAB0:
					stp->st_ncol +=n;
					break;
				case TAB1:
					stp->st_ncol +=n;
					if (n >= 5
					    && !st_wdelay(stp,wbp,cp,n,0))
						return wbp;
					break;
				case TAB2:
					stp->st_ncol +=n;
					if (!st_wdelay(stp,wbp,cp,2,0))
						return wbp;
					break;
				case TAB3:
					if (!(bp1 = st_wnew(stp,n,wbp,cp)))
						return wbp;
					stp->st_ncol +=n;
					do {
						*bp1->b_wptr++ = ' ';
					} while (--n);
					putnext(stp->st_wq,bp1);
					wbp->b_rptr++;	/* do not send ^I */
					stp->st_ocol = stp->st_ncol;
					break;
				}
				break;

			case VT:
				if ((stp->st_oflag & VTDLY)
				    && !st_wdelay(stp,wbp,cp,0177,0))
					return wbp;
				break;

			case FF:
				if ((stp->st_oflag & FFDLY)
				    && !st_wdelay(stp,wbp,cp,0177,0))
					return wbp;
				break;

			case LC:	/* send burst of lower case */
				if (!(stp->st_oflag & OLCUC)) {
					stp->st_ncol++;
					break;
				}
				n = (lim  - cp)<<1;
				if (n > 64) n = 64;
				if (!(bp1 = st_wnew(stp,n,wbp,cp)))
					return wbp;
				n = (bp1->b_datap->db_lim
				     - bp1->b_datap->db_base);
				do {
					c -= ('a'-'A');
					*bp1->b_wptr++ = c;
					stp->st_ncol++;
				} while (++cp < lim
					 && --n > 0
					 && LC == (ctype[c = *cp]));
				putnext(stp->st_wq,bp1);
				wbp->b_rptr = cp;
				cp--;
				stp->st_ocol = stp->st_ncol;
				break;

			default:	/* send burst of upper case */
				if (!(stp->st_lflag & XCASE)) {
					stp->st_ncol++;
					break;
				}
				n = (lim  - cp)<<1;
				if (n > 64) n = 64;
				if (!(bp1 = st_wnew(stp,n,wbp,cp)))
					return wbp;
				n = (bp1->b_datap->db_lim
				     - bp1->b_datap->db_base);
				do {
					*bp1->b_wptr++ = CESC;
					*bp1->b_wptr++ = type-SH_DELTA;
					stp->st_ncol += 2;
				} while (++cp < lim
					 && (n -= 2) > 1
					 && SH_DELTA <= (type = ctype[*cp]));
				putnext(stp->st_wq,bp1);
				wbp->b_rptr = cp;
				cp--;
				stp->st_ocol = stp->st_ncol;
				break;
			}

			++cp;
			ASSERT(cp >= wbp->b_datap->db_base);
		}

		if (!st_wsend(stp,wbp,cp))	/* send block on */
			return wbp;	/* quit if we cannot */

		bp1 = rmvb(wbp,wbp);	/* trim block from input */
		freeb(wbp);
		wbp = bp1;
	} while (0 != wbp		/* quit at end of message */
		 && canenable(stp->st_wq));	/* or downstream congestion */
	return wbp;
}

/* Wake any sleeping close. */

void
st_wake_close_drain(struct stty_ld *stp)
{
	if (!(stp->st_state & ST_CLOSE_DRAIN))
		return;

	/* should never get flow-controlled when doing close_drain */
	ASSERT(canenable(stp->st_wq));

	wakeup(stp->st_wq);
}

/* work on the output queue
 *	This code is similar to the old System V line disciplines.  However,
 *	it is faster both in the worst case and on average.  For example,
 *	it never scans the output more than once.  You should judge for
 *	yourself whether it is shorter or simpler.  It is supposed to have
 *	the same behavior, when viewed from the terminal or the application
 *	program.
 */
void
st_write_common(register queue_t *wq,
		register struct stty_ld *stp,
		register mblk_t *bp)
{
	register mblk_t *wbp;
	register struct iocblk *iocp;
	int inline;

#define FIX_BCOL() \
	if ((stp->st_state & (ST_BCOL_DELAY|ST_BCOL_BAD)) == ST_BCOL_BAD) { \
		stp->st_bcol = stp->st_ocol; \
		stp->st_state &= ~ST_BCOL_BAD; \
	}
#define ENQUEUE(q,bp) \
	if (inline) \
		putq(q,bp); \
	else \
		putbq(q,bp);

	if (!bp) {
		enableok(wq);
		inline = 0;
	} else {
		ASSERT(bp->b_datap->db_type == M_DATA);
		inline = 1;
	}
	for (;;) {
		if (inline) {
			wbp = bp;
			bp = NULL;
		} else {
			wbp = getq(wq);	/* get another message */
		}
		if (!wbp)
			break;

		switch (wbp->b_datap->db_type) {
		case M_DATA:
			if (!canenable(wq)) {	/* quit if blocked */
				/* wake sleeping close */
				st_wake_close_drain(stp);
				ENQUEUE(wq, wbp);
				return;
			}
			if (stp->st_emsg != 0) { /* do echos first */
				FIX_BCOL();
				if ((stp->st_emsg = st_mout(stp,stp->st_emsg))
				    != 0) {
					ENQUEUE(wq,wbp);
					/* wake sleeping close */
					st_wake_close_drain(stp);
					return;	/* quit if can't send */
				}
			}
			if (!canenable(wq) || (wbp = st_mout(stp,wbp)) != 0) {
				ENQUEUE(wq, wbp);
				/* wake sleeping close */
				st_wake_close_drain(stp);
				return;
			}
			stp->st_xcol = stp->st_ocol;
			break;

		case M_IOCTL:
			iocp = (struct iocblk*)wbp->b_rptr;
			if (iocp->ioc_cmd == TIOCSTI) {
				ASSERT(iocp->ioc_count == 1);
				putq(RD(wq), unlinkb(wbp));
				iocp->ioc_count = 0;
				wbp->b_datap->db_type = M_IOCACK;
				qreply(wq, wbp);
			} else {
				putnext(wq, wbp);
			}
			break;

		case M_FLUSH:
			st_wflush(stp,*wbp->b_rptr,wq);
			putnext(wq, wbp);
			break;

		default:
			putnext(wq, wbp);
			break;
		}
	}

	if (wbp = stp->st_emsg) {	/* do echos */
		FIX_BCOL();
		if (0 != (stp->st_emsg = st_mout(stp,wbp))) {
			st_wake_close_drain(stp);   /* wake sleeping close */
			return;		/* quit if not finished */
		}
	}
	if (stp->st_state & ST_BCOL_DELAY)
		stp->st_state &= ~ST_BCOL_DELAY;

	if (stp->st_state & ST_TABWAIT) {
		stp->st_state &= ~ST_TABWAIT;
		qenable(stp->st_rq);
	}
	if (stp->st_xcol > stp->st_ocol)
		stp->st_xcol = stp->st_ocol;
	if (stp->st_bcol > stp->st_ocol)
		stp->st_bcol = stp->st_ocol;

	st_wake_close_drain(stp);	/* wake sleeping close */
}

void
st_wsrv(register queue_t *wq)			/* our write queue */
{
	register struct stty_ld *stp = (struct stty_ld*)wq->q_ptr;

	ASSERT(stp->st_wq == wq);
	st_write_common(wq, stp, NULL);
}


static int st_bs(struct stty_ld *);

/* make room in the current, canonical line buffer
 *	come here when we run out of room in the buffer.
 */
static int				/* 0=failed */
st_iroom(struct stty_ld *stp)
{
	if (LDISC0 == stp->st_line) {	/* for old stuff, flush the line */
		mblk_t *ibp = stp->st_imsg;
		ASSERT(ibp != 0 && ibp == stp->st_ibp);
		ASSERT(ibp->b_rptr >= ibp->b_datap->db_base);
		ibp->b_wptr = ibp->b_rptr;
		freemsg(stp->st_lmsg);
		stp->st_lmsg = 0;
		stp->st_llen = 0;
		return 1;
	}

	/* kill last character */
	return st_bs(stp);
}



/* send typed-ahead ICANON lines to the stream head
 */
static void
st_sendold(struct stty_ld *stp)
{
	mblk_t *bp;
	queue_t *rq = stp->st_rq;

	while (0 != (bp = stp->st_lmsg)) {
		if (!canput(rq->q_next)) {
			return;
		}
		stp->st_lmsg = bp->b_cont;
		stp->st_llen -= (bp->b_wptr - bp->b_rptr);
		bp->b_cont = 0;
		putnext(rq,bp);
	}
}



/* send whatever input we have up the read queue, and get another buffer.
 */
static mblk_t*				/* 0=failed to allocate buffer */
st_rsend(struct stty_ld *stp,
	 int alc)			/* 0=do not try to allocate */
{
	mblk_t *imp;

	ASSERT(stp->st_lflag & ICANON);

	if (0 != stp->st_imsg) {	/* finish any buffer we have */
		stp->st_llen += msgdsize(stp->st_imsg);
		str_conmsg(&stp->st_lmsg,
			   &stp->st_lbp,
			   stp->st_imsg);
	}

	if (stp->st_lmsg		/* use typed-ahead line for new buf */
	    && (stp->st_lbp->b_wptr + MAX_CANON
		<= stp->st_lbp->b_datap->db_lim)
	    && 0 != (imp = dupb(stp->st_lbp))) {
		imp->b_rptr = imp->b_wptr;

	} else if (stp->st_llen >= MAX_LLEN) {
		imp = 0;

	} else if (alc) {		/* or create a new buffer */
		imp = str_allocb(stp->st_llen+MAX_CANON,
				 stp->st_rq, BPRI_HI);
	} else {
		imp = 0;
	}

	stp->st_state |= ST_BCOL_BAD;
	if (stp->st_emsg)
		stp->st_state |= ST_BCOL_DELAY;
	stp->st_imsg = imp;
	stp->st_ibp = imp;
	return imp;
}



/* kill/forget non-canonical timer
 */
static void
st_untime(register struct stty_ld *stp)
{
	if (ST_TIMING & stp->st_state)
		untimeout(stp->st_tid);

	stp->st_state &= ~(ST_TIMING|ST_TIMED);
}


/* do something when non-canonical timer expires
 */
static void
st_tock(stp)
register struct stty_ld *stp;
{
	stp->st_state &= ~ST_TIMING;
	stp->st_state |= ST_TIMED;
	qenable(stp->st_rq);
}



/* stuff a non-canonical input byte into a buffer
 *	u_char c;		stuff this byte
 *	register mblk_t *rmp;	from this buffer
 */
static int				/* 0=failed */
st_nonstuff(register struct stty_ld *stp, u_char c,
	    register mblk_t *rmp)
{
	register mblk_t *fbp, *ibp;

	fbp = stp->st_imsg;
	if (0 != fbp) {			/* if already have a buffer */
		ibp = stp->st_ibp;
		if (ibp->b_wptr		/* & have room, then go fast */
		    < ibp->b_datap->db_lim) {
			if (c == *ibp->b_wptr) {
				++ibp->b_wptr;	/* our buffer */
				stp->st_ahead++;
				return 1;	/* already has right char */
			}

			if (1 == ibp->b_datap->db_ref) {
				*ibp->b_wptr++ = c;	/* if buf private */
				stp->st_ahead++;
				return 1;	/* then we can change it */
			}
		}
	}


	if (c == *rmp->b_rptr		/* if copying char, */
	    && 0 != (ibp = dupb(rmp))) {	/* clone input */
		ASSERT(ibp->b_rptr >= ibp->b_datap->db_base);
		ibp->b_wptr = ibp->b_rptr+1;

	} else {			/* else, make a new buffer */
		ibp = str_allocb(rmp->b_wptr - rmp->b_rptr,
				 stp->st_rq,BPRI_LO);
		if (!ibp)
			return 0;
		*ibp->b_wptr++ = c;
	}
	stp->st_ahead++;

	if (!stp->st_imsg) stp->st_imsg = ibp;
	else stp->st_ibp->b_cont = ibp;
	stp->st_ibp = ibp;
	return 1;
}



/* stuff an echo into a suitable buffer
 *	On successful exit, there is guaranteed to be a current echo message.
 *	rmp == 0 or read queue msg
 */
static int				/* 0=failed */
st_estuff(register struct stty_ld *stp, register u_char c, mblk_t *rmp)
{
	register mblk_t *ebp;

	stp->st_lflag &= ~FLUSHO;

	if (0 != stp->st_emsg) {	/* if already have a buffer */
		ebp = stp->st_ebp;	/* & have room */
		if (ebp->b_wptr < ebp->b_datap->db_lim) {
			if (c == *ebp->b_wptr) {	/* go fast */
				++ebp->b_wptr;
				return 1;
			}

			if (1 == ebp->b_datap->db_ref) {/* if new buffer */
				*ebp->b_wptr++ = c;	/* ok to change it */
				return 1;
			}
		}
		if (!canenable(stp->st_wq)) {	/* quit if downstream plugged*/
			stp->st_state |= ST_TABWAIT;
			return 0;		/* and do not have room */
		}
	}

	if (rmp
	    && c == *rmp->b_rptr	/* if copying char, */
	    && rmp->b_rptr < rmp->b_datap->db_lim) {	/* clone input */
		ASSERT(rmp->b_rptr >= rmp->b_datap->db_base);
		ebp = dupb(rmp);
		if (!ebp) {
			BCALL_HI(0, stp->st_rq);
			return 0;
		}
		ebp->b_wptr = ebp->b_rptr+1;
		ASSERT(ebp->b_wptr <= ebp->b_datap->db_lim);

	} else {			/* else, make a new buffer */
		ebp = str_allocb(rmp ? (rmp->b_wptr - rmp->b_rptr) : 1,
				 stp->st_rq,BPRI_LO);
		if (!ebp)
			return 0;
		*ebp->b_wptr++ = c;
	}

	if (!stp->st_emsg) {
		stp->st_emsg = ebp;
		if (canenable(stp->st_wq))
			qenable(stp->st_wq);
	} else {
		stp->st_ebp->b_cont = ebp;
	}
	stp->st_ebp = ebp;
	return 1;
}

/* see that there is room for some echos
 * n == want this much room
 */
static mblk_t*				/* 0=failed */
st_echeck(register struct stty_ld *stp, register int n)
{
	register mblk_t *ebp;

	ebp = stp->st_ebp;
	if (!stp->st_emsg
	    || 1 != ebp->b_datap->db_ref
	    || ebp->b_wptr >= (ebp->b_datap->db_lim - n)) {
		ASSERT(n <= STR_MAXBSIZE);
		ebp = str_allocb(n, stp->st_rq,BPRI_LO);
		if (ebp) {		/* fail if cannot get another block */
			if (!stp->st_emsg) {
				stp->st_emsg = ebp;
				if (canenable(stp->st_wq))
					qenable(stp->st_wq);
			} else {
				stp->st_ebp->b_cont = ebp;
			}
			stp->st_ebp = ebp;
		}
	}

	return ebp;
}

/*
 * echo a byte
 *	Check for control characters
 *	register u_char c	echo this
 *	mblk_t *rmp;		0 or read queue msg
 */
static int				/* 0=failed */
st_echo(register struct stty_ld *stp, register u_char c, mblk_t *rmp)
{
	if (stp->st_state & ST_MAXBEL) {
		stp->st_state &= ~ST_MAXBEL;
		return st_estuff(stp,c,rmp);
	}
	if (stp->st_line != LDISC0) {
		switch (st_types[c]) {
		case CT:		/* convert control characters */
			/* XXXrs - ECHOCTL should require IEXTEN */
			if (!(stp->st_lflag & ECHOCTL))
				return 1;
			/* else, fall through... */
		case BS:
		case LF:
		case CR:
		case VT:
		case FF:
			if (!st_echeck(stp,2))
				return 0;
			(void)st_estuff(stp, '^',rmp);
			c ^= '@';
			break;
		}
	}

	return st_estuff(stp,c,rmp);
}



/* send a string of 'echos'
 *	n == length of the string
 */
static int				/* 0=failed */
st_estr(register struct stty_ld *stp, register int n, register char *str)
{
	register mblk_t *ebp;

	if (!(ebp = st_echeck(stp,n)))
		return 0;

	stp->st_lflag &= ~FLUSHO;
	while (--n >= 0)
		*ebp->b_wptr++ = *str++;
	ASSERT(ebp->b_wptr <= ebp->b_datap->db_lim);
	return 1;
}



/* delete the last character input
 */
static int				/* 0=failed */
st_bs(register struct stty_ld *stp)
{
	static char bs[] = "\b \b\b \b\b \b\b \b\b \b\b \b\b \b\b \b";
	static char bs1[] = "\b\b\b\b\b\b\b\b";
	register mblk_t *ibp = stp->st_imsg;
	register short new;
	register u_char *cp, *lim;
	register u_char *ctype;

	ASSERT(0 != ibp && ibp == stp->st_ibp);

	lim = ibp->b_wptr-1;
	if (stp->st_line == LDISC0) {
		/* the standard SV line discipline always echos backspaces,
		 *	even if there is nothing to do.
		 */
		switch (stp->st_lflag & (ECHO|ECHOE)) {
		case (ECHO|ECHOE):
			    if (!st_estr(stp,3, &bs[0]))
				    return 0;
			    break;
		case (ECHO):
			    if (!st_estr(stp,1, &bs[0]))
				    return 0;
			    break;
		case (ECHOE):
			    if (!st_estr(stp,2, &bs[1]))
				    return 0;
			    break;
		}

		if (lim >= ibp->b_rptr)	/* backup if possible */
			ibp->b_wptr = lim;
		return 1;
	}

	if (lim < ibp->b_rptr)		/* succeed if nothing to do */
		return 1;

	if (stp->st_lflag & ECHO) {
		if (stp->st_wq->q_first	/* wait until is output quiet */
		    || stp->st_emsg) {
			stp->st_state |= ST_TABWAIT;
			noenable(stp->st_rq);
			return 0;
		}

		new = stp->st_ocol;
		ctype = ((stp->st_lflag & XCASE)
			 ? &st_types_c[0]
			 : &st_types[0]);
		switch (ctype[*lim]) {
		case NP:
			break;
		case SC:
		case LC:
			new--;
			break;
		case CT:
		case BS:
		case LF:
		case CR:
		case VT:
		case FF:
			/* XXXrs - ECHOCTL should require IEXTEN */
			if (stp->st_lflag & ECHOCTL)
				new -= 2;
			break;

		case HT:
			new = stp->st_bcol;
			for (cp = ibp->b_rptr; cp < lim; cp++) {
				switch (ctype[*cp]) {
				case NP:
					break;
				case SC:
				case LC:
					new++;
					break;
				case CT:
				case BS:
				case LF:
				case CR:
				case VT:
				case FF:
					/*
					 * XXXrs - ECHOCTL should require
					 *         IEXTEN
					 */
					if (stp->st_lflag & ECHOCTL)
						new += 2;
					break;
				case HT:
					new += 8;
					new &= ~7;
					break;
				default:
					new += 2;
					break;
				}
			}
			break;

		default:
			new -= 2;
			break;
		}


		/* Work only when necessary, when we know we are moving
 		 * left or when OPOST is off and we think we are probably
 		 * moving left but cannot tell because we are not allowed
 		 * to understand the output.
 		 */
		if (new < stp->st_ocol
    		    || (new != stp->st_ocol
                       	&& !(OPOST & stp->st_oflag))) {
			/* XXXrs - ECHOPRT should require IEXTEN */
			if (stp->st_lflag & ECHOPRT) {
				if (!st_echo(stp, *lim, (mblk_t*)0))
					return 0;
			} else if (new < stp->st_xcol) {
				if (!st_retype(stp,1,stp->st_cc[VRPRNT],1))
					return 0;
			} else if (!(stp->st_lflag & ECHOE)) {
				new = (short)(stp->st_ocol - new);
				if (new >= sizeof(bs1))
					new = sizeof(bs1);
				if (!st_estr(stp, new, bs1))
					return 0;
			} else {
				new = 3*(short)(stp->st_ocol - new);
				if (new >= sizeof(bs))
					new = sizeof(bs);
				if (!st_estr(stp, new, bs))
					return 0;
			}
		}
	}

	ASSERT(lim >= ibp->b_datap->db_base);
	ASSERT(lim <= ibp->b_datap->db_lim);
	ibp->b_wptr = lim;
	return 1;
}



/* empty the line buffer
 */
static int				/* 0=failed */
st_del_line(struct stty_ld *stp,
	    u_char c)			/* character to echo */
{
	register mblk_t *ibp = stp->st_imsg;

	ASSERT(0 != ibp && ibp == stp->st_ibp);
	/* XXXrs - ECHOKE should require IEXTEN */
	if (stp->st_lflag & ECHOK || stp->st_lflag & ECHOKE) {
		if (stp->st_line == LDISC0 || !(stp->st_lflag & ECHOKE)) {
			register mblk_t *ebp;

			if (!(ebp = st_echeck(stp,2)))
				return 0;
			if (stp->st_lflag & ECHO)
				*ebp->b_wptr++ = c;
			*ebp->b_wptr++ = '\n';

		} else while (ibp->b_rptr < ibp->b_wptr) {
			if (!st_bs(stp))
				return 0;
		}
	}

	ibp->b_wptr = ibp->b_rptr;
	ASSERT(ibp->b_wptr <= ibp->b_datap->db_lim);
	return 1;
}



/* delete the previous word
 */
static int				/* 0=failed */
st_del_word(struct stty_ld *stp)
{
	register u_char c;
	register mblk_t *ibp = stp->st_imsg;

	ASSERT(ibp && ibp == stp->st_ibp);

	while (ibp->b_rptr < ibp->b_wptr) {
		c = *(ibp->b_wptr - 1);
		if (' ' != c && '\t' != c)
			break;
		if (!st_bs(stp))
			return 0;
	}

	while (ibp->b_rptr < ibp->b_wptr) {
		c = *(ibp->b_wptr - 1);
		if (' ' == c || '\t' == c)
			break;
		if (!st_bs(stp))
			return 0;
	}

	ASSERT(ibp->b_wptr <= ibp->b_datap->db_lim);
	return 1;
}



/* retype what we have so far
 */
static int				/* 0=failed */
st_retype(struct stty_ld *stp,
	  int delta,			/* exclude these bytes from echo */
	  u_char chr,			/* echo this */
	  int donl)			/* prepend new-line? */
{
	register mblk_t *bp;
	register u_char *cp;
	register int sz;

	if (stp->st_wq->q_first		/* when things are quiet, */
	    || !st_echeck(stp,MAX_RETYPE)) {	/* get room to send line(s) */
		stp->st_state |= ST_TABWAIT;
		noenable(stp->st_rq);
		return 0;
	}

	if (chr)
		(void)st_echo(stp, chr, (mblk_t*)0);
	if (donl)
		(void)st_estuff(stp, '\n', (mblk_t*)0);
	stp->st_bcol = 0;
	stp->st_xcol = 0;


	if (!(stp->st_lflag & ECHO))	/* be quiet for su */
		return 1;

	for (bp = stp->st_lmsg; bp; bp = bp->b_cont) {	/* echo other lines */
		cp = bp->b_rptr;
		sz = bp->b_wptr - cp;
		if (sz <= 0) {		/* 0-length is EOF */
			(void)st_estr(stp,2,ECHO_EOF);
		} else {
			while (--sz)
				(void)st_echo(stp,*cp++,bp);
			if (*cp == '\n')
				(void)st_estuff(stp,'\n',bp);
			else
				(void)st_echo(stp,*cp,bp);
		}
	}

	bp = stp->st_imsg;
	ASSERT(bp && bp == stp->st_ibp);

	cp = bp->b_rptr;		/* now echo current line */
	sz = (bp->b_wptr-cp) - delta;	/* decide how many bytes */
	if (sz < 0) sz = 0;		/* must be echoed */

	while (sz--)
		(void)st_echo(stp,*cp++,bp);

	return 1;
}


/* accept a new input message
 */
int
st_rput(queue_t *rq, mblk_t *bp)
{
	register struct stty_ld *stp;
	register struct iocblk *iocp;
	register mblk_t *imp;

	stp = (struct stty_ld*)rq->q_ptr;
	ASSERT(stp->st_rq == rq);

	switch (bp->b_datap->db_type) {
	case M_BREAK:

		/*
		 * We look at the apparent modes here instead of the
		 * effective modes. Effective modes cannot be used if
		 * IGNBRK, BRINT and PARMRK have been negotiated to be
		 * handled by the driver. Since M_BREAK should be sent
		 * upstream only if break processing was not already done,
		 * it should be ok to use the apparent modes.
		 */

		if (!(stp->st_iflag & IGNBRK )) {
			if (stp->st_iflag & BRKINT) {
				/*
				 * Flush read or write side
				 * Restart the input or output
				 */
				flushq(rq, FLUSHDATA);
				(void) putctl1(WR(rq)->q_next, M_FLUSH, FLUSHR);				/* always send a M_START1 donwstream */
				(void) putctl(WR(rq)->q_next, M_STARTI);
				flushq(WR(rq), FLUSHDATA);
				(void) putctl1(rq->q_next, M_FLUSH, FLUSHW);
				(void) putctl(WR(rq)->q_next, M_START);
				(void) putctl1(rq->q_next, M_PCSIG, SIGINT);

				freemsg (bp);
				return(0);
			} else if (stp->st_iflag & PARMRK) {
				/*
				 *  Send '\377','\0', '\0'.
				 */
				bp->b_datap->db_type = M_DATA;
				*bp->b_wptr++ = (unsigned char) '\377';
				*bp->b_wptr++ = '\0';
				*bp->b_wptr++ = '\0';
				putnext(rq, bp);
				return(0);
			} else {
				/*
				 * Act as if a '\0' came in.
				 */
				bp->b_datap->db_type = M_DATA;
				*bp->b_wptr++ = '\0';
				putnext(rq, bp);
				return(0);
			}

		} else {
			freemsg(bp);
		}
		break;
	case M_IOCACK:			/* let service fnc change state	*/
	case M_FLUSH:
	case M_HANGUP:
		enableok(rq);
		/* XXXrs - if we get popped before the service
		 *         procedure runs, this message could be lost
		 */
		putq(rq,bp);
		break;

	case M_DATA:
		/*
		 * If somebody below us ("intelligent" communications board,
		 * pseudo-tty controlled by an editor) is doing
		 * canonicalization, don't scan it for special characters.
		 */
		if (stp->st_state & ST_NOCANON) {
			putq(rq, bp);
			break;
		}

		if (0 != stp->st_imsg
		    || !(stp->st_state & ST_INPASS)
		    || 0 != stp->st_rrunning	/* if caught up, */
		    || 0 != rq->q_first
		    || !st_pass(rq,bp)) {	/* try to pass it on */
			putq(rq,bp);	/* else, save if for later */
		}
		break;

	case M_CTL:
		/* The M_CTL has been standardized to look like
		 * an M_IOCTL message.
		 */

		if ((bp->b_wptr - bp->b_rptr) != sizeof (struct iocblk)) {
			/* May be for someone else; pass it on */
			putnext (rq, bp);
			break;
		}
		iocp = (struct iocblk *)bp->b_rptr;

		switch (iocp->ioc_cmd) {
		case MC_NO_CANON:
			stp->st_state |= ST_NOCANON;
			/* Clear out partial messages */
			imp = stp->st_imsg;
			if (imp && imp->b_rptr >= imp->b_wptr) {
			    freemsg(imp);
			    stp->st_imsg = 0;
			}
			if (stp->st_lmsg) {
			    str_conmsg(&stp->st_imsg,
				       &stp->st_ibp,
				       stp->st_lmsg);
			    stp->st_lmsg = 0;
			}
			stp->st_llen = 0;
			stp->st_ahead = msgdsize(stp->st_imsg);
			stp->st_state &= ~(ST_LIT|ST_ESC);
			stp->st_lflag &= ~FLUSHO;
			break;

		case MC_DO_CANON:
			stp->st_state &= ~ST_NOCANON;
			break;
		default:
			break;
		}
		putnext(rq, bp);	/* In case anyone else has to see it */
		break;
		
	default:
		putnext(rq,bp);
		break;
	}
	return(0);
}

int
st_start_echoprt(struct stty_ld *stp)
{
	/* XXXrs - ECHOPRT should require IEXTEN */
	if (stp->st_state & ST_ECHOPRT || !(stp->st_lflag & ECHOPRT))
		return 1;
	stp->st_state |= ST_ECHOPRT;
	return (st_echo(stp, '\\', (mblk_t *)0));
}

int
st_stop_echoprt(struct stty_ld *stp)
{
	stp->st_state &= ~ST_ECHOPRT;
	return (st_echo(stp, '/', (mblk_t *)0));
}

/* process character input
 *	This function tries to emulate the system 5 tty line discipline,
 *	including its quirks and bugs.  Think carefully before you fix it.
 */
static mblk_t*				/* NULL or unused data */
st_cin(struct stty_ld *stp,
       register mblk_t *rmp)
{
	u_char c;
	int echo;
	uint lflag = stp->st_lflag;
	uint iflag = stp->st_iflag;
	u_char *cp;
	mblk_t *ibp;
	long lim;

#define NEWLIM() (MAX_CANON - (ibp->b_wptr - ibp->b_rptr))

#define CISTUFF(c) { \
	if (echo != BEL || c == BEL) { \
		*ibp->b_wptr++ = (c); \
		lim--; \
		ASSERT(ibp->b_wptr <= ibp->b_datap->db_lim); \
	} \
}

#define CKSTUFF(min) { \
	if (lim == (min) && stp->st_iflag & IMAXBEL) { \
		stp->st_state |= ST_MAXBEL; \
		echo = BEL; \
	} else { \
		while (lim < (min)) { \
			if (!st_iroom(stp)) \
				return rmp; \
			lim = NEWLIM(); \
		} \
	} \
}

/* send what we have and try to get a new input buffer */
#define CSEND() {if (0 == (ibp=st_rsend(stp,1))) return rmp; lim = NEWLIM();}

#define DO_ECHO() {if (echo != -1 && !st_echo(stp,(char)echo,rmp)) return rmp;}


	ibp = stp->st_imsg;
	if (lflag & ICANON) {		/* create line buffer */
		if (0 == ibp
		    || ibp != stp->st_ibp
		    || (lim = NEWLIM()) < 0)
			CSEND();
	} else {
		if (stp->st_ahead >= MAX_AHEAD
		    && !canput(stp->st_rq->q_next)) {
			return rmp;
		}
		st_untime(stp);		/* reset canonical timer */
		lim = STR_MAXBSIZE;
	}


	for (cp = rmp->b_rptr; ; rmp->b_rptr = cp) {

		while (cp >= rmp->b_wptr) {	/* next block at block end */
			register mblk_t *nbp = rmp->b_cont;
			freeb(rmp);
			rmp = nbp;
			if (!nbp)
				return 0;	/* quit at end of message */
			cp = rmp->b_rptr;
		}

		SYSINFO.canch++;
		c = *cp++;		/* get the next character */
		echo = ((lflag & ECHO) ? c : -1);

		/* after literal character, for fancy line discipline,
		 *	just take the character
		 */
		if ((stp->st_state & ST_LIT)
		    && LDISC0 != stp->st_line) {
			CKSTUFF(1);
			DO_ECHO();
			CISTUFF(c);
			stp->st_state &= ~(ST_ESC|ST_LIT);
			continue;
		}

		if (stp->st_state & ST_ECHOPRT &&
		    c != stp->st_cc[VERASE] &&
		    (c != stp->st_cc[VWERASE] || !(stp->st_lflag & IEXTEN)) &&
		    c != stp->st_cc[VKILL] && !st_stop_echoprt(stp))
			return rmp;

		if (c == '\n' && (iflag & INLCR))
			c = '\r';
		else if (c == '\r') {
			stp->st_lflag &= ~FLUSHO;
			if (iflag & IGNCR)
				continue;
			if (iflag & ICRNL) {
				c = '\n';
				if (lflag & ECHO)
					echo = c;
			}
		} else if ((iflag & IUCLC)
			   && 'A' <= c && c <= 'Z') {
			c += ('a'-'A');
		}

		if ((lflag & ISIG)
		    && (c == stp->st_cc[VINTR]	    /* choose SIGINT in case */
		        || c == stp->st_cc[VQUIT]   /* of ambiguity */
		        || ((c == stp->st_cc[VSUSP]
		             || (c == stp->st_cc[VDSUSP] &&
				 (stp->st_lflag & IEXTEN)))
			    && c != CNSWTCH
			    && stp->st_line != LDISC0))
		    && !CHAR_IS_DISABLED(c)) {
			stp->st_state &= ~(ST_ESC|ST_LIT);
			stp->st_lflag &= ~FLUSHO;
			if (0 == (lflag & NOFLSH)) {
				if (c != stp->st_cc[VDSUSP] ||
				    !(stp->st_lflag & IEXTEN)) {
					st_rflush(stp,FLUSHR,stp->st_rq);
					st_wflush(stp,FLUSHW,stp->st_wq);
					if (!putctl1(stp->st_rq,
						     M_FLUSH, FLUSHW)
				    	    || !putctl1(stp->st_wq,
							M_FLUSH,FLUSHR)) {
						BCALL_HI(1,stp->st_rq);
						return rmp;
					}
				}
			} else if (0 != (lflag & ICANON)
				   && ibp->b_wptr != ibp->b_rptr) {
				if (!posix_tty_default)
					CSEND();
			}
			if (!putctl1(stp->st_rq->q_next,
				     (c == stp->st_cc[VDSUSP] &&
				      (stp->st_lflag & IEXTEN)) ? M_SIG :
								 M_PCSIG,
				     (c == stp->st_cc[VINTR]) ? SIGINT :
				     ((c == stp->st_cc[VQUIT]) ? SIGQUIT :
				      SIGTSTP))) {
				BCALL_HI(1,stp->st_rq);
				return rmp;
			}
			if (!(lflag & NOFLSH)) {
				freemsg(rmp);
				return 0;
			}
			continue;

		}

		if (lflag & ICANON) {
			/* XXXrs - PENDIN should require IEXTEN */
			if (lflag & PENDIN) {
				if (!st_retype(stp,0,0,0))
				    return rmp;
				stp->st_lflag &= ~PENDIN;
				lflag &= ~PENDIN;
			}

			if (c == '\n'
			    || ((c == stp->st_cc[VEOL]
			         || (c == stp->st_cc[VEOL2] &&
				     (stp->st_lflag & IEXTEN)))
			        && !CHAR_IS_DISABLED(c))) {
				if (stp->st_state & ST_LIT) {
					CKSTUFF(2);
					CISTUFF(CESC);
				} else {
					CKSTUFF(1);
				}

				/*
				 * CKSTUFF sets echo to BEL if buffer full
				 * and IMAXBEL set.  Don't want to echo BEL
				 * if doing newline.  Newline/EOL allowed as
				 * last character in buffer.
				 */
				echo = ((lflag & ECHO) ? c : -1);

				stp->st_state &= ~(ST_ESC|ST_LIT|ST_MAXBEL);
				stp->st_lflag &= ~FLUSHO;
				if (echo != -1
				    || ((lflag & ECHONL) && c == '\n')) {
					if (c == '\n') {
						if (!st_estuff(stp,c,rmp))
							return rmp;
					} else {
						if (!st_echo(stp,c,rmp))
							return rmp;
					}
				}
				CISTUFF(c);
				rmp->b_rptr = cp;
/* advance the input now so that we can know if it is worthwhile to
 *	allocate another line buffer.  It is a waste on the typical system
 *	with many quiesent windows and remote sessions to have idle
 *	line buffers for each.
 */
				while (cp >= rmp->b_wptr) { /* next block */
					register mblk_t *nbp = rmp->b_cont;
					freeb(rmp);
					rmp = nbp;
					if (!nbp) {
						(void)st_rsend(stp,0);
						return 0;
					}
					cp = rmp->b_rptr;
				}
				CSEND();
				continue;
			}

			if (!(stp->st_state & (ST_ESC|ST_LIT))) {
				if (c == CESC && (lflag & XCASE)) {
					DO_ECHO();
					stp->st_state |= ST_ESC;
					stp->st_lflag &= ~FLUSHO;
					continue;
				}

				if (c == stp->st_cc[VLNEXT] &&
				    (stp->st_lflag & IEXTEN)
			            && !CHAR_IS_DISABLED(c)) {
					if (LDISC0 == stp->st_line) {
						DO_ECHO();
					} else {
						if (echo != -1
						    && !st_estr(stp,2,"^\b"))
							return rmp;
					}
					stp->st_state |= ST_LIT;
					stp->st_lflag &= ~FLUSHO;
					continue;
				}

				if (c == stp->st_cc[VERASE]
			            && !CHAR_IS_DISABLED(c)) {
					stp->st_lflag &= ~FLUSHO;
					if (!st_start_echoprt(stp) ||
					    !st_bs(stp))
						return rmp;
					continue;
				}

				if (c == stp->st_cc[VKILL]
			            && !CHAR_IS_DISABLED(c)) {
					stp->st_lflag &= ~FLUSHO;
					if (!st_start_echoprt(stp) ||
					    !st_del_line(stp,c))
						return rmp;
					continue;
				}

				if (c == stp->st_cc[VEOF]
			            && !CHAR_IS_DISABLED(c)) {
					CSEND();
					stp->st_lflag &= ~FLUSHO;
					if (LDISC0 != stp->st_line) {
						(void)st_estr(stp,4,ECHO_EOF);
					}
					continue;
				}

				if (LDISC0 != stp->st_line) {
				    if (c == stp->st_cc[VWERASE] &&
					(stp->st_lflag & IEXTEN)
			                && !CHAR_IS_DISABLED(c)) {
					stp->st_lflag &= ~FLUSHO;
					if (!st_start_echoprt(stp) ||
					    !st_del_word(stp))
					    return rmp;
					continue;
				    }

				    /* XXXrs - VRPRNT aka VREPRINT */
				    if (c == stp->st_cc[VRPRNT] &&
					(stp->st_lflag & IEXTEN)
			                && !CHAR_IS_DISABLED(c)) {
					stp->st_lflag &= ~FLUSHO;
					if (!st_retype(stp,0,0,1))
					    return rmp;
					continue;
				    }

				    /* XXXrs - VFLUSHO aka VDISCARD */
				    if (c == stp->st_cc[VFLUSHO] &&
					(stp->st_lflag & IEXTEN)
			                && !CHAR_IS_DISABLED(c)) {
					if (!(stp->st_lflag & FLUSHO)) {
					    st_wflush(stp,FLUSHW, stp->st_wq);
					    if (!putctl1(stp->st_wq->q_next,
							 M_FLUSH, FLUSHW)) {
						BCALL_HI(1,stp->st_rq);
						return rmp;
					    }
					    if (msgdsize(stp->st_imsg) != 0
						|| stp->st_lmsg != 0) {
						if (!st_retype(stp,0,0,1))
						    return rmp;
					    } else {
						if (!st_echo(stp,c,rmp))
						    return rmp;
					    }
					    stp->st_lflag |= FLUSHO;
					} else {
					    stp->st_lflag &= ~FLUSHO;
					}
					continue;
				    }
				}
				stp->st_lflag &= ~FLUSHO;
				CKSTUFF(1);
				DO_ECHO();

			} else {	/* previous was 'literal' character */
				register u_char bslash = CESC;

				CKSTUFF(2);
				if (c != stp->st_cc[VEOF])
					DO_ECHO();	/* do not echo ^D */

				if ((c == stp->st_cc[VERASE]
				     || c == stp->st_cc[VKILL]
				     || c == stp->st_cc[VEOF])
			                && !CHAR_IS_DISABLED(c)) {
					bslash = 0;

				} else if ((lflag & XCASE)
					   && (stp->st_state & ST_ESC)) {
					bslash = 0;
					switch (c) {
					case CESC: break;
					case '\'': c = '`'; break;
					case '(': c = '{'; break;
					case '!': c = '|'; break;
					case ')': c = '}'; break;
					case '^': c = '~'; break;
					default:
						if ('a' <= c && c <= 'z')
							c +=('A'-'a');
					}
				}

				stp->st_state &= ~(ST_ESC|ST_LIT|ST_MAXBEL);

				if (LDISC0 == stp->st_line
				    && bslash == CESC) {
					CISTUFF(bslash);
					if (CESC == c)
						stp->st_state |= ST_LIT;
				}
			}
			CISTUFF(c);

		} else {		/* non-canonical input */
			if (echo != -1
			    && !st_estuff(stp,(char)echo,rmp))
				return rmp;
			if (!st_nonstuff(stp,c,rmp)) {	/* pass it on */
				if (echo != -1) /* unecho if failed */
					--stp->st_ebp->b_wptr;
				return rmp;
			}
		}
	}
#undef NEWLIM
#undef CISTUFF
#undef CKSTUFF
#undef CSEND
#undef DO_ECHO
}


/* work on the input queue
 */
int
st_rsrv(queue_t *rq)			/* our read queue */
{
	register struct stty_ld *stp;
	register mblk_t *rmp;
	register struct iocblk *iocp;
	extern int posix_tty_default;
	extern int tty_auto_strhold;

	stp = (struct stty_ld*)rq->q_ptr;
	ASSERT(stp->st_rq == rq);

	enableok(rq);
	st_sendold(stp);		/* send previous canonical lines */
	for (;;) {
		if (!(stp->st_state & ST_ISTTY)) {	/* tell head */
			register mblk_t *bp;
			register struct stroptions *sop;

			if (stp->st_iflag & IBLKMD) {
				stp->st_iflag = IBLKMD;
				stp->st_lflag = 0;
				stp->st_cc[VMIN] = 1;
				stp->st_cc[VTIME] = 1;
			}

			/* decide how raw & fast input is */
			stp->st_state &= ~(ST_INRAW|ST_INPASS);
			if (!(stp->st_iflag & (INLCR|IGNCR|ICRNL|IUCLC))
			    && !(stp->st_lflag & (ICANON|ISIG|ECHO))) {
				stp->st_state |= ST_INRAW;

				if (stp->st_cc[VMIN] <= 1)
					stp->st_state |= ST_INPASS;
			}

			bp = str_allocb(sizeof(struct stroptions),
					rq,BPRI_LO);
			if (!bp)
				break;
			bp->b_datap->db_type = M_SETOPTS;
			sop = (struct stroptions*)bp->b_rptr;
			bp->b_wptr += sizeof(struct stroptions);
			sop->so_flags = (SO_READOPT|SO_HIWAT|SO_LOWAT
					 |SO_ISTTY|SO_VTIME|SO_TOSTOP);

			/*
			 * XXXrs - Set STRHOLD in stream head unless
			 * it looks like the application plans to do
			 * it's own echoing or line processing.
			 */
			if (tty_auto_strhold) {
				if ((stp->st_lflag & (ECHO|ICANON)) !=
				    (ECHO|ICANON))
					sop->so_flags |= SO_NOSTRHOLD;
				else
					sop->so_flags |= SO_STRHOLD;
			}
			if (posix_tty_default)
				sop->so_hiwat = 256;
			else
				sop->so_hiwat = 1;
			sop->so_lowat = 0;
			if (stp->st_lflag & ICANON) {	/* in cooked mode, */
				sop->so_readopt = RMSGN;	/* keep EOL */
				sop->so_vtime = 0;
			} else {
				sop->so_readopt = RNORM;
				if (0 != stp->st_cc[VMIN]) {
					sop->so_vtime = 0;
				} else {
					sop->so_vtime = (stp->st_cc[VTIME]==0
							 ? -1
							 : stp->st_cc[VTIME]);
				}
			}
			/* XXXrs - TOSTOP should require IEXTEN? */
			sop->so_tostop = (stp->st_cflag & LOBLK) ||
					 (stp->st_lflag & TOSTOP);
			putnext(rq, bp);

			stp->st_state |= ST_ISTTY;
		}


		stp->st_rrunning = 1;
		rmp = getq(rq);		/* get another message */
		if (!rmp)
			break;

		switch (rmp->b_datap->db_type) {
		case M_IOCACK:
			iocp = (struct iocblk*)rmp->b_rptr;
			switch (iocp->ioc_cmd) {
			case TCXONC:
				if (1 == *(int*)(rmp->b_cont->b_rptr))
					stp->st_lflag &= ~FLUSHO;
				break;

			case TCGETA:
				/*
				 * The lflags come from us.
				 */
				STERMIO(rmp)->c_lflag = stp->st_lflag;
				break;
				
			case TCSETA:
			case TCSETAW:
			case TCSETAF:
				if (!(stp->st_lflag & ICANON)) {
				    if (STERMIO(rmp)->c_lflag & ICANON) {
					register mblk_t *imp;

					ASSERT(stp->st_lmsg == 0);
					imp = stp->st_imsg;
					if (imp) {
					    if (imp->b_rptr >= imp->b_wptr)
						freemsg(imp);
					    else {
						stp->st_llen = msgdsize(imp);
						str_conmsg(&stp->st_lmsg,
							   &stp->st_lbp,
							   imp);
					    }
					    stp->st_imsg = 0;
					    stp->st_ahead = 0;
					}
				    }
				} else {
				    if (!(STERMIO(rmp)->c_lflag & ICANON)) {
					register mblk_t *imp;

					imp = stp->st_imsg;
					if (imp && imp->b_rptr >= imp->b_wptr) {
					    freemsg(imp);
					    stp->st_imsg = 0;
					}
					if (stp->st_lmsg) {
					    str_conmsg(&stp->st_imsg,
						       &stp->st_ibp,
						       stp->st_lmsg);
					    stp->st_lmsg = 0;
					}
					stp->st_llen = 0;
					stp->st_ahead = msgdsize(stp->st_imsg);
					stp->st_state &= ~(ST_LIT|ST_ESC);
					stp->st_lflag &= ~FLUSHO;
				    }
				}
				stp->st_termio = *STERMIO(rmp);
				if (stp->st_line == LDISC0)
					stp->st_cc[VLNEXT] = CESC;
				stp->st_state &= ~ST_ISTTY;
				break;

			case TCBLKMD:
				stp->st_iflag = IBLKMD;
				stp->st_state &= ~(ST_ISTTY|ST_LIT|ST_ESC);
				stp->st_lflag &= ~FLUSHO;
				break;
			}
			putnext(rq,rmp);
			break;


		case M_FLUSH:
			st_rflush(stp,*rmp->b_rptr,rq);
			putnext(rq,rmp);
			break;

		case M_HANGUP:
			if (!putctl1(stp->st_rq->q_next, M_PCSIG,
				     (posix_tty_default ? SIGHUP : SIGTSTP))) {
				BCALL_HI(1,stp->st_rq);
				putbq(rq, rmp);
				goto for_exit;
			}
			putnext(rq, rmp);
			break;

		/* Process data messages.
		 *	In 'RAW' mode, just pass them on, without loosing
		 *	them, and without appearing to have an empty input
		 *	queue when things are constipated, so that flow
		 *	control will work.
		 *	In other modes, (try to) process the characters.
		 */
		case M_DATA:
			/*
			 * If somebody below us ("intelligent" communications
			 *  board, pseudo-tty controlled by an editor) is doing
			 * canonicalization, don't scan it for special characters.
			 */
			if (stp->st_state & ST_NOCANON) {
				putnext(rq, rmp);
				break;
			}

			if (stp->st_state & ST_INRAW) {
				if ((stp->st_ahead >= MAX_AHEAD
				     || (stp->st_state & ST_TIMED))
				    && !canput(rq->q_next)) {
					putbq(rq, rmp);
					goto for_exit;
				}
				stp->st_ahead += msgdsize(rmp);
				str_conmsg(&stp->st_imsg,
					   &stp->st_ibp, rmp);
			} else {
				rmp = st_cin(stp, rmp);
				if (0 != rmp) {
					putbq(rq, rmp);
					goto for_exit;
				}
			}
			break;

		default:
			putnext(rq,rmp);
			break;
		}
	}

for_exit:;
	if (stp->st_lmsg) {
		/* handle ICANON case */
		st_sendold(stp);

	} else if (0 == (stp->st_lflag & ICANON)
		   && 0 != (rmp = stp->st_imsg)) {
/* Here, we have a non-empty buffer waiting to go upstream.
 * So, ship it up stream if it satisfies the minimum size requirement,
 *	or if it has been waiting long enough.
 * If the stream is constipated, then we may as well go to sleep.
 */
		uint n = stp->st_cc[VMIN];
		if (n <= 1
		    || 0 != (stp->st_state & ST_TIMED)
		    || stp->st_ahead >= n) {
			if (!canput(rq->q_next)) {
				stp->st_state |= ST_TIMED;
				noenable(rq);
			} else {
				putnext(rq,rmp);
				stp->st_imsg = 0;
				stp->st_ahead = 0;
				st_untime(stp);
			}

		} else {
			n = stp->st_cc[VTIME];
			if (0 != n
			    && 0 == (stp->st_state & ST_TIMING)) {
				stp->st_tid = MP_STREAMS_TIMEOUT(rq->q_monpp,
							st_tock, stp,
							(n * (short)(HZ/10)));
				stp->st_state |= ST_TIMING;
			}
		}
	}
	stp->st_rrunning = 0;
	return(0);
}
