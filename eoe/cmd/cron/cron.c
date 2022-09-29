/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.53 $"

#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h> 
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <paths.h>
#include <sat.h>
#include <clearance.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <stdarg.h>
#ifdef CRONPROJ
#include <proj.h>
#endif /* CRONPROJ */
#include "cron.h"
#include "elm.h"
#include "funcs.h"
#include <syslog.h>


#define MAIL		"/bin/mail"	/* mail program to use */
#define MAIL_HDR	"To: %s\nSubject: cron <%s@%s> %s\n\n"

#define TMPINFILE	"/tmp/crinXXXXXX"  /* file to put stdin in for cmd  */
#define	TMPDIR		"/tmp"
#define	PFX		"crout"

#define INMODE		S_IRUSR			/* mode for stdin file	*/
#define FIFOMODE	(S_IRUSR | S_IWUSR)	/* mode for stdout file */
#define ISUID		(S_ISUID | S_ISGID)	/* mode for verifing at jobs */

#define INFINITY	2147483647L	/* upper bound on time	*/
#define MAXFUDGE	7		/* must be a prime */
#define CUSHION		120L
#define	MAXRUN		25		/* max total jobs allowed in system */
#define REALMINRUN	1		/* minimum number of jobs allowed */
#define REALMAXRUN	250		/* maximum number of jobs allowed */
#define ZOMB		100		/* proc slot used for mailing output */

#define	JOBF		'j'
#define	NICEF		'n'
#define	USERF		'u'
#define WAITF		'w'

#define BCHAR		'>'
#define	ECHAR		'<'

#define	DEFAULT		0
#define	LOAD		1

#define BADCD		"can't change directory to the crontab directory."
#define NOREADDIR	"can't read the crontab directory."

#define BADJOBOPEN	"unable to read your at job."
#define BADSTAT		"can't access your crontab file.  Resubmit it."
#define CANTCDHOME	"can't change directory to your home dir (%s)\nYour commands will not be executed."
#define CANTEXECSH	"unable to exec the shell for one of your commands: %s"
#define EOLN		"unexpected end of line"
#define NOREAD		"can't read your crontab file.  Resubmit it."
#define NOSTDIN		"unable to create a standard input file for one of your crontab commands.\nThat command was not executed."
#define OUTOFBOUND	"number too large or too small for field"
#define STDOUTERR	"one of your commands generated output or errors, but cron was unable to mail you this output.\nRemember to redirect standard output and standard error for each of your commands."
#define STDATMSG	"at job completed."
#define UNEXPECT	"unexpected symbol found"
#define DIDFORK didfork
#define NOFORK !didfork


#define DEBUG_TIME() (debug_time = time(0), \
		      (void)cftime(debug_tstring, "%T",(&debug_time)), \
		      debug_tstring)
static time_t debug_time;
static char debug_tstring[100];

struct event {
	time_t time;	/* time of the event	*/
	short etype;	/* what type of event; 0=cron, 1=at	*/
	char *cmd;	/* command for cron, job name for at	*/
	struct usr *u;	/* ptr to the owner (usr) of this event	*/
	struct event *link; 	/* ptr to another event for this user */
	int sm;			/* if true, send mail always */
	union { 
		struct { /* for crontab events */
			char *minute;	/*  (these	*/
			char *hour;	/*   fields	*/
			char *daymon;	/*   are	*/
			char *month;	/*   from	*/
			char *dayweek;	/*   crontab)	*/
			char *input;	/* ptr to stdin	*/
		} ct;
		struct { /* for at events */
			short exists;	/* for revising at events	*/
			int eventid;	/* for el_remove-ing at events	*/
		} at;
	} of; 
};

struct usr {	
	char *name;		/* name of user (e.g. "root") */
	char *home;		/* home directory for user */
	uid_t uid;		/* user id */
	gid_t gid;		/* group id */
	mac_t mac;		/* MAC label to run at, if appropriate */
#ifdef ATLIMIT
	int aruncnt;		/* counter for running jobs per uid */
#endif
#ifdef CRONLIMIT
	int cruncnt;		/* counter for running cron jobs per uid */
#endif
	int ctid;		/* for el_remove-ing crontab events */
	short ctexists;		/* for revising crontab events */
	struct event *ctevents;	/* list of this usr's crontab events */
	struct event *atevents;	/* list of this usr's at events */
	struct usr *nextusr;	/* ptr to next user */
	int refcnt;		/* reference count */
};

/*
 * These macros are used to manage references to a user structure
 */
#define USR_HOLD(u)	((u)->refcnt++)
#define USR_RELE(u)	{ \
			if (--(u)->refcnt <= 0) { \
				free(u->name); \
				free(u->home); \
				mac_free(u->mac); \
				free(u); \
			} \
		}

struct	queue
{
	int njob;	/* limit */
	int nice;	/* nice for execution */
	int nwait;	/* wait time to next execution attempt */
	int nrun;	/* number running */
}	
	qd = {100, 2, 60},		/* default values for queue defs */
	qt[NQUEUE];

struct	queue	qq;
int	wait_time = 60;


/*
 * The runinfo table is allocated based on the maximum number of jobs
 * that can be simultaneously invoked from cron.  The default is
 * MAXRUN.  The user can specify a different number using the '-j'
 * option.
 *
 * It should be noted that the number chosen for the highest '-j'
 * value is arbitrary -- the current algorithms for traversing the
 * run table are array-based and inherently inefficient.
 */
struct	runinfo
{
	pid_t	pid;
	short	que;
	short   etype;		/* 0=cron 1=at */
	struct usr *rusr;	/* pointer to usr struct */
	char	*cmd;		/* command running */
	int	sm;		/* send mail */
	int	outfd;		/* descriptor where stdout & stderr go */
	time_t	start_time;
	time_t	end_time;
} *rt;

short didfork = 0;	/* flag to see if I'm process group leader */
int msgfd;		/* file descriptor for fifo queue */
int ecid = 1;		/* for giving event classes distinguishable id names
			   for el_remove'ing them. MUST be initialized to 1 */
int delayed;		/* is job being rescheduled or did it run first time */
int cwd;		/* current working directory */
int running;		/* zero when no jobs are executing */
char *atdir = ATDIR;
struct event *next_event;	/* the next event to execute	*/
struct usr *uhead;	/* ptr to the list of users	*/
struct usr *ulast;	/* ptr to last usr table entry */
time_t init_time;

char hostname[MAXHOSTNAMELEN+1];	/* for local hostname */

/* audit, capabilities & mandatory access control */
int    audit_enabled;	/* set iff sysconf(_SC_AUDIT) > 0 */
int    mac_enabled;	/* set iff sysconf(_SC_MAC) > 0 */
mac_t  mac_moldy;	/* mac label for reading :mac directories */
cap_t  ecap;		/* empty capability set */
size_t ncap = 2;	/* number of entries in capset */
cap_value_t capset[10] = {CAP_SETGID, CAP_SETUID};

/* user's default environment for the shell */
char homedir[5+PATH_MAX]="HOME=";
char path[5+PATH_MAX]="PATH=";
char shell[6+PATH_MAX]="SHELL=";
#define ENV_SIZE 64
char logname[8+ENV_SIZE]="LOGNAME=";
char env_user[6+ENV_SIZE]="USER=";
char tzone[256]="TZ=";
char *envinit[]={
	homedir,
	logname,
	env_user,
	path,
	shell,
	tzone,
	0};
unsigned int secfudge;	/* delay after the minute tick by this; this
			   lack of punctuality reduces etherstorms */
int reinit = 0;
int maxrun = MAXRUN;

extern char **environ;

/* at job pathname generation */
char *at_cmd_create(const struct event *);

/* abort processing on error */
void crabort(const char *, int);

/* MAC utility functions */
mac_t mac_swap(mac_t);
void  mac_restore(mac_t);
mac_t getflabel(const char *);
int   setflabel(const char *, const char *);

/* signal handlers */
void cronend(int);
void cronhup(int);
void timeout(int);

/* creation/deletion of at & cron jobs */
void del_atjob(const char *, const char *, mac_t);
void mod_atjob(const char *, mac_t);
void add_atevent(struct usr *, const char *, time_t, short, int);
void del_ctab(const char *, mac_t);
void mod_ctab(const char *, mac_t);
void rm_ctevents(struct usr *);

/* directory scanning for at & cron jobs */
void dscan(DIR *, void (*)(const char *, mac_t));

/* moldy subdirectory handling for at & cron jobs */
void read_cron_mac(const char *, mac_t);
void read_at_mac(const char *, mac_t);

/* job execution */
void ex(const struct event *);

/* crontab file parser */
void readcron(struct usr *);
char *next_field(int, int, const struct usr *);
time_t next_time(const struct event *);
int  next_ge(int, const char *);

/* queue definitions file parser */
void quedefs(int);
void parsqdef(const char *);

/* job loop */
void msg_wait(time_t, int);

