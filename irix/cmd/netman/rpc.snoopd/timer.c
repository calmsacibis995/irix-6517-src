/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Timer functions for snoopd
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

#include "timer.h"

struct timeval curtime;

void
timer_set(struct timer* t, int sec, int usec, void (*func)(), void* arg)
{
	t->t_state = T_STOPPED;
        t->t_value.tv_sec = sec;
        t->t_value.tv_usec = usec;
	t->t_func = func;
	t->t_arg = arg;
}

void
timer_start(struct timer* t)
{
        t->t_remaining.tv_sec = t->t_value.tv_sec;
        t->t_remaining.tv_usec = t->t_value.tv_usec;
	t->t_state = T_RUNNING;
	(void) gettimeofday(&curtime, 0);
	t->t_lasttime = curtime;
}

void
timer_tick(struct timer* t)
{
	if (t->t_state == T_STOPPED)
		return;

	(void) gettimeofday(&curtime, 0);

	timesub(&t->t_ticked, &curtime, &t->t_lasttime);
	timesub(&t->t_remaining, &t->t_remaining, &t->t_ticked);
	if (t->t_remaining.tv_sec < 0 || t->t_remaining.tv_usec < 0) {
		if (t->t_func != 0)
			(*t->t_func)(t->t_arg);
		t->t_remaining.tv_sec = t->t_value.tv_sec;
		t->t_remaining.tv_usec = t->t_value.tv_usec;
	}

	t->t_lasttime = curtime;
}

void
timer_stop(struct timer* t)
{
	t->t_state = T_STOPPED;
}
