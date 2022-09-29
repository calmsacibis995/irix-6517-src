/* Copyright (c) 1984 AT&T      */
/* All Rights Reserved          */

/* THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T  */
/* The copyright notice above does not evidence any     */
/* actual or intended publication of such source code.  */

/* #ident       "@(#)cron:at.c  1.12" */
#ident	"$Revision: 1.26 $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include "cron.h"
#include "funcs.h"
#include "permit.h"
#include <locale.h>		/* i18n=internationalization */
#include <unistd.h>		/* i18n */
#include <sgi_nl.h>		/* i18n */
#include <msgs/uxsgicore.h>	/* i18n */
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <sat.h>
#include <getopt.h>


#define TMPFILE		"_at"	/* prefix for temporary files     */
				/* Mode for creating files in ATDIR. */
#define ATMODE		(S_IRUSR | S_IRGRP | S_IROTH)
#define ROOT		0	/* user-id of super-user */
#define BUFSIZE		512	/* for copying files */
#define PIDSIZE         11	/* for PID as a string */
#define LINESIZE	130	/* for listing jobs */
#define	MAXTRYS		100	/* max trys to create at job file */
#define	RIGHT_NOW	2	/* execute at job right now */
#define TMPLINE_MAX	200

/* i18n messages (in irix/cmd/messages/uxsgicore/msgs.src) */
#define BADDATE		0	/* "bad date specification"               */
#define BADSHELL	1	/* "because your login shell isn't /bin/sh,
				 * you can't use at"      */
#define WARNSHELL	2	/* "warning: commands will be executed     *
				 * using /bin/sh\n"               */
#define CANTCD		3	/* "can't change directory to the at *
				 * directory" */
#define CANTCHOWN	4	/* "can't change the owner of your job to
				 * you" */
#define CANTCREATE	5	/* "can't create a job for you"           */
#define INVALIDUSER	6	/* "you are not a valid user (no entry in  *
				 * /etc/passwd)"                  */
#define NOOPENDIR	7	/* "can't open the at directory"          */
#define NOTALLOWED	8	/* "you are not authorized to use at. *
				 * Sorry." */
#define NOTHING		9	/* "nothing specified" */
#define NOPROTOTYPE	10	/* "no prototype" */
#define PIPEOPENFAIL	11	/* "pipe open failed" */
#define FORKFAIL	12	/* "fork failed" */
#define INVALIDQUEUE	13	/* "invalid queue specified: not lower case * 
				 * letter */
#define INCOMPATOPT	14	/* "incompatible options" */
#define OUTOFMEM	15	/* "Out of memory" */
#define TOOLATE		16	/* "too late" */
#define QUEUEFULL	17	/* "queue full" */
#define CANTGETSTAT	18	/* "Can not get status of spooling directory
				 * for at */
#define BADDATECONV	19	/* "bad date specification" */
#define IMPROPER	20	/* "job may not be executed at the proper *
				 * time" */
#define NOCRON		21	/* "cron may not be running - call your *
				 * system administrator " */
#define MSGQOPEN	22	/* "error in message queue open" */
#define MSGSENTERR	23	/* "error in message send" */
#define FILEOPENFAIL	24	/* "Cannot open %s" */

#define EXIT_WITH_1	1	/* for flag exit_1 */
#define DONT_EXIT	0	/* for flag exit_1 */

extern char *argp;
extern int yyparse(void);

