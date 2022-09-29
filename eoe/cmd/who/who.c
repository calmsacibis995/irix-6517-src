/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.19 $"
/***************************************************************************
 * Command: who
 *
 * Inheritable Privileges: None
 *       Fixed Privileges: None
 *
 *
 * Notes:	This program analyzes information found in /var/adm/utmpx.
 *
 *		Additionally information is gathered from /etc/inittab
 *		if requested.
 *	
 *	
 *		Syntax:
 *	
 *			who am i	Displays info on yourself
 *	
 *			who -a		Displays information about All 
 *					entries in /var/adm/utmpx
 *	
 *			who -A		Displays ACCOUNTING information
 *					(non-functional)
 *	
 *			who -b		Displays info on last boot
 *	
 *			who -d		Displays info on DEAD PROCESSES
 *	
 *			who -H		Displays HEADERS for output
 *	
 *			who -l 		Displays info on LOGIN entries
 *	
 *			who -m 		Displays only info about current 
 *					terminal
 *					
 *			who -p 		Displays info on PROCESSES spawned
 *					by init
 *	
 *			who -q		Displays short information on
 *					current users who LOGGED ON
 *	
 *			who -r		Displays info of current run-level
 *	
 *			who -s		Displays requested info in SHORT form
 *	
 *			who -t		Displays info on TIME changes
 *	
 *			who -T		Displays writeability of each user
 *					(+ writeable, - non-writeable, ? hung)
 *	
 *			who -u		Displays LONG info on users
 *					who have LOGGED ON
 *
 ***************************************************************************/

#define		DATE_FMT	"%b %e %H:%M"
#define		DATE_FMTID	":734"
/*
 *  %b	Abbreviated month name
 *  %e	Day of month
 *  %H	hour (24-hour clock)
 *  %M  minute
 */
#include	<errno.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<string.h>
#include	<sys/types.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<sys/stat.h>
#include	<time.h>
#include	<utmpx.h>
#include	<locale.h>
#include	<pfmt.h>
#ifdef __sgi
#include	<paths.h>
#endif /* __sgi */

char	comment[80];	/* holds inittab comment	*/
char	*cptr;		/* general purpose char ptr	*/
int	fildes;		/* file descriptor for inittab	*/
int	filesw = 0;	/* 1 = Alternate file used	*/
int	Hopt = 0;	/* 1 = who -H			*/
char	*inittab;	/* ptr to inittab contents	*/
char	*iinit;		/* index into inittab		*/
int	justme = 0;	/* 1 = who am i			*/
int	lopt = 0;	/* 1 = who -l			*/
long	*lptr;		/* general purpose long ptr	*/
int	mopt = 0;	/* 1 = who -m			*/
char	*myname;	/* pointer to invoker's name 	*/
char	*mytty;		/* holds device user is on	*/
char	nameval[8];	/* holds invoker's name		*/
int	number = 8;	/* number of users per -q line	*/
extern	char *optarg;	/* for getopt()			*/
int	optcnt=0;	/* keeps count of options	*/
extern	int optind;	/* for getopt()			*/
char	outbuf[BUFSIZ];	/* buffer for output		*/	
char	*program;	/* holds name of this program	*/
int	qopt = 0;	/* 1 = who -q			*/
int	sopt = 0;	/* 1 = who -s 	       		*/
struct	stat stbuf;	/* area for stat buffer		*/
struct	stat *stbufp;	/* ptr to structure		*/
extern	char *sys_errlist[]; /* error msgs for errno    */
int	terse = 1;	/* 1 = print terse msgs		*/
int	Topt = 0;	/* 1 = who -T			*/
time_t	timnow;		/* holds current time		*/
char	timeval[40];	/* holds time of login?		*/
int	totlusrs = 0;	/* cntr for users on system	*/
int	uopt = 0;	/* 1 = who -u			*/
char	user[80];	/* holds user name		*/
struct	utmpx *utmpp;	/* pointer for getutxent()	*/
int	validtype[UTMAXTYPE+1];	/* holds valid types	*/
int	wrap;		/* flag to indicate wrap	*/
char	time_buf[40];	/* holds date and time string	*/

void process (void);
void ck_file(char *);

