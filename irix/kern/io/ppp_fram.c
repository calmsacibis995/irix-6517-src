/* PPP input framer
 *
 * Copyright 1993 Silicon Graphics, Inc.  All rights reserved.
 */

/* For the BSD compress code:
 *
 * Copyright (c) 1985, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods, derived from original work by Spencer Thomas
 * and Joseph Orost.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ident "$Revision: 1.38 $"

#include "sys/param.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/stream.h"
#include "sys/strmp.h"
#include "sys/strids.h"
#include "sys/stropts.h"
#include "sys/systm.h"
#include "sys/ioctl.h"
#include "sys/conf.h"
#include "sys/ddi.h"
#include "bstring.h"
#include "sys/mbuf.h"

#include "sys/if_ppp.h"


/* discard a frame this often to stress things */
int ppp_fram_test, ppp_fram_test_cnt;

/* discard a byte from async streams this often */
int ppp_fram_async_test, ppp_fram_async_test_cnt;

/* trash occassional sync frames */
int ppp_fram_sync_test, ppp_fram_sync_cnt;


#ifndef STR_UNLOCK_SPL
#define STR_UNLOCK_SPL(s) splx(s)
#undef STR_LOCK_SPL
#define STR_LOCK_SPL(s) (s = splstr())
#endif
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63)
int ppp_framdevflag = 0;
#else
int ppp_framdevflag = D_MP;
#endif

struct module_info ppp_fram_info = {
	STRID_PPP_FRAME,		/* module ID */
	"ppp_fram",			/* module name */
	2,				/* minimum HDLC packet size */
	PPP_BUF_MAX,			/* maximum packet size	*/
	2*PPP_DEF_MRU,			/* hi-water mark */
	0,				/* lo-water mark */
	0,
};

static void pf_rput(queue_t*, mblk_t*);
static void pf_rsrv(queue_t*);
static int pf_open(queue_t*, dev_t*, int, int, struct cred*);
static pf_close(queue_t*, int, struct cred*);
static void pf_wput(queue_t*, mblk_t*);

static struct qinit pf_rinit = {
	(int (*)(queue_t*,mblk_t*))pf_rput,
	(int (*)(queue_t*))pf_rsrv, pf_open, pf_close, 0, &ppp_fram_info, 0
};
static struct qinit pf_winit = {
	(int (*)(queue_t*,mblk_t*))pf_wput,
	0, 0, 0, 0, &ppp_fram_info, 0
};

struct streamtab ppp_framinfo = {&pf_rinit, &pf_winit, 0,0};


/* everything there is to know about each PPP framer */
struct ppp_pf *ppp_pf_first = 0;
static int pf_num = 0;

#define MAX_MODS (MAX_PPP_DEVS*MAX_PPP_LINKS)


/* FCS table from RFC-1331 */
u_short ppp_fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};



/* allocate or free a structure */
#define KSALLOC(s,n) ((struct s*)kern_malloc(sizeof(struct s)*(n)))
#define KSFREE(s) {if ((s) != 0) {kern_free(s); (s) = 0;}}


#define CK_WPTR(bp) ASSERT((bp)->b_wptr <= (bp)->b_datap->db_lim)


/* open PPP Framer STREAMS module
 */
/* ARGSUSED */
static int				/* 0, minor #, or failure reason */
pf_open(queue_t *rq,			/* our read queue */
	dev_t *devp,
	int flag,
	int sflag,
	struct cred *crp)
{
	struct ppp_pf *pfp, **pfpp;
	int s;

	if (sflag != MODOPEN)
		return EINVAL;

	if (0 != rq->q_ptr) {
		ASSERT(((struct ppp_pf*)rq->q_ptr)->pf_rq == rq);

	} else {			/* initialize on the 1st open */
		STR_LOCK_SPL(s);
		/* look for an available slot */
		pfpp = &ppp_pf_first;
		for (;;) {
			pfp = *pfpp;
			if (!pfp) {
				if (pf_num >= MAX_MODS) {
					STR_UNLOCK_SPL(s);
					return EBUSY;
				}
				/* make one if needed and possible */
				pfp = KSALLOC(ppp_pf,1);
				if (!pfp) {
					STR_UNLOCK_SPL(s);
					return ENOMEM;
				}
				bzero(pfp,sizeof(*pfp));
				*pfpp = pfp;
			}
			if (!pfp->pf_rq)
				break;
			pfpp = &pfp->pf_next;
		}

		rq->q_ptr = (caddr_t)pfp;
		pfp->pf_rq = rq;
		WR(rq)->q_ptr = (caddr_t)pfp;
		pfp->pf_wq = WR(rq);
		bzero(&pfp->pf_meters,sizeof(pfp->pf_meters));
		pfp->pf_accm = 0;
		pfp->pf_index = -1;
		pfp->pf_head = 0;
		pfp->pf_tail = (mblk_t*)0xf00dbadL;
		pfp->pf_state = (PF_ST_GAP | PF_ST_OFF_RX);
		pfp->pf_ccp_id = PPP_CCP_DISARM_ID;
		STR_UNLOCK_SPL(s);
	}

	return 0;
}



/* flush input or output
 */
static void
pf_flush(u_char op,
	 struct ppp_pf *pfp)
{
	int s;

	STR_LOCK_SPL(s);
	if (op & FLUSHR) {		/* free partially received packet */
		flushq(pfp->pf_rq,FLUSHALL);
		str_unbcall(pfp->pf_rq);
		freemsg(pfp->pf_head);
		pfp->pf_head = 0;
		pfp->pf_tail = (mblk_t*)0xfeedbfdL;
		pfp->pf_state &= ~(PF_ST_ESC | PF_ST_HEAD_RDY);
		pfp->pf_state |= PF_ST_GAP;
	}
	STR_UNLOCK_SPL(s);
}



/* close PPP STREAMS module */
/* ARGSUSED */
static
pf_close(queue_t *rq, int flag, struct cred *crp)
{
	struct ppp_pf *pfp = (struct ppp_pf*)rq->q_ptr;
	int s;

	STR_LOCK_SPL(s);
	ASSERT(pfp->pf_rq == rq);
	ASSERT(pfp->pf_wq->q_ptr == (caddr_t)pfp);

	pf_flush(FLUSHRW, pfp);

	rq->q_ptr = 0;
	pfp->pf_wq->q_ptr = 0;
	pfp->pf_rq = 0;
	pfp->pf_wq = 0;

	STR_UNLOCK_SPL(s);
	return 0;			/* XXX it is really a void */
}



static void
pf_ack_ioctl(struct ppp_pf *pfp,
	     struct ppp_arg *arg,
	     mblk_t *bp)
{
	if (pfp->pf_index == -1)
		pfp->pf_index = arg->dev_index;
	arg->version = IF_PPP_VERSION;
	bp->b_datap->db_type = M_IOCACK;
	qreply(pfp->pf_wq,bp);
}



/* turn on input compression for IOCTL
 */
int
pf_sioc_rx_init(struct iocblk *iocp,
		struct ppp_arg *arg,
		struct pred_db **predp, struct bsd_db **bsdp,
		int pred_bit, int bsd_bit)
{
	if (arg->v.ccp.proto & SIOC_PPP_CCP_PRED1) {
		KSFREE(*bsdp);
		*predp = pf_pred1_init(*predp,
				       arg->v.ccp.unit,
				       arg->v.ccp.mru);
		if (*predp != 0) {
			(*predp)->db_i.debug = arg->v.ccp.debug;
			return pred_bit;
		}

	} else if (arg->v.ccp.proto & SIOC_PPP_CCP_BSD) {
		KSFREE(*predp);
		*bsdp = pf_bsd_init(*bsdp,
				    arg->v.ccp.unit,
				    arg->v.ccp.bits,
				    arg->v.ccp.mru);
		if (*bsdp != 0) {
			(*bsdp)->debug = arg->v.ccp.debug;
			return bsd_bit;
		}
	} else {
		return 0;
	}

	iocp->ioc_error = ENOMEM;
	return 0;
}


/* pass output along
 */
