/* SMT Ring Management state machine
 *
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.16 $
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <rpc/rpc.h>
#include <netdb.h>

#include <sm_types.h>
#include <sm_log.h>
#include <sm_mib.h>
#include <sm_addr.h>
#include <smt_snmp.h>
#include <smt_snmp_api.h>

#include <smtd.h>
#include <smtd_svc.h>
#include <smtd_snmp.h>
#include <smtd_conf.h>
#include <smtd_parm.h>
#include <smtd_event.h>
#include <smtd_rmt.h>
#include <ma.h>

#undef DBG_RMT
#if defined(DBG_RMT) || defined(lint)
#define DBGRMT(s) sm_log(LOG_DEBUG, 0, s)
#else
#define DBGRMT(s)
#endif

/* stop RMT timer */
#define DISABLE_TRM() (mac->trm_ck = SMT_NEVER)

/* (re)start RMT timer */
#define RESET_TRM(v) (mac->trm_ck = mac->trm_base = mac->sampletime.tv_sec)


/* see if the RMT timer has exceded a limit */
#define CK_TRM(v) (mac->trm_base+(v) <= mac->sampletime.tv_sec)


/* These are the "entry actions", which "are performed on
 * every transition inot a state".  (See RMT footnotes in SMT 7.2.)
 */

static void
rmt_entry_isolated(SMT_MAC* mac,
		   char* msg)
{
	mac->rmtstate = RM_ISOLATED;
	/* reset other RMT flags */
	mac->rmt_flag = RMF_NO_FLAG;

	if (!(smtp->conf_cap&SMT_HOLDAVAIL))
		mac->rmt_flag |= RMF_RE_FLAG;
	if (mac->path.tracestatus == PT_INIT
	    || mac->path.tracestatus == PT_PROP)
		mac->path.tracestatus = PT_TERM;
	smtp->ecmstate = EC_OUT;

	DISABLE_TRM();
	DBGRMT(msg);
}

static void
rmt_entry_nonop(SMT_MAC* mac,
		char* msg)
{
	mac->rmtstate = RM_NONOP;
	mac->rmt_flag &= ~(RMF_LOOP_AVAIL|RMF_MAC_AVAIL);
	mac->path.tracestatus = PT_NOTRACE;
	smtp->ecmstate = EC_IN;
	RESET_TRM(T_NON_OP);
	DBGRMT(msg);
}


static void
rmt_entry_ringop(SMT_MAC* mac,
		 char* msg)
{
	mac->rmtstate = RM_RINGOP;
	mac->rmt_flag &= ~RMF_NO_FLAG;
	if (mac->rmt_flag & RMF_LOOP)
		mac->rmt_flag |= RMF_LOOP_AVAIL;
	if (mac->rmt_flag & RMF_JOIN)
		mac->rmt_flag |= RMF_MAC_AVAIL;
	DISABLE_TRM();
	DBGRMT(msg);
}


static void
rmt_entry_detect(SMT_MAC* mac,
		 char* msg)
{
	mac->rmtstate = RM_DETECT;
	RESET_TRM(T_STUCK);
	DBGRMT(msg);
}


static void
rmt_entry_ringop_dup(SMT_MAC* mac,
		     char* msg)
{
	char str[128];

	mac->rmtstate = RM_RINGOP_DUP;
	sm_log(LOG_EMERG, 0,
	       "Someone is using my MAC address(%s), but ring is up",
	       fddi_ntoa(&mac->addr, str));
	DISABLE_TRM();
	DBGRMT(msg);
}


static void
rmt_entry_nonop_dup(SMT_MAC* mac,
		    char* msg)
{
	mac->rmtstate = RM_NONOP_DUP;
	RESET_TRM(MIN(T_STUCK,T_ANNOUNCE));
	DBGRMT(msg);
}


/* kill things when the ring dies because we have a duplicate address
 */
static void
dup_act(SMT_MAC* mac,
	char* msg)
{
	char str[128];

	/* If the ring is dead,
	 * we take the bad interface off for awhile.
	 */
	if (mac->flops < MAX_DUPSULK/DUPSULK) {
		mac->flops++;
		sm_log(LOG_EMERG, 0, "Someone is using my MAC address(%s)",
		       fddi_ntoa(&mac->addr, str));
	}
	(void)sm_reset(mac->name, 0);
	mac->changed = 1;
	mac->rmt_flag |= RMF_DUP;
	rmt_entry_nonop_dup(mac,msg);
	RESET_TRM(mac->flops*DUPSULK);
}

