/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.16 $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smtd.h>
#include <cf.h>

static void conf_phy(FILE*, SMT_MAC*, int);

#define GETVAL(min,max,def) getval(val,"conf file",name, min,max,def)
#define GETINT(min,max,def) getint(val,"conf file",name, min,max,def)

/*
 *
 */
void
conf_station(FILE *fp)
{
	char **lp;
	char val[CFBUFSIZE];
	char line[LINESIZE];
	char name[CFBUFSIZE];
	char equal[CFBUFSIZE];

	/* smtp->sid		handled by SANITY CHECK */
	/* smtp->st_type	handled by SANITY CHECK */
	smtp->srf_on = 1;
	smtp->trace_on = 1;
	smtp->reportlimit = SMT_DEFAULT_RPORTLIMIT;
	/* smtp->phy_ct		handled by SANITY CHECK */
	/* smtp->mac_ct		handled by SANITY CHECK */
	/* smtp->master_ct	handled by SANITY CHECK */
	/* smtp->pathavail	handled by SANITY CHECK */
	smtp->conf_cap = SMT_WRAPAB;
	smtp->conf_policy = 0;
	smtp->conn_policy = SMT_REJECT_MM;
	smtp->t_notify = T_NOTIFY;
	smtp->pmf_on = 0;
	smtp->vers_op = SMT_VER;
	smtp->vers_hi = SMT_VER_HI;
	smtp->vers_lo = SMT_VER_LO;
	(void)fddi_aton(SMT_SRF_DA, &smtp->sr_mid);

	while (lp = getline(line, sizeof(line), fp)) {
		skipblank(lp);
		if ((**lp == '#') || (**lp == '\n')) {
			continue;
		}

		if (sscanf(*lp, "%s%s%s", name, equal, val) != 3) {
			if (!strcmp(name, "ENDSTATION"))
				break;
			sm_log(LOG_ERR, 0, "conf: bad format: %s", *lp);
			break;
		}
		if (!strcmp(name, "ENDSTATION"))
			break;

		if ((strlen(equal) != 1) || (equal[0] != '=')) {
			sm_log(LOG_ERR, 0, "conf: bad format: %s", *lp);
			break;
		}

		if (!strcmp(name, "StationId")) {
			if (atosid(val, &smtp->sid)) {
				sm_log(LOG_ERR, 0, "conf: bad sid: %s", *lp);
				break;
			}
		} else if (!strcmp(name, "sr_mid")) {
			if (!fddi_aton(val, &smtp->sr_mid)) {
				sm_log(LOG_ERR, 0,
					"conf: bad SRF address: %s", *lp);
				break;
			}
		} else if (!strcmp(name, "StationType")) {
			smtp->st_type = (enum fddi_st_type)GETINT(0,3,0);
			if (smtp->st_type == SAC)
				smtp->nodeclass = SMT_CONCENTRATOR;
			else
				smtp->nodeclass = SMT_STATION;
		} else if (!strcmp(name, "phy_ct")) {
			smtp->phy_ct = GETVAL(1,0,0);	/* XXX */
		} else if (!strcmp(name, "mac_ct")) {
			smtp->mac_ct = GETVAL(1,0,0);	/* XXX */
		} else if (!strcmp(name, "master_ct")) {
			smtp->master_ct = GETVAL(1,0,0);    /* XXX */
		} else if (!strcmp(name, "pathavail")) {
			smtp->pathavail = GETVAL(1,0,0);    /* XXX */
		} else if (!strcmp(name, "conf_cap")) {
			smtp->conf_cap = GETVAL(1,0,SMT_WRAPAB); /* XXX */
		} else if (!strcmp(name, "conf_policy")) {
			smtp->conf_policy = GETVAL(1,0,0);  /* XXX */
		} else if (!strcmp(name, "conn_policy")) {
			smtp->conn_policy = GETVAL(1,0,	/* XXX */
						   SMT_REJECT_MM);
		} else if (!strcmp(name, "reportlimit")) {
			smtp->reportlimit = GETVAL(1,0,	/* XXX */
						   SMT_DEFAULT_RPORTLIMIT);
		} else if (!strcmp(name, "t_notify")) {
			smtp->t_notify = GETVAL(T_NOTIFY_MIN, T_NOTIFY_MAX,
						T_NOTIFY);
		} else if (!strcmp(name, "srf_on")) {
			smtp->srf_on = GETVAL(0,1,1);
		} else if (!strcmp(name, "pmf_on")) {
			smtp->pmf_on = GETVAL(0,1,0);
		} else if (!strcmp(name, "trace_on")) {
			smtp->trace_on = GETVAL(0,1,1);
		} else if (!strcmp(name, "vers_op")) {
			smtp->vers_op = GETVAL(0,SMT_VER,SMT_VER);
		} else if (!strcmp(name, "user_data")) {
			int len;

			*lp += strlen(name);
			skipblank(lp);
			*lp += strlen(equal);
			skipblank(lp);
			len = strlen(*lp);
			if (len > sizeof(smtp->userdata)) {
				len = sizeof(smtp->userdata);
				sm_log(LOG_ERR, 0,
				       "conf: user data too long");
			}
			bcopy(*lp, &smtp->userdata, len);
		} else {
			sm_log(LOG_ERR, 0,
			       "conf: bad station entry: %s", name);
			break;
		}
	}

	CFDEBUG((LOG_DBGCF, 0, "conf_station..."));
}

