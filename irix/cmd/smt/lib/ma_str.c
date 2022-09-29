/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.19 $
 */

#include <sys/types.h>
#include <string.h>
#include <sm_types.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smtd_fs.h>
#include <ma_str.h>

static char idono[] = "????";

char *
ma_mac_state_str(register enum mac_states ms, int verbose)
{
	if (verbose)
	    switch (ms) {
		case MAC_OFF:		return("OFF");
		case MAC_IDLE:		return("IDLE");
		case MAC_CLAIMING:	return("CLAIM");
		case MAC_BEACONING:	return("BEACON");
		case MAC_ACTIVE:	return("ACTIVE");
	    }
	else
	    switch (ms) {
		case MAC_OFF:		return("OFF");
		case MAC_IDLE:		return("IDL");
		case MAC_CLAIMING:	return("CLM");
		case MAC_BEACONING:	return("BEC");
		case MAC_ACTIVE:	return("ACT");
	    }
	return(idono);
}

char *
ma_pc_type_str(register enum fddi_pc_type pc)
{
	switch (pc) {
		case PC_A: return("A");
		case PC_B: return("B");
		case PC_M: return("M");
		case PC_S: return("S");
		case PC_UNKNOWN: return("?");
	}
	return(idono);
}

char *
ma_st_type_str(register enum fddi_st_type st)
{
	switch (st) {
		case SAS: return("SAS");
		case SAC: return("SAC");
		case SM_DAS: return("SM_DAS");
		case DM_DAS: return("DM_DAS");
	}
	return(idono);
}

char *
ma_pcm_tgt_str(register enum pcm_tgt tgt)
{
	switch (tgt) {
		case PCM_DEFAULT: return("DEFAULT");
		case PCM_NONE: return("NONE");
		case PCM_CMT: return("CMT");
	}
	return(idono);
}

char *
ma_pcm_state_str(register enum pcm_state pcm)
{
	switch (pcm) {
		case PCM_ST_OFF:	return("OFF");
		case PCM_ST_BREAK:	return("BREAK");
		case PCM_ST_TRACE:	return("TRACE");
		case PCM_ST_CONNECT:	return("CONNECT");
		case PCM_ST_NEXT:	return("NEXT");
		case PCM_ST_SIGNAL:	return("SIGNAL");
		case PCM_ST_JOIN:	return("JOIN");
		case PCM_ST_VERIFY:	return("VERIFY");
		case PCM_ST_ACTIVE:	return("ACTIVE");
		case PCM_ST_BYPASS:	return("BYPASS");
		case PCM_ST_MAINT:	return("MAINT");
	}
	return(idono);
}

char *
ma_rt_ls_str(register enum rt_ls ls)
{
	static char spcl[20];

	switch (ls) {
		case R_PCM_QLS:		return("r QLS");
		case R_PCM_ILS:		return("r ILS");
		case R_PCM_MLS:		return("r MLS");
		case R_PCM_HLS:		return("r HLS");
		case R_PCM_ALS:		return("r ALS");
		case R_PCM_ULS:		return("r ULS");
		case R_PCM_NLS:		return("r NLS");
		case R_PCM_LSU:		return("r LSU");
		case T_PCM_QLS:		return("t QLS");
		case T_PCM_ILS:		return("t ILS");
		case T_PCM_MLS:		return("t MLS");
		case T_PCM_HLS:		return("t HLS");
		case T_PCM_ALS:		return("t ALS");
		case T_PCM_ULS:		return("t ULS");
		case T_PCM_NLS:		return("t NLS");
		case T_PCM_LSU:		return("t LSU");
		case T_PCM_REP:		return("t REP");
		case T_PCM_SIG:		return("t SIG");
		case T_PCM_THRU:	return("THRU");
		case T_PCM_WRAP:	return("WRAP");
		case T_PCM_WRAP_AB:	return("WRAP_AB");
		case T_PCM_LCT:		return("LCT");
		case T_PCM_LCTOFF:	return("LCTOFF");
		case T_PCM_OFF:		return("OFF");
		case T_PCM_BREAK:	return("BREAK");
		case T_PCM_TRACE:	return("TRACE");
		case T_PCM_CONNECT:	return("CONNECT");
		case T_PCM_NEXT:	return("NEXT");
		case T_PCM_SIGNAL:	return("SIGNAL");
		case T_PCM_JOIN:	return("JOIN");
		case T_PCM_VERIFY:	return("VERIFY");
		case T_PCM_ACTIVE:	return("ACTIVE");
		case T_PCM_MAINT:	return("MAINT");
		case T_PCM_BYPASS:	return("BYPASS");
	}
	(void)sprintf(spcl,"?+%d", ls-SMT_LOG_X);
	return(spcl);
}