static void
rmt_entry_directed(SMT_MAC* mac,
		   char* msg)
{
	int len;
	struct d_bec_frame frame;

	mac->rmtstate = RM_DIRECTED;

	bzero(&frame,sizeof(frame));
	/* take a good guess about the bad upstream neighbor */
	if (bcmp(&mac->una, &smtd_null_mid,
		 sizeof(mac->una))) {
		bcopy(&mac->una, frame.una, sizeof(frame.una));
		len = D_BEC_SIZE;
	} else if (bcmp(&mac->old_una, &smtd_null_mid,
			sizeof(mac->old_una))) {
		bcopy(&mac->old_una, frame.una, sizeof(frame.una));
		len = D_BEC_SIZE;
	} else {
		len = D_BEC_SIZE-sizeof(frame.una);
	}
	(void)fddi_aton(SMT_DIRECTED_BEACON_DA, &frame.hdr.mac_da);
	mac->changed = 1;
	sm_d_bec(mac->name, &frame,len, mac->smtsock);

	RESET_TRM(T_DIRECT);
	DBGRMT(msg);
}


static void
rmt_entry_trace(SMT_MAC* mac,
		char* msg)
{
	mac->rmtstate = RM_TRACE;
	smtp->ecmstate = EC_TRACE;
	mac->path.tracestatus = PT_INIT;

	mac->changed = 1;
	if (sm_trace(mac->name,0) != SNMP_ERR_NOERROR)
		sm_log(LOG_DEBUG, 0, "%s sm_trace failed", mac->name);

	DISABLE_TRM();
	DBGRMT(msg);
}



/* These are bodies of the various states.
 *	These functions are called when the heartbeat timer has expired
 *	or when something else interesting may have happened.
 *	If nothing has really happened, these functions must do nothing,
 *	which is why they are separate from the "entry action" functions.
 */

/*
 * RM0: ISOLATED
 */
static int
rmt_isolated(SMT_MAC *mac)
{
	DBGRMT("ISOLATED");

	/* RM(01) - ADD MAC */
	if (mac->rmt_flag & (RMF_JOIN|RMF_LOOP)) {
		rmt_entry_nonop(mac,"RM01");
		return(1);
	}
	return(0);
}

/*
 * RM1: NON_OP
 */
static int
rmt_nonop(SMT_MAC *mac)
{
	DBGRMT("NONOP");

	/* RM11: Optional Hold Policy - Beaconning */
	if (smtp->conf_cap & SMT_HOLDAVAIL &&
	    !(mac->rmt_flag&RMF_NO_FLAG) &&
	    (mac->rmt_flag&(RMF_MYBCN|RMF_OTRBCN) ||
	     (mac->rmt_flag&RMF_TRT_EXP && mac->rmt_flag&(RMF_T4|RMF_T5)))) {
		mac->rmt_flag |= RMF_NO_FLAG;
		rmt_entry_nonop(mac,"RM11a");
		return(1);
	}

	/* RM12: Ring Op */
	if (mac->ringop) {
		rmt_entry_ringop(mac,"RM12");
		return(1);
	}

	/* RM13: Ring not recovering after 1sec since nonop detected */
	if (CK_TRM(T_NON_OP)) {
		mac->rmt_flag &= ~RMF_BN_FLAG;
		mac->rmt_flag |= RMF_NO_FLAG;
		rmt_entry_detect(mac,"RM13");
		return(1);
	}

	return(0);
}

/*
 * RM2: RING_OP
 */
static int
rmt_ringop(SMT_MAC *mac)
{
	/* RM21a: not Ring op */
	if (!mac->ringop) {
		rmt_entry_nonop(mac,"RM21a");
		return(1);
	}

	/* SGI does not do Restricted Token Monitoring */
	/*	RM21b: TRM > T_Rmode && R_flag */
	/*	RM22: Enter_Rmode */

	/* RM25: DUPA test fail */
	if (mac->dupa_test == DUPA_FAIL) {
		mac->rmt_flag &= ~(RMF_LOOP_AVAIL|RMF_MAC_AVAIL);
		mac->da_flag = 1;
		rmt_entry_ringop_dup(mac,"RM25");
	}

	return(0);
}