/* job scheduling */
void resched(int);

/* job reaping & cleanup */
void idle(time_t);
void cleanup(struct runinfo *, int);

/* job search & match */
struct usr *find_usr(const char *, mac_t);

/* initialization */
void initialize(int);
void read_dirs(void);
int  maxfds(void);

/* logging & job notification*/
void logit(char, struct runinfo *, int);
void mail(const char *, const char *, const char *, int);
void msg(const char *, ...);

int
main(int argc, char **argv)
{
	time_t t, t_old;
	time_t last_time;
	time_t ne_time;		/* amt of time until next event execution */
	time_t lastmtime = 0L;
	struct usr *u, *u2;
	struct event *e, *e2, *eprev;
	struct stat buf;
	int c;
	pid_t rfork = 0;
	struct sigaction act;

	/* initialization for systems supporting MAC */
	if (mac_enabled = (sysconf(_SC_MAC) > 0)) {
		mac_t omac = mac_get_proc();
		if (omac == NULL) {
			fprintf(stderr, "unable to get process MAC label\n");
			exit(1);
		}
		mac_moldy = mac_set_moldy(omac);
		mac_free(omac);
		if (mac_moldy == NULL) {
			fprintf(stderr, "unable to get moldy MAC label\n");
			exit(1);
		}
		atdir = MACATDIR;

		capset[ncap++] = CAP_MAC_RELABEL_OPEN;
		capset[ncap++] = CAP_MAC_RELABEL_SUBJ;
		capset[ncap++] = CAP_MAC_READ;
		capset[ncap++] = CAP_MAC_MLD;
	}

	/* initialization for systems supporting audit */
	if (audit_enabled = (sysconf(_SC_AUDIT) > 0)) {
		capset[ncap++] = CAP_AUDIT_CONTROL;
		capset[ncap++] = CAP_AUDIT_WRITE;
	}

	/* check privileges of caller */
	if (cap_envp(0, ncap, capset) == -1) {
		fprintf(stderr, "insufficient privilege\n");
		exit(1);
	}

	/* increase number of open files to system max */
	if (maxfds() == -1) {
		fprintf(stderr, "unable to increase fd limit\n");
		exit(1);
	}

	/* create empty capability set for children */
	if ((ecap = cap_init()) == NULL) {
		perror("cap_init");
		exit(1);
	}

	for (c = 1; c < argc; c++) {
		if (!strcmp(argv[c], "nofork")) {
			rfork++;
			continue;
		} else if (!strcmp(argv[c], "-j")) {
			c++;
			if (c >= argc) {
				fprintf(stderr, "-j missing argument, ignored\n");
				continue;
			}
			sscanf(argv[c], "%d", &maxrun);
			if (maxrun < REALMINRUN || maxrun > REALMAXRUN) {
				fprintf(stderr,
					"-j must be between %d and %d\n",
					REALMINRUN, REALMAXRUN);
				exit(1);
			}
		} else {
			fprintf(stderr, "Unknown argument '%s' ignored\n",
				argv[c]);
		}
	}
	
	/*
	 * Allocate runtable
	 */
	rt = calloc(maxrun, sizeof(struct runinfo));
	if (!rt) {
		fprintf(stderr, "Unable to allocate runtable of size %d\n",
			maxrun);
		exit(1);
	}

	/*
	 *  We use LOG_NDELAY here because there may a out-of-memory
	 *  condition later which may require a syslog entry.  So, we want
	 *  to make sure we can open a direct path to the syslogd daemon.
	 */
	openlog("cron", LOG_PID|LOG_NDELAY|LOG_PERROR, LOG_DAEMON);

	if (!rfork) {
begin:
		if (rfork = fork()) {
			if (rfork == -1) {
				(void)sleep(30);
				goto begin; 
			}
			exit(0); 
		}
		didfork++;
		setpgrp();	/* detach cron from console */
	}
	

	umask(022);
	secfudge = (unsigned)gethostid() % MAXFUDGE;

	/* set up signal handling */
	act.sa_flags = 0;
	(void) sigemptyset(&act.sa_mask);
	act.sa_handler = SIG_IGN;
	(void) sigaction(SIGINT, &act, (struct sigaction *) NULL);
	(void) sigaction(SIGQUIT, &act, (struct sigaction *) NULL);
	act.sa_handler = cronhup;
	(void) sigaction(SIGHUP, &act, (struct sigaction *) NULL);
	act.sa_handler = cronend;
	(void) sigaction(SIGTERM, &act, (struct sigaction *) NULL);
	act.sa_handler = timeout;
	(void) sigaction(SIGALRM, &act, (struct sigaction *) NULL);

	if (gethostname(hostname, MAXHOSTNAMELEN) < 0)
		strcpy(hostname, "UNKNOWN");

	initialize(1);
	quedefs(DEFAULT);	/* load default queue definitions */
	msg("*** cron started ***   pid = %d", (int) getpid());
	t_old = time(0);
	last_time = t_old;
	for (;;) {			/* MAIN LOOP	*/
		t = time(0);
		if (reinit || t_old > t || t-last_time > CUSHION+MAXFUDGE) {
			reinit = 0;
			/* the time was set backwards or forward */
			el_delete();
			u = uhead;
			while (u!=NULL) {
				rm_ctevents(u);
				e = u->atevents;
				while (e!=NULL) {
					free(e->cmd);
					e2 = e->link;
					free(e);
					e = e2; 
				}
				u2 = u->nextusr;
				USR_RELE(u);
				u = u2; 
			}
			close(msgfd);
			initialize(0);
			t = time(0); 
		}
		t_old = t;
		if (next_event == NULL) {
			if (el_empty()) ne_time = INFINITY;
			else {	
				next_event = (struct event *) el_first();
				ne_time = next_event->time - t; 
			}
		} else {
			ne_time = next_event->time - t;
#ifdef DEBUG
			fprintf(stderr,"next_time=%ld  %s",
				next_event->time,ctime(&next_event->time));
#endif
		}
		if (ne_time > 0) {
			idle(ne_time);
			if (!next_event || reinit) {
				last_time = INFINITY;
				continue;
			}
		}
		if (stat(QUEDEFS,&buf)) {
			if (lastmtime != -1)
				msg("cannot stat %s", QUEDEFS);
			lastmtime = -1;
		} else {
			if(lastmtime != buf.st_mtime) {
				quedefs(LOAD);
				lastmtime = buf.st_mtime;
			}
		}
		last_time = next_event->time;	/* save execution time */
		ex(next_event);
		switch(next_event->etype) {
		/* add cronevent back into the main event list */
		case CRONEVENT:
			if(delayed) {
				delayed = 0;
				break;
			}
			next_event->time = next_time(next_event);
			el_add( next_event,next_event->time,
			    (next_event->u)->ctid ); 
			break;
		/* remove at or batch job from system */
		default:
			eprev=NULL;
			e=(next_event->u)->atevents;
			while (e != NULL)
				if (e == next_event) {
					if (eprev == NULL)
						(e->u)->atevents = e->link;
					else	eprev->link = e->link;
					free(e->cmd);
					free(e);
					break;	
				}
				else {	
					eprev = e;
					e = e->link; 
				}
			break;
		}
		next_event = NULL; 
	}
}


/*ARGSUSED*/
void
cronhup(int sig)
{
	fflush(stderr);
	if (freopen(ACCTFILE,"a",stdout) == NULL) {
		msg("cannot open %s", ACCTFILE);
	} else {
		close(fileno(stderr));
		dup2(1,2);
		msg("*** cron log restarted ***   pid=%d", (int) getpid());
	}
	reinit = 1;
}


void 
initialize(int firstpass)
{
	static int flag = 0;
	char *tz;
#ifdef DEBUG
	fprintf(stderr,"%s: in initialize\n",DEBUG_TIME());
#endif
	init_time = time((long *) 0);
	el_init(8,init_time,(long)(60*60*24),10);
	if(firstpass)
		if(access(FIFO,R_OK|W_OK) == -1) {
			if(errno == ENOENT) {
				if(mknod(FIFO,S_IFIFO|FIFOMODE,0)!=0)
					crabort("cannot create fifo queue",1);
				if (setflabel(FIFO, FIFOLBL) == -1)
					crabort("cannot set fifo label", 1);
			} 
			else  {
				if(NOFORK) {
					/* didn't fork... init(1M) is waiting */
					(void)sleep(60);
				}
				perror("FIFO");
				crabort("cannot access fifo queue",1);
			}
		}
		else {
			if(NOFORK) {
				/* didn't fork... init(1M) is waiting
				 * the wait is painful, but we don't
				 * want init respawning this quickly */
				(void)sleep(60);
			}
			crabort("cannot start cron; FIFO exists",0);
		}
	if((msgfd = open(FIFO, O_RDWR)) < 0) {
		perror("! open");
		crabort("cannot open fifo queue",1);
	}
	if (set_cloexec(msgfd) == -1) {
		perror("FIFO close-on-exec");
		crabort("cannot set close-on-exec for fifo queue", 1);
	}
	/* check for possible NULL or overflow */
	if ((tz = getenv("TZ")) == NULL)
		crabort("no TZ evironment variable", 1);
	if (strlen(tz) + 4 > sizeof(tzone))
		crabort("TZ evironment variable too long", 1);
	(void)sprintf(tzone,"TZ=%s",tz);
	(void)sprintf(path,"PATH=%s",_PATH_ROOTPATH);
	(void)sprintf(shell,"SHELL=%s",_PATH_BSHELL);

	/* read directories, create users list,
	   and add events to the main event list	*/
	uhead = NULL;
	read_dirs();
	next_event = NULL;
	if(flag)
		return;
	if(freopen(ACCTFILE,"a",stdout) == NULL)
		fprintf(stderr,"cannot open %s\n",ACCTFILE);
	close(fileno(stderr));
	dup(1);
	/* this must be done to make popen work....i dont know why */
	freopen("/dev/null","r",stdin);
	flag = 1;
}


