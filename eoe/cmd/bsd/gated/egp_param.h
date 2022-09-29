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
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/egp_param.h,v 1.1 1989/09/18 19:01:57 jleong Exp $
 *
 */

/* egp_param.h
 *
 * Defines various egp parameters
 */

/* Retry counts */

#define NACQ_RETRY	5 	/* Max. No. acquire retransmits before 
				   switch to longer interval */
#define NCEASE_RETRY	3 	/* Max. No. cease retransmits */
#define NPOLL		2	/* Max. No. NR polls to send or receive
				   with same id */

/* Acquire interval constants */

/* MINHELLOINT below is used for neigh. acquire retransmit interval when
 * in state UNACQUIRED or ACQUIRE_SENT or cease retry interval when not
 * acquired
 */
#define LONGACQINT	240	/* Neigh. acquire retransmit interval (sec)
				   when no response after NACQ_RETRY or
				   after ceased */
#define	ACQDELAY	3600	/* Delay (seconds) before try to reacquire
				   neighbor that is misbehaving */

/* Hello interval constants */

#define MINHELLOINT	30	/* Minimum interval for sending and
				   receiving hellos */
#define MAXHELLOINT	120	/* Maximum hello interval, sec. */
#define HELLOMARGIN	2	/* Margin in hello interval to allow for delay
				   variation in the network */
/* Poll interval constants */

#define MINPOLLINT	120	/* Minimum interval for sending and receiving
				   polls */
#define MAXPOLLINT  	480	/* Maximum poll interval, sec. */

/* repoll interval is set to the hello interval for the particular neighbor */

/* Reachability test constants */

#define NCOMMANDS	4	/* No. commands sent on which reachability is
				   based */
#define NRESPONSES	2	/* No. responses expected per NCOMMANDs sent,
				   if down, > NRESPONSES => up,
				   if up, < NRESPONSES => down */
#define NUNREACH	60	/* No. consecutive times neighbor is 
				   unreachbable before ceased */
#define MAXNOUPDATE	3	/* Maximum # successive polls (new id) for
				   which no update was received before cease
				   and try to acquire an alternative :/

/* Command reception rate constants */

#define	CHKCMDTIME	480	/* No. seconds betw. check for recv too many
				   acq., hello or poll commands */
#define NMAXCMD		20	/* Max. # acq., hello and poll commands
				   allowed during CHKCMDTIME seconds */