static void
pf_wput(queue_t *wq,			/* our write queue */
	mblk_t *bp)
{
	struct ppp_pf *pfp = (struct ppp_pf*)wq->q_ptr;
	queue_t *rq = RD(wq);
	struct iocblk *iocp;
	struct ppp_arg *arg;
	int i, s;
#	define CK_ARG {						\
	if (iocp->ioc_count == sizeof(*arg)) {			\
		arg = (struct ppp_arg*)bp->b_cont->b_rptr;	\
	} else {						\
		bp->b_datap->db_type = M_IOCNAK;		\
		qreply(pfp->pf_wq,bp);				\
		STR_UNLOCK_SPL(s);				\
		return;}}


	ASSERT(pfp->pf_wq == wq);
	ASSERT(rq->q_ptr == (caddr_t)pfp);

	switch (bp->b_datap->db_type) {
	case M_FLUSH:
		pf_flush(*bp->b_rptr, pfp);
		break;

	case M_IOCTL:
		iocp = (struct iocblk*)bp->b_rptr;
		STR_LOCK_SPL(s);
		switch (iocp->ioc_cmd) {
		case SIOC_PPP_LCP:
			CK_ARG;
			pfp->pf_state &= ~PF_ST_SYNC;
			if (arg->v.lcp.bits & SIOC_PPP_LCP_SYNC)
				pfp->pf_state |= PF_ST_SYNC;
			pfp->pf_accm = arg->v.lcp.rx_accm;
			STR_UNLOCK_SPL(s);
			pf_ack_ioctl(pfp, arg, bp);
			return;

		case SIOC_PPP_RX_OK:
			CK_ARG;
			if (arg->v.ok) {
				pfp->pf_state &= ~(PF_ST_OFF_RX|PF_ST_STUTTER);
				qenable(rq);
			} else {
				pfp->pf_state |= PF_ST_OFF_RX;
			}
			STR_UNLOCK_SPL(s);
			pf_ack_ioctl(pfp, arg, bp);
			return;

		case SIOC_PPP_1_RX:
			CK_ARG;
			pfp->pf_state |= PF_ST_STUTTER;
			pfp->pf_state &= ~PF_ST_OFF_RX;
			STR_UNLOCK_SPL(s);
			pf_ack_ioctl(pfp, arg, bp);
			qenable(rq);
			return;

		case SIOC_PPP_CCP_RX:
			CK_ARG;
			pfp->pf_state &= ~(PF_ST_CP
					   | PF_ST_OFF_RX
					   | PF_ST_STUTTER);
			pfp->pf_ccp_id = PPP_CCP_DISARM_ID;
			STR_UNLOCK_SPL(s);
			i = pf_sioc_rx_init(iocp, arg,
					    &pfp->pf_pred, &pfp->pf_bsd,
					    PF_ST_RX_PRED, PF_ST_RX_BSD);
			STR_LOCK_SPL(s);
			pfp->pf_state |= i;
			STR_UNLOCK_SPL(s);
			qenable(rq);
			pf_ack_ioctl(pfp, arg, bp);
			return;

		case SIOC_PPP_CCP_ARM_CA:
			CK_ARG;
			/* If the daemon had to separately arm the CCP
			 * Configure-Ack watcher and enable input
			 * compression, it would be hard to avoid
			 * the race between the two operations.
			 * So arming the watcher also turns on input.
			 */
			ASSERT(iocp->ioc_count == sizeof(*arg));
			pfp->pf_state &= ~(PF_ST_OFF_RX | PF_ST_STUTTER);
			pfp->pf_ccp_id = arg->v.ccp_arm.id;
			STR_UNLOCK_SPL(s);
			qenable(rq);
			pf_ack_ioctl(pfp, arg, bp);
			return;

		case SIOC_PPP_MP:
			CK_ARG;
			if (arg->v.mp.on) {
				pfp->pf_state &= ~(PF_ST_CP
						   | PF_ST_STUTTER
						   | PF_ST_OFF_RX);
				pfp->pf_state |= PF_ST_MP;
				pfp->pf_ccp_id = PPP_CCP_DISARM_ID;
				STR_UNLOCK_SPL(s);
				KSFREE(pfp->pf_bsd);
				KSFREE(pfp->pf_pred);
				qenable(rq);
			} else {
				pfp->pf_state &= ~PF_ST_MP;
				STR_UNLOCK_SPL(s);
			}
			pf_ack_ioctl(pfp, arg, bp);
			return;

		}
		STR_UNLOCK_SPL(s);
		break;
	}

	putnext(wq, bp);
#undef ARG
#undef CK_ARG
}



/* Send a frame or error indication upstream.
 *	pfp->pf_type=type of packet
 *	pfp->p_head=start of packet
 *
 */
static int				/* 0=could not send everything */
pf_sendup(struct ppp_pf *pfp)
{
	register struct ppp_buf *pb;
	register mblk_t *bp;

	if ((pfp->pf_state & PF_ST_OFF_RX)
	    || !canput(pfp->pf_rq->q_next)) {
		pfp->pf_state |= PF_ST_HEAD_RDY;
		return 0;
	}

	pfp->pf_state &= ~PF_ST_HEAD_RDY;

	bp = pfp->pf_head;
	pb = (struct ppp_buf*)bp->b_rptr;
	ASSERT(bp->b_wptr >= bp->b_rptr+PPP_BUF_HEAD);
	ASSERT(PPP_BUF_ALIGN(pb));

	pfp->pf_head = 0;

	/* Must delay checking for Configure-Ack or running uncompressed
	 * packets through the BSD-Compress dictionary until ready to
	 * send upstream to avoid the race.
	 */

	if (pfp->pf_type == BEEP_FRAME) {
		/* All of the header and the protocol field of the
		 * PPP frame must be in the first STREAMS buffer.
		 */
		ASSERT(bp->b_wptr >= bp->b_rptr+PPP_BUF_MIN);

		/* Remove FCS from async frames, quickly.
		 */
		if (!(PF_ST_SYNC & pfp->pf_state)) {
			if (pfp->pf_tail->b_rptr+2
			    <= pfp->pf_tail->b_wptr) {
				pfp->pf_tail->b_wptr -= 2;
			} else {
				(void)adjmsg(bp,-2);
				pb = (struct ppp_buf*)bp->b_rptr;
			}
		}

		pfp->pf_tail = (mblk_t*)0xfeedbadL;

		/* trash a frame periodically to stress things */
		if (ppp_fram_test != 0
		    && ++ppp_fram_test_cnt >= ppp_fram_test) {
			ppp_fram_test_cnt = 0;
			freemsg(bp);
			return 1;
		}

		/* The MUX deals with all compression for IETF multilink
		 */
		if ((pfp->pf_state & PF_ST_MP))
			goto notframe;

		switch (pb->proto) {
		case PPP_CP:
			/* Compressed packet.
			 *  If compression is enabled, try to decompress it.
			 *  On failure, send upstream to ask the daemon
			 *  to renegotiate.
			 *
			 * This module must decompress in case the peer sends
			 * compressed packets when the stream has not been
			 * linked to the STREAMS mux.  It is also convenient
			 * to use the queue of this module when compression
			 * is blocked in BF&I multilink.
			 */
			pfp->pf_meters.cp_rx++;
			if (pfp->pf_state & PF_ST_RX_BSD) {
				bp = pf_bsd_decomp(pfp->pf_bsd,bp);
			} else if (pfp->pf_state & PF_ST_RX_PRED) {
				bp = pf_pred1_decomp(pfp->pf_pred,bp);
			} else {
				pfp->pf_type = BEEP_CP_BAD;
				pfp->pf_meters.cp_rx_bad++;
			}
			if (bp != 0) {
				pb = (struct ppp_buf*)bp->b_rptr;
				break;
			}

			/* if decompression failed, tell the daemon
			 */
			pfp->pf_meters.cp_rx_bad++;
			pfp->pf_type = BEEP_CP_BAD;
			bp = allocb(PPP_BUF_MIN, BPRI_HI);
			if (!bp)
				return 1;
			pb = (struct ppp_buf*)bp->b_wptr;
			bp->b_wptr += PPP_BUF_MIN;
			CK_WPTR(bp);
			pfp->pf_state &= ~PF_ST_CP;
			break;

		case PPP_CCP:
			if (!pullupmsg(bp,
				       PPP_BUF_MIN+sizeof(pb->un.pkt))) {
				pfp->pf_meters.nobuf++;
				freemsg(bp);
				return 1;
			}
			pb = (struct ppp_buf*)bp->b_rptr;
			switch (pb->un.pkt.code) {
			case PPP_CODE_CA:
				/* If we were waiting for a Configure-ACK,
				 * then tell the daemon and stop things.
				 */
				if (pb->un.pkt.id == pfp->pf_ccp_id)
					pfp->pf_state |= PF_ST_STUTTER;
				break;
			case PPP_CODE_RACK:
				if (pfp->pf_state & PF_ST_CP)
					pfp->pf_state |= PF_ST_STUTTER;
				break;
			}
			break;

		default:
			ASSERT(MIN_BSD_COMP_PROTO == 0);
			if ((pfp->pf_state & PF_ST_RX_BSD)
			    && pb->proto <= MAX_BSD_COMP_PROTO)
				pf_bsd_incomp(pfp->pf_bsd, bp, pb->proto);
			break;
		}
	}

notframe:
	pb->type = pfp->pf_type;
	pb->dev_index = pfp->pf_index;
	pb->bits = 0;

	putnext(pfp->pf_rq, bp);

	if (pfp->pf_state & PF_ST_STUTTER) {
		/* Async frames cannot be decoded ahead of time because the
		 * rx ACCM might be about to change.  Depending on the ACCM,
		 * bytes will be ignored.  So stop running after a success.
		 */
		pfp->pf_state |= PF_ST_OFF_RX;
		return 0;
	}
	return 1;
}


