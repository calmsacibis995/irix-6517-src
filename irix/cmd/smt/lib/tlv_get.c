/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.13 $
 */

#include <sys/types.h>
#include <sys/time.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smtd.h>
#include <smtd_parm.h>
#include <smtd_fs.h>
#include <smtd_svc.h>
#include <tlv_get.h>

/*
 *
 *	fddiSMT GET operation.
 *	FORMAT: TLV
 *	return REASON_CODE.
 *	All the validity check is done already by caller.
 */
static int
rstr(char **dst, u_long *dlen, u_long type, char *val, u_long len, u_long mask)
{
	if (tlv_build_tl(dst, dlen, type, len))
		return(RC_PARSE);
	if (tlv_build_string(dst, dlen, val, len, mask))
		return(RC_PARSE);
	return(RC_SUCCESS);
}

static int
ristr(char **dst, u_long *dlen, u_long type, char *val, u_long len, u_long mask, u_long rid)
{
	if (tlv_build_tl(dst, dlen, type, len+sizeof(long)))
		return(RC_PARSE);
	if (tlv_build_int(dst, dlen, rid, 2))
		return(RC_PARSE);
	if (tlv_build_string(dst, dlen, val, len, mask))
		return(RC_PARSE);
	return(RC_SUCCESS);
}

static int
rint(char **dst, u_long *dlen, u_long type, u_long val, int valsize)
{
	if (tlv_build_tl(dst, dlen, type, sizeof(int)))
		return(RC_PARSE);
	if (tlv_build_int(dst, dlen, val, valsize))
		return(RC_PARSE);
	return(RC_SUCCESS);
}

static int
riint(char **dst, u_long *dlen, u_long type, u_long val, int valsize, u_long rid)
{
	if (tlv_build_tl(dst, dlen, type, (sizeof(long) * 2)))
		return(RC_PARSE);
	if (tlv_build_int(dst, dlen, rid, 2))
		return(RC_PARSE);
	if (tlv_build_int(dst, dlen, val, valsize))
		return(RC_PARSE);
	return(RC_SUCCESS);
}

#define RSEQ(x, y) return(rstr(dst,dlen,type,(char *)&(x),sizeof(x),(y)))
#define RISEQ(s, x, y) return(ristr(dst,dlen,type, \
				    (char*)&(s->x),sizeof(s->x),y,s->rid))
#define RISTR(s, x, y, z) return(ristr(dst,dlen,type, \
				       (char*)&(s->x),(z),(y),s->rid))

#define	RINT(x)		return(rint(dst, dlen, type, (x), 4))
#define	RSHORT(x)	return(rint(dst, dlen, type, (x), 2))
#define	RCHAR(x)	return(rint(dst, dlen, type, (x), 1))

#define	RIINT(s,x)	return(riint(dst, dlen, type, s->x, 4, s->rid))
#define	RISHORT(s,x)	return(riint(dst, dlen, type, s->x, 2, s->rid))
#define	RICHAR(s,x)	return(riint(dst, dlen, type, s->x, 1, s->rid))

#define RTS(x) { \
	(x).tv_sec = htonl((x).tv_sec); \
	(x).tv_usec = htonl((x).tv_usec); \
	RSEQ((x), TLV_NOALIGN); }

#define RITS(s, x) { \
	(s->x).tv_sec = htonl((s->x).tv_sec); \
	(s->x).tv_usec = htonl((s->x).tv_usec); \
	RISEQ(s, x, TLV_NOALIGN); }

#define RGROUP(proc, n) { \
		register u_long i; \
		for (i = 1; i <= n && *dlen > 0; i++) \
			if (proc(entity, type+i, dst, dlen) != RC_SUCCESS) \
				return(RC_PARSE); \
		return(RC_SUCCESS); }

#define	CHECK_DST(x,y) { \
	off = sizeof(x); \
	if (tlv_build_tl(dst, dlen, (y), off)) return(RC_PARSE); \
	pu = (PARM_UNION *)(*dst); }

/*
 *
 */
