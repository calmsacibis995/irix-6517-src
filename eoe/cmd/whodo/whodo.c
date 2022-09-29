/*	Copyright (c) 1993 UNIX System Laboratories, Inc.	*/
/*	(a wholly-owned subsidiary of Novell, Inc.).     	*/
/*	All Rights Reserved.                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF UNIX SYSTEM     	*/
/*	LABORATORIES, INC. (A WHOLLY-OWNED SUBSIDIARY OF NOVELL, INC.).	*/
/*	The copyright notice above does not evidence any actual or     	*/
/*	intended publication of such source code.                      	*/

/*		copyright	"%c%" 	*/

#ident	"$Revision: 1.19 $"

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

/***************************************************************************
 * Command: whodo
 * Inheritable Privileges: P_MACREAD
 *       Fixed Privileges: P_DACREAD
 * Notes:
 *
 ***************************************************************************/

#include   <stdio.h>
#include   <stdlib.h>
#include   <string.h>
#include   <errno.h>
#include   <sys/types.h>
#include   <fcntl.h>
#include   <time.h>
#include   <sys/errno.h>
#include   <sys/types.h>
#include   <sys/param.h>
#include   <sys/time.h>
#ifndef __sgi
#include   <priv.h>
#include   <mac.h>
#endif	/* ifndef __sgi */
#include   <deflt.h>
#include   <utmpx.h>
#include   <sys/utsname.h>
#include   <sys/stat.h>
#include   <dirent.h>
#include   <sys/proc.h>		/* for SZOMB */
#include   <sys/procfs.h>	/* /proc header file */
#include   <paths.h>

#define NMAX sizeof(ut->ut_name)
#ifndef __sgi
#define LMAX sizeof(ut->ut_line)
#define DIV60(t)	((t+30)/60)    /* x/60 rounded */
#endif	/* ifndef __sgi */

#define ENTRY   sizeof(struct psdata)
#define RET_ERR     (-1)
#define error(str)      fprintf(stderr, "%s: %s\n", arg0, str)

#define HSIZE		256		/* size of process hash table 	*/
#define PS_DATA		"/tmp/.ps_data/.ps_data"	/*  ps_data file built by ps command */
#define INITPROCESS	(pid_t)1	/* init process pid */
#define NONE		'n'		/* no state */
#define VISITED		'v'		/* marked node as visited */
#define ZOMBIE		'z'		/* zombie process */

#define DEFFILE "mac"	/* /etc/default file containing the default LTDB lvl */
#define LTDB_DEFAULT 2  /* default LID for non-readable LTDB */

/*
 * File /tmp/.ps_data/.ps_data is built by ps command; we only use the
 * only use 1st part (device info)
 */
struct psdata {
	char    device[MAXNAMLEN];	/* device name 		 */
	dev_t   dev;    		/* major/minor of device */
};
struct psdata   *psptr;
int 	ndevs;				/* number of configured devices */

struct uproc {
	pid_t	p_upid;			/* user process id */
	int	p_state;		/* proc state: none, zombie, visited */
	dev_t   p_ttyd;			/* controlling tty of process */
	time_t  p_time;			/* ticks of user & system time */
#ifndef __sgi
	time_t	p_ctime;		/* ticks of child user & system time */
	int	p_igintr;		/* 1=ignores SIGQUIT and SIGINT*/
#endif	/* ifndef __sgi */
	char    p_comm[PRARGSZ+1];	/* command */
	char    p_args[PRARGSZ+1];	/* command line arguments */
	struct uproc	*p_child,	/* first child pointer */
			*p_sibling,	/* sibling pointer */
			*p_pgrplink,	/* pgrp link */
			*p_link;	/* hash table chain pointer */
#ifndef __sgi
	level_t	p_lid;			/* MAC level identifier */
#endif	/* ifndef __sgi */
};

/*
 * Define hash table for struct uproc.
 * Hash function uses process id
 * and the size of the hash table(HSIZE)
 * to determine process index into the table. 
 */
struct uproc	pr_htbl[HSIZE];

struct 	uproc	*findhash(pid_t);
void		showproc(struct uproc *);
#ifndef __sgi
time_t  	findidle(char *);
int		clnarglist(char *);
void		calctotals(struct uproc *);
#endif	/* ifndef __sgi */

