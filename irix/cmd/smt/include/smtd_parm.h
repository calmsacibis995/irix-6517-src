#ifndef __SMTD_PARM_H
#define __SMTD_PARM_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.8 $
 */

#define	PTYPE_UNA		0x0001	/* Upstream Nbr Addr */
#define	PTYPE_STD		0x0002	/* Station Descriptor */
#define	PTYPE_STAT		0x0003	/* Station State */
#define	PTYPE_MTS		0x0004	/* Message Time Stamp */
#define	PTYPE_SP		0x0005	/* Station Policies */
#define	PTYPE_PL		0x0006	/* Path Latency */
#define	PTYPE_MACNBRS		0x0007	/* MAC nbrs */
#define	PTYPE_PDS		0x0008	/* Path Descriptor */
#define	PTYPE_MAC_STAT		0x0009	/* MAC Status */
#define	PTYPE_LER		0x000a	/* LER status */
#define	PTYPE_MFC		0x000b	/* Mac Frame Counters */
#define	PTYPE_MFNCC		0x000c	/* Mac Frame Not Copied Counter */
#define	PTYPE_MPV		0x000d	/* Mac Priority Values */
#define	PTYPE_EBS		0x000e	/* Elasticity Buffer Status */
#define	PTYPE_MF		0x000f	/* Manuf Field */
#define	PTYPE_USR		0x0010	/* USeR */
#define	PTYPE_ECHO		0x0011	/* ECHO data */
#define	PTYPE_RC		0x0012	/* Reason Code */
#define	PTYPE_RFB		0x0013	/* Rejected Frame Begin */
#define	PTYPE_VERS		0x0014	/* VERSion */

#define	PTYPE_ESF		0xffff	/* Extended Service Frame */

#define	PTYPE_MFC_COND		0x208d	/* Mac Frame error CONDition */
#define	PTYPE_PLER_COND		0x4050	/* Port LER CONDition */
#define	PTYPE_MFNC_COND		0x208e	/* Mac frame Not Copied CONDition */
#define	PTYPE_DUPA_COND		0x208c	/* DUP Addr CONDition */
#define	PTYPE_PEB_COND		0x4052	/* Port EB error CONDition */
#define	PTYPE_PD_EVNT		0x1046	/* Path Desc/Config change evnt */
#define	PTYPE_UICA_EVNT		0x4051	/* Undesirable/Illegal Conn Attempt */
#define	PTYPE_TRAC_EVNT		0x300e	/* TRACe status */
#define	PTYPE_NBRCHG_EVNT	0x208f	/* mac NBR CHanGe */

#define	PTYPE_AUTH		0x0021	/* AUTHorization */

/*
 * SMT PARAMETERS.
 */
typedef struct {
	u_short pad;
	LFDDI_ADDR	una;
} PARM_UNA;

typedef struct {
	u_char	nodeclass;	/* 0-station, 1-concentrator */
	u_char	mac_ct;
	u_char	nonmaster_ct;
	u_char	master_ct;
} PARM_STD;	/* STation Descriptor */

typedef struct {
	u_short pad;
	u_char topology;
	u_char dupa;
} PARM_STAT;	/* Station Status */

typedef struct {
	u_short	conf_policy;
	u_short conn_policy;
} PARM_SP;	/* Station Policy */

typedef struct {
	u_short	indx1;
	u_short ring1;
	u_short	indx2;
	u_short ring2;
} PARM_PL;	/* Path Latency */

typedef struct {
	u_short pad;
	u_short macidx;
	LFDDI_ADDR una;
	LFDDI_ADDR dna;
} PARM_MACNBRS;	/* MAC neighbors */

/* Path DescriptorS */
/* PHY records, one per PHY */
typedef struct {
	u_short	pad;
	u_char	pc_type;
	u_char	conn_state;
	u_char	pc_nbr;
	u_char	rmac_indecated;
	u_short	conn_rid;
} PARM_PD_PHY;		/* PHY Path Descriptor Event */

/* MAC records, one per MAC */
typedef struct {
	LFDDI_ADDR	addr;
	u_short		rid;
} PARM_PD_MAC;		/* MAC Path Descriptor Event */


/* Path List */
typedef struct {
	u_char pad[2];
	u_short pathtype;	/* 1 - primary, 2 - secondary */
} PARM_PD_LIST;

typedef struct {
	u_short	restype;	/* 2 - mac, 4 - port */
	u_short rid;
} PARM_PD_VAL;

typedef struct {
	u_short pad;
	u_short macidx;
	u_long	treq;
	u_long	tneg;
	u_long	tmax;
	u_long	tvx;
	u_long	tmin;
	u_long	sba;
	u_long	frame_ct;
	u_long	error_ct;
	u_long	lost_ct;
} PARM_MAC_STAT;	/* One entry per MAC */

typedef struct {
	u_short	pad;
	u_short	phyidx;
	u_char	pad1;
	u_char	ler_cutoff;
	u_char	ler_alarm;
	u_char	ler_estimate;
	u_long	lem_reject_ct;
	u_long	lem_ct;
} PARM_LER;	/* Link Error Rate monitoring */

typedef struct {
	u_short	pad;
	u_short	macidx;
	u_long	recv_ct;
	u_long	xmit_ct;
} PARM_MFC;	/* MAC Frame Counters */

typedef struct {
	u_short	pad;
	u_short	macidx;
	u_long	notcopied_ct;
} PARM_MFNCC;	/* MAC Frame Not Copied Counters */

