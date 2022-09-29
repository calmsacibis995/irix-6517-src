/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/strftime.c	1.16"

#include	"synonyms.h"
#include	"shlib.h"
#include	"mplib.h"
#include	<fcntl.h>
#include	<time.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<locale.h>
#include	<string.h>
#include	<stddef.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<unistd.h>
#include	"_locale.h"
#include 	<errno.h>  /* DMG: used for setoserror(ENOSYS) dev/test only */  

static void  settime(void);
static char *itoa(int, char *, int);
static int jan1(int);
static char *gettz(const struct tm *);
enum {
	aJan, aFeb, aMar, aApr, aMay, aJun, aJul, aAug, aSep, aOct, aNov, aDec,
	    Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec,
	    aSun, aMon, aTue, aWed, aThu, aFri, aSat,
	    Sun, Mon, Tue, Wed, Thu, Fri, Sat,
	    Local_time, Local_date,  DFL_FMT,
	    AM, PM, DATE_FMT, FMT_AMPM,
	    LAST
};
static const char * __time[] = {
	"Jan","Feb","Mar","Apr","May","Jun","Jul", "Aug", "Sep","Oct", "Nov", "Dec",
	"January", "February", "March","April",
	"May","June", "July", "August", "September",
	"October", "November", "December",
	"Sun","Mon", "Tue", "Wed","Thu", "Fri","Sat",
	"Sunday","Monday","Tuesday","Wednesday", "Thursday","Friday","Saturday",
	"%H:%M:%S","%m/%d/%y", "%a %b %e %H:%M:%S %Y",
	"AM", "PM", "%a %b %e %T %Z %Y", "%I:%M:%S %p", 
	NULL
};


/* move out of function scope so we get a global symbol for use with data cording */
static char	dflcase[] = "%?";
static char	dflcase2[] = "%??";

