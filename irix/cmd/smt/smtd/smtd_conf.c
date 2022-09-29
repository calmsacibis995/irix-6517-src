/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.57 $
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>		/* For sysconf() */

#include <sm_types.h>
#include <sm_log.h>
#include <smt_asn1.h>
#include <smt_snmp.h>
#include <smt_parse.h>

#include <smtd.h>
#include <smtd_conf.h>
#include <smtd_svc.h>
#include <smt_subr.h>
#include <smtd_parm.h>
#include <smtd_event.h>
#include <smtd_rmt.h>
#include <cf.h>
#include <cf_sanity.h>
#include <ma.h>

/*
 *	1. read config.
 *	2. make table.
 *	3. patch table.
 *	4. Bring up ifaces.
 */
void
config_smtd()
{
	FILE *fp;
	char line[LINESIZE];
	char name[CFBUFSIZE];
	char **lp;

	/* read config */
	/* make table */
	/* patch table */

	CFDEBUG((LOG_DBGCF, 0, "smtd_config:"));
	valid_nsmtd = 0;
	if ((fp = fopen(smtd_cfile, "r")) == NULL) {
		sm_log(LOG_ERR, SM_ISSYSERR, "can't open %s", smtd_cfile);
		return;
	}
	while (lp = getline(line, sizeof(line), fp)) {
		skipblank(lp);
		if ((**lp == '#') || (**lp == '\n'))
			continue;

		if (sscanf(*lp, "%s", name) != 1) {
			sm_log(LOG_EMERG, 0, "Error in config file: "
			       "can't decode section delimiter: %s", *lp);
			smtd_quit();
		}

		if (strcmp(name, "STATION:") == 0) {
			if (valid_nsmtd >= NSMTD) {
				sm_log(LOG_EMERG, 0, "Error in config file: "
				       "too many stations defined: max=%d",
				       NSMTD);
				smtd_quit();
			}
			smtd_setcx(valid_nsmtd);
			valid_nsmtd++;
			conf_station(fp);
		} else if (strcmp(name, "MAC:") == 0) {
			conf_mac(fp);
		} else {
			sm_log(LOG_EMERG, 0,
		"Error in config file: unknown section delimiter: %s", name);
			smtd_quit();
		}
	}
	(void)fclose(fp);
}

static int
getmacconf(SMT_MAC *mac)
{
	SMT_MAC *conf;

	for (conf = smtd_config; conf; conf = conf->next) {
		if (!strncmp(conf->name, mac->name, strlen(mac->name)))
			break;
	}
	if (conf == 0) {
		sm_log(LOG_ERR, 0, "%s: config missing", mac->name);
		return(SNMP_ERR_NOSUCHNAME);
	}

	mac->phy_ct = conf->phy_ct;
	mac->pathavail = conf->pathavail;
	mac->pathrequested = conf->pathrequested;
	mac->addr = conf->addr;

	mac->treq = conf->treq;
	mac->maxflops = conf->maxflops;
	mac->fsc = conf->fsc;
	mac->bridge = conf->bridge;
	mac->st_type = conf->st_type;
	mac->tmax = conf->tmax;
	mac->tmin = conf->tmin;
	mac->tvx = conf->tvx;
	mac->tmax_lobound = conf->tmax_lobound;
	mac->tvx_lobound = conf->tvx_lobound;
	mac->fr_threshold = conf->fr_threshold;
	mac->fnc_threshold = conf->fnc_threshold;

	/* defaults */
	mac->curfsc = mac->fsc;
	mac->llcavail = 1;       /* always true */
	mac->st = (struct smt_st *)Malloc(sizeof(struct smt_st));
	mac->st1= (struct smt_st *)Malloc(sizeof(struct smt_st));
	return(SNMP_ERR_NOERROR);
}

static int
getphyconf(SMT_MAC *mac, int phyidx)
{
	SMT_PHY *sphy, *tphy;
	SMT_MAC *cmac;

	for (cmac = smtd_config; cmac; cmac = cmac->next) {
		CFDEBUG((LOG_DBGCF, 0, "macconf: scan %s", cmac->name));
		if (!strncmp(cmac->name, mac->name, strlen(mac->name)))
			break;
	}
	if (cmac == 0) {
		sm_log(LOG_ERR, 0, "%s: %s info missing",
				mac->name, phyidx ? "secondary" : "primary");
		return(SNMP_ERR_NOSUCHNAME);
	}

	if (phyidx == 0) {
		sphy = cmac->primary;
		tphy = mac->primary;
	} else {
		sphy = cmac->secondary;
		tphy = mac->secondary;
	}
	*tphy = *sphy;
	return(SNMP_ERR_NOERROR);
}