typedef struct {
	u_short	pad;
	u_short	macidx;
	u_long	pri0;
	u_long	pri1;
	u_long	pri2;
	u_long	pri3;
	u_long	pri4;
	u_long	pri5;
	u_long	pri6;
} PARM_MPV;	/* MAC Priority Values */

typedef struct {
	u_short	pad;
	u_short	phyidx;
	u_long	eberr_ct;
} PARM_EBS;	/* PHY Elasticy Buffer Status */

typedef struct {
#define	RC_NOCLASS	0x1	/* Frame Class Not Supported */
#define	RC_NOVERS	0x2	/* Frame Version Not Supported */
#define	RC_SUCCESS	0x3	/* Success */
#define	RC_BADSETCOUNT	0x4	/* Bad SETCOUNT */
#define	RC_READONLY	0x5	/* Attempted to change readonly patameter */
#define	RC_NOPARM	0x6	/* Requested Patameter is not supported */
#define	RC_NOMORE	0x7	/* No more room or parameter for add or rmv */
#define	RC_RANGE	0x8	/* Out Of Range */
#define	RC_AUTH		0x9	/* Authentification failed */
#define	RC_PARSE	0xa	/* Patameter parsing failed. */
#define	RC_TOOLONG	0xb	/* Frame too long. */
#define	RC_INVALID	0xc	/* Unrecognized Patameter. */
	u_long	reason;
} PARM_RC;	/* Reason Code */


typedef struct {
	u_short	pad;
	LFDDI_ADDR	esfid;
} PARM_ESF;

typedef struct {
	u_short	cs;	/* Condition State */
	u_short macidx; /* MIB index */
	u_long	mfc;	/* frame count */
	u_long	mec;	/* mac err cnt */
	u_long	mlc;	/* mac lost cnt */
	u_long	mbfc;	/* mac base frame cnt */
	u_long	mbec;	/* mac base err cnt */
	u_long	mblc;	/* mac base lost cnt */
	struct timeval	mbmts;	/* mac base msg time stamp */
	u_short pad;
	u_short mer;	/* mac frame err ratio */
} PARM_MFC_COND;	/* MAC Frame Count Condition */

typedef struct {
	u_short	cs;	/* Condition State */
	u_short	phyidx;	/* MIB index */
	u_char	pad;
	u_char	cutoff;
	u_char	alarm;
	u_char	estimate;
	u_long	ler_reject_ct;
	u_long	ler_ct;
	u_char	pad1[3];
	u_char	ble;	/* Base Ler Estimate */
	u_long	blrc;	/* Base Ler reject cnt */
	u_long	blc;	/* Base ler cnt */
	struct timeval	bmts;	/* base msg time stamp */
} PARM_PLER_COND;	/* PHY Ler Condition */

typedef struct {
	u_short	cs;
	u_short	macidx;
	u_long	ncc;	/* not copied cnt */
	u_long	rcc;	/* received frame count */
	u_long	bncc;	/* base not copied cnt */
	struct timeval	bmts;	/* base msg time stamp */
	u_long	brcc;	/* base received frame count */
	u_short pad;
	u_short nc_ratio;
} PARM_MFNC_COND;	/* mac frame not copied condition */

typedef struct {
	u_short	cs;
	u_short macidx;
	LFDDI_ADDR dupa;
	LFDDI_ADDR una_dupa;
} PARM_DUPA_COND;		/* Dup Addr Condition */

typedef struct {
	u_short	cs;
	u_short phyidx;
	u_long	errcnt;
} PARM_PEB_COND;		/* PHY EB Condition */

typedef struct {
	u_short pad;
	u_short	phyidx;
	u_char	pc_type;
	u_char	conn_state;
	u_char	pc_nbr;
	u_char	accepted;
} PARM_UICA_EVNT;	/* Undesirable or Illegal connection Attempt */

typedef struct {
	u_char	pad;
	u_char	trace_start;
	u_char	trace_term;
	u_char	trace_prop;
} PARM_TRAC_EVNT;

typedef struct {
	u_short		cs;
	u_short		macidx;
	LFDDI_ADDR	old_una;
	LFDDI_ADDR	una;
	LFDDI_ADDR	old_dna;
	LFDDI_ADDR	dna;
} PARM_NBRCHG_EVNT;


typedef union {
	int		ary[32];	/* ensure size */
	PARM_UNA        una;
	PARM_STD        std;
	PARM_STAT       stat;
	struct timeval	mts;
	PARM_SP         sp;
	PARM_PL         pl;
	PARM_MACNBRS    macnbrs;
	PARM_PD_PHY     pd_phy;
	PARM_PD_MAC     pd_mac;
	PARM_MAC_STAT   mac_stat;
	PARM_LER        ler;
	PARM_MFC        mfc;
	PARM_MFNCC      mfncc;
	PARM_MPV        mpv;
	PARM_EBS        ebs;
	USER_DATA	user;
	PARM_RC         rc;
	PARM_MFC_COND	mfc_cond;
	PARM_PLER_COND  pler_cond;
	PARM_MFNC_COND  mfnc_cond;
	PARM_PEB_COND   peb_cond;
	PARM_DUPA_COND  dupa_cond;
	PARM_UICA_EVNT  uica_evnt;
	PARM_TRAC_EVNT	trac_evnt;
	PARM_NBRCHG_EVNT nbrchg_evnt;
	SMT_MANUF_DATA	manuf;
	PARM_ESF        esf;
} PARM_UNION;
#endif /* __SMTD_PARM_H */
