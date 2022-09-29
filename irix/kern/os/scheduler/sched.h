/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _SYS_SCHED_H_
#define _SYS_SCHED_H_

#include <standards.h>

/*
 * POSIX Scheduling Header File.
 */


/*
 * Scheduling Policies
 */
#define SCHED_FIFO	1
#define SCHED_RR	2
#define	SCHED_TS	3
#define	SCHED_NP	4

#define SCHED_OTHER	SCHED_TS

#if _SGIAPI
#include <sys/param.h>
/*
 * Priority Ranges
 */
#define	PX_PRIO_MIN	0
#define PX_PRIO_MAX	255
#define TS_PRIO_MAX	2*NZERO
#define TS_PRIO_MIN	1
#endif

/*
 * Scheduling Parameter Structure
 */
struct sched_param {
	int sched_priority;
};

#endif
