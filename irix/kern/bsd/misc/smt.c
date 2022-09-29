/* common FDDI SMT functions
 *
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.61 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/systm.h"
#include "bstring.h"

#include "ether.h"
#include "sys/fddi.h"
#include "sys/smt.h"
#include "sys/mbuf.h"
#include "sys/capability.h"


#ifndef IFNET_LOCK
#define IFNET_LOCK(ifp,s) (s = splimp());
#define IFNET_UNLOCK(ifp,s) splx(s);
#define IFNET_ISLOCKED(ifp) 1
#endif


char *smt_pc_label[] = {
	"pcA:",
	"pcB:",
	"pcS:",
	"pcM:"
	"pc?:",
};


int smt_debug_lvl = 2;

int smt_max_connects = 32;		/* give up if cannot join by this */


int					/* decimal log scaled by 1000 */
slog(__uint32_t x)
{
	static int tbl[16] = {
		0x7fffffff,		/* 0 */
		0,			/* 1 */
		301,			/* 2 */
		477,			/* 3 */
		602,			/* 4 */
		699,			/* 5 */
		778,			/* 6 */
		845,			/* 7 */
		903,			/* 8 */
		954,			/* 9 */
		1000,			/* 10 */
		1042,			/* 11 */
		1079,			/* 12 */
		1114,			/* 13 */
		1146,			/* 14 */
		1176			/* 15 */
	};
	int c;

	c = 0;
	if (x >= (15<<16))
		c += 4816, x >>= 16;
	if (x >= (15<<8))
	       c += 2408, x >>= 8;
	if (x >= (15<<4))
	       c += 1204, x >>= 4;
	while (x > 15)
	       c += 301, x >>= 1;
	c += tbl[x];

	return c;
}


/* compute LER given a number of errors and a duration
 */
uint
smt_ler(struct timeval *lem_time,
	__uint32_t ct)
{
#	define MAX_LER 16		/* LER cannot be bigger than this */
	int lg;

#ifdef DEBUG
	static int smt_ler_debug;

	if (smt_ler_debug != 0)
		return smt_ler_debug;
#endif

	/* compute decimal log(usec*125/count).
	 */
	if (ct == 0)
		return MAX_LER;

	if (lem_time->tv_sec >= 100) {
		lg = slog(lem_time->tv_sec*125)+6000;
	} else {
		lg = slog((lem_time->tv_usec
			   + lem_time->tv_sec*USEC_PER_SEC)*125);
	}
	lg -= slog(ct);

	if (lg < 0)
		return 0;
	lg = (lg+500)/1000;		/* remove scaling */
	if (lg >= MAX_LER)
		return MAX_LER;
	return lg;
}



/* forget any pending timer
 */
void
smt_clear_timeout(struct smt *smt)
{
	smt->smt_tid++;
	if (PCM_SET(smt,PCM_TIMER)) {
		untimeout(smt->smt_ktid);
		smt->smt_st.flags &= ~PCM_TIMER;
	}
}


/* handle PCM timers
 */
void
smt_timo(struct smt *smt,
	 int tid)
{
	IFNET_LOCK((struct ifnet *)smt->smt_pdata);
	if (tid == smt->smt_tid
	    && PCM_SET(smt,PCM_TIMER)) {
		smt->smt_st.flags &= ~PCM_TIMER;
		smt->smt_ops->pcm(smt);
	} else {
		SMTDEBUGPRINT(smt,0,
			      ("%s stray tid=%d smt_tid=%d smt_ktid=0x%x\n",
			       PC_LAB(smt),
			       tid, smt->smt_tid, smt->smt_ktid));
	}
	IFNET_UNLOCK((struct ifnet *)smt->smt_pdata);
}


/* start a PCM timer
 */
void
smt_timeout(struct smt *smt,
	    int hzs)			/* HZ to delay */
{
	smt_clear_timeout(smt);
	smt->smt_ktid = timeout(smt_timo, (caddr_t)smt, hzs+1,
				smt->smt_tid);
	smt->smt_st.flags |= PCM_TIMER;
}


/* ensure a that timer is running
 *	This happens if the kernel callout happens too soon
 */
void
smt_ck_timeout(struct smt *smt,
	       int usec)		/* delay this much longer */
{
	static int smt_fake_timer;	/* times this swill was needed */

	if (!PCM_SET(smt,PCM_TIMER)) {
		if (usec < 0)
			usec = 0;
		smt_fake_timer++;
		SMT_USEC_TIMER(smt, usec);
	}
}


/* restart things
 */
void
smt_off(struct smt *smt)
{
	smt_clear_timeout(smt);
	TPC_RESET(smt);
	smt->smt_ops->off(smt, smt->smt_pdata);
}


/* clear SMT counters, flags, and so on for a driver
 */