/* accept a new STREAMS message from the serial line
 */
static void
pf_rput(queue_t *rq,			/* data read queue */
	mblk_t *bp)
{
	struct ppp_pf *pfp = (struct ppp_pf*)rq->q_ptr;
	struct ppp_buf *pb;
	mblk_t *nbp;

	ASSERT(pfp->pf_rq == rq);
	ASSERT(pfp->pf_wq->q_ptr == (caddr_t)pfp);

	switch (bp->b_datap->db_type) {
	case M_DATA:
	case M_PROTO:
	case M_PCPROTO:
		putq(rq,bp);
		return;

	case M_FLUSH:
		pf_flush(*bp->b_rptr, pfp);
		break;

	case M_HANGUP:
		/* try not to destroy stream stack with a hang-up message.
		 */
		nbp = allocb(PPP_BUF_MIN,BPRI_HI);
		if (nbp != 0) {
			freemsg(bp);
			bp = nbp;
			pb = (struct ppp_buf*)bp->b_wptr;
			bp->b_wptr += PPP_BUF_MIN;
			CK_WPTR(bp);
			bzero(pb,PPP_BUF_MIN);
			pb->type = BEEP_DEAD;
			pb->dev_index = pfp->pf_index;
		}
	}

	/* Just pass non-data messages upstream, including M_CTLs
	 * containing ISDN activity indications.
	 */
	putnext(rq, bp);
}



/* deal with accumulated streams buffers
 */
