/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.21 $
 */

#include <signal.h>
#include <bstring.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_snmp.h>

#include <smtd_parm.h>
#include <smtd.h>
#include <tlv_get.h>
#include <smtd_fs.h>
#include <smtd_event.h>

#define SR_MAXTIMO	(1 << smtp->reportlimit)
#define backoff(t)      (((1 << (t)) >= SR_MAXTIMO) ? SR_MAXTIMO : (1 << (t)))

extern long sr_timer;

static char sr_fb[SMT_MAXINFO];
static EVQ *evq = 0;
static u_long ev_cnt = 0;
static u_long ev_reporting = 0;

static void
dequeue_evq(EVQ *ev)
{
	register EVQ *oev;

	if (!ev || (oev = evq) == 0)
		return;

	if (ev == oev) {
		evq = ev->next;
		ev->next = 0;
		return;
	}

	while (oev->next) {
		if (ev == oev->next) {
			oev->next = ev->next;
			ev->next = 0;
			return;
		}
		oev = oev->next;
	}
}

static void
enqueue_ev(EVQ *ev)
{
	register EVQ *oev;

	if (!ev)
		return;
	ev->next = 0;	/* paranoid */

	oev = evq;
	if (oev == 0) {
		evq = ev;
		return;
	}

	while (oev->next)
		oev = oev->next;
	oev->next = ev;
}

static void
sr_purge(int event)
{
	register EVQ *tmp;
	register EVQ *ev = evq;

	while (ev) {
		tmp = ev->next;
		if ((ev->event == event) && (ev->context == smtp)) {
			dequeue_evq(ev);
			free(ev);
		}
		ev = tmp;
	}
}

static void
sr_gc()
{
	register EVQ *tmp;
	register EVQ *ev = evq;

	while (ev) {
		tmp = ev->next;
		if (ev->context == smtp) {
			ev->tick_cnt++;
			if (!ev->tick_start || ev->tick_cnt>smtp->reportlimit) {
				dequeue_evq(ev);
				free(ev);
			}
		}
		ev = tmp;
	}
}

static int
sr_build(char *info, u_long infolen)
{
	register len;
	int found = 0;
	char *buf = info;
	register EVQ *ev = evq;

	if (!ev) {
		return(-1);
	}

	/* set time stamp */
	(void)tlvget(PTYPE_MTS, 0, 0, &info, &infolen);
	(void)tlvget(0x1034, 0, 0, &info, &infolen);
#ifdef OPTIONAL
	(void)tlvget(PTYPE_MF, &ridp, &ridlen, &info, &infolen);
	(void)tlvget(PTYPE_USR, &ridp, &ridlen, &info, &infolen);
#endif

	while (ev) {
		if (ev->context == smtp) {
			if ((len = ev->infolen) > infolen)
				return(-1);
			if (len > 0) {
				bcopy(ev->info, info, len);
				info += len;
				infolen -= len;
				found++;
			}
			if (smtp->mac)
				ev->tick_start = !(smtp->mac->ringop);
		}
		ev = ev->next;
	}

	if (found > 0)
		return(info-buf);
	return(0);
}


static void
sr_report(int newevent)
{
	int len, i;
	SMT_MAC *mac;
	SMTD *cx = smtp;

	/* pace at least 2 seconds */
	if (newevent) {
		if (ev_cnt <= 1)
			goto sr_ret;
		ev_cnt = 0;
	}

	for (i = 0; i < nsmtd; i++) {
		smtd_setcx(i);
		/* check if anything to report */
		if ((len = sr_build(sr_fb, sizeof(sr_fb))) <= 0) {
			EVNTDEBUG((LOG_DBGEVNT,0,"no events for ring %d", i));
			continue;
		}

		for (mac = smtp->mac; mac != 0; mac = mac->next) {
			updatemac(mac);
			if (!mac->ringop) {
				EVNTDEBUG((LOG_DBGEVNT,0,
					"%s: SRF suppressed due to no ringop",
					mac->name));
				continue;
			}
			if (fs_announce(mac, SMT_FC_SRF, sr_fb, len)
				!= RC_SUCCESS) {
				sm_log(LOG_ERR,0,"SRF failed for %s",
				       mac->name);
			} else {
				EVNTDEBUG((LOG_DBGEVNT,0,
					   "%s:SRF to %s, len=%d",
					   mac->name,
					   midtoa(&smtp->sr_mid), len));
			}
		}
		sr_gc();
	}

sr_ret:
	ev_cnt++;
	sr_timer = curtime.tv_sec + backoff(ev_cnt);
	smtp = cx;
}