void
read_dirs(void)
{
	DIR  *dirp;
	mac_t omac;

#ifdef DEBUG
	fprintf(stderr,"%s: Read Directories\n",DEBUG_TIME());
#endif
	if (chdir(CRONDIR) == -1)
		crabort(BADCD, 1);
	cwd = CRON;
	dirp = opendir(".");
	if (dirp != NULL) {
		dscan(dirp, mod_ctab);
		closedir(dirp);
	} else {
		crabort(NOREADDIR, 1);
	}
	if (mac_enabled) {
		omac = mac_swap(mac_moldy);
		if (chdir(MACCRONDIR) == -1)
			crabort(BADCD, 1);
		cwd = TRIX_CRON;
		dirp = opendir(".");
		if (dirp != NULL) {
			dscan(dirp, read_cron_mac);
			closedir(dirp);
		} else {
			mac_restore(omac);
			crabort(NOREADDIR, 1);
		}
		mac_restore(omac);
	}
	omac = mac_swap(mac_moldy);
	if (chdir(atdir) == -1) {
		mac_restore(omac);
		msg("Cannot chdir to at directory");
		return;
	}
	cwd = (mac_enabled ? TRIX_AT : AT);
	dirp = opendir(".");
	if (dirp != NULL) {
		dscan(dirp, mac_enabled ? read_at_mac : mod_atjob);
		closedir(dirp);
	} else {
		msg("cannot chdir to at directory");
	}
	mac_restore(omac);
}