struct tm *tp, at, rt;
int gmtflag = 0, utc_flag = 0;
char now_flag = 0;		/* 0 = not "now", 1 = "now", 2 = "rightnow" */
int mday[12] = {31, 38, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static char login[LLEN + 1], pname[80], pname1[80];
static char filenamebuf[PATH_MAX + 1], argpbuf[BUFSIZE];
static int mac_enabled;
static time_t timbuf, when, now;
static char jobtype = ATEVENT;	/* set to 1 if batch job */
static char *tfname, *atpath, *cbp;
static const int dmsize[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* forward declarations */
static void usages(void);
char *mkjobname(time_t, int);
void handle_signals(void);
void atabort(int, int);
void atime(struct tm *, struct tm *);
time_t gtime(const struct tm *);
void copy(FILE *, int, uid_t, uid_t);
static void sendmsg(char, const char *);
int topt_gtime(size_t);
int gpair(void);
uid_t get_audit_id(void);

int
main(int argc, char **argv)
{
	DIR *dir;
	FILE *fp;
	struct passwd *pw;
	struct stat buf;
	uid_t user, euid;
	char *ptr, *job, *jobname, *pp, queuename = '\0';
	time_t t = 0;
	int fopt = 0, lopt = 0, mopt = 0, qopt = 0, topt = 0, ropt = 0;
	int st = 1, i, fd, c, len, errflg = 0;

	/* i18n */
	setlocale(LC_ALL, "");
	setcat("uxsgicore");
	setlabel("at");

	/* Are we running on a system with Mandatory Access Control? */
	mac_enabled = (sysconf(_SC_MAC) > 0);

	/* Determine which working directory to use */
	atpath = (mac_enabled ? MACATDIR : ATDIR);

	/* usage */
	if (argc < 2) {
		usages();
	}

	pp = getuser(user = getuid());
	if (pp == NULL) {
		atabort(per_errno == 2 ? BADSHELL : INVALIDUSER, EXIT_WITH_1);
	}
	strncpy(login, pp, sizeof (login) - 1);
	login[sizeof (login) - 1] = '\0';
	if (!allowed(login, ATALLOW, ATDENY))
		atabort(NOTALLOWED, EXIT_WITH_1);

	/* analyze all options */
	while ((c = getopt(argc, argv, "f:q:t:mrl")) != -1) {
		switch (c) {
			case 'l':
				lopt++;
				break;
			case 'm':
				mopt++;
				break;
			case 'f':
				fopt++;
				strncpy(filenamebuf, optarg, PATH_MAX);
				filenamebuf[PATH_MAX] = '\0';
				break;
			case 't':
				topt = 1;
				cbp = optarg;
				if (topt_gtime(strlen(cbp))) {
					atabort(BADDATECONV, EXIT_WITH_1);
				}
				break;

			case 'r':
				ropt = 1;
				break;

			case 'q':
				qopt = 1;
				if (strlen(optarg) != 1 || !(isalpha(*optarg) && islower(*optarg))) {
					atabort(INVALIDQUEUE, EXIT_WITH_1);
					errflg++;
				}
				queuename = *optarg;
				jobtype = *optarg - 'a';
				break;
			default:
				errflg++;
				break;
		}
	}

	if (errflg) {
		usages();
	}

	/* weed out incompatible options */
	if (((mopt || fopt || topt) && !(ropt) && !(lopt))
	    || (!(mopt || fopt || topt || lopt || qopt) && ropt)
	    || (!(mopt || fopt || topt) && (lopt || qopt) && !ropt)
	    || (!(mopt || fopt || topt || lopt || qopt || ropt))) {
		st = optind;	/* where time spec begins */
	} else
		atabort(INCOMPATOPT, EXIT_WITH_1);

	/* -r option is always used by itself */
	if (ropt && !mopt && !fopt && !qopt && !topt && !lopt) {
		/* remove jobs that are specified */
		if (chdir(atpath) == -1)
			atabort(CANTCD, EXIT_WITH_1);
		for (i = 2; i < argc; i++) {
			if (strchr(argv[i], '/') != NULL)
				_sgi_nl_error(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_invalidjob, "invalid job name %s"), argv[i]);
			else if (stat(argv[i], &buf) == -1)
				_sgi_nl_error(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_jobdoesnotexist, "%s does not exist"), argv[i]);
			else if (user != buf.st_uid && user != ROOT)
				_sgi_nl_error(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_youdontown, "you don't own %s"), argv[i]);
			else {
				sendmsg(DELETE, argv[i]);
				unlink(argv[i]);
			}
		}
		exit(0);
	}

	if (lopt && !mopt && !topt && !fopt) {
		/* list jobs for user */
		if (chdir(atpath) == -1)
			atabort(CANTCD, EXIT_WITH_1);
		if (argc == 2 || qopt) {
			/* list all jobs for a user */
			if ((dir = opendir(atpath)) == NULL)
				atabort(NOOPENDIR, EXIT_WITH_1);
			for (;;) {
				struct dirent *dp;

				if ((dp = readdir(dir)) == NULL)
					break;

				/* ignore `.' and `..' */
				if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
					continue;

				/* check ownership of job */
				if (stat(dp->d_name, &buf) == -1) {
					unlink(dp->d_name);
					continue;
				}
				if (user != buf.st_uid && user != ROOT)
					continue;

				/* get start time of job */
				errno = 0;
				t = (time_t) strtol(dp->d_name, &ptr, 10);
				if (ptr == dp->d_name || errno == ERANGE || *ptr != '.')
					continue;

				/*
				 * display either all jobs or only those that 
				 * are associated with a given queue (option)
				 */
				if (!qopt || (qopt && *++ptr == queuename)) {
					if (user == ROOT && (pw = getpwuid(buf.st_uid)) != NULL)
						printf("user = %s\t%s\t%s", pw->pw_name, dp->d_name, asctime(localtime(&t)));
					else
						printf("%s\t%s", dp->d_name, asctime(localtime(&t)));
				}
			}
			(void) closedir(dir);
		} else {
			/* list particular jobs for user */
			for (i = 2; i < argc; i++) {
				errno = 0;
				t = (time_t) strtol(argv[i], &ptr, 10);
				if (ptr == argv[i] || errno == ERANGE || *ptr != '.' || strchr(argv[i], '/') != NULL)
					_sgi_nl_error(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_invalidjob, "invalid job name %s"), argv[i]);
				else if (stat(argv[i], &buf) == -1)
					_sgi_nl_error(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_jobdoesnotexist, "%s does not exist"), argv[i]);
				else if (user != buf.st_uid && user != ROOT)
					_sgi_nl_error(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_youdontown, "you don't own %s"), argv[i]);
				else
					printf("%s\t%s", argv[i], asctime(localtime(&t)));
			}
		}
		exit(0);
	}

	/* figure out what time to run the job */
	if (argc == 1 && jobtype != BATCHEVENT)
		atabort(NOTHING, EXIT_WITH_1);
	time(&now);
	if (jobtype != BATCHEVENT) {	/* at job */
		argp = argpbuf;
		i = st;
		while (i < argc && strlen(argp) + strlen(argv[i]) < BUFSIZE) {
			strcat(argp, argv[i]);
			strcat(argp, " ");
			i++;
		}
		tp = localtime(&now);
		mday[1] = 28 + leap(tp->tm_year);
		if (!topt)
			yyparse();
		atime(&at, &rt);
		when = gtime(&at);
		if (!gmtflag) {
			if (topt)
				when = timbuf;
			when += timezone;
			if (localtime(&when)->tm_isdst) {
				when -= (time_t) (timezone - altzone);
			}
		}
		if (utc_flag) {
			if (when < now)
				when += 24 * 60 * 60;	/* add a day */
		}
		if (now_flag == RIGHT_NOW)
			when = now;
	} else			/* batch job */
		when = now;
	if (when < now)		/* time has already past */
		atabort(TOOLATE, EXIT_WITH_1);

	len = strlen(atpath) + strlen(TMPFILE) + PIDSIZE + 3;
	tfname = (char *) xmalloc(len);
	snprintf(tfname, len, "%s/%s.%d", atpath, TMPFILE, (int) getpid());

	/* catch SIGINT, HUP, QUIT, and TERM signals */
	handle_signals();

	/* create temporary jobfile */
	if ((fd = open(tfname, O_CREAT | O_EXCL | O_WRONLY, ATMODE)) == -1)
		atabort(CANTCREATE, EXIT_WITH_1);
	if ((fp = fdopen(fd, "w")) == NULL)
		atabort(CANTCREATE, EXIT_WITH_1);

	/* Change its ownership to the user */
	if (fchown(fd, user, getgid()) == -1) {
		fclose(fp);
		unlink(tfname);
		atabort(CANTCHOWN, EXIT_WITH_1);
	}

	/* copy job info into temporary jobfile */
	snprintf(pname, sizeof (pname), "%s", PROTO);
	snprintf(pname1, sizeof (pname1), "%s.%c", PROTO, 'a' + jobtype);
	copy(fp, fopt, user, euid = geteuid());

	/* 
	 * Set SUID and SGID bits on the file so that if an owner of a file
	 * gives that file away to someone else, the SUID/SGID bits will no
	 * longer be set. If this happens, atrun will not execute the file
	 */
	if (setreuid((uid_t) -1, user) == -1 ||
	    fchmod(fd, S_ISUID | S_ISGID | ATMODE) == -1 ||
	    setreuid((uid_t) -1, euid) == -1) {
		fclose(fp);
		unlink(tfname);
		atabort(NOTALLOWED, EXIT_WITH_1);
	}
	fclose(fp);

	/* move temporary jobfile into real job slot */
	if (rename(tfname, job = mkjobname(when, mopt)) == -1) {
		unlink(tfname);
		free(job);
		atabort(CANTCREATE, EXIT_WITH_1);
	}

	/* notify cron of the new job */
	sendmsg(ADD, jobname = strrchr(job, '/') + 1);
	if (per_errno == 2)
		atabort(WARNSHELL, DONT_EXIT);
	fprintf(stderr, "job %s at %.24s\n", jobname, ctime(&when));
	free(job);

	if (when - t - CR_MINUTE < CR_HOUR)
		atabort(IMPROPER, DONT_EXIT);
	return(0);
}

