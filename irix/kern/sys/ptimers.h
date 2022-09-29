/*
 * Copyright 1996-1997 Silicon Graphics, Inc. 
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
 * rights reserved under the Copyright Lanthesaws of the United States.
 */
#ifndef __SYS_PTIMERS_H__
#define __SYS_PTIMERS_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Id: ptimers.h,v 1.4 1997/01/15 04:44:41 jwag Exp $"

/*
 * NOTE: this header is included as part of a POSIX1b header (time.h)
 * Watch for name space pollution
 */
#include <sys/timespec.h>

typedef struct itimerspec {
	timespec_t it_interval;	/* timer period */
	timespec_t it_value;	/* timer expiration */
} itimerspec_t;
/*
 * Clock definitions used by above POSIX clock functions
 */
#define CLOCK_REALTIME 	1		/* The time of day clock (POSIX) */
/* These are special SGI clocks, not standard to POSIX  */
#define CLOCK_SGI_CYCLE 2		/* Free running hardware counter */
#define CLOCK_SGI_FAST	3		/* Clock used to set fast timers */
/*
 * Flags used in timer_settime()
 */
#define TIMER_ABSTIME	0x00000001	/* Flag that time is absolute */

#if _KERNEL
#include <sys/signal.h>
/*
 * structure to represent posix timers in the proc structure.
 */
#define _SGI_POSIX_TIMER_MAX 32		/* Number of posix timers per proc */

typedef struct ptimer_info {
	__int64_t	next_timeout;	/* Time/timeto next timeout */
    	__int64_t 	interval_tick;	/* Number of ticks in interval */
	int 		signo;		/* Signal to send */
	sigval_t 	value;		/* Value to pass to signal */
	int 		clock_type;	/* What clock to use, 0 is unused */
    	toid_t		next_toid;	/* id of next timeout */
    	int		overrun_cnt;	/* count of extra timer expirations */
} ptimer_info_t;
#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif /* !__SYS_PTIMERS_H__ */