void
sr_timo(void)
{
	if (sr_timer <= curtime.tv_sec
	    || sr_timer > curtime.tv_sec+SR_MAXTIMO) {
		ev_reporting++;
		sr_report(0);
		ev_reporting--;
	}
}


static EVQ *
getevq(int event)
{
	register EVQ *ev = 0;

	for (ev = evq; ev; ev = ev->next) {
		if ((ev->event == event) && (ev->context == smtp)) {
			dequeue_evq(ev);
			break;
		}
	}

	if (ev != 0) {
		sr_purge(event);
	} else if ((ev = (EVQ *)Malloc(sizeof(EVQ))) == 0) {
		return(0);
	}

	/* set transient time stamp */
	smt_gts(&smtp->trans_ts);
	ev->next = 0;
	ev->context = smtp;
	ev->event = event;
	ev->tick_cnt = 0;
	return(ev);
}

#define SETRID(x, y)	{ \
			rid[0] = 0; rid[1] = htons(((x)obj)->rid); \
			ridp = (char *)&rid[0]; ridlen = sizeof(rid); \
			ptype = y; break; }

/*
 *	event: just add.
 *	condition: reuse iff exists.
 */
void
sr_event(int event, int type, void *obj)
{
	EVQ *ev;
	char *info;
	u_long infolen;
	char *ridp;
	u_long ridlen;
	u_short rid[2];
	u_long ptype = 0;

	if (!smtp->srf_on) {
		EVNTDEBUG((LOG_DBGEVNT, 0, "sr_event: srf off"));
		return;
	}

	/*
	 * build pdus.
	 */
	switch(event) {
	  /* SMT events/conds = 1 */
	  case EVNT_SMTCONF:	SETRID(SMT_MAC *, PTYPE_PD_EVNT);

	  /* MAC events/conds = 2 */
	  case EVNT_FRAMEERR:	SETRID(SMT_MAC *, PTYPE_MFC_COND);
	  case EVNT_NOTCOPIED:	SETRID(SMT_MAC *, PTYPE_MFNC_COND);
	  case EVNT_DUPA:	SETRID(SMT_MAC *, PTYPE_DUPA_COND);
	  case EVNT_NBRCHG:	SETRID(SMT_MAC *, PTYPE_NBRCHG_EVNT);

	  /* PORT events/conds = 4 */
	  case EVNT_PLER:	SETRID(SMT_PHY *, PTYPE_PLER_COND);
	  case EVNT_EBE:	SETRID(SMT_PHY *, PTYPE_PEB_COND);
	  /* UICA is event but treat it as cond for something good */
	  case EVNT_UCA:	SETRID(SMT_PHY *, PTYPE_UICA_EVNT);

	  /* Attach events/conds = 6 */

	  default:
		sm_log(LOG_ERR, 0, "invalid event 0x%x ignored", event);
		return;
	}

	if (ev_reporting)
		return;
	ev_reporting = 1;
	if ((ev = getevq(event)) == 0) {
		EVNTDEBUG((LOG_DBGEVNT, 0, "sr_event: getevq failed"));
		goto ev_ret;
	}

	info = ev->info;
	infolen = sizeof(ev->info);

	if (tlvget(ptype, &ridp, &ridlen, &info, &infolen) != RC_SUCCESS) {
		sm_log(LOG_ERR, 0, "event 0x%x tlvget failed", event);
		goto ev_ret;
	}
	ev->infolen = info - ev->info;
	assert(ev->infolen <= sizeof(ev->info));
	enqueue_ev(ev);
	sr_report(event);

ev_ret:
	ev_reporting = 0;
}
