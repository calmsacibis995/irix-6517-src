/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)date:date.c	1.24.1.7" */
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/date/RCS/date.c,v 1.19 1997/10/24 23:56:42 jiju Exp $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.

		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/*
**	date - with format capabilities and international flair
*/

#include        <stdio.h>
#include	<time.h>
#include	<sys/time.h>
#include	<sys/types.h>
#include	<locale.h>
#include	<fcntl.h>
#ifdef sgi
#include	<utmpx.h>	/* includes utmp.h */
#else
#include	<utmp.h>
#endif
#include	<pfmt.h>
#include	<errno.h>
#include	<string.h>


#define	year_size(A)	(((A) % 4) ? 365 : 366)

static 	char	buf[1024];
static	time_t	clock_val;
static  short	month_size[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static  struct  utmp	wtmp[2] = { {"", "", OTIME_MSG, 0, OLD_TIME, 0, 0, 0}, 
			    {"", "", NTIME_MSG, 0, NEW_TIME, 0, 0, 0} };
#ifdef sgi
static	struct	utmpx	wtmpx[2];
extern	void	getutmpx(const struct utmp *, struct utmpx *);
#endif

static  int uflag = 0;
static  int nflag = 0;                  /* !=0 to not set the network time */
static  int vflag = 0;			/* variable format flag */

/*
 * Procedure:     main
 */
main(argc, argv)
int	argc;
char	**argv;
{
	register struct tm *tp;
	struct timeval tv;
	char *fmt = NULL;
	char *locp;
	int retval;		/* scratch variable */
	
	locp = setlocale(LC_ALL, "");
	if (locp == (char *)0) {
		(void)setlocale(LC_TIME, "");
		(void)setlocale(LC_CTYPE, "");
	}

	(void)setcat("uxcore.abi");
	(void)setlabel("UX:date");

	/*  Initialize variables  */
	(void)time(&clock_val);

	if (argc > 1) {
		if (strcmp(argv[1], "-u") == 0) {
			argc--; argv++;
			uflag++;
		}
		else if (strcmp(argv[1], "-n") == 0) {
			argc--; argv++;
			nflag++;
		}
		else if (strcmp(argv[1], "-a") == 0) {
			if (argc < 3){
				usage(1);
				exit(1);
			}
			get_adj(argv[2], &tv);

			retval = adjtime(&tv, 0);
			if( retval < 0 ) {
				pfmt(stderr, 
				  MM_ERROR, ":142:Cannot adjust date: %s\n",
					strerror(errno));
				exit(1);
			}
			exit(0);
		}		
	}
	if (argc > 1) {	
		if (*argv[1] == '+') {
			fmt = &argv[1][1];
			vflag++;
		}
                else if (strcmp(argv[1], "--") != 0) {
                        if (setdate(localtime(&clock_val), argv[1]))
                                exit(1);
                }
                else if (*argv[2] == '+') {
                        fmt = &argv[2][1];
                        vflag++;
                }
                else if (setdate(localtime(&clock_val), argv[2]))
                                exit(1);
	}

	if (uflag && vflag) {
		tp = gmtime(&clock_val);
		strftime(buf, sizeof(buf), fmt, tp);	
	} else if (uflag && !vflag) {
		tp = gmtime(&clock_val);
		strftime(buf, sizeof(buf),
		   (const char *)gettxt(":143", "%a %b %e %H:%M:%S GMT %Y"),
			tp);
		} else {	/* !uflag */
			tp = localtime(&clock_val);
			strftime(buf, sizeof(buf), fmt, tp);
		}

	puts(buf);

	exit(0);
}

/*
 * Procedure:     setdate
 */
setdate(current_date, date)
struct tm	*current_date;
char		*date;
{
	register int i;			/* scratch variable */
	int mm, dd=0, hh, min, yy, wf=0;
	int minidx = 6;
	static int tdate[4];
	
	static char badconv[] = ":144:Bad conversion\n";

#ifdef sgi
        char *sp, *p;
        int sec = 0;

        if (0 != (sp = strchr(date,'.'))) {
                sec = strtol(sp+1,&p,10);
                if (sec >= 0 && sec < 60 && *p == '\0')
                        *sp = '\0';
                else
                        date[0] = '\0';         /* To force error msg */
        }
#endif 


	/*  Parse date string  */
	switch(strlen(date)) {
	case 12:
		yy = atoi(&date[8]);
                if (yy < 1970 || yy > 2037) {
                        (void) pfmt(stderr, MM_ERROR, badconv);
                        usage(0);
                        return(1);
                }
		date[8] = '\0';
		break;
	case 10:
		i = atoi(&date[8]);
#ifdef old /* Bug#538632 */
		if(i >= 69 && i <= 99)		/* Same as touch */
			yy = 1900 + i;
		else	yy = 2000 + i;
#endif /* old */
                if (i >= 0 && i < 38)
                        yy = 2000 + i;
                else if (i > 69 && i <= 99)
                        yy = 1900 + i;
                else {
                        (void) pfmt(stderr, MM_ERROR, badconv);
                        usage(0);
                        return(1);
                }
		date[8] = '\0';
		break;
	case 8:
		yy = 1900 + current_date->tm_year;
		break;
	case 4: 
		yy = 1900 + current_date->tm_year;
		mm = current_date->tm_mon + 1; 	/* tm_mon goes from 1 to 11*/
		dd = current_date->tm_mday;
		minidx = 2;
		break;	
	default:
		(void) pfmt(stderr, MM_ERROR, badconv);
		usage(0);
		return(1);
	}
	tdate[0] = (int)date[minidx-2];
	tdate[1] = (int)date[minidx-1];
	tdate[2] = (int)date[minidx];
	tdate[3] = (int)date[minidx+1];
	for (i = 0; i < 4; i++) {
		if(!isdigit((int)tdate[i])) {
			(void) pfmt(stderr, MM_ERROR, badconv);
			usage(0);
			return(1);
		}
	}
	min = atoi(&date[minidx]);
	date[minidx] = '\0';
	hh = atoi(&date[minidx-2]);
	date[minidx-2] = '\0';
	if (!dd) { 
		/* if dd is 0 (not between 1 and 31), then 
		 * read the value supplied by the user.
		 */
		dd = atoi(&date[2]);
		date[2] = '\0';
		mm = atoi(&date[0]);
	}
	if(hh == 24)
		hh = 0, dd++;

	/*  Validate date elements  */
	if(!((yy<2100) && (mm >= 1 && mm <= 12) && (dd >= 1 && dd <= 31) &&
		(hh >= 0 && hh <= 23) && (min >= 0 && min <= 59))) {
		(void) pfmt(stderr, MM_ERROR, badconv);
		usage(0);
		return(1);
	}

	/*  Build date and time number  */
	for(clock_val = 0, i = 1970; i < yy; i++)
		clock_val += year_size(i);
	/*  Adjust for leap year  */
	if (year_size(yy) == 366 && mm >= 3)
		clock_val += 1;
	/*  Adjust for different month lengths  */
	while(--mm)
		clock_val += (time_t)month_size[mm - 1];
	/*  Load up the rest  */
	clock_val += (time_t)(dd - 1);
	clock_val *= 24;
	clock_val += (time_t)hh;
	clock_val *= 60;
	clock_val += (time_t)min;
	clock_val *= 60;
#ifdef sgi
        clock_val += sec;
#endif

	if (!uflag) {
		/* convert to GMT assuming standard time */
		/* correction is made in localtime(3C) */

		clock_val += (time_t)timezone;


		/* correct if daylight savings time in effect */

		if (localtime(&clock_val)->tm_isdst)
			clock_val = clock_val - (time_t)(timezone - altzone); 

	}

	(void) time(&wtmp[0].ut_time);


#ifdef sgi
	if ((nflag || !settime(clock_val)) && stime(&clock_val) < 0) {
#else
	if(stime(&clock_val) < 0) {
#endif
		(void) pfmt(stderr, MM_ERROR|MM_NOGET, "%s\n", strerror(errno));
		return(1);
	}
	(void) time(&wtmp[1].ut_time);

	/* Update utmp file - if update fails, just continue */
	pututline(&wtmp[0]);
	pututline(&wtmp[1]);
	
	wf = open(WTMP_FILE, O_WRONLY | O_APPEND);
	if ( wf >= 0) {
		(void)write(wf, (char *)wtmp, sizeof(wtmp));
		close(wf);
	} 
#ifdef sgi
	/*
	 * convert the wtmp entries into utmpx format in wtmpx[0..1]
	 * and append to /var/adm/wtmpx to keep wtmp and wtmpx in sync.
	 */
	getutmpx(&wtmp[0], &wtmpx[0]);
	getutmpx(&wtmp[1], &wtmpx[1]);
	wf = open(WTMPX_FILE, O_WRONLY|O_APPEND);
	if ( wf >= 0) {
		(void)write(wf, (char *)wtmpx, sizeof(wtmpx));
		close(wf);
	} 
#endif

	return(0);
}

/*
 * Procedure:     get_adj
 */
get_adj(cp, tp)
        char *cp;
        struct timeval *tp;
{
        register int mult;
        int sign;

        tp->tv_sec = tp->tv_usec = 0;
        if (*cp == '-') {
                sign = -1;
                cp++;
        } else {
                sign = 1;
        }
        while (*cp >= '0' && *cp <= '9') {
                tp->tv_sec *= 10;
                tp->tv_sec += *cp++ - '0';
        }
        if (*cp == '.') {
                cp++;
                mult = 100000;
                while (*cp >= '0' && *cp <= '9') {
                        tp->tv_usec += (*cp++ - '0') * mult;
                        mult /= 10;
                }
        }
        if (*cp){
        	usage(1);
        	exit(1);
        }

        tp->tv_sec *= sign;
        tp->tv_usec *= sign;
}

/*
 * Procedure:     usage
 */
usage(complain)
int complain;
{
 	if (complain)
		pfmt(stderr, MM_ERROR, ":8:Incorrect usage\n");
	pfmt(stderr, MM_ACTION,
		"uxsgicore:88:Usage:\n\tdate [-u] [+format]\n\tdate [-u | -n] [[mmdd] HHMM | mmddHHMM[[cc]yy]][.ss]\n\tdate [-a [-]sss.fff]\n");
}

#ifdef sgi
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <protocols/timed.h>


/*
 * Set the date in the machines controlled by timedaemons
 * by communicating the new date to the local timedaemon.
 * If the timedaemon is in the master state, it performs the
 * correction on all slaves.  If it is in the slave state, it
 * notifies the master that a correction is needed.
 * Returns 1 on success, 0 on failure.
 */
settime(tv)
time_t tv;
{
	int s, found, length, port, err;
	fd_set ready;
	char hostname[MAXHOSTNAMELEN+1];
	struct timeval tout;
	struct servent *sp;
	struct tsp msg;
	struct sockaddr_in sin, dest, from;

	sp = getservbyname("timed", "udp");
	if (sp == 0) {
		(void) pfmt(stderr, MM_ACTION, "uxsgicore:89:udp/timed: unknown service\n");
		return (0);
	}
	dest.sin_port = sp->s_port;
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = htonl((u_long)INADDR_ANY);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		if (errno != EPROTONOSUPPORT)
			(void) pfmt(stderr, MM_ACTION, "uxsgicore:3:socket\n");
		goto bad;
	}
	bzero((char *)&sin, sizeof (sin));
	sin.sin_family = AF_INET;
	for (port = IPPORT_RESERVED - 1; port > IPPORT_RESERVED / 2; port--) {
		sin.sin_port = htons((u_short)port);
		if (bind(s, (struct sockaddr *)&sin, sizeof (sin)) >= 0)
			break;
		if (errno != EADDRINUSE) {
			if (errno != EADDRNOTAVAIL)
				(void) pfmt(stderr, MM_ACTION, "uxsgicore:90:bind\n");
			goto bad;
		}
	}
	if (port == IPPORT_RESERVED / 2) {
		(void) pfmt(stderr, MM_ACTION, "uxsgicore:91:all ports in use\n");
		goto bad;
	}
	msg.tsp_type = TSP_SETDATE;
	msg.tsp_vers = TSPVERSION;
	(void) gethostname(hostname, sizeof (hostname));
	(void) strncpy(msg.tsp_name, hostname, sizeof (hostname));
	msg.tsp_seq = 0;
	msg.tsp_time.tv_sec = htonl(tv);
	msg.tsp_time.tv_usec = 0;
	if (connect(s, &dest, sizeof(struct sockaddr_in)) < 0) {
		(void) pfmt(stderr, MM_ACTION, "uxsgicore:92:connect\n");
		goto bad;
	}
	if (send(s, (char *)&msg, sizeof (struct tsp), 0) < 0) {
		if (errno == ECONNREFUSED) {
			(void)close(s);	/* no daemon */
			return (0);
		}
		(void) pfmt(stderr, MM_ACTION, "uxsgicore:93:send\n");
		goto bad;
	}

	FD_ZERO(&ready);
	FD_SET(s, &ready);
	for (;;) {
		tout.tv_sec = 5;
		tout.tv_usec = 0;
		found = select(FD_SETSIZE, &ready,
			       (fd_set *)0, (fd_set *)0, &tout);

		length = sizeof(err);
		if (getsockopt(s, SOL_SOCKET, SO_ERROR,
			       (char *)&err, &length) == 0
		    && err) {
			errno = err;
			if (errno == ECONNREFUSED)
				return (0);
			(void) pfmt(stderr, MM_ACTION, "uxsgicore:94:send (delayed error)");
			break;
		}

		if (found <= 0)
			break;

		length = sizeof (struct sockaddr_in);
		if (recvfrom(s, (char *)&msg, sizeof (struct tsp), 0,
			     &from, &length) < 0) {
			if (errno != ECONNREFUSED)
				(void) pfmt(stderr, MM_ACTION, "uxsgicore:95:recvform");
			break;
		}

		if (msg.tsp_type == TSP_ACK
		    || msg.tsp_type == TSP_DATEACK) {
			(void)close(s);
			return (1);
		}
	}
bad:
	(void) pfmt(stderr, MM_ACTION,
		"uxsgicore:96: Can't reach time daemon, time set locally.\n");
	(void)close(s);
	return (0);
}
#endif
