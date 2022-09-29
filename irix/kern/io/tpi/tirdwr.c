/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uts-comm:net/transport/tirdwr.c	1.8"
/*
 * Transport Interface Library read/write module - issue 1
 */

#include "sys/types.h"
#include "sys/param.h"
#include "sys/stream.h"
#include "sys/strmp.h"
#include "sys/stropts.h"
#include "sys/tirdwr.h"
#include "sys/tihdr.h"
#include "sys/signal.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/cred.h"
#include "sys/kmem.h"

extern int trw_cnt;

static void	strip_strhead(queue_t *q);
static void	send_fatal(queue_t *q, mblk_t *mp);
static int	check_strhead(queue_t *q);


/* 
 * stream data structure definitions 
 */
STATIC int tirdwropen(queue_t *, dev_t *, int, int, cred_t *);
STATIC int tirdwrclose(queue_t *, int, cred_t *);
STATIC int tirdwrrput(queue_t *, mblk_t *);
STATIC int tirdwrwput(queue_t *, mblk_t *);

static struct module_info tirdwr_info = {
	TIRDWR_ID, 
	"tirdwr", 
	0, 
	INFPSZ, 
	4096, 
	1024
};
static struct qinit tirdwrrinit = {
	tirdwrrput, 
	NULL, 
	tirdwropen, 
	tirdwrclose,
	NULL,
	&tirdwr_info,
	NULL
};
static struct qinit tirdwrwinit = {
	tirdwrwput,
	NULL,
	tirdwropen,
	tirdwrclose,
	NULL,
	&tirdwr_info,
	NULL
};
struct streamtab tirdwrinfo = {
	&tirdwrrinit,
	&tirdwrwinit,
	NULL,
	NULL 
};

int tirdwrdevflag = 0;

/*
 * tirdwropen - open routine gets called when the
 *	       module gets pushed onto the stream.
 */
/*ARGSUSED*/
STATIC int
tirdwropen(register queue_t *q, dev_t *devp, int flag, int sflag, cred_t *cr)
{

	register struct trw_trw *trwptr;
	register mblk_t *mp;
	register struct stroptions *stropt;

	ASSERT(q != NULL);

	/* check if already open */
	if (q->q_ptr)
		return(0);

	/* find free data structure */
	for (trwptr=trw_trw; trwptr < &trw_trw[trw_cnt]; trwptr++)
		if (!(trwptr->trw_flags & USED))
			break;
	if (trwptr >= &trw_trw[trw_cnt])
		return(ENOSPC);

	if (!check_strhead(q))
		return(EPROTO);

	if ((mp = allocb(sizeof(struct T_ordrel_req), BPRI_MED)) == NULL)
		return(EAGAIN);
	
	if ((trwptr->trw_mp = allocb(sizeof(struct stroptions),
	    BPRI_MED)) == NULL) {
		freeb(mp);
		return(EAGAIN);
	}
	trwptr->trw_mp->b_next = mp;

	/*
	 * set RPROTDIS so that zero-len M_PROTO messages generated
	 * by tirdwr in response to T_ORDREL_IND get past any modules
	 * that may be pushed above us (some modules will discard
	 * zero-len messages)
	 */
	if ((mp = allocb(sizeof(struct stroptions), BPRI_MED)) == NULL) {
		freeb(trwptr->trw_mp->b_next);
		freeb(trwptr->trw_mp);
		return(EAGAIN);
	}
	mp->b_datap->db_type = M_SETOPTS;
	stropt = (struct stroptions *)mp->b_rptr;
	stropt->so_flags = SO_READOPT;
	stropt->so_readopt = RPROTDIS;
	mp->b_wptr += sizeof(struct stroptions);
	putnext(q, mp);

	strip_strhead(q);

	/* initialize data structure */
	trwptr->trw_flags = USED;
	trwptr->trw_rdq = q;
	q->q_ptr = (caddr_t)trwptr;
	WR(q)->q_ptr = (caddr_t)trwptr;
	WR(q)->q_maxpsz = WR(q)->q_next->q_maxpsz;
	q->q_maxpsz = q->q_next->q_maxpsz;

	return(0);
}

