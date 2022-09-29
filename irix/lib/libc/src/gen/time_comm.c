/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/time_comm.c	1.32"
/*
 * Functions that are common to ctime(3C) and cftime(3C)
 *
 * XXX Horrible global usage - the entire TZ stuff is
 * not reentrent - we assume that things don't change much (and lock
 * the actual setting of the various TZ variables
 */
#ifdef __STDC__
	#pragma weak tzset = _tzset
#ifndef  _LIBC_ABI
	#pragma weak localtime_r = _localtime_r
	#pragma weak gmtime_r = _gmtime_r
#endif /* _LIBC_ABI */
#endif
#include	"synonyms.h"
#include	"shlib.h"
#include	"mplib.h"
#include	<ctype.h>
#include	<errno.h>
#include  	<stdio.h>
#include  	<limits.h>
#include	<sys/types.h>
#include	<sys/param.h>
#include 	<time.h>
#include 	<stdlib.h>
#include 	<string.h>
#include 	<tzfile.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	"gen_extern.h"

static char	*getdigit(char *, int *);
static char	*gettime(char *, time_t *, int);
static int	getdst(char *, time_t *, time_t *);
static int	posixgetdst(char *, time_t *, time_t *, time_t);
static char	*posixsubdst(char *, unsigned char *, int *, int *, int *, int *, time_t *);
static void	getusa(time_t *, time_t *, struct tm *);
static int 	istzchar(char, int);
static char	*getzname(char *, char **);
static time_t	sunday(struct tm *, time_t);
static time_t 	detzcode(char *);
static int	_tzload(char *);
static char	*tzcpy(char *, char *);
static void	_ltzset(time_t);

#define	SEC_PER_MIN	60
#define	SEC_PER_HOUR	(60*60)
#define	SEC_PER_DAY	(24*60*60)
#define	SEC_PER_YEAR	(365*24*60*60)
#define	LEAP_TO_70	(70/4)
#define FEB28		(58)

#define SEC_PER_LEAPYR  (SEC_PER_YEAR + SEC_PER_DAY)
#define SEC_PER_QUADYR  (3 * SEC_PER_YEAR + SEC_PER_LEAPYR)

#define MINTZNAME	3
#define	year_size(A)	(isleap(A) ? 366 : 365)

static time_t 	start_dst = -1 ;    /* Start date of alternate time zone */
static time_t  	end_dst = -1 ;      /* End date of alternate time zone */

#ifndef TRUE
#define TRUE		1
#define FALSE		0
#endif /* !TRUE */

static struct tm *	offtime(const time_t *, long, struct tm *);

struct ttinfo {				/* time type information */
	time_t		tt_gmtoff;	/* GMT offset in seconds */
	int		tt_isdst;	/* used to set tm_isdst */
	int		tt_abbrind;	/* abbreviation list index */
};

struct state {
	int		timecnt;
	int		typecnt;
	int		charcnt;
	time_t		*ats;
	char	*types;
	struct ttinfo	*ttis;
	char		*chars;
};

static struct state	*_tz_state = NULL;
static struct tm	tm = { 0, };

struct tm *
gmtime(const time_t *clock)
{
	register struct tm *	tmp;

	tmp = offtime(clock, 0L, &tm);
	return tmp;
}

struct tm *
gmtime_r(const time_t *clock, struct tm *res)
{
	return offtime(clock, 0L, res);
}

struct tm *
localtime_r(const time_t *timep, struct tm *res)
{
	register struct tm *		tmp;
	register struct state 		*s;
	time_t				daybegin, dayend;
	time_t				curr;
      	LOCKDECLINIT(l, LOCKLOCALE);

	_ltzset(*timep);
	s = _tz_state;

	tmp = offtime(timep, -timezone, res);
	if (!daylight) {
		UNLOCKLOCALE(l);
		return(tmp);
	}

	if (s != 0 && start_dst != -1)
		/* Olson method */
		curr = *timep;
	else
		/* this was POSIX time */
		curr = tmp->tm_yday*SEC_PER_DAY + tmp->tm_hour*SEC_PER_HOUR +
			tmp->tm_min*SEC_PER_MIN + tmp->tm_sec;

	if ( start_dst == -1 && end_dst == -1)
		getusa(&daybegin, &dayend, tmp);
	else
	{
		daybegin = start_dst;
		dayend  = end_dst;
	}
	if (daybegin <= dayend) {
		if (curr >= daybegin && curr < dayend) {
			tmp = offtime(timep, -altzone, res);
			tmp->tm_isdst = 1;
		}
	} else {	/* Southern Hemisphere */
		if (!(curr >= dayend && curr < daybegin)) {
			tmp = offtime(timep, -altzone, res);
			tmp->tm_isdst = 1;
		}
	}
	UNLOCKLOCALE(l);
	return(tmp);
}