static int
getSMT(void *entity, u_long type, char **dst, u_long *dlen)
{
	register int obj = typetoobj(type);

	TLVDEBUG((LOG_DBGTLV, 0, "getSMT: obj=%d", obj));

	switch(obj) {
		/* fddiSMTStationidGrp */
		case 10: RGROUP(getSMT, 7);
		case 11: RSEQ(smtp->sid, TLV_NOALIGN);
		case 13: RSHORT(smtp->vers_op);
		case 14: RSHORT(smtp->vers_hi);
		case 15: RSHORT(smtp->vers_lo);
		case 16: RSEQ(smtp->manuf, TLV_NOALIGN);
		case 17: RSEQ(smtp->userdata, TLV_NOALIGN);

		/* fddiSMTStationConfigGrp */
		case 20: RGROUP(getSMT, 10);
		case 21: RCHAR(smtp->mac_ct);
		case 22: RCHAR(smtp->nonmaster_ct);
		case 23: RCHAR(smtp->master_ct);
		case 24: RCHAR(smtp->pathavail);
		case 25: RSHORT(smtp->conf_cap);
		case 26: RSHORT(smtp->conf_policy);
		case 27: RSHORT(smtp->conn_policy);
		case 28: RINT(smtp->reportlimit);
		case 29: RSHORT(smtp->t_notify);
		case 30: RSHORT(smtp->srf_on);

		/* fddiSMTStatusGrp */
		case 40: RGROUP(getSMT, 4);
		case 41: RCHAR(smtp->ecmstate);
		case 42: RCHAR(smtp->cf_state);
		case 43: RCHAR(smtp->hold_state);
		case 44: RCHAR(smtp->rd_flag);

		/* fddiSMTMIBOperationGrp */
		case 50: RGROUP(getSMT, 4);
		case 51: RTS(smtp->msg_ts);
		case 52: RTS(smtp->trans_ts);
		case 53: {
			setcount_t *ssc, tsc;

			ssc = &smtp->setcount;
			tsc.sc_cnt = htonl(ssc->sc_cnt);
			tsc.sc_ts.tv_sec = htonl(ssc->sc_ts.tv_sec);
			tsc.sc_ts.tv_usec = htonl(ssc->sc_ts.tv_usec);
			RSEQ(tsc, TLV_NOALIGN);
		}
		case 54: RSEQ(smtp->last_sid, TLV_NOALIGN);

		case 70: { /* PTYPE_PD_EVNT */
			u_long i;
			SMT_PHY *phy;

			if (tlv_build_tl(dst, dlen, PTYPE_PD_EVNT, 0))
				return(RC_PARSE);

			/* cf_state */
			if (getSMT(entity, 0x102a, dst, dlen) != RC_SUCCESS)
				return(RC_PARSE);

			phy = smtp->phy;
			i = 1;
			while (phy) {
				tlv_build_tl(dst, dlen, 0x3212, 12);
				/* path index */
				tlv_build_int(dst, dlen, i, 2);
				/* port */
				tlv_build_short(dst, dlen, 4, 2);
				tlv_build_short(dst, dlen, phy->rid, 2);
				/* mac */
				tlv_build_short(dst, dlen, 2, 2);
				tlv_build_short(dst, dlen, phy->macplacement,2);
				i++;
				phy = phy->next;
			}
			return(RC_SUCCESS);
		}
	}
	return(RC_PARSE);
}

/*
 * entity points to mac.
 */