static const char
#ifdef i386
	using_init_base[] = ":888:Using /etc/conf/cf.d/init.base instead\n\n",
#endif
	badstat[] = ":5:Cannot access %s: %s\n",
	badopen[] = ":4:Cannot open %s: %s\n",
	badread[] = ":341:Read error in %s: %s\n";

main(argc, argv)
int	argc;
char	**argv;
{
char	c;
int	goerr = 0;	/* non-zero indicates cmd error	*/
int	i;
int	optsw;		/* switch for while of getopt()	*/
int	rc;
#ifdef i386
int	initbase = 0;
#endif /* i386 */

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxcore.abi");
	(void)setlabel("UX:who");

	validtype[USER_PROCESS] = 1;
	validtype[EMPTY] = 0;
	stbufp = &stbuf;

	/*
		Strip off path name of this command
	*/
	for (i = strlen(argv[0]); i >= 0 && (c = argv[0][i]) != '/'; --i);
	if (i >= 0) argv[0] += i+1;
	program = argv[0];

	/*
		Buffer stdout for speed
	*/
	setbuf(stdout, outbuf);

	cptr = timeval;	
	
	/*
		Retrieve options specified on command line
	*/
	while ((optsw = getopt(argc, argv, "abdHlmn:pqrstTu")) != EOF) {
		optcnt++;
		switch(optsw) {
	
			case 'a':
				optcnt += 7;
				validtype[ACCOUNTING] = 1;
				validtype[BOOT_TIME] = 1;
				validtype[DEAD_PROCESS] = 1;
				validtype[LOGIN_PROCESS] = 1;
				validtype[INIT_PROCESS] = 1;
				validtype[RUN_LVL] = 1;
				validtype[OLD_TIME] = 1;
				validtype[NEW_TIME] = 1;
				validtype[USER_PROCESS] = 1;
				Topt = 1;
				Hopt = 1;
				if (!sopt) terse = 0;
				break;
	
			case 'A':
				validtype[ACCOUNTING] = 1;
				terse = 0;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;
	
			case 'b':
				validtype[BOOT_TIME] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;
	
			case 'd':
				validtype[DEAD_PROCESS] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
				terse = 0;
				break;
	
			case 'H':
				optcnt--; /* Don't count Header */
				Hopt = 1;
				break;
	
			case 'l':
				validtype[LOGIN_PROCESS] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
				terse = 0;
				lopt = 1;
				break;

			case 'm':
				validtype[USER_PROCESS] = 1;
				terse = 1;
				mopt = 1;
				break;

			case 'n':
				number = atoi(optarg);
				if (number < 1) {
					pfmt(stderr, MM_ERROR,
						":735:Number of users per line must be at least 1\n");
					exit(1);
				}
				break;

			case 'p':
				validtype[INIT_PROCESS] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;
	
			case 'q':
				qopt = 1;
				break;
	
			case 'r':
				validtype[RUN_LVL] = 1;
				terse = 0;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;
	
			case 's':
				sopt = 1;
				terse = 1;
				break;
	
			case 't':
				validtype[OLD_TIME] = 1;
				validtype[NEW_TIME] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;
	
			case 'T':
				Topt = 1;
				/*FALLTHRU*/

			case 'u':
				uopt = 1;
				validtype[USER_PROCESS] = 1;
				if (!sopt) terse = 0;
				break;
	
			case '?':
				goerr++;
				break;
		}
	}
	
	if (goerr > 0) {
		pfmt(stderr, MM_ACTION, ":889:Usage:\t%s [-abdHlnpqrstTu] [am i] [utmp_like_file]\n", program);
		pfmt(stderr, MM_NOSTD, ":737:a\tall (Abdlprtu options)\n");
		pfmt(stderr, MM_NOSTD, ":738:b\tboot time\n");
		pfmt(stderr, MM_NOSTD, ":739:d\tdead processes\n");
		pfmt(stderr, MM_NOSTD, ":740:H\tprint header\n");
		pfmt(stderr, MM_NOSTD, ":741:l\tlogin processes\n");
		pfmt(stderr, MM_NOSTD, ":742:n #\tspecify number of users per line for -q\n");
		pfmt(stderr, MM_NOSTD, ":743:p\tprocesses other than getty or users\n");
		pfmt(stderr, MM_NOSTD, ":744:q\tquick %s\n", program);
		pfmt(stderr, MM_NOSTD, ":745:r\trun level\n");
		pfmt(stderr, MM_NOSTD, ":746:s\tshort form of %s (no time since last output or pid)\n", program);
		pfmt(stderr, MM_NOSTD, ":747:t\ttime changes\n");
		pfmt(stderr, MM_NOSTD, ":748:T\tstatus of tty (+ writable, - not writable, ? hung)\n");
		pfmt(stderr, MM_NOSTD, ":749:u\tuseful information\n");
		exit(1);
	}

	if (argc == optind + 1) {
		optcnt++;
		ck_file(argv[optind]);
		utmpname(argv[optind]);
		filesw = 1;
	}

	/*
		Test for 'who am i' or 'who am I'
	*/
	if (argc == 3 && strcmp(argv[1], "am") == 0 && ((argv[2][0] == 'i' || argv[2][0] == 'I') && argv[2][1] == '\0')) {
		justme = 1;
		myname = nameval;
		cuserid(myname);
		if ((mytty = ttyname(fileno(stdin))) == NULL &&
		    (mytty = ttyname(fileno(stdout))) == NULL &&
		    (mytty = ttyname(fileno(stderr))) == NULL) {
			pfmt(stderr, MM_ERROR,
				":750:Must be attached to a terminal for the 'am I' option\n");
			fflush(stderr);
			exit(1);
		} else mytty += 5; /* bump past "/dev/" */
	}

	if (!terse) {
		if (Hopt) pfmt(stdout, MM_NOSTD,
			":751:NAME       LINE         TIME          IDLE         PID  COMMENTS\n");

		timnow = time(0);

#ifdef __sgi
		if ((rc = stat(_PATH_INITTAB, stbufp)) == -1)
			pfmt(stderr, MM_ERROR, badstat, _PATH_INITTAB,
#else
		if ((rc = stat("/etc/inittab", stbufp)) == -1)
			pfmt(stderr, MM_ERROR, badstat, "/etc/inittab",
#endif /* __sgi */
				strerror(errno));

		if (stbufp->st_size == 0)
			exit(1);

		if ((inittab = malloc(stbufp->st_size+1)) == NULL) {
			pfmt(stderr, MM_ERROR,
				":752:Cannot allocate %d bytes: %s\n",
				stbufp->st_size+1, strerror(errno));
			exit(errno);
		}

#ifdef __sgi
		if ((rc = stat(_PATH_INITTAB, stbufp)) == -1) { /*}*/
			pfmt(stderr, MM_ERROR, badstat, _PATH_INITTAB,
#else
		if ((rc = stat("/etc/inittab", stbufp)) == -1) {
			pfmt(stderr, MM_ERROR, badstat, "/etc/inittab",
#endif /* __sgi */
				strerror(errno));
#ifdef i386
			pfmt(stderr, MM_INFO, using_init_base);
			initbase = 1;
#else
			exit(errno);
#endif /* i386 */

#ifdef __sgi
		} else if ((fildes = open(_PATH_INITTAB, O_RDONLY)) == -1) {
			pfmt(stderr, MM_ERROR, badopen, _PATH_INITTAB,
#else
		} else if ((fildes = open("/etc/inittab", O_RDONLY)) == -1) {
			pfmt(stderr, MM_ERROR, badopen, "/etc/inittab",
#endif /* __sgi */
				strerror(errno));
#ifdef i386
			pfmt(stderr, MM_INFO, using_init_base);
			initbase = 1;
#else
			exit(errno);
#endif /* i386 */

		} else if ((rc = read(fildes, inittab, stbufp->st_size)) != stbufp->st_size) {
#ifdef __sgi
			pfmt(stderr, MM_ERROR, badread, _PATH_INITTAB,
#else
			pfmt(stderr, MM_ERROR, badread, "/etc/inittab",
#endif /* __sgi */
				strerror(errno));
#ifdef i386
			pfmt(stderr, MM_INFO, using_init_base);
			initbase = 1;
#else
			exit(errno);
#endif /* i386 */
		}

#ifdef i386
		if (initbase) {
			if ((rc = stat("/etc/conf/cf.d/init.base", stbufp)) == -1) {
				pfmt(stderr, MM_ERROR, badstat,
					"/etc/conf/cf.d/init.base",
					strerror(errno));
				exit(errno);
			}
	
			if ((fildes = open("/etc/conf/cf.d/init.base", O_RDONLY)) == -1) {
				pfmt(stderr, MM_ERROR, badopen,
					"/etc/conf/cf.d/init.base",
					strerror(errno));
				exit(errno);
			}

			if ((rc = read(fildes, inittab, stbufp->st_size)) != stbufp->st_size) {
				pfmt(stderr, MM_ERROR, badread,
					"/etc/conf/cf.d/init.base",
					strerror(errno));
				exit(errno);
			}
		}

#endif /* i386 */

		inittab[stbufp->st_size] = '\0';
		iinit = inittab;
	}
	else if (Hopt) pfmt(stdout, MM_NOSTD,
		":753:NAME       LINE         TIME\n");

	process();

	/*
		'who -q' requires EOL upon exit,
		followed by total line
	*/
	if (qopt) pfmt(stdout, MM_NOSTD, ":754:\n# users=%d\n", totlusrs);
	return(0);
}
dump()
{
char	device[32];
time_t hr;
time_t	idle;
time_t min;
char	path[64];
char	pexit;
char	pterm;
int	rc;
char	w;	/* writeability indicator */

	/*
		Get and check user name
	*/
	strcpy(user, utmpp->ut_user);
	if ((rc = strlen(user)) > 8) user[8]='\0';
	if ((rc = strlen(user)) == 0) strcpy(user, "   .");
	totlusrs++;

	/*
		Do print in 'who -q' format
	*/
	if (qopt) {
		if ((totlusrs - 1)%number == 0 && totlusrs > 1) printf("\n");
		printf("%-8s ", user);
		return(0);
	}

	pexit = ' ';
	pterm = ' ';

	/*
		Get exit info if applicable
	*/
	if (utmpp->ut_type == RUN_LVL || utmpp->ut_type == DEAD_PROCESS) {
		pterm = utmpp->ut_exit.e_termination;
		pexit = utmpp->ut_exit.e_exit;
	}

	/*
		Massage ut_tv.tv_sec field
	*/
	lptr = &utmpp->ut_tv.tv_sec;
	cftime(time_buf, gettxt(DATE_FMTID, DATE_FMT), lptr);

	/*
		Get and massage device
	*/
	if ((rc = strlen(utmpp->ut_line)) == 0) strcpy(device, "     .");
	else strcpy(device, utmpp->ut_line);

	/*
		Get writeability if requested
	*/
	if (Topt && (utmpp->ut_type == USER_PROCESS || utmpp->ut_type == LOGIN_PROCESS || utmpp->ut_type == INIT_PROCESS)) {
		w = '-';
		strcpy(path, "/dev/");
		strcat(path, utmpp->ut_line);

		if ((rc = stat(path, stbufp)) == -1) w = '?';
		else if (stbufp->st_mode & S_IWOTH) w = '+';
	}
	else w = ' ';

	/*
		Skip terminals that are not the current one  (-m option)
	*/
	if (mopt) { /* this option needs to know what "device" is */
		mytty = ttyname(fileno(stdin));
		mytty += 5; /* bump after /dev/ */
		if(strcmp(mytty, device) && (utmpp->ut_type == USER_PROCESS)) 
			return (0);
	}

	/*
		Skip the non-LOGIN users under "validtype[LOGIN_PROCESS] = 1" 
	*/
	if (lopt) { /* this option needs to know who the "user" is */
		if (strcmp(user,"LOGIN") && (utmpp->ut_type == LOGIN_PROCESS)) 
			return(0);
	}

	/*
		Print the TERSE portion of the output
	*/
	printf("%-8s %c %-12s %s", user, w, device, time_buf);

	if (!terse) {
		strcpy(path, "/dev/");
		strcat(path, utmpp->ut_line);

		/*
			Stat device for idle time
			(Don't complain if you can't)
		*/
		if ((rc = stat(path, stbufp)) != -1) {
			idle = timnow - stbufp->st_mtime;
			hr = idle/3600;
			min = (unsigned)(idle/60)%60;
			if (hr == 0 && min == 0) printf("   .  ");
			else {
				if (hr < 24) pfmt(stdout, MM_NOSTD,
					":755: %2d:%2.2d", hr, min);
				else pfmt(stdout, MM_NOSTD, ":756:  old ");
			}
		}

		/*
			Add PID for verbose output
		*/
		if (utmpp->ut_type != BOOT_TIME && utmpp->ut_type != RUN_LVL && utmpp->ut_type != ACCOUNTING) printf("  %10d", utmpp->ut_pid);

		/*
			Handle /etc/inittab comment
		*/
		if (utmpp->ut_type == DEAD_PROCESS) {
			pfmt(stdout, MM_NOSTD,
				":757:  id=%4.4s term=%-3d exit=%d",
				utmpp->ut_id, pterm, pexit);
		} else if (utmpp->ut_type != INIT_PROCESS) {
			/*
				Search for each entry in inittab
				string. Keep our place from
				search to search to try and
				minimize the work. Wrap once if needed
				for each entry.
			*/
			wrap = 0;
			/*
				Look for a line beginning with 
				utmpp->ut_id
			*/
			while ((rc = strncmp(utmpp->ut_id, iinit, strcspn(iinit, ":"))) != 0) {
				for (; *iinit != '\n'; iinit++);
				iinit++;

				/*
					Wrap once if necessary to 
					find entry in inittab
				*/
				if (*iinit == '\0') {
					if (!wrap) {
						iinit = inittab;
						wrap = 1;
					}
				}
			}
	
			if (*iinit != '\0') {
				/*
					We found our entry
				*/
				for (iinit++; *iinit != '#' && *iinit != '\n'; iinit++);

				if (*iinit == '#') {
					for (iinit++; *iinit == ' ' || *iinit == '\t'; iinit++);
					for(rc = 0; *iinit != '\n'; iinit++) comment[rc++] = *iinit;
					comment[rc] = '\0';
					printf("  %s", comment);
				}

			} else {
				iinit = inittab;	/* Reset pointer */
#ifdef DEBUG /* added to aid utmp debugging but confuses who -r output */
				pfmt(stdout, MM_NOSTD,
					":757:  id=%4.4s term=%-3d exit=%d",
					utmpp->ut_id, pterm, pexit);
#endif
			}
		} else
			pfmt(stdout, MM_NOSTD, ":758: id=%4.4s", utmpp->ut_id);
		
	}

	/*
		Handle RUN_LVL process (If no alt. file - Only one!)
	*/
	if (utmpp->ut_type == RUN_LVL) {
		printf("    %c   %10d  %c", pterm, utmpp->ut_pid, pexit);
		if (optcnt == 1 && !validtype[USER_PROCESS]) {
			printf("\n");
			exit(0);
		}
	}

	/*
		Handle BOOT_TIME process (If no alt. file - Only one!)
	*/
	if (utmpp->ut_type == BOOT_TIME) {
		if (optcnt == 1 && !validtype[USER_PROCESS]) {
			printf("\n");
			exit(0);
		}
	}

	/* 
	 * if from the network, print the name of the remote host
	 */
	if (utmpp->ut_host[0])
		printf("  (%.*s)", sizeof(utmpp->ut_host), utmpp->ut_host);

	/*
		Now, put on the trailing EOL
	*/
	printf("\n");
	return(0);
}
vt_dump()
{
char	device[32];
time_t hr;
time_t	idle;
time_t min;
char	path[128];
char	pexit;
char	pterm;
int	rc;
char	w;	/* writeability indicator */

	/*
		Get and check user name
	*/
	strcpy(user, utmpp->ut_user);
	if ((rc = strlen(user)) > 8) user[8]='\0';
	if ((rc = strlen(user)) == 0) strcpy(user, "   .");
	totlusrs++;

	/*
		Do print in 'who -q' format
	*/
	if (qopt) {
		if ((totlusrs - 1)%number == 0 && totlusrs > 1) printf("\n");
		printf("%-8s ", user);
		return(0);
	}

	pexit = ' ';
	pterm = ' ';

	/*
		Get exit info if applicable
	*/
	if (utmpp->ut_type == RUN_LVL || utmpp->ut_type == DEAD_PROCESS) {
		pterm = utmpp->ut_exit.e_termination;
		pexit = utmpp->ut_exit.e_exit;
	}

	/*
		Massage ut_tv.tv_sec field
	*/
	lptr = &utmpp->ut_tv.tv_sec;
	cftime(time_buf, gettxt(DATE_FMTID, DATE_FMT), lptr);

	/*
		Get and massage device
	*/
	if ((rc = strlen(mytty)) == 0) strcpy(device, "     .");
	else strcpy(device, mytty);

	/*
		Get writeability if requested
	*/
	if (Topt && (utmpp->ut_type == USER_PROCESS || utmpp->ut_type == LOGIN_PROCESS || utmpp->ut_type == INIT_PROCESS)) {
		w = '-';
		strcpy(path, "/dev/");
		strcat(path, mytty);

		if ((rc = stat(path, stbufp)) == -1) w = '?';
		else if (stbufp->st_mode & S_IWOTH) w = '+';
	}
	else w = ' ';

	/*
		Skip terminals that are not the current one  (-m option)
	*/
	if (mopt) { /* this option needs to know what "device" is */
		mytty = ttyname(fileno(stdin));
		mytty += 5; /* bump after /dev/ */
		if(strcmp(mytty, device)) return (0);
	}

	/*
		Skip the non-LOGIN users under "validtype[LOGIN_PROCESS] = 1" 
	*/
	if (lopt) { /* this option needs to know who the "user" is */
		if (strcmp(user,"LOGIN")) return(0);
	}

	/*
		Print the TERSE portion of the output
	*/
	printf("%-8s %c %-12s %s", user, w, device, time_buf);

	if (!terse) {
		strcpy(path, "/dev/");
		strcat(path, mytty);

		/*
			Stat device for idle time
			(Don't complain if you can't)
		*/
		if ((rc = stat(path, stbufp)) != -1) {
			idle = timnow - stbufp->st_mtime;
			hr = idle/3600;
			min = (unsigned)(idle/60)%60;
			if (hr == 0 && min == 0) printf("   .  ");
			else {
				if (hr < 24) pfmt(stdout, MM_NOSTD,
					":755: %2d:%2.2d", hr, min);
				else pfmt(stdout, MM_NOSTD, ":756:  old ");
			}
		}

		/*
			Add PID for verbose output
		*/
		if (utmpp->ut_type != BOOT_TIME && utmpp->ut_type != RUN_LVL && utmpp->ut_type != ACCOUNTING) printf("  %10d", utmpp->ut_pid);

		/*
			Handle /etc/inittab comment
		*/
		if (utmpp->ut_type == DEAD_PROCESS) 
			pfmt(stdout, MM_NOSTD,
				":757:  id=%4.4s term=%-3d exit=%d",
				utmpp->ut_id, pterm, pexit);
		else if (utmpp->ut_type != INIT_PROCESS) {
			/*
				Search for each entry in inittab
				string. Keep our place from
				search to search to try and
				minimize the work. Wrap once if needed
				for each entry.
			*/
			wrap = 0;
			/*
				Look for a line beginning with 
				utmpp->ut_id
			*/
			while ((rc = strncmp(utmpp->ut_id, iinit, strcspn(iinit, ":"))) != 0) {
				for (; *iinit != '\n'; iinit++);
				iinit++;

				/*
					Wrap once if necessary to 
					find entry in inittab
				*/
				if (*iinit == '\0') {
					if (!wrap) {
						iinit = inittab;
						wrap = 1;
					}
				}
			}
	
			if (*iinit != '\0') {
				/*
					We found our entry
				*/
				for (iinit++; *iinit != '#' && *iinit != '\n'; iinit++);

				if (*iinit == '#') {
					for (iinit++; *iinit == ' ' || *iinit == '\t'; iinit++);
					for(rc = 0; *iinit != '\n'; iinit++) comment[rc++] = *iinit;
					comment[rc] = '\0';
					printf("  %s", comment);
				}

			}
			else iinit = inittab;	/* Reset pointer */
		}
		if (utmpp->ut_type == INIT_PROCESS)
			pfmt(stdout, MM_NOSTD, ":758: id=%4.4s", utmpp->ut_id);
		
	}

	/*
		Handle RUN_LVL process (If no alt. file - Only one!)
	*/
	if (utmpp->ut_type == RUN_LVL) {
		printf("    %c%10d    %c", pterm, utmpp->ut_pid, pexit);
		if (optcnt == 1 && !validtype[USER_PROCESS]) {
			printf("\n");
			exit(0);
		}
	}

	/*
		Handle BOOT_TIME process (If no alt. file - Only one!)
	*/
	if (utmpp->ut_type == BOOT_TIME) {
		if (optcnt == 1 && !validtype[USER_PROCESS]) {
			printf("\n");
			exit(0);
		}
	}

	/* 
	 * if from the network, print the name of the remote host
	 */
	if (utmpp->ut_host[0])
		printf("  (%.*s)", sizeof(utmpp->ut_host), utmpp->ut_host);


	/*
		Now, put on the trailing EOL
	*/
	printf("\n");
	return(0);
}

void process()
{
int	rc;

	/*
		Loop over each entry in /var/adm/utmpx
	*/
	while ((utmpp = getutxent()) != NULL) {
#ifdef DEBUG
	printf("ut_user '%s'\nut_id '%s'\nut_line '%s'\nut_type '%d'\n\n", utmpp->ut_user, utmpp->ut_id, utmpp->ut_line,utmpp->ut_type);
#endif
		if (utmpp->ut_type >= 0 && utmpp->ut_type <= UTMAXTYPE) {
			/*
				Handle "am i"
			*/
			if (justme) {
#ifdef __sgi
				if (( rc = strncmp(myname, utmpp->ut_user, 8)) == 0 && !strncmp(mytty, utmpp->ut_line, 8)) { /* } */
#else
				if (( rc = strcmp(myname, utmpp->ut_user)) == 0 && !strcmp(mytty, utmpp->ut_line)) {
#endif /* __sgi */
					dump();
					exit(0);
				}
				continue;
			}
	
			/*
				Print the line if we want it
			*/
			if (validtype[utmpp->ut_type]) dump();
		}
		else pfmt(stderr, MM_ERROR,
			":759:Entry has ut_type of %d when maximum is %d\n",
			utmpp->ut_type, UTMAXTYPE);
	}


	if (justme) {
		/*
		 * If justme and got here, must be a vt.
		 * Just try for name match this time.
		 * Assuming utmp hasn't been updated with vt name.
		 */

		setutxent();

		while ((utmpp = getutxent()) != NULL) {
			if (utmpp->ut_type <= UTMAXTYPE) {
#ifdef __sgi
				if ((rc = strncmp(myname, utmpp->ut_user, 8))==0){ /* } */
#else
				if ((rc = strcmp(myname, utmpp->ut_user))==0){
#endif /* __sgi */
					vt_dump();
					exit(0);
				}
				continue;
	
			}
		}
	}
}

/*
	This routine checks the following:

	1.	File exists

	2.	We have read permissions

	3.	It is a multiple of utmp entries in size

	Failing any of these conditions causes who(1) to
	abort processing.

	4.	If file is empty we exit right away as there
		is no info to report on.
*/
void
ck_file(char *name)
{
	FILE	*file;
	struct	stat sbuf;
	int	rc;

	/*
		Does file exist? Do stat to check, and save structure
		so that we can check on the file's size later on.
	*/
	if ((rc = stat(name,&sbuf)) == -1) {
		pfmt(stderr, MM_ERROR, badstat, name, strerror(errno));
		exit(1);
	}

	/*
		The only real way we can be sure we can access the
		file is to try. If we succeed then we close it.
	*/
	if ((file = fopen(name,"r")) == NULL) {
		pfmt(stderr, MM_ERROR, badopen, name, strerror(errno));
		exit(1);
	}
	fclose(file);

	/*
		If the file is empty, we are all done.
	*/
	if (!sbuf.st_size) exit(0);

	/*
		Make sure the file is a utmp file.
		We can only check for size being a multiple of
		utmp structures in length.
	*/
	rc = sbuf.st_size % sizeof(struct utmp);
	if (rc) {
		pfmt(stderr, MM_ERROR, ":760:File '%s' is not a utmp file\n",
			name);
		exit(1);
	}
}
