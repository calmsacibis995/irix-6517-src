#ifndef __SUBR__
#define __SUBR__
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.12 $
 */

#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
extern "C" {
#endif
#include <sm_types.h>
#include <smtd_parm.h>
#include <smtd.h>
#include <smtd_fs.h>
#include <sm_addr.h>
#include <sm_map.h>
#ifdef _LANGUAGE_C_PLUS_PLUS_2_0
}
#endif
#include "comm.h"

//#define DBG_PICK	1
//#define DBG_HAND	1
//#define DBG_RANGE	1
//#define DBG_SIF	1
//#define DBG_RING	1
//#define DBG_MAP	1
//#define DBG_UI	1
//#define DBG_VERB	1

#ifdef DBG_RING
#define DBGRING(a)	printf a
#else
#define DBGRING(a)
#endif

#ifdef DBG_MAP
#define DBGMAP(a)	printf a
#else
#define DBGMAP(a)
#endif

#ifdef DBG_UI
#define DBGUI(a)	printf a
#else
#define DBGUI(a)
#endif

#ifdef DBG_VERB
#define DBGVERB(a)	printf a
#else
#define DBGVERB(a)
#endif

#ifdef DBG_PICK
#define DBGPICK(a)	printf a
#else
#define DBGPICK(a)
#endif

#ifdef DBG_SIF
#define DBGSIF(a)	printf a
#else
#define DBGSIF(a)
#endif

#define TERM_SIGNAL	SIGQUIT

#define OFF		1
#define ON		2

#define FSI_STD		0x1
#define FSI_VERS	0x2
#define FSI_STAT	0x4
#define FSI_SP		0x8	/* station policy */
#define FSI_MACNBRS	0x10
#define FSI_PDS		0x20	/* path descriptors */
#define FSI_UNA		0x40
#define FSI_SIF		(FSI_VERS|FSI_SP|FSI_MACNBRS|FSI_PDS)
#define FSI_MAC_STAT	0x100
#define FSI_LER		0x200	/* link error */
#define FSI_MFC		0x400	/* mac frame counters */
#define FSI_MFNCC	0x800	/* mac frame not copied counters */
#define FSI_MPV		0x1000	/* mac priority values */
#define FSI_EBS		0x2000	/* elasticity buffer stat */
#define FSI_MF		0x4000	/* manufacturer data */
#define FSI_USR		0x8000	/* user data */

#define FSISET(x, y) ((x&y) == y)

#define DUMP_RING	0
#define DUMP_TREQ	1
#define DUMP_TNEG	2
#define DUMP_TMAX	3
#define DUMP_TVX	4
#define DUMP_TMIN	5
#define DUMP_SBA	6
#define DUMP_FRAME	7
#define DUMP_ERR	8
#define DUMP_LOST	9
#define DUMP_LER	10
#define DUMP_LEM	11
#define DUMP_RECV	12
#define DUMP_XMIT	13
#define DUMP_NCC	14
#define DUMP_EBS	15
#define DUMPCNT		16

/* union discriminant */
typedef struct dumpinfo {
	u_long	d_u;
	float	d_f;
	u_long	z_u;
	float	z_f;
} DUMPINFO;

typedef struct sinfo {
	LFDDI_ADDR sa;
	LFDDI_ADDR una;
	LFDDI_ADDR dna;
	int		sif;
	char		saname[64];
	PARM_PD_PHY	*primary;
	PARM_PD_PHY	*secondary;

	PARM_STD	std;	/* Station Descriptor */
	int		vers;	/* Version */
	PARM_STAT	stat;	/* Station State */
	PARM_SP		sp;	/* Station Policies */
	PARM_MACNBRS	*nbrs;	/* Neighbors */
	PARM_PD_PHY	*pds;	/* Path Descriptor */

	PARM_MAC_STAT	*mac_stat;
	PARM_LER	*ler;
	PARM_MFC	*mfc;
	PARM_MFNCC	*mfncc;
	PARM_MPV	*mpv;
	PARM_EBS	*ebs;
	SMT_MANUF_DATA	mf;
	USER_DATA	usr;
	DUMPINFO	zdump[DUMPCNT];
	int		space[10];	/* forward compat buffer */
} SINFO;

