/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.10 $
 */

#include <sys/types.h>
#include <sys/time.h>

#include <sm_types.h>
#include <sm_log.h>
#include <smtd.h>
#include <smtd_parm.h>
#include <smtd_fs.h>
#include <smtd_svc.h>
#include <tlv_set.h>

/*
 *	fddiSMT SET operation.
 *	FORMAT: TLV
 *	return REASON_CODE.
 *	All the validity check is done already by caller.
 */
#define rstr(a,b,c,d,e) tlv_parse_string(a,b,c,d,e)?RC_PARSE:RC_SUCCESS
#define RSEQ(x,  y) return(rstr(src, slen, (char *)&(x), sizeof(x), (y)))
#define RSTR(x,y,z) return(rstr(src, slen, (char *)&(x),       (z), (y)))

#define rint(a,b,c,d)  tlv_parse_int(a,b,c,d)?RC_PARSE:RC_SUCCESS
#define	RINT(x)   return(rint(src, slen, (u_long *)&(x), 4))
#define	RSHORT(x) return(rint(src, slen, (u_long *)&(x), 2))
#define	RCHAR(x)  return(rint(src, slen, (u_long *)&(x), 1))


#define GET_RID {if (tlv_parse_int(src, slen, &rid, 2)) return(RC_PARSE);}
#define GET_MID	{GET_RID; \
		if (rid > smtp->mac_ct) return(RC_PARSE); \
		if ((mac = getmacbyrid(rid)) == 0) return(RC_PARSE); }
#define GET_PID	{GET_RID; \
		if (rid >= smtp->phy_ct) return(RC_PARSE); \
		if ((phy = getphybyrid(rid)) == 0) return(RC_PARSE); }

/*
 *
 */
static int
setSMT(u_long type, u_long len, char **src, u_long *slen)
{
	register int obj = typetoobj(type);

	TLVDEBUG((LOG_DBGTLV, 0, "setSMT: obj=%d", obj));

    	switch(obj) {
		/* fddiSMTStationidGrp */
		case 13: RSHORT(smtp->vers_op);
		case 17: RSEQ(smtp->userdata, TLV_NOALIGN);

		/* fddiSMTStationConfigGrp */
		case 26: RSHORT(smtp->conf_policy);
		case 27: RSHORT(smtp->conn_policy);
		case 28: RINT(smtp->reportlimit);
		case 29: RSHORT(smtp->t_notify);
		case 30: RCHAR(smtp->srf_on);

		/* ACTION Grp */
		case 60: return(RC_AUTH);
    	}
    	return(RC_PARSE);
}

static int
setMAC(u_long type, u_long len, char **src, u_long *slen)
{
	u_long	 rid;
	register SMT_MAC *mac;
	register int obj = typetoobj(type);

	GET_MID;
	TLVDEBUG((LOG_DBGTLV, 0, "setMAC: obj=%d", (int)obj));

    	switch(obj) {
		/* MAC capability Grp */
 		case 13: RINT(mac->tmax_lobound);

		/* MAC Config Grp */
 		case 32: RCHAR(mac->pathrequested);

		/* MAC Address Grp */
 		case 42: RSTR(mac->l_alias[0],TLV_POSTALIGN,mac->la_ct*6);
 		case 43: RSTR(mac->s_alias[0],TLV_POSTALIGN,mac->sa_ct*6);
 		case 44: RSTR(mac->l_grpaddr[0],TLV_POSTALIGN,mac->lg_ct*6);
 		case 45: RSTR(mac->s_grpaddr[0],TLV_POSTALIGN,mac->sg_ct*6);

		/* MAC Operation Grp */
 		case 51: RINT(mac->treq);
 		case 53: RINT(mac->tmax);
 		case 54: RINT(mac->tvx);
 		case 55: RINT(mac->tmin);
 		case 63: RSHORT(mac->curfsc);

		/* MAC Frame Condition Grp */
 		case 95: RSHORT(mac->fr_threshold);

		/* MAC Not Copied Condition Grp */
 		case 103: RSHORT(mac->fnc_threshold);

		/* MAC ACTION */
		case 120: return(RC_AUTH);
    	}
    	return(RC_PARSE);
}

static int
setPATH(u_long type, u_long len, char **src, u_long *slen)
{
	u_long	 rid;
	register SMT_PATH *path;
	register SMT_PATHCLASS *pc;
	register int obj = typetoobj(type);
	register int subid = typetosubid(type);

	GET_RID; /* pathclass indes or path index */
	TLVDEBUG((LOG_DBGTLV, 0, "setPATH: subid=%d, obj=%d", subid, obj));

    	switch(subid) {
	case 0: /* path class */
		if (rid > 2)
			break;
		pc = &smtp->pathclass[rid-1];
		switch(obj) {
			case 12: RINT(pc->maxtrace);
			case 13: RINT(pc->tvx_lobound);
			case 14: RINT(pc->tmax_lobound);
		}
		break;
	case 2: /* path config grp */
		if ((path = getpathbyrid(rid)) == 0)
    			return(RC_PARSE);

		switch(obj) {
			case 13: RINT(path->latency);
			case 15: RSHORT(path->sba);
			case 19: RINT(path->rmode);
		}
		break;
    	}
    	return(RC_PARSE);
}

static int
setPORT(u_long type, u_long len, char **src, u_long *slen)
{
	u_long   rid;
	register SMT_PHY *phy;
	register int obj = typetoobj(type);

	GET_PID;
	TLVDEBUG((LOG_DBGTLV, 0, "setPHY: obj=%d", obj));

    	switch(obj) {
		/* PHY Config Grp */
		case 14: RCHAR(phy->phy_connp);
		case 17: RCHAR(phy->pathrequested);
		case 21: RICHAR(phy->loop_time);

		/* PHY Operation Grp */
		case 31: RCHAR(phy->maint_ls);
		case 32: RINT(phy->tb_max);

		/* PHY Ler Grp */
		case 58: RCHAR(phy->ler_cutoff);
		case 59: RCHAR(phy->ler_alarm);

		/* ACTION */
		case 70: return(RC_AUTH);
    	}
    	return(RC_PARSE);
}

static int
setATTACH(u_long type, u_long len, char **src, u_long *slen)
{
	u_long	 rid;
	register SMT_PHY *phy;
	register SMT_MAC *mac;
	register int obj = typetoobj(type);

	GET_MID;
	if ((phy = mac->primary) == 0)
    		return(RC_PARSE);

	TLVDEBUG((LOG_DBGTLV, 0, "setATTACH: obj=%d", obj));
    	switch(obj) {
		case 15: RINT(phy->ipolicy);
    	}
    	return(RC_PARSE);
}

/*
 * tlvset:
 */
static int (*setcalls[])() = {
	0,		/* 0 */
	setSMT,		/* 1 */
	setMAC,		/* 2 */
	setPATH,	/* 3 */
	setPORT,	/* 4 */
	0,		/* 5 */
	setATTACH,	/* 6 */
};

/*
 *	This module "Set" src TLV to MIB.
 *
 *	type -- Object type.
 *	len --- Object length(may not be needed).
 *	src - TLV to set(has rid iff necessary).
 *
 *	Returns Reason-Code and both src pointers and length
 *	updated accordingly.
 */
int
tlvset(u_long type, u_long len, char **src, u_long *slen)
{
	register int id = typetoid(type);

	if ((id > (sizeof(setcalls)/sizeof(char*))) || (setcalls[id] == 0))
		return(RC_PARSE);
	return((*setcalls[id])(type, len, src, slen));
}