char *
ma_rt_ls_str2(register enum rt_ls ls)
{
	switch (ls) {
		case R_PCM_QLS:
		case T_PCM_QLS:
			return("QLS");
		case R_PCM_ILS:
		case T_PCM_ILS:
			return("ILS");
		case R_PCM_MLS:
		case T_PCM_MLS:
			return("MLS");
		case R_PCM_HLS:
		case T_PCM_HLS:
			return("HLS");
		case R_PCM_ALS:
		case T_PCM_ALS:
			return("ALS");
		case R_PCM_ULS:
		case T_PCM_ULS:
			return("ULS");
		case R_PCM_NLS:
		case T_PCM_NLS:
			return("NLS");
		case R_PCM_LSU:
		case T_PCM_LSU:
			return("LSU");
		case T_PCM_REP:
			return("REP");
		case T_PCM_SIG:
			return("SIG");
		default:
			return(ma_rt_ls_str(ls));
	}
	/*NOTREACHED*/
}

char *
ma_pcm_ls_str(register enum pcm_ls ls)
{
	switch (ls) {
		case PCM_QLS: return("QLS");
		case PCM_ILS: return("ILS");
		case PCM_MLS: return("MLS");
		case PCM_HLS: return("HLS");
		case PCM_ALS: return("ALS");
		case PCM_ULS: return("ULS");
		case PCM_NLS: return("NLS");
		case PCM_LSU: return("LSU");
	}
	return(idono);
}

char *
ma_pcm_flag_str(register u_long f, register char *fp)
{
	fp[0] = '<';
	fp[1] = 0;
	if (f) {
		/* official */
		if (f & PCM_BS_FLAG)	strcat(fp, "BS,");
		if (f & PCM_LS_FLAG)	strcat(fp, "LS,");
		if (f & PCM_TC_FLAG)	strcat(fp, "TC,");
		if (f & PCM_TD_FLAG)	strcat(fp, "TD,");
		if (f & PCM_RC_FLAG)	strcat(fp, "RC,");
		if (f & PCM_CF_JOIN)	strcat(fp, "JOIN,");
		if (f & PCM_WITHHOLD)	strcat(fp, "WITHHOLD,");
		if (f & PCM_THRU_FLAG)	strcat(fp, "THRU,");
		if (f & PCM_PC_DISABLE)	strcat(fp, "DISABLED,");
		if (f & PCM_WA_FLAG)	strcat(fp, "WA,");
		if (f & PCM_WAT_FLAG)	strcat(fp, "WAT,");

		/* less official */
		if (f & PCM_LEM_FAIL)	strcat(fp, "LEMFAIL,");

		if (f & PCM_NE)		strcat(fp, "Noise_Event,");
		if (f & PCM_TIMER)	strcat(fp, "TIMER,");
		if (f & PCM_RNGOP)	strcat(fp, "RNGOP,");

		if (f & PCM_HAVE_BYPASS) strcat(fp, "OBS,");

		if (f & PCM_CON_U)	strcat(fp, "CON_Undesire,");
		if (f & PCM_CON_X)	strcat(fp, "CON_Illegal,");

		if (f & PCM_DEBUG)	strcat(fp, "DEBUG,");

		if (f & PCM_SELF_TEST)	strcat(fp, "SELF_TEST,");
		if (f & PCM_TRACE_OTR)  strcat(fp, "TR_OTR,");
		if (f & PCM_DRAG)	strcat(fp, "DRAG,");

		if (f & PCM_PHY_BUSY)	strcat(fp, "PHY_BUSY,");
		if (f & PCM_PC_START)	strcat(fp, "PC_START,");
	}

	if (fp[strlen(fp)-1] == ',')
		fp[strlen(fp)-1] = '>';
	else
		strcat(fp, ">");

	return(fp);
}

char *
ma_fc_str(register u_char fc)
{
	switch (fc) {
		case SMT_FC_NIF:	return("NIF");
		case SMT_FC_CONF_SIF:	return("CIF");
		case SMT_FC_OP_SIF:	return("OIF");
		case SMT_FC_ECF:	return("ECF");
		case SMT_FC_RAF:	return("RAF");
		case SMT_FC_RDF:	return("RDF");
		case SMT_FC_SRF:	return("SRF");
		case SMT_FC_GET_PMF:	return("GET");
		case SMT_FC_CHG_PMF:	return("CHG");
		case SMT_FC_ADD_PMF:	return("ADD");
		case SMT_FC_RMV_PMF:	return("RMV");
		case SMT_FC_ESF:	return("ESF");
	}
	return(idono);
}

char *
ma_ft_str(register u_char ft)
{
	switch (ft) {
		case SMT_FT_ANNOUNCE:	return("ANNOUNCE");
		case SMT_FT_REQUEST:	return("REQUEST");
		case SMT_FT_RESPONSE:	return("RESPONSE");
	}
	return(idono);
}