static int
getMAC(void *entity, u_long type, char **dst, u_long *dlen)
{
	u_long off;
	register PARM_UNION *pu;
	register int obj = typetoobj(type);
	register SMT_MAC *mac = (SMT_MAC *)entity;

	TLVDEBUG((LOG_DBGTLV, 0, "getMAC: obj=%d", obj));

	switch(obj) {
		/* MAC capability Grp */
		case 10: RGROUP(getMAC, 4);
		case 11: RISHORT(mac, fsc);
		case 12: RISHORT(mac, bridge);
		case 13: RIINT(mac, tmax_lobound);
		case 14: RIINT(mac, tvx_lobound);

		/* MAC Config Grp */
		case 20: RGROUP(getMAC, 13);
		case 22: RICHAR(mac, pathavail);
		case 23: RICHAR(mac, curpath);
		case 24: RISEQ(mac, una, TLV_PREALIGN);
		case 25: RISEQ(mac, dna, TLV_PREALIGN);
		case 26: RISEQ(mac, old_una, TLV_PREALIGN);
		case 27: RISEQ(mac, old_dna, TLV_PREALIGN);
		case 28: RICHAR(mac, rootconcentrator);
		case 29: RISHORT(mac, dupa_test);
		case 32: RICHAR(mac, pathrequested);
		case 33: RISHORT(mac, dnaport);

		/* MAC Address Grp */
		case 40: RGROUP(getMAC, 5);
		case 41: RISTR(mac,addr, TLV_PREALIGN, 6);
		case 42: RISTR(mac,l_alias[0], TLV_POSTALIGN,mac->la_ct*6);
		case 43: RISTR(mac,s_alias[0], TLV_POSTALIGN,mac->sa_ct*6);
		case 44: RISTR(mac,l_grpaddr[0],TLV_POSTALIGN,mac->lg_ct*6);
		case 45: RISTR(mac,s_grpaddr[0],TLV_POSTALIGN,mac->sg_ct*6);

		/* MAC Operation Grp */
		case 50: RGROUP(getMAC, 13);
		case 51: RIINT(mac, treq);
		case 52: RIINT(mac, tneg);
		case 53: RIINT(mac, tmax);
		case 54: RIINT(mac, tvx);
		case 55: RIINT(mac, tmin);
		case 56: RIINT(mac, pri0);
		case 57: RIINT(mac, pri1);
		case 58: RIINT(mac, pri2);
		case 59: RIINT(mac, pri3);
		case 60: RIINT(mac, pri4);
		case 61: RIINT(mac, pri5);
		case 62: RIINT(mac, pri6);
		case 63: RISHORT(mac, curfsc);

		/* MAC Counter Grp */
		case 70: RGROUP(getMAC, 4);
		case 71: RIINT(mac, frame_ct);
		case 72: RIINT(mac, recv_ct);
		case 73: RIINT(mac, xmit_ct);
		case 74: RIINT(mac, token_ct);

		/* MAC Error cnt Grp */
		case 80: RGROUP(getMAC, 6);
		case 81: RIINT(mac, err_ct);
		case 82: RIINT(mac, lost_ct);
		case 83: RIINT(mac, tvxexpired_ct);
		case 84: RIINT(mac, notcopied_ct);
		case 85: RISHORT(mac, late_ct);
		case 86: RIINT(mac, ringop_ct);

		/* MAC Frame Condition Grp */
		case 90: RGROUP(getMAC, 6);
		case 91: RIINT(mac, bframe_ct);
		case 92: RIINT(mac, berr_ct);
		case 93: RIINT(mac, blost_ct);
		case 94: RITS(mac, basetime);
		case 95: RISHORT(mac, fr_threshold);
		case 96: RISHORT(mac, fr_errratio);

		/* MAC Not Copied Condition Grp */
		case 100: RGROUP(getMAC, 5);
		case 101: RIINT(mac, bnc_ct);
		case 102: RITS(mac, btncc);
		case 103: RISHORT(mac, fnc_threshold);
		case 104: RISHORT(mac, brcv_ct);
		case 105: RISHORT(mac, nc_ratio);

		/* MAC status Grp */
		case 110: RGROUP(getMAC, 6);
		case 111: RICHAR(mac, rmtstate);
		case 112: RICHAR(mac, da_flag);
		case 113: RICHAR(mac, unada_flag);
		case 114: RICHAR(mac, frm_errcond);
		case 115: RICHAR(mac, frm_nccond);
		case 116: RISHORT(mac, llcavail);

		/* MAC Root MAC Status Grp */
		case 120: RGROUP(getMAC, 3);
		case 121: RICHAR(mac, msloop_stat);
		case 122: RICHAR(mac, root_dnaport);
		case 123: RICHAR(mac, root_curpath);

		case 140: /* PTYPE_DUPA_COND */
			CHECK_DST(PARM_DUPA_COND, PTYPE_DUPA_COND);
			pu->dupa_cond.cs = htons(mac->dupa_flag?1:0);
			pu->dupa_cond.macidx = htons(mac->rid);
			pu->dupa_cond.dupa = mac->addr;
			pu->dupa_cond.una_dupa = mac->una;
			*dlen -= off; *dst += off;
			return(RC_SUCCESS);

		case 141: /* PTYPE_MFC_COND */
			CHECK_DST(PARM_MFC_COND, PTYPE_MFC_COND);
			pu->mfc_cond.cs = htons(mac->frm_errcond);
			pu->mfc_cond.macidx = htons(mac->rid);
			pu->mfc_cond.mfc = htonl(mac->frame_ct);
			pu->mfc_cond.mec = htonl(mac->err_ct);
			pu->mfc_cond.mlc = htonl(mac->lost_ct);
			pu->mfc_cond.mbfc = htonl(mac->bframe_ct);
			pu->mfc_cond.mbec = htonl(mac->berr_ct);
			pu->mfc_cond.mblc = htonl(mac->blost_ct);
			pu->mfc_cond.mbmts.tv_sec = htonl(mac->basetime.tv_sec);
			pu->mfc_cond.mbmts.tv_usec =
						htonl(mac->basetime.tv_usec);
			pu->mfc_cond.mer = htons((u_short)(mac->fr_errratio));
			*dlen -= off; *dst += off;
			return(RC_SUCCESS);

		case 142: /* PTYPE_MFNC_COND */
			CHECK_DST(PARM_MFNC_COND, PTYPE_MFNC_COND);
			pu->mfnc_cond.cs = htons(mac->frm_nccond);
			pu->mfnc_cond.macidx = htons(mac->rid);
			pu->mfnc_cond.ncc = htonl(mac->notcopied_ct);
			pu->mfnc_cond.rcc = htonl(mac->recv_ct);
			pu->mfnc_cond.bncc = htonl(mac->bnc_ct);
			pu->mfnc_cond.bmts.tv_sec = htonl(mac->basetime.tv_sec);
			pu->mfnc_cond.bmts.tv_usec =
						htonl(mac->basetime.tv_usec);
			pu->mfnc_cond.brcc = htonl(mac->brcv_ct);
			pu->mfnc_cond.pad = 0;
			pu->mfnc_cond.nc_ratio =htons((u_short)(mac->nc_ratio));
			*dlen -= off; *dst += off;
			return(RC_SUCCESS);

		case 143: /* PTYPE_NBRCHG_EVNT */
			CHECK_DST(PARM_NBRCHG_EVNT, PTYPE_NBRCHG_EVNT);
			pu->nbrchg_evnt.cs = 0;
			if (!bcmp((char*)&mac->old_una, (char*)&mac->una,
				sizeof(mac->una)))
				pu->nbrchg_evnt.cs |= 1;
			if (!bcmp((char*)&mac->old_dna, (char*)&mac->dna,
				sizeof(mac->dna)))
				pu->nbrchg_evnt.cs |= 2;
			pu->nbrchg_evnt.cs = htons(pu->nbrchg_evnt.cs);
			pu->nbrchg_evnt.macidx = htons(mac->rid);
			pu->nbrchg_evnt.old_una = mac->old_una;
			pu->nbrchg_evnt.una = mac->una;
			pu->nbrchg_evnt.old_dna = mac->old_dna;
			pu->nbrchg_evnt.dna = mac->dna;
			*dlen -= off; *dst += off;
			return(RC_SUCCESS);
	}
	return(RC_PARSE);
}