unsigned        size;
int             fd;
char            *arg0;
int		header = 1;	/* true if -h flag: don't print heading */
int		lflag = 0;	/* true if -l flag: w command format */
char *  	sel_user;	/* login of particular user selected */
time_t		now;		/* current time of day */
time_t  	uptime;		/* time of last reboot & elapsed time since */
int     	nusers;		/* number of users logged in now */
time_t  	idle; 		/* number of minutes user is idle */
time_t  jobtime;                /* total cpu time visible */

#ifndef __sgi
char    doing[520];             /* process attached to terminal */
time_t  proctime;               /* cpu time of process in doing */
int	curpid, empty;

char	*usage_str = "usage: whodo [ -hl[Z|z] ] [ user ]";

/*
 * print level information
 *	-1: don't print level
 *	LVL_ALIAS: print alias name, if it exists
 *	LVL_FULL: print fully qualified level name
 */
int	zflag;
int	Zflag;
int	lvlformat;
#define	SHOWLVL()	(lvlformat != -1)

level_t curr_lvl;	/* level of ls process when exec'ed by user */
level_t ltdb_lvl;	/* level of the LTDB */

#else
char	*usage_str = "usage: whodo [ -h ] [ user ]";
#endif	/* ifndef __sgi */


/*
 * Procedure:     main
 *
 * Restrictions:
 *                localtime:  none
 *                stat(2): P_MACREAD
 *                open(2): P_MACREAD
 *                read(2):  none
 *                ctime: none
 *                lvlfile(2): none
 *                lvlproc(2): none
 *                opendir: none
 */