void
smt_clear(struct smt *smt)
{
	smt_clear_timeout(smt);

	bzero(&smt->smt_st.log[0], sizeof(smt->smt_st.log));
	smt->smt_st.log_ptr = 0;
	smt->smt_st.connects = 0;
	smt->smt_st.lct_tot_fail = 0;
	smt->smt_st.flags &= PCM_SAVE_FLAGS;
	smt->smt_st.rngbroke.lo = smt->smt_st.rngop.lo;
	smt->smt_st.pcm_state = PCM_ST_OFF;
	smt->smt_st.npc_type = PC_UNKNOWN;
}


/* finish tracing
 */
void
smt_fin_trace(struct smt *smt,
	      struct smt *osmt)
{
	smt->smt_ops->alarm(smt, smt->smt_pdata, SMT_ALARM_TFIN);

	smt->smt_st.flags &= ~PCM_PC_DISABLE;
	smt->smt_st.flags |= PCM_SELF_TEST;
	osmt->smt_st.flags &= ~PCM_PC_DISABLE;
	osmt->smt_st.flags |= PCM_SELF_TEST;
	smt_off(osmt);
	smt_off(smt);
}


/* set and log a new PCM state
 */
void
smt_new_st(struct smt *smt,
	   enum pcm_state st)
{
	enum rt_ls log_ls;

	if (st == smt->smt_st.pcm_state)
		return;

	switch (st) {
	case PCM_ST_OFF:
		log_ls = T_PCM_OFF;
		break;
	case PCM_ST_BREAK:
		log_ls = T_PCM_BREAK;
		SMTDEBUGPRINT(smt,0, ("%sBREAK\n", PC_LAB(smt)));
		break;
	case PCM_ST_TRACE:
		log_ls = T_PCM_TRACE;
		break;
	case PCM_ST_CONNECT:
		log_ls = T_PCM_CONNECT;
		break;
	case PCM_ST_NEXT:
		log_ls = T_PCM_NEXT;
		break;
	case PCM_ST_SIGNAL:
		log_ls = T_PCM_SIGNAL;
		break;
	case PCM_ST_JOIN:
		log_ls = T_PCM_JOIN;
		break;
	case PCM_ST_VERIFY:
		log_ls = T_PCM_VERIFY;
		break;
	case PCM_ST_ACTIVE:
		log_ls = T_PCM_ACTIVE;
		break;
	case PCM_ST_MAINT:
		log_ls = T_PCM_MAINT;
		break;
	case PCM_ST_BYPASS:
		log_ls = T_PCM_BYPASS;
		break;
#ifdef debug
	default:
		panic("unknown PCM state");
#endif
	}

	if ((st != PCM_ST_NEXT && st != PCM_ST_SIGNAL)
	    || (smt->smt_st.pcm_state != PCM_ST_NEXT
		&& smt->smt_st.pcm_state != PCM_ST_SIGNAL))
		SMT_LSLOG(smt,log_ls);
	smt->smt_st.pcm_state = st;
}


/* process an SMT socket ioctl
 */
int					/* return errno */
smt_ioctl(struct ifnet *ifp,
	  struct smt *smt1,
	  struct smt *smt2,
	  int cmd,
	  struct smt_sioc *sioc)
{
	struct smt *smt;
	struct smt_conf conf;
	struct d_bec_frame d_bec;
	int len;
	int error = 0;
	ulong flags;
	int s;

	ASSERT(IFNET_ISLOCKED(ifp));
	smt = (sioc->phy == 0 ? smt1 : smt2);

