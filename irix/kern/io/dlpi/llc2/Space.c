
/******************************************************************
 *
 *  SpiderX25 - LLC2 Multiplexer
 *
 *  Copyright 1991 Spider Systems Limited
 *
 *  SPACE.C for LLC2
 *
 *    Kernel structures for LLC2.
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/0/s.Space.c
 *	@(#)Space.c	1.13
 *
 *	Last delta created	16:01:56 4/30/93
 *	This file extracted	16:55:27 5/28/93
 *
 */

#ident "@(#)Space.c	1.13 (Spider) 5/28/93"

/* Read queue control */
#define RQHWM		     3  /* High water mark of queued read messages  */
#define RQLWM		     2  /* Low water mark of queued read messages   */

/* LLC2 default tuning values */
#define N2_DFLT		     20 /* Maximum number of retries		    */
#define T1_DFLT		     9  /* Acknowledgement time     (unit 0.1 sec)  */
#define TPF_DFLT             7  /* P/F cycle retry time     (unit 0.1 sec)  */
#define TREJ_DFLT           24  /* Reject retry time        (unit 0.1 sec)  */
#define TBUSY_DFLT          96  /* Remote busy check time   (unit 0.1 sec)  */
#define TIDLE_DFLT         240  /* Idle P/F cycle time      (unit 0.1 sec)  */
#define ACKDLY_DFLT          4  /* RR delay time            (unit 0.1 sec)  */
#define NACKMAX_DFLT         3  /* Maximum number of unack'ed Rx I-frames   */
#define TXWIND_DFLT          7  /* Transmit window (if no XID received)     */
#define TXPRBE_DFLT          0  /* P-bit position before end of Tx window   */
#define MAXILEN_DFLT      1031  /* Maximum I-frame length		    */
#define XIDWIND_DFLT         7  /* XID window size (receive window)         */
#define XIDNDUP_DFLT         2  /* Duplicate MAC XID count  (0 => no test)  */
#define XIDTDUP_DFLT         3  /* Duplicate MAC XID time   (unit 0.1 sec)  */


/* --------------  C O U N T S   A N D   S T R U C T U R E S  ------------- */

#ifdef ENP
#define FAR
#endif

#ifdef DEVMT
#define FAR far
#endif


#define FAR
#define STATIC static
typedef unsigned char *bufp_t;
#define BUFP_T bufp_t

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/snet/uint.h>
#include <sys/snet/ll_proto.h>
#include <sys/snet/ll_control.h>
#include <sys/snet/dl_proto.h>
#include <sys/snet/timer.h>
#include <sys/snet/llc2.h>

/* Read queue length limits */
uint     llc2_rqhwm = RQHWM;
uint     llc2_rqlwm = RQLWM;

/* Default tuning value table */
llc2tune_t llc2_tdflt = {
    N2_DFLT,        /* N2:          Maximum number of retries		    */
    T1_DFLT,        /* T1:          Acknowledgement time    (unit 0.1 sec)  */
    TPF_DFLT,       /* Tpf:         P/F cycle retry time    (unit 0.1 sec)  */
    TREJ_DFLT,      /* Trej:        Reject retry time       (unit 0.1 sec)  */
    TBUSY_DFLT,     /* Tbusy:       Remote busy check time  (unit 0.1 sec)  */
    TIDLE_DFLT,     /* Tidle:       Idle P/F cycle time     (unit 0.1 sec)  */
    ACKDLY_DFLT,    /* ack_delay:   RR delay time           (unit 0.1 sec)  */
    NACKMAX_DFLT,   /* notack_max:  Maximum number of unack'ed Rx I-frames  */
    TXWIND_DFLT,    /* tx_window:   Transmit window (if no XID received)    */
    TXPRBE_DFLT,    /* tx_probe:    P-bit position before end of Tx window  */
    MAXILEN_DFLT,   /* max_I_len:   Maximum I-frame length		    */
    XIDWIND_DFLT,   /* xid_window:  XID window size (receive window)        */
    XIDNDUP_DFLT,   /* xid_Ndup:    Duplicate MAC test maximum tries        */
    XIDTDUP_DFLT    /* xid_Tdup:    Duplicate MAC XID time  (unit 0.1 sec)  */
};

