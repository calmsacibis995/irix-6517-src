/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.20 $
 */

#include <bstring.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_snmp.h>

#include <smtd_parm.h>
#include <smtd.h>
#include <ma.h>
#include <tlv_get.h>
#include <smtd_fs.h>
#include <smtd_event.h>
#include <smtd_nn.h>


/*
 * NN Pass Actions.
 */
static void
nn_pass(LFDDI_ADDR *sa, SMT_MAC *mac)
{
	u_long tmp;
#ifdef STASH
	sm_log(LOG_DEBUG, 0, "nn_pass: %s", mac->name);
#endif
	mac->dupa_test = DUPA_PASS;
	tmp = mac->dupa_flag & SMT_DUPA_ME;
	mac->dupa_flag &= ~SMT_DUPA_ME;
	if (tmp) {
		sr_event(EVNT_DUPA, EVTYPE_DEASSERT, mac);
		smtd_rmt(mac);		/* tell RMT */
	}

	mac->dv_flag = 1;
	mac->tdv = curtime.tv_sec;
	mac->nn_xid = SMTXID;
	mac->nn_state = NT0;
	if (bcmp(sa, &mac->dna, sizeof(*sa))) {
		/* dna changed */
		if (bcmp(&mac->dna, &smtd_null_mid, sizeof(mac->dna)))
			mac->old_dna = mac->dna;
		mac->dna = *sa;
		sr_event(EVNT_NBRCHG, EVTYPE_EVENT, mac);
		nn_reset(mac);
	}
}


/*
 * NN Fail Actions.
 */
static void
nn_fail(SMT_MAC *mac)
{
#ifdef STASH
	sm_log(LOG_DEBUG, 0, "nn_fail: %s", mac->name);
#endif
	if (bcmp(&mac->dna, &smtd_null_mid, sizeof(mac->dna))) {
		mac->dv_flag = 0;
		mac->old_dna = mac->dna;
		mac->dna = smtd_null_mid;
		sr_event(EVNT_NBRCHG, EVTYPE_EVENT, mac);
	}
	mac->nn_state = NT0;
}

/*
 * NN Send_Actions
 */
static void
nn_send(SMT_MAC *mac)
{
	int sts;

	if (mac->ringop) {
		sts = fs_request(mac, SMT_FC_NIF, &smtd_broad_mid, 0, 0);
		if (sts != RC_SUCCESS)
			sm_log(LOG_ERR, 0, "nt_send: rc=%d", sts);
		else
			mac->nn_state = NT1;
	}

	/*
	 * Pick the next time, but retain the phase of the announcements
	 * so that they do not all happen at once.
	 * Be aggressive when first starting or when the ring is down.
	 */
	while (mac->tnn < curtime.tv_sec+T_NOTIFY_MIN) {
		if (mac->tnn < curtime.tv_sec - smtp->t_notify) {
			mac->tnn = curtime.tv_sec+T_NOTIFY_MIN;
			break;
		}
		if (mac->ringop
		    && (curtime.tv_sec - mac->pcmtime > T_NOTIFY_MAX
			|| mac->dv_flag)) {
			mac->tnn += smtp->t_notify;
		} else {
			mac->tnn += T_NOTIFY_MIN;
		}
	}
}

/*
 * NN Reset_Actions
 */
void
nn_reset(SMT_MAC *mac)
{
#ifdef STASH
	sm_log(LOG_DEBUG, 0, "nn_reset:");
#endif
	mac->dupa_test = DUPA_NONE;
	mac->nn_xid = SMTXID;
	mac->nn_state = NT0;
	nn_send(mac);
}

/*
 *
 */
void
nn_receive(SMT_MAC *mac, SMT_FB *dp)
{
	register struct lmac_hdr *mh;
	register smt_header	 *fp;

	mh = FBTOMH(dp);
	fp = FBTOFP(dp);

#ifdef STASH
	{
	LFDDI_ADDR sa;
	bitswapcopy(&mh->mac_sa, &sa, sizeof(sa));
	sm_log(LOG_DEBUG,0,
		"%s:NIF_RESPOND from %s",mac->name,midtoa((char*)&sa));
	}
#endif
	if (((mh->mac_bits & MAC_E_BIT) != 0) ||
	    (mh->mac_fc != FC_SMTINFO)) {
		/* Ignore garbage */
		return;
	}

	/* Was it addressed to us?  If so, see if some other station
	 * saw it first.
	 */
	if (!bcmp(&mac->addr, &mh->mac_da, sizeof(mh->mac_da))) {
		if (mh->mac_bits & MAC_A_BIT) {
			/* Dup addr response */
			nn_fail(mac);

		} else {
			/* Normal response */
			nn_pass(&mh->mac_sa, mac);
		}
	}
}