/*
 * tirdwrclose - This routine gets called when the module
 *              gets popped off of the stream.
 */
/*ARGSUSED*/
STATIC int
tirdwrclose(register queue_t *q, int flag, cred_t *cr)
{
	register struct trw_trw *trwptr;
	register mblk_t *mp;
	register union T_primitives *pptr;
	register struct stroptions *stropt;

	ASSERT(q != NULL);

	/* assign saved data structure from queue */
	trwptr = (struct trw_trw *)q->q_ptr;
	ASSERT(trwptr != NULL);

	if ((trwptr->trw_flags & ORDREL) && !(trwptr->trw_flags & FATAL)) {
		mp = trwptr->trw_mp->b_next;
		trwptr->trw_mp->b_next = NULL;
		pptr = (union T_primitives *)mp->b_rptr;
		mp->b_wptr = mp->b_rptr + sizeof(struct T_ordrel_req);
		pptr->type = T_ORDREL_REQ;
		mp->b_datap->db_type = M_PROTO;
		putnext(WR(q), mp);
	} else {
		freeb(trwptr->trw_mp->b_next);
		trwptr->trw_mp->b_next = NULL;
	}

	mp = trwptr->trw_mp;
	mp->b_datap->db_type = M_SETOPTS;
	stropt = (struct stroptions *)mp->b_rptr;
	stropt->so_flags = SO_READOPT;
	stropt->so_readopt = RPROTNORM;
	mp->b_wptr += sizeof(struct stroptions);
	putnext(q, mp);

	trwptr->trw_flags = 0;
	trwptr->trw_rdq = NULL;
	trwptr->trw_mp = NULL;

	return(0);
}

/*
 * tirdwrrput - Module read queue put procedure.
 *             This is called from the module or
 *	       driver downstream.
 */
STATIC int
tirdwrrput(register queue_t *q, register mblk_t *mp)
{
	register union T_primitives *pptr;
	register struct trw_trw *trwptr;
	register mblk_t *tmp;

	ASSERT(q != NULL);
	trwptr = (struct trw_trw *)q->q_ptr;
	ASSERT(trwptr != NULL);
	if ((trwptr->trw_flags & FATAL) && !(trwptr->trw_flags & WAITACK)) {
		freemsg(mp);
		return(0);
	}

	switch(mp->b_datap->db_type) {
	default:
		putnext(q, mp);
		return(0);

	case M_DATA:
		if (msgdsize(mp) == 0) {
			freemsg(mp);
			return(0);
		}
		putnext(q, mp);
		return(0);

	case M_PCPROTO:
	case M_PROTO:
		/* is there enough data to check type */
		if ((mp->b_wptr - mp->b_rptr) < sizeof(int)) {
			send_fatal(q, mp);
			return(0);
		}
		pptr = (union T_primitives *)mp->b_rptr;
		switch (pptr->type) {
		case T_EXDATA_IND:
			send_fatal(q, mp);
			return(0);

		case T_DATA_IND:
			if (msgdsize(mp) == 0) {
				freemsg(mp);
				return(0);
			}

			tmp = (mblk_t *)unlinkb(mp);
			freemsg(mp);
			putnext(q, tmp);
			return(0);

		case T_ORDREL_IND:
			/*
			 * hide zero-len message in M_PROTO
			 * so won't be discarded by any modules
			 * that may be pushed above us
			 */
			trwptr->trw_flags |= ORDREL;
			mp->b_datap->db_type = M_PROTO;
			mp->b_wptr = mp->b_rptr;
			if ((mp->b_cont = allocb(0, BPRI_MED)) != NULL) {
				mp->b_cont->b_datap->db_type = M_DATA;
				mp->b_cont->b_wptr = mp->b_cont->b_rptr;
			} else {
				/*
				 * send up zero-len M_DATA, then, on the
				 * chance it may get thru
				 */
				mp->b_datap->db_type = M_DATA;
			}
			putnext(q, mp);
			return(0);

		case T_DISCON_IND:
			trwptr->trw_flags |= DISCON;
			trwptr->trw_flags &= ~ORDREL;
			if (msgdsize(mp) != 0) {
				tmp = (mblk_t *)unlinkb(mp);
				putnext(q, tmp);
			}
			mp->b_datap->db_type = M_HANGUP;
			mp->b_wptr = mp->b_rptr;
			putnext(q, mp);
			return(0);

		default:
			send_fatal(q, mp);
			return(0);
		}
	}
}