main(argc, argv)
int argc;
char *argv[];
{
	register struct utmpx    *ut;
	struct utmpx		*utmpbegin;
	register struct tm      *tm;
	struct uproc    *up, *parent, *pgrp;
#ifndef __sgi
	struct psinfo		info;
	struct sigaction	act_i,act_q;
	struct pstatus		sinfo;
#else
	struct prpsinfo		info;
#endif
	unsigned        utmpend;
	struct stat     sbuf;
	struct utsname  uts;
	DIR		*dirp;
	struct	dirent  *dp;
#ifndef __sgi
	char 		pname[MAXNAMELEN],sname[MAXNAMELEN],aname[MAXNAMELEN];
	int		procfd,sfd,actfd;
#else
	char 		pname[MAXNAMELEN];
	int		procfd;
	int		pdlen;
#endif
	register 	int i;
	int             rc;
	int 		days, hrs, mins;
	long		nsec;
	int 		ret = 0;

#ifndef __sgi
	extern level_t  ltdb_lvl;
#endif

	arg0 = argv[0];
	
	while (argc > 1) {
		if (argv[1][0] == '-') {
			for (i=1; argv[1][i]; i++) {
			       switch (argv[1][i]) {

				case 'h':
					header = 0;
					break;

#ifndef __sgi
				case 'l':
					lflag++;
					break;

				case 'z':
					zflag++;
					break;

				case 'Z':
					Zflag++;
					break;
#endif

				default:
					fprintf(stderr, "%s\n", usage_str);
					exit(1);
				}
			}
		} else {
			if (!isalnum(argv[1][0]) || argc > 2) {
				printf("usage: %s [ -hl ] [ user ]\n", arg0);
				exit(1);
			} else
				sel_user = argv[1];
		}
		argc--; argv++;
	}      

#ifndef __sgi
	/*
	 * The z and Z options are mutually exclusive.
	 */
	if (zflag && Zflag) {
		fprintf(stderr,
		"UX:whodo: ERROR: invalid combination of options -Z & -z\n");
		fprintf(stderr, "%s\n", usage_str);
		exit(1);
	}

	if (zflag)
		lvlformat = LVL_ALIAS;
	else if (Zflag)
		lvlformat = LVL_FULL;
	else
		lvlformat = -1;

	/*
	 *  Read UTMPX_FILE which contains the information about
	 *  each login's users.
	 */
	procprivl(CLRPRV, MACREAD_W, 0);
#endif	/* ifndef __sgi */

	if (stat(UTMPX_FILE, &sbuf) == RET_ERR) {
		fprintf(stderr, "%s: stat error of %s: %s\n",
			arg0, UTMPX_FILE, strerror(errno));
		exit(1);
	}
	size = (unsigned)sbuf.st_size;
	if ((ut = (struct utmpx *)malloc(size)) == NULL) {
		fprintf(stderr, "%s: malloc error of %s: %s\n",
			arg0, UTMPX_FILE, strerror(errno));
		exit(1);
	}
	if ((fd = open(UTMPX_FILE, O_RDONLY)) == RET_ERR) {
		fprintf(stderr, "%s: open error of %s: %s\n",
			arg0, UTMPX_FILE, strerror(errno));
		exit(1);
	}

#ifndef __sgi
	procprivl(SETPRV, MACREAD_W, 0);
#endif	/* ifndef __sgi */

	if (read(fd, (char *)ut, size) == RET_ERR) {
		fprintf(stderr, "%s: read error of %s: %s\n",
			arg0, UTMPX_FILE, strerror(errno));
		exit(1);
	}
	utmpbegin = ut;			/* ptr to start of utmp data*/
	utmpend = (unsigned)ut + size;  /* ptr to end of utmp data */
	close(fd);

	time(&now);	/* get current time */

	if (header) {	/* print a header */
#ifndef __sgi
		if (lflag) {	/* w command format header */
			prtat(&now);
			for (ut = utmpbegin; ut < (struct utmpx *)utmpend; ut++){

				if (ut->ut_type == USER_PROCESS) {
					nusers++;
				} else if (ut->ut_type == BOOT_TIME) {
					uptime = now - ut->ut_time;
					uptime += 30;
					days = uptime / (60*60*24);
					uptime %= (60*60*24);
					hrs = uptime / (60*60);
					uptime %= (60*60);
					mins = uptime / 60;

					printf("  up");
					if (days > 0)
						printf(" %d day%s,",
						  days, days > 1 ? "s" : "");
					if (hrs > 0 && mins > 0) {
						printf(" %2d:%02d,", hrs, mins);
					} else {
						if (hrs > 0)
							printf(" %d hr%s,", hrs,
							  hrs > 1 ? "s" : "");
						if (mins > 0)
							printf(" %d min%s,", 
							  mins,
							  mins > 1 ? "s" : "");

					}
				}
			}
			
			ut = utmpbegin;	/* rewind utmp data */
			printf("  %d user%s\n", nusers, nusers > 1 ? "s" : "");
			printf("User     tty           login@  idle   JCPU   PCPU  what\n");
		} else
#endif	/* ifndef __sgi */
		{	/* standard whodo header */

			/*
			 * Print current time and date, and system name.
			 */
			printf("%s", ctime(&now));
			uname(&uts);
			printf("%s\n", uts.nodename);

		}
	}

	/*
	 * Read in device info from PS_DATA file.
	 */

#ifndef __sgi
	procprivl(CLRPRV, MACREAD_W, 0);
#else
        /**
         * read in device info from ps_data file
         **/
	/* this used to just invoke "ps", which allowed a bad guy to
	 * get an arbitrary pgm running with GID 0 (sys)
	 * Revert to real gid before invoking /sbin/ps, then back to
	 * sys to open /dev/kmem.
	 */
	 /* Note that since whodo is no longer a setgid program,
	  * fiddling with the gid is no longer necessary - so
	  * can just invoke ps without worrying about gids - 5/26/95
	  */
	(void)system("/sbin/ps >/dev/null 2>&1"); /* ensure ps_data exists */
#endif	/* ifndef __sgi */
	
	if ((fd = open(PS_DATA, O_RDONLY)) == RET_ERR) {
		fprintf(stderr, "%s: open error of %s: %s\n",
			arg0, PS_DATA, strerror(errno));
		exit(1);
	}

#ifndef __sgi
	procprivl(SETPRV, MACREAD_W, 0);
#endif	/* ifndef __sgi */

	/* First int tells how many entries follow. */
	if (read(fd, (char *)&ndevs, sizeof(ndevs)) == RET_ERR) {
		fprintf(stderr,
		  "%s: read error of size of device table info: %s\n",
		  arg0, strerror(errno));
		exit(1);
	}
	/*
	 * Allocate memory and read in device table from PS_DATA file.
	 */
	if ((psptr = (struct psdata *)malloc(ndevs*ENTRY)) == NULL) {
		fprintf(stderr, "%s: malloc error of %s device table: %s\n",
			arg0, PS_DATA, strerror(errno));
		exit(1);
	}
	if (read(fd, (char *)psptr, ndevs*ENTRY) == RET_ERR) {
		fprintf(stderr, "%s: read error of %s device info: %s\n",
			arg0, PS_DATA, strerror(errno));
		exit(1);
	}
	close(fd);

#ifndef __sgi
	/*
	 * establish up front if the enhanced security package was installed 
         * the routine read_ltdb will:
	 *
	 * - Check to make sure MAC is installed
	 * - Attempt to read the LTDB at the current process level
	 * - If unsuccessful, change level to the level of the LTDB abd
	 *   attempt to read the LTDB again.
	 *   A return of -1 means that MAC is not installed.
	 *   A return of >0 means that the LTDB was not readable at the
	 *     current process level BUT the process was able to change
	 *     level to the level of the LTDB and read it.  The return
	 *     value is the lid that was changed to.
	 *   A return value of 0 means that MAC is installed, and no level
	 *     change was done.  This does not necessarily mean the LTDB
	 *     was readable.  If the LTDB was unreadable, we may not care
	 *     in some circumstances (for example, if we're doing a whodo
	 *     on a user who we're not permitted to see).  Wait for the
	 *     lvlout to fail to print an error.
	 */

	if (SHOWLVL()) {
		if ((ret = read_ltdb ()) < 0) 
		{
			fprintf(stderr,
"UX:whodo: ERROR: illegal option\nUX:whodo: ERROR: system service not installed\n");
			fprintf(stderr, "%s\n", usage_str);
			exit(1);
		} else
			ltdb_lvl = ret;
	}
#endif	/* ifndef __sgi */

	/*
	 * Loop through /proc, reading info about each process
	 * and building the parent/child tree.
	 */
	if (!(dirp = opendir(_PATH_PROCFSPI))) {
		fprintf(stderr, "%s: could not open %s: %s\n",
			arg0, _PATH_PROCFSPI, strerror(errno));
		exit(1);
	}

#ifdef __sgi
	(void) sprintf(pname, "%s%c", _PATH_PROCFSPI, '/');
	pdlen = strlen(pname);
#endif	/* __sgi */

	while (dp = readdir(dirp)) {
#ifndef __sgi
retry:
		if (dp->d_name[0] == '.')
			continue;

		sprintf(pname, "%s/%s/psinfo", PROCDIR, dp->d_name);
		if ((procfd = open(pname, O_RDONLY)) == -1) {
			continue;
		}
		if ((rc = read(procfd,&info,sizeof(info))) != sizeof(info)){
		        int saverr = errno;

			(void) close(procfd);

			if (rc != -1)
			        fprintf(stderr,"whodo: Unexpected return value %d on %s\n",rc, pname);
			else if (saverr == EACCES)
			        continue;
			else if (saverr == EAGAIN) 
			        goto retry;
			else if (saverr != ENOENT) 
			        fprintf(stderr, "whodo: pinfo on %s: %s \n",
				pname, strerror(errno));
			continue;
		}

		sprintf(sname, "%s/%s/status", PROCDIR, dp->d_name);
		if ((sfd = open(sname, O_RDONLY)) == -1)
			continue;

		if((rc=read(sfd,&sinfo,sizeof(pstatus_t)))!=sizeof(pstatus_t)){
		        int saverr = errno;

			(void) close(procfd);
			(void) close(sfd);

			if (rc != -1)
			        fprintf(stderr,"whodo: Unexpected return value %d on %s\n",rc, sname);
			else if (saverr == EACCES)
			        continue;
			else if (saverr == EAGAIN) 
			        goto retry;
			else if (saverr != ENOENT) 
			        fprintf(stderr, "whodo: pinfo on %s: %s \n",
				sname, strerror(errno));
			continue;
		}
#else	/* __sgi */
		if (dp->d_name[0] == '.')		/* skip . and .. */
			continue;
		(void) strcpy(pname + pdlen, dp->d_name);
retry:
		if ((procfd = open(pname, O_RDONLY)) == -1)
			continue;

		/*
		 * Get the info structure for the process and close quickly.
		 */
		if (ioctl(procfd, PIOCPSINFO, (char *) &info) == -1) {
			int	saverr = errno;

			(void) close(procfd);
			if (saverr == EACCES)
				continue;
			if (saverr == EAGAIN)
				goto retry;
			if (saverr != ENOENT)
				fprintf(stderr, "PIOCPSINFO on %s: %s",
					pname, strerror(saverr));
			continue;
		}

		(void) close(procfd);

#endif	/* ifndef __sgi */

		up = findhash(info.pr_pid);
		up->p_ttyd = info.pr_ttydev;
#ifndef __sgi
		up->p_state = (info.pr_nlwp == 0) ? ZOMBIE : NONE;
#else
		up->p_state = (info.pr_state == SZOMB) ? ZOMBIE : NONE;
#endif
		strncpy(up->p_comm, info.pr_fname, sizeof(info.pr_fname));

	        /*
		 * Compute times, rounding nanoseconds to seconds
		 * while avoiding overflow.
		 */
		up->p_time = info.pr_time.tv_sec;
		nsec = info.pr_time.tv_nsec;
		if (nsec >= 1500000000)
			up->p_time += 2;
		else if (nsec >= 500000000)
			up->p_time++;

#ifndef __sgi
		up->p_ctime=sinfo.pr_cutime.tv_sec + sinfo.pr_cstime.tv_sec;
		nsec = sinfo.pr_cutime.tv_nsec + sinfo.pr_cstime.tv_nsec;
		if (nsec >= 1500000000)
			up->p_ctime += 2;
		else if (nsec >= 500000000)
			up->p_ctime++;

		sprintf(aname,"%s/%s/sigact", PROCDIR, dp->d_name);
		if ((actfd = open(aname, O_RDONLY)) == -1) 
			continue;
		if(lseek(actfd,sizeof(struct sigaction)*(SIGINT-1),0) == -1 ||
		   read(actfd,&act_i,sizeof(act_i))!=sizeof(act_i) || 
		   lseek(actfd,sizeof(struct sigaction)*(SIGQUIT-1),0 )== -1 ||
		   read(actfd,&act_q,sizeof(act_q))!=sizeof(act_q)) {
		        close(actfd);
		        close(procfd);
			close(sfd);
			continue;
		}
			
		up->p_igintr = (act_i.sa_handler == SIG_IGN) &&
		               (act_q.sa_handler == SIG_IGN);
#endif	/* ifndef __sgi */

		up->p_args[0] = 0;

#ifndef __sgi
		/*
		 * Get the level of a process.  If this fails,
		 * don't print level information.
		 * Not necessary for the -l option.
		 */
		if (!lflag && SHOWLVL()) {
			if (lvlfile(pname, MAC_GET, &up->p_lid) == -1)
				up->p_lid = (level_t)0;
		}

		/*
		 * Process args if there's a chance we'll print it.
		 */
		if (lflag) { /* w command needs args */
			clnarglist(info.pr_psargs);
			strcpy(up->p_args, info.pr_psargs);
			if (up->p_args[0] == 0 ||
			up->p_args[0] == '-' && up->p_args[1] <= ' ' ||
			up->p_args[0] == '?') {
				strcat(up->p_args, " (");
				strcat(up->p_args, up->p_comm);
				strcat(up->p_args, ")");
			}
		}
#endif	/* ifndef __sgi */

		/*
		 * Link pgrp together in case parents go away 
		 * Pgrp chain is a single linked list originating
		 * from the pgrp leader to its group member. 
		 */
		if (info.pr_pgrp != info.pr_pid) {	/* not pgrp leader */
			pgrp = findhash(info.pr_pgrp);
			up->p_pgrplink = pgrp->p_pgrplink;
			pgrp->p_pgrplink = up;
		}
		parent = findhash(info.pr_ppid);

		/* if this is the new member, link it in */
		if (parent->p_upid != INITPROCESS) {
			if (parent->p_child) {
				up->p_sibling = parent->p_child;
				up->p_child = 0;
			}
			parent->p_child = up;
		}
		
#ifndef __sgi		/* Note that for __sgi, procfd was closed above */
		close(procfd); 
		close(sfd); 
		close(actfd); 
#endif	/* ifndef __sgi */
	}  	/* end while (dp=readdir(dirp)) */

	closedir(dirp);

	/*
	 * Loop through utmp file, printing process info
	 * about each logged-in user.
	 */
	for (; ut < (struct utmpx *)utmpend; ut++) {
		if (ut->ut_type != USER_PROCESS)
			continue;
		if (sel_user && strncmp(ut->ut_name, sel_user, NMAX) !=0)
			continue;	/* we're looking for somebody else */
		tm = localtime(&ut->ut_xtime);
#ifndef __sgi
		if (lflag) {	/* -l flag format (w command) */
			/* print login name of the user */
			printf("%-*.*s ", NMAX, NMAX, ut->ut_name);

			/* print tty user is on */
			printf("%-*.*s", LMAX, LMAX, ut->ut_line);
			
			/* print when the user logged in */
			prtat(&ut->ut_time);

			/* print idle time */
			idle = findidle(ut->ut_line);
			if (idle >= 36 * 60)
				printf("%2ddays ", (idle + 12 * 60) / (24 * 60));
			else
				prttime(idle, " ");	
			showtotals(findhash((pid_t)ut->ut_pid));

		} else
#endif	/* ifndef __sgi */
		{	/* standard whodo format */
			printf("\n%-12.12s %-8.8s %2.1d:%2.2d\n",
			    ut->ut_line, ut->ut_name, tm->tm_hour, tm->tm_min);
			showproc(findhash((pid_t)ut->ut_pid));
		}
	}
		
	exit(0);
}

