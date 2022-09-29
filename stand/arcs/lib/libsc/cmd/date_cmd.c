#ident	"lib/libsc/cmd/date_cmd.c: $Revision: 1.5 $"

/*
 * date_cmd.c - print out the date stored in the BB clock
 */

#include <arcs/types.h>
#include <arcs/time.h>
#include <libsc.h>

static char *month[] = {
    "",
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

/*ARGSUSED*/
int
date_cmd(int argc, char **argv)
{
    TIMEINFO *t;
    
    t = GetTime();

#ifdef	BASE_OFFSET
/* old proms used to return year offset since 1970
 */
#define BASE_YEAR	1970		/* should be 0 */

    if (t->Year < BASE_YEAR)
	t->Year += BASE_YEAR;
#endif	/* BASE_OFFSET */

    printf ("Decimal date: %u %u %u %u %u %u %u.\n",
	t->Month, t->Day, t->Year, t->Hour, t->Minutes, t->Seconds,
	t->Milliseconds);
    if ((t->Month < 1 || t->Month > 12) ||
	    (t->Day < 1 || t->Day > 31) ||
	    (t->Year < 1 || t->Year > 2010) ||
	    (t->Hour > 23) || (t->Minutes > 59) || (t->Seconds > 59) ||
	    (t->Milliseconds > 999)) {
	printf ("Error in date\n");
	return 0;
    }
    printf ("%u %s %u, %02u:%02u:%02u\n", t->Day, month[t->Month], t->Year,
	t->Hour, t->Minutes, t->Seconds);

    return 0;
}
