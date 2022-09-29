/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Portions Copyright (c) 1988, Sun Microsystems, Inc.	*/
/*	All Rights Reserved. 					*/

#ident	"@(#)touch:touch.c	1.11.1.2"
/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <pfmt.h>
#include <string.h>
#include <errno.h>
#include <msgs/uxsgicore.h>

#define	dysize(y) \
	(((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0) ? 366 : 365)

struct	stat64	stbuf;
int	status;
int dmsize[12]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

char	*cbp;
time_t	timbuf;

gtime(int which, size_t len)
{
	register int i, y, t;
	int d, h, m;
	long nt;
	int c = 0;		/* entered century value */
	int s = 0;		/* entered seconds value */
	int point = 0;		/* relative byte position of decimal point */
	int ssdigits = 0;	/* count seconds digits include decimal pnt */
	int noyear = 0;		/* 0 means year is entered; 1 means none */

	tzset();

	/*
	 * mmddhhmm is a total of 8 bytes min
	 */
	if (len < 8)
		return(1);
	if (which) {		/* if '1'; means -t option */
		/*
		 * -t [[cc]yy]mmddhhmm[.ss] is a total of 15 bytes max
		 */
		if (len > 15)
			return(1);
		/*
		 *  Determine the decimal point position, if any.
		 */
		for (i=0; i<len; i++) {
			if (*(cbp + i) == '.') {
				point = i;
				break;
			}
		}
		/*
		 *  If there is a decimal point present,
		 *  AND:
		 *
		 *	the decimal point is positioned in bytes 0 thru 7;
		 *  OR
		 *	the the number of digits following the decimal point
		 *	is greater than two
		 *  OR
		 *	the the number of digits following the decimal point
		 *	is less than two
		 *  then,
		 *	error terminate.
		 *      
		 */
		/* the "+ 1" below means add one for decimal */
		if (point && ((point < 8) || ((len - (point + 1)) > 2) ||
				((len - (point + 1)) < 2)) )
		{
			return(1);
		}
		/*
		 * -t [[cc]yy]mmddhhmm.[ss] is greater than 12 bytes
		 * -t [yy]mmddhhmm.[ss]     is greater than 12 bytes
		 *
		 *  If there is no decimal present and the length is greater
		 *  than 12 bytes, then error terminate.
		 */
		if (!point && (len > 12))
			return(1);
		switch(len) {
		case 11:
			if (*(cbp + 8) != '.')
				return(1);
			break;
		case 13:
			if (*(cbp + 10) != '.')
				return(1);
			break;
		case 15:
			if (*(cbp + 12) != '.')
				return(1);
			break;
		}
		if (!point)
			ssdigits = 0;
		else
			ssdigits = ((len - point) + 1);
		if ((len - ssdigits) > 10) {
			/*
			 * -t ccyymmddhhmm is the input
			 */

			/* detemine c -- century number */
			c = gpair();

			/* detemine y -- year    number */
			y = gpair();
			if (y<0) {
				(void) time(&nt);
				y = localtime(&nt)->tm_year;
			}
			if ((c != 19) && ((y >= 69) && (y <= 99)))
				return(1);
			if ((c != 20) && ((y >= 0) && (y <= 68)))
				return(1);
			goto mm_next;
		}
		if ((len - ssdigits) > 8) {
			/*
			 * -t yymmddhhmm is the input
			 */
			/* detemine yy -- year    number */
			y = gpair();
			if (y<0) {
				(void) time(&nt);
				y = localtime(&nt)->tm_year;
			}
			if ((y >= 69) && (y <= 99))
				c = 19;			/* 19th century */
			if ((y >= 0) && (y <= 68))
				c = 20;			/* 20th century */
			goto mm_next;
		}
		if ((len - ssdigits) < 10) {
			/*
			 * -t mmddhhmm is the input
			 */
			noyear++;
		}
	}
mm_next:
	t = gpair();
	if(t<1 || t>12)
		return(1);
	d = gpair();
	if(d<1 || d>31)
		return(1);
	h = gpair();
	if(h == 24) {
		h = 0;
		d++;
	}
	m = gpair();
	if(m<0 || m>59)
		return(1);
	if (!which) {
		/*
		 *  There wasn't a "-t" input.
		 */
		y = gpair();
		if (y<0) {
			(void) time(&nt);
			y = localtime(&nt)->tm_year % 100;
		}
                if ((y >= 69) && (y <= 99))
                        c = 19;                 /* 19th century */
                if ((y >= 0) && (y <= 68))
                        c = 20;                 /* 20th century */
	} else {
		/*
		 * There was a "-t" input.
		 * If there is a decimal get the seconds inout
		 */
		 if (point) {
			cbp++;		/* skip over decimal point */
			s = gpair();	/* get [ss] */
			if (s<0) {
				return(1);
			}
			if (!((s >= 0) && (s <= 61)))
				return(1);
		 }
		 if (noyear) {
			(void) time(&nt);
			y = localtime(&nt)->tm_year;
		 }
	}
	if (*cbp == 'p')
		h += 12;
	if (h<0 || h>23)
		return(1);
	timbuf = 0;
	if (c && (c == 20))
		y += 2000;
	else
		y += 1900;
	for(i=1970; i<y; i++)
		timbuf += dysize(i);
	/* Leap year */
	if (dysize(y)==366 && t >= 3)
		timbuf += 1;
	while(--t)
		timbuf += dmsize[t-1];
	timbuf += (d-1);
	timbuf *= 24;
	timbuf += h;
	timbuf *= 60;
	timbuf += m;
	timbuf *= 60;
	timbuf += s;
	return(0);
}

gpair()
{
	register int c, d;
	register char *cp;

	cp = cbp;
	if(*cp == 0)
		return(-1);
	c = (*cp++ - '0') * 10;
	if (c<0 || c>100)
		return(-1);
	if(*cp == 0)
		return(-1);
	if ((d = *cp++ - '0') < 0 || d > 9)
		return(-1);
	cbp = cp;
	return (c+d);
}

/*
 * Could this string be a datetime specification of the form "mmddhhmm[yy]"?
 * If it contains all digits and is 8 or 10 digits long, then it could be;
 * otherwise it isn't.
 */
static
isdatetime(s)
char *s;
{
	register int	c;
	register int	i = 0;

	while(c = s[i]) {
		if(!isdigit(c))
			return(0);
		i++;
	}

	if (i != 8 && i != 10)
		return(0);
	else
		return(1);
}

main(argc, argv)
char *argv[];
{
	register c;
#ifdef sgi
        struct utimbuf times;
#else
	struct utbuf { time_t actime, modtime; } times;
#endif
	int mflg=1, aflg=1, cflg=0, fflg=0, nflg=0, errflg=0, optc, fd;
	int rflg=0, tflg=0;
	int stflg=0;
	char *proto;
	char *dashr_file = 0;
	struct  stat64 prstbuf; 
	char *usage_OB = 
		":583:Usage: %s [%s] [mmddhhmm[yy]] file ...\n";
	char *usage_XOP = 
		":Usage: %s [%s] [%s | %s] files\n";
	char form_XOP[100];
	char *tusage = "-amc";
	char *susage = "-f file";
	char *rusage = "-r ref_file";
	char *t_usage = "-t [[cc]yy]mmddhhmm[.ss]";
	char *susageid = ":584";
	extern char *optarg, *basename();
	extern int optind;
	int i, k;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	argv[0] = basename(argv[0]);
	if (!strcmp(argv[0], "settime")) {
		(void)setlabel("UX:settime");
		while ((optc = getopt(argc, argv, "f:")) != EOF)
			switch (optc) {
			case 'f':
				fflg++;
				proto = optarg;
				break;
			default:
				errflg++;
				break;
			};
		stflg = 1;
		++cflg;
	} 
	else {
		(void)setlabel("UX:touch");
		while ((optc=getopt(argc, argv, "amcfr:t:")) != EOF)
			switch(optc) {
			case 'm':
				mflg++;
				aflg--;
				break;
			case 'a':
				aflg++;
				mflg--;
				break;
			case 'c':
				cflg++;
				break;
			case 'f':
				break;		/* silently ignore   */ 
			case 'r':
				rflg++;
				dashr_file = optarg;
				break;
			case 't':
				tflg++;
				cbp= optarg;
				break;
			case '?':
				errflg++;
			}
	}
	if( ((argc-optind) < 1) || errflg || 
		(rflg && tflg) || (fflg && rflg) || (tflg && stflg) ||
			(rflg && stflg) )
	{
		if (!errflg)
			pfmt(stderr, MM_ERROR, ":8:Incorrect usage\n");
		(void) pfmt(stderr, MM_ACTION, usage_OB, argv[0],
				stflg ? gettxt(susageid, susage) : tusage);
		if (!stflg) {
			for(i=0; i<sizeof(form_XOP); i++)
				form_XOP[i] = 0;
			k = sprintf(form_XOP, "%s%s", 
				_SGI_MMX_touch_usage, usage_XOP);
			(void) pfmt(stderr, MM_STD, form_XOP, 
				argv[0], tusage, rusage, t_usage);
		}
		exit(2);
	}
	if (rflg) {
		if (stat64(dashr_file, &prstbuf) == -1) {
			pfmt(stderr, MM_ERROR, ":12:%s: %s\n",
				dashr_file, strerror(errno));
			exit(2);
		}
		status = 0;
		goto got_dashr;
	}
	status = 0;
	if (fflg) {
		if (stat64(proto, &prstbuf) == -1) {
			pfmt(stderr, MM_ERROR, ":12:%s: %s\n", proto,
				strerror(errno));
			exit(2);
		}
	} 
	/*
	 * If the -t option wasn't specifed, we need to figure out if the
	 * first operand is a datetime specification or a filename.
	 * If more than 1 operand was specifed, and the first one looks like
	 * a datetime, assume it IS a datetime.  Otherwise it is a filename.
	 */
	else if(!tflg && ((argc - optind) <= 1 || !isdatetime(argv[optind])))
		if((aflg <= 0) || (mflg <= 0))
                        prstbuf.st_atime = prstbuf.st_mtime = time((long *) 0);
		else
			nflg++;
	else {
		if (!tflg) {
			if (isdatetime(argv[optind]) && (argc >= 3)) {
				cbp = (char *)argv[optind++];;
			}
		}

		if(gtime(tflg, (size_t)strlen(cbp))) {
			(void) pfmt(stderr, MM_ERROR,
				":585:Bad date conversion\n");
			exit(2);
		}
		timbuf += timezone;
		if (localtime(&timbuf)->tm_isdst)
			timbuf += -1*60*60;
		prstbuf.st_mtime = prstbuf.st_atime = timbuf;
	}
got_dashr:
	for(c=optind; c<argc; c++) {
		if(stat64(argv[c], &stbuf)) {
			if(cflg) {
				if (stflg)
					status++;
				continue;
			}
			else if ((fd = creat (argv[c], (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))) < 0) {
				(void) pfmt(stderr, MM_ERROR, 
					":148:Cannot create %s: %s\n", 
					argv[c], strerror(errno));
				status++;
				continue;
			}
			else {
				(void) close(fd);
				if(stat64(argv[c], &stbuf)) {
					(void) pfmt(stderr, MM_ERROR,
						":5:Cannot access %s: %s\n",
						argv[c], strerror(errno));
					status++;
					continue;
				}
			}
		}
		times.actime = prstbuf.st_atime;
		times.modtime = prstbuf.st_mtime;
		if (!rflg && (mflg <= 0))
			times.modtime = stbuf.st_mtime;
		if (!rflg && (aflg <= 0))
			times.actime = stbuf.st_atime;

#ifdef sgi
		if(utime(argv[c], (nflg? 0: &times))) {
#else
		if(utime(argv[c], (struct utbuf *)(nflg? 0: &times))) {
#endif
			(void) pfmt(stderr, MM_ERROR,
				":586:Cannot change times on %s: %s\n",
				argv[c], strerror(errno));
			status++;
			continue;
		}
	}
	exit(status);	/*NOTREACHED*/
}
