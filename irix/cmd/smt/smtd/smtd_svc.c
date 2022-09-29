/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.23 $
 */

#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <netinet/in.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_snmp.h>

#include <smtd.h>
#include <smtd_svc.h>
#include <smtd_fs.h>
#include <smtd_nn.h>
#include <smtd_parm.h>
#include <smtd_event.h>
#include <ma.h>
#include <smtd_rmt.h>

static void gc_fs(void);

enum fddi_pc_type
getpctype(SMT_MAC *mac, int unit, int phyidx)
{
	switch (mac->st_type) {
		case SAC:
			return(PC_M);
		case SAS:
			return(PC_S);
		case SM_DAS:
			return((phyidx == 0) ? PC_B : PC_A);
		case DM_DAS:
			return(((unit&1) == 0) ? PC_B : PC_A);
	}
	return(PC_UNKNOWN);
}

void
reset_rids()
{
	register int rid;
	register SMT_PHY *phy;
	register SMT_MAC *mac;

	for (phy = smtp->phy, rid = 1; phy; rid++, phy = phy->next) {
		phy->rid = rid;
		phy->path.rid = rid;
	}
	for (mac = smtp->mac, rid = 1; mac; rid++, mac = mac->next) {
		mac->rid = rid;
		mac->path.rid = rid + smtp->phy_ct;
	}
}

SMT_PATH *
getpathbyrid(int rid)
{
	SMT_MAC *mac;
	SMT_PHY *phy;
	SMT_PATH *path;

	if (rid > smtp->phy_ct) {
		rid -= smtp->phy_ct;
		if ((mac = getmacbyrid(rid)) == 0)
			return(0);
		path = &mac->path;
	} else {
		if ((phy = getphybyrid(rid)) == 0)
			return(0);
		path = &phy->path;
	}

	if (path->rid != rid) {
		sm_log(LOG_ERR, 0, "fatal path rid mismatch");
		return(0);
	}
	return(path);
}

SMT_PHY *
getphybyrid(int rid)
{
	SMT_PHY *phy;

	for (phy = smtp->phy; phy; phy = phy->next) {
		if (phy->rid == rid) {
			CFDEBUG((LOG_DBGCF, 0, "phyby(%d) found", rid));
			return(phy);
		}
	}

	CFDEBUG((LOG_DBGCF, 0, "phyby(%d) not found", rid));
	return(0);
}

SMT_MAC *
getmacbyrid(int rid)
{
	SMT_MAC *mac;

	for (mac = smtp->mac; mac; mac = mac->next) {
		if (mac->rid == rid) {
			CFDEBUG((LOG_DBGCF,0,"macby(%d) = %s", rid, mac->name));
			return(mac);
		}
	}

	CFDEBUG((LOG_DBGCF, 0, "macby(%d) not found", rid));
	return(0);
}

SMT_MAC *
getmacbyname(char *ifname, int unit)
{
	int i;
	SMT_MAC *mac;

	for (i = 0; i < nsmtd; i++) {
		smtd_setcx(i);
		for (mac = smtp->mac; mac; mac = mac->next) {
			if (strncmp(mac->name, ifname, strlen(ifname)))
				continue;
			if (mac->name[strlen(ifname)] == (char)(unit + '0')) {
				CFDEBUG((LOG_DBGCF, 0,
					"macby(%s%d) found", ifname, unit));
				return(mac);
			}
		}
	}

	CFDEBUG((LOG_DBGCF, 0, "macby(%s%d) not found", ifname, unit));
	return(0);
}

/*
 * Handle SMT timeout.
 */
void
smtd_timeout(void)
{
	register SMT_MAC *mac;

	SCHEDDEBUG((LOG_DBGSCHED, 0, "timeout(%d:%d)",
		    curtime.tv_sec, curtime.tv_usec));

	for (mac = smtp->mac; mac != 0; mac = mac->next) {
		if (mac->tnn > curtime.tv_sec
		    && mac->trm_ck > curtime.tv_sec)
			continue;

		updatemac(mac);		/* get recent status from driver */
		nn_expire(mac);		/* send NIF */
		nn_verify(mac);
		smtd_rmt(mac);		/* check RMT state machine */
	}

	/* Garbage collect registered fs */
	gc_fs();
}

/*
 * Handle Notification from kernel.
 */
