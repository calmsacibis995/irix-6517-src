/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/cal/RCS/cal.c,v 1.11 1997/05/31 23:25:29 pcr Exp $"

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <locale.h>
#include <fmtmsg.h>
#include <unistd.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

char	mon[] = {
	0,
	31, 29, 31, 30,			/* days per month */
	31, 30, 31, 31,
	30, 31, 30, 31,
};

char	*dayX;
char	*dayP;
char	*monX;
int	sizeP;
int	sizeL;
int	offP[7];			/* offsets in output string */

struct montxt {
	char	*cmsg;
	char	*dmsg;
};

struct montxt montxt[12] = {
	{ _SGI_DMMX_cal_jan,	"January"	},
	{ _SGI_DMMX_cal_feb,	"February"	},
	{ _SGI_DMMX_cal_mar,	"March"		},
	{ _SGI_DMMX_cal_apr,	"April"		},
	{ _SGI_DMMX_cal_may,	"May"		},
	{ _SGI_DMMX_cal_jun,	"June"		},
	{ _SGI_DMMX_cal_jul,	"July"		},
	{ _SGI_DMMX_cal_aug,	"August"	},
	{ _SGI_DMMX_cal_sep,	"September"	},
	{ _SGI_DMMX_cal_oct,	"October"	},
	{ _SGI_DMMX_cal_nov,	"November"	},
	{ _SGI_DMMX_cal_dec,	"December"	},
};

char	*smon[12];
char	*sbuf;

extern struct tm *localtime();
extern long time();
struct tm *thetime;
long timbuf;

char	cmd_label[] = "UX:cal";

/*
 * some message prints
 */
static void
err_year(s)
char *s;
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_invalid_year, "Invalid year %s"), s);
	exit(2);
}

static void
err_month(s)
char *s;
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_invalid_month, "Invalid month %s"), s);
	exit(2);
}

static void
err_dayP()
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    gettxt(_SGI_DMMX_cal_bad_dayP, "bad dayP format"));
	exit(1);
}

/*
 * convert a number
 */
static int
number(str)
char *str;
{
	register n, c;
	register char *s;

	n = 0;
	s = str;
	while(c = *s++) {
		if(c<'0' || c>'9') {
			return(-1);
		}
		n = n*10 + c-'0';
	}
	return(n);
}

/*
 * return day of the week
 * of jan 1 of given year
 */