struct tm *
localtime(const time_t *timep)
{
	return localtime_r(timep, &tm);
}


static struct tm *
offtime(const time_t *clock, long offset, struct tm *res)
{
	register struct tm *	tmp;
	register long		days;
	register long		rem;
	register int		y;
	register int		yleap;
	register const int *		ip;

	tmp = res;
	days = *clock / SECS_PER_DAY;
	rem = *clock % SECS_PER_DAY;
	rem += offset;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}
	tmp->tm_hour = (int) (rem / SECS_PER_HOUR);
	rem = rem % SECS_PER_HOUR;
	tmp->tm_min = (int) (rem / SECS_PER_MIN);
	tmp->tm_sec = (int) (rem % SECS_PER_MIN);
	tmp->tm_wday = (int) ((EPOCH_WDAY + days) % DAYS_PER_WEEK);
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYS_PER_WEEK;
	y = EPOCH_YEAR;
	if (days >= 0)
		for ( ; ; ) {
			yleap = isleap(y);
			if (days < (long) __year_lengths[yleap])
				break;
			++y;
			days = days - (long) __year_lengths[yleap];
		}
	else do {
		--y;
		yleap = isleap(y);
		days = days + (long) __year_lengths[yleap];
	} while (days < 0);
	tmp->tm_year = y - TM_YEAR_BASE;
	tmp->tm_yday = (int) days;
	ip = __mon_lengths[yleap];
	for (tmp->tm_mon = 0; days >= (long) ip[tmp->tm_mon]; ++(tmp->tm_mon))
		days = days - (long) ip[tmp->tm_mon];
	tmp->tm_mday = (int) (days + 1);
	tmp->tm_isdst = 0;
	return tmp;
}

double
difftime(time_t time1, time_t time0)
{
	return((double)(time1 - time0));
}

