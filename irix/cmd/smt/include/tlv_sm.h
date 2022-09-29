#ifndef TLV_SM_H
#define TLV_SM_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

#include <tlv.h>

/* Upstream Nbr. */
extern int tlv_parse_una(char**, u_long*, LFDDI_ADDR *);
extern int tlv_build_una(char**, u_long*, LFDDI_ADDR *);

/* Station Description. */
extern int tlv_parse_std(char**, u_long*, PARM_STD*);
extern int tlv_build_std(char**, u_long*, PARM_STD*);

/* Station Status. */
extern int tlv_parse_stat(char**, u_long*, PARM_STAT*);
extern int tlv_build_stat(char**, u_long*, PARM_STAT*);

/* Message Time Stamp. */
extern int tlv_parse_mts(char**, u_long*, struct timeval*);
extern int tlv_build_mts(char**, u_long*, struct timeval*);

/* Station Policy. */
extern int tlv_parse_sp(char**, u_long*, PARM_SP*);
extern int tlv_build_sp(char**, u_long*, PARM_SP*);

/* Path Latency. */
extern int tlv_parse_pl(char**, u_long*, PARM_PL*);
extern int tlv_build_pl(char**, u_long*, PARM_PL*);

/* MAC neighbors. */
extern int tlv_parse_macnbrs(char**, u_long*, PARM_MACNBRS*);
extern int tlv_build_macnbrs(char**, u_long*, PARM_MACNBRS*);

/* Path Descriptors. */
extern int tlv_parse_pds(char**, u_long*, PARM_PD_MAC*, u_long*);
extern int tlv_build_pds(char**, u_long*, PARM_PD_MAC*, u_long);

/* MAC status. */
extern int tlv_parse_mac_stat(char**, u_long*, PARM_MAC_STAT*);
extern int tlv_build_mac_stat(char**, u_long*, PARM_MAC_STAT*);

/* PHY LEM Status. */
extern int tlv_parse_ler(char**, u_long*, PARM_LER*);
extern int tlv_build_ler(char**, u_long*, PARM_LER*);

/* MAC frmae counters. */
extern int tlv_parse_mfc(char**, u_long*, PARM_MFC*);
extern int tlv_build_mfc(char**, u_long*, PARM_MFC*);

/* MAC FRAME NOT COPIED COUNTER. */
extern int tlv_parse_mfncc(char**, u_long*, PARM_MFNCC*);
extern int tlv_build_mfncc(char**, u_long*, PARM_MFNCC*);

/* MAC Priority Value. */
extern int tlv_parse_mpv(char**, u_long*, PARM_MPV*);
extern int tlv_build_mpv(char**, u_long*, PARM_MPV*);

/* PHY Elasticy Count. */
extern int tlv_parse_ebs(char**, u_long*, PARM_EBS*);
extern int tlv_build_ebs(char**, u_long*, PARM_EBS*);

/* Manufacturer . */
extern int tlv_parse_mf(char**, u_long*, SMT_MANUF_DATA*);
extern int tlv_build_mf(char**, u_long*, SMT_MANUF_DATA*);

/* USER . */
extern int tlv_parse_usr(char**, u_long*, USER_DATA*);
extern int tlv_build_usr(char**, u_long*, USER_DATA*);

/* Echo */
extern int tlv_parse_echo(char**, u_long*, char*, u_long);
extern int tlv_build_echo(char**, u_long*, char*, u_long);

/* Reason Code. */
extern int tlv_parse_rc(char**, u_long*, u_long*);
extern int tlv_build_rc(char**, u_long*, u_long);

/* Return Frame Begin. */
extern int tlv_parse_rfb(char**, u_long*, char*, u_long*);
extern int tlv_build_rfb(char**, u_long*, char*, u_long*);

/* VERSION. */
extern int tlv_parse_vers(char**, u_long*, u_long*, u_long*, u_long*);
extern int tlv_build_vers(char**, u_long*, u_long,	u_long, u_long);

/* Extended Service Frame. */
extern int tlv_parse_esf(char**, u_long*, LFDDI_ADDR*);
extern int tlv_build_esf(char**, u_long*, LFDDI_ADDR*);

/* mac frame counter condition. */
extern int tlv_parse_mfc_cond(char**, u_long*, PARM_MFC_COND*);
extern int tlv_build_mfc_cond(char**, u_long*, PARM_MFC_COND*);

/* phy ler condition. */
extern int tlv_parse_pler_cond(char**, u_long*, PARM_PLER_COND*);
extern int tlv_build_pler_cond(char**, u_long*, PARM_PLER_COND*);

/* mac frame not copied condition. */
extern int tlv_parse_mfnc_cond(char**, u_long*, PARM_MFNC_COND*);
extern int tlv_build_mfnc_cond(char**, u_long*, PARM_MFNC_COND*);

/* mac dupa condition. */
extern int tlv_parse_dupa_cond(char**, u_long*, PARM_DUPA_COND*);
extern int tlv_build_dupa_cond(char**, u_long*, PARM_DUPA_COND*);

/* UICA EVNT. */
extern int tlv_parse_uica_evnt(char**, u_long*, PARM_UICA_EVNT*);
extern int tlv_build_uica_evnt(char**, u_long*, PARM_UICA_EVNT*);

/* TRACE EVNT. */
extern int tlv_parse_trac_evnt(char**, u_long*, PARM_TRAC_EVNT*);
extern int tlv_build_trac_evnt(char**, u_long*, PARM_TRAC_EVNT*);

/* MAC NBR changed evnt. */
extern int tlv_parse_nbrchg_evnt(char**, u_long*, PARM_NBRCHG_EVNT*);
extern int tlv_build_nbrchg_evnt(char**, u_long*, PARM_NBRCHG_EVNT*);

#endif