static char *rcstr[] = {
	"",
	"Frame class not supported",
	"Frame version not supported",
	"Success",
	"Bad SETCOUNT",
	"Attempted to change readonly parameter",
	"Requested parameter is not supported",
	"No more room or parameter for add or rmv",
	"Out of range",
	"Authentification failed",
	"Parameter parsing failed",
	"Frame too long",
	"Unrecognized parameter",
};

char *
ma_rc_str(register u_long rc)
{
	if (rc >= RC_NOCLASS && rc <= RC_PARSE)
		return(rcstr[rc]);
	return(idono);
}

char *
ma_pc_signal_str(register u_long f, register int numbits, register char *fp)
{
	fp[0] = '<';
	fp[1] = 0;

	f &= (1 << numbits) - 1;

	if (f & PCM_BIT(0))	strcat(fp, "Esc,");
	if (numbits >= 3) {
		if (f & PCM_BIT(1)) {
			if (f & PCM_BIT(2))
				strcat(fp, "Port_M,");
			else
				strcat(fp, "Port_S,");
		} else {
			if (f & PCM_BIT(2))
				strcat(fp, "Port_B,");
			else
				strcat(fp, "Port_A,");
		}
	}
	if (numbits >= 4) {
		if (f & PCM_BIT(3))
			strcat(fp, "CONN,");
		else
			strcat(fp, "WITHHOLD,");
	}
	if (numbits >= 6) {
		if (f & PCM_BIT(4)) {
			if (f & PCM_BIT(5))
				strcat(fp, "EXT_LCT,");
			else
				strcat(fp, "LONG_LCT,");
		} else {
			if (f & PCM_BIT(5))
				strcat(fp, "MED_LCT,");
			else
				strcat(fp, "SHORT_LCT,");
		}
	}

	if (f & PCM_BIT(6)) strcat(fp, "MAC_FOR_LCT,");
	if (f & PCM_BIT(7)) strcat(fp, "LCT_FAIL,");
	if (f & PCM_BIT(8)) strcat(fp, "LOCAL_LOOP,");
	if (f & PCM_BIT(8)) strcat(fp, "MAC_ON_OUTPORT,");

	if (fp[strlen(fp)-1] == ',')
		fp[strlen(fp)-1] = '>';
	else
		strcat(fp, ">");
	return(fp);
}

char *
ma_mac_bits_str(register u_char f, register char *fp)
{
	fp[0] = '<';
	fp[1] = 0;
	if (f) {
		if (f & MAC_E_BIT)	strcat(fp, "E,");
		if (f & MAC_A_BIT)	strcat(fp, "A,");
		if (f & MAC_C_BIT)	strcat(fp, "C,");
		if (f & MAC_ERROR_BIT)	strcat(fp, "ERROR,");
		if (f & MAC_DA_BIT)	strcat(fp, "DA,");
	}

	if (fp[strlen(fp)-1] == ',')
		fp[strlen(fp)-1] = '>';
	else
		strcat(fp, ">");

	return(fp);
}

static char *nt[] = {
	"0 ", "1 ", "2 ", "3 ", "4 ", "5 ", "6 ", "7 ",
};

char *
ma_mac_fc_str(register u_char f, register char *p)
{
	p[0] = 0;
	if (MAC_FC_IS_VOID(f)) {
		strcat(p, "Void");
	} else if (f == MAC_FC_CLASS) {
		strcat(p, "Nonrestricted Token");
	} else if (f == (MAC_FC_CLASS|MAC_FC_ALEN)) {
		strcat(p, "Restricted Token");
	} else if (MAC_FC_IS_SMT(f)) {
		if (f&MAC_FC_ALEN == 0)	/* short addr */
			strcat(p, "Short Addressed ");
		if ((f|MAC_FC_ALEN) == FC_SMTINFO)
			strcat(p, "SMTINFO");
		else if ((f|MAC_FC_ALEN) == FC_NSA)
			strcat(p, "NSA");
		else
			strcat(p, "SMT");
	} else if (MAC_FC_IS_MAC(f)) {
		if (f&MAC_FC_ALEN == 0)	/* short addr */
			strcat(p, "Short Addressed ");
		if (f&0xf == 0x2)
			strcat(p, "Beacon");
		else if (f&0xf == 0x3)
			strcat(p, "Claim");
		else
			strcat(p, "MAC");
	} else if (MAC_FC_IS_LLC(f)) {
		if (f&MAC_FC_CLASS)
			strcat(p, "Sychronous ");
		if (f&MAC_FC_ALEN == 0)
			strcat(p, "Short Addressed ");
		strcat(p, "Priority ");
		strcat(p, nt[f&0x7]);
		strcat(p, "LLC");
	} else if (MAC_FC_IS_IMP(f)) {
		if (f&MAC_FC_CLASS)
			strcat(p, "Sychronous ");
		if (f&MAC_FC_ALEN == 0)
			strcat(p, "Short Addressed ");
		strcat(p, "Proto ");
		strcat(p, nt[f&0x7]);
		strcat(p, "IMP");
	}
	return(p);
}
