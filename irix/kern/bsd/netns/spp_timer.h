/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)spp_timer.h	7.1 (Berkeley) 3/12/88
 */
#ifdef __cplusplus
extern "C" {
#endif
#ifdef sgi
# ifdef _KERNEL
#  define KERNEL
# endif
#endif

/*
 * Definitions of the SPP timers.  These timers are counted
 * down PR_SLOWHZ times a second.
 */
#define	SPPT_NTIMERS	4

#define	SPPT_REXMT	0		/* retransmit */
#define	SPPT_PERSIST	1		/* retransmit persistance */
#define	SPPT_KEEP	2		/* keep alive */
#define	SPPT_2MSL	3		/* 2*msl quiet time timer */

/*
 * The SPPT_REXMT timer is used to force retransmissions.
 * The SPP has the SPPT_REXMT timer set whenever segments
 * have been sent for which ACKs are expected but not yet
 * received.  If an ACK is received which advances tp->snd_una,
 * then the retransmit timer is cleared (if there are no more
 * outstanding segments) or reset to the base value (if there
 * are more ACKs expected).  Whenever the retransmit timer goes off,
 * we retransmit one unacknowledged segment, and do a backoff
 * on the retransmit timer.
 *
 * The SPPT_PERSIST timer is used to keep window size information
 * flowing even if the window goes shut.  If all previous transmissions
 * have been acknowledged (so that there are no retransmissions in progress),
 * and the window is too small to bother sending anything, then we start
 * the SPPT_PERSIST timer.  When it expires, if the window is nonzero,
 * we go to transmit state.  Otherwise, at intervals send a single byte
 * into the peer's window to force him to update our window information.
 * We do this at most as often as SPPT_PERSMIN time intervals,
 * but no more frequently than the current estimate of round-trip
 * packet time.  The SPPT_PERSIST timer is cleared whenever we receive
 * a window update from the peer.
 *
 * The SPPT_KEEP timer is used to keep connections alive.  If an
 * connection is idle (no segments received) for SPPTV_KEEP amount of time,
 * but not yet established, then we drop the connection.  If the connection
 * is established, then we force the peer to send us a segment by sending:
 *	<SEQ=SND.UNA-1><ACK=RCV.NXT><CTL=ACK>
 * This segment is (deliberately) outside the window, and should elicit
 * an ack segment in response from the peer.  If, despite the SPPT_KEEP
 * initiated segments we cannot elicit a response from a peer in SPPT_MAXIDLE
 * amount of time, then we drop the connection.
 */

#define	SPP_TTL		30		/* default time to live for SPP segs */
/*
 * Time constants.
 */
#define	SPPTV_MSL	( 15*PR_SLOWHZ)		/* max seg lifetime */
#define	SPPTV_SRTTBASE	0			/* base roundtrip time;
						   if 0, no idea yet */
#define	SPPTV_SRTTDFLT	(  3*PR_SLOWHZ)		/* assumed RTT if no info */

#define	SPPTV_PERSMIN	(  5*PR_SLOWHZ)		/* retransmit persistance */
#define	SPPTV_PERSMAX	( 60*PR_SLOWHZ)		/* maximum persist interval */

#define	SPPTV_KEEP	( 75*PR_SLOWHZ)		/* keep alive - 75 secs */
#define	SPPTV_MAXIDLE	(  8*SPPTV_KEEP)	/* maximum allowable idle
						   time before drop conn */

#define	SPPTV_MIN	(  1*PR_SLOWHZ)		/* minimum allowable value */
#define	SPPTV_REXMTMAX	( 64*PR_SLOWHZ)		/* max allowable REXMT value */

#define	SPP_LINGERTIME	120			/* linger at most 2 minutes */

#define	SPP_MAXRXTSHIFT	12			/* maximum retransmits */

#ifdef	SPPTIMERS
char *spptimers[] =
    { "REXMT", "PERSIST", "KEEP", "2MSL" };
#endif

/*
 * Force a time value to be in a certain range.
 */
#define	SPPT_RANGESET(tv, value, tvmin, tvmax) { \
	(tv) = (value); \
	if ((tv) < (tvmin)) \
		(tv) = (tvmin); \
	else if ((tv) > (tvmax)) \
		(tv) = (tvmax); \
}

#ifdef KERNEL
extern int spp_backoff[];
#endif
#ifdef __cplusplus
}
#endif