/*
 * RM3: DETECT
 */
static int
rmt_detect(SMT_MAC *mac)
{
	DBGRMT("DETECT");

	/* RM32: ringop */
	if (mac->ringop) {
		rmt_entry_ringop(mac,"RM32");
		return(1);
	}

	/* RM33a: BN_FLAG & beacon received. */
	if (mac->rmt_flag&RMF_BN_FLAG &&
	    mac->rmt_flag&(RMF_MYBCN|RMF_OTRBCN)) {
		mac->rmt_flag &= ~RMF_BN_FLAG;
		rmt_entry_detect(mac,"RM33a");
		return(1);
	}
	/* RM33b: */
	if (!(mac->rmt_flag&RMF_BN_FLAG)
	    && (mac->rmt_flag&RMF_TRT_EXP) && (mac->rmt_flag&(RMF_T4|RMF_T5))
	    && !(mac->rmt_flag&(RMF_MYBCN|RMF_OTRBCN))) {
		mac->rmt_flag |= RMF_BN_FLAG;
		rmt_entry_detect(mac,"RM33b");
		return(1);
	}
	/* RM34a: Receiving My_claim & time since last claim > 2D_max
	 * RM34b: Receiving My_beacon & time since last beacon > 2D_max
	 * RM34c: t_bid != t_req
	 */
	if (mac->rmt_flag&RMF_DUP_DET) {
		mac->da_flag = 1;
		mac->rmt_flag &= ~(RMF_BN_FLAG|RMF_JM_FLAG);
		dup_act(mac,"RM34");
		return(1);
	}

	/* RM36: stuck beaconing */
	if (CK_TRM(T_STUCK)
	    && (mac->rmt_flag & RMF_JOIN) &&
	    (mac->rmt_flag & RMF_BN_FLAG)) {
		rmt_entry_directed(mac,"RM36");
		return(1);
	}

	return(0);
}

/*
 * RM4: NON_OP_DUP
 */
static int
rmt_nonop_dup(SMT_MAC *mac)
{
	DBGRMT("NONOP_DUP");

	/* RM41: new uniq addr--we do not change addresses */

	/* RM45: ring op */
	if (mac->ringop) {
		rmt_entry_ringop_dup(mac,"RM45");
		return(1);
	}

	/* RM44a: Not beaconning */
	if ((mac->rmt_flag & RMF_BN_FLAG) &&
	    (mac->rmt_flag & (RMF_MYBCN|RMF_OTRBCN))) {
		mac->rmt_flag &= ~RMF_BN_FLAG;
		rmt_entry_nonop_dup(mac,"RM44a");
		return(1);
	}

	/* RM44b: Beaconning */
	if (((mac->rmt_flag&RMF_BN_FLAG) == 0) &&
	    ((mac->rmt_flag&RMF_TRT_EXP) && (mac->rmt_flag&(RMF_T4|RMF_T5)))) {
		mac->rmt_flag |= RMF_BN_FLAG;
		rmt_entry_nonop_dup(mac,"RM44b");
		return(1);
	}

	/* RM44c: Duplicate Action */
	if (!(mac->rmt_flag & (RMF_BN_FLAG | RMF_DUP)) &&
	    CK_TRM(T_ANNOUNCE)) {
		mac->rmt_flag |= RMF_NO_FLAG;
		dup_act(mac,"RM44c");
		return(0);
	}

	/* RM46: Stuck Beacon */
	if ((mac->rmt_flag & RMF_JOIN) &&
	    (mac->rmt_flag & RMF_BN_FLAG) &&
	    CK_TRM(T_STUCK)) {
		rmt_entry_directed(mac,"RM46");
		return(1);
	}

	/* stay here for a while.  Pick a time that is not close
	 * to any of the standard timers to avoid confusing
	 * system administrators.
	 */
	if ((mac->rmt_flag&RMF_DUP) && CK_TRM(mac->flops*DUPSULK)) {
		char str[128];

		mac->da_flag = 0;
		rmt_entry_isolated(mac, "try again");
		sm_log(LOG_EMERG, 0,
		       "Checking to see if duplicate MAC address fixed",
		       fddi_ntoa(&mac->addr, str));
		(void)sm_reset(mac->name, 1);
		mac->changed = 1;
		nn_reset(mac);
		rmt_entry_isolated(mac,"RM40-retry");
		return(1);
	}

	return(0);
}