static void
pf_rsrv(queue_t *rq)
{
	register struct ppp_pf *pfp;
	register u_char *wptr, *rptr;
	register char c;
	register int lim, len;
	register u_short fcs;
	register mblk_t *bp, *nbp;
	register int wlen;


	pfp = (struct ppp_pf*)rq->q_ptr;

	ASSERT(pfp->pf_rq == rq);
	ASSERT(pfp->pf_wq->q_ptr == (caddr_t)pfp);


	if ((pfp->pf_state & PF_ST_HEAD_RDY)
	    && !pf_sendup(pfp))
		return;

	/* handle synchronous packets */
	if (PF_ST_SYNC & pfp->pf_state) for (;;) {
		register struct ppp_buf *pb;
nxt_sync:

		bp = getq(rq);
		if (0 == bp)
			return;

		/* No fragmented LLC frames here.
		 * The sync. drivers must provide LLC frames in
		 * single messages consisting of 1 or more STREAMS
		 * buffers.
		 */
		ASSERT(pfp->pf_head == 0);

		/* put header on the front */
		nbp = allocb(PPP_BUF_HEAD_INFO, BPRI_HI);
		if (!nbp) {
			pfp->pf_meters.nobuf++;
			freemsg(bp);
			goto nxt_sync;
		}
		pb = (struct ppp_buf*)nbp->b_wptr;
		pb->addr = 0;
		pb->cntl = 0;
		nbp->b_wptr += PPP_BUF_MIN;

		len = msgdsize(bp);
		pfp->pf_meters.raw_ibytes += len;
		if (len < 1) {
			/* Send an indication of null frames up stream
			 * so that the VJ decompressor can reset itself
			 */
			pfp->pf_meters.null++;
			pfp->pf_type = BEEP_TINY;
			freemsg(bp);
			pb->proto = 0;

		} else if (ppp_fram_sync_test != 0
			   && ++ppp_fram_sync_test >= ppp_fram_sync_test) {
			/* trash a frame occassionally to stress things */
			pfp->pf_type = BEEP_FCS;
			freemsg(bp);
			pb->proto = 0;

		} else if (bp->b_datap->db_type == M_DATA) {
			nbp->b_cont = bp;
			rptr = bp->b_rptr;
			if (rptr+4 >= bp->b_wptr) {
				(void)pullupmsg(bp, MIN(4,len));
				rptr = bp->b_rptr;
			}
			/* remove address and control fields */
			if (rptr+2 <= bp->b_wptr
			    && *rptr == PPP_ADDR
			    && *(rptr+1) == PPP_CNTL) {
				pb->addr = PPP_ADDR;
				pb->cntl = PPP_CNTL;
				rptr += 2;
			} else {
				pb->addr = 0;
				pb->cntl = 0;
			}
			if (rptr >= bp->b_wptr) {
				/* bad packet if no PPP header */
				pb->proto = 0;
				pfp->pf_type = BEEP_TINY;
			} else {
				/* get the protocol field,
				 * expanding it if compressed,
				 * and faking protocol 0 if
				 * the buffer is short */
				pb->proto = *rptr++;
				if (0 == (1 & pb->proto)
				    && rptr < bp->b_wptr)
					pb->proto = ((pb->proto << 8)
						     + *rptr++);
				bp->b_rptr = rptr;
				pfp->pf_type = BEEP_FRAME;
				if (len > PPP_DEF_MRU+4) {
					if (len > PPP_MAX_MTU+4) {
					    pfp->pf_meters.babble++;
					    pfp->pf_type = BEEP_BABBLE;
					} else {
					    pfp->pf_meters.long_frame++;
					}
				}
			}

		} else {
			/* XXX It would be nice to copy the
			 * data of the bad packet from the ISDN
			 * driver to a simple data packet.
			 */
			pfp->pf_meters.bad_fcs++;
			pfp->pf_type = BEEP_FCS;
			freemsg(bp);
			pb->proto = 0;
		}
		pfp->pf_head = nbp;
		pfp->pf_tail = (mblk_t*)0xfeedb4dL;
		if (!pf_sendup(pfp))
			return;
	}


	/* Assemble frames from stream of async bytes.
	 *
	 * Async frames cannot be decoded ahead of time because the
	 * rx ACCM might be about to change.  Depending on the ACCM,
	 * bytes will be ignored.  So stop running after successfully
	 * assembling a frame, when in the one-at-a-time mode set
	 * by the daemon.
	 */
	if (pfp->pf_state & PF_ST_OFF_RX)
		return;
	fcs = pfp->pf_fcs;
	len = pfp->pf_len;
	bp = 0;
	for (;;) {
nxt_async:
		if (bp == 0
		    && (bp = getq(rq)) == 0) {
			pfp->pf_len = len;
			pfp->pf_fcs = fcs;
			return;
		}

		/* advance to the next block of the async. stream */
		rptr = bp->b_rptr;
		lim = bp->b_wptr - rptr;
		if (lim == 0) {
			nbp = bp->b_cont;
			freeb(bp);
			bp = nbp;
			continue;
		}

		/* Optimize for the common case, which is copying bytes
		 * that are not escaped in the middle of a frame.
		 *
		 * Look for the start of the next frame.
		 * Skip bytes until a framing byte.  We must not
		 * do this if we just saw a framing byte.
		 */
		if (pfp->pf_state & PF_ST_GAP) {
			do {
				c = *rptr++;
				if (pfp->pf_state & PF_ST_ESC) {
					pfp->pf_state &= ~PF_ST_ESC;
					continue;
				}
				if (c == PPP_ESC) {
					pfp->pf_state |= PF_ST_ESC;
					continue;
				}
				if (c == PPP_FLAG) {
					pfp->pf_state &= ~PF_ST_GAP;
					break;
				}
			} while (--lim != 0);
			pfp->pf_meters.raw_ibytes += rptr - bp->b_rptr;
			bp->b_rptr = rptr;
			continue;
		}

		/* Get a new buffer for framed data, if needed.
		 */
		if (!pfp->pf_head
		    || (wptr = pfp->pf_tail->b_wptr,
			wlen = pfp->pf_tail->b_datap->db_lim - wptr,
			wlen == 0)) {
			wlen = msgdsize(bp);
			/* Ensure that it is big enough for the PPP structure
			 * and a TCP ACK of data. */
			if (!pfp->pf_head) {
				if (wlen < 5)
					wlen = 5;   /* VJ-compressed header */
				wlen += (PPP_BUF_MIN
					 + MAX(PRED_OVHD,BSD_OVHD));
			}
			nbp = allocb(MIN(wlen,PPP_MAX_SBUF), BPRI_HI);
			if (!nbp) {
				pfp->pf_meters.nobuf++;
				pfp->pf_state |= PF_ST_GAP;
				freemsg(bp);
				bp = 0;
				goto nxt_async;
			}
			wptr = nbp->b_wptr;
			if (!pfp->pf_head) {
				wptr += PPP_BUF_HEAD;
				len = 0;
				fcs = PPP_FCS_INIT;
				pfp->pf_head = nbp;
			} else {
				pfp->pf_tail->b_cont = nbp;
			}
			pfp->pf_tail = nbp;
			wlen = nbp->b_datap->db_lim - wptr;
		}

		/* Expand compressed address, control, and protocol fields.
		 */
		if (len <= 4) {
			if (len >= 2) {
				register struct ppp_buf *pb;

				pb = (struct ppp_buf*)(pfp->pf_head->b_rptr);

				/* reconstitute the address and control
				 * fields.
				 *
				 * Do not bother to check that the output
				 * STREAMS buffer is big enough because we
				 * are at the start the brand new buffer.
				 */
				if (pb->addr != PPP_ADDR
				    || pb->cntl != PPP_CNTL) {
					pb->un.l_info[0] = pb->proto<<16;
					pb->proto = (pb->addr<<8)+pb->cntl;
					pb->addr = PPP_ADDR;
					pb->cntl = PPP_CNTL;
					len += 2;
					wptr += 2;
					wlen -= 2;
				}

				/* expand the protocol field */
				if (len >= 3
				    && ((pb->proto>>8)&1) != 0) {
					pb->un.l_info[0]=((pb->un.l_info[0]>>8)
							+(pb->proto<<24));
					pb->proto >>= 8;
					len++;
					wptr++;
					wlen--;
				}
			}
			/* If we still do not have the address, control,
			 * and protocols fields,
			 * then be sure we come back soon.
			 */
			if (len < 4)
				wlen = MIN(wlen, 4-len);
		}

		lim = MIN(lim, wlen);

		/* limit babble frames */
		lim = MIN(lim, PPP_MAX_MTU - len);
		if (lim <= 0) {
			pfp->pf_meters.babble++;
			pfp->pf_state |= PF_ST_GAP;
			pfp->pf_type = BEEP_BABBLE;
			if (!pf_sendup(pfp)) {
				putbq(rq, bp);
				return;
			}
			goto nxt_async;
		}

		/* Do not check the escape state in the inner loop. */
		if (pfp->pf_state & PF_ST_ESC) {
			pfp->pf_state &= ~PF_ST_ESC;
			c = PPP_ESC;
			lim++;
		} else {
			c = *rptr++;
		}
		for (;;) {
			/* trash a byte  to stress things */
			if (ppp_fram_async_test != 0
			    && (++ppp_fram_async_test_cnt
				>= ppp_fram_async_test)) {
				ppp_fram_async_test_cnt = 0;
				if (--lim == 0)
					break;
				c = *rptr++;
			}

			if (c == PPP_FLAG) {
				pfp->pf_meters.raw_ibytes += rptr - bp->b_rptr;
				bp->b_rptr = rptr;
				pfp->pf_tail->b_wptr = wptr;
				CK_WPTR(pfp->pf_tail);
				if (pfp->pf_state & PF_ST_ESC) {
					pfp->pf_state &= ~PF_ST_ESC;
					pfp->pf_state |= PF_ST_GAP;
					pfp->pf_meters.abort++;
					pfp->pf_type = BEEP_ABORT;
				} else if (len <= 4) {
					if (len == 0) {
						pfp->pf_meters.null++;
						goto nxt_async;
					}
					pfp->pf_meters.tiny++;
					pfp->pf_type = BEEP_TINY;
				} else if (fcs != PPP_FCS_GOOD) {
					pfp->pf_meters.bad_fcs++;
					pfp->pf_type = BEEP_FCS;
				} else {
					pfp->pf_type = BEEP_FRAME;
					if (len > PPP_DEF_MRU+4)
					    pfp->pf_meters.long_frame++;
				}
				if (!pf_sendup(pfp)) {
					putbq(rq, bp);
					return;
				}
				goto nxt_async;
			}

			if (c == PPP_ESC) {
				if (--lim == 0) {
					pfp->pf_state |= PF_ST_ESC;
					break;
				}
				c = *rptr++;
				if (c == PPP_FLAG) {
					/* an escaped-flag aborts the frame */
					pfp->pf_meters.abort++;
					pfp->pf_state |= PF_ST_GAP;
					pfp->pf_tail->b_wptr = wptr;
					CK_WPTR(pfp->pf_tail);
					pfp->pf_meters.raw_ibytes += (rptr
							- bp->b_rptr);
					bp->b_rptr = rptr;
					pfp->pf_type = BEEP_ABORT;
					if (!pf_sendup(pfp)) {
						putbq(rq, bp);
						return;
					}
					goto nxt_async;
				}
				c ^= PPP_ESC_BIT;
				*wptr++ = c;
				len++;
				fcs = PPP_FCS(fcs,c);

			} else if (c >= PPP_ACCM_RX_MAX
				   || 0 == ((pfp->pf_accm>>c) & 1)) {
				/* if not a byte to ignore, then save it */
				*wptr++ = c;
				len++;
				fcs = PPP_FCS(fcs,c);
			}

			if (--lim == 0)
				break;
			c = *rptr++;
		}

		/* ran out of bytes */
		pfp->pf_tail->b_wptr = wptr;
		CK_WPTR(pfp->pf_tail);
		pfp->pf_meters.raw_ibytes += rptr - bp->b_rptr;
		bp->b_rptr = rptr;
	}
}




/* PPP "Predictor" compression */

#define PRED_HASH(h,x) ((u_short)(((h)<<4) ^ (x)))

#define PRED_LEN_MASK	0x7fff
#define PRED_COMP_BIT	0x8000		/* packet was compressible */


/* Initialize the database.
 */
struct pred_db *
pf_pred1_init(struct pred_db *db, int unit, int mru)
{
	if (!db) {
		db = KSALLOC(pred_db,1);
		if (!db)
			return 0;
	}

	bzero(db, sizeof(*db));
	db->db_i.unit = unit;
	db->db_i.mru = mru;
	return db;
}