#ifndef __sgi
/*
 * Procedure:     read_ltdb
 *
 * Restrictions:
 *		lvlin: none
 *		lvlproc: none
 *		defopen: none
 *	        defclose: none
 *		defread: none
 *
 *
 * Inputs: None
 * Returns: 0 if MAC is installed and there's no proc LID change,
 *          > 0 if the LTDB is at a level which is not dominated by this
 *          process, but was readable after lvlproc () call
 *          and -1 if MAC is not installed. 
 *          Levels of LTDB and the current process
 *	    are set here. These are used later when the per-file LIDs are
 *	    translated into text levels.
 *
 *         
 *
 * Notes: Attempt to read the LTDB at the current process level, if the
 *        call to lvlin fails, get the level of the LTDB (via defread),
 *        change the process level to that of the LTDB and try again.
 *   
 *        This routine does not return failure if the LTDB was unreadable.
 *        This is because, in rare circumstances, we may not actually have
 *        to access the LTDB.  So we wait for a lvlout to fail during
 *        actuall processing before printing an error.
 *
 */
read_ltdb ()
{
	FILE    *def_fp;
	static char *ltdb_deflvl;
	level_t test_lid, ltdb_lid;

/*
 *	Check to see if MAC is installed. If lvlproc fails with ENOPKG
 *	MAC is not installed ...
 */

	if ((lvlproc(MAC_GET, &test_lid) != 0) && (errno == ENOPKG)) 
		return (-1);

/*
 *	If lvlin is successful the LTDB is readable at the current process
 *	level, no other processing is required, return 0....
 */

	if (lvlin ("SYS_PRIVATE", &test_lid) == 0)  
		return (0);
/*
 *	If the LTDB was unreadable:
 *	- Get the LTDB's level (via defread)
 *	- Get the current level of the process
 *	- Set p_setplevel privilege
 *	- Change process level to the LTDB's level
 *	- Try lvlin again, read of LTDB should succeed since process level
 *        is equal to level of the LTDB
 *
 *	Failure to obtain the process level or read the LTDB is not a fatal
 *      error, return 0.
 */


	if ((def_fp = defopen(DEFFILE)) != NULL) {
		if (ltdb_deflvl = defread(def_fp, "LTDB"))
			ltdb_lid = (level_t) atol (ltdb_deflvl);
		(void) defclose(NULL);
	}
	
	if ((def_fp == NULL) || !(ltdb_deflvl))
		ltdb_lid = (level_t) LTDB_DEFAULT;


	if (ltdb_lid <= 0)
		return (0);

	if (lvlproc (MAC_GET, &curr_lvl) != 0)
		return (0);

	procprivl (SETPRV, SETPLEVEL_W, 0);
	lvlproc (MAC_SET, &ltdb_lid);


	if (lvlin ("SYS_PRIVATE", &test_lid) != 0) 
		return (0);


	if (lvlproc (MAC_SET, &curr_lvl) != 0)
		return (0);

	(void) procprivl (CLRPRV, SETPLEVEL_W, 0);

	return (ltdb_lid);

}
#endif	/* ifndef __sgi */

