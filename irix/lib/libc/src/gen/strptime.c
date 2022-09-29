
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		
	strptime.c: 
		This embodiment is derived from getdate.c
		from which it differs in the following 
		particulars:

		 add:	
			format extensions for xpg4 compliance
				%C   ... CENTURY Number (19-20)
				%j   ... tm_yday
				%U   ... mon_week
				%W   ... sun_week
				%E_  ... recognized but ignored
				%O_  ... recognized but ignored
		change:
			existing formats with different semantics
				%n, %t, ... map to white space
				{%a, %A},{%b, %B} are now
					abbreviation indifferent

		remove:
			DATEMASK oriented processing removed.
				read_templ() procedure reduced. 
			Time Zone processing removed:
				%Z formats not used.


*/      
#ifdef __STDC__
	#pragma weak strptime = _strptime
#endif
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"

#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <stdlib.h>
#include <nl_types.h>
#include <langinfo.h>
#include <errno.h>
#include <unistd.h>
#include "gen_extern.h"
#include "_locale.h"


static int settime(void);
static int parse_fmt(const char *, struct tm *);
static void align_tm (struct tm *);
static int jan1(int);
#ifdef NEVER
static void Month(int);
static struct	tm  *calc_date(struct tm *);
#endif
static int number(int);
static int search(int,int);
static char	*input = NULL;
static int	meridian = 0;
static int	mon_week = -1;
static int	sun_week = -1;

enum STR
{
	   aJan, aFeb, aMar, aApr, aMay, aJun, aJul, aAug, aSep, aOct, aNov, aDec,
	   Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec,
	   aSun, aMon, aTue, aWed, aThu, aFri, aSat,
	   Sun, Mon, Tue, Wed, Thu, Fri, Sat,
	   Local_time, Local_date, DFL_FMT,
	   AM, PM, DATE_FMT, FMT_AMPM,
	   LAST
};
/*
 * Default values.
 */

static char  saved_locale[LC_NAMELEN] = "C";

static char *__time[(int)LAST + 1] = {
	"jan", "feb", "mar", "apr", "may", "jun", 
	"jul", "aug", "sep", "oct", "nov", "dec", 

	"january", "february", "march", "april", "may", "june", 
	"july", "august", "september", "october", "november", "december", 

	"sun", "mon", "tue", "wed", "thu", "fri", "sat", 

	"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", 
	"saturday", 

	"%H:%M:%S", "%m/%d/%y", "%a %b %e %H:%M:%S %Y",
	"am", "pm", "%a %b %e %T %Z %Y", "%I:%M:%S %p", NULL,
};


char *
strptime(const char *buf, const char *format, struct tm *tm)
{

	struct tm temp_tm;
	LOCKDECLINIT(l, LOCKLOCALE);


	if (settime() == 0) {
		UNLOCKLOCALE(l);
		return 0;
	}
	input = (char *) buf;
	temp_tm.tm_sec		= -1;
	temp_tm.tm_min		= -1;
	temp_tm.tm_hour		= -1;
	temp_tm.tm_mday		= -1;
	temp_tm.tm_mon		= -1;
	temp_tm.tm_year		= -1;
	temp_tm.tm_wday		= -1;
	temp_tm.tm_yday		= -1;
	temp_tm.tm_isdst	= -1;

	meridian = mon_week = sun_week = -1;

	if (strlen(format) == 0 || parse_fmt(format, &temp_tm) == 0) {
		UNLOCKLOCALE(l);
		return((char *) 0);
	}
	align_tm (&temp_tm);

	if (input) {
		if(temp_tm.tm_sec != -1) tm->tm_sec = temp_tm.tm_sec;
		if(temp_tm.tm_min != -1) tm->tm_min = temp_tm.tm_min;
		if(temp_tm.tm_hour != -1) tm->tm_hour = temp_tm.tm_hour;
		if(temp_tm.tm_mday != -1) tm->tm_mday = temp_tm.tm_mday;
		if(temp_tm.tm_mon != -1) tm->tm_mon = temp_tm.tm_mon;
		if(temp_tm.tm_year != -1) tm->tm_year = temp_tm.tm_year;
		if(temp_tm.tm_wday != -1) tm->tm_wday = temp_tm.tm_wday;
		if(temp_tm.tm_yday != -1) tm->tm_yday = temp_tm.tm_yday;
		if(temp_tm.tm_isdst != -1) tm->tm_isdst = temp_tm.tm_isdst;
	}
	UNLOCKLOCALE(l);
	return(input);
}