static void
usages(void)
{
	_sgi_nl_usage(SGINL_USAGE, "at", gettxt(_SGI_MMX_at_usage1, "at [-m] [-f file] [-q queuename] time [ date ] [ +increment ]"));
	_sgi_nl_usage(SGINL_USAGE, "at", gettxt(_SGI_MMX_at_usage2, "at -r job..."));
	_sgi_nl_usage(SGINL_USAGE, "at", gettxt(_SGI_MMX_at_usage3, "at -l [ job ... ]"));
	_sgi_nl_usage(SGINL_USAGE, "at", gettxt(_SGI_MMX_at_usage4, "at -l -q queuename..."));
	exit(2);
}

char *
mkjobname(time_t t, int m_option)
{
	int i, len, off = (m_option ? 1 : 0);
	uid_t satid = get_audit_id();
	char *name;
	struct stat buf;

	name = (char *) xmalloc(TMPLINE_MAX);
	for (i = 0; i < MAXTRYS; i++) {
		len = snprintf(name, TMPLINE_MAX, "%s/%ld.%c", atpath,
			       (long) t, 'a' + jobtype);
		if (m_option && len < TMPLINE_MAX - 1)
			strcat(name, "m");
		if (satid != -1) {
			snprintf(name + len + off, TMPLINE_MAX - len - off,
				 "+%ld", (long) satid);
		}
		if (stat(name, &buf) == -1)
			return(name);
		t += 1;
	}
	atabort(QUEUEFULL, EXIT_WITH_1);
	return(NULL);
}