time_t
mktime(struct tm *timeptr)
{
	struct tm	*tptr;
	time_t	secs;
	int	temp;
	int	isdst = 0;
	time_t	daybegin, dayend;
	struct tm	stm;

	int	leap;
	int	sec_sign = 1;
	time_t	secs_abs;

	/*
	 *  XPG4 fix until waiver is gotten.  If no waiver can
	 *  be obtained, then use this fix.  If waiver is gotten, then
	 *  remove this call: "_ltzset(0);".
	 */
	_ltzset(0);

	secs = timeptr->tm_sec + SEC_PER_MIN * timeptr->tm_min +
		SEC_PER_HOUR * timeptr->tm_hour +
		SEC_PER_DAY * (timeptr->tm_mday - 1);

	/* Picked the entire code of mktime() from 6.5 and modified it to take
           care of some special cases.Leap years were not taken care of
           properly */

	/*
	 * canonicalize secs - need to be sure year is correct
	 * This also helps with overflow detection
	 */

	/*  Picked up Doug's fix for leap year stuff and modified it 
	 *  to also check for -ve time offsets -- code basically does 
	 *  the same but sec_sign indicates whether to add or subtract.
	 *
	 *  NOTE:  This has been changed to fit the more general case if
	 *	   the data submitted to mktime crosses a leap year, or worse
	 *	   crosses over multiple leap years.
	 *
	 *	   Although it isn't the cleanest, the idea is to lop off the
	 *	   number of seconds beyond a quad year (leap cycle).  The yr_secs[]
	 *	   is then used to help determine where I am left in the leap year
	 *	   cycle.  Then the previous code for calculating the leap days
	 *	   below, works fine.  drbanks
	*/

	/* The quad year check is fine, since every four years within the valid
	 * range (1970 to 2038) is a leap year, even the year 2000.  But the
	 * leap_yr_secs[] check was bogus since it always used the current year
	 * to do it's leap year calculation, but didn't take into account that
	 * depending what month we're in, the february that we're jumping over
	 * may be next year's, not this year's (which means we may have used the
	 * wrong leap year calculation).
	 * 
	 * Got rid of the leap_yr_secs[] check, and also some similar code that
	 * was trying to handle a negative second count.  Replaced them with
	 * code which normalizes the month and year (for both positive and
	 * negative second counts) and results in a non-negative count which
	 * represents a time within the normalized month and year.
	 * 
	 * Doing it this way seemed a lot more straightforward than trying to
	 * fix the leap_yr_secs[] checks (especially in the case of negative
	 * second counts).  tee
	 */

	if (secs < 0)
		sec_sign = -1;
	else
		sec_sign = 1;

	secs_abs = sec_sign * secs;

	if (secs_abs >= SEC_PER_QUADYR){
		timeptr->tm_year += sec_sign * 4 * (secs_abs/SEC_PER_QUADYR);
		secs_abs %= SEC_PER_QUADYR;
	}

	if (timeptr->tm_mon >= 12) {
		timeptr->tm_year += timeptr->tm_mon / 12;
		timeptr->tm_mon %= 12;
	} else if (timeptr->tm_mon < 0) {
		temp = -timeptr->tm_mon;
		timeptr->tm_year -= (1 + temp / 12);
		timeptr->tm_mon = 12 - temp % 12;

		if (timeptr->tm_mon == 12) {  /* For border conditions. */
			timeptr->tm_year++;
			timeptr->tm_mon = 0;
		} 
	}

	if (sec_sign == 1) {
		/*
		 * chew up one month worth of seconds at a time,
		 * until there is less than one month left.
		 */
		leap = isleap(timeptr->tm_year + 1900) ? 1 : 0;
		while (secs_abs >= SEC_PER_DAY * __mon_lengths[leap][timeptr->tm_mon]) {
			secs_abs -= SEC_PER_DAY * __mon_lengths[leap][timeptr->tm_mon];
			timeptr->tm_mon++;
			if (timeptr->tm_mon == 12) {
				timeptr->tm_mon = 0;
				timeptr->tm_year++;
				leap = isleap(timeptr->tm_year + 1900) ? 1 : 0;
			}
		}
	} else {	/* sec_sign == -1 */
		/*
		 * secs is negative, so we back up one month at a time,
		 * until we back across it, and it becomes positive.
		 */
		leap = isleap(timeptr->tm_year + 1900) ? 1 : 0;
		while (secs_abs > 0) {
			if (timeptr->tm_mon-- == 0) {
				timeptr->tm_mon = 11;
				timeptr->tm_year--;
				leap = isleap(timeptr->tm_year + 1900) ? 1 : 0;
			}
			secs_abs -= SEC_PER_DAY * __mon_lengths[leap][timeptr->tm_mon];
		}
	}

	/*
	 * At this point, secs will be a non-negative number that is guaranteed
	 * to fall within the month timeptr->tm_mon of the year timeptr->tm_year.
	 */

	secs = secs_abs * sec_sign;

	/*
	 * make a half-hearted attempt to determine whether we can
	 * represent the time requested - to do this in earnest would
	 * take reams of code, since users can pass in virtually any number
	 * (positive or negative)
	 */
	if (timeptr->tm_year > (2138-1900) || timeptr->tm_year < 70)
		return(-1);

	/*
	 * This leap year stuff isn't quite correct - but
	 * it really doesn't matter since we can only represent
	 * the years 1970 through 2038 - and 2000 is in fact a leap year!
	 */
	secs += SEC_PER_YEAR * (timeptr->tm_year - 70) +
		 SEC_PER_DAY * ((timeptr->tm_year + 3)/4 - 1 - LEAP_TO_70);

	if (leap)
		secs += SEC_PER_DAY * __lyday_to_month[timeptr->tm_mon];
	else
		secs += SEC_PER_DAY * __yday_to_month[timeptr->tm_mon];

	_ltzset(secs);
	if (timeptr->tm_isdst > 0) 
		secs += altzone;
	else {
		if (timeptr->tm_isdst < 0)
			isdst = -1;
		secs += timezone;
	}
	tptr = localtime_r((time_t *)&secs, &stm);

	if (isdst == -1) {
		if (start_dst == -1 && end_dst == -1)
			getusa(&daybegin, &dayend, tptr);
		else {
			daybegin = start_dst;
			dayend = end_dst;
		}
		temp = tptr->tm_sec + SEC_PER_MIN * tptr->tm_min +
			SEC_PER_HOUR * tptr->tm_hour + SEC_PER_DAY * tptr->tm_yday;
		if (daybegin <= dayend) {
			if (temp >= daybegin && temp < dayend + (timezone - altzone)) {
				secs -= (timezone - altzone);
				tptr = localtime_r((time_t *)&secs, &stm);
			}
		} else {	/* Southern Hemisphere */
			if (!(temp >= dayend + (timezone - altzone) && temp < daybegin)) {
				secs -= (timezone - altzone);
				tptr = localtime_r((time_t *)&secs, &stm);
			}
		}
	}

	*timeptr = *tptr;

	if (secs < 0)
		return(-1);
	else
		return(secs);
}