/*
 * Initialize pointers to month and weekday names and to meridian markers.
 */
/* move out of function scope so we get a global symbol for use with data cording */
static char *ostr = (char *)0;

static int
settime(void)
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
                return 1;

        if (setlocale(LC_CTYPE, "") == NULL) goto err1;

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
        if (j >= LAST)
        {
                (void) memcpy((void *)__time, my_time, sizeof(my_time));
                (void) strcpy(saved_locale, locale);
                if (ostr != 0)   /* free the previoulsy allocated local array*/
                        free(ostr);
                ostr = str;
                (void)close(fd);
                return 1;
        }

err3:
        free(str);
err2:
        (void)close(fd);
err1:
        (void)strcpy(_cur_locale[LC_TIME], saved_locale);
        return 0;
}



/*
 * Parse the number given by the specification.
 * Allow at most length digits.
 */
static int
number(int length)
{
	int	val;
	unsigned char c;

	val = 0;
	if(!isdigit((unsigned char)*input))
		return -1;
	while (length--) {
		if(!isdigit(c = *input))
			return val;
		val = 10*val + c - '0';
		input++;
	}
	return val;
}

/*
 * Search for format string in __time array
 */
static int
search(int start, int end)
{
	int	i;
	unsigned int length;

	for (i=start; i<=end; i++) {
		length = (unsigned int)strlen(__time[i]);
		if(strncasecmp(__time[i], input, length) == 0) 
		{
			input += strlen(__time[i]);
			return i;
		}
	}
	return(-1);
}

static void align_tm (struct tm * t)
{
	int tmp;
	/* 	post capture processing: 
		(1)	set time according to meridian
	*/
 	if ((t->tm_hour >= 0) && (meridian >= 0))	
        {
                switch (meridian)
                {
                        case PM:
                                t->tm_hour %= 12;
                                t->tm_hour += 12;
                                break;
                        case AM:
                                t->tm_hour %= 12;
                                break;
                        default:;
                                
                }
        }
	/*	(2) handle special date cases:
		%U or %W give week number, so that if date and 
		day of week are also known, we can compute the
		rest.
	*/
	if (t->tm_year < 0 || t->tm_wday < 0 ) return; /* no data */
	if (mon_week < 0 && sun_week < 0 ) return;  /* no scope */
	tmp = jan1(t->tm_year);
	/* use Sun=0 as default if both have been specified */
	if( sun_week >= 0) { /* %U case */
		t->tm_yday =  (sun_week * 7) + t->tm_wday - tmp;
		if (t->tm_wday == 0) t->tm_yday += 7;
	} /* %W case */
	else  t->tm_yday =  (mon_week * 7) + t->tm_wday - tmp;
	return;
}

/*
 * Match lines with input specification.
	return 0 on failure.
 */
