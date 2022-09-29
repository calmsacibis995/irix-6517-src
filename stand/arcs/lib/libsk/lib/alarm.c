#ident	"lib/libsk/lib/alarm.c:  $Revision: 1.6 $"

/*
 * alarm.c -- standalone alarm routines
 *
 *	The alarm can only be used by libsk programs because
 *	it relies on calling psignal to generate SIGALRM.
 */

#include <arcs/types.h>
#include <arcs/time.h>
#include <arcs/signal.h>
#include <libsc.h>
#include <libsk.h>

static unsigned alarm_time;
static int alarm_on;

void
_init_alarm(void)
{
	alarm_on = 0;
}

/*
 * alarm -- arrange for SIGALRM to be generated in future
 * returns seconds left on previous alarm
 * alarm(0) cancels current alarm
 * check_alarm is polled by _scandevs()
 */
unsigned int
alarm(unsigned secs)
{
	unsigned int cur_time, old_time;

	cur_time = (unsigned int)GetRelativeTime();
	old_time = alarm_on ? _max(alarm_time - cur_time, 1) : 0;
	alarm_time = cur_time + secs;
	alarm_on = secs;		/* alarm(0) turns off alarm */
	return(old_time);
}

/*
 * check_alarm -- called from _scandevs() to implement alarm clock
 */
void
check_alarm(void)
{
	extern int psignal(int);

	if (alarm_on && GetRelativeTime() >= alarm_time) {
		alarm_on = 0;
		(void)psignal(SIGALRM);
	}
}