/* compress a packet using Predictor Type 1.
 *	Assume the protocol is known to be >= 0x21 and < 0xff.
 */
int
pf_pred1_comp(struct pred_db *db,
	      u_char *cp_buf,		/* compress into here */
	      int proto,		/* this original PPP protocol */
	      struct mbuf *m,		/* from here */
	      int dlen,			/* data not counting protocol */
	      int pc)			/* !=0 to compress protocol field */
{
	register int index;
	register u_short hash;
	register u_short fcs;
	register u_char flags, *flagsp;
	register int tlen, mlim, slim;
	register u_char *rptr, *wptr, *wptr0;
	u_char c_proto[2];
	struct mbuf *n;


	if (pc) {
		c_proto[0] = proto;
		db->db_i.comp += PRED_OVHD-1;
		mlim = 1;
	} else {
		c_proto[0] = proto>>8;
		c_proto[1] = proto;
		db->db_i.comp += PRED_OVHD;
		mlim = 2;
	}
	rptr = c_proto;			/* real protocol ID at packet start */

	tlen = dlen + mlim;
	fcs = PPP_FCS_INIT;
	fcs = PPP_FCS(fcs,tlen>>8);
	fcs = PPP_FCS(fcs,tlen);

	hash = db->db_i.hash;
	n = m;
	wptr = &cp_buf[2];
	wptr0 = wptr+mlim;
	flags = 0;
	flagsp = wptr++;
	index = 8;
	for (;;) {
		if (!mlim) {
			if (!n) {
				*flagsp = flags >> index;
				db->db_i.hash = hash;
				break;
			}
			mlim = n->m_len;
			rptr = mtod(n, u_char*);
			n = n->m_next;
			if (!mlim)
				continue;   /* skip empty mbufs */
		}
		if (!index) {
			*flagsp = flags;
			flagsp = wptr++;
			index = 8;
		}
		slim = MIN(mlim, index);
		index -= slim;
		mlim -= slim;
		do {
			u_char b = *rptr++;
			fcs = PPP_FCS(fcs,b);
			flags >>= 1;
			if (db->tbl[hash] == b) {
				flags |= 0x80;
			} else {
				db->tbl[hash] = b;
				*wptr++ = b;
			}
			hash = PRED_HASH(hash,b);
		} while (--slim != 0);
	}

	/* If compressing made it smaller, then send the compressed buffer.
	 * Otherwise send the original.
	 */
	index = wptr - wptr0;
	if (index < tlen) {
		db->db_i.comp += index;
		*(u_short*)cp_buf = tlen | PRED_COMP_BIT;
	} else {
		db->db_i.comp += dlen;
		db->db_i.incomp++;
		*(u_short*)cp_buf = tlen;
		if (pc) {
			cp_buf[2] = proto;
			(void)m_datacopy(m, 0,tlen-1, (caddr_t)&cp_buf[3]);
			wptr = &cp_buf[tlen-1+3];
		} else {
			cp_buf[2] = proto>>8;
			cp_buf[3] = proto;
			(void)m_datacopy(m, 0,tlen-2, (caddr_t)&cp_buf[4]);
			wptr = &cp_buf[tlen-2+4];
		}
	}

	db->db_i.uncomp += dlen;

	fcs ^= PPP_FCS_INIT;
	*wptr++ = fcs;
	*wptr++ = fcs >> 8;

	return (wptr - cp_buf);
}


/* Decompress Predictor Type 1.
 *	It would be nice to de-compress from stream buffers directly
 *	to mbufs, but we cannot know until we decompress that the
 *	packet contains an IP packet.
 */
mblk_t*
pf_pred1_decomp(struct pred_db *db,
		mblk_t *cmsg)		/* compressed input */
{
	register u_char flags, b;
	register int comp, explen, slen, index;
	register u_char *rptr, *wptr;
	register u_short fcs;
	register int dlen;
	register u_short hash = db->db_i.hash;
	register struct ppp_buf *pb;
	mblk_t *dmsg, *bp;

	rptr = cmsg->b_rptr;
	ASSERT(cmsg->b_wptr >= rptr+PPP_BUF_MIN);
	ASSERT(PPP_BUF_ALIGN(rptr));
	rptr += PPP_BUF_MIN;

	/* get the Predictory Type 1 length and expansion flag */
	comp = 0;
	slen = 2;
	do {
		while (rptr >= cmsg->b_wptr) {
			bp = cmsg;
			cmsg = cmsg->b_cont;
			freeb(bp);
			if (!cmsg) {
				if (db->db_i.debug)
					printf("pred1_decomp%d: missing %d "
					       "bytes of Predictor header\n",
					       db->db_i.unit, slen);
				return 0;
			}
			rptr = cmsg->b_rptr;
		}
		comp = (comp << 8) + *rptr++;
	} while (--slen != 0);
	explen = comp & PRED_LEN_MASK;

	if (explen > db->db_i.mru || explen == 0) {
		/* give up if the length is impossible */
		freemsg(cmsg);
		if (db->db_i.debug)
			printf("pred_1_decomp%d: bad length 0x%x\n",
			       db->db_i.unit, comp);
		return 0;
	}

	dmsg = allocb(explen+PPP_BUF_HEAD_INFO, BPRI_HI);
	if (!dmsg) {
		/* give up if cannot get an uncompressed buffer */
		freemsg(cmsg);
		if (db->db_i.debug)
			printf("pred_1_decomp%d: buffer allocation failed\n",
			       db->db_i.unit);
		return 0;
	}
	wptr = dmsg->b_wptr;
	pb = (struct ppp_buf*)wptr;
	pb->type = BEEP_FRAME;
	/* assume the protocol field is compressed */
	pb->proto = 0;
	wptr += PPP_BUF_HEAD_PROTO+1;

	if (!(comp & PRED_COMP_BIT))
		db->db_i.incomp++;
	db->db_i.uncomp += explen;	/* count expanded length */
	db->db_i.comp += PRED_OVHD-1;	/* and header and FCS */
	dmsg->b_wptr = wptr;

	fcs = PPP_FCS_INIT;
	fcs = PPP_FCS(fcs,explen>>8);
	fcs = PPP_FCS(fcs,explen);
	index = 0;
	do {
		slen = cmsg->b_wptr - rptr;
		if (slen <= 0) {
			bp = cmsg;
			cmsg = cmsg->b_cont;
			freeb(bp);
			if (!cmsg) {
				/* Give up on bad packet length.  There should
				 * have been 2 bytes remaining for the FCS.
				 */
				freemsg(cmsg);
				freemsg(dmsg);
				if (db->db_i.debug)
					printf("pred1_decomp%d: packet too "
					       "short (comp=0x%x)\n",
					       db->db_i.unit, comp);
				return 0;
			}
			rptr = cmsg->b_rptr;
			continue;
		}

		db->db_i.comp += slen;	/* count raw length */

		if (!(comp & PRED_COMP_BIT)) {
			/* update dictionary for incompressible data */
			do {
				b = *rptr++;
				fcs = PPP_FCS(fcs,b);
				*wptr++ = b;
				db->tbl[hash] = b;
				hash = PRED_HASH(hash,b);
				if (--explen == 0)
					goto fin;
			} while (--slen != 0);
			continue;
		}

		if (index == 0) {
			/* be quick about most of the packet
			 */
			while (slen >= 9 && explen >= 8) {
				flags = *rptr++;
				--slen;
				for (index = 8; index != 0; index--) {
					if (flags & 1) {
						b = db->tbl[hash];
					} else {
						--slen;
						b = *rptr++;
						db->tbl[hash] = b;
					}
					fcs = PPP_FCS(fcs,b);
					hash = PRED_HASH(hash,b);
					*wptr++ = b;
					flags >>= 1;
				}
				if ((explen -= 8) == 0)
					goto fin;
			}
		}

		/* Handle last dribble at the end of an input buffer
		 * or the partial block at the start of an input buffer.
		 */
		if (index != 0 || slen < 9 || explen < 8) {
			dlen = MAX(index,8);
			while (dlen != 0) {
				if (index == 0) {
					if (!slen)
						break;
					--slen;
					flags = *rptr++;
					index = 8;
				}
				if (flags & 1) {
					b = db->tbl[hash];
				} else {
					if (!slen)
						break;
					--slen;
					b = *rptr++;
					db->tbl[hash] = b;
				}
				--index;
				flags >>= 1;
				hash = PRED_HASH(hash,b);
				fcs = PPP_FCS(fcs,b);
				*wptr++ = b;
				if (--explen == 0)
					goto fin;
				--dlen;
			}
		}
	} while (explen != 0);
fin:
	db->db_i.hash = hash;

	slen = 2;
	do {
		while (rptr >= cmsg->b_wptr) {
			bp = cmsg;
			cmsg = cmsg->b_cont;
			freeb(bp);
			if (!cmsg) {
				freemsg(dmsg);
				if (db->db_i.debug)
					printf("pred1_decomp%d: missing %d "
					       "bytes of FCS (comp=0x%x)\n",
					       db->db_i.unit, slen, comp);
				return 0;
			}
			rptr = cmsg->b_rptr;
		}
		fcs = PPP_FCS(fcs,*rptr);
		rptr++;
	} while (--slen != 0);
	if (rptr != cmsg->b_wptr || cmsg->b_cont != 0) {
		cmsg->b_rptr = rptr;
		slen = msgdsize(cmsg);
		db->db_i.padding += slen;
		if (slen != 0 && db->db_i.debug)
			printf("pred1_decomp%d: %d bytes of padding "
			       "after 0x%x\n",
			       db->db_i.unit, slen, comp);
	}
	freemsg(cmsg);

	if (fcs != PPP_FCS_GOOD) {
		freemsg(dmsg);
		if (db->db_i.debug)
			printf("pred1_decomp%d: bad FCS (comp=0x%x)\n",
			       db->db_i.unit, comp);
		return 0;
	}

	/* handle uncompressed PPP protocol field */
	if (pb->proto == 0) {
		bcopy(pb->un.info,1+(char*)&pb->proto,
		      wptr - pb->un.info);
		wptr--;
		db->db_i.comp++;
	}
	dmsg->b_wptr = wptr;
	CK_WPTR(dmsg);
	return dmsg;
}


