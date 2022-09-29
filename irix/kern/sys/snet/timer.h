/******************************************************************
 *
 *  SpiderX25 - Timing Module
 *
 *  Copyright 1991 Spider Systems Limited
 *
 *  TIMER.H
 *
 *    Multiple timer module header file.
 *
 ******************************************************************/


/*
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/include/sys/snet/0/s.timer.h
 *	@(#)timer.h	1.4
 *
 *	Last delta created	18:06:14 4/13/92
 *	This file extracted	14:53:31 11/13/92
 *
 */
#ifndef _X25_TIMER_H
#define _X25_TIMER_H

/* Lock out clock ISR */
#define splclock()   splhi()

/* Timers header, used to process expiries */
typedef struct thead
{
    void         (*th_expfunc)(caddr_t);
    caddr_t        th_exparg;
    struct x25_timer  *th_expired;
} thead_t;

/* Individual x25_timer */
typedef struct x25_timer
{
    unchar         tm_id;
    unchar         tm_offset;
    ushort         tm_left;
    struct x25_timer  *tm_next;
    struct x25_timer **tm_back;
} x25_timer_t;


extern int init_timers(thead_t*, x25_timer_t *, int, void (*)(),caddr_t);
extern int set_timer(register x25_timer_t*, unsigned short);
extern int stop_timers(thead_t*, x25_timer_t *, int);
extern int run_timer(register x25_timer_t*, unsigned short);

#endif /* X25_TIMER_H */