	switch (cmd) {
	case SIOC_SMTGET_CONF:
		len = MIN(sizeof(struct smt_conf), sioc->len);
		s = splimp();
		error = smt->smt_ops->st_get(smt, smt->smt_pdata);
		splx(s);
		if (error != 0)
			return error;
		IFNET_UNLOCK(ifp);
		if (copyout((caddr_t)&smt->smt_conf,
			    (caddr_t)(__psunsigned_t)sioc->smt_ptr_u.u32ptr,
			    len))
			error = EFAULT;
		IFNET_LOCK(ifp);
		break;


	case SIOC_SMTSET_CONF:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		len = MIN(sizeof(struct smt_conf), sioc->len);
		IFNET_UNLOCK(ifp);
		if (copyin((caddr_t)(__psunsigned_t)sioc->smt_ptr_u.u32ptr,
			   &conf, len))
			error = EFAULT;
		IFNET_LOCK(ifp);
		if (error == 0)
			error = smt->smt_ops->conf_set(smt, smt->smt_pdata,
						       &conf);
		break;


	case SIOC_SMTGET_ST:
		len = MIN(sizeof(struct smt_st), sioc->len);
		s = splimp();
		error = smt->smt_ops->st_get(smt, smt->smt_pdata);
		splx(s);
		if (error != 0)
			return error;
		IFNET_UNLOCK(ifp);
		if (copyout((caddr_t)&smt->smt_st,
			    (caddr_t)(__psunsigned_t)sioc->smt_ptr_u.u32ptr,
			    len))
			error = EFAULT;
		IFNET_LOCK(ifp);
		break;


	case SIOC_SMT_TRACE:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		s = splimp();
		if (iff_dead(ifp->if_flags)) {
			splx(s);
			return EINVAL;
		}

		SMTDEBUGPRINT(smt1,0,("trace request\n"));

		smt2 = smt1->smt_ops->osmt(smt1,smt1->smt_pdata);

		/* ignore it if already tracing.
		 */
		if (smt1->smt_st.pcm_state == PCM_ST_TRACE
		    || PCM_SET(smt, PCM_SELF_TEST)
		    || smt2->smt_st.pcm_state == PCM_ST_TRACE
		    || PCM_SET(smt, PCM_SELF_TEST)) {
			splx(s);
			return 0;
		}

		/* A dual-MAC (DM_DAS) system looks more like a single-attach
		 *	(SAS) system.  Each interface is associated with
		 *	a single struct smt.
		 * If we are THRU and SM_DAS, then send MLS upstream and so on
		 *	the other PHY.  This phy just waits patiently without
		 *	a timer.
		 * If we are in WRAP, upstream is the same as downstream.
		 */
		if (smt1->smt_conf.type == SAS) {
			smt = smt1;

		} else if (PCM_SET(smt1, PCM_THRU_FLAG)) {
			smt1->smt_st.flags |= PCM_TRACE_OTR;
			smt = smt2;

		} else if (PCM_SET(smt1, PCM_CF_JOIN)) {
			smt_off(smt2);
			smt = smt1;
		} else if (PCM_SET(smt2, PCM_CF_JOIN)) {
			smt_off(smt1);
			smt = smt2;
		} else {
			smt_fin_trace(smt1,smt2);
			splx(s);
			return error;
		}

		smt->smt_ops->trace_req(smt,smt->smt_pdata);
		splx(s);
		return error;


	case SIOC_SMT_ARM:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		s = splimp();
		if (0 == smt->smt_alarm_mbuf) {
			smt->smt_alarm_mbuf = m_get(M_WAIT, MT_DATA);
			if (0 == smt->smt_alarm_mbuf) {
				splx(s);
				return ENOBUFS;
			}
		}
		sioc->smt_ptr_u.alarm = smt->smt_st.alarm;
		smt->smt_st.alarm = 0;
		splx(s);
		break;

	case SIOC_SMT_MAINT:
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		s = splimp();
		if (iff_dead(ifp->if_flags)) {
			splx(s);
			return EINVAL;
		}
		flags = smt->smt_st.flags;

		/* ALS says to go back to what we were doing before.
		 */
		if (sioc->smt_ptr_u.ls == PCM_ALS) {
			smt->smt_st.flags &= ~PCM_PC_DISABLE;
			smt->smt_st.connects = 0;
		} else {
			smt_clear_timeout(smt);
			smt->smt_st.flags &= PCM_SAVE_FLAGS;
			smt->smt_st.flags |= PCM_PC_DISABLE;
			smt_new_st(smt, PCM_ST_MAINT);
			smt->smt_ops->setls(smt, smt->smt_pdata,
					    (enum rt_ls)sioc->smt_ptr_u.ls);
		}
		/* WRAP or unWRAP the other PHY
		 */
		if (0 != ((flags ^ smt->smt_st.flags) & PCM_PC_DISABLE))
			smt_off(smt);
		splx(s);
		break;

	case SIOC_SMT_LEM_FAIL:
		/* used by SMT deamon when the LER gets bad */
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		s = splimp();
		if (iff_dead(ifp->if_flags)) {
			splx(s);
			return EINVAL;
		}
		smt->smt_st.flags |= (PCM_NE | PCM_LEM_FAIL);
		smt->smt_ops->pcm(smt);
		splx(s);
		break;

	case SIOC_SMT_DIRECT_BEC:
		/* start directed beacon.
		 *	This must be followed by a TRACE request after a
		 *	suitable delay.  The normal beacon will be restored
		 *	by the driver the next time the MAC is inserted
		 *	into the ring after CMT signalling.
		 */
		if (!_CAP_ABLE(CAP_NETWORK_MGT))
			return EPERM;
		len = MIN(sioc->len, D_BEC_SIZE);
		if (len != 0) {
			IFNET_UNLOCK(ifp);
			if (copyin((caddr_t)(__psunsigned_t
					     )sioc->smt_ptr_u.u32ptr,
				   &d_bec, len))
				error = EFAULT;
			IFNET_LOCK(ifp);
		}
		if (error != 0)
			break;
		s = splimp();
		if (iff_dead(ifp->if_flags))
			error =  EINVAL;
		else
			smt1->smt_ops->d_bec(smt1, smt1->smt_pdata,
					     &d_bec,len);
		splx(s);
		break;

	default:
		error = EINVAL;
	}

	return error;
}