void
dscan(DIR *df, void (*fp)(const char *, mac_t))
{
	struct dirent *dp;
	mac_t label;

	seekdir(df, (long) 0);
	for (dp = readdir(df); dp != NULL; dp = readdir(df)) {
		/* ignore filenames beginning with `:', such as :mac */
		if (mac_enabled && dp->d_name[0] == ':')
			continue;

		/* ignore `.' and `..' */
		if (dp->d_name[0] == '.' && (dp->d_name[1] == '\0' || (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))
			continue;

		label = getflabel(dp->d_name);
		(*fp)(dp->d_name, label);
		mac_free(label);
	}
}

void
read_mac(const char *name, mac_t label, void (*fp)(const char *, mac_t))
{
	DIR *dirp;
	mac_t omac;

	if (*name == '.' || *name == ':' || *name == '_')
		return;
	omac = mac_swap(label);
	if ((dirp = opendir(name)) == NULL) {
		mac_restore(omac);
		return;
	}
	if (chdir(name) == -1) {
		mac_restore(omac);
		return;
	}
	dscan(dirp, fp);
	closedir(dirp);
	mac_restore(omac);
	switch(cwd) {
		case CRON:
			(void) chdir(CRONDIR);
			break;
		case TRIX_CRON:
			(void) chdir(MACCRONDIR);
			break;
		case AT:
		case TRIX_AT:
			(void) chdir(atdir);
			break;
	}
}

void
read_cron_mac(const char *name, mac_t label)
{
	read_mac(name, label, mod_ctab);
}

void
read_at_mac(const char *name, mac_t label)
{
	read_mac(name, label, mod_atjob);
}

void
mod_ctab(const char *name, mac_t label)
{
	struct	clearance *clp;
	struct	passwd	*pw;
	struct	stat	buf;
	struct	usr	*u;
	char	namebuf[PATH_MAX];
	mac_t	omac;

	if((pw=getpwnam(name)) == NULL)
		return;
	if (mac_enabled) {
		if ((clp = sgi_getclearancebyname(name)) == NULL)
			return;
		if (mac_clearedlbl(clp, label) != MAC_CLEARED)
			return;
	}
	strcat(strcat(strcpy(namebuf, !mac_enabled || cwd == CRON ? CRONDIR : MACCRONDIR), "/"), name);
	omac = mac_swap(label);
	if(stat(namebuf,&buf)) {
		mail(name,BADSTAT,NULL,2);
		unlink(namebuf);
		mac_restore(omac);
		return;
	}
	if((u=find_usr(name, label)) == NULL) {
#ifdef DEBUG
		fprintf(stderr,"new user (%s) with a crontab\n",name);
#endif
		u = (struct usr *) xmalloc(sizeof(struct usr));
		u->name = xmalloc(strlen(name)+1);
		strcpy(u->name,name);
		u->home = xmalloc(strlen(pw->pw_dir)+1);
		strcpy(u->home,pw->pw_dir);
		u->uid = pw->pw_uid;
		u->gid = pw->pw_gid;
		if (mac_enabled) {
			u->mac = mac_dup(label);
			if (u->mac == NULL) {
				free(u->home);
				free(u->name);
				free(u);
				unlink(namebuf);
				mac_restore(omac);
				return;
			}
		} else {
			u->mac = NULL;
		}
		u->ctexists = TRUE;
		u->ctid = ecid++;
		u->ctevents = NULL;
		u->atevents = NULL;
#ifdef ATLIMIT
		u->aruncnt = 0;
#endif
#ifdef CRONLIMIT
		u->cruncnt = 0;
#endif
		u->nextusr = uhead;
		u->refcnt = 1;
		uhead = u;
		readcron(u);
	} else {
		u->uid = pw->pw_uid;
		u->gid = pw->pw_gid;
		if(strcmp(u->home,pw->pw_dir) != 0) {
			free(u->home);
			u->home = xmalloc(strlen(pw->pw_dir)+1);
			strcpy(u->home,pw->pw_dir);
		}
		u->ctexists = TRUE;
		if(u->ctid == 0) {
#ifdef DEBUG
			fprintf(stderr,"%s now has a crontab\n",u->name);
#endif
			/* user didnt have a crontab last time */
			u->ctid = ecid++;
			u->ctevents = NULL;
			readcron(u);
			mac_restore(omac);
			return;
		}
#ifdef DEBUG
		fprintf(stderr,"%s has revised his crontab\n",u->name);
#endif
		rm_ctevents(u);
		el_remove(u->ctid,0);
		readcron(u);
	}
	mac_restore(omac);
}


void
mod_atjob(const char *name, mac_t label)
{
	time_t	tim;
	struct	passwd	*pw;
	struct	stat	buf;
	struct	usr	*u;
	struct	event	*e;
	char	namebuf[PATH_MAX], *ptr;
	short	atqtype;
	int	smval;
	mac_t	omac;

	errno = 0;
	tim = (time_t) strtol(name, &ptr, 10);
	if (ptr == name || errno == ERANGE || *ptr != '.')
		return;
	ptr++;
	if ( ! (isascii(*ptr) && islower(*ptr)) )
		return;
	atqtype = *ptr - 'a';
	smval = (*++ptr == 'm');

	strcat(strcat(strcpy(namebuf, atdir), "/"), name);
	omac = mac_swap(label);
	if(stat(namebuf,&buf) || atqtype >= NQUEUE) {
		unlink(namebuf);
		mac_restore(omac);
		return;
	}
	if((buf.st_mode & ISUID) != ISUID) {
		unlink(namebuf);
		mac_restore(omac);
		return;
	}
	if((pw=getpwuid(buf.st_uid)) == NULL) {
		unlink(namebuf);
		mac_restore(omac);
		return;
	}
	if((u=find_usr(pw->pw_name, label)) == NULL) {
#ifdef DEBUG
		fprintf(stderr,"new user (%s) with an at job = %s\n",pw->pw_name,name);
#endif
		u = (struct usr *) xmalloc(sizeof(struct usr));
		u->name = xmalloc(strlen(pw->pw_name)+1);
		strcpy(u->name,pw->pw_name);
		u->home = xmalloc(strlen(pw->pw_dir)+1);
		strcpy(u->home,pw->pw_dir);
		u->uid = pw->pw_uid;
		if (mac_enabled) {
			u->mac = mac_dup(label);
			if (u->mac == NULL) {
				free(u->home);
				free(u->name);
				free(u);
				unlink(namebuf);
				mac_restore(omac);
				return;
			}
		} else {
			u->mac = NULL;
		}
		/* If the user has executed a newgrp, have the
		 * at job run with that group id.
		 */
		u->gid = buf.st_gid;
		u->ctexists = FALSE;
		u->ctid = 0;
		u->ctevents = NULL;
		u->atevents = NULL;
#ifdef ATLIMIT
		u->aruncnt = 0;
#endif
#ifdef CRONLIMIT
		u->cruncnt = 0;
#endif
		u->nextusr = uhead;
		u->refcnt = 1;
		uhead = u;
		add_atevent(u,name,tim,atqtype,smval);
	} else {
		u->uid = pw->pw_uid;
		/* If the user has executed a newgrp, have the
		 * at job run with that group id.
		 */
		u->gid = buf.st_gid;
		if(strcmp(u->home,pw->pw_dir) != 0) {
			free(u->home);
			u->home = xmalloc(strlen(pw->pw_dir)+1);
			strcpy(u->home,pw->pw_dir);
		}
		e = u->atevents;
		while(e != NULL)
			if(strcmp(e->cmd,name) == 0) {
				e->of.at.exists = TRUE;
				break;
			} else
				e = e->link;
		if (e == NULL) {
#ifdef DEBUG
			fprintf(stderr,"%s has a new at job = %s\n",u->name,name);
#endif
			add_atevent(u,name,tim,atqtype,smval);
		}
	}
	mac_restore(omac);
}


void
add_atevent(struct usr *u, const char *job, time_t tim, short atqtype, int sm)
{
	struct event *e;

	e=(struct event *) xmalloc(sizeof(struct event));
	e->etype = atqtype;
	e->cmd = xmalloc(strlen(job)+1);
	e->sm = sm;
	strcpy(e->cmd,job);
	e->u = u;
#ifdef DEBUG
	fprintf(stderr,"add_atevent: user=%s, job=%s, time=%ld\n",
		u->name,e->cmd, e->time);
#endif
	e->link = u->atevents;
	u->atevents = e;
	e->of.at.exists = TRUE;
	e->of.at.eventid = ecid++;
	if(tim < init_time)		/* old job */
		e->time = init_time;
	else
		e->time = tim;
	el_add(e, e->time, e->of.at.eventid); 
}


char line[CTLINESIZE];		/* holds a line from a crontab file	*/
int cursor;			/* cursor for the above line	*/

void
readcron(struct usr *u)
{
	/* readcron reads in a crontab file for a user (u).
	   The list of events for user u is built, and 
	   u->events is made to point to this list.
	   Each event is also entered into the main event list. */

	FILE *cf;			/* cf will be a user's crontab file */
	struct event *e;
	int start, i;
	char namebuf[PATH_MAX];

	/* read the crontab file */
	strcat(strcat(strcpy(namebuf, !mac_enabled || cwd == CRON ? CRONDIR : MACCRONDIR), "/"), u->name);
	if ((cf=fopen(namebuf,"r")) == NULL) {
		mail(u->name,NOREAD,NULL,2);
		return; 
	}
	while (fgets(line,CTLINESIZE,cf) != NULL) {
		/* process a line of a crontab file */
		cursor = 0;
		while(line[cursor] == ' ' || line[cursor] == '\t')
			cursor++;
		/* commented line OR empty line - ariel */
		if(line[cursor] == '#' || line[cursor] == '\n')
			continue;
		e = (struct event *) xmalloc(sizeof(struct event));
		e->etype = CRONEVENT;
		e->of.ct.minute = NULL;
		e->of.ct.hour = NULL;
		e->of.ct.daymon = NULL;
		e->of.ct.month = NULL;
		e->of.ct.dayweek = NULL;
		if ((e->of.ct.minute=next_field(0,59,u)) == NULL) goto badline;
		if ((e->of.ct.hour=next_field(0,23,u)) == NULL) goto badline;
		if ((e->of.ct.daymon=next_field(1,31,u)) == NULL) goto badline;
		if ((e->of.ct.month=next_field(1,12,u)) == NULL) goto badline;
		if ((e->of.ct.dayweek=next_field(0,6,u)) == NULL) goto badline;
		if (line[++cursor] == '\0') {
			mail(u->name,EOLN,NULL,1);
			goto badline; 
		}
		/* get the command to execute */
		/* skip leading white space */
		while (line[cursor] == ' ' || line[cursor] == '\t')
			cursor++;
		start = cursor;
again:
		while ((line[cursor]!='%')&&(line[cursor]!='\n')
		    &&(line[cursor]!='\0') && (line[cursor]!='\\')) cursor++;
		if(line[cursor] == '\\') {
			if (line[++cursor]=='%') {
				for (i=cursor; (line[i]!='\n')&&(line[i]!='\0');
				    i++) {
					line[i-1] = line[i];
				}
				line[i-1] = '\0';
			}
			else
				cursor++;
			goto again;
		}
		e->cmd = xmalloc(cursor-start+1);
		strncpy(e->cmd,line+start,cursor-start);
		e->cmd[cursor-start] = '\0';
		/* see if there is any standard input	*/
		if (line[cursor] == '%') {
			e->of.ct.input = xmalloc(strlen(line)-cursor+1);
			strcpy(e->of.ct.input,line+cursor+1);
			for (i=0; i<strlen(e->of.ct.input); i++)
				if (e->of.ct.input[i] == '%') e->of.ct.input[i] = '\n'; 
		}
		else e->of.ct.input = NULL;
		/* have the event point to it's owner	*/
		e->u = u;

		/* insert this event at the front of this user's event list   */
		e->link = u->ctevents;
		u->ctevents = e;

		/* set the time for the first occurance of this event	*/
		e->time = next_time(e);
		e->sm = 0;

		/* finally, add this event to the main event list	*/
		el_add(e,e->time,u->ctid);
#ifdef DEBUG
		(void)cftime(debug_tstring, "%D %T", &e->time);
		fprintf(stderr,"inserting cron event \"%s\" at %ld=%s\n",
			e->cmd,e->time, debug_tstring);
#endif
		continue;

badline: 
		free(e->of.ct.minute);
		free(e->of.ct.hour);
		free(e->of.ct.daymon);
		free(e->of.ct.month);
		free(e->of.ct.dayweek);
		free(e); 
	}

	fclose(cf);
}


/* mail a user a message.
 *	The common mail messages are the output of commands.  The
 *	processes which mail those messages are reaped like normal user
 *	processes.  The mail messages sent here are exceptional.
 *	For example, the crontab command should notice invalid commands.
 */
void
mail(const char *usrname, const char *mmsg, const char *arg, int format)
{
	FILE *mpipe;
	char *temp, *i;
	struct passwd *ruser_ids;
	pid_t fork_val;

#ifdef TESTING
	return;
#endif
	fork_val = fork();
	if (fork_val == -1) {
		msg("failed to fork for mail message");

	} else if (fork_val != 0) {
		(void)alarm(2);
		if (0 > waitpid(fork_val, (int *) NULL, 0))
			running++;
		(void)alarm(0);

	} else {
		if ((ruser_ids = getpwnam(usrname)) == 0) {
			msg("failed to find username \"%s\"for mail message",
			    usrname);
			exit(0);
		}
		if (setuid(ruser_ids->pw_uid) == -1)
			exit(1);
		if (cap_set_proc(ecap) == -1)
			exit(1);
		temp = xmalloc(strlen(MAIL)+strlen(usrname)+2);
		(void)strcat(strcat(strcpy(temp,MAIL)," "),usrname);
		mpipe = popen(temp,"w");
		free(temp);
		if (mpipe == NULL) {
			msg("failed to create pipe for mail message");
		} else {
			/* separate msg from sendmail headers */
			if (format == 3)
				fprintf(mpipe, "To: %s\nSubject: cron output\n\n", usrname);
			else 
				fprintf(mpipe, "To: %s\nSubject: cron error\n\n", usrname);
			if (format == 1) {
				i = strrchr(line,'\n');
				if (i != NULL) *i = ' ';
				fprintf(mpipe,
"Your crontab file has an error in the following entry:\n\t%s\n\t%s\nThis entry has been ignored.\n",
						   line,mmsg);
			} else if (arg) {
				fprintf(mpipe, "Cron: ");
				fprintf(mpipe, mmsg, arg);
			} else {
				fprintf(mpipe, "Cron: %s\n",mmsg);
			}
		}
		exit(0);
	}
}


char * 
next_field(int lower, int upper, const struct usr *u)
{
	/* next_field returns a pointer to a string which holds 
	   the next field of a line of a crontab file. If (numbers
	   in this field are out of range (lower..upper), or there
	   is a syntax error) then NULL is returned, and a mail message
	   is sent to the user telling him which line the error was in. */

	char *s;
	int num1, num2, start;

	while ((line[cursor]==' ') || (line[cursor]=='\t')) cursor++;
	start = cursor;
	if (line[cursor] == '\0') {
		mail(u->name,EOLN,NULL,1);
		return(NULL); 
	}
	if (line[cursor] == '*') {
		cursor++;
		if ((line[cursor]!=' ') && (line[cursor]!='\t')) {
			mail(u->name,UNEXPECT,NULL,1);
			return(NULL); 
		}
		s = xmalloc(2);
		strcpy(s,"*");
		return(s); 
	}
	for (;;) {
		if (!isdigit(line[cursor])) {
			mail(u->name,UNEXPECT,NULL,1);
			return(NULL); 
		}
		num1 = 0;
		do { 
			num1 = num1*10 + (line[cursor]-'0'); 
		}			while (isdigit(line[++cursor]));
		if ((num1<lower) || (num1>upper)) {
			mail(u->name,OUTOFBOUND,NULL,1);
			return(NULL); 
		}
		if (line[cursor]=='-') {
			if (!isdigit(line[++cursor])) {
				mail(u->name,UNEXPECT,NULL,1);
				return(NULL); 
			}
			num2 = 0;
			do { 
				num2 = num2*10 + (line[cursor]-'0'); 
			}				while (isdigit(line[++cursor]));
			if ((num2<lower) || (num2>upper)) {
				mail(u->name,OUTOFBOUND,NULL,1);
				return(NULL); 
			}
		}
		if ((line[cursor]==' ') || (line[cursor]=='\t')) break;
		if (line[cursor]=='\0') {
			mail(u->name,EOLN,NULL,1);
			return(NULL); 
		}
		if (line[cursor++]!=',') {
			mail(u->name,UNEXPECT,NULL,1);
			return(NULL); 
		}
	}
	s = xmalloc(cursor-start+1);
	strncpy(s,line+start,cursor-start);
	s[cursor-start] = '\0';
	return(s);
}


time_t 
next_time(const struct event *e)
{
	/* returns the integer time for the next occurance of event e.
	   the following fields have ranges as indicated:
	PRGM  | min	hour	day of month	mon	day of week
	------|-------------------------------------------------------
	cron  | 0-59	0-23	    1-31	1-12	0-6 (0=sunday)
	time  | 0-59	0-23	    1-31	0-11	0-6 (0=sunday)
	   NOTE: this routine is hard to understand. */

	struct tm *tm, otm, *ntm;
	int tm_mon, tm_mday, tm_wday, wday, m, min, h, hr, carry, day, days,
	    d1, day1, carry1, d2, day2, carry2, daysahead,
	    mon, yr, db, wd, today;
	time_t t;

	t = time(0);
again:
	tm = localtime(&t);
	otm = *tm;

	tm_mon = next_ge(tm->tm_mon+1,e->of.ct.month) - 1;	/* 0-11 */
	tm_mday = next_ge(tm->tm_mday,e->of.ct.daymon);		/* 1-31 */
	tm_wday = next_ge(tm->tm_wday,e->of.ct.dayweek);	/* 0-6  */
	today = TRUE;
	if ( (strcmp(e->of.ct.daymon,"*")==0 && tm->tm_wday!=tm_wday)
	    || (strcmp(e->of.ct.dayweek,"*")==0 && tm->tm_mday!=tm_mday)
	    || (tm->tm_mday!=tm_mday && tm->tm_wday!=tm_wday)
	    || (tm->tm_mon!=tm_mon)) today = FALSE;

	m = tm->tm_min+1;
	if ((tm->tm_hour + 1) <= next_ge(tm->tm_hour%24, e->of.ct.hour)) {
		m = 0;
	}
	min = next_ge(m%60,e->of.ct.minute);
	carry = (min < m) ? 1:0;
	h = tm->tm_hour+carry;
	hr = next_ge(h%24,e->of.ct.hour);
	carry = (hr < h) ? 1:0;
	if ((!carry) && today) {
		/* this event must occur today	*/
		if (tm->tm_min>min)
			t +=(time_t)(hr-tm->tm_hour-1)*CR_HOUR + 
			    (time_t)(60-tm->tm_min+min)*CR_MINUTE;
		else t += (time_t)(hr-tm->tm_hour)*CR_HOUR +
			(time_t)(min-tm->tm_min)*CR_MINUTE;
		t = t - (long)tm->tm_sec; 
		goto dstfix;
	}

	min = next_ge(0,e->of.ct.minute);
	hr = next_ge(0,e->of.ct.hour);

	/* calculate the date of the next occurance of this event,
	   which will be on a different day than the current day.	*/

	/* check monthly day specification	*/
	d1 = tm->tm_mday+1;
	day1 = next_ge((d1-1)%days_in_mon(tm->tm_mon,tm->tm_year)+1,e->of.ct.daymon);
	carry1 = (day1 < d1) ? 1:0;

	/* check weekly day specification	*/
	d2 = tm->tm_wday+1;
	wday = next_ge(d2%7,e->of.ct.dayweek);
	if (wday < d2) daysahead = 7 - d2 + wday;
	else daysahead = wday - d2;
	day2 = (d1+daysahead-1)%days_in_mon(tm->tm_mon,tm->tm_year)+1;
	carry2 = (day2 < d1) ? 1:0;

	/* based on their respective specifications,
	   day1, and day2 give the day of the month
	   for the next occurance of this event.	*/

	if ((strcmp(e->of.ct.daymon,"*")==0) && (strcmp(e->of.ct.dayweek,"*")!=0)) {
		day1 = day2;
		carry1 = carry2; 
	}
	if ((strcmp(e->of.ct.daymon,"*")!=0) && (strcmp(e->of.ct.dayweek,"*")==0)) {
		day2 = day1;
		carry2 = carry1; 
	}

	yr = tm->tm_year;
	if ((carry1 && carry2) || (tm->tm_mon != tm_mon)) {
		/* event does not occur in this month	*/
		m = tm->tm_mon+1;
		mon = next_ge(m%12+1,e->of.ct.month)-1;		/* 0..11 */
		carry = (mon < m) ? 1:0;
		yr += carry;
		/* recompute day1 and day2	*/
		day1 = next_ge(1,e->of.ct.daymon);
		db = days_btwn(tm->tm_mon,tm->tm_mday,tm->tm_year,mon,1,yr) + 1;
		wd = (tm->tm_wday+db)%7;
		/* wd is the day of the week of the first of month mon	*/
		wday = next_ge(wd,e->of.ct.dayweek);
		if (wday < wd) day2 = 1 + 7 - wd + wday;
		else day2 = 1 + wday - wd;
		if ((strcmp(e->of.ct.daymon,"*")!=0) && (strcmp(e->of.ct.dayweek,"*")==0))
			day2 = day1;
		if ((strcmp(e->of.ct.daymon,"*")==0) && (strcmp(e->of.ct.dayweek,"*")!=0))
			day1 = day2;
		day = (day1 < day2) ? day1:day2; 
	}
	else { /* event occurs in this month	*/
		mon = tm->tm_mon;
		if (!carry1 && !carry2) day = (day1 < day2) ? day1 : day2;
		else if (!carry1) day = day1;
		else day = day2;
	}

	/* now that we have the min,hr,day,mon,yr of the next
	   event, figure out what time that turns out to be.	*/

	days = days_btwn(tm->tm_mon,tm->tm_mday,tm->tm_year,mon,day,yr);
	t += (time_t)(23-tm->tm_hour)*CR_HOUR + (time_t)(60-tm->tm_min)*CR_MINUTE
	    + (time_t)hr*CR_HOUR + (time_t)min*CR_MINUTE + (time_t)days*CR_DAY;
	t = t-(long)tm->tm_sec;
dstfix:
	ntm = localtime(&t);
	if (otm.tm_isdst == ntm->tm_isdst)
		return(t);
	else if (otm.tm_isdst && !ntm->tm_isdst) {
		/* current time in dst, new is not */
		return(t - altzone + timezone);
	} else {
		/* current time not in dst, new is */
		t = t - timezone + altzone;

		/* Now, if we time happened to be in the missing hour..
		 * we skip this time and try again
		 */
		ntm = localtime(&t);
		if (ntm->tm_isdst == 0) {
			/* in the witching hour */
			t = t - altzone + timezone;
			goto again;
		}
		return(t);
	}
}

int 
next_ge(int current, const char *list)
{
	/* list is a character field as in a crontab file;
	   	for example: "40,20,50-10"
	   next_ge returns the next number in the list that is
	   greater than or equal to current.
	   if no numbers of list are >= current, the smallest
	   element of list is returned.
	   NOTE: current must be in the appropriate range.	*/

	const char *cursor = list;
	char *ptr;
	const int DUMMY = 100;
	int n, n2, min = DUMMY, min_gt = DUMMY;

	if (strcmp(list, "*") == 0)
		return(current);

	for (;;) {
		errno = 0;
		n = (int) strtol(cursor, &ptr, 10);
		if (ptr == cursor || errno == ERANGE || n == current)
			return(current);
		cursor = ptr;
		if (n < min)
			min = n;
		if (n > current && n < min_gt)
			min_gt = n;
		if (*cursor == '-') {
			errno = 0;
			n2 = (int) strtol(++cursor, &ptr, 10);
			if (ptr == cursor || errno == ERANGE)
				return(current);
			cursor = ptr;
			if (n2 > n) {
				if (current > n && current <= n2)
					return(current);
			} else {
				if (current > n || current <= n2)
					return(current);
			}
		}
		if (*cursor++ == '\0')
			break;
	}
	return (min_gt != DUMMY ? min_gt : min);
}

void
del_atjob(const char *name, const char *usrname, mac_t label)
{
	struct	event	*e, *eprev;
	struct	usr	*u;

	if((u = find_usr(usrname, label)) == NULL)
		return;
	e = u->atevents;
	eprev = NULL;
	while(e != NULL)
		if(strcmp(name,e->cmd) == 0) {
			if(next_event == e)
				next_event = NULL;
			if(eprev == NULL)
				u->atevents = e->link;
			else
				eprev->link = e->link;
			el_remove(e->of.at.eventid, 1);
			free(e->cmd);
			free(e);
			break;
		} else {
			eprev = e;
			e = e->link;
		}
	if(!u->ctexists && u->atevents == NULL) {
#ifdef DEBUG
		fprintf(stderr,"%s removed from usr list\n",usrname);
#endif
		if(ulast == NULL)
			uhead = u->nextusr;
		else
			ulast->nextusr = u->nextusr;
		USR_RELE(u);
	}
}


void 
del_ctab(const char *name, mac_t label)
{

	struct usr *u;

	if((u = find_usr(name, label)) == NULL)
		return;
	rm_ctevents(u);
	el_remove(u->ctid, 0);
	u->ctid = 0;
	u->ctexists = 0;
	if(u->atevents == NULL) {
#ifdef DEBUG
		fprintf(stderr,"%s removed from usr list\n",name);
#endif
		if(ulast == NULL)
			uhead = u->nextusr;
		else
			ulast->nextusr = u->nextusr;
		USR_RELE(u);
	}
}


void
rm_ctevents(struct usr *u)
{
	struct event *e2, *e3;

	/* see if the next event (to be run by cron)
	   is a cronevent owned by this user.		*/
	if ( (next_event!=NULL) && 
	    (next_event->etype==CRONEVENT) &&
	    (next_event->u==u) )
		next_event = NULL;
	e2 = u->ctevents;
	while (e2 != NULL) {
		free(e2->cmd);
		free(e2->of.ct.minute);
		free(e2->of.ct.hour);
		free(e2->of.ct.daymon);
		free(e2->of.ct.month);
		free(e2->of.ct.dayweek);
		free(e2->of.ct.input);
		e3 = e2->link;
		free(e2);
		e2 = e3; 
	}
	u->ctevents = NULL;
}


struct usr *
find_usr(const char *uname, mac_t label)
{
	struct usr *u;

	u = uhead;
	ulast = NULL;
	while (u != NULL) {
		if (strcmp(u->name,uname) == 0)
			if (!mac_enabled || mac_equal(label, u->mac) > 0)
				return(u);
		ulast = u;
		u = u->nextusr; 
	}
	return(NULL);
}


void
ex(const struct event *e)
{
	int i, fd, sp_flag;
	pid_t rfork;
	char *at_cmdfile, *cron_infile;
	struct stat buf;
	struct queue *qp;
	struct runinfo *rp;
	uid_t satid = e->u->uid;
	sigset_t set, oset;

	qp = &qt[e->etype];	/* set pointer to queue defs */

	if(qp->nrun >= qp->njob) {
		if (e->etype != CRONEVENT)
			msg("%c queue max run limit reached", e->etype + 'a');
		resched(qp->nwait);
		return;
	}
	for(rp=rt; rp < rt+maxrun; rp++) {
		if(rp->pid == 0)
			break;
	}
	if(rp >= rt+maxrun) {
		msg("maxrun (%d) procs reached", maxrun);
		resched(qp->nwait);
		return;
	}
#ifdef ATLIMIT
	if((e->u)->uid != 0 && (e->u)->aruncnt >= ATLIMIT) {
		msg("ATLIMIT (%d) reached for uid %d", ATLIMIT,
		    (int) (e->u)->uid);
		resched(qp->nwait);
		return;
	}
#endif
#ifdef CRONLIMIT
	if((e->u)->uid != 0 && (e->u)->cruncnt >= CRONLIMIT) {
		msg("CRONLIMIT (%d) reached for uid %d", CRONLIMIT,
		    (int) (e->u)->uid);
		resched(qp->nwait);
		return;
	}
#endif

	if ((cron_infile = tempnam(TMPDIR, PFX)) == NULL) {
		resched(wait_time);
		return;
	}

	/*
	 * Don't allow signals between creating the cron tmpfile
	 * and deleting it. Otherwise a spurious signal could
	 * occur after the file is created and before it is deleted,
	 * which would cause cron to not properly clean up after itself.
	 */
	(void) sigfillset(&set);
	(void) sigprocmask(SIG_BLOCK, &set, &oset);
	rp->outfd = open(cron_infile, O_CREAT|O_TRUNC|O_RDWR|O_APPEND, 0);
	if (rp->outfd != -1) {
		(void) unlink(cron_infile);
	} else {
		rp->outfd = open("/dev/null", O_RDWR, (mode_t) 0);
	}
	(void) sigprocmask(SIG_SETMASK, &oset, (sigset_t *) 0);
	free(cron_infile);
	if (rp->outfd == -1 || set_cloexec(rp->outfd) == -1) {
		if (rp->outfd != -1)
			close(rp->outfd);
		resched(wait_time);
		return;
	}

	if((rfork = fork()) == -1) {
		close(rp->outfd);
		msg("cannot fork");
		resched(wait_time);
		(void)sleep(30);
		return;
	}
	if(rfork) {	/* parent process */
		++qp->nrun;
		++running;
		rp->pid = rfork;
		rp->que = e->etype;
		rp->sm = e->sm;
#ifdef ATLIMIT
		if(e->etype != CRONEVENT)
			(e->u)->aruncnt++;
#endif
#if ATLIMIT && CRONLIMIT
		else
			(e->u)->cruncnt++;
#else
#ifdef CRONLIMIT
		if(e->etype == CRONEVENT)
			(e->u)->cruncnt++;
#endif
#endif
		rp->rusr = (e->u);
		
		/*
		 * Place a hold on the user structure.  This prevents
		 * it from being deleted out from under us while we
		 * wait for completion.  The reference will be freed
		 * in idle() when we return from wait().
		 */
		USR_HOLD(rp->rusr);

		logit((char)BCHAR,rp,0);
		return;
	}

	(void)close(0);

	/* restore rlimit signals */
	signal(SIGXCPU, SIG_DFL);
	signal(SIGXFSZ, SIG_DFL);

	/*
	 * Set the MAC label, should that be appropriate.
	 * Do it prior to reading an at job so that the moldy
	 * subdirectory is handled properly.
	 */
	mac_free(mac_swap(e->u->mac));

	if (e->etype != CRONEVENT ) {
		char *cp, *ptr;
		/* open jobfile as stdin to shell */
		at_cmdfile = at_cmd_create(e);
		if (stat(at_cmdfile,&buf)) exit(1);
		if (cp = strchr(at_cmdfile, '+')) {
			errno = 0;
			satid = (uid_t) strtol(++cp, &ptr, 10);
			if (ptr == cp || errno == ERANGE || *ptr != '\0') {
				unlink(at_cmdfile);
				exit(1);
			}
		}
		if ((buf.st_mode & ISUID) != ISUID) {
			/* if setuid bit off, original owner has 
			   given this file to someone else	*/
			unlink(at_cmdfile);
			exit(1); 
		}
		if (open(at_cmdfile, O_RDONLY) != 0) {
			mail((e->u)->name,BADJOBOPEN,NULL,2);
			unlink(at_cmdfile);
			exit(1); 
		}
		unlink(at_cmdfile);
		free(at_cmdfile);
	}

	/*
	 * Audit what happens henceforth on the user's behalf.
	 */
	if (audit_enabled) {
		/* Audit initiation of the job */
		if (satsetid(satid) == 0) {
			(void) ia_audit("CRON", e->u->name, 1, "New Session");
		} else {
			(void) ia_audit("CRON", e->u->name, 0, "No Session");
			exit(1);
		}
	}

#ifdef CRONPROJ
	/* Fire up a new array session */
	if (newarraysess() == -1) {
		exit(1);
	}
	{
		/* Set up the correct project ID */
		prid_t prid = getdfltprojuser(e->u->name);
		if (prid != (prid_t) -1 && setprid(prid) == -1)
			exit(1);
	}
#endif /* CRONPROJ */
        /*
         * set correct user and group identification and initialize
         * the supplementary group access list
         */
	(void) initgroups(e->u->name, e->u->gid);	/* ok if it fails */
        if (setgid(e->u->gid) == -1 || setuid(e->u->uid) == -1)
		exit(1);

	/* At this point relinquish all capability */
	if (cap_set_proc(ecap) == -1)
		exit(1);

	sp_flag = FALSE;
	if (e->etype == CRONEVENT) {
		/* check for standard input to command	*/
		if (e->of.ct.input != NULL) {
			char infile[sizeof(TMPINFILE)];

			strcpy(infile, TMPINFILE);
			cron_infile = mktemp(infile);
			(void) sigprocmask(SIG_BLOCK, &set, &oset);
			fd = open(cron_infile, O_CREAT|O_TRUNC|O_RDWR, 0);
			if (fd != 0) {
				if (fd != -1)
					(void) unlink(cron_infile);
				(void) sigprocmask(SIG_SETMASK, &oset,
						   (sigset_t *) 0);
				mail((e->u)->name, NOSTDIN, NULL, 2);
				exit(1); 
			}
			(void) unlink(cron_infile);
			(void) sigprocmask(SIG_SETMASK, &oset, (sigset_t *) 0);
			if (write(fd, e->of.ct.input, strlen(e->of.ct.input))
			    != strlen(e->of.ct.input)) {
				mail((e->u)->name, NOSTDIN, NULL, 2);
				exit(1); 
			}
			(void) lseek(fd, (off_t) 0, SEEK_SET);
		} else if (open("/dev/null",O_RDONLY)==-1) {
			open("/",O_RDONLY);
			sp_flag = TRUE; 
		}
	}

	/* redirect stdout and stderr for the shell */
	fflush(stderr);
	fflush(stdout);
	close(1);			/* redirect stdout */
	dup(rp->outfd);
	close(2);			/* redirect stderr */
	dup(rp->outfd);

	/* separate msg from sendmail headers */
	fprintf(stdout, MAIL_HDR, e->u->name,
		e->u->name, hostname, e->cmd);
	fflush(stdout);
	if (sp_flag) close(0);

	strcat(homedir,(e->u)->home);
	strcat(logname,(e->u)->name);
	strcat(env_user,(e->u)->name);
	environ = envinit;
	if (chdir((e->u)->home) == -1) {
		mail((e->u)->name,CANTCDHOME,(e->u)->home,2);
		exit(1); 
	}
#ifdef TESTING
	exit(1);
#endif
	if((e->u)->uid != 0)
		nice(qp->nice);
	if (e->etype == CRONEVENT)
		execl(SHELL,"sh","-c",e->cmd,0);
	else /* type == ATEVENT */
		execl(SHELL,"sh",0);
	mail((e->u)->name,CANTEXECSH,strerror(errno),2);
	exit(1);
}


void
idle(time_t t)
{
	time_t	now, t_old;
	pid_t	pid;
	int	prc;
	struct runinfo *rp;

	/*
	 * Guard against time being set backwards. If this occurs,
	 * no events will fire until that difference has been made up.
	 */
	now = time(0);
	t_old = now;
	while(t > 0L) {
		if (reinit)
			return;
		if (running) {
			if(t > wait_time)
				t = wait_time;
			else
				t += secfudge;
#ifdef DEBUG
			fprintf(stderr, "%s: alarm(%ld:%02ld) in idle;",
				DEBUG_TIME(), t/60, t%60);
#endif
			fflush(stderr);		/* get normal logging out */
			(void)alarm(t);
			pid = wait(&prc);
			t = (time_t) alarm(0);
#ifdef DEBUG
			fprintf(stderr,
				"\t%s: wait(&%x)=%d with %ld:%02ld left\n",
				DEBUG_TIME(), prc, pid, t/60,t%60);
#endif
			now = time(0);
			if (now < t_old) {
				msg("Time went backwards");
				reinit = 1;
				if (pid == -1)
					return;
			}
			t_old = now;
			if (next_event)
				t = next_event->time - now;
			if (pid == -1) {
				msg_wait(t, 0);
			} else {
				for(rp=rt;rp < rt+maxrun; rp++)
					if(rp->pid == pid)
						break;
				if(rp >= rt+maxrun) {
					msg("unexpected pid returned %d (ignored)", (int) pid);
					/* incremented in mail() */
					running--;
				} else {
					struct usr *u = rp->rusr;
					if(rp->que == ZOMB) {
						running--;
						rp->pid = 0;
						close(rp->outfd);
						USR_RELE(u);
					} else {
						cleanup(rp,prc);
						if (rp->pid == 0)
							USR_RELE(u);
						msg_wait(0, 1);
					}
				}
			}
		} else {
			msg_wait(t, 0);
		}
		now = time(0);
		if (now < t_old) {
			msg("Time went backwards");
			reinit = 1;
		}
		if (!next_event)
			return;
		t_old = now;
		t = next_event->time - now;
	}
}

/*
 * print_summary
 *	print a summary of the command that was run to ease debugging
 */

void
print_summary(FILE * fp, struct runinfo *rip, int rc)
{
	char		start_time[30];
	char		end_time[30];
	char *		cron_or_at;

	cron_or_at = (rip->etype == CRONEVENT) ? "cron" : "at";

	fprintf(fp,
	    "\n\n*************************** %s ****************************\n"
		"The above message is the standard output and standard error\n"
		"of the following %s job:\n\n"
		"command:    %s%s\n"
		"user@host:  %s@%s\n",
			cron_or_at, cron_or_at,
			rip->cmd,
			((rip->etype == CRONEVENT) ?
				"" :
				" (at temporary file)"),
			rip->rusr->name, hostname);

	if (rip->etype == CRONEVENT) {
		fprintf(fp,
			"crontab:    %s/%s\n"
			"homedir:    %s\n",
				CRONDIR, rip->rusr->name,
				rip->rusr->home);
	}
	fprintf(fp,
		"started:    %s"
		"ended:      %s",
			ctime_r((const time_t *)&rip->start_time, start_time),
			ctime_r((const time_t *)&rip->end_time, end_time));

	if (WIFEXITED(rc) && WEXITSTATUS(rc) != 0)
		fprintf(fp, "exit-code:  %d\n", WEXITSTATUS(rc));
	else if (WIFSIGNALED(rc))
		fprintf(fp, "signaled:   %d\n", WTERMSIG(rc));
	else if (WIFSTOPPED(rc))
		fprintf(fp, "stopped:    %d\n", WSTOPSIG(rc));
	fflush(fp);
}

void
cleanup(struct runinfo *pr, int rc)
{
	FILE	*fp;
	struct	usr	*p;
	struct	stat	buf;
	mac_t	omac;

	logit((char)ECHAR,pr,rc);
	--qt[pr->que].nrun;
	pr->pid = 0;
	--running;
	p = pr->rusr;
#ifdef ATLIMIT
	if(pr->que != CRONEVENT)
		--p->aruncnt;
#endif
#if ATLIMIT && CRONLIMIT
	else
		--p->cruncnt;
#else
#ifdef CRONLIMIT
	if(pr->que == CRONEVENT)
		--p->cruncnt;
#endif
#endif
	if (fstat(pr->outfd, &buf) == -1) {
		close(pr->outfd);
		return;
	}

	omac = mac_swap(p->mac);
	/*
	 * Did this file get any output from cmd?
	 * each %s is 2 chars (2*4=8) terminating \0 is 1
	 */
	if (buf.st_size > sizeof(MAIL_HDR) - 9 + 2 * strlen(p->name) +
	    strlen(hostname) + strlen(pr->cmd)) {

		/* mail user stdout and stderr */
		pr->que = ZOMB;
		pr->pid = fork();

		/* child: mail job output to user as that user */
		if (pr->pid == 0) {
			/* set uid to user's uid */
			if (setuid(p->uid) == -1)
				exit(127);

			/* relinquish all capability */
			if (cap_set_proc(ecap) == -1)
				exit(127);

			/* open stream on job output file */
			if ((fp = fdopen(pr->outfd, "r+")) == NULL) {
				mail(p->name, STDOUTERR, NULL, 2);
				exit(127);
			}

			/* append job summary to user output */
			print_summary(fp, pr, rc);

			/* move file offset to beginning */
			rewind(fp);

			/* dup output file onto stdin */
			close(0);
			if (dup(pr->outfd) != 0)
				exit(127);
			fclose(fp);

			/* send mail */
			execl(MAIL, "mail", p->name, (char *) 0);
			exit(127);
		}

		if (pr->pid != -1) {
			/* parent: indicate running job */
			running++;
		} else {
			/* fork failed: clean up job slot */
			pr->pid = 0;
			close(pr->outfd);
		}
	} else {
		/* No output from command */
		if (pr->sm) /* user asked to send a mail */
			mail(p->name, STDATMSG, NULL, 3);

		close(pr->outfd);
	}
	mac_restore(omac);
}


void
msg_wait(time_t t, int multi)
{
	struct	message msgbuf;
	struct	stat msgstat;
	int	cnt, msgs, i;
	mac_t	lbl;

	if(fstat(msgfd,&msgstat) != 0)
		crabort("cannot stat fifo queue",1);
	if(msgstat.st_size == 0 && running)
		return;

	if (t > 1) t += secfudge;
	else t = 1;
#ifdef DEBUG
	fprintf(stderr, "%s: alarm(%ld:%02ld) in msg_wait;",
		DEBUG_TIME(), t/60, t%60);
#endif
	fflush(stderr);			/* get normal logging out */

	/* read in each entry */
	if (!(msgs = msgstat.st_size / sizeof(msgbuf)) && !multi)
		msgs = 1;
	for (i = 0; i < msgs; i++) {
		msgbuf.etype = 0;
		(void)alarm(t);
		cnt = read(msgfd, &msgbuf, sizeof(msgbuf));
#ifdef DEBUG
		t = (time_t) alarm(0);
		fprintf(stderr, "\t%s: read()=%ld with %ld:%02ld left\n",
			DEBUG_TIME(), (unsigned long) cnt, t/60, t%60);
#else
		(void)alarm(0);
#endif
		if (cnt != sizeof(msgbuf))
			return;

		lbl = NULL;
		switch(msgbuf.etype) {
			case TRIX_AT:
				lbl = mac_from_text(msgbuf.label);
				if (lbl == NULL)
					break;
			case AT:
				switch(msgbuf.action)
				{
					case ADD:
						mod_atjob(msgbuf.fname, lbl);
						break;
					case DELETE:
						del_atjob(msgbuf.fname,
							  msgbuf.logname, lbl);
						break;
					default:
						msg("message received - bad action");
						break;
				}
				mac_free(lbl);
				break;
			case TRIX_CRON:
				lbl = mac_from_text(msgbuf.label);
				if (lbl == NULL)
					break;
			case CRON:
				switch(msgbuf.action)
				{
					case ADD:
						mod_ctab(msgbuf.fname, lbl);
						break;
					case DELETE:
						del_ctab(msgbuf.fname, lbl);
						break;
					default:
						msg("message received - bad action");
						break;
				}
				mac_free(lbl);
				break;
			default:
				msg("message received - bad event");
				break;
		}
		if (next_event != NULL) {
			if (next_event->etype == CRONEVENT)
				el_add(next_event,next_event->time,
				       (next_event->u)->ctid);
			else /* etype == ATEVENT */
				el_add(next_event,next_event->time,
				       next_event->of.at.eventid);
			next_event = NULL;
		}
		fflush(stdout);
	}
}


/*ARGSUSED*/
void
timeout(int sig)
{
}

/*ARGSUSED*/
void
cronend(int sig)
{
	msg("SIGTERM");
	if(unlink(FIFO) < 0)	/* FIFO vanishes when cron finishes */
		perror("cron could not unlink FIFO");
	msg("******* CRON ABORTED ********");
	exit(1);
}

void
crabort(const char *mssg, int clean)
{
	/* crabort handles exits out of cron */
	int c;

	if(clean) {
		if(unlink(FIFO) < 0)	/* FIFO vanishes when cron finishes */
			perror("cron could not unlink FIFO");
	}
	/* write error msg to console */
	if ((c=open(_PATH_CONSOLE,O_WRONLY))>=0) {
		write(c,"cron aborted: ",14);
		write(c,mssg,strlen(mssg));
		write(c,"\n",1);
		close(c); 
	}
	msg(mssg);
	msg("******* CRON ABORTED ********");
	exit(1);
}

void
msg(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr,"! ");
	va_start(ap, fmt);
	fprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr," %s\n", DEBUG_TIME());
	fflush(stdout);
}