/*
 * RM5: RING_OP_DUP
 */
static int
rmt_ringop_dup(SMT_MAC *mac)
{
	DBGRMT("RINGOP_DUP");

	/* RM52: DUPA test passed */
	if (mac->dupa_test == DUPA_PASS) {
		mac->da_flag = 0;
		rmt_entry_ringop(mac,"RM52");
		return(1);
	}

	/* RM54a: not Ring OP */
	if (!mac->ringop) {
		mac->rmt_flag &= ~(RMF_JM_FLAG|RMF_BN_FLAG);
		dup_act(mac,"RM54a");
		return(1);
	}

	/* SGI does not do Restricted Token Monitoring */
	/* RM54b: TRM>T_Rmode & R_FLAG */
	/* RM55: Enter Mode */

	return(0);
}

/*
 * RM6: DIRECTED
 */
static int
rmt_directed(SMT_MAC *mac)
{
	DBGRMT("DIRECTED");

	/* RM63: Beacon and not DUP */
	if (!mac->da_flag && (mac->rmt_flag&(RMF_MYBCN|RMF_OTRBCN))) {
		mac->rmt_flag &= ~RMF_BN_FLAG;
		/* turn off directed beacon */
		mac->changed = 1;
		sm_d_bec(mac->name, 0,0, mac->smtsock);
		rmt_entry_detect(mac,"RM63");
		return(1);
	}

	/* RM64: Beacon and DUP */
	if (mac->da_flag && (mac->rmt_flag&(RMF_MYBCN|RMF_OTRBCN))) {
		mac->rmt_flag &= ~RMF_BN_FLAG;
		/* turn off directed beacon */
		mac->changed = 1;
		sm_d_bec(mac->name, 0,0, mac->smtsock);
		dup_act(mac,"RM64");
		return(1);
	}

	/* RM67: Directed completed */
	if (CK_TRM(T_DIRECT)
	    && (mac->rmt_flag&RMF_RE_FLAG)) {
		rmt_entry_trace(mac,"RM67");
		return(1);
	}

	return(0);
}

/*
 * RM7: TRACE
 */
/* ARGSUSED */
static int
rmt_trace(SMT_MAC *mac)
{
	DBGRMT("TRACE");

	/* stay here until the hardware says it is finished */
	return(0);
}

static int (*rmttbl[])() = {
	rmt_isolated,
	rmt_nonop,
	rmt_ringop,
	rmt_detect,
	rmt_nonop_dup,
	rmt_ringop_dup,
	rmt_directed,
	rmt_trace,
};

void
smtd_rmt(SMT_MAC *mac)
{
	/* do nothing if RMT turned off */
	if (!smtp->trace_on) {
		DISABLE_TRM();
		return;
	}

	if (((mac->primary
	      && mac->primary->pcm_state == PCM_ST_TRACE)
	     || (mac->secondary
		 && mac->secondary->pcm_state == PCM_ST_TRACE))
	    && mac->rmtstate != RM_TRACE) {
		mac->rmtstate = RM_TRACE;
		smtp->ecmstate = EC_TRACE;
		mac->path.tracestatus = PT_PROP;
	}

	if (!(mac->rmt_flag & (RMF_JOIN|RMF_LOOP | RMF_DUP))
	    && mac->rmtstate != RM_ISOLATED)
		rmt_entry_isolated(mac,"RMx0");

	for (;;) {
		/* run RMT until it says to stop */
		if (!(*rmttbl[mac->rmtstate])(mac))
			break;
	}

	if (mac->trm_ck != SMT_NEVER)
		mac->trm_ck = mac->sampletime.tv_sec+1;
}

void
rmt_reset(SMT_MAC *mac)
{
	rmt_entry_isolated(mac,"RMreset");
}

/* duplicate address detected
 */
void
dup_det(SMT_MAC *mac)
{
	u_long tmp;

	mac->dupa_test = DUPA_FAIL;
	tmp = mac->dupa_flag & SMT_DUPA_ME;
	mac->dupa_flag |= SMT_DUPA_ME;
	if (!tmp) {
		sr_event(EVNT_DUPA, EVTYPE_ASSERT, mac);
		smtd_rmt(mac);
	}
}