/*
 *
 */
static void
conf_phy(FILE *fp, SMT_MAC *mac, int phyidx)
{
	char line[LINESIZE];
	char name[CFBUFSIZE];
	char val[CFBUFSIZE];
	char equal[CFBUFSIZE];
	char **lp;
	SMT_PHY *phy;
	int err = 0;
#define USEC2MSEC(x) ((x)/1000)

	if (phyidx == 1)
		phy = mac->primary;
	else if (phyidx == 2)
		phy = mac->secondary;
	else {
		sm_log(LOG_INFO, 0, "conf: ignored 3rd phy for %s",
		       mac->name);
		return;
	}
	if (phy == 0) {
		phy = (SMT_PHY *)Malloc(sizeof(*phy));

		/* phy->type		handled by SANITY CHECK */
		/* phy->pctype		handled by SANITY CHECK */
		phy->ler_cutoff = SMT_LER_CUT_DEF;
		phy->ler_alarm = SMT_LER_ALM_DEF;
		phy->tb_max = USEC2MSEC(SMT_TB_MAX);
		phy->debug = 2;
		phy->ip_pri = 1;
		phy->pcm_tgt = PCM_CMT;
		phy->imax = USEC2MSEC(SMT_I_MAX);
		phy->ipolicy = 1;
		phy->fotx = PF_MULTI;
		phy->phy_connp = PC_MAC_LCT|PC_MAC_PLACEMENT;
		phy->maint_ls = PCM_QLS;
		phy->ler_estimate = 16;
		goto phy_ret;
	}

	while (lp = getline(line, sizeof(line), fp)) {
		skipblank(lp);
		if ((**lp == '#') || (**lp == '\n')) {
			continue;
		}

		if (sscanf(*lp, "%s%s%s", name, equal, val) != 3) {
			if (!strcmp(name, "ENDPHY"))
				break;
			sm_log(LOG_ERR, 0, "conf: bad format: %s", *lp);
			err = 1;
			break;
		}
		if (!strcmp(name, "ENDPHY"))
			break;

		if ((strlen(equal) != 1) || (equal[0] != '=')) {
			sm_log(LOG_ERR, 0, "conf: bad format: %s", *lp);
			err = 1;
			break;
		}

		if (!strcmp(name, "type")) {
			phy->type = (enum fddi_st_type)GETINT(0,3,0);
		} else if (!strcmp(name, "pctype")) {
			phy->pctype = (enum fddi_pc_type)GETINT(0,3,0);
		} else if (!strcmp(name, "tb_max")) {
			phy->tb_max = GETVAL(0,100*USEC2MSEC(SMT_TB_MAX),
					     SMT_TB_MAX);
		} else if (!strcmp(name, "ler_cutoff")) {
			phy->ler_cutoff = GETVAL(SMT_LER_CUT_MIN,
						 SMT_LER_CUT_MAX,
						 SMT_LER_CUT_DEF);
		} else if (!strcmp(name, "ler_alarm")) {
			phy->ler_alarm = GETVAL(SMT_LER_ALM_MIN,
						SMT_LER_ALM_MAX,
						SMT_LER_ALM_DEF);
		} else if (!strcmp(name, "debug")) {
			phy->debug = GETVAL(0,99,2);
		} else if (!strcmp(name, "ip_pri")) {
			phy->ip_pri = GETVAL(0,7,1);
		} else if (!strcmp(name, "pcm_tgt")) {
			phy->pcm_tgt = (enum pcm_tgt)GETINT(PCM_CMT,
							    PCM_CMT,PCM_CMT);
		} else if (!strcmp(name, "imax")) {
			phy->imax = GETVAL(USEC2MSEC(SMT_I_MAX),
					   USEC2MSEC(SMT_I_MAX),
					   USEC2MSEC(SMT_I_MAX));
		} else if (!strcmp(name, "ipolicy")) {
			phy->ipolicy = GETVAL(0,1,1);
		} else if (!strcmp(name, "fotx")) {
			phy->fotx = (PMD_FOTX)GETINT(0,3,PF_MULTI);
		} else if (!strcmp(name, "conn_policy")) {
			phy->phy_connp = GETVAL(1,0,
						PC_MAC_LCT|PC_MAC_PLACEMENT);
		} else {
			sm_log(LOG_ERR, 0, "conf: bad entry: %s", name);
			err = 1;
			break;
		}
	}

	if (err) {
		CFDEBUG((LOG_DBGCF, 0, "conf_phy(%s):%d... failed",
					mac->name, phy->pctype));
		dequeue_phy(phy);
		return;
	}

phy_ret:
	if (phyidx == 1)
		mac->primary = phy;
	else if (phyidx == 2)
		mac->secondary = phy;

	CFDEBUG((LOG_DBGCF, 0, "conf_phy(%s)... done", mac->name));
}