static int
updatephy(SMT_MAC *mac, register SMT_PHY *phy, register struct smt_st *st)
{
	int i;
	register int nflag = st->flags;

	if (phy->pcm_flags != nflag) {
		register int uca;
		phy->pcm_flags = nflag;

		/* attachment info */
		phy->obs = (nflag&PCM_HAVE_BYPASS) ? 1 : 0;
		i = (nflag&PCM_CF_JOIN) ? 1 : 0;
		if (phy->insert != i) {
			mac->pcmtime = curtime.tv_sec;
			/* Force NIF after PCM.
			 * If it was not our A that died, then it was our
			 * S or B, and in either case our downstream
			 * neighbor is suspect.  If it was our A and
			 * we are wrapped through it, then our downstream
			 * neighbor is suspect.
			 * And similarly for our upstream neighbor.
			 */
			if (phy->pctype != PC_A
			    || smtp->cf_state == SC_WRAP_A)
				mac->dv_flag = 0;
			if (phy->pctype != PC_B
			    || smtp->cf_state == SC_WRAP_B)
				mac->uv_flag = 0;
			if (mac->tnn > curtime.tv_sec+T_NOTIFY_MIN)
				mac->tnn -= smtp->t_notify;
			phy->insert = i;
		}

		/* PHY Operation Grp */
		phy->bs_flag = (nflag&PCM_BS_FLAG) ? 1 : 0;

		uca = (nflag&(PCM_CON_U|PCM_CON_X)) ? 1 : 0;
		if (phy->uica_flag != uca) {
			phy->uica_flag = uca;
			if (uca)
				sr_event(EVNT_UCA,EVTYPE_ASSERT,(void*)phy);
			else
				sr_event(EVNT_UCA,EVTYPE_DEASSERT,(void*)phy);
		}

		if (nflag & PCM_WA_FLAG) {
			phy->withhold = (PHY_WITHHOLD)(mac->primary == phy
						       ? holding_prm
						       : holding_sec);
		} else {
			phy->withhold = (PHY_WITHHOLD)not_holding;
		}
	}

	if (!phy->insert)
		phy->ce_state = ISOLATED;
	else if (phy->tls != st->tls) {
		phy->tls = st->tls;
		if ((st->tls == T_PCM_WRAP) && (mac->phy_ct > 1))
			phy->ce_state = INSERT_X;
		else if (phy->pctype == PC_A)
			phy->ce_state = INSERT_S;
		else
			phy->ce_state = INSERT_P;
	}

	phy->remotemac = st->r_val & PCM_BIT(9)?1:0;
	phy->loop_time = st->t_next;
	phy->lctfail_ct = st->lct_fail;

	if (phy->pcnbr != st->npc_type) {
		phy->pcnbr = st->npc_type;
		if (phy->pcnbr == phy->pctype) {
			if ((phy->pcnbr == PC_A) &&
			    ((smtp->topology&SMT_AA_TWIST) == 0)) {
				smtp->topology |= SMT_AA_TWIST;
				sm_log(LOG_EMERG, 0, "A<->A twist detected");
			} else if ((phy->pcnbr == PC_B) &&
				   ((smtp->topology&SMT_BB_TWIST) == 0)) {
				smtp->topology |= SMT_BB_TWIST;
				sm_log(LOG_EMERG, 0, "B<->B twist detected");
			}
		} else
			smtp->topology &= ~(SMT_AA_TWIST|SMT_BB_TWIST);
	}
	if ((phy->pcnbr == PC_M) || (phy->pctype == PC_S))
		smtp->topology &= ~SMT_ROOTSTA;
	else
		smtp->topology |= SMT_ROOTSTA;

	/* PHY Error Ctrs Grp */
	if (phy->eberr_delta != (st->eovf.lo - phy->eberr_ct)) {
		phy->eberr_delta = st->eovf.lo - phy->eberr_ct;
		phy->eberr_ct = st->eovf.lo;
		if (phy->eberr_delta)
			sr_event(EVNT_EBE, EVTYPE_ASSERT, phy);
		else
			sr_event(EVNT_EBE, EVTYPE_DEASSERT, phy);
	}

	/* PHY Status Grp */
	if (phy->pcm_state != st->pcm_state) {
		phy->pcm_state = st->pcm_state;
		phy->conn_state = st->pcm_state==PCM_ST_CONNECT? PC_CONNECTING:
				  st->pcm_state==PCM_ST_ACTIVE? PC_ACTIVE :
				  st->pcm_state==PCM_ST_OFF? PC_DISABLED:
				  PC_STANDBY;
	}

	/* PHY Ler Grp */
	if (phy->lem_ct != st->lem_count || phy->ler_estimate != st->ler2) {
		phy->bler_estimate = phy->ler_estimate;
		phy->blemreject_ct = phy->lemreject_ct;
		phy->blem_ct = phy->lem_ct;
		phy->bler_ts = mac->sampletime;

		phy->lem_ct = st->lem_count;
		phy->ler_estimate = st->ler2;
		phy->lemreject_ct = st->lct_tot_fail;

		if (phy->pler_cond != (phy->ler_alarm>=phy->ler_estimate)) {
			phy->pler_cond ^= 1;
			if (phy->pler_cond) {
				sm_log(LOG_EMERG, 0, "%s.%d: LER alarm = %d",
					mac->name, phy->rid-1,
					phy->ler_estimate);
				sr_event(EVNT_PLER,EVTYPE_ASSERT,phy);
			} else {
				sr_event(EVNT_PLER,EVTYPE_DEASSERT,phy);
			}
		}

		if (phy->pler_cond
		    && (nflag&PCM_CF_JOIN)
		    && phy->ler_cutoff >= phy->ler_estimate) {
			mac->changed = 1;
			(void)sm_lem_fail(mac->name,
					  mac->smtsock,phy->rid-1);
		}
	}

	return(SNMP_ERR_NOERROR);
}

