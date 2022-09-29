/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.16 $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smtd.h>
#include <cf.h>

int
phy_sanity(SMT_MAC *mac, int phyidx)
{
	char *cp;
	SMT_PHY *phy;
	SMT_PATH *path;
	enum fddi_pc_type pctype;

	if (!mac)
		return(0);

	phy = phyidx == 0 ? mac->primary : mac->secondary;
	if (!phy)
		return(0);

	switch (mac->st_type) {
	case SAC:
		if (phy->pctype != PC_M) {
			sm_log(LOG_INFO, 0, "sanity: %s.%d pctype fixed to M",
				mac->name, phyidx);
			phy->pctype = PC_M;
		}
		phy->pathrequested = PATHTYPE_PRIMARY;
		break;
	case SAS:
		if (phy->pctype != PC_S) {
			sm_log(LOG_INFO, 0, "sanity: %s.%d pctype fixed to S",
				mac->name, phyidx);
			phy->pctype = PC_S;
		}
		phy->pathrequested = PATHTYPE_PRIMARY;
		break;
	case SM_DAS:
		pctype = phyidx == 0 ? PC_B : PC_A;
		if (phy->pctype != pctype) {
			sm_log(LOG_INFO, 0, "sanity: %s.%d pctype fixed to %s",
				mac->name, phyidx, pctype==PC_A?"PC_A":"PC_B");
			phy->pctype = pctype;
		}
		if (pctype == PC_A)
			phy->pathrequested = PATHTYPE_SECONDARY;
		else
			phy->pathrequested = PATHTYPE_PRIMARY;
		break;
	case DM_DAS:
		cp = mac->name;
		while (isalpha(*cp))
			cp++;
		pctype = *cp == '0' ? PC_B : PC_A;
		if (phy->pctype != pctype) {
			sm_log(LOG_INFO, 0,
			       "sanity: %s.%d pctype fixed to %s",
			       mac->name, phyidx,
			       pctype==PC_A?"PC_A":"PC_B");
			phy->pctype = pctype;
		}
		if (pctype == PC_A)
			phy->pathrequested = PATHTYPE_SECONDARY;
		else
			phy->pathrequested = PATHTYPE_PRIMARY;
		break;
	default:
		sm_log(LOG_ERR,0,
		       "sanity: unsupported MACtype=%d",mac->st_type);
		return(-1);
	}
	phy->pathavail = mac->pathavail;

        mac->msloop_stat = MSL_UNKNOWN;
        mac->root_dnaport = PC_UNKNOWN;
        mac->root_curpath = PATHTYPE_UNKNOWN;

	sm_log(LOG_DEBUG, 0, "sanity: phy->pathreq = %x", phy->pathrequested);
	/*
	 * check paths
	 * trace must be performed to get real numbers.
	 */
	path = &phy->path;
	path->pathclass = &smtp->pathclass[1];
	path->type = phy->pathrequested;
	path->order = PO_ASCENDING;
	path->latency = 1;
	path->tracestatus = PT_NOTRACE;
	path->sba = 0;
	path->sbaoverhead = 0;
	path->status = phy->pcm_state;
	path->rtype = PATH_PORTTYPE;
	path->rmode = 0;

	path->pathclass->maxtrace = 0;
	path->pathclass->tvx_lobound = MAX(mac->tvx,
					   path->pathclass->tvx_lobound);
	path->pathclass->tmax_lobound = MAX(mac->tmax,
					    path->pathclass->tmax_lobound);

	phy->macplacement = 0;
	phy->phyplacement = 0;
	return(0);
}