/*
 * tirdwrwput - Module write queue put procedure.
 *             This is called from the module or
 *	       stream head upstream.
 */
STATIC int
tirdwrwput(register queue_t *q, register mblk_t *mp)
{
	register struct trw_trw *trwptr;

	ASSERT(q != NULL);
	trwptr = (struct trw_trw *)q->q_ptr;
	ASSERT(trwptr != NULL);

	if (trwptr->trw_flags & FATAL) {
		freemsg(mp);
		return(0);
	}

	switch(mp->b_datap->db_type) {
	default:
		putnext(q, mp);
		return(0);

	case M_DATA:
		if (msgdsize(mp) == 0) {
			freemsg(mp);
			return(0);
		}
		putnext(q, mp);
		return(0);

	case M_PROTO:
	case M_PCPROTO:
		send_fatal(q, mp);
		return(0);
	}
}

static void
send_fatal(register queue_t *q, register mblk_t *mp)
{
	register struct trw_trw *trwptr;

	trwptr = (struct trw_trw *)q->q_ptr;
	trwptr->trw_flags |= FATAL;
	mp->b_datap->db_type = M_ERROR;
	*mp->b_datap->db_base = EPROTO;
	mp->b_rptr = mp->b_datap->db_base;
	mp->b_wptr = mp->b_datap->db_base + sizeof(char);
	freemsg(unlinkb(mp));
	if (q->q_flag&QREADR)
		putnext(q, mp);
	else
		qreply(q, mp);
}

static int
check_strhead(queue_t *q)
{
	register mblk_t *mp;
	register union T_primitives *pptr;

	for (mp = q->q_next->q_first; mp != NULL; mp = mp->b_next) {
		switch(mp->b_datap->db_type) {
		case M_PROTO:
			pptr = (union T_primitives *)mp->b_rptr;
			if ((mp->b_wptr - mp->b_rptr) < sizeof(int))
				return(0);
			switch (pptr->type) {
			case T_EXDATA_IND:
				return(0);

			case T_DATA_IND:
				if (mp->b_cont &&
				    (mp->b_cont->b_datap->db_type != M_DATA))
					return(0);
				break;

			default:
				return(0);
			}
			break;

		case M_PCPROTO:
			return(0);

		case M_DATA:
		case M_SIG:
			break;

		default:
			return(0);
		}
	}
	return(1);
}

static void
strip_strhead(queue_t *q)
{
	register mblk_t *mp;
	register mblk_t *emp;
	register mblk_t *tmp;
	register union T_primitives *pptr;

	q = q->q_next;
	for (mp = q->q_first; mp != NULL;) {
		switch(mp->b_datap->db_type) {
		case M_PROTO:
			pptr = (union T_primitives *)mp->b_rptr;
			switch (pptr->type) {
			case T_DATA_IND:
				if (msgdsize(mp) == 0) {
strip0:
					tmp = mp->b_next;
					rmvq(q, mp);
					freemsg(mp);
					mp = tmp;
					break;
				}
				emp = mp->b_next;
				rmvq(q, mp);
				tmp = (mblk_t *)unlinkb(mp);
				freeb(mp);
				insq(q, emp, tmp);
				mp = emp;
				break;
			}
			break;

		case M_DATA:
			if (msgdsize(mp) == 0)
				goto strip0;
			mp = mp->b_next;
			break;

		case M_SIG:
			mp = mp->b_next;
			break;
		}
	}
	return;
}