void
smtd_indicate(register SMT_MAC *mac)
{
#define ALL_ARM1 (SMT_ALARM_RNGOP|SMT_ALARM_LEM|SMT_ALARM_BS)
#define ALL_ARM2 (SMT_ALARM_CON_X|SMT_ALARM_CON_U|SMT_ALARM_CON_W)
#define ALL_ARM3 (SMT_ALARM_TFIN|SMT_ALARM_TBEG|SMT_ALARM_SICK)
#define ALL_ARM	 (ALL_ARM1|ALL_ARM2|ALL_ARM3)
	u_long alarm;
	u_long arm0 = 0;
	u_long arm1 = 0;

	if (sm_arm(mac->name, mac->smtsock, &arm0, 0) != SNMP_ERR_NOERROR)
		sm_log(LOG_INFO, 0, "%s:0 sm_arm alarm failed", mac->name);
	if (mac->st_type == SM_DAS &&
	    sm_arm(mac->name, mac->smtsock, &arm1, 1) != SNMP_ERR_NOERROR)
		sm_log(LOG_INFO, 0, "%s:1 sm_arm alarm failed", mac->name);
	if ((alarm = arm0|arm1) == 0) {
		sm_log(LOG_INFO, 0, "%s: false alarm ignored", mac->name);
		return;
	} else if (alarm&(~ALL_ARM)) {
		sm_log(LOG_INFO, 0, "%s: bogus alarm(%x) ignored",
			mac->name, (alarm&(~ALL_ARM)));
	}

	if (alarm&SMT_ALARM_RNGOP) {
		sm_log(LOG_INFO, 0, "RINGOP_sig");
	}
	if (alarm&SMT_ALARM_LEM) {
		sr_event(EVNT_PLER, EVTYPE_ASSERT, (void *)mac->primary);
		sm_log(LOG_INFO, 0, "LEM_sig");
	}
	if (alarm&SMT_ALARM_BS) {
		sm_log(LOG_INFO, 0, "BS_FLAG_sig");
	}
	if (alarm&SMT_ALARM_CON_X) {
		sr_event(EVNT_UCA, EVTYPE_ASSERT, (void *)mac->primary);
		sm_log(LOG_INFO, 0, "C_X_sig");
	}
	if (alarm&SMT_ALARM_CON_U) {
		sr_event(EVNT_UCA, EVTYPE_ASSERT, (void *)mac->primary);
		sm_log(LOG_INFO, 0, "C_U_sig");
	}
	if (alarm&SMT_ALARM_CON_W) {
		sm_log(LOG_INFO, 0, "WH_sig");
	}
	if (alarm&SMT_ALARM_TFIN) {
		mac->path.tracestatus = PT_TERM;
		sm_log(LOG_INFO, 0, "TFIN_sig");
	}
	if (alarm&SMT_ALARM_TBEG) {
		sm_log(LOG_INFO, 0, "TBEG_sig");
	}
	if (alarm&SMT_ALARM_SICK) {
		sr_event(EVNT_PLER, EVTYPE_ASSERT, (void *)mac->primary);
		sm_log(LOG_INFO, 0, "SICK_sig");
	}

	mac->changed = 1;
	updatemac(mac);
	smtd_rmt(mac);
}

/* delete a frame service entry */
static void
del_fs(SMT_FS_INFO **pred,		/* pointer to predecessor */
       SMT_FS_INFO *tgt)
{
	if (tgt) {
		*pred = tgt->next;
		(void)close(tgt->lsock);
		SCHEDDEBUG((LOG_DBGSCHED,0,
			    "unq_fs: sock=%d, port=%d, fc=%d, ft=%d",
			    tgt->lsock, (int)tgt->fsi_to.sin_port,
			    tgt->fc, tgt->ft));
		free(tgt);
	}
}


void
smtd_resp_map(int fc, int ft, char *dp, int len)
{
	int i;
	SMT_FS_INFO *p, **q, **qq;

	q = &smtp->fs;
	while (0 != (p = *q)) {
		/* 0 is promiscuous */
		if ((p->fc && p->fc != fc) || (p->ft && p->ft != ft)) {
			q = &p->next;
			continue;
		}
		FSDEBUG((LOG_DBGFS, 0, "smtdfs: s%d:p%d len %d",
			 p->lsock,(int)p->fsi_to.sin_port, len));
		i = send(p->lsock,(char*)dp,len,0);
		if (i != len) {
			if (i < 0) {
				del_fs(q,p);
				sm_log(LOG_ERR, SM_ISSYSERR,
				       "resp_map: send");
				continue;
			} else {
				sm_log(LOG_ERR, SM_ISSYSERR,
				       "smtdfs: %d of %d bytes written",
				       i,len);
			}
		}
		p->svcnum++;
		q = &p->next;
	}
}

int
unq_fs(SMT_FS_INFO *r)
{
	SMT_FS_INFO *p, **q;

	SCHEDDEBUG((LOG_DBGSCHED, 0, "unq_fs"));
	if (!r)
		return(SNMP_ERR_NOSUCHNAME);

	q = &smtp->fs;
	while (0 != (p = *q)) {
		SCHEDDEBUG((LOG_DBGSCHED, 0, "unq_fs: scan timo=%d port=%d",
				p->timo, (int)p->fsi_to.sin_port));
		if ((p->fsi_to.sin_port == r->fsi_to.sin_port) &&
		    (p->fc == r->fc) && (p->ft == r->ft))
			break;
		q = &p->next;
	}

	del_fs(q,p);
	return(SNMP_ERR_NOERROR);
}

/*
 * Grabage collect frame service requesters (such as fddivis)
 * that have not reregistered recently.
 */
