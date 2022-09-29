#ifndef __SMTD_EVENT_H
#define __SMTD_EVENT_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.9 $
 */

#include <sys/time.h>

#define EVNT_SMTCONF	0x1	/* MADATORY: OPTIONAL: smt_cc_evnt */
#define EVNT_DUPA	0x4	/* MADATORY: OPTIONAL: mac_dupa_cond */
#define EVNT_FRAMEERR	0x8	/* MADATORY: OPTIONAL: mac_fe_cond */
#define EVNT_NOTCOPIED	0x10	/* OPTIONAL: OPTIONAL: mac_nc_cond */
#define EVNT_NBRCHG	0x20	/* MADATORY: OPTIONAL: mac_nbrchg_cond */
#define EVNT_PLER	0x40	/* MADATORY: MADATORY: port_ler_cond */
#define EVNT_UCA	0x80	/* MADATORY: MADATORY: port_uca_cond */
#define EVNT_EBE	0x100	/* OPTIONAL: OPTIONAL: port_ebe_cond */

/* event types */
#define EVTYPE_DEASSERT	0
#define EVTYPE_ASSERT	1
#define EVTYPE_EVENT	2

typedef struct evq {
	struct evq *next;	/* what else */
	SMTD   *context;	/* context */
	u_long event;		/* event flag */
	u_long tick_start;	/* start ticking */
	u_long tick_cnt;	/* tick count */
	u_long infolen;		/* parameter length */
	char   info[64];	/* parameter + space for tlv en/decode */
} EVQ;

extern void sr_init();
extern void sr_event(int, int, void *);

#endif /* __SMTD_EVENT_H */
