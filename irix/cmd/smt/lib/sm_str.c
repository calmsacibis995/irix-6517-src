/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.12 $
 */

#include <sys/types.h>
#include <string.h>
#include <sm_types.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smtd_fs.h>
#include <sm_str.h>

static char idono[] = "????";

char *
sm_rmt_flag_str(register u_long f, register char *fp)
{
	fp[0] = '<';
	fp[1] = 0;
	if (f) {
		if (f & RMF_LOOP)	strcat(fp, "LOOP,");
		if (f & RMF_JOIN)	strcat(fp, "JOIN,");
		if (f & RMF_LOOP_AVAIL)	strcat(fp, "LOOP_AVAIL,");
		if (f & RMF_MAC_AVAIL)	strcat(fp, "MAC_AVAIL,");
		if (f & RMF_JM_FLAG)	strcat(fp, "JM,");
		if (f & RMF_NO_FLAG)	strcat(fp, "NO,");
		if (f & RMF_BN_FLAG)	strcat(fp, "BN,");
		if (f & RMF_CLAIM_FLAG)	strcat(fp, "CLM,");
		if (f & RMF_RE_FLAG)	strcat(fp, "RE,");
		if (f & RMF_TRT_EXP)	strcat(fp, "TRT,");
		if (f & RMF_DUP_DET)	strcat(fp, "DUP_DET,");
		if (f & RMF_DUP)	strcat(fp, "DUPLICATE,");
		if (f & RMF_TBID)	strcat(fp, "TBID,");
		if (f & RMF_MYBCN)	strcat(fp, "MYBCN,");
		if (f & RMF_OTRBCN)	strcat(fp, "OTRBCN,");
		if (f & RMF_MYCLM)	strcat(fp, "MYCLM,");
		if (f & RMF_OTRCLM)	strcat(fp, "OTRCLM,");
		if (f & RMF_T4)		strcat(fp, "T4,");
		if (f & RMF_T5)		strcat(fp, "T5,");
	}

	if (fp[strlen(fp)-1] == ',')
		fp[strlen(fp)-1] = '>';
	else
		strcat(fp, ">");

	return(fp);
}

char *
sm_cond_str(u_short cond)
{
	if (cond)
		return("SET");
	return("CLEAR");
}

char *
sm_nodeclass_str(int nc)
{
	if (nc)
		return("CONCENTRATOR");
	return("STATION");
}

char *
sm_conf_policy_str(register u_long policy, register char *conf)
{
	conf[0] = 0;
	if (policy) {
		if (policy & SMT_HOLDAVAIL) strcat(conf, "HOLDAVAIL,");
		if (policy & SMT_WRAPAB)    strcat(conf, "WRAPAB,");
	} else {
		strcat(conf, "NONE,");
	}

	conf[strlen(conf)-1] = 0;
	return(conf);
}

/*
 * SMT level connection policy.
 */
char *
sm_conn_policy_str(register u_long policy, register char *conn)
{
	conn[0] = 0;
	strcat(conn, "REJECT(");
	if (policy) {
		if (policy & SMT_REJECT_AA) strcat(conn, "AA,");
		if (policy & SMT_REJECT_AB) strcat(conn, "AB,");
		if (policy & SMT_REJECT_AS) strcat(conn, "AS,");
		if (policy & SMT_REJECT_AM) strcat(conn, "AM,");
		if (policy & SMT_REJECT_BA) strcat(conn, "BA,");
		if (policy & SMT_REJECT_BB) strcat(conn, "BB,");
		if (policy & SMT_REJECT_BS) strcat(conn, "BS,");
		if (policy & SMT_REJECT_BM) strcat(conn, "BM,");
		if (policy & SMT_REJECT_SA) strcat(conn, "SA,");
		if (policy & SMT_REJECT_SB) strcat(conn, "SB,");
		if (policy & SMT_REJECT_SS) strcat(conn, "SS,");
		if (policy & SMT_REJECT_SM) strcat(conn, "SM,");
		if (policy & SMT_REJECT_MA) strcat(conn, "MA,");
		if (policy & SMT_REJECT_MB) strcat(conn, "MB,");
		if (policy & SMT_REJECT_MS) strcat(conn, "MS,");
		if (policy & SMT_REJECT_MM) strcat(conn, "MM,");
	} else {
		strcat(conn, "NONE,");
	}

	conn[strlen(conn)-1] = 0;
	strcat(conn, ")");
	return(conn);
}