/*
 * ptype 0 - primary
 *	 1 - secondary
 */
static int
smtd_setconf(SMT_MAC *mac, int ptype)
{
	SMT_PHY *phy;
	struct smt_conf conf;

	phy = ptype == 0 ? mac->primary : mac->secondary;
	bcopy( (char*)&smtp->manuf, (char*)&conf.mnfdat, sizeof(smtp->manuf));
	conf.t_max = mac->tmax;		/* FDDI timers */
	conf.t_min = mac->tmin;
	conf.t_req = mac->treq;
	conf.tvx = mac->tvx;
	conf.ler_cut = phy->ler_cutoff;
	conf.type = phy->type;
	conf.pc_type = phy->pctype;
	conf.debug = phy->debug;
	conf.ip_pri = phy->ip_pri;
	conf.pcm_tgt = phy->pcm_tgt;

	sm_log(LOG_INFO, 0,
		"Setting sttype=%d pc_type=%x",conf.type,conf.pc_type);

	if (sm_setconf(mac->name, ptype, &conf) != SNMP_ERR_NOERROR)
		return(SNMP_ERR_NOSUCHNAME);

	phy->type = conf.type;
	phy->pcm_tgt = conf.pcm_tgt;

	if (sm_getconf(mac->name, 0, &conf) != SNMP_ERR_NOERROR)
		return(SNMP_ERR_NOSUCHNAME);
	mac->tvx = conf.tvx;
	mac->treq = conf.t_req;
	mac->tmin = conf.t_min;
	mac->tmax = conf.t_max;

	return(SNMP_ERR_NOERROR);
}