/*
 *
 * entity points to rid.
 */
static int
getPATH(void *entity, u_long type, char **dst, u_long *dlen)
{
	u_long off;
	register SMT_PATH *path;
	register SMT_PATHCLASS *pc;
	register int obj = typetoobj(type);
	register int subid = typetosubid(type);
	register u_long	rid = *((u_long *)entity);

	TLVDEBUG((LOG_DBGTLV, 0,
		"getPATH: rid=%d, subid=%d, obj=%d",subid,obj,rid));

	switch(subid) {
	case 0: /* path class */
		if (rid > 2)
			break;
		pc = &smtp->pathclass[rid-1];
		switch(obj) {
			case 10: RGROUP(getPATH, 4);
			case 12: RIINT(pc, maxtrace);
			case 13: RIINT(pc, tvx_lobound);
			case 14: RIINT(pc, tmax_lobound);
		}
		break;
	case 2: /* nonlocal path config grp */
		if ((path = getpathbyrid(rid)) == 0)
			break;

		switch(obj) {
			case 10: RGROUP(getPATH, 9);
			case 11: RISHORT(path, type);
			case 12: RISHORT(path, order);
			case 13: RIINT(path, latency);
			case 14: RISHORT(path, tracestatus);
			case 15: RISHORT(path, sba);
			case 16: RISHORT(path, sbaoverhead);
			case 17: RISHORT(path, status);
			case 18: RISHORT(path, rtype);
			case 19: RIINT(path, rmode);
		}
		break;
	case 30: { /* PTYPE_TRAC_EVNT */
		SMT_MAC *mac;
		PARM_UNION *pu;

		CHECK_DST(PARM_TRAC_EVNT, PTYPE_TRAC_EVNT);
		mac = smtp->mac;
		pu->trac_evnt.pad = 0;
		pu->trac_evnt.trace_start = mac->path.tracestatus==PT_INIT?1:0;
		pu->trac_evnt.trace_term = mac->path.tracestatus==PT_TERM?1:0;
		pu->trac_evnt.trace_prop = mac->path.tracestatus==PT_PROP?1:0;
		*dlen -= off; *dst += off;
		return(RC_SUCCESS);
		}
	}
	return(RC_PARSE);
}

