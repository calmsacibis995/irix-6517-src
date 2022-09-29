/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*  NOTE: this was originally the att1.y code from the 'at' command */

%{
#ident	"cmd/wakeupat/parse.y: $Revision: 1.7 $"

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/syssgi.h>

#define	dysize(A)	(((A)%4) ? 365 : 366)

int	gmtflag = 0;
int	dflag = 0;
char	*prog;
char	argpbuf[80];
char	pname[80];
char	pname1[80];
char	argbuf[80];
struct	tm	*tp, at, rt;
extern char *argp;
char 	now_flag;
int	utc_flag;

time_t	when, now;
time_t gtime(struct tm *tptr);
void atime(struct tm *a, struct tm *b);

int	mday[12] =
{
	31,38,31,
	30,31,30,
	31,31,30,
	31,30,31,
};
int	mtab[12] =
{
	0,   31,  59,
	90,  120, 151,
	181, 212, 243,
	273, 304, 334,
};
int     dmsize[12] = {
	31,28,31,30,31,30,31,31,30,31,30,31};


main(int cnt, char **args)
{
	int i;
	size_t tlen = 0;

	prog = *args;

	if(cnt < 2) {
		fprintf(stderr, "Usage: %s date string\n", prog);
		exit(1);
	}

	for(i=1; i < cnt; i++) {
		tlen += strlen(args[i]) + 1;
		if(tlen >= sizeof(argpbuf)) {
			fprintf(stderr, "%s: date string is too long\n", prog);
			exit(2);
		}
		strcat(argpbuf,args[i]);
		strcat(argpbuf, " ");
	}

	argp = argpbuf;
	time(&now);
	tp = localtime(&now);
	mday[1] = 28 + leap(tp->tm_year);
	yyparse();
	atime(&at, &rt);
	when = gtime(&at);
	if(!gmtflag) {
		when += timezone;
		if(localtime(&when)->tm_isdst)
			when -= 60 * 60;
	}
	if (utc_flag) {
		if (when < now) when +=24 * 60 * 60; /* add a day */
	}
	if (now_flag == 2/*RIGHT_NOW*/) when = now;
	if((unsigned long)when < (unsigned long)now) {
		fprintf(stderr, "The date \"%s\" is before current time\n", argpbuf);
		exit(3);
	}
	if(syssgi(SGI_SET_AUTOPWRON, when)) {
		if(errno == ENOPKG)
			fprintf(stderr,
				"%s: This system does not support software power on\n",
				prog);
		else
			fprintf(stderr, "%s: Unable to set auto power on time: %s\n",
				prog, strerror(errno));
		exit(4);
	}
	printf("Set power on time for:  %s", ctime(&when));
	return 0;
}


/*
 * return time from time structure
 */
time_t
gtime(struct tm *tptr)
{
	register i;
	long	tv;
	extern int dmsize[];

	tv = 0;
	for (i = 1970; i < tptr->tm_year+1900; i++)
		tv += dysize(i);
	if (dysize(tptr->tm_year) == 366 && tptr->tm_mon >= 2)
		++tv;
	for (i = 0; i < tptr->tm_mon; ++i)
		tv += dmsize[i];
	tv += tptr->tm_mday - 1;
	tv = 24 * tv + tptr->tm_hour;
	tv = 60 * tv + tptr->tm_min;
	tv = 60 * tv + tptr->tm_sec;
	return tv;
}

yywrap()
{
	return 1;
}

void
endit(char *err)
{
	fprintf(stderr, "%s: Unable to convert %s to a valid date", prog,
		argpbuf);
	if(err) fprintf(stderr, ": %s", err);
	fputc('\n', stderr);
	exit(5);
}

/*ARGSUSED*/
void
yyerror(const char *s)
{
	endit(NULL);
}

/*
 * add time structures logically
 */
void
atime(struct tm *a, struct tm *b)
{
	if ((a->tm_sec += b->tm_sec) >= 60) {
		b->tm_min += a->tm_sec / 60;
		a->tm_sec %= 60;
	}
	if ((a->tm_min += b->tm_min) >= 60) {
		b->tm_hour += a->tm_min / 60;
		a->tm_min %= 60;
	}
	if ((a->tm_hour += b->tm_hour) >= 24) {
		b->tm_mday += a->tm_hour / 24;
		a->tm_hour %= 24;
	}
	a->tm_year += b->tm_year;
	if ((a->tm_mon += b->tm_mon) >= 12) {
		a->tm_year += a->tm_mon / 12;
		a->tm_mon %= 12;
	}
	a->tm_mday += b->tm_mday;
	while (a->tm_mday > mday[a->tm_mon]) {
		a->tm_mday -= mday[a->tm_mon++];
		if (a->tm_mon > 11) {
			a->tm_mon = 0;
			mday[1] = 28 + leap(++a->tm_year);
		}
	}
}

leap(year)
{
	return year % 4 == 0;
}

%}
%{


extern	int	gmtflag;
extern	int	mday[];
extern	struct	tm *tp, at, rt;
extern	char 	now_flag;
extern	int	utc_flag;
%}
%token	TIME
%token	NOW
%token	NOON
%token	MIDNIGHT
%token	MINUTE
%token	HOUR
%token	DAY
%token	WEEK
%token	MONTH
%token	YEAR
%token	UNIT
%token	SUFF
%token	AM
%token	PM
%token	ZULU
%token	NEXT
%token	NUMB
%token	COLON
%token	COMMA
%token	PLUS
%token	TZ_NAME
%token	UTC
%token	UNKNOWN
%right	NUMB
%%