/*
 *
 */
void
nn_verify(SMT_MAC *mac)
{
	if ((curtime.tv_sec - mac->tuv > T_NN_OUT) &&
	    (bcmp(&mac->una, &smtd_null_mid, sizeof(mac->una)))) {
		mac->uv_flag = 0;
		mac->old_una = mac->una;
		mac->una = smtd_null_mid;
		sr_event(EVNT_NBRCHG, EVTYPE_EVENT, mac);
		sm_log(LOG_DEBUG, 0,
		       "nn_verify: %s: una timed out", mac->name);
	}
	if ((curtime.tv_sec - mac->tdv > T_NN_OUT) &&
	    (bcmp(&mac->dna, &smtd_null_mid, sizeof(mac->dna)))) {
		mac->dv_flag = 0;
		mac->old_dna = mac->dna;
		mac->dna = smtd_null_mid;
		sr_event(EVNT_NBRCHG, EVTYPE_EVENT, mac);
		sm_log(LOG_DEBUG, 0,
		       "nn_verify: %s: dna timed out", mac->name);
	}

	if ((curtime.tv_sec - mac->tflop > T_NN_OUT) &&
	    (mac->flops < mac->maxflops)) {
		mac->flops++;
		sm_log(LOG_EMERG, 0,
		       "No SMT frames received for %d seconds; reset #%d",
		       curtime.tv_sec - mac->tflop, mac->flops);
		mac->tflop = curtime.tv_sec;
		/* do not keep resetting if we cannot */
		if (sm_reset(mac->name, 1))
			mac->flops = mac->maxflops;
		mac->changed = 1;
	}

}

/*
 *
 */
void
nn_respond(SMT_MAC *mac, SMT_FB *dp, u_long len)
{
	LFDDI_ADDR da;
	register struct lmac_hdr *mh;
	register smt_header	 *fp;
	struct timeval now;

	mh = FBTOMH(dp);
	fp = FBTOFP(dp);

	/*
	 * See if this frame is from my una.
	 */
	if (((mh->mac_bits & MAC_A_BIT) == 0) &&
	    ((mh->mac_bits & MAC_C_BIT) == 0) &&
	    ((mh->mac_bits & MAC_E_BIT) == 0) &&
	    (mh->mac_fc == FC_NSA) &&
	    (!bcmp(&mh->mac_da, &smtd_broad_mid, sizeof(mh->mac_da)))) {
		LFDDI_ADDR una;
		PARM_STD std;
		PARM_STAT st;
		char *info = FBTOINFO(dp);
		u_long infolen = fp->len;

		smt_gts(&now);
		mac->tuv = now.tv_sec;
		mac->uv_flag = 1;
		if (bcmp(&mh->mac_sa, &mac->una, sizeof(mac->una))) {
			/* una changed */
			if (bcmp(&mac->una, &smtd_null_mid, sizeof(mac->una)))
				mac->old_una = mac->una;
			mac->una = mh->mac_sa;
			sr_event(EVNT_NBRCHG, EVTYPE_EVENT, mac);
			nn_reset(mac);
		}

		if (tlv_parse_una(&info, &infolen, &una) == RC_SUCCESS &&
		    tlv_parse_std(&info, &infolen, &std) == RC_SUCCESS &&
		    tlv_parse_stat(&info, &infolen, &st) == RC_SUCCESS) {
			register int t1, unda;

			if ((t1 = st.dupa&SMT_DUPA_ME) != 0)
				mac->dupa_flag |= SMT_DUPA_UNA;
			else
				mac->dupa_flag &= ~SMT_DUPA_UNA;

			unda = mac->unda_flag;
			if (t1 && !unda) {	/* assert */
				mac->unda_flag = 1;
				sr_event(EVNT_DUPA, EVTYPE_ASSERT, mac);
			} else if (!t1 && unda) { /* deassert */
				mac->unda_flag = 0;
				sr_event(EVNT_DUPA,EVTYPE_DEASSERT,mac);
			}
		}
	} else if (!(((mh->mac_bits & MAC_E_BIT) == 0) &&
		   ((mh->mac_fc == FC_SMTINFO) ||
		   (!bcmp(&mh->mac_da, &mac->addr, sizeof(mh->mac_da)))))) {
		return;
	}

	da = mh->mac_sa;
	fs_respond(mac,SMT_FC_NIF,&da,dp,len,RC_SUCCESS);
}

/***********************************************************************/
void
nn_expire(SMT_MAC *mac)
{
	if (curtime.tv_sec >= mac->tnn)
		nn_send(mac);
}
