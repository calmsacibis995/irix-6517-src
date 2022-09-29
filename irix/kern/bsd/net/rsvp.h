#ifndef KRSVP_H
#define KRSVP_H
/*
 * Copyright 1995 Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

/*
 * The following bits are in psif_flags
 *
 * Values for ps_parms.flags: (only the lower 16 bits are used)
 * 
 * PS_DISABLED means don't do any packet scheduling.  Admission control is
 * still performed but the packet classification and scheduling are not.
 * ip_output calls the driver transmit routine directly instead of ps_output.
 */
#define PSIF_EXT_MASK		0x0000ffff
#define PS_DISABLED		0x00000001 

/* These are used internally by rsvp.c */
#define PSIF_INT_MASK		0xffff0000
#define PSIF_PSON		0x00010000
#define PSIF_POLL_TXQ		0x00020000
#define PSIF_TICK_INTR		0x00040000
#define PSIF_TXFREE_TRUE	0x00080000
#define PSIF_ADMIN_PSOFF	0x00100000


#ifdef _KERNEL

struct ps_parms {
	int bandwidth;		/* total bwidth of interface */
	int txfree;		/* max pckts on tx free list */
	int (*txfree_func)(struct ifnet *); /* returns tx free list length */
	/* func notifies driver if reservations are on/off */
	void (*state_func)(struct ifnet *, int);
	int flags;
};


/*
 * Flags for psif_lowerfg
 */
#define PSIFL_TXQ_FULL		0x00000001
#define PSIFL_GOT_STAT		0x00000002
#define PSIFL_IN_SEND		0x00000004

/*
 * defines for rsvp.c 
 * xxx_PKTS_PER_TICK is an estimate of how many MTU sized pkts the driver
 * can send in one tick.  It is used on TICK_INTR drivers.
 */
#define NUM_NC_MBUFS		5
#define MIN_CL_MBUFS		32
#define MIN_TXFREE		5
#define DEFAULT_10MB_BATCH	4
#define DEFAULT_100MB_BATCH	25
#define DEFAULT_IPG_XPI_BATCH	38
#define DEFAULT_EP_ET_BATCH	10
#define IPG_XPI_PKTS_PER_TICK	25
#define EP_PKTS_PER_TICK	8


extern void ps_xmit_done(struct ifnet *ifp);	/* old.  for compatibility */
extern void ps_txq_stat(struct ifnet *ifp, int curr_tx_free);
extern int ps_init(struct ifnet *ifp, struct ps_parms *parms);
extern int ps_reinit(struct ifnet *ifp, struct ps_parms *parms);

#endif /* _KERNEL */

#endif /* KRSVP_H */
