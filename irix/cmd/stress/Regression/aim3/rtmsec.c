/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) rtmsec.c:3.2 5/30/92 20:18:48" };
#endif

#include "suite3.h"
#include "testerr.h"

#if defined(T_TIMES)
/* ---------- rtmsec for UNIX Systems 5 and later */
#include <sys/types.h>
#include <sys/times.h>

#ifndef  ZILOG
#include <string.h>
#else
char *strcpy();
char *strncpy();
#endif

#include <signal.h>
#include <time.h>

/*
** long rmtsec(int)
**
** Description
**	Returns real time in milliseconds.
**
** Arguments
**	TRUE (1) - resets its internal 'epoch' marker
**	FALSE(0) - returns time since last reset
**
** Modification
**	6/12/89 Added documentation, minor fixes; Tin Le
*/
long rtmsec(reset)
int reset;
{
/* rtmsec - the timing routines used by multiuser */
	struct tms	ts;
	static long	epoch=0l;
	long		tmp;

	/*
	**  Milliseconds returned are now since first call.
	*/
	if ( epoch == 0l || reset == TRUE ) {
	    epoch = times(&ts);		/* save */
	    DEBUG(fprintf(stderr, "rtmsec(): resetting epoch to %ld\n",epoch));
	}
	if(epoch < 0l)  {
	    fprintf(stderr,"rtmsec(): times returned < 0: %ld\n",epoch);
	    epoch = 0l;
	}
	if(1000l * ((tmp = (times(&ts) - epoch)) / HZ) < 0l)
	    fprintf(stderr,"rtmsec(): 1000 * (times - epoch) / HZ = %ld\n",
		1000l*(tmp/HZ));
	return ( 1000l*((times(&ts)-epoch) / HZ) );
}

#else	/* NOT SYSTEM V or just wanted to use the ftime() instead */

/* ---------- rtmsec for Version 7, and for all Berkeley BSD */
#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/time.h>
#include <strings.h>
#include <signal.h>

/*
** long rmtsec(int)
**
** Description
**	Returns real time in milliseconds.
**
** Arguments
**	TRUE (1) - resets its internal 'epoch' marker
**	FALSE(0) - returns time since last reset
**
** Modification
**	6/12/89 Added documentation, minor fixes; Tin Le
*/
long rtmsec(reset)
int reset;
{
	struct timeb	timeb;
	static long	epoch;
	/* 
	**  Milliseconds returned are now since first call.  In order
	**  to avoid multiply overflow, the new code subtracts
	**  epoch before multiplying by 1000 to convert seconds to
	**  milliseconds. Notice, 68 years, expressed in
	**  seconds, exceeds 2**31!, and that is before multiplying by
	**  1000.  Happily, this alteration will not affect
	**  time computations based on the difference
	**  of any two rtmsec calls, as epoch cancels itself thusly:
	**  	(rtmsec2-epoch) - (rtmsec1-epoch)  =
	**	(rtmsec2-rtmsec1) + (epoch-epoch)  =
	**	(rtmsec2-rtmsec1) + 0		   =
	**  This is true for any epoch value.
	*/

	if (epoch == 0l || reset == TRUE) {
	      ftime(&timeb), epoch = timeb.time;	/* save it */
	      DEBUG(fprintf(stderr, "rtmsec(): resetting epoch to %ld\n",epoch));
	}

	ftime(&timeb);
	return ( (timeb.time-epoch)*1000L + timeb.millitm );
}
#endif