/* ARGSUSED */
void
catch(int sig)
{
	if (tfname != NULL)
		(void) unlink(tfname);
	exit(1);
}

void
handle_signals(void)
{
	int i, sigs[] = {SIGINT, SIGHUP, SIGQUIT, SIGTERM};
	struct sigaction act, oact, *snil = NULL;

	act.sa_flags = 0;
	act.sa_handler = catch;
	if (sigemptyset(&act.sa_mask) == -1)
		atabort(CANTCREATE, EXIT_WITH_1);

	/* 
	 * Install signal handler(s), but only if we are not
	 * currently ignoring the signal in question.
	 */
	for (i = 0; i < (sizeof (sigs) / sizeof (sigs[0])); i++) {
		if (sigaction(sigs[i], snil, &oact) == -1)
			atabort(CANTCREATE, EXIT_WITH_1);

		if (oact.sa_handler == SIG_IGN)
			continue;

		if (sigaction(sigs[i], &act, snil) == -1)
			atabort(CANTCREATE, EXIT_WITH_1);
	}
}

/* 
 * this function displays all messages without arguments i18n'ly
 */
void
atabort(int msg_number, int exit_1)
{
	switch (msg_number) {
		case BADDATE:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_baddate, "bad date specification"));
			break;
		case BADSHELL:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_badshell, "because your login shell isn't /bin/sh, you can't use at"));
			break;
		case WARNSHELL:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_warnshell, "warning: commands will be executed using /bin/sh"));
			break;
		case CANTCD:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_cantcd, "can't change directory to the at directory"));
			break;
		case CANTCHOWN:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_cantchown, "can't change the owner of your job to you"));
			break;
		case CANTCREATE:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_cantcreate, "can't create a job for you"));
			break;
		case INVALIDUSER:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_invaliduser, "you are not a valid user (no entry in /etc/passwd)"));
			break;
		case NOOPENDIR:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_noopendir, "can't open the at directory"));
			break;
		case NOTALLOWED:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_notallowed, "you are not authorized to use at.  Sorry."));
			break;
		case NOTHING:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_nothing, "nothing specified"));
			break;
		case NOPROTOTYPE:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_noproto, "no prototype"));
			break;
		case PIPEOPENFAIL:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_pipefail, "pipe open failed"));
			break;
		case FORKFAIL:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_forkfail, "fork failed"));
			break;
		case INVALIDQUEUE:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_invalidqueue, "invalid queue specified: not lower case letter"));
			break;
		case INCOMPATOPT:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_incompatopt, "incompatible options"));
			break;
		case OUTOFMEM:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_outofmem, "Out of memory"));
			break;
		case TOOLATE:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_toolate, "too late"));
			break;
		case QUEUEFULL:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_queuefull, "queue full"));
			break;
		case CANTGETSTAT:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_cantgetstat, "Can not get status of spooling directory for at"));
			break;
		case BADDATECONV:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_baddateconv, "bad date conversion"));
			break;
		case IMPROPER:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_at_improper, "this job may not be executed at the proper time"));
			break;
		case FILEOPENFAIL:
			_sgi_nl_usage(SGINL_NOSYSERR, "at", gettxt(_SGI_MMX_CannotOpen, "Cannot open file %s"), filenamebuf);
			break;

		default:
			break;
	}

	if (exit_1)
		exit(1);
}