/*
 * entity points to port.
 */
static int
getPORT(void *entity, u_long type, char **dst, u_long *dlen)
{
	u_long off;
	register PARM_UNION *pu;
	register int obj = typetoobj(type);
	register SMT_PHY *phy = (SMT_PHY *)entity;

	TLVDEBUG((LOG_DBGTLV, 0, "getPHY: obj=%d", obj));

	switch(obj) {
		/* PHY Config Grp */
		case 10: RGROUP(getPORT, 12);
		case 12: RICHAR(phy, pctype);
		case 13: RICHAR(phy, pcnbr);
		case 14: RICHAR(phy, phy_connp);
		case 15: RICHAR(phy, remotemac);
		case 16: RICHAR(phy, ce_state);
		case 17: RICHAR(phy, pathrequested);
		case 18: RICHAR(phy, macplacement);
		case 19: RICHAR(phy, pathavail);
		case 21: RICHAR(phy, loop_time);
		case 22: RICHAR(phy, fotx);

		/* PHY Operation Grp */
		case 30: RGROUP(getPORT, 3);
		case 31: RICHAR(phy, maint_ls);
		case 32: RIINT(phy, tb_max);
		case 33: RICHAR(phy, bs_flag);

		/* PHY Error Cnts Grp */
		case 40: RGROUP(getPORT, 2);
		case 41: RIINT(phy, eberr_ct);
		case 42: RIINT(phy, lctfail_ct);

		/* PHY Ler Grp */
		case 50: RGROUP(getPORT, 9);
		case 51: RISHORT(phy, ler_estimate);
		case 52: RIINT(phy, lemreject_ct);
		case 53: RIINT(phy, lem_ct);
		case 54: RICHAR(phy, bler_estimate);
		case 55: RIINT(phy, blemreject_ct);
		case 56: RIINT(phy, blem_ct);
		case 57: RITS(phy, bler_ts);
		case 58: RICHAR(phy, ler_cutoff);
		case 59: RICHAR(phy, ler_alarm);

		/* PHY Status Grp */
		case 60: RGROUP(getPORT, 4);
		case 61: RISHORT(phy, conn_state);
		case 62: RICHAR(phy, pcm_state);
		case 63: RICHAR(phy, withhold);
		case 64: RICHAR(phy, pler_cond);

		case 80: /* PTYPE_PLER_COND */
			CHECK_DST(PARM_PLER_COND, PTYPE_PLER_COND);
			pu->pler_cond.cs = htons(phy->pler_cond);
			pu->pler_cond.phyidx = htons(phy->rid);
			pu->pler_cond.pad = 0;
			pu->pler_cond.cutoff = phy->ler_cutoff;
			pu->pler_cond.alarm = phy->ler_alarm;
			pu->pler_cond.estimate = phy->ler_estimate;
			pu->pler_cond.ler_reject_ct = htonl(phy->lemreject_ct);
			pu->pler_cond.ler_ct = htonl(phy->lem_ct);
			pu->pler_cond.pad1[0] = 0;
			pu->pler_cond.pad1[1] = 0;
			pu->pler_cond.pad1[2] = 0;
			pu->pler_cond.ble = phy->bler_estimate;
			pu->pler_cond.blrc = htonl(phy->blemreject_ct);
			pu->pler_cond.blc = htonl(phy->blem_ct);
			pu->pler_cond.bmts.tv_sec = htonl(phy->bler_ts.tv_sec);
			pu->pler_cond.bmts.tv_usec= htonl(phy->bler_ts.tv_usec);
			*dlen -= off; *dst += off;
			return(RC_SUCCESS);

		case 81: /* PTYPE_UICA_EVNT */
			CHECK_DST(PARM_UICA_EVNT, PTYPE_UICA_EVNT);
			pu->uica_evnt.pad = 0;
			pu->uica_evnt.phyidx = htons(phy->rid);
			pu->uica_evnt.pc_type = phy->pctype;
			pu->uica_evnt.conn_state = phy->conn_state;
			pu->uica_evnt.pc_nbr = phy->pcnbr;
			pu->uica_evnt.accepted =
			    smtp->conn_policy^(1<<((phy->pctype*4)+phy->pcnbr));
			*dlen -= off; *dst += off;
			return(RC_SUCCESS);

		case 82: /* PTYPE_PEB_COND */
			CHECK_DST(PARM_PEB_COND, PTYPE_PEB_COND);
			pu->peb_cond.cs = htons(phy->eberr_delta?1:0);
			pu->peb_cond.phyidx = htons(phy->rid);
			pu->peb_cond.errcnt = htonl(phy->eberr_ct);
			*dlen -= off; *dst += off;
			return(RC_SUCCESS);
	}
	return(RC_PARSE);
}

