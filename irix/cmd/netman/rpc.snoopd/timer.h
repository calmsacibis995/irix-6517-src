#ifndef TIMER_H
#define TIMER_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Timer include file
 *
 *	$Revision: 1.3 $
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

#include <sys/time.h>

enum timerflag {
	T_STOPPED = 0,
	T_RUNNING = 1
};

struct timer {
	enum timerflag	t_state;
	void		(*t_func)();
	void		*t_arg;
	struct timeval	t_value;
	struct timeval	t_remaining;
	struct timeval	t_lasttime;
	struct timeval	t_ticked;
};

#define timer_isrunning(timer)	((timer)->t_state == T_RUNNING)

#define timesub(res, t2, t1)					\
	(res)->tv_sec = (t2)->tv_sec - (t1)->tv_sec;		\
	(res)->tv_usec = (t2)->tv_usec - (t1)->tv_usec;		\
	if (((res)->tv_sec > 0 && (res)->tv_usec < 0) ||	\
	    ((res)->tv_sec <= 0 && (res)->tv_usec <= -1000000))	\
		(res)->tv_sec--, (res)->tv_usec += 1000000

#define timeadd(res, t1, t2)					\
	(res)->tv_sec = (t1)->tv_sec + (t2)->tv_sec;		\
	(res)->tv_usec = (t1)->tv_usec += (t2)->tv_usec;	\
	if ((res)->tv_usec > 1000000) {				\
		(res)->tv_sec++;				\
		(res)->tv_usec -= 1000000;			\
	}

extern struct timeval curtime;

void timer_set(struct timer *, int, int, void (*)(), void *);
void timer_start(struct timer *);
void timer_tick(struct timer *);
void timer_stop(struct timer *);

#endif /* !TIMER_H */