void
updatemac(register SMT_MAC *mac)
{
	CFM_STATE cfs;
	struct ifreq ifr;
	register enum rt_ls tls;
	register u_long df, dl, de;
	long dt;
	register struct smt_st *st, *st1;

	if ((!mac) || (!mac->primary))
		return;

	if (mac->sampletime.tv_sec == curtime.tv_sec
	    && !mac->changed)
		return;
	mac->changed = 0;
	mac->sampletime = curtime;

	st = mac->st;
	st1 = mac->st1;
	cfs = smtp->cf_state;
	if (sm_stat(mac->name, 0, st) != SNMP_ERR_NOERROR) {
		/* reset the board once if we cannot get its status
		 */
		if (mac->flips == 0) {
			sm_log(LOG_EMERG, 0, "cannot get status; reset #%d",
				mac->flips);
			mac->tflip = curtime.tv_sec;
			/* do not keep resetting if we cannot */
			if (sm_reset(mac->name, 1))
				mac->flips = mac->maxflops;
			mac->changed = 1;
		}
		return;
	}
	mac->addr = st->addr;
	tls = mac->primary->tls;
	if (updatephy(mac, mac->primary, st) != SNMP_ERR_NOERROR)
		return;

	mac->err_ct = st->error.lo;
	mac->lost_ct = st->lost_ct.lo;
	mac->late_ct = st->trtexp.lo;	/* TRT exp = late count */
	mac->tvxexpired_ct = st->tvxexp.lo;
	mac->notcopied_ct = st->misfrm.lo;
	mac->ringop_ct = st->rngop.lo;
	mac->frame_ct = st->frame_ct.lo;
	mac->token_ct = st->tok_ct.lo;
	mac->tneg = st->t_neg;

	mac->ringop = (st->flags&PCM_RNGOP);
	if (st->flags & PCM_CF_JOIN)
		mac->rmt_flag |= RMF_JOIN;
	else
		mac->rmt_flag &= ~RMF_JOIN;

	if (tls != st->tls) {
		tls = st->tls;
		if (tls == T_PCM_WRAP) {
			smtp->topology |= SMT_WRAPPED;
			switch (mac->primary->pctype) {
				case PC_A: smtp->cf_state = SC_WRAP_A; break;
				case PC_B: smtp->cf_state = SC_WRAP_B; break;
				case PC_S: smtp->cf_state = SC_WRAP_S; break;
				case PC_M:
				default: smtp->cf_state = SC_ISOLATED; break;
			}
		} else if (tls == T_PCM_WRAP_AB) {
			smtp->topology |= SMT_WRAPPED;
			smtp->cf_state = SC_WRAP_AB;
		} else {
			smtp->topology &= ~SMT_WRAPPED;
			if (tls==T_PCM_THRU) {
				switch (mac->primary->pctype) {
					case PC_A: smtp->cf_state = SC_THRU_B;
						   break;
					case PC_B: smtp->cf_state = SC_THRU_A;
						   break;
					case PC_S: smtp->cf_state = SC_WRAP_S;
						   break;
					case PC_M:
					default: smtp->cf_state = SC_ISOLATED;
						 break;
				}
			}
		}
	}

	if (mac->secondary) {
		if (sm_stat(mac->name, 1, st1) != SNMP_ERR_NOERROR)
			return;
		tls = mac->secondary->tls;
		if (updatephy(mac, mac->secondary, st1) != SNMP_ERR_NOERROR)
			return;
		mac->ringop |= (st1->flags&PCM_RNGOP);
		if (st1->flags&PCM_CF_JOIN)
			mac->rmt_flag |= RMF_JOIN;

		if (tls != st1->tls) {
			tls = st1->tls;
			if (tls == T_PCM_WRAP) {
				smtp->topology |= SMT_WRAPPED;
				switch (mac->secondary->pctype) {
					case PC_A: smtp->cf_state = SC_WRAP_A;
						   break;
					case PC_B: smtp->cf_state = SC_WRAP_B;
						   break;
					case PC_S: smtp->cf_state = SC_WRAP_S;
						   break;
					case PC_M:
					default: smtp->cf_state = SC_ISOLATED;
						 break;
				}
			} else if (tls == T_PCM_WRAP_AB) {
				smtp->topology |= SMT_WRAPPED;
				smtp->cf_state = SC_WRAP_AB;
			} else if (smtp->cf_state!=SC_THRU_B &&
				   tls==T_PCM_THRU) {
				switch (mac->secondary->pctype) {
					case PC_A: smtp->cf_state = SC_THRU_B;
						   break;
					case PC_B: smtp->cf_state = SC_THRU_A;
						   break;
					case PC_S: smtp->cf_state = SC_WRAP_S;
						   break;
					case PC_M:
					default: smtp->cf_state = SC_ISOLATED;
						 break;
				}
			}
		}
	}

	/* connect conf state */
	if (cfs != smtp->cf_state)
		goto skip_cfs;

	switch (smtp->cf_state) {
		case SC_THRU_A:
			mac->curpath = PATHTYPE_SECONDARY;
			break;
		case SC_WRAP_S:
		case SC_THRU_B:
		case SC_WRAP_AB:
			mac->curpath = PATHTYPE_PRIMARY;
			break;
		case SC_THRU_AB:
		case SC_WRAP_A:
		case SC_WRAP_B:
			mac->curpath = PATHTYPE_PRIMARY|PATHTYPE_SECONDARY;
			break;
		case SC_ISOLATED:
			mac->curpath = PATHTYPE_ISOLATED;
			break;
		default:
			mac->curpath = PATHTYPE_UNKNOWN;
	}

	/* Downstream neighbor port type */
	switch (smtp->cf_state) {
		case SC_WRAP_A:
		case SC_THRU_A:
			if (mac->secondary)
				mac->dnaport = mac->secondary->pcnbr;
			break;
		case SC_WRAP_S:
		case SC_WRAP_B:
		case SC_THRU_B:
		case SC_THRU_AB:
			mac->dnaport = mac->primary->pcnbr;
			break;
		case SC_WRAP_AB:
		case SC_ISOLATED:
		default:
			mac->dnaport = PC_UNKNOWN;
	}

	/* set PATH */
#define NUMPHYS smtp->phy_ct
	if (mac->st_type == DM_DAS) {
		int orid;
		SMT_MAC *om = 0;

		if (mac->curpath == (PATHTYPE_PRIMARY|PATHTYPE_SECONDARY)) {
			if (mac->rid & 0x1)
				orid = mac->rid + 1;
			else
				orid = mac->rid - 1;
			om = getmacbyrid(orid);
		}
		if (om) {
			/* Let only one mac change path desc. */
			if ((smtp->cf_state == SC_WRAP_A &&
					mac->primary->pctype == PC_A) ||
			    (smtp->cf_state == SC_WRAP_B &&
					mac->primary->pctype == PC_B)) {
				mac->primary->macplacement = mac->rid + NUMPHYS;
				mac->pathplacement = om->rid+NUMPHYS;
				om->pathplacement = om->primary->rid;
				om->primary->macplacement = 0;
				om->primary->phyplacement = mac->primary->rid;
			}
		} else {
			mac->primary->macplacement = mac->rid + NUMPHYS;
			mac->pathplacement = mac->primary->rid;
		}
	} else {
		switch (mac->curpath) {
		case PATHTYPE_PRIMARY:
		case PATHTYPE_SECONDARY:
			/* O.K. cause there could be no seconddary path only */
			mac->pathplacement = mac->primary->rid;
			if (mac->secondary) {
				mac->secondary->macplacement = mac->rid+NUMPHYS;
				mac->primary->macplacement = 0;
				mac->primary->phyplacement=mac->secondary->rid;
			}
			break;

		case PATHTYPE_PRIMARY|PATHTYPE_SECONDARY:
			if (smtp->cf_state == SC_WRAP_A) {
				mac->secondary->macplacement=mac->rid+NUMPHYS;
				mac->pathplacement = mac->secondary->rid;
				mac->primary->macplacement = 0;
				mac->primary->phyplacement = 0;
			} else { /* must be WRAP_B */
				mac->primary->macplacement = mac->rid+NUMPHYS;
				mac->pathplacement = mac->primary->rid;
				mac->secondary->macplacement = 0;
				mac->secondary->phyplacement = 0;
			}
			break;

		default:
			mac->primary->macplacement = 0;
			mac->primary->phyplacement = 0;
			mac->pathplacement = 0;
			if (mac->secondary) {
				mac->secondary->macplacement = 0;
				mac->secondary->phyplacement = 0;
			}
		}
	}
skip_cfs:


	/* get current PHY and MAC states for the RMT state machine */
#define ULC(x, f) { \
	if (LCTR(mac->primary->x) < LCTR(st->x)) mac->rmt_flag |= f; \
	else mac->rmt_flag &= ~f; mac->primary->x = st->x; }

	ULC(trtexp, RMF_TRT_EXP);	/* TRT expired and late count != 0 */
	ULC(myclm, RMF_MYCLM);		/* my CLAIM frames received */
	ULC(loclm, RMF_OTRCLM);		/* lower CLAIM frames received */
	ULC(hiclm, RMF_OTRCLM);		/* higher CLAIM frames received */
	ULC(mybec, RMF_MYBCN);		/* my beacon frames received */
	ULC(otrbec, RMF_OTRBCN);	/* other beacon frames */
	ULC(dup_mac, RMF_DUP_DET);	/* RMT duplicate MAC frames */