/* PPP "BSD compress" compression
 *  The differences between this compression and the classic BSD LZW
 *  source are obvious from the requirement that the classic code worked
 *  with files while this handles arbitrarily long streams that
 *  are broken into packets.  They are:
 *
 *	When the code size expands, a block of junk is not emitted by
 *	    the compressor and not expected by the decompressor.
 *
 *	New codes are not necessarily assigned every time an old
 *	    code is output by the compressor.  This is because a packet
 *	    end forces a code to be emitted, but does not imply that a
 *	    new sequence has been seen.
 *
 *	The compression ratio is checked at the first end of a packet
 *	    after the appropriate gap.  Besides simplifying and speeding
 *	    things up, this makes it more likely that the transmitter
 *	    and receiver will agree when the dictionary is cleared when
 *	    compression is not going well.
 */

/*
 * the next two codes should not be changed lightly, as they must not
 * lie within the contiguous general code space.
 */
#define	CLEAR	256			/* table clear output code */
#define FIRST	257			/* first free entry */
#define LAST	255

#define BSD_INIT_BITS	MIN_BSD_BITS

#define MAXCODE(b) ((1 << (b)) - 1)
#define BADCODEM1 MAXCODE(MAX_BSD_BITS);

#define BSD_HASH(prefix,suffix,hshift) ((((__uint32_t)(suffix)) << (hshift)) \
					^ (__uint32_t)(prefix))
#define BSD_KEY(prefix,suffix) ((((__uint32_t)(suffix)) << 16)	\
				+ (__uint32_t)(prefix))

#define CHECK_GAP	10000		/* Ratio check interval */

/* clear the dictionary
 */
static void
pf_bsd_clear(struct bsd_db *db)
{
	db->clear_count++;
	db->max_ent = FIRST-1;
	db->n_bits = BSD_INIT_BITS;
	db->ratio = 0;
	db->prev_bytes_out += db->bytes_out;
	db->bytes_out = 0;
	db->prev_in_count += db->in_count;
	db->in_count = 0;
	db->checkpoint = CHECK_GAP;
}


/* If the dictionary is full, then see if it is time to reset it.
 *
 * Compute the compression ratio using fixed-point arithmetic
 * with 8 fractional bits.
 *
 * Since we have an infinite stream instead of a single file,
 * watch only the local compression ratio.
 *
 * Since both peers must reset the dictionary at the same time even in
 * the absence of CLEAR codes (while packets are incompressible), they
 * must compute the same ratio.
 */
static int				/* 1=output CLEAR */
pf_bsd_check(struct bsd_db *db)
{
	register u_int new_ratio;

	if (db->in_count >= db->checkpoint) {
		/* age the ratio by limiting the size of the counts */
		if (db->in_count >= BSD_COMP_RATIO_MAX
		    || db->bytes_out >= BSD_COMP_RATIO_MAX) {
			db->in_count -= db->in_count/4;
			db->bytes_out -= db->bytes_out/4;
		}

		db->checkpoint = db->in_count + CHECK_GAP;

		if (db->max_ent >= db->maxmaxcode) {
			/* Reset the dictionary only if the ratio is worse,
			 * or if it looks as if it has been poisoned
			 * by incompressible data.
			 *
			 * This does not overflow, because
			 *	db->in_count <= BSD_COMP_RATIO_MAX.
			 */
			new_ratio = db->in_count<<BSD_COMP_RATIO_SCALE_LOG;
			if (db->bytes_out != 0)
				new_ratio /= db->bytes_out;

			if (new_ratio < db->ratio
			    || new_ratio < 1*BSD_COMP_RATIO_SCALE) {
				pf_bsd_clear(db);
				return 1;
			}
			db->ratio = new_ratio;
		}
	}
	return 0;
}


/* Initialize the database.
 */
struct bsd_db *
pf_bsd_init(struct bsd_db *db,		/* initialize this database */
	    int unit,			/* for debugging */
	    int bits,			/* size of LZW code word */
	    int mru)			/* MRU for input, 0 for output */
{
	register int i;
	register u_short *lens;
	register u_int newlen, hsize, hshift, maxmaxcode;

	switch (bits) {
	case 9:				/* needs 82152 for both directions */
	case 10:			/* needs 84144 */
	case 11:			/* needs 88240 */
	case 12:			/* needs 96432 */
		hsize = 5003;
		hshift = 4;
		break;
	case 13:			/* needs 176784 */
		hsize = 9001;
		hshift = 5;
		break;
	case 14:			/* needs 353744 */
		hsize = 18013;
		hshift = 6;
		break;
	case 15:			/* needs 691440 */
		hsize = 35023;
		hshift = 7;
		break;
	case 16:			/* needs 1366160--far too much, */
		/* hsize = 69001; */	/* and 69001 is too big for cptr */
		/* hshift = 8; */	/* in struct bsd_db */
		/* break; */
	default:
		if (db) {
			if (db->lens)
				kern_free(db->lens);
			kern_free(db);
		}
		return 0;
	}
	maxmaxcode = MAXCODE(bits);
	newlen = sizeof(*db) + (hsize-1)*(sizeof(db->dict[0]));

	if (db) {
		lens = db->lens;
		if (db->totlen != newlen) {
			if (lens)
				kern_free(lens);
			kern_free(db);
			db = 0;
		}
	}
	if (!db) {
		db = (struct bsd_db*)kern_malloc(newlen);
		if (!db)
			return 0;
		if (mru == 0) {
			lens = 0;
		} else {
			lens = (u_short*)kern_malloc((maxmaxcode+1)
						     * sizeof(*lens));
			if (!lens) {
				kern_free(db);
				return 0;
			}
			i = LAST+1;
			while (i != 0)
				lens[--i] = 1;
		}
		i = hsize;
		while (i != 0) {
			db->dict[--i].codem1 = BADCODEM1;
			db->dict[i].cptr = 0;
		}
	}

	bzero(db,sizeof(*db)-sizeof(db->dict));
	db->lens = lens;
	db->unit = unit;
	db->mru = mru;
	db->hsize = hsize;
	db->hshift = hshift;
	db->maxmaxcode = maxmaxcode;
	db->clear_count = -1;

	pf_bsd_clear(db);

	return db;
}