/*
 * PHY level connection policy.
 */
char *
sm_phy_connp_str(register u_long policy, register char *conn)
{
	conn[0] = 0;
	if (policy) {
		if (policy&PC_MAC_LCT)	     strcat(conn, "LCT,");
		if (policy&PC_MAC_LOOP)      strcat(conn, "LOOP,");
		if (policy&PC_MAC_PLACEMENT) strcat(conn, "PLACEMENT,");
	} else
		strcat(conn, "NONE,");

	conn[strlen(conn)-1] = 0;
	return(conn);
}

char *
sm_hold_state_str(register HOLD_STATE hs)
{
	switch (hs) {
		case not_holding: return("DISABLED");
		case holding_prm: return("PRIMARY");
		case holding_sec: return("SECONDARY");
	}
	return(idono);
}

char *
sm_ecm_state_str(register ECM_STATE ec)
{
	switch(ec) {
		case EC_OUT:	return("OUT");
		case EC_IN:	return("IN");
		case EC_TRACE:	return("TRACE");
		case EC_LEAVE:	return("LEAVE");
		case EC_PATHTEST: return("PATHTEST");
		case EC_INSERT: return("INSERT");
		case EC_CHECK:	return("CHECK");
	}
	return(idono);
}

char *
sm_cfm_state_str(register CFM_STATE cf)
{
	switch (cf) {
		case SC_ISOLATED:	return("ISOLATED");
		case SC_WRAP_A:		return("WRAP_A");
		case SC_WRAP_B:		return("WRAP_B");
		case SC_WRAP_AB:	return("WRAP_AB");
		case SC_THRU_A:		return("THRU_A");
		case SC_THRU_B:		return("THRU_B");
		case SC_THRU_AB:	return("THRU_AB");
		case SC_WRAP_S:		return("WRAP_S");
	}
	return(idono);
}

char *
sm_topology_str(register u_long topology, register char *topo)
{
	topo[0] = 0;
	if (topology) {
		if (topology & SMT_WRAPPED)	strcat(topo, "WRAPPED,");
		if (topology & SMT_UNROOT)	strcat(topo, "UNROOT,");
		if (topology & SMT_AA_TWIST)	strcat(topo, "AA_TWIST,");
		if (topology & SMT_BB_TWIST)	strcat(topo, "BB_TWIST,");
		if (topology & SMT_ROOTSTA)	strcat(topo, "ROOTSTA,");
		if (topology & SMT_DO_SRF)	strcat(topo, "DO_SRF,");
	} else
		strcat(topo, "NONE,");

	topo[strlen(topo)-1] = 0;
	return(topo);
}

char *
sm_dupa_test_str(register DUPA_TEST t)
{
	switch (t) {
		case DUPA_NONE: return("NONE");
		case DUPA_PASS: return("PASS");
		case DUPA_FAIL: return("FAIL");
	}
	return(idono);
}

char *
sm_flag_str(register u_long flag)
{
	if (flag)
		return("ON");
	return("OFF");
}

char *
sm_nn_state_str(register u_long nns)
{
	if (nns == 0)
		return("NT0");
	if (nns == 1)
		return("NT1");
	return(idono);
}

char *
sm_bridge_str(register u_long bridge)
{
	if (bridge == BRIDGE_TYPE0)
		return("802.1b-style");
	if (bridge == BRIDGE_TYPE1)
		return("802.5-style");
	return(idono);
}

char *
sm_pathtype_str(register u_long pt, register char *pts)
{
	pts[0] = 0;
	if (pt) {
		if (pt & PATHTYPE_PRIMARY)	strcat(pts, "PRM,");
		if (pt & PATHTYPE_SECONDARY)	strcat(pts, "SEC,");
		if (pt & PATHTYPE_LOCAL)	strcat(pts, "LOC,");
		if (pt & PATHTYPE_ISOLATED)	strcat(pts, "ISO,");
	} else
		strcat(pts, "UNKNOWN,");

	pts[strlen(pts)-1] = 0;
	return(pts);
}