#undef ULC

	if (mac->rmt_flag & RMF_DUP_DET
	    && mac->dupa_test != DUPA_FAIL) {
		dup_det(mac);
	}

	mac->rmt_flag &= ~(RMF_T4|RMF_T5);
	if (!(mac->rmt_flag & (RMF_MYCLM|RMF_OTRCLM|RMF_MYBCN|RMF_OTRBCN))) {
		/* cannot be stuck beaconing or claiming if we are
		 * receiving claim or beacon frames.
		 */
		if (st->mac_state == MAC_CLAIMING)
			mac->rmt_flag |= RMF_T4;
		if (st->mac_state == MAC_BEACONING)
			mac->rmt_flag |= RMF_T5;
	}


#define RATIODECAY	600
#define RATIOPAD	((u_long)0x10000)
	/* Ignore ratios for the first 10 min. */
	dt = mac->sampletime.tv_sec - mac->btncc.tv_sec - RATIODECAY;

	/* update not-copied counters */
	if (mac->notcopied_ct < mac->bnc_ct || mac->recv_ct < mac->brcv_ct) {
		mac->bnc_ct = mac->notcopied_ct;
		mac->brcv_ct = mac->recv_ct;
	} else if (dt > 0) {
		de = (mac->notcopied_ct-mac->bnc_ct)*dt/RATIODECAY;
		df = (mac->recv_ct-mac->brcv_ct)*dt/RATIODECAY;
		if (df > 0) {
			mac->bnc_ct += de;
			mac->brcv_ct += df;
			mac->btncc.tv_sec += dt;
			mac->nc_ratio = (de*RATIOPAD)/(df+de);
		}
		if (mac->frm_nccond != (mac->nc_ratio
					>= mac->fnc_threshold)) {
			mac->frm_nccond ^= 1;
			if (mac->frm_nccond)
				sr_event(EVNT_NOTCOPIED,EVTYPE_ASSERT,mac);
			else
				sr_event(EVNT_NOTCOPIED,EVTYPE_DEASSERT,mac);
		}
	}

	/* update error counters */
	if (mac->frame_ct < mac->bframe_ct || mac->err_ct < mac->berr_ct ||
	    mac->lost_ct < mac->blost_ct) {
		mac->bframe_ct = mac->frame_ct;
		mac->berr_ct = mac->err_ct;
		mac->blost_ct = mac->lost_ct;

	} else if (dt > 0) {
		df = (mac->frame_ct-mac->bframe_ct)*dt/RATIODECAY;
		de = (mac->err_ct-mac->berr_ct)*dt/RATIODECAY;
		dl = (mac->lost_ct-mac->blost_ct)*dt/RATIODECAY;
		if (df > 0) {
			mac->bframe_ct += df;
			mac->berr_ct += de;
			mac->blost_ct += dl;
			mac->basetime.tv_sec += dt;
			mac->fr_errratio = ((de+dl)*RATIOPAD)/(df+dl);
		}
		if (mac->frm_errcond != (mac->fr_errratio
					 >= mac->fr_threshold)) {
			mac->frm_errcond ^= 1;
			if (mac->frm_errcond)
				sr_event(EVNT_FRAMEERR, EVTYPE_ASSERT,mac);
			else
				sr_event(EVNT_FRAMEERR, EVTYPE_DEASSERT,mac);
		}
	}

	/* get xmit/recv counts */
	(void)strncpy(ifr.ifr_name, mac->name, sizeof(ifr.ifr_name));
	if (ioctl(mac->statsock, SIOCGIFSTATS, &ifr) >= 0) {
		mac->xmit_ct = ifr.ifr_stats.ifs_opackets;
		mac->recv_ct = ifr.ifr_stats.ifs_ipackets;
	}
	/* make sure iface is alive */
	(void)strncpy(ifr.ifr_name, mac->name, sizeof(ifr.ifr_name));
	if (ioctl(mac->statsock, SIOCGIFFLAGS, (caddr_t)&ifr) >= 0) {
		mac->ifflag = ifr.ifr_flags;
	} else {
		mac->ifflag = 0;
	}
}