void
conf_mac(FILE *fp)
{
	char line[LINESIZE];
	char name[CFBUFSIZE];
	char val[CFBUFSIZE];
	char equal[CFBUFSIZE];
	char **lp;
	int nameset;
	SMT_MAC *mac;
	int err = 0;
	int phyidx = 0;

	mac = (SMT_MAC *)Malloc(sizeof(*mac));
	/* Fields which must be given */
	nameset = 0;

	/* Fields which are handled by SANITY CHECK */
	/* mac->phy_ct		handled by SANITY CHECK */
	/* mac->pathavail	handled by SANITY CHECK */
	/* mac->pathrequested	handled by SANITY CHECK */
	/* mac->addr		handled by SANITY CHECK */

	mac->maxflops = 1;
	mac->fsc = 0;
	mac->bridge = 0;
	mac->treq = FDDITOBCLK(FDDI_DEF_T_REQ);
	mac->tmax = FDDITOBCLK(FDDI_MIN_T_MAX);
	mac->tmin = FDDITOBCLK(FDDI_MIN_T_MIN);
	mac->tvx = FDDITOBCLK(FDDI_DEF_TVX);
	mac->tmax_lobound = FDDITOBCLK(FDDI_MIN_T_MAX);
	mac->tvx_lobound = FDDITOBCLK(FDDI_MIN_TVX);
	mac->fr_threshold = 15;
	mac->fnc_threshold = 15;

	mac->phy_ct = 2;
	conf_phy(fp, mac, 1);
	conf_phy(fp, mac, 2);

	while (lp = getline(line, sizeof(line), fp)) {
		skipblank(lp);
		if ((**lp == '#') || (**lp == '\n')) {
			continue;
		}

		if (sscanf(*lp, "%s%s%s", name, equal, val) != 3) {
			if (!strcmp(name, "ENDMAC"))
				break;
			if (!strcmp(name, "PHY:")) {
				phyidx++;
				conf_phy(fp, mac, phyidx);
				continue;
			}
			err = 1;
			sm_log(LOG_ERR, 0, "conf: bad format: %s", *lp);
			break;
		}

		if (!strcmp(name, "ENDMAC"))
			break;
		if (!strcmp(name, "PHY:")) {
			phyidx++;
			conf_phy(fp, mac, phyidx);
			continue;
		}

		if ((strlen(equal) != 1) || (equal[0] != '=')) {
			err = 1;
			sm_log(LOG_ERR, 0, "conf: bad format: %s", *lp);
			break;
		}

		if (!strcmp(name, "addr")) {
			if (fddi_aton(val, &mac->addr) == 0) {
				sm_log(LOG_ERR, 0,
					"conf: bad mac addr: %s", *lp);
				err = 1;
				break;
			}
		} else if (!strcmp(name, "name")) {
			strcpy(mac->name, val);
			nameset = 1;
		} else if (!strcmp(name, "phy_ct")) {
			mac->phy_ct = GETVAL(1,2,1);
		} else if (!strcmp(name, "fsc")) {
			mac->fsc = GETVAL(0,0x3f,0);
		} else if (!strcmp(name, "bridge")) {
			mac->bridge = GETVAL(0,3,0);
		} else if (!strcmp(name, "pathavail")) {
			mac->pathavail = GETVAL(1,0,0);	/* XXX */
		} else if (!strcmp(name, "pathrequested")) {
			mac->pathrequested = GETVAL(1,0,0); /* XXX */
		} else if (!strcmp(name, "treq")) {
			mac->treq = TOBCLK(GETVAL(FROMBCLK(FDDI_MIN_T_MIN),
						  FROMBCLK(-0x7fffff),
						  FROMBCLK(FDDI_DEF_T_REQ)));
		} else if (!strcmp(name, "tmax")) {
			mac->tmax = TOBCLK(GETVAL(FROMBCLK(FDDI_MIN_T_MAX),
						  FROMBCLK(-0x7fffff),
						  FROMBCLK(FDDI_MIN_T_MAX)));
		} else if (!strcmp(name, "tmin")) {
			mac->tmin = TOBCLK(GETVAL(FROMBCLK(FDDI_MIN_T_MIN),
						  FROMBCLK(FDDI_MIN_T_MAX),
						  FROMBCLK(FDDI_MIN_T_MIN)));
		} else if (!strcmp(name, "tvx")) {
			mac->tvx = TOBCLK(GETVAL(FROMBCLK(FDDI_MIN_TVX),
						 FROMBCLK(FDDI_MAX_TVX),
						 FROMBCLK(FDDI_DEF_TVX)));
		} else if (!strcmp(name, "tmax_lobound")) {
			mac->tmax_lobound = TOBCLK(GETVAL(FROMBCLK(FDDI_MIN_T_MAX),
							  FROMBCLK(-0x7fffff),
							  FROMBCLK(FDDI_MIN_T_MAX)));
		} else if (!strcmp(name, "tvx_lobound")) {
			mac->tvx_lobound = TOBCLK(GETVAL(FROMBCLK(FDDI_MIN_TVX),
							 FROMBCLK(FDDI_MAX_TVX),
							 FROMBCLK(FDDI_DEF_TVX)));
		} else if (!strcmp(name, "fr_threshold")) {
			mac->fr_threshold = GETVAL(1,0,15); /* XXX */
		} else if (!strcmp(name, "fnc_threshold")) {
			mac->fnc_threshold = GETVAL(1,0,15); /* XXX */
		} else if (!strcmp(name, "sttype")) {
			mac->st_type = (enum fddi_st_type)GETINT(1,0,0);
		} else if (!strcmp(name, "maxflops")) {
			mac->maxflops = GETVAL(1,0,1);
		} else {
			sm_log(LOG_ERR, 0, "conf: bad entry(%s)", name);
			err = 1;
			break;
		}
	}

	if (!nameset) {
		sm_log(LOG_ERR, 0, "conf: bad MAC entry: name not set");
		err = 1;
	}
	if (err) {
		CFDEBUG((LOG_DBGCF, 0, "conf_mac(%s)... failed", mac->name));
		dequeue_mac(mac);
		return;
	}

	if ((mac->phy_ct >= 1) && (mac->primary == 0)) {
	    sm_log(LOG_ERR, 0, "conf: primary missing for %s",mac->name);
	}
	if ((mac->phy_ct >= 2) && (mac->secondary == 0)) {
	    sm_log(LOG_ERR, 0, "conf: secondary missing for %s",mac->name);
	}

	mac->next = smtd_config;
	smtd_config = mac;
	CFDEBUG((LOG_DBGCF, 0, "conf_mac(%s)... done", mac->name));
}