/*
 * Procedure:     showproc
 *
 * Restrictions:
 *		  lvlproc: none
 *                lvlout: P_MACREAD
 * Notes:
 *
 *  Used for standard whodo format. 
 *  This is the recursive routine descending the process
 *  tree starting from the given process pointer(up).
 *  It used depth-first search strategy and also marked
 *  each node as printed as it traversed down the tree.
 *  
 ***************************************************/
void
showproc(register struct uproc	*up)
{
	struct	uproc	*zp;
	char            *getty(dev_t);

	/* print the data for this process */
	if (up->p_state == VISITED)	/* we've already been here */
		return;
	else if (up->p_state == ZOMBIE)
		printf("    %-12.12s %5ld %4.1ld:%2.2ld %s",
			"  ?", up->p_upid, 0L, 0L, "<defunct>");
	else 
		printf("    %-12.12s %5ld %4.1ld:%2.2ld %s",
			getty(up->p_ttyd), up->p_upid,
			up->p_time/60L, up->p_time%60L,
			up->p_comm);


#ifndef __sgi
#define	INITBUFSIZ	512

	if ( up->p_state == NONE && SHOWLVL()) {
		/*
		 * Pre-allocate buffer to store level name.  This should
		 * save a lvlout call in most cases.  Double size
		 * dynamically if level won't fit in buffer.
		 */
		static char	lvl_buf[INITBUFSIZ];
		static char	*lvl_name = lvl_buf;
		static int	lvl_namesz = INITBUFSIZ;
		int		wcnt = strlen(up->p_comm);

		procprivl(CLRPRV, MACREAD_W, 0);

		/*
		 *
		 * if ltdb_lvl is > 0 then the LTDB is at a level which
                 *    is unreadable by this process at its current level,
                 *    change level to the level of the LTDB and issue lvlout
                 */

		if (ltdb_lvl > 0) {
			(void) procprivl(SETPRV, SETPLEVEL_W, 0);
			(void) lvlproc(MAC_SET, &ltdb_lvl);
		}

		while (lvlout(&up->p_lid, lvl_name, lvl_namesz, lvlformat)
		      == -1) {
			if ((lvlformat == LVL_FULL) && (errno == ENOSPC)) {
				char *tmp_name;
				if ((tmp_name = malloc(lvl_namesz*2))
				==  (char *)NULL) {
					fprintf(stderr,
		"UX:whodo: ERROR: no memory to print level for pid %u\n",
						up->p_upid);
					goto out;
				}
				lvl_namesz *= 2;
				if (lvl_name != lvl_buf)
					free(lvl_name);
				lvl_name = tmp_name;
			} else {
				fprintf(stderr,
	"UX:whodo: ERROR: cannot translate level to text format for pid %u\n",
					up->p_upid);
				goto out;
			}
		}


		/* align if command name is less than 8 chars wide */
		while (wcnt < 8) {
			putc(' ', stdout);
			wcnt++;
		}
		printf("  %s", lvl_name);
	}
out:

/*
 *	Reset process to it's correct level & drop p_setplevel
 */

	if (ltdb_lvl > 0) {
		(void) lvlproc(MAC_SET, &curr_lvl);
		(void)procprivl(CLRPRV, SETPLEVEL_W, 0);
	
	}

	procprivl(SETPRV, MACREAD_W, 0);
#endif	/* ifndef __sgi */
	printf("\n");
	up->p_state = VISITED;

	/* descend for its children */
	if (up->p_child) {
		showproc(up->p_child);
		for(zp = up->p_child->p_sibling; zp; zp = zp->p_sibling) {
			showproc(zp);
		}  /* end for */
	}

	/* print the pgrp relation */
	if (up->p_pgrplink)
		showproc(up->p_pgrplink);
}



