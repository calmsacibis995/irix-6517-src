/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * Changes Copyright (c) 1989 Silicon Graphics, Inc.
 * based on  @(#)w.c	5.3 (Berkeley) 2/23/86
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * w - print system status (who and what)
 *
 * This program is similar to the systat command on Tenex/Tops 10/20
 * It used to need read permission on /dev/mem and /dev/kmem but
 * has been converted to use sysget.
 */
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <utmpx.h>
#include <sys/stat.h>

#include <sys/time.h>
#include <sys/times.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysmp.h>
#include <sys/sysget.h>
#include <sys/var.h>
#include <sys/ioctl.h>
#include <sys/procfs.h>
#include <dirent.h>
#include <fcntl.h>
#include <paths.h>
#include <errno.h>

/*
 * The proc filesystem (see proc(4) manual page)
 * is scanned for information
 */
struct prcred pcred;
struct prpsinfo pinfo;

#define	equal(a,b)	(!strcmp((a),(b)))

#define NMAX sizeof(utmp->ut_name)
#define LMAX NMAX

#define ARGWIDTH	35	/* # chars left on 80 col crt for args */

struct pr {
	pid_t	w_pid;			/* proc.p_pid */
	time_t	w_time;			/* CPU time used by this process */
	time_t	w_ctime;		/* CPU time used by children */
	time_t	w_start;		/* time process started */
	dev_t	w_tty;			/* tty device of process */
	uid_t	w_uid;			/* uid of process */
	char	w_comm[PRCOMSIZ+1];	/* proc name, null terminated */
	char	w_args[ARGWIDTH+1];	/* args if interesting process */
} *pr;

dev_t	tty;
uid_t	uid;
char	doing[520];		/* process attached to terminal */
time_t	proctime;		/* cpu time of process in doing */
int avenrun[3];

#define	DIV60(t)	((t+30)/60)    /* x/60 rounded */ 
#define	TTYEQ		(tty == pr[i].w_tty && uid == pr[i].w_uid)
#define IGINT		(1+3*1)		/* ignoring both SIGINT & SIGQUIT */

static time_t findidle(char *);
static void readpr(void);
static long getnamelist(void);
static void gettty(char *devstr);
static void putline(void);
static void prttime(time_t, char *);
static void prtat(long *);

int	debug;			/* true if -d flag: debugging output */
int	ttywidth = 80;		/* width of tty */
int	header = 1;		/* true if -h flag: don't print heading */
int	lflag = 1;		/* true if -l flag: long style output */
#ifdef UTMPX_FILE
int	wide_from = 0;		/* print "from" info on separate line */
int	prfrom = 1;		/* true if not -f flag: print host from */
#else
int	prfrom = 0;
#endif
int	login;			/* true if invoked as login shell */
time_t	idle;			/* number of minutes user is idle */
int	nusers;			/* number of users logged in now */
char *	sel_user;		/* login of particular user selected */
char firstchar;			/* first char of name of prog invoked as */
time_t	jobtime;		/* total cpu time visible */
time_t	now;			/* the current time of day */
time_t	uptime = 0;		/* time of last reboot & elapsed time since */
int	np;			/* number of processes currently active */
int     use_tty_flag = 1;       /* Default output is assumed to be the terminal. */
struct	utmp *utmp;
#ifdef UTMPX_FILE
struct	utmpx *utmpx;
#endif

int
main(int argc, char **argv)
{
	int days, hrs, mins;
	register int i, j;
	char *cp;
	register int curstart;
	struct winsize win;
	struct tms tms;
	sgt_cookie_t ck;
	int sgt_res;

	login = (argv[0][0] == '-');
	cp = rindex(argv[0], '/');
	firstchar = login ? argv[0][1] : (cp==0) ? argv[0][0] : cp[1];
	cp = argv[0];	/* for Usage */

	while (argc > 1) {
		if (argv[1][0] == '-') {
			for (i=1; argv[1][i]; i++) {
				switch(argv[1][i]) {

				case 'd':
					debug++;
					break;

#ifdef UTMPX_FILE
				case 'f':
					prfrom = !prfrom;
					break;
				case 'W':
					wide_from++;
					prfrom = 0;
					break;
#endif

				case 'h':
					header = 0;
					break;

				case 'l':
					lflag++;
					break;

				case 's':
					lflag = 0;
					break;

				case 'u':
				case 'w':
					firstchar = argv[1][i];
					break;

				default:
					printf("Bad flag %s\n", argv[1]);
					exit(1);
				}
			}
		} else {
			if (!isalnum(argv[1][0]) || argc > 2) {
				printf("Usage: %s [ -hlsuw ] [ user ]\n", cp);
				exit(1);
			} else
				sel_user = argv[1];
		}
		argc--; argv++;
	}

	if (firstchar != 'u') {
		readpr();
		if (ioctl(1, TIOCGWINSZ, &win) != -1 && win.ws_col > 70)
			ttywidth = win.ws_col;

                /*  Fix for output not being sent to screen -- check ws.col 
		    and if 0, the a boolean flag (global) is reset.
		*/
		if (win.ws_col == 0) 
			use_tty_flag = 0; 

	}

	time(&now);
	if (header) {
		/* Print time of day */
		prtat(&now);

		while ( ( utmp = getutent()) != NULL ) {
			if (utmp->ut_type == USER_PROCESS )
				nusers++;
			else if (utmp->ut_type == BOOT_TIME) {
				uptime = now -utmp->ut_time;
			}
		}

		/* get uptime via times() only if it is not 
		 * available via the utmp entry. 
		 */
		if (!uptime) uptime = times(&tms) / HZ;

		uptime += 30;
		days = uptime / (60*60*24);
		uptime %= (60*60*24);
		hrs = uptime / (60*60);
		uptime %= (60*60);
		mins = uptime / 60;

		printf("  up");
		if (days > 0)
			printf(" %d day%s,", days, days>1?"s":"");
		if (hrs > 0 && mins > 0) {
			printf(" %2d:%02d,", hrs, mins);
		} else {
			if (hrs > 0)
				printf(" %d hr%s,", hrs, hrs>1?"s":"");
			if (mins > 0)
				printf(" %d min%s,", mins, mins>1?"s":"");
		}

		printf("  %d user%s", nusers, nusers>1?"s":"");

		/*
		 * Print 1, 5, and 15 minute load averages.
		 * (Found by looking in kernel for avenrun).
		 * On a cell system there is one per kernel so we
		 * take the average by dividing by the cells.
		 */
		printf(",  load average:");

		SGT_COOKIE_INIT(&ck);
		SGT_COOKIE_SET_KSYM(&ck, KSYM_AVENRUN);
		sgt_res = sysget(SGT_KSYM, (char *)avenrun, sizeof(avenrun),
			SGT_READ | SGT_SUM, &ck);
		if (sgt_res < 0) {
				perror("sysget");
				exit(errno);
		}
		sgt_res /= sizeof(avenrun);	/* Number of cells */

		for (i=0; i < (sizeof avenrun/sizeof avenrun[0]); i++) {
			avenrun[i] /= sgt_res;
			if (i > 0)
				printf(",");
			printf(" %.2f", (((double)avenrun[i])/1024.0));
		}

		printf("\n");
		if (firstchar == 'u')
			exit(0);

		/* Headers for rest of output */
		if (lflag && prfrom)
			printf("User     tty from            login@   idle   JCPU   PCPU  what\n");
		else if (lflag)
			printf("User     tty       login@   idle   JCPU   PCPU  what\n");
		else if (prfrom)
			printf("User    tty from              idle  what\n");
		else
			printf("User    tty    idle  what\n");
		fflush(stdout);
	}


	setutent();
	while ( ( utmp = getutent() ) != NULL )
	{
		static struct utmpx utmpx_entry;

		if (utmp->ut_type != USER_PROCESS)
			continue;	/* that tty is free */
		if (sel_user && strncmp(utmp->ut_name, sel_user, NMAX) != 0)
			continue;	/* we wanted only somebody else */

		if ( equal( utmp->ut_line, "syscon" ) ||
		     equal( utmp->ut_line, "systty" ) )
			continue;

#ifdef UTMPX_FILE
		memcpy(utmpx_entry.ut_id, utmp->ut_id, sizeof(utmp->ut_id));
		utmpx_entry.ut_type = utmp->ut_type;

		setutxent();
		utmpx = getutxid((struct utmpx *)&utmpx_entry);
		if (utmpx != NULL) {
		    /* 
		     * Make sure the entries match -- the utmpx one may
		     * be out-of-date.
		     */
		    /* User names instead of of pids are compared for
		     * match. 
		     */
			
		    if (strncmp(utmp->ut_user, utmpx->ut_user,
				MIN(sizeof(utmp->ut_user), sizeof(utmpx->ut_user))) ||
			utmp->ut_time != utmpx->ut_xtime) {
			utmpx = NULL;
		    } else {
			/* 
			 * ftpd puts hostname in ut_line -- use proper id 
			 * from utx_line.
			 */
			strncpy(utmp->ut_line, utmpx->ut_line, 
				MIN(sizeof(utmp->ut_line), sizeof(utmpx->ut_line)));
		    }
		}
#endif
		gettty( utmp->ut_line );
		idle = findidle( utmp->ut_line );

		jobtime = 0;
		proctime = 0;
		strcpy(doing, "-");	/* default act: normally never prints */
		curstart = -1;
		for (i=0; i<np; i++) {	/* for each process on this tty */
			if (!(TTYEQ))
				continue;
			jobtime += pr[i].w_time + pr[i].w_ctime;
			proctime += pr[i].w_time;
			/* 
			 * Meaning of debug fields following proc name is:
			 *		(==> this proc is not a candidate.)
			 * *:		proc pgrp == tty pgrp.
			 */
			 if (debug)
				printf("\t\t%d\t%s\n", pr[i].w_pid, pr[i].w_args);

			if(pr[i].w_start>curstart){
				curstart = pr[i].w_start;
				strcpy(doing, lflag ? pr[i].w_args : pr[i].w_comm);
			}
		}
		putline();
	}
}

/* figure out the major/minor device # pair for this tty */
static void
gettty(char *devstr)
{
	char ttybuf[20];
	struct stat statbuf;

	ttybuf[0] = 0;
	strcpy(ttybuf, "/dev/");
	strcat(ttybuf, devstr);
	stat(ttybuf, &statbuf);
	tty = statbuf.st_rdev;
	uid = statbuf.st_uid;
}

/*
 * putline: print out the accumulated line of info about one user.
 */
static void
putline(void)
{
	register int tm;
	int width = ttywidth - 1;

	/* print login name of the user */
	printf("%-*.*s ", NMAX, NMAX, utmp->ut_name);
	width -= NMAX + 1;

	/* print tty user is on */
	if (lflag && !prfrom) {
		/* long form: all (up to) LMAX chars */
		printf("%-*.*s", LMAX, LMAX, utmp->ut_line);
		width -= LMAX;
	} else {
		/* short form: 3 chars, skipping 'tty' if there */
		if (utmp->ut_line[0]=='t' && utmp->ut_line[1]=='t' && utmp->ut_line[2]=='y')
			printf("%-3.3s", &utmp->ut_line[3]);
		else
			printf("%-3.3s", utmp->ut_line);
		width -= 3;
	}

#ifdef UTMPX_FILE
	if (prfrom) {
		printf(" %-14.14s", utmpx ? utmpx->ut_host : " ");
		width -= 15;
	}
#endif

	if (lflag) {
		/* print when the user logged in */
		prtat(&utmp->ut_time);
		width -= 8;
	}

	/* print idle time */
	if (idle >= 36 * 60)
		printf(" %2ddays ", (idle + 12 * 60) / (24 * 60));
	else {
		printf(" ");
		prttime(idle," ");
	}
	width -= 8;

	if (lflag) {
		/* print CPU time for all processes & children */
		prttime(jobtime," ");
		width -= 7;
		/* print cpu time for interesting process */
		prttime(proctime," ");
		width -= 7;
	}

	/* what user is doing, either command tail or args */
        /* Check here if the output is being sent to screen. If so 
	   then print upto 80 characters. 
        */

	if (use_tty_flag == 0) {
          printf (" %s\n", doing);
	}
	else
	  printf(" %-.*s\n", width-1, doing);
#ifdef UTMPX_FILE
	if (wide_from && utmpx) {
		printf("%*.s %-s\n", NMAX, " ", utmpx->ut_host);
	}
#endif
	fflush(stdout);
}

/* find & return number of minutes current tty has been idle */
static time_t
findidle( char	*ttystr )
{
	struct stat stbuf;
	time_t lastaction, diff;
	char ttyname[20];

	strcpy(ttyname, "/dev/");
	strncat(ttyname, ttystr, LMAX);
	if (stat(ttyname, &stbuf) < 0)
	    return 0;
	time(&now);
	lastaction = stbuf.st_atime;
	diff = now - lastaction;
	diff = DIV60(diff);
	if (diff < 0) diff = 0;
	return(diff);
}

#define	HR	(60 * 60)
#define	DAY	(24 * HR)
#define	MON	(30 * DAY)

/*
 * prttime prints a time in hours and minutes or minutes and seconds.
 * The character string tail is printed at the end, obvious
 * strings to pass are "", " ", or "am".
 */
static void
prttime(time_t tim, char *tail)
{

	if (tim >= 60) {
		printf("%3d:", tim/60);
		tim %= 60;
		printf("%02d", tim);
	} else if (tim > 0)
		printf("    %2d", tim);
	else
		printf("      ");
	printf("%s", tail);
}

char *weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char *month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* prtat prints a 12 hour time given a pointer to a time of day */
static void
prtat(long *time)
{
	struct tm *p;
	register int hr, pm;

	p = localtime(time);
	hr = p->tm_hour;
	pm = (hr > 11);
	if (hr > 11)
		hr -= 12;
	if (hr == 0)
		hr = 12;
	if (now - *time <= 18 * HR)
		prttime(hr * 60 + p->tm_min, pm ? "pm" : "am");
	else if (now - *time <= 7 * DAY)
		printf(" %s%2d%s", weekday[p->tm_wday], hr, pm ? "pm" : "am");
	else
		printf(" %2d%s%02d", p->tm_mday, month[p->tm_mon], p->tm_year%100);
}

/*
 * readpr finds and reads in the array pr, containing the interesting
 * parts of a process
 */
static void
readpr(void)
{
	int procfd;
	char procpath[100];
	DIR *dirp;
	struct dirent *dentp;

	/*
	 * Determine which processes to print info about by searching
	 * the /proc/pinfo directory and looking at each process.
	*/
	if ((dirp = opendir(_PATH_PROCFSPI)) == NULL) {
		(void) fprintf(stderr,
			       "Cannot open /proc/pinfo directory :%s\n",
			       strerror(errno));
		exit(1);
	}

	pr = NULL;
	np = 0;		/* global define for the number of active process */

	/*
	 * Basic idea - open the /proc directory and read all the files
	 * use the file name and then open using the ioctl for proc(4)
	 * get information instead of using syssgi call and other "old"
	 * methods of getting information.
	 */
	while (dentp = readdir(dirp)) {

		if (dentp->d_name[0] == '.')            /* skip . and .. */
                        continue;
		
		(void) sprintf(procpath,"%s%s",_PATH_PROCFSPI,dentp->d_name);

		if((procfd = open(procpath,O_RDONLY)) == -1)
			continue;

		if (ioctl(procfd, PIOCPSINFO, (char *) &pinfo) == -1) {
			(void) close(procfd);
			continue;
		}

		if (ioctl(procfd, PIOCCRED, (char *) &pcred) == -1) {
			(void) close(procfd);
			continue;
		}

		if (pinfo.pr_ttydev == PRNODEV){
			(void) close(procfd);
			continue;
		}

		/* decide if it's an interesting process */
		if (pinfo.pr_state==0 || pinfo.pr_state==SZOMB || pinfo.pr_pgrp==0){
			(void) close(procfd);
			continue;
		}

		/* Check if is a valid pid */
		if (pinfo.pr_pid < 0 || pinfo.pr_pid >= MAXPID){
			(void) close(procfd);
			continue;
		}

		/* 
		 * Read from PIOCPSINFO
		 */
		pr = realloc(pr, (np + 1) * sizeof(*pr));
		pr[np].w_pid = pinfo.pr_pid;
		pr[np].w_tty = pinfo.pr_ttydev;
		pr[np].w_start = pinfo.pr_start.tv_sec;
		strcpy(pr[np].w_comm, pinfo.pr_fname);
		strncpy(pr[np].w_args, pinfo.pr_psargs, ARGWIDTH);
		pr[np].w_args[ARGWIDTH] = '\0';
		pr[np].w_time = pinfo.pr_time.tv_sec;
		pr[np].w_ctime = pinfo.pr_ctime.tv_sec;

		/*
		 * Read the credentials structure to get the real
		 * user id.
		 */
		pr[np].w_uid = pcred.pr_ruid;

		np++; 		/* increment for valid process information */
		(void) close(procfd);
	}
	(void) closedir(dirp);
}

static long
getnamelist(void)
{
	return(sysmp(MP_KERNADDR, MPKA_AVENRUN) & 0x7fffffff);
}