char *
sm_rmtstate_str(register RMT_STATE rs)
{
	switch (rs) {
		case RM_ISOLATED:	return("ISOLATED");
		case RM_NONOP:		return("NONOP");
		case RM_RINGOP:		return("RINGOP");
		case RM_DETECT:		return("DETECT");
		case RM_NONOP_DUP:	return("NONOP_DUP");
		case RM_RINGOP_DUP:	return("RINGOP_DUP");
		case RM_DIRECTED:	return("DIRECTED");
		case RM_TRACE:		return("TRACE");
	}
	return(idono);
}

char *
sm_msloop_str(register u_long msl)
{
	switch (msl) {
		case MSL_UNKNOWN:   return("UNKNOWN");
		case MSL_SUSPECTED: return("SUSPECTED");
		case MSL_NOLOOP:    return("NOLOOP");
	}
	return(idono);
}

char *
sm_ce_state_str(register CE_STATE ce)
{
	switch (ce) {
		case ISOLATED:	return("ISOLATED");
		case INSERT_P:	return("INSERT_P");
		case INSERT_S:	return("INSERT_S");
		case INSERT_X:	return("INSERT_X");
		case LOCAL:	return("LOCAL");
	}
	return(idono);
}

char *
sm_conn_state_str(register PHY_CONN_STATE cn)
{
	switch (cn) {
		case PC_DISABLED:	return("DISABLED");
		case PC_CONNECTING:	return("CONNECTING");
		case PC_STANDBY:	return("STANDBY");
		case PC_ACTIVE:		return("ACTIVE");
	}
	return(idono);
}

char *
sm_withhold_str(register PHY_WITHHOLD w)
{
	switch (w) {
		case PW_NONE:	return("NONE");
		case PW_MM:	return("MM");
		case PW_OTHER:	return("OTHER");
	}
	return(idono);
}

char *
sm_fotx_str(register PMD_FOTX f)
{
	switch (f) {
		case PF_MULTI:	return("MULTI");
		case PF_SMODE1: return("SMODE1");
		case PF_SMODE2: return("SMODE2");
		case PF_SONET:	return("SONET");
	}
	return(idono);
}

char *
sm_ptype_str(u_long type)
{
	static char str[64];

	switch (type) {
	case PTYPE_UNA:		return("Upstream Nbr Addr");
	case PTYPE_STD:		return("Station Descriptor");
	case PTYPE_STAT:	return("Station State");
	case PTYPE_MTS:		return("Message Time Stamp");
	case PTYPE_SP:		return("Station Policies");
	case PTYPE_PL:		return("Path Latency");
	case PTYPE_MACNBRS:	return("MAC nbrs");
	case PTYPE_PDS:		return("Path Descriptor");
	case PTYPE_MAC_STAT:	return("MAC Status");
	case PTYPE_LER:		return("LER status");
	case PTYPE_MFC:		return("Mac Frame Counters");
	case PTYPE_MFNCC:	return("Mac Frame Not Copied Counter");
	case PTYPE_MPV:		return("Mac Priority Values");
	case PTYPE_EBS:		return("Elasticity Buffer Status");
	case PTYPE_MF:		return("Manuf Field");
	case PTYPE_USR:		return("USeR");
	case PTYPE_ECHO:	return("ECHO data");
	case PTYPE_RC:		return("Reason Code");
	case PTYPE_RFB:		return("Rejected Frame Begin");
	case PTYPE_VERS:	return("VERSion");
	case PTYPE_ESF:		return("Extended Service Frame");
	case PTYPE_MFC_COND:	return("Mac Frame error CONDition");
	case PTYPE_PLER_COND:	return("Port LER CONDition");
	case PTYPE_MFNC_COND:	return("Mac frame Not Copied CONDition");
	case PTYPE_DUPA_COND:	return("DUP Addr CONDition");
	case PTYPE_PEB_COND:	return("Port EB error CONDition");
	case PTYPE_PD_EVNT:	return("Path Desc/Config change evnt");
	case PTYPE_UICA_EVNT:	return("Undesirable/Illegal Conn Attempt");
	case PTYPE_TRAC_EVNT:	return("TRACe status");
	case PTYPE_NBRCHG_EVNT:	return("mac NBR CHanGe");
	case PTYPE_AUTH:	return("AUTHorization");
	}

	sprintf(str, "%x", type);
	return(str);
}