/*
 *
 */
static int
getATTACH(void *entity, u_long type, char **dst, u_long *dlen)
{
	register int obj = typetoobj(type);
	register SMT_PHY *phy = (SMT_PHY *)entity;

	TLVDEBUG((LOG_DBGTLV, 0, "getATTACH: obj=%d", obj));
	if (!phy)
		return(RC_PARSE);

	switch(obj) {
		case 10: RGROUP(getATTACH, 5);
		case 11: RISHORT(phy, type);
		case 12: RICHAR(phy, obs);
		case 13: RIINT(phy, imax);
		case 14: RICHAR(phy, insert);
		case 15: RICHAR(phy, ipolicy);
	}
	return(RC_PARSE);
}

#define GRID {if (tlv_parse_int(src,slen,&rid,sizeof(short))) return(RC_PARSE);}
#define GMID {GRID; if ((mac = getmacbyrid(rid)) == 0) return(RC_PARSE);}
#define GPID {GRID; if ((phy = getphybyrid(rid)) == 0) return(RC_PARSE);}

/*
 *
 * Build to dst.
 */
static int
getFS(u_long type, char **src, u_long *slen, char **dst, u_long *dlen)
{
	u_long	 off, rid, i;
	register SMT_MAC *mac;
	register SMT_PHY *phy;
	register PARM_UNION *pu;
	register int obj = typetoobj(type);

	switch(obj) {
	case PTYPE_UNA:
		CHECK_DST(PARM_UNA, PTYPE_UNA);
		GMID;
		pu->una.una = mac->una;
		break;

	case PTYPE_STD:
		CHECK_DST(PARM_STD, PTYPE_STD);
		pu->std.nodeclass  = smtp->nodeclass;
		pu->std.mac_ct = smtp->mac_ct;
		pu->std.nonmaster_ct = smtp->nonmaster_ct;
		pu->std.master_ct = smtp->master_ct;
		break;

	case PTYPE_STAT:
		CHECK_DST(PARM_STAT, PTYPE_STAT);
		GMID;	/* get rid and convert it into mid */
		pu->stat.pad = 0;
		pu->stat.topology = smtp->topology;
		pu->stat.dupa  = mac->dupa_flag;
		break;

	case PTYPE_MTS:
		CHECK_DST(struct timeval, PTYPE_MTS);
		smt_gts(&smtp->msg_ts);
		pu->mts.tv_sec  = htonl(smtp->msg_ts.tv_sec);
		pu->mts.tv_usec = htonl(smtp->msg_ts.tv_usec);
		break;

	case PTYPE_SP:
		CHECK_DST(PARM_SP, PTYPE_SP);
		pu->sp.conf_policy = htons(smtp->conf_policy);
		pu->sp.conn_policy = htons(smtp->conn_policy);
		break;

	case PTYPE_PL:
#ifdef OPTIONAL
		/* ISSUE: what is it ? */
		CHECK_DST(PARM_PL, PTYPE_PL);
		pu->pl.indx1 = ?
		pu->pl.ring1 = ?
		pu->pl.indx2 = ?
		pu->pl.ring2 = ?
		break;
#else
		return(RC_PARSE);
#endif

	case PTYPE_MACNBRS:
		CHECK_DST(PARM_MACNBRS, PTYPE_MACNBRS);
		GMID;	/* get rid and convert it into mid */
		pu->macnbrs.pad = 0;
		if (mac->secondary)
			pu->macnbrs.macidx = mac->secondary->macplacement;
		else
			pu->macnbrs.macidx = 0;
		if (pu->macnbrs.macidx == 0)
			pu->macnbrs.macidx = mac->primary->macplacement;
		pu->macnbrs.macidx = htons(pu->macnbrs.macidx);
		pu->macnbrs.una = mac->una;
		pu->macnbrs.dna = mac->dna;
		break;

	case PTYPE_PDS: {
		register PARM_PD_PHY *pdp;
		register PARM_PD_MAC *pdm;

		i = smtp->mac_ct + smtp->phy_ct;
		off = i * sizeof(PARM_PD_MAC);
		if (tlv_build_tl(dst, dlen, PTYPE_PDS, off))
			return(RC_PARSE);

		phy = smtp->phy;
		pdp = (PARM_PD_PHY *)(*dst);
		while (phy) {
			pdp->pad = 0;
			pdp->pc_type = phy->pctype;
			pdp->conn_state = phy->conn_state;
			pdp->pc_nbr = phy->pcnbr;
			pdp->rmac_indecated= phy->remotemac;
			if (phy->macplacement)
				pdp->conn_rid = htons(phy->macplacement);
			else
				pdp->conn_rid = htons(phy->phyplacement);
			phy = phy->next;
			pdp++;
			i--;
		}

		mac = smtp->mac;
		pdm = (PARM_PD_MAC *)(pdp);
		while (mac) {
			pdm->addr = mac->addr;
			pdm->rid = htons(mac->pathplacement);
			mac = mac->next;
			pdm++;
			i--;
		}
		if (i) {
			sm_log(LOG_ERR, 0, "bad phy/mac count");
			return(RC_PARSE);
		}
		break;
	}
	case PTYPE_MAC_STAT:
		CHECK_DST(PARM_MAC_STAT, PTYPE_MAC_STAT);
		GMID;	/* get rid and convert it into mid */
		pu->mac_stat.pad = 0;
		if (mac->secondary)
			pu->mac_stat.macidx = mac->secondary->macplacement;
		else
			pu->mac_stat.macidx = 0;
		if (pu->mac_stat.macidx == 0)
			pu->mac_stat.macidx = mac->primary->macplacement;
		pu->mac_stat.macidx = htons(pu->mac_stat.macidx);
		pu->mac_stat.treq = htonl(mac->treq);
		pu->mac_stat.tneg = htonl(mac->tneg);
		pu->mac_stat.tmax = htonl(mac->tmax);
		pu->mac_stat.tvx = htonl(mac->tvx);
		pu->mac_stat.tmin = htonl(mac->tmin);
		pu->mac_stat.sba = htonl(mac->path.sba);
		pu->mac_stat.frame_ct = htonl(mac->frame_ct);
		pu->mac_stat.error_ct = htonl(mac->err_ct);
		pu->mac_stat.lost_ct = htonl(mac->lost_ct);
		break;

	case PTYPE_LER:
		CHECK_DST(PARM_LER, PTYPE_LER);
		GPID; /* phy's rid and mid are same */
		pu->ler.pad = 0;
		pu->ler.phyidx = htons(phy->rid);
		pu->ler.pad1 = 0;
		pu->ler.ler_cutoff = phy->ler_cutoff;
		pu->ler.ler_alarm = phy->ler_alarm;
		pu->ler.ler_estimate = phy->ler_estimate;
		pu->ler.lem_reject_ct = htonl(phy->lemreject_ct);
		pu->ler.lem_ct = htonl(phy->lem_ct);
		break;

	case PTYPE_MFC:
		CHECK_DST(PARM_MFC, PTYPE_MFC);
		GMID;	/* get rid and convert it into mid */
		pu->mfc.pad = 0;
		if (mac->secondary)
			pu->mfc.macidx = mac->secondary->macplacement;
		else
			pu->mfc.macidx = 0;
		if (pu->mfc.macidx == 0)
			pu->mfc.macidx = mac->primary->macplacement;
		pu->mfc.macidx = htons(pu->mfc.macidx);
		pu->mfc.recv_ct = htonl(mac->recv_ct);
		pu->mfc.xmit_ct = htonl(mac->xmit_ct);
		break;

	case PTYPE_MFNCC:
		CHECK_DST(PARM_MFNCC, PTYPE_MFNCC);
		GMID;	/* get rid and convert it into mid */
		pu->mfncc.pad = 0;
		pu->mfncc.macidx = htons(mac->rid);
		pu->mfncc.notcopied_ct = htonl(mac->notcopied_ct);
		break;

	case PTYPE_MPV:
		CHECK_DST(PARM_MPV, PTYPE_MPV);
		GMID;	/* get rid and convert it into mid */
		pu->mpv.pad = 0;
		if (mac->secondary)
			pu->mpv.macidx = mac->secondary->macplacement;
		else
			pu->mpv.macidx = 0;
		if (pu->mpv.macidx == 0)
			pu->mpv.macidx = mac->primary->macplacement;
		pu->mpv.macidx = htons(pu->mpv.macidx);
		pu->mpv.pri0 = htonl(mac->pri0);
		pu->mpv.pri1 = htonl(mac->pri1);
		pu->mpv.pri2 = htonl(mac->pri2);
		pu->mpv.pri3 = htonl(mac->pri3);
		pu->mpv.pri4 = htonl(mac->pri4);
		pu->mpv.pri5 = htonl(mac->pri5);
		pu->mpv.pri6 = htonl(mac->pri6);
		break;

	case PTYPE_EBS:
		CHECK_DST(PARM_EBS, PTYPE_EBS);
		GPID; /* phy's rid and mid are same */
		pu->ebs.pad = 0;
		pu->ebs.phyidx = htons(phy->rid);
		pu->ebs.eberr_ct = htonl(phy->eberr_ct);
		break;

	case PTYPE_MF:
		CHECK_DST(SMT_MANUF_DATA, PTYPE_MF);
		*((SMT_MANUF_DATA *)(*dst)) = smtp->manuf;
		break;

	case PTYPE_USR:
		CHECK_DST(USER_DATA, PTYPE_USR);
		*((USER_DATA *)(*dst)) = smtp->userdata;
		break;

	/* NON-MIB */
	case PTYPE_ECHO:
		off = *slen;
		if (tlv_build_tl(dst, dlen, PTYPE_ECHO, off))
			return(RC_PARSE);
		bcopy(*src, *dst, off);
		break;

	case PTYPE_RC:
		CHECK_DST(PARM_RC, PTYPE_RC);
		if (tlv_parse_int(src, slen, &i, 2))
			return(RC_PARSE);
		((PARM_RC *)(*dst))->reason = htonl(i);
		break;

	case PTYPE_RFB:
		off = *slen > *dlen ? *dlen : *slen;
		if (tlv_build_tl(dst, dlen, PTYPE_RFB, off))
			return(RC_PARSE);
		bcopy(*src, *dst, off);
		*src += *slen;
		*slen = 0;
		break;

	case PTYPE_VERS:
		i = smtp->vers_hi - smtp->vers_lo + 1;
		off = (i * sizeof(short)) + sizeof(int);
		if (i & 1) /* pad for alignment */
			off += sizeof(short);
		if (tlv_build_tl(dst, dlen, PTYPE_VERS, off))
			return(RC_PARSE);

		(*dst)[0] = 0;
		(*dst)[1] = 0;
		(*dst)[2] = i;
		(*dst)[3] = smtp->vers_op;
		TLVDEBUG((LOG_DBGTLV, 0, "%d versions supported", i));
		{
			u_short *usp = (u_short *)&((*dst)[4]);

			for (i = smtp->vers_lo; i <= smtp->vers_hi; i++, usp++)
				*usp = htons(i);
			if ((i-smtp->vers_lo)&1) {
				*usp = 0;
				TLVDEBUG((LOG_DBGTLV, 0, "versions padded"));
			}
		}
		break;

	case PTYPE_ESF:
		CHECK_DST(PARM_ESF, PTYPE_ESF);
		((PARM_ESF *)(*dst))->pad = 0;
		if (tlv_parse_string(src, slen, *dst,
			sizeof(LFDDI_ADDR), TLV_PREALIGN))
			return(RC_PARSE);
		break;

	/* TBD: Authentication */
	case PTYPE_AUTH:
	default:
		return(RC_PARSE);
	}

	*dlen -= off;
	*dst += off;
	return(RC_SUCCESS);
}

/*
 * tlvget:
 *
 *	This module "Get" MIB to dst in TLV form.
 *
 *	type -- Object type.
 *	len --- Object length(may not be needed.)
 *	dst --- resulting TLV.
 *
 *	Returns Reason-Code and dst pointers and length
 *	updated accordingly.
 */
int
tlvget(u_long type, char **src, u_long *slen, char **dst, u_long *dlen)
{
	SMT_MAC *mac;
	SMT_PHY *phy;
	u_long	rid;
	register int id = typetoid(type);

	switch (id) {
		case 0: return(getFS(type, src, slen, dst, dlen));
		case 1: return(getSMT(0, type, dst, dlen));
		case 2: GMID; return(getMAC(mac, type, dst, dlen));
		case 3: GRID; return(getPATH(&rid, type, dst, dlen));
		case 4: GPID; return(getPORT(phy, type, dst, dlen));
		case 6: GMID; return(getATTACH(mac->primary,type,dst,dlen));
	}
	return(RC_PARSE);
}