int
yywrap(void)
{
	return 1;
}

/* ARGSUSED */
void
yyerror(const char *a)
{
	atabort(BADDATE, EXIT_WITH_1);
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

/* 
 * return time from time structure
 */
time_t
gtime(const struct tm *tptr)
{
	int i;
	time_t tv = 0;

	for (i = 1970; i < tptr->tm_year + 1900; i++)
		tv += 365 + leap(i);
	if (leap(tptr->tm_year) && tptr->tm_mon >= 2)
		++tv;
	for (i = 0; i < tptr->tm_mon; ++i)
		tv += dmsize[i];
	tv += tptr->tm_mday - 1;
	tv = 24 * tv + tptr->tm_hour;
	tv = 60 * tv + tptr->tm_min;
	tv = 60 * tv + tptr->tm_sec;
	return tv;
}

/* 
 * make job file from proto + stdin
 */
void
copy(FILE *fp, int f_option, uid_t ruid, uid_t euid)
{
	int c;
	FILE *pfp;
	char *val, **ep, dirbuf[PATH_MAX];
	struct rlimit rlimtab;
	mode_t um;
	extern char **environ;

	fprintf(fp, ": %s job\n", jobtype ? "batch" : "at");
	for (ep = environ; *ep; ep++) {
		if (strchr(*ep, '\'') != NULL)
			continue;
		if ((val = strchr(*ep, '=')) == NULL)
			continue;
		*val++ = '\0';
		fprintf(fp, "export %s; %s='%s'\n", *ep, *ep, val);
		*--val = '=';
	}
	if ((pfp = fopen(pname1, "r")) == NULL &&
	    (pfp = fopen(pname, "r")) == NULL)
		atabort(NOPROTOTYPE, EXIT_WITH_1);
	um = umask(0);
	while (!feof(pfp) && (c = getc(pfp)) != EOF) {
		if (c != '$')
			fputc(c, fp);
		else {
			switch (c = getc(pfp)) {
				case EOF:
					goto out;
				case 'd':
					dirbuf[0] = '\0';

					/* use real uid for operation */
					if (setreuid((uid_t) -1, ruid) == -1)
						atabort(NOTALLOWED, EXIT_WITH_1);

					if (getcwd(dirbuf, PATH_MAX) == NULL)
						atabort(NOTALLOWED, EXIT_WITH_1);

					/* revert back to saved e uid */
					if (setreuid((uid_t) -1, euid) == -1)
						atabort(NOTALLOWED, EXIT_WITH_1);

					fprintf(fp, "%s", dirbuf);

					break;
				case 'l':
					getrlimit(RLIMIT_FSIZE, &rlimtab);
					if (rlimtab.rlim_cur == RLIM_INFINITY)
						fprintf(fp, "unlimited");
					else
						fprintf(fp, "%llu",
							(unsigned long long) rlimtab.rlim_cur);

					break;
				case 'm':
					fprintf(fp, "%o", (unsigned int) um);
					break;
				case '<':
					fclose(pfp);

					if (f_option) {
						/* use real uid for fopen */
						if (setreuid((uid_t) -1, ruid) == -1)
							atabort(NOTALLOWED, EXIT_WITH_1);

						if ((pfp = fopen(filenamebuf, "r")) == NULL)
							atabort(FILEOPENFAIL, EXIT_WITH_1);

						/* revert to saved e uid */
						if (setreuid((uid_t) -1, euid) == -1)
							atabort(NOTALLOWED, EXIT_WITH_1);
					} else {
						pfp = stdin;
					}
					while ((c = getc(pfp)) != EOF)
						fputc(c, fp);
					break;
				case 't':
					fprintf(fp, ":%lu", (unsigned long) when);
					break;
				default:
					fputc(c, fp);
			}
		}
	}
out:
	/* 
	 * Flush the output before we tell cron about this command file.
	 */
	fflush(fp);
	if (pfp != stdin)
		fclose(pfp);
}

static void
sendmsg(char action, const char *fname)
{
	static int msgfd = -2;
	struct message msg;

	if (msgfd == -2) {
		if ((msgfd = open(FIFO, O_WRONLY | O_NDELAY)) == -1) {
			if (errno == ENXIO || errno == ENOENT)
				atabort(NOCRON, DONT_EXIT);
			else
				atabort(MSGQOPEN, DONT_EXIT);
			msgfd = -2;
			return;
		}
	}

	msg.action = action;
	strncpy(msg.logname, login, sizeof(msg.logname));
	msg.logname[sizeof(msg.logname) - 1] = '\0';
	strncpy(msg.fname, fname, sizeof(msg.fname));
	msg.fname[sizeof(msg.fname) - 1] = '\0';

	if (mac_enabled) {
		mac_t lbl;
		char *lblstr;
		size_t lblstrlen;

		msg.etype = TRIX_AT;

		/* get our mac label */
		if ((lbl = mac_get_proc()) == NULL)
		{
			fprintf(stderr, "crontab: error in mac processing\n");
			close(msgfd);
			return;
		}

		/* convert mac label to human-readable form */
		lblstr = mac_to_text(lbl, &lblstrlen);
		mac_free(lbl);
		if (lblstr == NULL)
		{
			fprintf(stderr, "crontab: error in mac processing\n");
			close(msgfd);
			return;
		}

		/* copy to message buffer */
		strncpy(msg.label, lblstr, sizeof(msg.label));
		msg.label[sizeof(msg.label) - 1] = '\0';
		mac_free(lblstr);
	} else {
		msg.etype = AT;
	}

	if (write(msgfd, &msg, sizeof(msg)) != sizeof(msg))
		atabort(MSGSENTERR, DONT_EXIT);
}

/* This function translates the -t option time into timbuf */
/* This is very similar to gtime in the command "touch" */
int
topt_gtime(size_t len)
{
	int i, y, t;
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
	/* if (which) {         if '1'; means -t option */
	/* 
	 * -t [[cc]yy]mmddhhmm[.ss] is a total of 15 bytes max
	 */
	if (len > 15)
		return(1);
	/* 
	 *  Determine the decimal point position, if any.
	 */
	for (i = 0; i < len; i++) {
		if (*(cbp + i) == '.') {
			point = i;
			break;
		}
	}
	/* 
	 *  If there is a decimal point present,
	 *  AND:
	 *
	 *      the decimal point is positioned in bytes 0 thru 7;
	 *  OR
	 *      the the number of digits following the decimal point
	 *      is greater than two
	 *  OR
	 *      the the number of digits following the decimal point
	 *      is greater than two
	 *  OR
	 *      the the number of digits following the decimal point
	 *      is less than two
	 *  then,
	 *      error terminate.
	 *      
	 */
	/* the "+ 1" below means add one for decimal */
	if (point && ((point < 8) || ((len - (point + 1)) > 2) ||
		      ((len - (point + 1)) < 2)))
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
	switch (len) {
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
		if (y < 0) {
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
		if (y < 0) {
			(void) time(&nt);
			y = localtime(&nt)->tm_year;
		}
		if ((y >= 69) && (y <= 99))
			c = 19;	/* 19th century */
		if ((y >= 0) && (y <= 68))
			c = 20;	/* 20th century */
		goto mm_next;
	}
	if ((len - ssdigits) < 10) {
		/* 
		 * -t mmddhhmm is the input
		 */
		noyear++;
	}
	/* } */
mm_next:
	t = gpair();
	if (t < 1 || t > 12)
		return(1);
	d = gpair();
	if (d < 1 || d > 31)
		return(1);
	h = gpair();
	if (h == 24) {
		h = 0;
		d++;
	}
	m = gpair();
	if (m < 0 || m > 59)
		return(1);
	/* } else { */
	/* realign ! aaa */
	/* 
	 * There was a "-t" input.
	 * If there is a decimal get the seconds inout
	 */
	if (point) {
		cbp++;		/* skip over decimal point */
		s = gpair();	/* get [ss] */
		if (s < 0) {
			return(1);
		}
		if (!((s >= 0) && (s <= 61)))
			return(1);
	}
	if (noyear) {
		(void) time(&nt);
		y = localtime(&nt)->tm_year;
	}
	/* } */
	if (*cbp == 'p')
		h += 12;
	if (h < 0 || h > 23)
		return(1);
	timbuf = 0;
	if (c && (c == 20))
		y += 2000;
	else
		y += 1900;
	for (i = 1970; i < y; i++)
		timbuf += 365 + leap(i);
	/* Leap year */
	if (leap(y) && t >= 3)
		timbuf += 1;
	while (--t)
		timbuf += dmsize[t - 1];
	timbuf += (d - 1);
	timbuf *= 24;
	timbuf += h;
	timbuf *= 60;
	timbuf += m;
	timbuf *= 60;
	timbuf += s;
	return(0);
}

/* This function is called by topt_gtime */
/* This is the same gpair function as in the command "touch" */
int
gpair(void)
{
	int c, d;
	char *cp;

	cp = cbp;
	if (*cp == 0)
		return(-1);
	c = (*cp++ - '0') * 10;
	if (c < 0 || c > 100)
		return(-1);
	if (*cp == 0)
		return(-1);
	if ((d = *cp++ - '0') < 0 || d > 9)
		return(-1);
	cbp = cp;
	return(c + d);
}

uid_t
get_audit_id(void)
{
	return(sysconf(_SC_AUDIT) > 0 ? satgetid() : -1);
}
