#ifndef __COMM__
#define __COMM__
/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Visual FDDI Managment System
 *
 *	$Revision: 1.5 $
 */

#define PROGRAM		"fddivis"
#define VERSION		1.0

#define FV_RGBFILE	"fddivis.rgb"
#define RCFILE		".fddivisrc"
#define IMAGESAVE	"/usr/sbin/scrsave"
#define	SMTPING		"/usr/etc/smtping"

#define CHILD_SIDE 1
#define PARENT_SIDE 0

#define TERMINATE	1
#define OPSIF		2
#define ANYSIF		3
#define RANGE		4
#define ISFRAME		5
#define ITSME		6
#define POLLMODE	7
#define POLL_PASSIVE		0
#define POLL_ACTIVE		1
#define POLL_AGGRESIVE		2
#define DUMP		8
#define FLUSH		9
#define ADDRMODE	10
#define ADDRMODE_NON		0
#define ADDRMODE_NFLAG		1
#define ADDRMODE_NSFLAG		2
#define FREEZE		11
#define SHRINK		12
#define GROW		13
#define DEMO		14
#define SEARCH		15
#define DRAWHAND	16
#define BITMODE		17
#define DOPOST		18
#define SEL_NIF			0x1
#define SEL_CSIF		0x2
#define SEL_OPSIF		0x4
#define SEL_SRF			0x8
#define SEL_CLM			0x10
#define SEL_BCN			0x20
#define FR_SEL		19
#define RINGOPR			0x0
#define WRAPPED			0x1
#define CLAIMING		0x2
#define BEACONING		0x4
#define DELETED			0x40000000
#define STALE			0x80000000
#define OPR_SIG		20
#define QINT		21
#define AGEFACTOR	22
#define MAGADDR		23
#define DOSTAT		24
#define DINT		25
#define DDIR		26
#define DRECORD		27
#define DREPLAY		28

#define RINGTIMO        (1*T_NOTIFY+1)
#define REGTIMO         (60*10)		/* 10 min. */

#define MYPOSITION	((float)900)
#define FAR 0x7fff
#define ANGINC 60
#define PULSE 50
#define RADIUS 15

#define SIZETHRESHOLD	10
#define AEDGE	8
#define BNR	24

/*
 * gl defs
 */
#define RAD(x) ((((double)x * (double)M_PI) / (double)1800.0))
#define DEG(r) (((double)1800.0 * r) / (double)M_PI)

#define FVC_BLACK	0
#define FVC_RED		1
#define FVC_GREEN	2
#define FVC_YELLOW	3
#define FVC_BLUE	4
#define FVC_PURPLE	5
#define FVC_WHITE	7
#define FVC_BGREY	38
#define FVC_DGREY	41
#define FVC_GREY	45
#define FVC_LGREY	50
#define FVC_PGREEN	58
#define FVC_RED2	81
#define FVC_ORANGE	90
#define FVC_GOLD	92
#define FVC_TAN		215
#define FVC_BBLUE	226
#define FVC_LTBLUE	247

#define blackcol	FVC_BLACK
#define redcol		FVC_RED
#define greencol	FVC_GREEN
#define yellowcol	FVC_YELLOW
#define bluecol		FVC_BLUE
#define purplecol	FVC_PURPLE
#define whitecol	FVC_WHITE
#define bgreycol	FVC_BGREY
#define dgreycol	FVC_DGREY
#define greycol		FVC_GREY
#define lgreycol	FVC_LGREY
#define pgreencol	FVC_PGREEN
#define red2col		FVC_RED2
#define orangecol	FVC_ORANGE
#define goldcol		FVC_GOLD
#define bbluecol	FVC_BBLUE
#define tancol		FVC_TAN
#define ltbluecol	FVC_LTBLUE

/*
 * Overlay colors
 */
#define GREENOVLY	1
#define GREYOVLY	2
#define REDOVLY		3

#undef c3i
#define c3i color

#define SCRXMAX	getgdesc(GD_XPMAX)
#define SCRYMAX	getgdesc(GD_YPMAX)

#endif