#ifndef __sgi
/*
 * Procedure:     showtotals
 *
 * Restrictions:  none
 *
 * Notes:
 *
 *  Used for -l flag (w command) format.
 *  Prints the CPU time for all processes & children,
 *  and the cpu time for interesting process,
 *  and what the user is doing.
 *
 **************************************************/
showtotals(up)
register struct uproc	*up;
{
	jobtime = 0;
	proctime = 0;
	empty = 1;
	curpid = -1;
	strcpy(doing, "-");     /* default act: normally never prints */
	calctotals(up);

	/* print CPU time for all processes & children */
	prttime((time_t)jobtime, " ");	

	/* print cpu time for interesting process */
	prttime((time_t)proctime, " ");

	/* what user is doing, current process */
	printf(" %-.32s\n", doing);
}


/*
 * Procedure:     calctotals
 *
 * Restrictions:  none
 *
 * Notes:
 *
 * Used for -l flag (w command) format.
 * This recursive routine descends the process
 * tree starting from the given process pointer(up).
 * It uses depth-first search strategy and also marks
 * each node as visited as it traverses down the tree.
 * It calculates the process time for all processes &
 * children.  It also finds the "interesting" process
 * and determines its cpu time and command.
 *
 */
void 
calctotals(register struct uproc *up)
{
	register struct uproc   *zp;

	if (up->p_state != NONE)
		return;
	jobtime += up->p_time + up->p_ctime;
	proctime += up->p_time;

	if (empty && !up->p_igintr) {
		empty = 0;
		curpid = -1;
	}

	if (up->p_upid > curpid && (!up->p_igintr || empty)) {
		curpid = up->p_upid;
		strcpy(doing, up->p_args);
	}

	/* descend for its children */
	if (up->p_child) {
		calctotals(up->p_child);
		for(zp = up->p_child->p_sibling; zp; zp = zp->p_sibling) {
			calctotals(zp);
		}  /* end for */
	}
}
#endif	/* ifndef __sgi */