/* compress a packet
 *	Assume the protocol is known to be >= 0x21 and < 0xff.
 *	One change from the BSD compress command is that when the
 *	code size expands, we do not output a bunch of padding.
 */
int					/* new slen */
pf_bsd_comp(struct bsd_db *db,
	    u_char *cp_buf,		/* compress into here */
	    int proto,			/* this original PPP protocol */
	    struct mbuf *m,		/* from here */
	    int slen)
{
	register int hshift = db->hshift;
	register u_int max_ent = db->max_ent;
	register u_int n_bits = db->n_bits;
	register u_int bitno = 32;
	register __uint32_t accum = 0;
	register struct bsd_dict *dictp;
	register __uint32_t fcode;
	register u_char c;
	register int hval, disp, ent;
	register u_char *rptr, *wptr;
	struct mbuf *n;

#define OUTPUT(ent) {			\
	bitno -= n_bits;		\
	accum |= ((ent) << bitno);	\
	do {				\
		*wptr++ = accum>>24;	\
		accum <<= 8;		\
		bitno += 8;		\
	} while (bitno <= 24);		\
	}


	/* start with the protocol byte */
	ent = proto;
	db->in_count++;

	/* install sequence number */
	cp_buf[0] = db->seqno>>8;
	cp_buf[1] = db->seqno;
	db->seqno++;

	wptr = &cp_buf[2];
	slen = m->m_len;
	db->in_count += slen;
	rptr = mtod(m, u_char*);
	n = m->m_next;
	for (;;) {
		if (slen == 0) {
			if (!n)
				break;
			slen = n->m_len;
			rptr = mtod(n, u_char*);
			n = n->m_next;
			if (!slen)
				continue;   /* handle 0-length buffers */
			db->in_count += slen;
		}

		slen--;
		c = *rptr++;
		fcode = BSD_KEY(ent,c);
		hval = BSD_HASH(ent,c,hshift);
		dictp = &db->dict[hval];

		/* Validate and then check the entry. */
		if (dictp->codem1 >= max_ent)
			goto nomatch;
		if (dictp->f.fcode == fcode) {
			ent = dictp->codem1+1;
			continue;	/* found (prefix,suffix) */
		}

		/* continue probing until a match or invalid entry */
		disp = (hval == 0) ? 1 : hval;
		do {
			hval += disp;
			if (hval >= db->hsize)
				hval -= db->hsize;
			dictp = &db->dict[hval];
			if (dictp->codem1 >= max_ent)
				goto nomatch;
		} while (dictp->f.fcode != fcode);
		ent = dictp->codem1+1;	/* finally found (prefix,suffix) */
		continue;

nomatch:
		OUTPUT(ent);		/* output the prefix */

		/* code -> hashtable */
		if (max_ent < db->maxmaxcode) {
			struct bsd_dict *dictp2;
			/* expand code size if needed */
			if (max_ent >= MAXCODE(n_bits))
				db->n_bits = ++n_bits;

			/* Invalidate old hash table entry using
			 * this code, and then take it over.
			 */
			dictp2 = &db->dict[max_ent+1];
			if (db->dict[dictp2->cptr].codem1 == max_ent)
				db->dict[dictp2->cptr].codem1 = BADCODEM1;
			dictp2->cptr = hval;
			dictp->codem1 = max_ent;
			dictp->f.fcode = fcode;

			db->max_ent = ++max_ent;
		}
		ent = c;
	}
	OUTPUT(ent);			/* output the last code */
	db->bytes_out += (wptr-&cp_buf[2]   /* count complete bytes */
			  + (32-bitno+7)/8);

	if (pf_bsd_check(db))
		OUTPUT(CLEAR);		/* do not count the CLEAR */

	/* Pad dribble bits of last code with ones.
	 * Do not emit a completely useless byte of ones.
	 */
	if (bitno != 32)
		*wptr++ = (accum | (0xff << (bitno-8))) >> 24;

	/* Increase code size if we would have without the packet
	 * boundary and as the decompressor will.
	 */
	if (max_ent >= MAXCODE(n_bits)
	    && max_ent < db->maxmaxcode)
		db->n_bits++;

	return (wptr - cp_buf);
#undef OUTPUT
}


/* Update the "BSD Compress" dictionary on the receiver for
 * incompressible data by pretending to compress the incoming data.
 */
void
pf_bsd_incomp(struct bsd_db *db,
	      mblk_t *dmsg,
	      u_int ent)		/* start with the protocol byte */
{
	register u_int hshift = db->hshift;
	register u_int max_ent = db->max_ent;
	register u_int n_bits = db->n_bits;
	register struct bsd_dict *dictp;
	register __uint32_t fcode;
	register u_char c;
	register int hval, disp;
	register int slen;
	register u_int bitno = 7;
	register u_char *rptr;

	db->incomp_count++;

	db->in_count++;			/* count the protocol as 1 byte */
	db->seqno++;
	rptr = dmsg->b_rptr+PPP_BUF_HEAD_INFO;
	for (;;) {
		slen = dmsg->b_wptr - rptr;
		if (slen == 0) {
			dmsg = dmsg->b_cont;
			if (!dmsg)
				break;
			rptr = dmsg->b_rptr;
			continue;	/* skip zero-length buffers */
		}
		db->in_count += slen;

		do {
			c = *rptr++;
			fcode = BSD_KEY(ent,c);
			hval = BSD_HASH(ent,c,hshift);
			dictp = &db->dict[hval];

			/* validate and then check the entry */
			if (dictp->codem1 >= max_ent)
				goto nomatch;
			if (dictp->f.fcode == fcode) {
				ent = dictp->codem1+1;
				continue;   /* found (prefix,suffix) */
			}

			/* continue probing until a match or invalid entry */
			disp = (hval == 0) ? 1 : hval;
			do {
				hval += disp;
				if (hval >= db->hsize)
					hval -= db->hsize;
				dictp = &db->dict[hval];
				if (dictp->codem1 >= max_ent)
					goto nomatch;
			} while (dictp->f.fcode != fcode);
			ent = dictp->codem1+1;
			continue;	/* finally found (prefix,suffix) */

nomatch:				/* output (count) the prefix */
			bitno += n_bits;

			/* code -> hashtable */
			if (max_ent < db->maxmaxcode) {
				struct bsd_dict *dictp2;
				/* expand code size if needed */
				if (max_ent >= MAXCODE(n_bits))
					db->n_bits = ++n_bits;

				/* Invalidate previous hash table entry
				 * assigned this code, and then take it over.
				 */
				dictp2 = &db->dict[max_ent+1];
				if (db->dict[dictp2->cptr].codem1 == max_ent)
				    db->dict[dictp2->cptr].codem1 = BADCODEM1;
				dictp2->cptr = hval;
				dictp->codem1 = max_ent;
				dictp->f.fcode = fcode;

				db->max_ent = ++max_ent;
				db->lens[max_ent] = db->lens[ent]+1;
			}
			ent = c;
		} while (--slen != 0);
	}
	bitno += n_bits;		/* output (count) the last code */
	db->bytes_out += bitno/8;

	(void)pf_bsd_check(db);

	/* Increase code size if we would have without the packet
	 * boundary and as the decompressor will.
	 */
	if (max_ent >= MAXCODE(n_bits)
	    && max_ent < db->maxmaxcode)
		db->n_bits++;
}


/* Decompress "BSD Compress"
 */