int
mac_sanity(SMT_MAC *mac)
{
	char c;
	SMT_PATH *path;

	if (!mac)
		return(0);

	if (mac->fsc & ~(0x3f)) {
		mac->fsc &= 0x3f;
		sm_log(LOG_INFO, 0, "sanity: %s's fsc fixed to %x",
				mac->name, mac->fsc);
	}

	if (mac->bridge & ~(BRIDGE_TYPE0|BRIDGE_TYPE1)) {
		mac->bridge &= (BRIDGE_TYPE0|BRIDGE_TYPE1);
		sm_log(LOG_INFO, 0, "sanity: %s's bridge type fixed to %x",
				mac->name, mac->bridge);
	}
	/*
	 * - PATH counts only output.
	 * - Since sgi do not support concentrator no LOCAL path exists.
	 * - Even in WRAP case sgi's mac directly output to phy.
	 *
	 * - But WRAP case should be handled specially, so
	 * - theoretically there could be one and only LOCAL-PATH
	 * - which is concentrator input iff it is implemented as so.
	 *
	 * SO FORGET ABOUT LOCAL_PATH!!!!!!
	 *
	 * - For single attached or tree, iff any attachment exists,
	 * - then it is always primary.
	 */
	switch (mac->st_type) {
	case SAC:
	case SAS:
		mac->pathavail = PATHTYPE_ISOLATED|PATHTYPE_PRIMARY;
		mac->pathrequested = PATHTYPE_PRIMARY;
		break;
	case SM_DAS:
		mac->pathavail =
		    PATHTYPE_ISOLATED|PATHTYPE_PRIMARY|PATHTYPE_SECONDARY;
		mac->pathrequested = PATHTYPE_PRIMARY|PATHTYPE_SECONDARY;
		break;
	case DM_DAS:
		mac->pathavail =
		    PATHTYPE_ISOLATED|PATHTYPE_PRIMARY|PATHTYPE_SECONDARY;
		c = mac->name[strlen(mac->name) - 1] - '0';
		mac->pathrequested = (c&1)?PATHTYPE_PRIMARY:PATHTYPE_SECONDARY;
		break;
	default:
		sm_log(LOG_ERR, 0, "sanity: %s unsupported mac type = %d",
				mac->name, mac->st_type);
		return(-1);
	}
	mac->curpath = PATHTYPE_UNKNOWN;

	if (mac->tmax < MAX(FDDITOBCLK(FDDI_MIN_T_MAX),mac->treq)) {
		mac->tmax = MAX(FDDITOBCLK(FDDI_MIN_T_MAX),mac->treq);
		sm_log(LOG_INFO, 0, "sanity: %s tmax fixed to %f",
		       mac->name, FROMBCLK(mac->tmax));
	}

	if (mac->tmin > FDDITOBCLK(FDDI_MIN_T_MIN)) {
		mac->tmin = FDDITOBCLK(FDDI_MIN_T_MIN);
		sm_log(LOG_INFO, 0, "sanity: %d tmin fixed to %f",
		       mac->name, FROMBCLK(mac->tmin));
	}

	if ((mac->curfsc&mac->fsc) != mac->curfsc) {
		sm_log(LOG_INFO, 0, "sanity: %s's curfsc = %d",
				mac->name, mac->curfsc);
	}

	/*
	 * check paths
	 * trace must be performed to get real numbers.
	 */
	mac->pathplacement = 0;
	path = &mac->path;
	path->pathclass = &smtp->pathclass[1];
	path->type = mac->pathrequested;
	path->order = PO_ASCENDING;
	path->latency = 0;
	path->tracestatus = PT_NOTRACE;
	path->sba = 0;
	path->sbaoverhead = 0;
	path->status = 0;
	path->rtype = PATH_MACTYPE;
	path->rmode = 0;

	phy_sanity(mac, 0);
	phy_sanity(mac, 1);

	return(0);
}

int
smtd_sanity(SMTD *smtp)
{
	int i;
	SMT_MAC *mac;
	int mac_ct, phy_ct;

	/* check mac & phy cnt */
	mac_ct = phy_ct = 0;
	mac = smtp->mac;
	while (mac) {
		mac_ct++;
		phy_ct += mac->phy_ct;
		if ((mac->primary) && (mac->phy_ct < 1)) {
			dequeue_phy(mac->primary);
			mac->primary = 0;
			sm_log(LOG_INFO, 0, "sanity: %s: primary salvaged",
						mac->name);
		}
		if ((mac->secondary) && (mac->phy_ct < 2)) {
			dequeue_phy(mac->secondary);
			mac->secondary = 0;
			sm_log(LOG_INFO, 0, "sanity: %s: secondary salvaged",
						mac->name);
		}
		mac = mac->next;
	}
	if (smtp->mac_ct != mac_ct) {
		sm_log(LOG_INFO, 0, "sanity: mac_ct(%d) fixed to %d",
				smtp->mac_ct, mac_ct);
		smtp->mac_ct = mac_ct;
	}
	if (smtp->phy_ct != phy_ct) {
		sm_log(LOG_INFO, 0, "sanity: phy_ct(%d) fixed to %d",
				smtp->phy_ct, phy_ct);
		smtp->phy_ct = phy_ct;
	}

	/* Check station type */
	switch (smtp->st_type) {
		case SAC:
			smtp->nodeclass = SMT_CONCENTRATOR;
			smtp->master_ct = smtp->phy_ct - 2;
			break;

		case SAS:
		case SM_DAS:
		case DM_DAS:
			smtp->nodeclass = SMT_STATION;
			smtp->master_ct = 0;
			break;

		default:
			sm_log(LOG_ERR, 0,
				"sanity: unsupported station type=%d",
				smtp->st_type);
			return(-1);
	}
	smtp->nonmaster_ct = smtp->phy_ct - smtp->master_ct;

	/* check version */
	smtp->vers_hi = SMT_VER_HI;
	smtp->vers_lo = SMT_VER_LO;
	if ((smtp->vers_op > SMT_VER_HI) || (smtp->vers_op < SMT_VER_LO)) {
		smtp->vers_op = smtp->vers_hi;
		sm_log(LOG_INFO, 0,"sanity: op_vers fixed to %d",
				smtp->vers_op);
	}

	/* Station Config Grp */
	if (smtp->pathavail & ~(PATHTYPE_ISOLATED|PATHTYPE_PRIMARY|
				PATHTYPE_SECONDARY|PATHTYPE_LOCAL))
		sm_log(LOG_INFO, 0, "sanity: pathavail = %x",
		       smtp->pathavail);

	smtp->pathclass[0].rid = 1;
	smtp->pathclass[1].rid = 2;

	reset_rids();

	mac = smtp->mac;
	while (mac) {
		if (mac_sanity(mac))
			return(-1);
		smtp->pathavail |= mac->pathavail;
		mac = mac->next;
	}

	/* successful sanity check */
	return(0);
}
