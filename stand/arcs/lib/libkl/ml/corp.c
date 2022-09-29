/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.2 $"

#include <corp.h>

/*
 * Corp: Coroutines for PROMs
 *
 *   See corp.h for standard usage of this module.
 */

void corp_init(corp_t *c)
{
    c->count = 0;
}

void corp_fork(corp_t *c,
	       void (*entry)(corp_t *c, __uint64_t arg),
	       __uint64_t *sp, __uint64_t arg)
{
    c->proc[c->count].wake_time = rtc_time();

    if (! setjmp(c->proc[c->count++].context))
	corp_jump(c, (__uint64_t) entry, sp, arg);
}

void corp_exit(corp_t *c)
{
    jmp_buf		go;
    int			earliest, i;
    rtc_time_t		now	= rtc_time();

    for (earliest = 0, i = 1; i < c->count; i++)
	if (c->proc[i].wake_time < c->proc[earliest].wake_time)
	    earliest = i;

    if (c->proc[earliest].wake_time > now)
	rtc_sleep(c->proc[earliest].wake_time - now);

    memcpy(go, c->proc[earliest].context, sizeof (go));

    memcpy((void *) &c->proc[earliest	 ],
	   (void *) &c->proc[earliest + 1],
	   (--c->count - earliest) * sizeof (corp_proc_t));

    longjmp(go, 1);
}

void corp_sleep(corp_t *c, rtc_time_t usec)
{
    c->proc[c->count].wake_time = rtc_time() + usec;

    if (! setjmp(c->proc[c->count++].context))
	corp_exit(c);
}

void corp_wait(corp_t *c)
{
    while (c->count)
	corp_sleep(c, 0);
}