static void
_ltzset(time_t tim)
{
	register char *	name;
	int i;
	LOCKDECLINIT(l, LOCKLOCALE);

	if((name = getenv("TZ")) == 0 || *name == '\0') {
		/* TZ is not present, use GMT */
		(void) strcpy(tzname[0],"GMT");
		(void) strcpy(tzname[1],"   ");
		timezone = altzone = daylight = 0;
		UNLOCKLOCALE(l);
		return; /* TZ is not present, use GMT */
	}
	if (name[0] == ':') /* Olson method */ {
		name++;
		if (_tzload(name) != 0) {
			if (_tz_state) {
			    if (_tz_state->ats)
				free(_tz_state->ats);
			    if (_tz_state->types)
				free(_tz_state->types);
			    if (_tz_state->ttis)
				free(_tz_state->ttis);
			    if (_tz_state->chars)
				free(_tz_state->chars);
			    free(_tz_state);
			    _tz_state = 0;
			}
			goto out;
		}
		else {
			/*
			** Set tzname elements to initial values.
			*/
			if (tim == 0)
				tim = time(NULL);
			if (!(tzname[0]=tzcpy(tzname[0],&_tz_state->chars[0])))
				goto out;
			(void) strcpy(tzname[1],"   ");
			timezone = -_tz_state->ttis[0].tt_gmtoff;
			daylight = 0;
			for (i = 1; i < _tz_state->typecnt; ++i)
			{
				register struct ttinfo *	ttisp;
				ttisp = &_tz_state->ttis[i];
				if (ttisp->tt_isdst)
				{
					if (!(tzname[1]=tzcpy(tzname[1],&_tz_state->chars[ttisp->tt_abbrind])))
						goto out;
					daylight = 1;
				}
				else
				{
					if (!(tzname[0]=tzcpy(tzname[0],&_tz_state->chars[ttisp->tt_abbrind])))
						goto out;
					timezone = -ttisp->tt_gmtoff;
				}
			}
			start_dst = end_dst = 0;
			for (i=0;  i < _tz_state->timecnt && _tz_state->ats[i] < tim; i++) {
				register struct ttinfo *	ttisp;

				ttisp = &_tz_state->ttis[_tz_state->types[i]];
				if (ttisp->tt_isdst) {
					if (!(tzname[1]=tzcpy(tzname[1],&_tz_state->chars[ttisp->tt_abbrind])))
						goto out;
					daylight = 1;
					altzone = -ttisp->tt_gmtoff;
					start_dst = _tz_state->ats[i];
					if (i+1 < _tz_state->timecnt)
						end_dst = _tz_state->ats[i+1];
					else
						end_dst = INT_MAX;
				} else {
					if (!(tzname[0]=tzcpy(tzname[0],&_tz_state->chars[ttisp->tt_abbrind])))
						goto out;
					timezone = -ttisp->tt_gmtoff;
				}
			}
		}
	}
	else /* POSIX method */ {
		/* Get main time zone name and difference from GMT */
		if ( ((name = getzname(name,&tzname[0])) == 0) || 
			((name = gettime(name,&timezone,1)) == 0)) {
				/* No offset from GMT given, so use GMT */
				(void) strcpy(tzname[0],"GMT");
				(void) strcpy(tzname[1],"   ");
				timezone = altzone = daylight = 0;
				goto out;
		}
		(void) strcpy(tzname[1],"   ");
		altzone = timezone - SEC_PER_HOUR;
		start_dst = end_dst = 0;
		daylight = 0;

		/* Get alternate time zone name */
		if ( (name = getzname(name,&tzname[1])) == 0) {
			(void) strcpy(tzname[1],"   ");
			altzone = timezone;		/* no DST */
			goto out;
		}

		start_dst = end_dst = -1;
		daylight = 1;

		/* If the difference between alternate time zone and
		 * GMT is not given, use one hour as default.
		 */
		if (*name == '\0')
			goto out;
		if (*name != ';' && *name != ',')
		if ( (name = gettime(name,&altzone,1)) == 0 || 
			(*name != ';' && *name != ','))
				goto out;
		if (*name == ';')
			(void) getdst(name + 1,&start_dst, &end_dst);
		else
			(void) posixgetdst(name+1, &start_dst, &end_dst, tim);
	}
out:
	UNLOCKLOCALE(l);
}