void
logit(char cc, struct runinfo *rp, int rc)
{
	time_t t;

	t = time(0);
	if(cc == BCHAR) {
		fprintf(stderr,"%c  CMD: %s\n",cc, next_event->cmd);
		free(rp->cmd);
		rp->cmd = xmalloc(strlen(next_event->cmd) + 1);
		strcpy(rp->cmd, next_event->cmd);
		rp->etype = next_event->etype;
		rp->start_time = t;
	} else if (cc == ECHAR) {
		rp->end_time = t;
	}
	fprintf(stderr,"%c  %.8s %d %c %.24s",
		cc,(rp->rusr)->name, rp->pid, QUE(rp->que),ctime(&t));
	if (WIFSIGNALED(rc))
		fprintf(stderr, " ts=%d", WTERMSIG(rc));
	if (WIFSTOPPED(rc))
		fprintf(stderr, " ss=%d", WSTOPSIG(rc));
	if (WIFEXITED(rc) && WEXITSTATUS(rc) != 0)
		fprintf(stderr, " rc=%d", WEXITSTATUS(rc));
	putchar('\n');
	fflush(stdout);
}


void
resched(int delay)
{
	time_t nt;

	/* run job at a later time */
	nt = next_event->time + delay;
	if(next_event->etype == CRONEVENT) {
		next_event->time = next_time(next_event);
		if(nt < next_event->time)
			next_event->time = nt;
		el_add(next_event,next_event->time,(next_event->u)->ctid);
		delayed = 1;
		msg("rescheduling a cron job");
		return;
	}
	add_atevent(next_event->u, next_event->cmd, nt, next_event->etype, 
		    next_event->sm);
	msg("rescheduling at job");
}

