/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/rip.h,v 1.1 1989/09/18 19:02:10 jleong Exp $
 *
 */

/*
 * When we find any interfaces marked down we rescan the
 * kernel every CHECK_INTERVAL seconds to see if they've
 * come up.
 */
#ifdef sgi
#define CHECK_INTERVAL  TIMER_RATE      /* check every time */
#else
#define	CHECK_INTERVAL	(1*60)
#endif /* sgi */
#define RIP_INTERVAL	30

#define IPPROTO_RIP 520
#define RIP_PORT 520
#define RIPHOPCNT_INFINITY	16

#define	min(a,b)	((a)>(b)?(b):(a))

struct sockaddr_in addr;

char	rip_packet[RIPPACKETSIZE+1];

extern struct rip *ripmsg;

struct	servent *sp;

int	sendripmsg();
int	supply();