void
tzset(void)
{
	_ltzset((time_t)0);
}

static int
istzchar(char c, int flag)
{	

	/* if flag set, indicates first character of tzname which cannot be a colon */

	switch (c)	{
		case '0' :
		case '1' :
		case '2' :
		case '3' :
		case '4' :
		case '5' :
		case '6' :
		case '7' :
		case '8' :
		case '9' :
		case ',' :
		case '-' :
		case '+' :
		case '\0' :
			return(0);
		case ':' :
			if (flag) 
				return(0);
	}
	return(1);
}

static char *
getzname(char *p, char **tz)
{
	register char *q = p;
	register char c;
	
	if (!istzchar(*q,1))
		return(0);
	while (istzchar(*++q,0)) ;
	c = *q;
	*q = '\0';
	if (!(*tz=tzcpy(*tz, p))) {
		*q = c;
		return(0);
	}
	*q = c;
	return(q);	
}

/* move out of function scope so we get a global symbol for use with data cording */
static char *tz_x[2] = { 0, };

static char *
tzcpy(char *s1, char *s2)
{
	size_t	len;

	if (strlen(s1) >= (len = strlen(s2))) {
		(void) strcpy(s1, s2);
		if (len < MINTZNAME) {
			s1 += len;
			for (; len < MINTZNAME; len++)
				*s1++ = ' ';
			*s1 = '\0';
			s1 -= len;
		}
	} 
	else {
		int tmp;
		size_t maxlen;

		maxlen = (len < TZNAME_MAX) ? len : TZNAME_MAX;
		tmp = ((s1 == tzname[0]) ? 0 : 1);
		if ((tz_x[tmp] = realloc(tz_x[tmp], maxlen+1)) == NULL) 
			return 0;
		s1 = tz_x[tmp];
		(void) strncpy(s1, s2, maxlen);
		s1[maxlen] = '\0';
	}
	return(s1);
}


static char *
gettime(char *p, time_t *timez, int f)
{
	register time_t t = 0;
	int d, sign = 0;

	d = 0;
	if (f)
		if ( (sign = (*p == '-')) || (*p == '+'))
			p++;
	if ( (p = getdigit(p,&d)) != 0)
	{
		t = d * SEC_PER_HOUR;
		if (*p == ':')
		{
			if( (p = getdigit(p+1,&d)) != 0)
			{
				t += d * SEC_PER_MIN;
				if (*p == ':')
				{
					if( (p = getdigit(p+1,&d)) != 0)
						t += d;
				}
			}
		}
	}
	if(sign)
		*timez = -t;
	else
		*timez = t;
	return(p);
}

static char *
getdigit(char *ptr, int *d)
{


	if (!isdigit(*ptr))
		return(0);
 	*d = 0;
	do
	{
		*d *= 10;
		*d += *ptr - '0';
	}while( (isdigit(*++ptr)));
	return(ptr);
}