int
jan1(yr)
{
	register y, d;

	/*
	 *	normal gregorian calendar
	 *	one extra day per four years
	 */
	y = yr;
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

cal(m, y, p, w)
char *p;
{
	register d, i;
	register char *s;

	d = jan1(y);
	mon[2] = 29;
	mon[9] = 30;

	switch((jan1(y+1)+7-d)%7) {

	/*
	 *	non-leap year
	 */
	case 1:
		mon[2] = 28;
		break;

	/*
	 *	1752
	 */
	default:
		mon[9] = 19;
		break;

	/*
	 *	leap year
	 */
	case 2:
		;
	}
	for(i=1; i<m; i++)
		d += mon[i];
	d %= 7;

	for(i = 1; i <= mon[m]; i++) {
		if(i==3 && mon[m]==19) {
			i += 11;
			mon[m] += 11;
		}
		s = p + offP[d];
		if(i > 9)
			*s = i/10+'0';
		*++s = i%10+'0';
		if(++d == 7) {
			d = 0;
			s = p + w;
			p = s;
		}
	}
}

static void
pstr(str, n)
char *str;
{
	register i;
	register char *s;

	s = str;
	i = n;
	while(i--)
		if(*s++ == '\0')
			s[-1] = ' ';
	i = n+1;
	while(i--)
		if(*--s != ' ')
			break;
	s[1] = '\0';
	printf("%s\n", str);
}

/*
 * print formatted month name
 */
static void
monprint(m)
int m;
{
	register int n;
	char monbuf[256];

	n = sprintf(monbuf, monX, smon[m]);
	for(; n < sizeP; n++)
	    monbuf[n] = ' ';
	monbuf[sizeP - 1] = ' ';
	monbuf[sizeP] = 0;
	printf("%s", monbuf);
}

/*
 * print out complete year
 */
static int
pyear(y)
register int y;
{
	register int i;
	register int j;

	if((y < 1) || (y > 9999))
	    return(1);
	printf(gettxt(_SGI_DMMX_cal_year, "Year: %u"), y);
	printf("\n");
	for(i=0; i < 12; i += 3) {
		(void)bzero((void *)sbuf, 6 * sizeL);
		monprint(i);
		monprint(i + 1);
		monprint(i + 2);
		printf("\n%s%s%s\n", dayX, dayX, dayX);
		cal(i+1, y, sbuf, sizeL);
		cal(i+2, y, sbuf + sizeP, sizeL);
		cal(i+3, y, sbuf + sizeP + sizeP, sizeL);

		for(j = 0; j < (6 * sizeL); j+= sizeL)
			pstr(sbuf + j, sizeL);
	}
	printf("\n");
	return(0);
}

/*
 * print out just month
 */
static int
pmonth(m, y)
register int m;
register int y;
{
	register int i;

	if((y < 1) || (y > 9999))
	    return(1);
	printf(gettxt(_SGI_DMMX_cal_month, "   %s %u\n"), smon[m-1], y);
	printf("%s\n", dayX);
	(void)bzero((void *)sbuf, 6 * sizeL);
	cal(m, y, sbuf, sizeL);
	for(i=0; i < (6 * sizeL); i+= sizeL)
		pstr(sbuf + i, sizeL);
	return(0);
}

/*
 * main entry
 */
int
main(argc, argv)
int argc;
char *argv[];
{
	register int y;
	register int m;
	register char *s;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	/*
	 * initialize formats
	 */
	dayX = gettxt(_SGI_DMMX_cal_dayX, " S  M Tu  W Th  F  S   ");
	dayP = gettxt(_SGI_DMMX_cal_dayP, ".. .. .. .. .. .. ..   ");
	monX = gettxt(_SGI_DMMX_cal_monX, "     %s");
	sizeP = strlen(dayP);
	sizeL = 3 * sizeP;

	for(m = 0; m < 12; m++) {
	    smon[m] = gettxt(montxt[m].cmsg, montxt[m].dmsg);
	}
	for(y = 0, s = dayP; y < 7; y++) {
	    if( !(s = strchr(s, '.')))
		err_dayP();
	    offP[y] = s - dayP;
	    s++;
	    if(*s++ != '.')
		err_dayP();
	}

	if( !(sbuf = (char *)malloc(6 * (sizeL + 1)))) {
	    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		gettxt(_SGI_DMMX_outofmem, "Out of memory"));
	    exit(1);
	}

	/*
	 * scan arguments and print calendar
	 */
	switch(argc) {
	    case 1:
		timbuf = time(&timbuf);
		thetime = localtime(&timbuf);
		m = thetime->tm_mon + 1;
		y = thetime->tm_year + 1900;
		if(pmonth(m, y))
		    err_month("<system's time>");
		break;
	    case 2:
		y = number(argv[1]);
		if((y == -1) || pyear(y))
		    err_year(argv[1]);
		break;
	    case 3:
		/* if month is "--", treat as year */
		if (!strcmp(argv[1], "--")) {
			y = number(argv[2]);
			if((y == -1) || pyear(y))
			    err_year(argv[2]);
		} else {
			m = number(argv[1]);
			if((m < 1) || (m > 12))
			    err_month(argv[1]);
			y = number(argv[2]);
			if (y == -1)
			    err_year(argv[2]);
			if(pmonth(m, y))
			    err_year(argv[2]);
		}
		break;
	    default:
		_sgi_nl_usage(SGINL_USAGE, cmd_label,
		    gettxt(_SGI_DMMX_cal_usage, "cal [ [month] year ]"));
		exit(2);
	}
	exit(0);
}

