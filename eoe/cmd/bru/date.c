/************************************************************************
 *									*
 *			Copyright (c) 1984, Fred Fish			*
 *			     All Rights Reserved			*
 *									*
 *	This software and/or documentation is protected by U.S.		*
 *	Copyright Law (Title 17 United States Code).  Unauthorized	*
 *	reproduction and/or sales may result in imprisonment of up	*
 *	to 1 year and fines of up to $10,000 (17 USC 506).		*
 *	Copyright infringers may also be subject to civil liability.	*
 *									*
 ************************************************************************
 */


/*
 *  FILE
 *
 *	date.c    routines to do date conversion
 *
 *  SCCS
 *
 *	@(#)date.c	9.11	5/11/88
 *
 *  DESCRIPTION
 *
 *	Routines to convert an ascii date string to an internal
 *	time.  The date may be given in one of the forms:
 *
 *		DD-MMM-YY[,HH:MM:SS]
 *		MM/DD/YY[,HH:MM:SS]
 *		MMDDHHMM[YY]
 *
 */
 
#include "autoconfig.h"

#include <stdio.h>

#if unix || xenix
#  include <sys/types.h>
#  if BSD4_2
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#else
#  include "sys.h"		/* Fake for non-unix hosts */
#endif

#include <ctype.h>

#include "typedefs.h"		/* Locally defined types */
#include "dbug.h"
#include "manifest.h"		/* Manifest constants */
#include "errors.h"		/* Error codes */
#include "macros.h"		/* Useful macros */

extern VOID bru_error ();	/* Report an error to user */
extern char *s_strchr ();	/* Find character (forward) */
extern int s_tolower ();	/* Convert character to lower case */
extern long s_timezone ();	/* Return correction factor for timezone */

#define FEBRUARY	1	/* February is a special month */
#define LEAPSIZE	29	/* February has 29 days in leap years */
#define HOURS_PER_DAY	24	/* Maybe this will be bigger some day! */
#define MIN_PER_HOUR	60	/* Minutes per hour */
#define SEC_PER_MIN	60	/* Seconds per minute */


#define DAYS_IN_YEAR(year) (((year) % 4) ? 365 : 366)