static int
getdst(char *p, time_t *s, time_t *e)
{
	int lsd,led;
	time_t st,et;
	st = et = 0; 		/* Default for start and end time is 00:00:00 */
	if ( (p = getdigit(p,&lsd)) == 0 )
		return(0);
	lsd -= 1; 	/* keep julian count in sync with date  1-366 */
	if (lsd < 0 || lsd > 365)
		return(0);
	if ( (*p == '/') &&  ((p = gettime(p+1,&st,0)) == 0) )
			return(0);
	if (*p == ',')
	{
		if ( (p = getdigit(p+1,&led)) == 0 )
			return(0);
		led -= 1; 	/* keep julian count in sync with date  1-366 */
		if (led < 0 || led > 365)
			return(0);
		if ((*p == '/') &&  ((p = gettime(p+1,&et,0)) == 0) )
				return(0);
	}
	/* Convert the time into seconds */
	*s = (time_t)(lsd * SEC_PER_DAY + st);
	*e = (time_t)(led * SEC_PER_DAY + et - (timezone - altzone));
	return(1);
}


/* parameter "tim" is Time now */

static int
posixgetdst(char *p, time_t *s, time_t *e, time_t tim)
{
	int lsd,led;
	struct tm *std_tm;
	struct tm res;
	unsigned char stdtype, alttype;
	int stdjul, altjul;
	int stdm, altm;
	int stdn, altn;
	int stdd, altd;
	int xthru = 0;
	int wd_jan01, wd;
	int d, w;
	time_t st,et;
	st = et = 7200;	/* Default for start and end time is 02:00:00 */

	if ((p = posixsubdst(p,&stdtype,&stdjul,&stdm,&stdn,&stdd,&st)) == 0)
		return(0);
	if (*p != ',')
		return(0);
	if ((p = posixsubdst(p+1,&alttype,&altjul,&altm,&altn,&altd,&et)) == 0)
		return(0);

	std_tm = offtime(&tim, -timezone, &res);

	while (xthru++ < 2) {
		lsd = stdjul;
		led = altjul;
		if (stdtype == 'J' && isleap(std_tm->tm_year))
			if (lsd > FEB28)
				++lsd;		/* Correct for leap year */
		if (alttype == 'J' && isleap(std_tm->tm_year))
			if (led > FEB28)
				++led;		/* Correct for leap year */
		if (stdtype == 'M') {		/* Figure out the Julian Day */
			wd_jan01 = std_tm->tm_wday -
				(std_tm->tm_yday % DAYS_PER_WEEK);
			if (wd_jan01 < 0)
				wd_jan01 += DAYS_PER_WEEK;
			if (isleap(std_tm->tm_year))
				wd = (wd_jan01 + __lyday_to_month[stdm-1]) %
					DAYS_PER_WEEK;
			else
				wd = (wd_jan01 + __yday_to_month[stdm-1]) %
					DAYS_PER_WEEK;
			for (d = 1; wd != stdd; ++d)
				wd = ((wd+1) % DAYS_PER_WEEK);
			for (w = 1; w != stdn; ++w) {
				d += DAYS_PER_WEEK;
				if (d > __mon_lengths[
					isleap(std_tm->tm_year)][stdm-1]) {
					d -= DAYS_PER_WEEK;
					break;
				}
			}
			if (isleap(std_tm->tm_year))
				lsd = __lyday_to_month[stdm-1] + d - 1;
			else
				lsd = __yday_to_month[stdm-1] + d - 1;
		}
		if (alttype == 'M') {		/* Figure out the Julian Day */
			wd_jan01 = std_tm->tm_wday -
				(std_tm->tm_yday % DAYS_PER_WEEK);
			if (wd_jan01 < 0)
				wd_jan01 += DAYS_PER_WEEK;
			if (isleap(std_tm->tm_year))
				wd = (wd_jan01 + __lyday_to_month[altm-1]) %
					DAYS_PER_WEEK;
			else
				wd = (wd_jan01 + __yday_to_month[altm-1]) %
					DAYS_PER_WEEK;
			for (d = 1; wd != altd; ++d)
				wd = ((wd+1) % DAYS_PER_WEEK);
			for (w = 1; w != altn; ++w) {
				d += DAYS_PER_WEEK;
				if (d > __mon_lengths[
					isleap(std_tm->tm_year)][altm-1]) {
					d -= DAYS_PER_WEEK;
					break;
				}
			}
			if (isleap(std_tm->tm_year))
				led = __lyday_to_month[altm-1] + d - 1;
			else
				led = __yday_to_month[altm-1] + d - 1;
		}
		if ((lsd <= led) || (xthru == 2))
			break;
		else {	/* Southern Hemisphere */
			std_tm = offtime(&tim, -altzone, &res);
		}
	}	/* for (;;)	*/
	*s = (time_t) (lsd * SEC_PER_DAY + st);
	*e = (time_t) (led * SEC_PER_DAY + et - (timezone - altzone));
	return(1);
}