#define	QBUFSIZ		80

void
quedefs(int action)
{
	int	i, j;
	char	name[MAXNAMLEN+1];
	char	qbuf[QBUFSIZ];
	FILE	*fp;

	/* set up default queue definitions */
	for(i=0;i<NQUEUE;i++) {
		qt[i].njob = qd.njob;
		qt[i].nice = qd.nice;
		qt[i].nwait = qd.nwait;
	}
	if(action == DEFAULT)
		return;
	if((fp = fopen(QUEDEFS,"r")) == NULL) {
		msg("cannot open %s; using default queue definitions",
		    QUEDEFS);
		return;
	}
	while(fgets(qbuf, QBUFSIZ, fp) != NULL) {
		if((j=qbuf[0]-'a') < 0 || j >= NQUEUE || qbuf[1] != '.')
			continue;
		i = 0;
		while(qbuf[i] != NULL) {
			name[i] = qbuf[i];
			i++;
		}
		/* Append a NULL at the end of name as otherwise DEFAULT 
		   values aren't getting picked up. It uses the old values
		   in the buffer which may be the previous QUEUE's info. or
		   possibly garbage.
		*/
		name[i] = NULL;

		parsqdef(&name[2]);
		qt[j].njob = qq.njob;
		qt[j].nice = qq.nice;
		qt[j].nwait = qq.nwait;
	}
	fclose(fp);
}


