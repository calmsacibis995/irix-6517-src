
/*
 * @(#) $Id: rsvp_timer.c,v 1.8 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_timer.c ***************************
 *                                                               *
 *  Timer package for the RSVP daemon (rsvpd)                    *
 *                                                               *
 *****************************************************************/
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version:  Steven Berson & Bob Braden, May 1996.

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

/*
 * This timer package is based on
 * virtual time in miliseconds and on the assumption that there exists a
 * current time (time_now). The queue is ordered by decreasing time values,
 * and the q. is examined once in a time slice (granularity of the timer).
 * When events are due, they are executed.
 */

#include "rsvp_daemon.h"

/*	Timer queue element structure
 */
typedef struct Time_rec {
	struct Time_rec *next;
	unsigned long   due_time;
	char           *user_ptr;
	int             user_data;
}   time_rec;

static time_rec *time_q = NULL, *free_q = NULL;

unsigned long   time_now = 0;

/* external declarations */
extern int	resv_refresh_TimeOut(), path_refresh();
extern int	refresh_api();
extern int	api_refresh_delay(int);
extern void	rtap_sleepdone();

/* forward declarations */
void		del_from_timer(char *, int);
static time_rec *find_timer(void *, int);
void		print_timer_record(time_rec *tp);
int		RandomDelay(int);

/*
 * init_timer(): initializes the timer data structures. 
 */
int
init_timer(int q_size) {
	int             i;

	time_q = NULL;
	free_q = (time_rec *) calloc((unsigned) q_size, sizeof(time_rec));

	if (free_q == NULL) {
		log(LOG_ERR, 0, "Memory allocation for timer queue", 0);
		exit(1);
	}
	for (i = 0; i < q_size; i++)	/* chain all the free together */
		free_q[i].next = &(free_q[i + 1]);
	free_q[q_size - 1].next = NULL;
	return(0);
}


/*
 * add_to_timer():   Enters event into timer queue, in order by expiration.
 *	An event is identified by two values: A pointer and an integer.
 *	The pointer points to data related to the event, and the integer
 *	has flags to determin the type of the event (path, resv etc).
 */
void
add_to_timer(user_ptr, user_data, delay)
	char		*user_ptr;	/* a pointer to status  */
	int		user_data;	/* and integer flags    */
	int		delay;
{
	time_rec	**t, *tmp;
	unsigned long	due;

	assert(delay >= 0);

	/*
	 *  If this event already exists with a smaller due time,
	 *  then just return
	 */
	tmp = find_timer(user_ptr, user_data);
	if (tmp) {
		if (IsDebug(DEBUG_TIMERS)) {
			log(LOG_DEBUG, 0, "Event already exists ...\n");
			print_timer_record(tmp);
		}
		if (LTE(tmp->due_time, time_now+delay))
			return;
		else
			del_from_timer(user_ptr, user_data);
	}
	if ((tmp = free_q) == NULL) {	/* no more q. records   */
		log(LOG_WARNING, 0, "No free queue records", 0);
		assert(free_q != NULL);		/* i.e. abort() */
		return;
	}
	free_q = free_q->next;	/* get if off the free list */
	tmp->due_time = due = time_now + delay;
	tmp->user_ptr = user_ptr;
	tmp->user_data = user_data;
	for (t = &time_q; *t != NULL && LTE((*t)->due_time, due);
	     t = &((*t)->next));
	tmp->next = *t;
	*t = tmp;
	if (IsDebug(DEBUG_TIMERS)) {
		log(LOG_DEBUG, 0, "Added timer event - ");
		print_timer_record(tmp);
		timer_print();
	}
}

/*
 * del_from_timer():  Delete an event from the timer queue before its
 *	expiration time.
 */
void
del_from_timer(user_ptr, user_data)
	char           *user_ptr;	/* a pointer to status  */
	int             user_data;	/* and integer flags    */
{
	time_rec      **t, *tmp;

	for (t = &time_q; (*t) != NULL;) {
		if ((*t)->user_ptr != user_ptr || (*t)->user_data != user_data)
			t = &((*t)->next);
		else {		/* found a record to delete */
			tmp = *t;		/* unchain it  */
			*t = (*t)->next;
			tmp->next = free_q;	/* insert into free list   */
			free_q = tmp;
			if (IsDebug(DEBUG_TIMERS)) {
				log(LOG_DEBUG, 0, "Deleted timer event - ");
				print_timer_record(tmp);
			}
		}
	}
}

static time_rec *
find_timer(void *ptr, int code) {
	time_rec *tp;
	for (tp = time_q; tp; tp = tp->next) {
		if (tp->user_data == code && tp->user_ptr == (char *)ptr)
			return(tp);
	}
	return(tp);
}

/*
 * timer_wakeup():  Awakened periodically from select (period = granularity),
 *	to dequeue and execute all expired events in timer queue.
 */
void
timer_wakeup(int sig) {
	/* Activate all expired events */
	while (time_q != NULL && LTE(time_q->due_time, time_now)) {
		time_rec *t = time_q;
		time_q = time_q->next;	/* get it off the time_q and */
		t->next = free_q;	/* into the free list        */
		free_q = t;
		if (IsDebug(DEBUG_TIMERS)) {
			log(LOG_DEBUG, 0, "Activate timer event - ");
			print_timer_record(t);
			timer_print();
		}
		timer_signal(t->user_ptr, t->user_data);
	}
}

u_long
next_event_time() {
	if (time_q)
		return(time_q->due_time);
	else
		return(-1);
}

/*
 * timer_signal(): Execute expired event.
 */