static void
gc_fs(void)
{
	register SMT_FS_INFO *p;

restart:
	SCHEDDEBUG((LOG_DBGSCHED, 0, "gc_fs: start"));
	p = smtp->fs;
	while (p) {
		if (p->timo <= curtime.tv_sec) {
			SCHEDDEBUG((LOG_DBGSCHED,0,
				    "gc_fs: GOTIT! sock=%d, port=%d",
				    p->lsock, (int)p->fsi_to.sin_port));
			(void)unq_fs(p);
			goto restart;
		}
		p = p->next;
	}
	SCHEDDEBUG((LOG_DBGSCHED, 0, "gc_fs: done"));
}

/*
 * Register Frame Service.
 */
int
enq_fs(SMT_FS_INFO *p)
{
	SMT_FS_INFO *q;
	time_t min_timo, max_timo;

	if (!p)
		return(SNMP_ERR_NOSUCHNAME);
	min_timo = curtime.tv_sec+MIN_FS_REGTIMO;
	max_timo = curtime.tv_sec+MAX_FS_REGTIMO;
	if (p->timo < min_timo)
		p->timo = min_timo;
	else if (p->timo > max_timo)
		p->timo = max_timo;

	/* check DUP register attempt */
	for (q = smtp->fs; q; q = q->next) {
		if ((q->fsi_to.sin_port == p->fsi_to.sin_port) &&
		    (q->fc == p->fc) && (q->ft == p->ft)) {
			q->timo = p->timo;
			return(SNMP_ERR_NOSUCHNAME);
		}
	}

	p->next = NULL;
	p->svcnum = 0;
	p->lsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (p->lsock < 0) {
		sm_log(LOG_ERR, SM_ISSYSERR, "fs_reg: socket");
		return(SNMP_ERR_NOSUCHNAME);
	}
	if (0 > connect(p->lsock, &p->fsi_to, sizeof(p->fsi_to))) {
		sm_log(LOG_ERR, SM_ISSYSERR, "fs_req: connect");
		(void)close(p->lsock);
		return(SNMP_ERR_NOSUCHNAME);
	}

	if ((q = smtp->fs) == 0) {
		smtp->fs = p;
	} else {
		while (q->next)
			q = q->next;
		q->next = p;
	}

	FSDEBUG((LOG_DBGFS, 0,
		"FSreg: sock=%d,port=%d",p->lsock,(int)p->fsi_to.sin_port));
	return(SNMP_ERR_NOERROR);
}

void
fsq_clearlog()
{
	register int i;
	SMT_FS_INFO *fsp = smtp->fss;

	for (i = 0; i < SMT_NFRAMETYPE; i++, fsp++) {
		bzero((char*)fsp, sizeof(*fsp));
		if (i == SMT_NFRAMETYPE-1)
			fsp->fss_fc = SMT_FC_ESF;
		else
			fsp->fss_fc = i+1;
	}
	/* unknown entry */
	bzero((char*)fsp, sizeof(*fsp));
}

void
fsq_log(u_char fc, u_char ft, int rcv)
{
	SMT_FS_INFO *fsp;

	if ((fc >= SMT_FC_NIF) && (fc <= SMT_FC_RMV_PMF))
		fsp = &smtp->fss[fc-1];
	else if (fc == SMT_FC_ESF)
		fsp = &smtp->fss[SMT_NFRAMETYPE-1];
	else
		fsp = &smtp->fss[SMT_NFRAMETYPE];

	switch (ft) {
		case SMT_FT_ANNOUNCE:
			if (rcv)
				fsp->fss_ianoun++;
			else
				fsp->fss_oanoun++;
			break;
		case SMT_FT_REQUEST:
			if (rcv)
				fsp->fss_ireq++;
			else
				fsp->fss_oreq++;
			break;
		case SMT_FT_RESPONSE:
			if (rcv)
				fsp->fss_ires++;
			else
				fsp->fss_ores++;
			break;
	}
}

/*
 *
 */
void
fsq_stat(SMT_FSSTAT *stat)
{
	register int i = 0;
	register SMT_FS_INFO *q;
	register SMT_FS_INFO *e = &stat->entry[0];

	if (stat->num == -1) {
		q = smtp->fss;
		while (i <= SMT_NFRAMETYPE) {
			*e = *q;
			q++; e++;
			i++;
		}
	} else {
		q = smtp->fs;
		while (q && i < SMT_MAXFSENTRY) {
			*e = *q;
			q = q->next;
			i++;
			e++;
		}
	}

	stat->num = i;
	sm_log(LOG_DEBUG, 0, "FSQStat: %d entries", i);
}

/*
 *
 */
void
fsq_del(SMT_FSSTAT *stat)
{
	register int i;
	register SMT_FS_INFO *p;
	register SMT_FS_INFO *e;

	if (stat->num < 0) {
		if (stat->num == -1)
			fsq_clearlog();
		return;
	}

	if (stat->num == 0) {
		while (smtp->fs)
			(void)unq_fs(smtp->fs);
		return;
	}

	for (i = 0; i < stat->num && i < SMT_MAXFSENTRY; i++) {
		e = &stat->entry[i];
restart:
		p = smtp->fs;
		while (p) {
			if ((e->fsi_to.sin_port == p->fsi_to.sin_port) &&
			    (e->fc == p->fc) && (e->ft == p->ft)) {
				(void)unq_fs(p);
				goto restart;
			}
			p = p->next;
		}
	}
}