void 
parsqdef(const char *name)
{
	int i;

	qq = qd;
	while(*name) {
		i = 0;
		while(isdigit(*name)) {
			i *= 10;
			i += *name++ - '0';
		}
		switch(*name++) {
		case JOBF:
			qq.njob = i;
			break;
		case NICEF:
			qq.nice = i;
			break;
		case WAITF:
			qq.nwait = i;
			break;
		}
	}
}

char *
at_cmd_create(const struct event *e)
{
	char *cp = xmalloc(strlen(atdir) + strlen(e->cmd) + 2);

	sprintf(cp, "%s/%s", atdir, e->cmd);
	return (cp);
}

mac_t
mac_swap(mac_t mac)
{
	mac_t omac = NULL;

	if (mac_enabled && mac != NULL) {
		if ((omac = mac_get_proc()) != NULL) {
			if (mac_set_proc(mac) == -1) {
				mac_free(omac);
				omac = NULL;
			}
		}
	}
	return(omac);
}

void
mac_restore(mac_t mac)
{
	if (mac_enabled && mac != NULL) {
		(void) mac_set_proc(mac);
		mac_free(mac);
	}
}

mac_t
getflabel(const char *file)
{
	return(mac_enabled ? mac_get_file(file) : NULL);
}

int
setflabel(const char *file, const char *lbl_name)
{
	mac_t label;
	int r = 0;

	if (mac_enabled) {
		if ((label = mac_from_text(lbl_name)) != NULL) {
			r = mac_set_file(file, label);
			mac_free(label);
		} else {
			r = -1;
		}
	}
	return(r);
}

int
maxfds(void)
{
	struct rlimit lim;

	if (getrlimit(RLIMIT_NOFILE, &lim) == -1)
		return(-1);
	if (lim.rlim_cur == lim.rlim_max)
	    return(0);
	lim.rlim_cur = lim.rlim_max;
	return(setrlimit(RLIMIT_NOFILE, &lim));
}