int
timer_signal(user_ptr, user_data)
	char           *user_ptr;
	int             user_data;
{
	int             type = user_data & 0xff;
	int		rc, delay;
	Session		*destp;

	Log_Event_Cause = "T";
	switch (type) {

	    case TIMEV_API:
		/*  Refresh information from API.
		 * 	 user_ptr is the SID which indexes rsvp_vec[]
		 */
		rc = refresh_api((int) user_ptr);
		delay = api_refresh_delay((int) user_ptr);
		break;

	    case TIMEV_PATH:
		/*  Send Path refresh message(s) for specific destination.
		 *	refresh_path -> -1 if the dest expired (ttd arrived)
		 */
		destp = (Session *)user_ptr;
		rc = path_refresh(destp);
/*** experimental code
		if (destp->d_refr_p.d_fast_n > 0) {
			destp->d_refr_p.d_fast_n--;
			delay = REFRESH_FAST_T;
		}
		else
 ***/
		if (!rc)
 			delay = (int) RandomDelay(destp->d_Rtimop);
		break;

	    case TIMEV_RESV:
		/* Send Resv refresh messages(s) for specific destination.
		 *	resv_refresh -> -1 if the dest expired (ttd arrived)
		 */
		destp = (Session *)user_ptr;
		rc = resv_refresh_TimeOut(destp);
 		delay = (int) RandomDelay(destp->d_Rtimor);
		break;

	    case TIMEV_LocalRepair:
		/*	Delay of W seconds has passed to allow routing
		 *	protocol to settle down.  Now refresh Path state.
		 */
		destp = (Session *)user_ptr;
		rc = path_refresh(destp);
		delay = -1;	/* Do NOT reschedule */
		break;	

#ifdef RTAP
	    case TIMEV_RTAPsleep:
		/* Sleep command of integral rtap has expired.
		 */
		rtap_sleepdone();
		delay = -1;	/* Do NOT reschedule */
		break;
#endif
	    default:
		assert(0);
		return(-1);
	}
	/*
	 *	Reschedule event if appropriate
	 */
	if (rc == 0 && delay > 0)
		add_to_timer(user_ptr, user_data, delay);
	Log_Event_Cause = NULL;
	return(0);
}



/*
 * Compute_TTD(): Compute "time-to-die" in internal timer units,
 *	given ptr to TIME_VALUES struct in Session.
 * */
u_int32_t
Compute_TTD(TIME_VALUES *tvp)
	{
	u_int32_t t, Tval;

	Tval = tvp->timev_R;
	assert(Tval);	/* Check earlier! */
	t = time_now + (K_FACTOR+0.5) * 1.5 * MAX(Tval, MIN_TIMER_PERIOD);
	return(t);
}

/* Compute_R(): Given ptr to appropriate TIME_VALUES struct (in Session
 *	struct), compute appropriate refresh timeout period and update
 *	value in Session struct.  Return original value; used as
 *	'first-time' switch to start timer when state first appears.
 */
u_int32_t
Compute_R(TIME_VALUES *tvp)
	{
	u_int32_t timnew, timold;

	timnew = REFRESH_DEFAULT;
	/*	Base R could depend on load.
	 */

	if ((timold = tvp->timev_R)) {
		/*
		 *	Limit slew rate
		 */
		tvp->timev_R = MIN(timnew, (1+REFRESH_SLEW_MAX)*timold);
	}
	else
		tvp->timev_R = timnew;
	return(timold);
}


void
print_timer_record(time_rec *tp)
	{
	Session		*dp;
	int           	sid;
	int             type = tp->user_data & 0xff;
#define XXX_MASK 0xff
	int             flags = (tp->user_data >> 8) & XXX_MASK;
	int		delta = tp->due_time - time_now;

	switch (type) {

	case TIMEV_PATH:
		dp = (Session *) tp->user_ptr;
		log(LOG_DEBUG, 0,"Refresh path %s:%d in %d seconds\n",
		       inet_ntoa(dp->d_addr), ntoh16(dp->d_port), delta/1000);
		break;

	case TIMEV_RESV:
		dp = (Session *) tp->user_ptr;
		log(LOG_DEBUG, 0, "Refresh reservation %s:%d in %d seconds\n",
			inet_ntoa(dp->d_addr), ntoh16(dp->d_port), delta/1000);
		break;

	case TIMEV_API:
		sid = (int) (unsigned long) tp->user_ptr;
		log(LOG_DEBUG, 0, "Refresh API sid= %d in %d seconds\n", 
			sid, delta/1000);
		break;

	case TIMEV_LocalRepair:
		dp = (Session *) tp->user_ptr;
		log(LOG_DEBUG, 0,"Local repair %s:%d in %d seconds\n",
		       inet_ntoa(dp->d_addr), ntoh16(dp->d_port), delta/1000);
		break;

#ifdef RTAP
	case TIMEV_RTAPsleep:
		/* Sleep command of integral rtap has expired.
		 */
		log(LOG_DEBUG, 0, "RTAP sleep in %d seconds\n", delta/1000);
		break;

	case TIMEV_RTAPdata:
		/* It is time to send data
		 */
#endif

	default:
		log(LOG_DEBUG, 0, "Unknown timer event %d flags=%d at %d\n",
			type, flags, tp->due_time);
		break;
	}
}

void
timer_print()
{
	time_rec       *tp;

	for (tp = time_q; tp != NULL; tp = tp->next) {
		print_timer_record(tp);
	}
}

#define RANDOM_RATIO 0.25

/* Return random number uniformly distributed in range 0.5A, 1.5A,
 *	where A ='avgdelay'.
 */
int
RandomDelay(int avgdelay)
	{
	int rand(void);
	float del = ((float) rand()/0x80000000);

	return((int)(avgdelay * (0.5 + del)));
}