size_t
strftime(s, maxsize, format, tm)
char *s;
size_t maxsize;
const char *format;
const struct tm *tm;
{
	register const char	*p;
	register int	i,j;
	register char	c  , E, O; 
	size_t		size = 0;
	char		nstr[5];
	size_t		n;
	ssize_t		s_maxsize;
	LOCKDECLINIT(l, LOCKLOCALE);
	s_maxsize = (ssize_t) maxsize;

	settime();

	/* invoke mktime, for its side effects */
	{
		struct tm tmp;
		(void) memcpy(&tmp, tm, sizeof(struct tm));
		(void) mktime(&tmp);
	}


	/* Set format string, if not already set */
	if (format == NULL)
		format = __time[DATE_FMT];

	/* Build date string by parsing format string */
	while ((c = *format++) != '\0') {
		if (c != '%' || *format == 0) {
			if (++size >= s_maxsize) {
				UNLOCKLOCALE(l);
				return(0);
			}
			*s++ = c;
			continue;
		}
E_or_O:		switch (*format++) {
		case '%':	/* Percent sign */
			p = "%";
			break;
		case 'a':	/* Abbreviated weekday name */
			p = __time[aSun + tm->tm_wday];
			break;
		case 'A':	/* Weekday name */
			p = __time[Sun + tm->tm_wday];
			break;
		case 'b':	/* Abbreviated month name */
		case 'h':
			p = __time[aJan + tm->tm_mon];
			break;
		case 'B':	/* Month name */
			p = __time[Jan + tm->tm_mon];
			break;
		case 'c':	/* Localized date & time format */
			p = __time[DFL_FMT];
			goto recur;
		case 'C':  /*DMG: XPG4: Century number in the form cc */
			p = itoa(((1900 + tm->tm_year)/100), nstr, 2);
			break;
		case 'd':	/* Day number */
			p = itoa(tm->tm_mday, nstr, 2);
			break;
		case 'D':
			p = "%m/%d/%y";
			goto recur;
		case 'e':
			(void)itoa(tm->tm_mday, nstr, 2);
			if (tm->tm_mday < 10)
				nstr[0] = ' ';
			p = (const char *)nstr;
			break;
		case 'E':  /*DMG: XPG4: Modified Conversion Specifier E */
			if ((E = *format) == '\0') /* peek ahead to next fmt char */
				goto done;	/* format exhausted */
			switch ( E ) {
			case 'x':	/* alternative date representation */
			case 'X':	/* alternative time representation */
			case 'c':	/* alternative date & time format */
			case 'C':	/* alternative Name of BASE Year */
		 	case 'y':	/* alternative representation for offset from  %EC (year only) */
			case 'Y':	/* alternative representation for full year  */
					/*
                                         * I18N NOTE: set up ERA processing here

                                         *
                                         */
                                        goto E_or_O;  /* process the format char */

			default:
					E = 0;
					goto bad_parse;
			}
		case 'H':	/* Hour (24 hour version) */
			p = itoa(tm->tm_hour, nstr, 2);
			break;
		case 'I':	/* Hour (12 hour version) */
			if ((i = tm->tm_hour % 12) == 0)
				i = 12;
			p = itoa(i, nstr, 2);
			break;
		case 'j':	/* Julian date */
			p = itoa(tm->tm_yday + 1, nstr, 3);
			break;
		case 'K':  /*DMG: SGI Private Conversion Specifier K "KEEP" */
			switch (/* K = */ *format++) {
			case 'C':	/* keeps USL style %C (date default)  */
					p = __time[DATE_FMT];
					goto recur;
			default:
					dflcase2[2] = *(format - 1);
					dflcase2[1] = *(format - 2);
					p = (const char *)dflcase2;
			}
			break;
		case 'm':	/* Month number */
			p = itoa(tm->tm_mon + 1, nstr, 2);
			break;
		case 'M':	/* Minute */
			p = itoa(tm->tm_min, nstr, 2);
			break;
		case 'n':	/* Newline */
			p = "\n";
			break;
		case 'O':  /*DMG: XPG4: Modified Conversion Specifier O */
			if ((O = *format) == '\0') /* peek ahead to next fmt char */
					goto done; /* format exhausted */
			switch (  O ) {
			case 'w':	/* alternative numeric for week number of year 
						(Sunday as first day of week) */
			case 'd':	/* alternative numeric for day of month, leading zero 
					filled if defined else leading blank filled */
			case 'e':	/* alternative numeric for day of month, leading blank filled*/
			case 'H':	/* alternative numeric for hour (24 hour format) */
			case 'I':	/* alternative numeric for hour (12 hour format) */
			case 'm':	/* alternative numeric for month */
			case 'M':	/* alternative numeric for minutes */
			case 'S':	/* alternative numeric for seconds */
			case 'u':	/* alternative numeric for weekday (Monday = 1) */
			case 'U':	/* alternative numeric for week number of year (Sunday 
					as first day of week, rules per %U) */
			case 'V':	/* alternative numeric for week number of year (Monday 
					as first day of week, rules per %V) */
			case 'W':	/* alternative numeric for week number of year 
					(Monday as first day of week) */
			case 'y':	/* alternative numeric for year number (year number from %C) */
					/* 
					 * I18N NOTE: set up Orthography processing here
					
					 *
					 */
					goto E_or_O;  /* process the format char */
			default:
					O = 0;
					goto bad_parse;
			}
		case 'p':	/* AM or PM */
			if (tm->tm_hour >= 12) p = __time[PM];
					else p = __time[AM];
			break;
		case 'r':
			p = __time[FMT_AMPM];
			goto recur;
		case 'R':
			p = "%H:%M";
			goto recur;
		case 'S':	/* Seconds */
			p = itoa(tm->tm_sec, nstr, 2);
			break;
		case 't':	/* Tab */
			p = "\t";
			break;
		case 'T':
			p = "%H:%M:%S";
			goto recur;
		case 'u':  /*DMG: XPG4: day of week, Sunday = 7 */
			if ((i = tm->tm_wday % 7 ) == 0) i = 7;
			p = itoa(i, nstr, 1);
			break;
		case 'U':  /* Week number of year, taking Sunday as the first day of the week */
			p = itoa((tm->tm_yday - tm->tm_wday + 7)/7, nstr, 2);
			break;
		case 'V':  /* 2 digit Week number, Monday the first week day 
				first week of a year beginning on FRI SAT or
				SUN is counted as week 53 of prior year*/
			if ((i = 8 - tm->tm_wday) == 8) i = 1; /* 1 based wday */
			j = ((tm->tm_yday + i)/7)+1; /* 1 based week of year */
                        if (((i= jan1(tm->tm_year)) == 0) && tm->tm_yday == 0)
				j= 53; /* Jan 1 is on a Sunday */	
			else if (i > 4) {
				/* jan1() return the day of the week for 
				 * Jan 1 for any given year with Sunday 
				 * as the first day of the week.  
				 *
				 * Decrement the 1 based week count if 
				 * Jan 1 is either on a Fri. or Sat. 
				 */ 

				j -= 1;

				/* Adjust the week count back to 1 based 
				 * again if necessary.  
				 */

				if (j == 0) j= 53;	
			}
			p = itoa(j, nstr, 2);
			break;
		case 'w':	/* Weekday number */
			p = itoa(tm->tm_wday, nstr, 1);
			break;
		case 'W':	/* Week number of year, taking Monday as first day of week */
			if ((i = 8 - tm->tm_wday) == 8) i = 1;
			p = itoa((tm->tm_yday + i)/7, nstr, 2);
			break;
		case 'x':	/* Localized date format */
			p = __time[Local_date];
			goto recur;
		case 'X':	/* Localized time format */
			p = __time[Local_time];
			goto recur;
		case 'y':	/* Year in the form yy */
			p = itoa(tm->tm_year, nstr, 2);
			break;
		case 'Y':	/* Year in the form ccyy */
			p = itoa(1900 + tm->tm_year, nstr, 4);
			break;
		case 'Z':	/* Timezone */
			p = gettz(tm);
			break;

		bad_parse:
		default:
			dflcase[1] = *(format - 1);
			p = (const char *)dflcase;
			break;

		recur:;
			if ((n = strftime(s, maxsize-size, p, tm)) == 0) {
				UNLOCKLOCALE(l);
				return(0);
			}
			s += n;
			size += n;
			continue;
		}
		n = strlen(p);
		if ((size += n) >= s_maxsize) {
			UNLOCKLOCALE(l);
			return(0);
		}
		(void)strcpy(s, p);
		s += n;
	}
done:	*s = '\0';
	UNLOCKLOCALE(l);
	return(size);
}