args
	: time date incr {
		if (at.tm_min >= 60 || at.tm_hour >= 24)
			endit("bad minute/hour");
		if (at.tm_mon >= 12 || at.tm_mday > mday[at.tm_mon])
			endit("bad month/day");
                /* Make sure the validity of the year */
                if   (!(((at.tm_year >= 0) && (at.tm_year < 38)) ||
                        ((at.tm_year > 69) && (at.tm_year < 100)) ||
                        ((at.tm_year > 1969) && (at.tm_year < 2038))))
                        {
                                endit("bad year");
                        }
                if (at.tm_year < 38)
                        at.tm_year += 100;
                if (at.tm_year >= 138)
                        at.tm_year -= 1900;
                if (at.tm_year < 70 || at.tm_year >= 138)
                        {
                        endit("bad year");
                        }
		return 0;
	}
	| time date incr UNKNOWN {
		yyerror(NULL);
	}
	;

time
	: hour opt_suff {
	checksuff:	/* NOTE: for action jumping here: the last two */
			/*       parameters are used as $1 and $2 */
		at.tm_hour = $1;
		switch ($2) {
		case PM:
			if (at.tm_hour < 1 || at.tm_hour > 12)
				endit("bad hour");
			at.tm_hour %= 12;
			at.tm_hour += 12;
			break;
		case AM:
			if (at.tm_hour < 1 || at.tm_hour > 12)
				endit("bad hour");
			at.tm_hour %= 12;
			break;
		case ZULU:
			if (at.tm_hour == 24 && at.tm_min != 0)
				endit("bad zulu time");
			at.tm_hour %= 24;
			gmtflag = 1;
		}
	}
	| hour opt_tz {
	checktz:	
		at.tm_hour = $1;
		switch ($2) {
		case UTC:
			gmtflag = 1;
			utc_flag = 1;
			break;
		}
	}
	| hour COLON number opt_suff {
		at.tm_min = $3;
		$3 = $1;
		goto checksuff;
	}
	| hour COLON number opt_tz {
		at.tm_min = $3;
		$3 = $1;
		goto checktz;
	}
	| hour minute opt_suff {
		at.tm_min = $2;
		$2 = $1;
		goto checksuff;
	}
	| hour minute opt_tz {
		at.tm_min = $2;
		$2 = $1;
		goto checktz;
	}
	| hour SUFF TZ_NAME {
		switch ($3) {
                case UTC:
                        gmtflag = 1;
                        utc_flag = 1;
                        break;
                }
		$3 = $2;
		$2 = $1;
		goto checksuff;
	}
	| hour COLON number SUFF TZ_NAME {
		at.tm_min = $3;
		switch ($5) {
                case UTC:
                        gmtflag = 1;
                        utc_flag = 1;
                        break;
                }
		$5 = $4;
		$4 = $1;
		goto checksuff;
	}
	| TIME {
		switch ($1) {
		case NOON:
			at.tm_hour = 12;
			break;
		case MIDNIGHT:
			at.tm_hour = 0;
			break;
		case NOW:
			at.tm_hour = tp->tm_hour;
			at.tm_min = tp->tm_min;
			now_flag = 1;
			break;
		}
	}
	;

date
	: /*empty*/ {
		at.tm_mday = tp->tm_mday;
		at.tm_mon = tp->tm_mon;
		at.tm_year = tp->tm_year%100;
		if ((at.tm_hour < tp->tm_hour)
			|| ((at.tm_hour==tp->tm_hour)&&(at.tm_min<tp->tm_min)))
			rt.tm_mday++;
	}
	| MONTH number {
		at.tm_mon = $1;
		at.tm_mday = $2;
		at.tm_year = tp->tm_year%100;
		if (at.tm_mon < tp->tm_mon){
			at.tm_year++;
			at.tm_year = at.tm_year%100;
                        mday[1] = 28 + leap(at.tm_year);
                }
	}
	| MONTH number COMMA number {
		at.tm_mon = $1;
		at.tm_mday = $2;
		at.tm_year = $4;
		mday[1] = 28 + leap(at.tm_year);
	}
	| DAY {
		at.tm_mon = tp->tm_mon;
		at.tm_mday = tp->tm_mday;
		at.tm_year = tp->tm_year%100;
		if ($1 < 7) {
			rt.tm_mday = $1 - tp->tm_wday;
			if (rt.tm_mday < 0)
				rt.tm_mday += 7;
		} else if ($1 == 8)
			rt.tm_mday += 1;
	}
	;

incr
	: /*empty*/ {
		if (now_flag) now_flag ++; /* right now */
	}
	| NEXT UNIT	{ addincr:
		switch ($2) {
		case MINUTE:
			rt.tm_min += $1;
			break;
		case HOUR:
			rt.tm_hour += $1;
			break;
		case DAY:
			rt.tm_mday += $1;
			break;
		case WEEK:
			rt.tm_mday += $1 * 7;
			break;
		case MONTH:
			rt.tm_mon += $1;
			break;
		case YEAR:
			rt.tm_year += $1;
			mday[1] = 28 + leap(at.tm_year + rt.tm_year);
			break;
		}
	}
	| PLUS opt_number UNIT { goto addincr; }
	;

hour
	: NUMB		{ $$ = $1; }
	| NUMB NUMB	{ $$ = 10 * $1 + $2; }
	;
minute
	: NUMB NUMB	{ $$ = 10 * $1 + $2; }
	;
number
	: NUMB		{ $$ = $1; }
	| number NUMB	{ $$ = 10 * $1 + $2; }
	;
opt_number
	: /* empty */	{ $$ = 1; }
	| number	{ $$ = $1; }
	;
opt_suff
	: /* empty */	{ $$ = 0; }
	| SUFF		{ $$ = $1; }
	;
opt_tz
	: /* empty */ 	{ $$ = 0; }
	| TZ_NAME	{ $$ = $1; }
	;
%%