/*
 * Procedure:     getty
 *
 * Restrictions:  none
 *
 * Notes:
 *
 *   This routine gives back a corresponding device name
 *   from the device number given. 
 ************************************************************/

char *
getty(register dev_t dev)
{
	register struct psdata *ps, *ps_end;
	
	ps_end = &psptr[ndevs];
	for(ps = psptr; ps < ps_end; ps++) {
		if (ps->dev == dev)
			return(ps->device);
	}
	return("  ?  ");
}


/*
 * Procedure:     findhash
 *
 * Restrictions:  none
 *
 * Notes:
 *
 *   Findhash  finds the appropriate entry in the process
 *   hash table (pr_htbl) for the given pid in case that
 *   pid exists on the hash chain. It returns back a pointer
 *   to that uproc structure. If this is a new pid, it allocates
 *   a new node, initializes it, links it into the chain (after
 *   head) and returns a structure pointer.
 *
 ************************************************************/

struct uproc *
findhash(pid_t pid)
{
	register struct uproc *up, *tp;

	tp = up = &pr_htbl[(int)pid % HSIZE];
	if (up->p_upid == 0) {			/* empty slot */
		up->p_upid = pid;
		up->p_state = NONE;
		up->p_child = up->p_sibling = up->p_pgrplink = up->p_link = 0;
		return(up);
	}
	if (up->p_upid == pid) {			/* found in hash table */
		return(up);
	}
	for( tp = up->p_link; tp; tp = tp->p_link ) {	/* follow chain */
		if (tp->p_upid == pid) {
			return(tp);
		}
	}
	tp = (struct uproc *)malloc(sizeof(*tp));	/* add new node */
	if (!tp) {
		fprintf(stderr, "%s: out of memory!: %s\n",    
			arg0, strerror(errno));
		exit(1);
	}
	tp->p_upid = pid;
	tp->p_state = NONE;
	tp->p_child = tp->p_sibling = tp->p_pgrplink = (pid_t)0;
	tp->p_link = up->p_link;		/* insert after head */
	up->p_link = tp;
	return(tp);
}