static char *
itoa(i, ptr, dig)
register int i;
register char *ptr;
register int dig;
{
	ptr += dig;
	*ptr = '\0';
	while (--dig >= 0) {
		*(--ptr) = (char)(i % 10 + '0');
		i /= 10;
	}
	return(ptr);
}

static char saved_locale[LC_NAMELEN] = "C";
/* move out of function scope so we get a global symbol for use with data cording */
static char *ostr = (char *)0;

static void settime(void)
{
	register char *p;
	register int  j;
	char *locale;
	char *my_time[LAST];
	char *str;
	int  fd;
	struct stat buf;

	locale = _cur_locale[LC_TIME];

	if (strcmp(locale, saved_locale) == 0)
		return;

	if ( (fd = open(_fullocale(locale, "LC_TIME"), O_RDONLY)) == -1)
		goto err1;

	if( (fstat(fd, &buf)) != 0 || (str = malloc((size_t)buf.st_size + 2)) == NULL)
		goto err2;

	if ( (read(fd, str, (size_t)buf.st_size)) != buf.st_size)
		goto err3;

	/* Set last character of str to '\0' */
	p = &str[buf.st_size];
	p[0] = '\n';
	p[1] = '\0';

	/* p will "walk thru" str */
	p = str;

	j = -1;
	while (*p != '\0')
	{
		/* "Look for a newline, i.e. end of sub-string
		 * and  change it to a '\0'. If LAST pointers
		 * have been set in my_time, but the newline hasn't been seen
		 * yet, keep going thru the string leaving my_time alone.
		 */
		if (++j < LAST)
			my_time[j] = p;
		p = strchr(p,'\n');
		*p++ = '\0';
	}
	if (j == LAST)
	{
		(void) memcpy((void *)__time, my_time, sizeof(my_time));
		(void) strcpy(saved_locale, locale);
		if (ostr != 0)	 /* free the previoulsy allocated local array*/
			free(ostr);
		ostr = str;
		(void)close(fd);
		return;
	}

err3:
	free(str);
err2:
	(void)close(fd);
err1:
	(void)strcpy(_cur_locale[LC_TIME], saved_locale);
	return;
}

#define MAXTZNAME	3

static char *
gettz(tm)
const struct tm *tm;
{
	register char	*p;

	if (tm->tm_isdst)
		p = tzname[1];
	else
		p = tzname[0];
	if (strcmp(p, "   ") == 0)
		return("");
	else
		return(p);
}


/*	From gen/getdate.c:
 *      returns day of the week
 *      of jan 1 of given year
 */

static int
jan1(int yr)
{
        register int y, d;

/*
 *      normal gregorian calendar
 *      one extra day per four years
 */

        y = yr + 1900;
        d = 4+y+(y+3)/4;

/*
 *      julian calendar
 *      regular gregorian
 *      less three days per 400
 */

        if(y > 1800) {
                d -= (y-1701)/100;
                d += (y-1601)/400;
        }

/*
 *      great calendar changeover instant
 */

        if(y > 1752)
                d += 3;

        return(d%7);
}