static char *mtable[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int month_size[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static BOOLEAN time_sane ();	/* Test time for sanity */


/*
 *  FUNCTION
 *
 *	date    convert ascii date string to internal time format
 *
 *  SYNOPSIS
 *
 *	time_t date (cp)
 *	char *cp;
 *
 *  DESCRIPTION
 *
 *	Given pointer to an ascii date string (cp), converts the
 *	string to an internal time and returns that value.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin date
 *	    Default time is 1-Jan-70,00:00:00
 *	    Initialize the time structure
 *	    If the string contains a '-' character then
 *		Clear the month string
 *		Convert string of first form
 *		Convert the month
 *	    Else if string contains a "/" character then
 *		Convert string of second form
 *		Adjust month
 *	    Else
 *		Convert a string of the third form
 *		Adjust month
 *	    End if
 *	    If the time does not pass sanity checks then
 *		Tell user about conversion error
 *	    Else
 *		Convert the time
 *	    End if
 *	    Return time
 *	End date
 *
 */

time_t date (cp)
char *cp;
{
    time_t mytime;		/* Time as converted */
    auto char mstr[16];		/* Month in string form */
    auto struct tm t;		/* Components of time */
	int nmatch;

    DBUG_ENTER ("date");
    DBUG_PRINT ("date", ("convert '%s'", cp));
    mytime = 0;
	memset((void*)&t, 0, sizeof(t));
    if (s_strchr (cp, '-')) {
	mstr[0] = EOS;
 	nmatch = sscanf (cp, "%2d-%3s-%4d", &t.tm_mday, mstr, &t.tm_year);
	if(nmatch == 3)	/* get rest, if first part present */
		(void) sscanf (cp, ",%2d:%2d:%2d", &t.tm_hour, &t.tm_min, &t.tm_sec);
	t.tm_mon = conv_month (mstr);
    } else if (s_strchr (cp, '/')) {
	nmatch = sscanf (cp, "%2d/%2d/%4d", &t.tm_mon, &t.tm_mday, &t.tm_year);
	if(nmatch == 3)	/* get rest, if first part present */
		(void)sscanf (cp, ",%2d:%2d:%2d", &t.tm_hour, &t.tm_min, &t.tm_sec);
	t.tm_mon--;
    } else {
	sscanf (cp, "%2d%2d%2d%2d%4d", &t.tm_mon, &t.tm_mday,
		&t.tm_hour, &t.tm_min, &t.tm_year);
	t.tm_mon--;
    }
    if (!time_sane (&t))
	bru_error (ERR_NTIME, cp);
    else
	mytime = mktime (&t);
    DBUG_RETURN (mytime);
}


/*
 *  FUNCTION
 *
 *	time_sane    perform sanity check on time components
 *
 *  SYNOPSIS
 *
 *	static BOOLEAN time_sane (tp)
 *	register struct tm *tp;
 *
 *  DESCRIPTION
 *
 *	Performs some boundry condition checks on the time to catch
 *	obvious errors.
 *
 *	Returns TRUE if things look ok, FALSE otherwise.
 *	
 *	Note that leap days on the wrong year will NOT be caught.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin time_sane
 *	    Test seconds for sanity
 *	    Test minutes for sanity
 *	    Test hours for sanity
 *	    Test month for sanity
 *	    Test day of month lower bound
 *	    If sane so far then
 *		If month is FEBRUARY then
 *		    Test upper bound day of february
 *		Else
 *		    Test upper bound day of other month
 *		End if
 *	    End if
 *	    Test year for sanity
 *	    Return results of test
 *	End time_sane
 *
 */


static BOOLEAN time_sane (tp)
register struct tm *tp;
{
    register BOOLEAN sanity;

    DBUG_ENTER ("time_sane");
    DBUG_PRINT ("date", ("seconds = %d", tp -> tm_sec));
    DBUG_PRINT ("date", ("minutes = %d", tp -> tm_min));
    DBUG_PRINT ("date", ("hours = %d", tp -> tm_hour));
    DBUG_PRINT ("date", ("mon = %d", tp -> tm_mon));
    DBUG_PRINT ("date", ("monthday = %d", tp -> tm_mday));
    DBUG_PRINT ("date", ("year = %d", tp -> tm_year));
    sanity = (0 <= tp -> tm_sec && tp -> tm_sec < 60);
    sanity &= (0 <= tp -> tm_min && tp -> tm_min < 60);
    sanity &= (0 <= tp -> tm_hour && tp -> tm_hour < 24);
    sanity &= (0 <= tp -> tm_mon && tp -> tm_mon < 12);
    sanity &= (0 < tp -> tm_mday);
    if (sanity) {
	if (tp -> tm_mon == FEBRUARY) {
	    sanity &= (tp -> tm_mday <= LEAPSIZE);
	} else {
	    sanity &= (tp -> tm_mday <= month_size[tp -> tm_mon]);
	}
    }
	/* put date in 1900 - 2038 (0-138) range; assume if < 39, that
	 * we must have crossed over into the 2000's; want it to work
	 * with mktime() */
	if(tp->tm_year <= 39) tp->tm_year += 100;
	if(tp->tm_year >= 1900) tp->tm_year -= 1900;
    sanity &= (70 <= tp -> tm_year && tp -> tm_year < 139);
    DBUG_RETURN (sanity);
}


/*
 *  FUNCTION
 *
 *	conv_month    convert string month to integer month
 *
 *  SYNOPSIS
 *
 *	static int conv_month (mstr)
 *	char *mstr;
 *
 *  DESCRIPTION
 *
 *	Converts string pointed to by "mstr" to an integer month
 *	by comparing with internal table.  Note that comparison
 *	is case independent but there must be exactly 3 characters.
 *	This could be fixed with a small amount of effort.
 *
 *	Note that the month is zero-based, to match the form in
 *	the "tm" structure.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin conv_month
 *	    Set default to illegal month
 *	    For each month in the month table
 *		If the name matches month string then
 *		    Remember numeric month
 *		    Break month scan loop
 *		End if
 *	    End for
 *	    Return month
 *	End conv_month
 *
 */

static int conv_month (mstr)
char *mstr;
{
    register int i;
    register int month;

    DBUG_ENTER ("conv_month");
    month = -1;
    for (i = 0; i < 12; i++) {
	if (match (mtable[i], mstr)) {
	    month = i;
	    break;
	}
    }
    DBUG_RETURN (month);
}


/*
 *  FUNCTION
 *
 *	match    case independent match test
 *
 *  SYNOPSIS
 *
 *	static BOOLEAN match (cp1, cp2)
 *	register char *cp1;
 *	register char *cp2;
 *
 *  DESCRIPTION
 *
 *	Perform case independent match test on two strings.  Returns
 *	TRUE if match found, FALSE otherwise.
 *
 */


/*
 *  PSEUDO CODE
 *
 *	Begin match
 *	    Scan for end of either string or mismatch
 *	    Result is TRUE only if end of both strings
 *	    Return result
 *	End match
 *
 */

static BOOLEAN match (cp1, cp2)
register char *cp1;
register char *cp2;
{
    register BOOLEAN result;

    DBUG_ENTER ("match");
    if (cp1 == NULL || cp2 == NULL) {
	bru_error (ERR_BUG, "match");
	result = FALSE;
    } else {
	result = TRUE;
	while (*cp1 != EOS && *cp2 != EOS && result) {
	    result = (s_tolower (*cp1++) == s_tolower (*cp2++));
	}
    }
    DBUG_RETURN (result);
}