typedef struct ring {
	struct ring *next;
	struct ring *prev;

	SMT_STATIONID sid;
	int smac;
#define GHOST		0x1
#define SHADOW		0x2
	int ghost;
	unsigned long cursort;	/* marker to prevent looping when sorting ring*/

	int datt;	/* dual attach */
	int mports;	/* num of M ports */

	struct timeval tms;

	/* position */
	float ang;
	double radian;

	/* Station Colors */
	int lcp;	/* outline color */
	int pcp;	/* face color */
	int scrcp;	/* screen color */
	int acp;	/* address color */
	int litecp;	/* life lite color */

#define LT_LINK		0
#define LT_WRAP		1
#define LT_TWIST	2
#define LT_STAR		4
#define LT_STARWRAP	5
	/* Link */
	int blt;	/* backward link type */
	int blcp;	/* backward link color */
	int bls;	/* backward link style */

	int flt;	/* forward primary link type */
	int flcp;	/* forward primary link color */
	int fls;	/* forward primary link style */
	int fslcp;	/* forward secondary link color */
	int fsls;	/* forward secondary link style */

	int health;
	int rooted;
	struct ring *shadow;	/* shadow of Multi MAC concentrator */
	int space[17];	/* forward compat buffer */

	SINFO sm;
	SINFO dm;
} RING;
#define r_nodeclass	std.nodeclass
#define r_mac_ct	std.mac_ct
#define r_nonmaster_ct	std.nonmaster_ct
#define r_master_ct	std.master_ct
#define r_topology	stat.topology
#define r_dupa		stat.dupa
#define r_conf_policy	sp.conf_policy
#define r_conn_policy	sp.conn_policy

typedef struct recplay {
	struct	recplay *next;
	time_t	ts;	/* c_time */
	char	name[128];
	int	RecNum;
	int	RecStat;
	RING   *RecRp;
} RECPLAY;

#define SAMEADR(a,b)	(!bcmp((char*)&(a), (char*)&(b), sizeof(a)))
#define NULLADR(a)	(SAMEADR(a,zero_mac_addr) \
			 || SAMEADR(a,unknown_mac_addr))

extern void workstation(RING*, float);
extern void drawlink(RING*, float);
extern void drawbase(RING*);
extern void drawmonitor(RING*);
extern void drawmac2(RING*);
extern void drawcc(RING*);
extern void drawkeybd(RING*);
extern void drawgate(Coord, Coord, char*, int);
extern void opengate(int, Coord, Coord, Coord);

/* 1st arg to drawscene */
#define PICK_NONE	-7
#define PICK_MODE	-2

#define DOV3FS(x) v3f(x[0]); v3f(x[1]); v3f(x[2]); v3f(x[3])

/* gap between primary and secondary rings */
#define RINGGAP		5.0
#define HALFGAP		(RINGGAP/2.0)
#define QUATERGAP	(RINGGAP/4.0)

extern void getwshcol(char*, int);
extern void drawstr(Coord, Coord, char*);
extern void vert3f(float, float, float);
extern int  update_sif(SINFO *, SMT_FB*);
extern void dumpsif(FILE*, SINFO*);
extern void paintsif(RING*, float, float);
extern int  ler_alarm(SINFO*, int);
extern void cleanup_tmp(void);
extern void dostat(char *);
extern void dumpring(int, RING *, int);
extern void pr_sif(char *, int, int, RING*);
extern void visit(LFDDI_ADDR *);
extern char *getlbl(LFDDI_ADDR *);
extern char *getinfolbl(SINFO *);
extern int  check_smtd(void);
extern void SetStationColor(RING*);

extern void dopost(char*);

extern void fr_finish(void);
extern void recvframe(void);

#ifdef CHECKRING
extern void checkring(int, RING *);
#else
#define checkring(a, b)
#endif

#endif