mblk_t*					/* 0=failed, so zap CCP */
pf_bsd_decomp(struct bsd_db *db,
	      mblk_t *cmsg)
{
	register u_int max_ent = db->max_ent;
	register __uint32_t accum = 0;
	register u_int bitno = 32;	/* 1st valid bit in accum */
	register u_int n_bits = db->n_bits;
	register u_int tgtbitno = 32-n_bits;	/* bitno when accum full */
	register struct bsd_dict *dictp;
	register int explen, i;
	register u_int incode, oldcode, finchar;
	register u_char *p, *rptr, *rptr9, *wptr0, *wptr;
	mblk_t *dmsg, *dmsg1, *bp;

	db->decomp_count++;

	rptr = cmsg->b_rptr;
	ASSERT(cmsg->b_wptr >= rptr+PPP_BUF_MIN);
	ASSERT(PPP_BUF_ALIGN(rptr));
	rptr += PPP_BUF_MIN;

	/* get the sequence number */
	i = 0;
	explen = 2;
	do {
		while (rptr >= cmsg->b_wptr) {
			bp = cmsg;
			cmsg = cmsg->b_cont;
			freeb(bp);
			if (!cmsg) {
				if (db->debug)
					printf("bsd_decomp%d: missing"
					       " %d header bytes\n",
					       db->unit, explen);
				return 0;
			}
			rptr = cmsg->b_rptr;
		}
		i = (i << 8) + *rptr++;
	} while (--explen != 0);
	if (i != db->seqno++) {
		freemsg(cmsg);
		if (db->debug)
			printf("bsd_decomp%d: bad sequence number 0x%x"
			       " instead of 0x%x\n",
			       db->unit, i, db->seqno-1);
		return 0;
	}

	/* Guess how much memory we will need.  Assume this packet was
	 * compressed by at least 1.5X regardless of the recent ratio.
	 */
	if (db->ratio > (BSD_COMP_RATIO_SCALE*3)/2)
		explen = (msgdsize(cmsg)*db->ratio)/BSD_COMP_RATIO_SCALE;
	else
		explen = (msgdsize(cmsg)*3)/2;
	if (explen > db->mru)
		explen = db->mru;

	dmsg = dmsg1 = allocb(explen+PPP_BUF_HEAD_INFO, BPRI_HI);
	if (!dmsg1) {
		freemsg(cmsg);
		return 0;
	}
	wptr = dmsg1->b_wptr;

	((struct ppp_buf*)wptr)->type = BEEP_FRAME;
	/* the protocol field must be compressed */
	((struct ppp_buf*)wptr)->proto = 0;
	wptr += PPP_BUF_HEAD_PROTO+1;

	rptr9 = cmsg->b_wptr;
	db->bytes_out += rptr9-rptr;
	wptr0 = wptr;
	explen = dmsg1->b_datap->db_lim - wptr;
	oldcode = CLEAR;
	for (;;) {
		if (rptr >= rptr9) {
			bp = cmsg;
			cmsg = cmsg->b_cont;
			freeb(bp);
			if (!cmsg)	/* quit at end of message */
				break;
			rptr = cmsg->b_rptr;
			rptr9 = cmsg->b_wptr;
			db->bytes_out += rptr9-rptr;
			continue;	/* handle 0-length buffers */
		}

		/* Accumulate bytes until we have a complete code.
		 * Then get the next code, relying on the 32-bit,
		 * unsigned accum to mask the result.
		 */
		bitno -= 8;
		accum |= *rptr++ << bitno;
		if (tgtbitno < bitno)
			continue;
		incode = accum >> tgtbitno;
		accum <<= n_bits;
		bitno += n_bits;

		if (incode == CLEAR) {
			/* The dictionary must only be cleared at
			 * the end of a packet.  But there could be an
			 * empty message block at the end.
			 */
			if (rptr != rptr9
			    || cmsg->b_cont != 0) {
				cmsg->b_rptr = rptr;
				i = msgdsize(cmsg);
				if (i != 0) {
					freemsg(dmsg);
					freemsg(cmsg);
					if (db->debug)
					    printf("bsd_decomp%d: bad CLEAR\n",
						   db->unit);
					return 0;
				}
			}
			pf_bsd_clear(db);
			freemsg(cmsg);
			wptr0 = wptr;
			break;
		}

		/* Special case for KwKwK string. */
		if (incode > max_ent) {
			if (incode > max_ent+2
			    || incode > db->maxmaxcode
			    || oldcode == CLEAR) {
				freemsg(dmsg);
				freemsg(cmsg);
				if (db->debug)
					printf("bsd_decomp%d: bad code %x\n",
					       db->unit, incode);
				return 0;
			}
			i = db->lens[oldcode];
			/* do not write past end of buf */
			explen -= i+1;
			if (explen < 0) {
				db->undershoot -= explen;
				db->in_count += wptr-wptr0;
				dmsg1->b_wptr = wptr;
				CK_WPTR(dmsg1);
				explen = MAX(64,i+1);
				bp = allocb(explen, BPRI_HI);
				if (!bp) {
					freemsg(cmsg);
					freemsg(dmsg);
					return 0;
				}
				dmsg1->b_cont = bp;
				dmsg1 = bp;
				wptr0 = wptr = dmsg1->b_wptr;
				explen = dmsg1->b_datap->db_lim-wptr - (i+1);
			}
			p = (wptr += i);
			*wptr++ = finchar;
			finchar = oldcode;
		} else {
			i = db->lens[finchar = incode];
			explen -= i;
			if (explen < 0) {
				db->undershoot -= explen;
				db->in_count += wptr-wptr0;
				dmsg1->b_wptr = wptr;
				CK_WPTR(dmsg1);
				explen = MAX(64,i);
				bp = allocb(explen, BPRI_HI);
				if (!bp) {
					freemsg(dmsg);
					freemsg(cmsg);
					return 0;
				}
				dmsg1->b_cont = bp;
				dmsg1 = bp;
				wptr0 = wptr = dmsg1->b_wptr;
				explen = dmsg1->b_datap->db_lim-wptr - i;
			}
			p = (wptr += i);
		}

		/* decode code and install in decompressed buffer */
		while (finchar > LAST) {
			dictp = &db->dict[db->dict[finchar].cptr];
			*--p = dictp->f.hs.suffix;
			finchar = dictp->f.hs.prefix;
		}
		*--p = finchar;

		/* If not first code in a packet, and
		 * if not out of code space, then allocate a new code.
		 *
		 * Keep the hash table correct so it can be used
		 * with uncompressed packets.
		 */
		if (oldcode != CLEAR
		    && max_ent < db->maxmaxcode) {
			struct bsd_dict *dictp2;
			__uint32_t fcode;
			int hval, disp;

			fcode = BSD_KEY(oldcode,finchar);
			hval = BSD_HASH(oldcode,finchar,db->hshift);
			dictp = &db->dict[hval];

			/* look for a free hash table entry */
			if (dictp->codem1 < max_ent) {
				disp = (hval == 0) ? 1 : hval;
				do {
					hval += disp;
					if (hval >= db->hsize)
						hval -= db->hsize;
					dictp = &db->dict[hval];
				} while (dictp->codem1 < max_ent);
			}

			/* Invalidate previous hash table entry
			 * assigned this code, and then take it over
			 */
			dictp2 = &db->dict[max_ent+1];
			if (db->dict[dictp2->cptr].codem1 == max_ent) {
				db->dict[dictp2->cptr].codem1 = BADCODEM1;
			}
			dictp2->cptr = hval;
			dictp->codem1 = max_ent;
			dictp->f.fcode = fcode;

			db->max_ent = ++max_ent;
			db->lens[max_ent] = db->lens[oldcode]+1;

			/* Expand code size if needed.
			 */
			if (max_ent >= MAXCODE(n_bits)
			    && max_ent < db->maxmaxcode) {
				db->n_bits = ++n_bits;
				tgtbitno = 32-n_bits;
			}
		}
		oldcode = incode;
	}

	db->in_count += wptr-wptr0;
	dmsg1->b_wptr = wptr;
	CK_WPTR(dmsg1);

	db->overshoot += explen;

	/* Keep the checkpoint right so that incompressible packets
	 * clear the dictionary at the right times.
	 */
	if (pf_bsd_check(db)
	    && db->debug) {
		printf("bsd_decomp%d: peer should have "
		       "cleared dictionary\n", db->unit);
	}

	return dmsg;
}