static char *
posixsubdst(char *p, 
	unsigned char *type, 
	int *jul,
	int *m, 
	int *n, 
	int *d, 
	time_t *tm)
{

/*
**	nnn where nnn is between 0 and 365.
*/
	if (isdigit(*p)) {
		if ( (p = getdigit(p,jul)) == 0 )
			return(0);
		if (*jul < 0 || *jul > 365)
			return(0);
		*type = '\0';
	}
/*
** J<1-365> where February 28 is always day 59, and March 1 is ALWAYS
** day 60.  It is not possible to specify February 29.
**
** This is a hard problem.  We can't figure out what time it is until
** we know when daylight savings time begins and ends, and we can't
** know that without knowing what time it is!?!  Thank you, POSIX!
**
**
*/
	else if (*p == 'J') {
		if ((p = getdigit(p+1,jul)) == 0) 
			return(0);
		if (*jul <= 0 || *jul > 365)
			return(0);
		--(*jul);		/* make it between 0 and 364 */
		*type = 'J';
	}
/*
** Mm.n.d
**	Where:
**	m is month of year (1-12)
**	n is week of month (1-5)
**		Week 1 is the week in which the d'th day first falls
**		Week 5 means the last day of that type in the month
**			whether it falls in the 4th or 5th weeks.
**	d is day of week (0-6) 0 == Sunday
**
** This is a hard problem.  We can't figure out what time it is until
** we know when daylight savings time begins and ends, and we can't
** know that without knowing what time it is!?!  Design by committee.
** The saving grace is that this probably the right way to specify
** Daylight Savings since most countries change on the first/last
** Someday of the month.
*/
	else if (*p == 'M') {
		if ((p = getdigit(p+1,m)) == 0)
			return(0);
		if (*m <= 0 || *m > 12)
			return(0);
		if (*p != '.')
			return(0);
		if ((p = getdigit(p+1,n)) == 0)
			return(0);
		if (*n <= 0 || *n > 5)
			return(0);
		if (*p != '.')
			return(0);
		if ((p = getdigit(p+1,d)) == 0)
			return(0);
		if (*d < 0 || *d > 6)
			return(0);
		*type = 'M';
	}
	else
		return(0);

	if ((*p == '/') && ((p = gettime(p+1,tm,0)) == 0))
		return(0);
	return(p);
}


static void
getusa(time_t *s, time_t *e, struct tm *t)
{
	int i = 0;

	while (t->tm_year < __daytab[i].yrbgn) /* can't be less than 70	*/
		i++;
	*s = __daytab[i].daylb; /* fall through the loop when in correct interval	*/
	*e = __daytab[i].dayle;

	*s = sunday(t, *s);
	*e = sunday(t, *e);
	*s = (time_t)(*s * SEC_PER_DAY + 2 * SEC_PER_HOUR);
	*e = (time_t)(*e * SEC_PER_DAY + SEC_PER_HOUR);
	return;
}

static time_t
sunday(register struct tm *t, register time_t d)
{
	if(d >= 58)
		d += year_size(t->tm_year) - 365;
	return(d - (d - t->tm_yday + t->tm_wday + 700) % 7);
}