static SMTD *
findsmtslot(SMT_MAC *mac)
{
	int i;

	for (i = 0; i < valid_nsmtd; i++) {
		nsmtd = i+1;
		smtd_setcx(i);
		if (smtp->mac_ct == 0)
			goto slotfound;
		else if (smtp->mac_ct == 1) {
			if (smtp->st_type == DM_DAS)
				goto slotfound;
		}
	}
	CFDEBUG((LOG_DBGCF,0, "slot not found for %s\n", mac->name));
	return(0);

slotfound:
	CFDEBUG((LOG_DBGCF,0, "slot %d found for %s\n",i,mac->name));
	if (smtp->userdata.b[0] == 0) {
		strncpy(smtp->userdata.b,
			"SGI FDDI SMT " SMTD_VERSTR,
			sizeof(smtp->userdata));
	}
	return(smtp);
}

/*
 * creat new iphase and set default configuration.
 */
static SMT_MAC *
newiphase(char *ifname, int unit)
{
	SMT_MAC *mac;
	SMT_PHY *phy;
	struct smt_conf conf;

	CFDEBUG((LOG_DBGCF, 0, "%s%d: newiphase started\n", ifname, unit));

	/* New attach */
	mac = (SMT_MAC *)Malloc(sizeof(*mac));
	mac->statsock = -1;
	sprintf(mac->name, "%s%d", ifname, unit);
	if (getmacconf(mac) != SNMP_ERR_NOERROR)
		goto niret_1;
	if (findsmtslot(mac) == 0)
		goto niret_1;
	(void)sprintf(smtp->trunk, "%s", ifname);

	mac->boottime = curtime.tv_sec;

	/* Attach first phy structure */
	phy = (SMT_PHY *)Malloc(sizeof(*phy));
	mac->primary = phy;
	if (getphyconf(mac, 0) != SNMP_ERR_NOERROR)
		goto niret_2;
	if (sm_getconf(mac->name, 0, &conf) != SNMP_ERR_NOERROR)
		goto niret_2;
	phy->type = conf.type;
	phy->pctype = conf.pc_type;
	phy->bler_ts = curtime;

	mac->st_type = phy->type;
	if (bcmp((char*)&mac->addr,(char*)&smtd_null_mid,sizeof(mac->addr)))
		sm_set_macaddr(mac->name, &mac->addr);
	else {
		if (sm_stat(mac->name, 0, mac->st) != SNMP_ERR_NOERROR)
			 goto niret_2;
		mac->addr = mac->st->addr;
	}
	if (phy->pctype != getpctype(mac, unit, 0))
		sm_log(LOG_ERR, 0, "%s.0: primary PC_UNKNOWN", ifname);
	mac->phy_ct = (mac->st_type == SM_DAS) ? 2 : 1;

	smtp->st_type = phy->type;
	if (smtd_setsid(mac) != 0)
		goto niret_2;

	if (!smtp->manufset) {
		smtp->manufset = 1;
		bcopy(&conf.mnfdat, &smtp->manuf, sizeof(smtp->manuf));
	}
	if (sm_open(mac->name, FDDI_PORT_SMT, &mac->smtsock,&smtd_fdset)) {
		goto niret_2;
	}

	if (mac->statsock < 0
	    && (mac->statsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		goto niret_3;

	if (mac->phy_ct != 1) {
		/* Allocate second phy structure */
		mac->secondary = (SMT_PHY *)Malloc(sizeof(*(mac->secondary)));
		phy = mac->secondary;
		if (getphyconf(mac, 1) != SNMP_ERR_NOERROR
		    || sm_getconf(mac->name, 1, &conf) != SNMP_ERR_NOERROR) {
			dequeue_phy(mac->secondary);
			goto niret_4;
		}
		phy->type = conf.type;
		phy->pctype = conf.pc_type;
		phy->bler_ts = curtime;
		if (phy->pctype != getpctype(mac, unit, 1))
			sm_log(LOG_ERR, 0, "%s.0: secondary PC_UNKNOWN",
			       ifname);
	}

	mac->sampletime = curtime;
	mac->basetime = curtime;
	mac->btncc = curtime;
	mac->tflip = curtime.tv_sec;
	mac->tflop = curtime.tv_sec;

	enqueue_phy(mac->primary);
	enqueue_phy(mac->secondary);
	enqueue_mac(mac);
	reset_rids();
	rmt_reset(mac);
	if (sm_arm(mac->name, mac->smtsock, 0, 0) != SNMP_ERR_NOERROR) {
		sm_log(LOG_EMERG, 0, "%s.0: arm failed", mac->name);
	}
	if (mac->st_type == SM_DAS &&
	    sm_arm(mac->name, mac->smtsock, 0, 1) != SNMP_ERR_NOERROR) {
		sm_log(LOG_EMERG, 0, "%s.1: arm failed", mac->name);
	}
	return(mac);

niret_4:
	if (mac->statsock >= 0) {
		(void)close(mac->statsock);
		mac->statsock = -1;
	}
niret_3:
	sm_close(&mac->smtsock, &smtd_fdset);
niret_2:
	dequeue_phy(mac->primary);
niret_1:
	dequeue_mac(mac);
	return(0);
}

/*
 *
 */
int
upiphase(char *ifname, int unit)
{
	SMT_MAC *mac;

	if ((mac = getmacbyname(ifname, (int)unit)) != 0)
		return(SNMP_ERR_NOERROR);
	CFDEBUG((LOG_DBGCF, 0, "it is new iphase(%s%d)", ifname, unit));

	if ((mac = newiphase(ifname, unit)) == 0) {
		sm_log(LOG_ERR, 0, "newiphase for %s%d failed", ifname, unit);
		return(SNMP_ERR_NOSUCHNAME);
	}
	CFDEBUG((LOG_DBGCF, 0, "newiphase(%s%d) done", ifname, unit));

	mac->changed = 1;
	smtd_setconf(mac, 0);
	CFDEBUG((LOG_DBGCF, 0, "smtd_setconf phy0 done", ifname, unit));
	if (mac->secondary) {
		smtd_setconf(mac, 1);
		CFDEBUG((LOG_DBGCF,0,
			 "smtd_setconf phy1 done", ifname, unit));
	}
	if (smtd_sanity(smtp)) {
		sm_log(LOG_ERR,0,"sanity check for %s%d failed",ifname,unit);
		return(SNMP_ERR_NOSUCHNAME);
	}

	/* setup SRF */
	{
		LFDDI_ADDR multimid;
		bitswapcopy(&smtp->sr_mid, &multimid, sizeof(multimid));
		(void)sm_multi(mac->name, &multimid, 1, mac->smtsock);
	}

	fsq_clearlog();

	/* fire up nif for this mac */
	mac->tnn = curtime.tv_sec;
	nn_reset(mac);

	sr_event(EVNT_SMTCONF, EVTYPE_EVENT, mac);
	return(SNMP_ERR_NOERROR);
}

/*
 *
 */
int
downiphase(char *ifname, int unit)
{
	SMT_MAC *mac;
	LFDDI_ADDR multimid;

	sm_log(LOG_INFO, 0, "shutdown %s%d", ifname, unit);
	if ((mac = getmacbyname(ifname, (int)unit)) == 0) {
		sm_log(LOG_INFO,0,"%s%d was shutdown already", ifname, unit);
		return(SNMP_ERR_NOERROR);
	}

	mac->changed = 1;

	/* turn off SRF */
	bitswapcopy(&smtp->sr_mid, &multimid, sizeof(multimid));
	(void)sm_multi(mac->name, &multimid, 0, mac->smtsock);
	sr_event(EVNT_SMTCONF, EVTYPE_EVENT, mac);

	dequeue_phy(mac->primary);
	dequeue_phy(mac->secondary);

	sm_close(&mac->smtsock, &smtd_fdset);
	if (mac->statsock >= 0) {
		(void)close(mac->statsock);
		mac->statsock = -1;
	}
	dequeue_mac(mac);
	(void)smtd_sanity(smtp);

	return(SNMP_ERR_NOERROR);
}