#define	HR	(60 * 60)
#define	DAY	(24 * HR)
#define	MON	(30 * DAY)


#ifndef __sgi
/*
 * Procedure:     prttime
 *
 * Restrictions: none
 *
 * Notes:
 *
 * prttime(tim, tail)
 * prints a time in hours and minutes or minutes and seconds.
 * The character string tail is printed at the end, obvious
 * strings to pass are "", " ", or "am".
 */

prttime(tim, tail)
	time_t tim;
	char *tail;
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

 

/*
 * Procedure:     prtat
 *
 * Restrictions:
 *                localtime: none
 * Notes:
 *
 *	prints a 12 hour time given a pointer to a time of day 
 */

prtat(time)
	time_t *time;
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
		prttime(((time_t)(hr * 60 + p->tm_min)), pm ? "pm" : "am");
	else if (now - *time <= 7 * DAY)
		printf(" %s%2d%s", weekday[p->tm_wday], hr, pm ? "pm" : "am");
	else
		printf(" %2d%s%02d", p->tm_mday, month[p->tm_mon], p->tm_year%100);
}


/*
 * Procedure:     findidle
 *
 * Restrictions:
 *                stat(2):  none
 * Notes:
 *
 * 	find & return number of minutes current tty has been idle 
 */

time_t
findidle(char *devname)
{
	struct stat stbuf;
	time_t lastaction, diff;
	char ttyname[20];

	strcpy(ttyname, "/dev/");
	strcat(ttyname, devname);
	if (stat(ttyname, &stbuf) == -1)
		return (0);
	time(&now);
	lastaction = stbuf.st_atime;
	diff = now - lastaction;
	diff = DIV60(diff);
	if (diff < 0) diff = 0;
	return(diff);
}



/*
 * Procedure:     clnarglist
 *
 * Restrictions:  none
 *
 * Notes:
 *
 * clnarglist: given pointer to the argument string clean out
 * "unsavory" characters.
 */

clnarglist(char *arglist)
{
	register char	*c;
	register int 	err = 0;

	/* get rid of unsavory characters */
	for (c = arglist;*c == NULL; c++) {
		if ((*c < ' ') || (*c > 0176)) {
			if (err++ > 5) {
				*arglist = NULL;
				break;
			}
			*c = '?';
		}
	}
}
#endif	/* ifndef __sgi */