static int
_tzload(register char *name)
{
	register int	i;
	register int	fid;
	register struct state *s = _tz_state;

	if (s == 0) {
		s = (struct state *)calloc(1, sizeof (*s));
		if (s == 0)
			return -1;
		_tz_state = s;
	}
	if (name == 0 && (name = TZDEFAULT) == 0)
		return -1;
	{
		register char *	p;
		register int	doaccess;
		char		fullname[MAXPATHLEN];

		doaccess = name[0] == '/';
		if (!doaccess) {
			if ((p = TZDIR) == 0)
				return -1;
			if ((strlen(p) + strlen(name) + 1) >= sizeof fullname)
				return -1;
			(void) strcpy(fullname, p);
			(void) strcat(fullname, "/");
			(void) strcat(fullname, name);
			/*
			** Set doaccess if '.' (as in "../") shows up in name.
			*/
			while (*name != '\0')
				if (*name++ == '.')
					doaccess = TRUE;
			name = fullname;
		}
		if (doaccess && access(name, 4) != 0)
			return -1;
		if ((fid = open(name, 0)) == -1)
			return -1;
	}
	{
		register char *			p;
		register struct tzhead *	tzhp;
		char				buf[8192];

		i = (int)read(fid, buf, sizeof buf);
		if (close(fid) != 0 || i < (int)sizeof(*tzhp))
			return -1;
		tzhp = (struct tzhead *) buf;
		s->timecnt = (time_t) detzcode(tzhp->tzh_timecnt);
		s->typecnt = (time_t) detzcode(tzhp->tzh_typecnt);
		s->charcnt = (time_t) detzcode(tzhp->tzh_charcnt);
		if (s->timecnt > TZ_MAX_TIMES ||
			s->typecnt == 0 ||
			s->typecnt > TZ_MAX_TYPES ||
			s->charcnt > TZ_MAX_CHARS)
				return -1;
		if (i < (int)sizeof(*tzhp) +
			s->timecnt * (4 + (int)(sizeof (char))) +
			s->typecnt * (4 + 2 * (int)(sizeof (char))) +
			s->charcnt * (int)(sizeof (char)))
				return -1;
		if (s->ats) {
			free(s->ats);
			s->ats = 0;
		}
		if (s->types) {
			free(s->types);
			s->types = 0;
		}
		if (s->ttis) {
			free(s->ttis);
			s->ttis = 0;
		}
		if (s->timecnt != 0) {
			s->ats =
			  (time_t *)calloc((size_t)s->timecnt, sizeof(time_t));
			if (s->ats == 0)
				return -1;
			s->types =
			  (char *)calloc((size_t)s->timecnt, sizeof(char));
			if (s->types == 0) {
				free(s->ats);
				s->ats = 0;
				return -1;
			}
		}
		s->ttis =
		  (struct ttinfo *)calloc((size_t)s->typecnt, sizeof(struct ttinfo));
		if (s->ttis == 0) {
			if (s->ats) {
				free(s->ats);
				s->ats = 0;
			}
			if (s->types) {
				free(s->types);
				s->types = 0;
			}
			return -1;
		}
		if (s->chars)
			free(s->chars);
		s->chars =
		  (char *)calloc((size_t)s->charcnt+1, sizeof (char));
		if (s->chars == 0) {
			if (s->ats) {
				free(s->ats);
				s->ats = 0;
			}
			if (s->types) {
				free(s->types);
				s->types = 0;
			}
			free(s->ttis);
			s->ttis = 0;
			return -1;
		}
		p = buf + sizeof *tzhp;
		for (i = 0; i < s->timecnt; ++i) {
			s->ats[i] = detzcode(p);
			p += 4;
		}
		for (i = 0; i < s->timecnt; ++i)
			s->types[i] = (unsigned char) *p++;
		for (i = 0; i < s->typecnt; ++i) {
			register struct ttinfo *	ttisp;

			ttisp = &s->ttis[i];
			ttisp->tt_gmtoff = detzcode(p);
			p += 4;
			ttisp->tt_isdst = (unsigned char) *p++;
			ttisp->tt_abbrind = (unsigned char) *p++;
		}
		for (i = 0; i < s->charcnt; ++i)
			s->chars[i] = *p++;
		s->chars[i] = '\0';	/* ensure '\0' at end */
	}
	/*
	** Check that all the local time type indices are valid.
	*/
	for (i = 0; i < s->timecnt; ++i)
		if (s->types[i] >= s->typecnt)
			return -1;
	/*
	** Check that all abbreviation indices are valid.
	*/
	for (i = 0; i < s->typecnt; ++i)
		if (s->ttis[i].tt_abbrind >= s->charcnt)
			return -1;

	return 0;
}

static time_t
detzcode(char *codep)
{
	register time_t	result;
	register int	i;

	result = 0;
	for (i = 0; i < 4; ++i)
		result = (result << 8) | (codep[i] & 0xff);
	return result;
}