/*VARARGS2*/
static int
parse_fmt(const char *bp, struct tm *t)
{
	char	*fmt;
	int	ret;
	unsigned char c, d;
	fmt = (char *)bp;
	while ((c = *fmt++) != '\0') {
		if ((c != '%') && (*fmt != 'n') && (*fmt != 't'))
			while(isspace(d = *input))
				input++;
		if ( c == '%' ) {
	extend:		c = *fmt++;
		/* suppress leading white space elision for certain types */
			if(c != 't' && c != '\n' && c != 'c' && c != 'x' 
				&& c != 'n' && c != 'X' && c != 'E' && c != 'O')
				while(isspace(d = *input))
				 input++;
			switch (c) {
			case 'E':	/* alternative era representation */
			case 'O':	/* alternate orthography */
				goto extend;  /* for now we simply ignore */

			case 'a':
			case 'A':
				if((ret = search(Sun, Sat)) >= 0)
					ret = ret - (int) Sun ;
				else if ((ret = search(aSun, aSat)) >=  0)
					ret = ret - (int) aSun ;
				else return (0);
				t->tm_wday = ret;
				continue;

			case 'w':
				if((ret = number(2)) < 0 || ret > 6)
					return(0);
				t->tm_wday = ret ;
				continue;

			case 'd':
			case 'e':
				if ((ret = number(2)) < 1 || ret > 31)
					return(0);
				t->tm_mday = ret;
				continue;

			case 'h':
			case 'b':
			case 'B':
				if ((ret = search(Jan, Dec)) >= 0)
					ret = ret - (int) Jan ;
				else if ((ret = search(aJan, aDec)) >= 0)
					ret = ret - (int) aJan ;
				else	return(0);
				t->tm_mon = ret;
				continue;
			case 'Y':
	/* The last time UNIX can handle is 1/18/2038;
	   for simplicity stop at 2038 */
				if ((ret = number(4)) < 1970 || ret > 2037)
					return(0);
				else
					ret = ret - 1900;
				t->tm_year = ret;
				continue;

			case 'y':
				if((ret = number(2)) == -1)
					return(0);
				if(ret >= 0 && ret <= 68)
					t->tm_year = ret+100;
				else	t->tm_year = ret;
				continue;

			case 'C':
				/* XPG4 Centruy number */
				if ((ret = number(2)) < 19 || ret > 20)
					return(0);
				ret = ret - 19; 
				t->tm_year += ret  * 100; 
				continue;

			case 'j':
				/* XPG4 day of year (1-366)*/
				if ((ret = number(3)) < 1 || ret > 366)
					return(0);
				t->tm_yday = --ret;
				continue;

			case 'm':
				if ((ret = number(2)) <= 0 || ret > 12)
					return(0);
				t->tm_mon = ret-1;
				continue;
			case 'I':
				if ((ret = number(2)) < 1 || ret > 12)
					return(0);
				t->tm_hour = ret;
				continue;
			case 'p':
				if ((ret  = search(AM, PM)) < 0)
					return(0);
				meridian = ret;
				continue;
			case 'H':
				if ((ret = number(2)) < 0 || ret > 23)
					return(0);
				t->tm_hour = ret;
				continue;
			case 'M':
				if ((ret = number(2)) < 0 || ret > 59)
					return(0);
				t->tm_min = ret;
				continue;
			case 'S':
				if ((ret  = number(2)) < 0 || ret > 61)
					return(0);
				t->tm_sec = ret;
				continue;


			case 'U':
				if ((ret  = number(2)) < 0 || ret >= 53)
					return(0);
				mon_week  = ret;
				continue;

			case 'W':
				if ((ret  = number(2)) < 0 || ret >= 53)
					return(0);
				sun_week  = ret;
				continue;

	/*	XPG4 apparently expects the scope of 'any white space' 
		to include at least one occurrence in this context:
	*/
			case 't':
			case 'n':
			{
				char *cp;

				if(*input == 0)
					return 0;
				while(!isspace(d = *input)) {
					input++;
					if(*input == 0)
						return 0;
				}
				cp = input;	/* first white-space ptr */
				while(isspace(d = *input)) {
					input++;
					if(*input == 0) {
						input = cp;
						break;
					}
				}
				continue;
			}


			/* composite formats */

			case 'c':
				if(parse_fmt(__time[DFL_FMT], t))
					continue;
				return 0;

			case 'x':
				if(parse_fmt(__time[Local_date], t))
					continue;
				return 0;

			case 'X':
				if(parse_fmt(__time[Local_time], t))
					continue;
				return 0;

			case 'D':
				if(parse_fmt("%m/%d/%y", t))
					continue;
				return 0;

			case 'r':
				if(parse_fmt("%I:%M:%S %p", t))
					continue;
				return 0;

			case 'R':
				if(parse_fmt("%H:%M", t))
					continue;
				return 0;

			case 'T':
				if(parse_fmt("%H:%M:%S", t))
					continue;
				return 0;

			case '%':
				if(*input++ == '%')
					continue;
				return 0;
				
			default:
				return(0);
			}
		}
		else {
			while(isspace(c))
				c = *fmt++;
			if (c == '%') {
				fmt--;
				continue;
			}
			d = *input++;
			if ((d != tolower(c)) && (d != c))
				return(0);
			if (!d) {
				input--;
				break;
			}
		}
	}
	return(1);
}


/*
 *	return day of the week
 *	of jan 1 of given year
 */

static int
jan1(int yr)
{
	register int y, d;

/*
 *	normal gregorian calendar
 *	one extra day per four years
 */

	y = yr + 1900;
	d = 4+y+(y+3)/4;

/*
 *	julian calendar
 *	regular gregorian
 *	less three days per 400
 */

	if(y > 1800) {
		d -= (y-1701)/100;
		d += (y-1601)/400;
	}

/*
 *	great calendar changeover instant
 */

	if(y > 1752)
		d += 3;

	return(d%7);
}
