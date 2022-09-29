/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)login:login.c	1.43.6.83"

/*	Copyright (c) 1987, 1988 Microsoft Corporation */
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/***************************************************************************
 * Command:	login
 *
 * Usage:	login [[-p] name [env-var ... ]]
 *
 *
 * Files:	/etc/utmp
 *		/etc/wtmp
 * 		/etc/dialups
 *		/etc/d_passwd
 *	 	/var/adm/lastlog
 *		/var/adm/loginlog
 *		/etc/default/login
 *
 * Notes:	Conditional assemblies:
 *		NO_MAIL	causes the MAIL environment variable not to be set
 *			specified by CONSOLE.  CONSOLE MUST NOT be defined as
 *			either "/dev/syscon" or "/dev/systty"!!
 *		MAXTRYS is the number of attempts permitted.  0 is "no limit".
 *		AUX_SECURITY enables authentication through external programs
 *			instead of encrypted passwords.
 ***************************************************************************/
/* LINTLIBRARY */
#include <sys/types.h>
#include <utmpx.h>
#include <signal.h>
#include <pwd.h>
#include <ctype.h>
#include <syslog.h>
#include <proj.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>			/* For logfile locking */
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <utime.h> 
#include <termio.h>
#include <sys/stropts.h>
#include <shadow.h>			/* shadow password header file */
#include <time.h>
#include <sys/param.h> 
#include <sys/fcntl.h>
#include <deflt.h>
#include <grp.h>
#include <ia.h>
#include <libgen.h>
#include <stdlib.h>
#include <sat.h>
#include <mls.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <capability.h>
#include <sys/file.h> 			/* for flock in update_count() */

#include <errno.h>
#include <lastlog.h>
#include <locale.h>
#include <pfmt.h>
#include <sys/stream.h>
#include <paths.h>
#include <sys/quota.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <di_aux.h>
#include <limits.h>

#if defined(_SHAREII) || defined(DCE)
#include <dlfcn.h>
#endif /* defined(_SHAREII) || defined(DCE) */
#ifdef _SHAREII
#include <shareIIhooks.h>
SH_DECLARE_HOOK(SETMYNAME);
SH_DECLARE_HOOK(LOGIN);
#endif /* _SHAREII */

#define	LANG_FILE	"/.lang"	/* default lang file */
#define	TZ_FILE		"/.timezone"	/* default timezone file */

#ifdef AUX_SECURITY

#define SITE_OK 0
#define SITE_FAIL 1
#define SITE_AGAIN 2
#define SITE_CONTINUE 3

#endif /* AUX_SECURITY */

/*
 * Lastlog structure from 4.0 release.
 */
struct lastlog4 {
	int 	ll_status;		/* boolean to indicate login success 
					 * or failure. */
	time_t	ll_time;		
	char	ll_line[12];		/* if local: tty name,
					 * if rlogin: remote user name. */
	char	ll_host[64];
};

/*
 * Lastlog structure from 5.0 alpha releases.
 */
struct lastlog5a {
	time_t	ll_time;
	char	ll_line[12];		/* same as in utmp */
	char	ll_host[16];		/* same as in utmp */
	ulong	ll_level;		/* MAC security level */
};

/*
 * The following defines are macros used throughout login.
*/

#define	SCPYN(a, b)		(void) strncpy((a), (b), (sizeof((a))-1))
#define	EQN(a, b)		(!strncmp((a), (b), strlen(a)))
#define	ENVSTRNCAT(to, from)	{size_t deflen; deflen = strlen(to);\
				(void) strncpy((to) + deflen, (from),\
				 sizeof(to) - (1 + deflen));}
/*
 * The following defines are for different files.
*/

#define	SHELL		_PATH_BSHELL
#define	SHELL2		"/sbin/sh"
#define	LASTLOG		_PATH_LASTLOG
#define	LOGINLOG	_PATH_LOGINLOG
#define	DIAL_FILE	_PATH_DIALUPS
#define	DPASS_FILE	_PATH_DIALPASS
#define BADLOGDIR 	"/var/adm/badlogin"  /* used by update_count() */
#ifdef EXECSH
#define	SUBLOGIN	"<!sublogin>"

#define	NMAX		32
#endif /* EXECSH */

/*
 * The following defines are for MAXIMUM values.
*/

#define	MAXENV		     1024
#define	MAXLINE		      256
#define	MAXTIME		       60	/* default */
#define	MAXTRYS		        3	/* default */
#define	MAXARGS		       63
#define	MAX_TIMEOUT	(15 * 60)
#define	MAX_FAILURES 	       20	/* MAX value LOGFAILURES */

/*
 * The following defines are for DEFAULT values.
*/

#define	DEF_TZ				      "PST8PDT"
#define	DEF_HZ				          "100"
#define	DEFUMASK				   077
#define	DEF_PATH				_PATH_USERPATH
#define	DEF_SUPATH				_PATH_ROOTPATH
#define	DEF_TIMEOUT				    60
#define	DEF_LANG				"C"

/*
 * The following defines don't fit into the MAXIMUM or DEFAULT
 * categories listed above.
*/

#define	PBUFSIZE   PASS_MAX	/* max significant chars in a password */
#define	SLEEPTIME	   1	/* sleeptime before login incorrect msg */
#define	LNAME_SIZE	  32	/* size of logname */
#define	TTYN_SIZE	  15	/* size of logged tty name */
#define	TIME_SIZE	  30	/* size of logged time string */
#define	L_WAITTIME	   5	/* waittime for log file to unlock */
#define	DISABLETIME	  20	/* seconds login disabled after LOGFAILURES or 
				   MAXTRYS unsuccesful attempts. */
#define	LOGFAILURES	   3 	/* default */

#define	ENT_SIZE	  (LNAME_SIZE + TTYN_SIZE + TIME_SIZE + 3)
#define NAME_DELIM         32   /* LOCKOUTEXEMPT name delimiter */

#define MAC_SESSION	0
#define MAC_CLEARANCE	1

/* XXX from libcmd */
extern  FILE	*defopen(char *);
extern  char *defread(FILE *, char *);
extern char *sttyname(struct stat *);

/* XXX from libc */
extern int __ruserok_x(char *, int, char *, char *, uid_t, char *);

#ifdef DCE
int dce_verify(char *user, uid_t uid, gid_t gid, char *passwd, char **msg);
#endif /* DCE */

static	char	u_name[LNAME_SIZE],
		term[5+64] = {""},		/* "TERM=" */
		hertz[10] = { "HZ=" },
		timez[100] = { "TZ=" },
#ifdef EXECSH
		minusnam[16] = {"-"},
#endif /* EXECSH */
		env_remotehost[257+12] = { "REMOTEHOST=" },
		env_remotename[12+NMAX] = { "REMOTEUSER=" },
		user[LNAME_SIZE+6] = { "USER=" },
		path[1024] = { "PATH=" },
		home[1024] = { "HOME=" },
		shell[1024] = { "SHELL=" },
		logname[LNAME_SIZE + 8] = {"LOGNAME="},
#ifndef	NO_MAIL
		mail[LNAME_SIZE + 15] = { "MAIL=" },
#endif
		lang[NL_LANGMAX + 8] = { "LANG=" },
		*incorrectmsg = "Login incorrect\n",
		*incorrectmsgid = ":309",
		*envinit[10 + MAXARGS] = {home, path, logname, hertz, timez, term, user, lang, 0, 0};

static	char	zero[]	= { "" };

static	char 	*ttyn		= zero,
		*rttyn		= zero,
	 	*Def_tz		= zero,
		*Console	= zero,
		*Passreq	= zero,
		*Altshell	= zero,
		*Mandpass	= zero,
		*Initgroups	= zero,
		*SVR4_Signals   = zero,
		*Def_path	= zero,
		*Def_term	= zero,
	 	*Def_hertz	= zero,
		*Def_syslog	= zero,
#ifdef AUX_SECURITY
		*Def_sitepath	= zero,
#endif
		*Def_lang	= zero,
		*Def_supath	= zero,
                *Def_notlockout = zero;


static	unsigned Def_timeout	= DEF_TIMEOUT;

static	mode_t	Umask		= DEFUMASK;

static	long	Def_maxtrys	= MAXTRYS,
		Def_slptime	= SLEEPTIME,
		Def_distime	= DISABLETIME,
		Def_failures	= LOGFAILURES,
		Mac_Remote	= MAC_SESSION,
		Lockout		= 0;

static	int	pflag		=  0,	/* bsd: don't destroy the environ */
#ifdef EXECSH
		rflag		=  0,
		rflag_set	=  0,
#endif /* EXECSH */
		pwflag		=  0,	/* svr4: prompt for new password */
		hflag		=  0,
		intrupt		=  0,
		Idleweeks	= -1;

static	void donothing(void);
static int	set_uthost(char *, int);
static void getstr(char *, int, char *);
static	int dialpass(char *shellp, uid_t priv_uid);
static	int gpass(char *prmt, char *pswd, uid_t priv_uid);
static	char ** chk_args(char **pp);
static char ** getargs(char *inline);
static char ** getargs2(char *inline);
static int quotec(void);
static char * quotec2(char *s, int *cp);
static int legalenvvar(char *s);
static	void badlogin(int trys, char **log_entry);
static	char *mygetpass(char *prompt);
static	char * fgetpass(FILE *fi, FILE *fo, char *prompt);
static void catch(void);
static	void uppercaseterm(char *strp);
static	char * findttyname(int fd);
static	void init_defaults(void);
static int exec_pass(char *usernam);
static int doremotelogin(char *host);
static int doremoteterm(char *term);
static	int get_options(int argc, char **argv);
static	void usage(void);
static	int do_lastlog(struct utmpx *utmp);
static	void setup_environ(char **envp, char **renvp, char *dirp, char **shellp);
static	void pr_msgs(int lastlog_msg);
static	void update_utmp(struct utmpx *utmp);
static	void verify_pwd(int nopass, uid_t priv_uid);
static	int init_badtry(char **log_entry);
static	void logbadtry(int trys, char **log_entry);
static	int on_console(uid_t priv_uid);
static	int read_pass(uid_t priv_uid, int *nopass);
static	long get_logoffval(void);
static	int at_shell_level(void);
static void failure_log(void);
static int set_uthost(char *ut_hostp, int uthlen);
static unsigned char dositecheck(void);
static char * getsenv(char *varname, char **envp);
static void count_badlogins(int outcome, char *username);
static int update_count(int outcome, char *user);
static void early_advice(int rflag, int hflag);
static int cap_user_cleared(const char *, const cap_t);

static	uinfo_t	uinfo;

static	uid_t	ia_uid;
static	gid_t	ia_gid;
static	char	*ia_pwdpgm;

static	struct	lastlog ll;
static	int	syslog_fail;
static	int	syslog_success;

static	char	*args[MAXARGS];		/* pointer to arguments in envbuf[] */
static	char	envbuf[MAXLINE];	/* storage for argument text */

#ifdef EXECSH
static	char	QUOTAWARN[] = _PATH_QUOTA;
/* usererr is more like "remote_user_must_give_pwd", -1=yes, 0=no */
static	int	usererr = -1;

char	**remenvp = (char **)0;
char	luser[MAXLINE + 1];
char    rusername[NMAX+1], lusername[NMAX+1];
char    rpassword[NMAX+1];
char    remotehost[257];
char    terminal[64];

extern	char	**environ;
#endif /* EXECSH */
#ifdef AFS
extern int (*__afs_getsym(char *))();
extern int __afs_iskauth(void);
int (*afs_verify)(char *, char *, long *, int);
char *(*afs_gettktstring)(void);
long afs_exp = -1;
char afs_tktfile[1024] = { "KRBTKFILE=" };
char afs_passwdexp[22+12];
#endif
#ifdef DCE
static int	dfs_authentication = 0;
char		dce_tktfile[1024] = { "KRB5CCNAME=" };
#endif /* DCE */

/*
 * look for LANG= as argument and switch to that locale
 * last LANG= wins
 */
static char *
set_lang(char **envp)
{
	register char **ulp, *langp = (char *)0;

	if(ulp = envp) {
	    for(; *ulp; ulp++) {
		if( !strncmp(*ulp, "LANG=", 5))
		    langp = *ulp + 5;
	    }
	}
	if(langp)
	    (void)setlocale(LC_ALL, langp);
	return(langp);
}

/*
 * Procedure:	main
 *
 */
int
main(int argc, char **argv, char **renvp)
{
	register uid_t	priv_uid;

	register int
			trys = 0,		/* value for login attempts */
			lastlogok,
			writelog = 0,
			firstime = 1,
			uinfo_open = 0;

	static struct	utmpx	utmp;		/* static zeros the struct */

	char	**envp,
		*ia_dirp,
		*pdir = NULL,
		*ia_shellp,
		*pshell = NULL,
		*ttyprompt = NULL,
		*pwdmsgid = ":308",
		*pwdmsg = "Password:",
		inputline[MAXLINE],
		*log_entry[MAX_FAILURES],
		*loginmsg = "login: ",
		*loginmsgid = ":307";

	long	ia_expire,
		log_attempts = 0;
	int	log_trys = 0,		/* value for writing to logfile */
		nopassword = 1;

	struct	spwd	noupass = { "", "no:password" };

	cap_t	u_cap = (cap_t) NULL, ocap;
	mac_t	u_mac = (mac_t) NULL;
	cap_value_t	capv;

#ifdef EXECSH
	char	*endptr;
	int	i;
#endif /* EXECSH */

	if (cap_envl(0, CAP_AUDIT_CONTROL, CAP_AUDIT_WRITE, CAP_CHROOT,
		     CAP_DAC_WRITE, CAP_FOWNER, CAP_MAC_DOWNGRADE,
		     CAP_MAC_RELABEL_SUBJ, CAP_MAC_UPGRADE, CAP_MAC_WRITE,
		     CAP_PRIV_PORT, CAP_SETGID, CAP_SETUID, CAP_SETPCAP,
		     (cap_value_t) 0) == -1) {
		fprintf(stderr, "insufficient privilege\n");
		exit(1);
	}

	/*
	 * ignore the quit and interrupt signals early so
	 * no strange interrupts can occur.
	*/
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);

	(void) setlocale(LC_ALL, "");
	(void) setcat("uxcore");
	(void) setlabel("UX:login");

	tzset();

	errno = 0;
	u_name[0] = utmp.ut_user[0] = '\0';
	terminal[0] = '\0';


	/*
	 * Determine if this command was called by the user from
	 * shell level.  If so, issue a diagnostic and exit.
	*/
	if (at_shell_level()) {
		pfmt(stderr, MM_ERROR, ":666:cannot execute login at shell level.\n");
		exit(1);
	}

	/* indicate an ID-based privilege mechanism, ID 0 == root */
	priv_uid = 0;

	init_defaults();

#ifdef AFS
	afs_verify = (int (*)())__afs_getsym("afs_verify");
	afs_gettktstring = (char *(*)()) __afs_getsym("afs_gettktstring");
	if (afs_verify == NULL || afs_gettktstring == NULL)
		afs_verify = NULL;
#endif
#ifndef sgi
	/* don't set alarm here - some operations -like if yp is down
	 * can take a long time and shouldn't constitute the user
	 * not doing anything. Instead, we activate the alarm around
	 * the various places user's are asked for input
	 */
	/*
	 * Set the alarm to timeout in Def_timeout seconds if
	 * the user doesn't respond.  Also, set process priority.
	*/
	(void) alarm(Def_timeout);
#endif
	(void) nice(0);

	if (get_options(argc, argv) == -1) {
		usage();
		exit(1);
	}

	/*
	 * if devicename is not passed as argument, call findttyname(0)
	 * and findttyname(0)
	*/
	if (ttyn == NULL || *ttyn == NULL) {
		ttyn = findttyname(0);
		if (ttyn == NULL)
			ttyn = "/dev/???";
#ifdef sgi
		/* what? think you'll get a different answer next time?? */
		rttyn = ttyn;
#else
		rttyn = findttyname(0);
		if (rttyn == NULL)
			rttyn = "/dev/???";
#endif
	}
	else
		rttyn = ttyn;

	writelog = init_badtry(log_entry);

#ifdef EXECSH
        if (rflag) {
                /*
		 * next stdin string is the remote username; initialize
		 * `rusername', `luser' (needed by set_uthost), and
		 * set up utmp.ut_host before calling doremotelogin()
		 */
#ifdef sgi
		(void) alarm(Def_timeout);
#endif
		getstr(rusername, sizeof(rusername)-1, "remuser");
		getstr(luser, sizeof(luser)-1, "locuser");
#ifdef sgi
		(void) alarm(0);
#endif
		set_uthost(&(utmp.ut_host[0]), sizeof(utmp.ut_host)-1);
                usererr = doremotelogin(remotehost);
                if (0 != doremoteterm(terminal)) {
		    syslog(LOG_ERR|LOG_AUTH, "ioctl(TCSETA) on %s failed - bad baud rate?",
			   ttyn);
		    pfmt(stderr, MM_ERROR, "uxue:57:ioctl() failed: %s\n","TCSETA");
		    exit(1);

		}
        }
        else if (hflag) {
		strcpy(rusername, "UNKNOWN");
		set_uthost(&(utmp.ut_host[0]), sizeof(utmp.ut_host)-1);
        }

	early_advice(rflag, hflag);
#endif /* EXECSH */

	/*
	 * determine the number of login attempts to allow.
	 * A value of 0 is infinite.
	*/
	log_attempts = get_logoffval();

	/*
	 * get the prompt set by ttymon
	*/
	ttyprompt = getenv("TTYPROMPT");
	if ((ttyprompt != NULL) && (*ttyprompt != '\0')) {
#ifdef sgi
		(void) alarm(Def_timeout);
#endif
		/*
		 * if ttyprompt is set, there should be data on
		 * the stream already. 
		*/
		if ((envp = getargs(inputline)) != (char**)NULL) {
			uppercaseterm(*envp);
			/* call chk_args to process options	*/

			envp = chk_args(envp);
			if (*envp != (char *) NULL) {
				SCPYN(utmp.ut_user, *envp);
				SCPYN(u_name, *envp++);
			}
		}
#ifdef sgi
		(void) alarm(0);
#endif
	}
	else if (optind < argc) {
		SCPYN(utmp.ut_user, argv[optind]);
		SCPYN(u_name, argv[optind]);
		(void) strncpy(inputline, u_name, sizeof(inputline)-5);
		(void) strcat(inputline, "   \n");
		envp = &argv[optind + 1];
	}
	/*
	 * enter an infinite loop.  This loop will terminate on one of
	 * three conditions:
	 *
	 *	1) a successful login,
	 *
	 *	2) number of failed login attempts is greater than log_attempts,
	 *
	 *	3) an error occured and the loop exits.
	 *
	 *
	 */
	/* LINTED */
	for (;;) {
#ifdef sgi
		/* in case we 'continue' from bad password.. */
		(void) alarm(0);
#endif
		ia_uid = ia_gid = -1;
		/*
		 * free the storage for the master file
		 * information if it was previously allocated.
		*/
		if (uinfo_open) {
			uinfo_open = 0;
			ia_closeinfo(uinfo);
		}
		if ((pshell != NULL) && (*pshell != '\0')) {
			free(pshell);
			pshell = NULL;
		}
		if (pdir != NULL) {
			free(pdir);
			pdir = NULL;
		}

		/* If logging is turned on and there is an unsuccessful
		 * login attempt, put it in the string storage area
		*/

		if (writelog && (Def_failures > 0)) {
			logbadtry(log_trys, log_entry);

			if (log_trys == Def_failures) {
				/*
				 * write "log_trys" number of records out
				 * to the log file and reset log_trys to 1.
				*/
				badlogin(log_trys, log_entry);
				log_trys = 1;
			}
			else {
				++log_trys;
			}
		}
		/*
		 * On unsuccessfull login, update user's system-wide
		 * bad login count and lock out user if  count = Lockout.
		 */
		if (Lockout > 0 && trys)
			count_badlogins(0, u_name);

		/*
		 * don't do this the first time through.  Do it EVERY
		 * time after that, though.
		*/
		if (!firstime) {
			u_name[0] = utmp.ut_user[0] = '\0';
		}

		(void) fflush(stdout);
		/*
		 * one of the loop terminators.  If either of these
		 * conditions exists, exit when "trys" is greater
		 * than log_attempts and Def_maxtrys isn't 0.
		*/
		if (log_attempts && Def_maxtrys) {
			if (++trys > log_attempts) {
				/*
				 * If logging is turned on, output the string
			 	 * storage area to the log file, and sleep for
			 	 * DISABLETIME seconds before exiting.
				*/
				if (log_trys) {
					badlogin(log_trys, log_entry);
				}
				alarm(0);
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", "nobody", 0,
				    "Too many unsuccessful login attempts");
				cap_surrender(ocap);
				(void) sleep((unsigned)Def_distime);
				exit(1);
			}
		}
		/*
		 * keep prompting until the user enters something
		*/
		while (utmp.ut_user[0] == '\0') {
			if (rflag && firstime) {
				firstime = 0;
				SCPYN(utmp.ut_user, lusername);
				SCPYN(u_name, lusername);
				envp = remenvp;
			} else {
				/*
				 * if TTYPROMPT is not set, print out our own
				 * prompt
				 * otherwise, print out ttyprompt
				*/
				if ((ttyprompt == NULL) || (*ttyprompt == '\0'))
					pfmt(stdout, MM_NOSTD|MM_NOGET,
						gettxt(loginmsgid, loginmsg));
				else
					(void) fputs(ttyprompt, stdout);
				(void) fflush(stdout);
#ifdef sgi
				(void) alarm(Def_timeout);
#endif
				if ((envp = getargs(inputline)) != (char**)NULL) {
					envp = chk_args(envp);
					if (*envp != (char *) NULL) {
						SCPYN(utmp.ut_user, *envp);
						SCPYN(u_name, *envp++);
					}
				}
#ifdef sgi
				(void) alarm(0);
#endif
			}
		}
		(void)set_lang(envp);

		firstime = 0;
		/*
		 * If any of the common login messages was the input, we must be
		 * looking at a tty that is running login.  We exit because
		 * they will chat at each other until one times out.
		*/
		if (EQN(loginmsg, inputline) || EQN(pwdmsg, inputline) ||
				EQN(incorrectmsg, inputline)) {
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0,
				"Looking at a login line.");
			cap_surrender(ocap);
			pfmt(stderr, MM_ERROR, ":311:Looking at a login line.\n");
			exit(8);
		}

		if (ia_openinfo(u_name, &uinfo) || (uinfo == NULL)) {
#ifdef sgi
			(void) alarm(Def_timeout);
#endif
			(void) gpass(gettxt(pwdmsgid, pwdmsg), noupass.sp_pwdp,
				priv_uid);
			(void) dialpass("/sbin/sh", priv_uid);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0, "Invalid user name");
			cap_surrender(ocap);
			(void) sleep ((unsigned)Def_slptime);
			pfmt(stderr, MM_ERROR|MM_NOGET, gettxt(incorrectmsgid,
				incorrectmsg));
			failure_log();
	                continue;
		}
		/*
		 * set ``uinfo_open'' to 1 to indicate that the information
		 * from the master file needs to be freed if we go back to
		 * the top of the loop.
		*/
		uinfo_open = 1;

		/*
		 * get uid and gid info early for AUDIT
		*/
		ia_get_uid(uinfo, &ia_uid);
		ia_get_gid(uinfo, &ia_gid);
		ia_get_pwdpgm(uinfo, &ia_pwdpgm);
		ia_get_dir(uinfo, &ia_dirp);
		pdir = strdup(ia_dirp);
		ia_get_sh(uinfo, &ia_shellp);
		pshell = strdup(ia_shellp);

#ifdef AUX_SECURITY

/*
 * Enable auxilary security, allowing system administrators to specify
 * actions for login to take before asking for passwords.  This is done
 * via a switch statement and two labels.  ``accept'' is where to go
 * when the user is to be allowed in.  ``scont'' is where to go if 
 * the sitecheck program fails for some reason and you still want to
 * allow the user the chance to log in.  If you want to reprompt,
 * you should just break out of the loop, since it is immediately
 * followed by a continue.  This  will increment the number of tries, etc. 
 */

#ifdef EXECSH
		if (usererr == -1)
#endif /* EXECSH */
		if (Def_sitepath && *Def_sitepath) {
			switch (dositecheck()) {
			case SITE_OK:
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 1,
					 "external authentication succeeded");
				cap_surrender(ocap);
#ifdef sgi
				(void) alarm(Def_timeout);
#endif
				goto accept;
			case SITE_FAIL:
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "external authentication failed");
				cap_surrender(ocap);
				sleep((unsigned)Def_slptime);
				exit(1);
			case SITE_AGAIN:
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "external authentication retry");
				cap_surrender(ocap);
				sleep((unsigned)Def_slptime);
				break;
			case SITE_CONTINUE:
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "external authentication complete, also using IRIX authentication");
				cap_surrender(ocap);
				goto scont;
			}
			continue;
		}
	      scont:	       
#endif /* AUX_SECURITY */			 

#ifdef sgi
		(void) alarm(Def_timeout);
#endif
		/*
		 * get the user's password.
		 */
#ifdef EXECSH
		if (usererr == -1)
#endif /* EXECSH */
		if (read_pass(priv_uid, &nopassword)) {
			(void) dialpass(pshell, priv_uid);
			(void) sleep ((unsigned)Def_slptime);
			pfmt(stderr, MM_ERROR|MM_NOGET, gettxt(incorrectmsgid,
				incorrectmsg));
			failure_log();
			continue;
		}

		/*
		 * get dialup password, if necessary
		 */
accept:
		if (dialpass(pshell, priv_uid)) {
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0, "Invalid dialup password");
			cap_surrender(ocap);
			(void) sleep ((unsigned)Def_slptime);
			pfmt(stderr, MM_ERROR|MM_NOGET, gettxt(incorrectmsgid,
				incorrectmsg));
			failure_log();
			continue;
		}
#ifdef sgi
		(void) alarm(0);

		/*
		 * Verify the user is within clearance.  If MAC is not
		 * configured, the user is always within clearance.
		 */
		if (sysconf(_SC_MAC) > 0) {
			struct clearance *clp;
			char *mac_requested;

			/*
			 * get MAC info from database
			 */
			clp = sgi_getclearancebyname (u_name);
			if (clp == (struct clearance *) NULL) {
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "MAC clearance check failed");
				cap_surrender(ocap);
				continue;
			}

			/*
			 * determine what MAC label was requested
			 */
			mac_requested = getsenv ("MAC", envp);
			if (mac_requested == (char *) NULL) {
				if (clp->cl_default == (char *) NULL) {
					capv = CAP_AUDIT_WRITE;
					ocap = cap_acquire (1, &capv);
					ia_audit("LOGIN", u_name, 0,
						 "MAC clearance check failed");
					cap_surrender(ocap);
					continue;
				}
				mac_requested = clp->cl_default;
			}

			/*
			 * convert user input into internal form. 
			 * if remote login ignore user input and
			 * use the current process label unless
			 * MACREMOTE is CLEARANCE.
			 */
			if (Mac_Remote == MAC_SESSION && (rflag || hflag))
				u_mac = mac_get_proc ();
			else
				u_mac = mac_from_text (mac_requested);
			if (u_mac == (mac_t) NULL) {
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "MAC clearance check failed");
				cap_surrender(ocap);
				continue;
			}

			/*
			 * verify that the requested MAC label is permitted
			 */
			if (mac_clearedlbl (clp, u_mac) != MAC_CLEARED) {
				mac_free(u_mac);
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "MAC clearance check failed");
				cap_surrender(ocap);
				continue;
			}
		}

		/*
		 * Verify the user's capabilities (privileges)
		 */
		if (sysconf(_SC_CAP) > 0) {
			char *cap_requested;

			/*
			 * If capability was not explicitly requested this
			 * will be NULL.
			 */
			cap_requested = getsenv("CAP", envp);

			/*
			 * Look for an implicit capability request
			 */
			if (cap_requested == NULL) {
				struct user_cap *clp;

				if (clp = sgi_getcapabilitybyname(u_name))
					cap_requested = clp->ca_default;
				else
					cap_requested = "all=";
			}
			/*
			 * However you got it, convert it to binary form.
			 */
			u_cap = cap_from_text(cap_requested);

			/*
			 * Verify that this user is allowed the requested
			 * capability set.
			 */
			if (!cap_user_cleared(u_name, u_cap)) {
				cap_free(u_cap);
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "CAP clearance check failed");
				cap_surrender(ocap);
				continue;
			}
		}
#endif
		/*
		 * Check for login expiration
		*/
		ia_get_logexpire(uinfo, &ia_expire);
		if (ia_expire > 0) {
			if (ia_expire < DAY_NOW) {
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					"Account password has expired");
				cap_surrender(ocap);
				pfmt(stderr, MM_ERROR|MM_NOGET,
					gettxt(incorrectmsgid, incorrectmsg));
				exit(1);
			}
		}

		/*
		 * if this is an ID-based privilege mechanism and the
		 * user is privileged but NOT on the system console, exit!
		*/
		if ((priv_uid >= 0) && !on_console(priv_uid)) {
		  /* for consistency with unicos and to address pv: 200420
		   * print out slightly misleading error message
		   */
		   pfmt(stderr, MM_ERROR|MM_NOGET, gettxt(incorrectmsgid,
		   incorrectmsg));
		   exit(10);
		   }

		/*
		 * Have to set the process label before the chdir.
		 */
		if (sysconf(_SC_MAC) > 0) {
			capv = CAP_MAC_RELABEL_SUBJ;
			ocap = cap_acquire (1, &capv);
			if (mac_set_proc (u_mac) == -1) {
				cap_surrender (ocap);
				mac_free (u_mac);
				pfmt(stderr, MM_ERROR,
				     ":321:Bad user clearance.\n");
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "Bad user clearance");
				cap_surrender(ocap);
				exit(1);
			}
			cap_surrender (ocap);
			mac_free (u_mac);
		}

#ifdef EXECSH
		if (*ia_shellp == '*') {
			capv = CAP_CHROOT, ocap = cap_acquire (1, &capv);
			if (chroot(pdir) < 0 ) {
				cap_surrender (ocap);
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					"No directory for subsystem root");
				cap_surrender(ocap);
				pfmt(stderr, MM_ERROR,
					"uxsgicore:84:No root directory\n");
				continue;
			}
			cap_surrender (ocap);
			envinit[0] = SUBLOGIN;
			envinit[1] = (char*)NULL;
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
			    "LOGIN|+|%s|Subsystem root: %s", u_name, pdir);
			cap_surrender(ocap);
			pfmt(stderr, MM_INFO,
			     ":316:Logging in to subsystem root %s\n", pdir);
			execle("/usr/lib/iaf/scheme",
					"login", (char*)0, &envinit[0]);
			execle("/usr/bin/login", "login",
					(char*)0, &envinit[0]);
			execle("/etc/login", "login", (char*)0, &envinit[0]);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0,
				"Sublogin: No login programs on root");
			cap_surrender(ocap);
			pfmt(stderr, MM_ERROR, "uxsgicore:85:No /usr/lib/iaf/scheme or /usr/bin/login or /etc/login on root\n");
			exit(1);
		}
#endif /* EXECSH */

		/*
	 	* get the information for the last time this user logged
	 	* in, and set up the information to be recorded for this
	 	* session.
		*/
		lastlogok = do_lastlog(&utmp);

		break;		/* break out of while loop */
	}			/* end of infinite while loop */

	/*
	 * On successfull login: reset user's system-wide
	 * bad login count to zero.
	 */
	if (Lockout > 0)
		count_badlogins(1, u_name);

	if (syslog_success) {
		openlog("login", LOG_PID, LOG_AUTH);
		if (rflag_set || hflag) {
			syslog(LOG_INFO|LOG_AUTH, "%s@%s as %s",
				hflag ? "?" : rusername, remotehost, u_name);
		} else
			syslog(LOG_NOTICE|LOG_AUTH, "%s on %s", u_name, ttyn);
		closelog();
	}

	/*
	 * update the utmp and wtmp file entries.
	 */
	update_utmp(&utmp);

	/*
	 * check if the password has expired, the user wants to
	 * change password, etc.
	 */
	verify_pwd(nopassword, priv_uid);

	/*
	 * print advisory messages such as the Copyright messages,
	 * 
	 */
	pr_msgs(lastlogok);

	/*
	 * release the information held by the different "ia_"
	 * routines since that information is no longer needed.
	 */
	ia_closeinfo(uinfo);

#ifdef EXECSH

        if (quotactl(Q_SYNC, NULL, 0, NULL) == 0) {
		pid_t	pid;
		char	buf[10];

		sprintf(buf, "%d", ia_uid);
		if ((pid = fork()) == 0) {
			execl(QUOTAWARN, QUOTAWARN, "-n", buf, (char *)0);
			exit(1);
		} else if (pid != -1) {
			(void) waitpid(pid, (int *)NULL, 0);
		}
	}

	capv = CAP_FOWNER, ocap = cap_acquire (1, &capv);
	chmod(ttyn, S_IRUSR|S_IWUSR|S_IWGRP);
	chown(ttyn, ia_uid, ia_gid);
	cap_surrender (ocap);

	/* Set the sat_id to the user's UID.  */
	capv = CAP_AUDIT_CONTROL, ocap = cap_acquire (1, &capv);
	if (sysconf(_SC_AUDIT) > 0 && satsetid(ia_uid) < 0) {
		cap_surrender(ocap);
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "warning: satsetid failed");
		cap_surrender(ocap);
		exit(1);
	}
	cap_surrender(ocap);

	capv = CAP_SETGID, ocap = cap_acquire (1, &capv);
	if( setgid(ia_gid) == -1 ) {
		cap_surrender (ocap);
		pfmt(stderr, MM_ERROR, ":319:Bad group id.\n");
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "Bad group id");
		cap_surrender(ocap);
		exit(1);
	}
	cap_surrender (ocap);

	if (Initgroups && *Initgroups && 
	    strncasecmp(Initgroups, "YES", 3) == 0) {
		/* Initialize the supplementary group access list. */
		capv = CAP_SETGID, ocap = cap_acquire (1, &capv);
		if (initgroups(u_name, ia_gid) == -1) {
			cap_surrender (ocap);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0,
				"Could not initialize groups");
			cap_surrender(ocap);
			pfmt(stdout, MM_ERROR, 
			     ":320:Could not initialize groups.\n");
			exit(1);
		}
		if (initauxgroup(u_name, ia_gid, stdout) == -1) {
			cap_surrender (ocap);
			exit(1);
		}
		cap_surrender (ocap);
	} else {
		capv = CAP_SETGID, ocap = cap_acquire (1, &capv);
		if (setgroups(1, &ia_gid) == -1) {
			cap_surrender (ocap);
			pfmt(stdout, MM_ERROR, 
			     ":320:Could not initialize groups.\n");
			exit(1);
		}
		cap_surrender (ocap);
	}

	/*
	 * Start a new array session and set up this user's default
	 * project ID while we still have root privileges
	 */
	capv = CAP_SETUID, ocap = cap_acquire (1, &capv);
	newarraysess();
	setprid(getdfltprojuser(u_name));
	cap_surrender (ocap);

	/*
	 * Audit successful login (must be done as root, so we can't
	 * audit anything past the setuid).
	 */
	if (rflag_set || hflag) {
		/* expanded ia_audit for variable args */
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
			"LOGIN|+|%s|Remote login from %s@%s", u_name,
			hflag ? "?" : rusername, remotehost);
		cap_surrender(ocap);
	} else {
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
		    "LOGIN|+|%s|Successful login on %s", u_name, ttyn);
		cap_surrender(ocap);
	}

	capv = CAP_SETUID, ocap = cap_acquire (1, &capv);
#ifdef _SHAREII
	/*
	 *	Perform Share II resource limit checks and attach to the
	 *	user's lnode.  Root is exempt from resource checks.
	 */
	if (sgidladd(SH_LIMITS_LIB, RTLD_LAZY))
	{
		static const char *Myname = "login";

		SH_HOOK_SETMYNAME(Myname);
		if (SH_HOOK_LOGIN(ia_uid, ttyn))
		{
			cap_surrender(ocap);
			exit(1);
		}
			
	}
#endif /* _SHAREII */	
	if( setuid(ia_uid) == -1 ){
		cap_surrender (ocap);
		pfmt(stderr, MM_ERROR, ":321:Bad user id.\n");
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "Bad user id");
		cap_surrender(ocap);
		exit(1);
	}
	cap_surrender (ocap);
#ifdef DCE
	/*
	 * This routine sets up the basic environment.
	 */
	setup_environ(envp, renvp, pdir, &pshell);
#endif /* DCE */
#ifdef AFS
	/*
	 *      AFS environment vars must be set after the setuid
	 *      so we get the proper identity for the KRBTKFILE name. 
	 *      This code used to be executed in setup_environ. 
	 */
	if (afs_verify && __afs_iskauth()){ 
	  for (i = 0; envinit[i] != NULL; ++i) {};
		(void) strncat(afs_tktfile, (*afs_gettktstring)(), sizeof(afs_tktfile));
		envinit[i] = afs_tktfile;
		capv = CAP_FOWNER, ocap = cap_acquire (1, &capv);
		chown((*afs_gettktstring)(), ia_uid, ia_gid);
		cap_surrender (ocap);
	}
	if (afs_verify) {
		envinit[++i] = afs_passwdexp;
		sprintf(afs_passwdexp, "AFS_PASSWORD_EXPIRES=%u", afs_exp);
	}
#endif /* AFS */
	if (chdir(pdir) == -1)
	{
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
			  "LOGIN|-|%s|No home directory \"%s\"", u_name, pdir);
		cap_surrender(ocap);
		pfmt(stderr, MM_ERROR,
		     ":735:unable to change directory to \"%s\"\n", pdir);
		exit(1);
	}

	/*
	 * Set the user's capability set.
	 */
	if (sysconf(_SC_CAP) > 0) {
		capv = CAP_SETPPRIV, ocap = cap_acquire (1, &capv);
		if (cap_set_proc(u_cap) == -1)
		{
			cap_surrender(ocap);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0, "Bad capability set");
			cap_surrender(ocap);
			exit(1);
		}
		cap_free(u_cap);
		cap_free(ocap);
	}

	/*
	 * Re-enable the "BSD" signals SIGXCPU and SIGXFSZ if the 
	 * user doesn't want SVR4-type signal semantics.
	 */
	if (SVR4_Signals && *SVR4_Signals && 
	    !strncasecmp(SVR4_Signals, "NO", 2)) {
		(void) signal(SIGXCPU, SIG_DFL);
		(void) signal(SIGXFSZ, SIG_DFL);
	}

	ENVSTRNCAT(minusnam, basename(pshell));
	execl(pshell, minusnam, (char*)0);

	/* pshell was not an executable object file, maybe it
	 * is a shell proceedure or a command line with arguments.
	 * If so, turn off the SHELL= environment variable.
	 */
	for (i = 0; envinit[i] != NULL; ++i) {
		if ((envinit[i] == shell) &&
		    ((endptr = strchr(shell, '=')) != NULL))
			(*++endptr) = '\0';
	}


	if( access( pshell, R_OK|X_OK ) == 0 )
		execl(SHELL, "sh", pshell, (char*)0);
	pfmt(stderr, MM_ERROR, ":321:No shell\n");
	capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
	ia_audit("LOGIN", u_name, 0, "No shell");
	cap_surrender(ocap);
	exit(1);

#else	/* !EXECSH */
	exit(0);
#endif /* EXECSH */

	/* NOTREACHED */
}


/*
 * Procedure:	dialpass
 *
 *
 * Notes:	Opens either the DIAL_FILE or DPASS_FILE to determine
 *		if there is a dialup password on this system.
*/
static	int
dialpass(char *shellp, uid_t priv_uid)
{
	register FILE *fp;
	char defpass[PASS_MAX+1];
	char line[80];
	register char *p1, *p2;


	if ((fp = fopen(DIAL_FILE, "r")) == NULL) {
		return 0;
	}
	while ((p1 = fgets(line, sizeof(line), fp)) != NULL) {
		while (*p1 != '\n' && *p1 != ' ' && *p1 != '\t')
			p1++;
		*p1 = '\0';
		if (strcmp(line, rttyn) == 0)
			break;
	}
	(void) fclose(fp);
	if (p1 == NULL || (fp = fopen(DPASS_FILE, "r")) == NULL) {
		return 0;
	}

	defpass[0] = '\0';
	p2 = 0;
	while ((p1 = fgets(line, sizeof(line)-1, fp)) != NULL) {
		while (*p1 && *p1 != ':')
			p1++;
		*p1++ = '\0';
		p2 = p1;
		while (*p1 && *p1 != ':')
			p1++;
		*p1 = '\0';
		if (strcmp(shellp, line) == 0)
			break;

		/* Existing sites have /bin/sh as the default in d_passwd.
		 * To keep them secure, check both SHELL (/sbin/sh) 
		 * and /bin/sh to use as the default.  Last one found is
		 * used if both are present.
		 * BUG 184400
		 */
		if ((strcmp(SHELL, line) == 0) || (strcmp("/bin/sh", line)==0))
		{
			SCPYN(defpass, p2);
		}
		p2 = 0;
	}
	(void) fclose(fp);
	if (!p2)
		p2 = defpass;
	if (*p2 != '\0')
		return gpass(gettxt(":332", "Dialup Password:"), p2, priv_uid);
	return 0;
}


/*
 * Procedure:	gpass
 *
 * Notes:	getpass() fails if it cannot open /dev/tty.
 *		If this happens, and the real UID is privileged,
 *		(in an ID-based privilege mechanism) then use the
 *		current stdin and stderr.
 *
 *		This allows login to work with network connections
 *		and other non-ttys.
*/
static	int
gpass(char *prmt, char *pswd, uid_t priv_uid)
{
	register char *p1;
	cap_value_t capv;
	cap_t ocap;

	if (((p1 = mygetpass(prmt)) == (char *)0) && (getuid() == priv_uid)) {
		p1 = fgetpass(stdin, stderr, prmt);
	}
#ifdef AFS
	if (p1 && afs_verify) {
		if ((*afs_verify)(u_name, p1, &afs_exp, 0) == 0)
			return 0;
	}
#endif /* AFS */
#ifdef DCE
	if (p1 && dfs_authentication) {
		char *dce_err=NULL;
		if (dce_verify(u_name, ia_uid, ia_gid, p1, &dce_err)) {
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			if (dce_err) {
				satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
					  "LOGIN|-|%s|DCE authentication failed: %s",
					  u_name, dce_err);
				free(dce_err);
			} else {
				satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
					  "LOGIN|-|%s|DCE authentication failed",
					  u_name);
			}
			cap_surrender(ocap);
			return 1;
		}
		return 0;
	}
#endif /* DCE */
	if (!p1 || strcmp(crypt(p1, pswd), pswd)) {
		return 1;
	}
	return 0;
}


/*
 * Procedure:	chk_args
 *
*/
static	char **
chk_args(char **pp)
{
	char	*p,
		*invalidopt = ":669:Invalid options -h, -v\n",
		*badservice = ":593:System service not installed\n";

	pwflag = 0;

	while (*pp) {
		p = *pp;

		if (*p++ != '-') {
			return pp;
		}
		else {
			pp++;
			switch(*p++) {
			case 'v':
			case 'h':
				pfmt(stderr, MM_ERROR, invalidopt);
				pfmt(stderr, MM_ERROR, badservice);
				exit(1);
				break;
			case 'p':
			/*
			 * XXX This is the SVR4 login -p option which
			 * collides with the IRIX/BSD -p option. See
			 * comments in get_options().
			 */
				pwflag++;
				break;
			}
		}
	}
	return pp;
}


/*
 * Procedure:	getargs
 *
 * Notes:	scans the data enetered at the prompt and stores the
 *		information in the argument passed.  Exits if EOF is
 *		enetered.
*/
static char **
getargs(char *inline)
{
	int	c, llen = MAXLINE - 1;
	char	*ptr = envbuf, **reply = args;
	enum {
		WHITESPACE, ARGUMENT
	} state = WHITESPACE;

	while ((c = getc(stdin)) != '\n') {
		/*
		 * check ``llen'' to avoid overflow on ``inline''.
		*/
		if (llen > 0) {
			--llen;
			/*
			 * Save a literal copy of the input in ``inline''.
			 * which is checked in main() to determine if
			 * this login process "talking" to another login
			 * process.
			*/
			*(inline++) = (char) c;
		}
		switch (c) {
		case EOF:
			/*
			 * if the user enters an EOF character, exit
			 * immediately with the value of one (1) so it
			 * doesn't appear as if this login was successful.
			*/
			exit(1);
			/* FALLTHROUGH */
		case ' ':
		case '\t':
			if (state == ARGUMENT) {
				*ptr++ = '\0';
				state = WHITESPACE;
			}
			break;
		case '\\':
			c = quotec();
			/* FALLTHROUGH */
		default:
			if (state == WHITESPACE) {
				*reply++ = ptr;
				state = ARGUMENT;
			}
			*ptr++ = (char) c;
		}
		/*
		 * check if either the ``envbuf'' array or the ``args''
		 * array is overflowing.
		*/
		if (ptr >= envbuf + MAXLINE - 1
			|| reply >= args + MAXARGS - 1 && state == WHITESPACE) {
				(void) putc('\n', stdout);
			break;
		}
	}
	*ptr = '\0';
	*inline = '\0';
	*reply = NULL;

	return ((reply == args) ? NULL : args);
}

/*
 * like getargs() but from string
 */
static char **
getargs2(char *inline)
{
	int	c, llen = MAXLINE - 1;
	char	*ptr = envbuf, **reply = args;
	enum {
		WHITESPACE, ARGUMENT
	} state = WHITESPACE;

	while(c = *inline++) {
		if(llen > 0)
			--llen;
		switch(c) {
		case ' ':
		case '\t':
			if(state == ARGUMENT) {
				*ptr++ = '\0';
				state = WHITESPACE;
			}
			break;
		case '\\':
			inline = quotec2(inline, &c);
		default:
			if(state == WHITESPACE) {
				*reply++ = ptr;
				state = ARGUMENT;
			}
			*ptr++ = (char) c;
		}
		/*
		 * check if either the ``envbuf'' array or the ``args''
		 * array is overflowing.
		*/
		if(ptr >= envbuf + MAXLINE - 1
			|| reply >= args + MAXARGS - 1 && state == WHITESPACE) {
				break;
		}
	}
	*ptr = '\0';
	*reply = NULL;
	return ((reply == args) ? NULL : args);
}

/*
 * Procedure:	quotec
 *
 * Notes:	Reads from the "standard input" of the tty. It is
 *		called by the routine "getargs".
*/
static int
quotec(void)
{
	register int c, i, num;

	switch (c = getc(stdin)) {
	case 'n':
		c = '\n';
		break;
	case 'r':
		c = '\r';
		break;
	case 'v':
		c = '\013';
		break;
	case 'b':
		c = '\b';
		break;
	case 't':
		c = '\t';
		break;
	case 'f':
		c = '\f';
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		for (num=0, i=0; i<3; i++) {
			num = num * 8 + (c - '0');
			if ((c = getc(stdin)) < '0' || c > '7')
				break;
		}
		(void) ungetc(c, stdin);
		c = num & 0377;
		break;
	default:
		break;
	}
	return c;
}

/*
 * like quotec() but from string
 */
static char *
quotec2(char *s, int *cp)
{
	register int c, i, num;

	switch (c = *s++) {
	case 'n':
		c = '\n';
		break;
	case 'r':
		c = '\r';
		break;
	case 'v':
		c = '\013';
		break;
	case 'b':
		c = '\b';
		break;
	case 't':
		c = '\t';
		break;
	case 'f':
		c = '\f';
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		for (num=0, i=0; i<3; i++) {
			num = num * 8 + (c - '0');
			c = *s++;
			if (c < '0' || c > '7')
				break;
		}
		s--;
		c = num & 0377;
		break;
	default:
		break;
	}
	*cp = c;
	return(s);
}

static char *illegal[] = {
		"SHELL=",
		"HOME=",
		"LOGNAME=",
#ifndef	NO_MAIL
		"MAIL=",
#endif
		"CDPATH=",
		"IFS=",
		"PATH=",
		"USER=",
		0
};

static char *illegal_log[] = {	/* we syslog if in this set */
		"_RLD",	/* no =; any of the _RLD variables; include future */
		"LD_LIBRARY",	/* no =; any of the 3 ISAs variables; include future */
		0
};

/*
 * Procedure:	legalenvvar
 *
 * Notes:	Determines if it is legal to insert this
 *		environmental variable.
*/
static int
legalenvvar(char *s)
{
	register char **p;

	for (p = illegal; *p; p++)
		if (!strncmp(s, *p, strlen(*p)))
			return 0;

	for (p = illegal_log; *p; p++)
		if (!strncmp(s, *p, strlen(*p))) {
			/* use sprintf to avoid possibility of overrunning
			 * syslog buffer. */
			char msg[256];
			sprintf(msg, "ignored attempt to setenv(%.128s)", s);
			openlog("login", LOG_PID, LOG_AUTH);
			syslog(LOG_AUTH, msg);
			closelog();
			return 0;
		}
	return 1;
}


/*
 * Procedure:	badlogin
 *
 *
 * Notes:	log to the log file after "trys" unsuccessful attempts
*/
static	void
badlogin(int trys, char **log_entry)
{
	int retval, count, fildes;


	failure_log();

	/* Tries to open the log file. If succeed, lock it and write
	   in the failed attempts */
	if ((fildes = open (LOGINLOG, O_APPEND|O_WRONLY)) == -1)
		return;
	else {
		(void) sigset(SIGALRM, donothing);
		(void) alarm(L_WAITTIME);
		retval = lockf(fildes, F_LOCK, 0L);
		(void) alarm(0);
		(void) sigset(SIGALRM, SIG_DFL);
		if (retval == 0) {
			for (count = 0 ; count < trys ; count++) {
			   (void) write(fildes, log_entry[count],
				(unsigned) strlen (log_entry[count]));
				*log_entry[count] = '\0';
			}
			(void) lockf(fildes, F_ULOCK, 0L);
			(void) close(fildes);
		}
		return;
	}
}


/*
 * Procedure:	donothing
 *
 * Notes:	called by "badlogin" routine when SIGALRM is
 *		caught.  The intent is to do nothing when the
 *		alarm is caught.
*/
static	void
donothing(void) {}


/*
 * Procedure:	getpass
 *
 * Restrictions:
 *		fopen:		none
 *		setbuf:		none
 *		fclose:		none
 *
 * Notes:	calls "fgetpass" to read the user's password entry.
*/
static	char *
mygetpass(char *prompt)
{
	char *p;
	FILE *fi;

	if ((fi = fopen("/dev/tty", "r")) == NULL) {
		return (char*)NULL;
	}
	setbuf(fi, (char*)NULL);
	p = fgetpass(fi, stderr, prompt);
	if (fi != stdin)
		(void) fclose(fi);
	return p;
}


/*
 * Procedure:	fgetpass
 * Restrictions:
 *
 *		ioctl(2):	None
 * 
 * Notes:	issues the "Password: " prompt and reads the input
 *		after turning off character echoing.
*/
static	char *
fgetpass(FILE *fi, FILE *fo, char *prompt)
{
	struct termio ttyb;
	tcflag_t flags;
	register char *p;
	register int c;
	static char pbuf[PBUFSIZE + 1];
	void (*sig)();

	sig = signal(SIGINT, catch);
	intrupt = 0;
	(void) ioctl(fileno(fi), TCGETA, &ttyb);
	flags = ttyb.c_lflag;
	ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	(void) ioctl(fileno(fi), TCSETAF, &ttyb);
	(void) fputs(prompt, fo);
	for (p = pbuf; !intrupt && (c = getc(fi)) != '\n' && c != EOF;) {
		if (p < &pbuf[PBUFSIZE])
			*p++ = (char) c;
	}
	*p = '\0';
	(void) putc('\n', fo);
	ttyb.c_lflag = flags;
	(void) ioctl(fileno(fi), TCSETAW, &ttyb);
	(void) signal(SIGINT, sig);
	if (intrupt)
		(void) kill(getpid(), SIGINT);

	return pbuf;
}


/*
 * Procedure:	catch
 *
 * Notes:	called by fgetpass if the process catches an
 *		INTERRUPT signal.
*/
static void
catch(void)
{
	++intrupt;
}


/*
 * Procedure:	uppercaseterm
 *
 * Restrictions:
 *		ioctl(2):	None
 *
 * Notes:	if all input characters are upper case set the
 *		corresponding termio so ALL input and output is
 *		UPPER case.
*/
static	void
uppercaseterm(char *strp)
{
	int 	upper = 0;
	int 	lower = 0;
	char	*sp;
	struct	termio	termio;

	for (sp = strp; *sp; sp++) {
		if (islower(*sp)) 
			lower++;
		else if (isupper(*sp))
			upper++;
	}

	if (upper > 0 && lower == 0) {
		(void) ioctl(0,TCGETA,&termio);
		termio.c_iflag |= IUCLC;
		termio.c_oflag |= OLCUC;
		termio.c_lflag |= XCASE;
		(void) ioctl(0,TCSETAW,&termio);
		for (sp = strp; *sp; sp++) 
			if (*sp >= 'A' && *sp <= 'Z' ) *sp += ('a' - 'A');
	}
}


/*
 * Procedure:	findttyname
 *
 *
 * Notes:	call ttyname(), but do not return syscon, systty,
 *		or sysconreal do not use syscon or systty if console
 *		is present, assuming they are links.
*/
static	char *
findttyname(int fd)
{
	char	*lttyn;

	lttyn = ttyname(fd);

	if (lttyn == NULL) return NULL;

	if (((strcmp(lttyn, "/dev/syscon") == 0) ||
	     (strcmp(lttyn, "/dev/sysconreal") == 0) ||
	     (strcmp(lttyn, "/dev/systty") == 0)) &&
	     (access("/dev/console", F_OK) == 0))
		lttyn = "/dev/console";

	return lttyn;
}




/*
 * Procedure:	init_defaults
 *
 * Restrictions:
 *		defopen:	None
 *		lvlin:		None
 *		lvlvalid:	None
 *
 * Notes:	reads the "login" default file in "/etc/defaults"
 *		directory.  Also initializes other variables used
 *		throughout the code.
*/
static	void
init_defaults(void)
{
	FILE *defltfp;
	register char	*ptr,
			*Pndefault = "login";

	if ((defltfp = defopen(Pndefault)) != NULL) {
		if ((Console = defread(defltfp, "CONSOLE")) != NULL)
			if (*Console)
				Console = strdup(Console);
			else
				Console = NULL;
		if ((Altshell = defread(defltfp, "ALTSHELL")) != NULL)
			if (*Altshell)
				Altshell = strdup(Altshell);
			else
				Altshell = NULL;
		if ((Passreq = defread(defltfp, "PASSREQ")) != NULL)
			if (*Passreq)
				Passreq = strdup(Passreq);
			else
				Passreq = NULL;
		if ((Mandpass = defread(defltfp, "MANDPASS")) != NULL)
			if (*Mandpass)
				Mandpass = strdup(Mandpass);
			else
				Mandpass = NULL;
		if ((Initgroups = defread(defltfp, "INITGROUPS")) != NULL)
			if (*Initgroups)
				Initgroups = strdup(Initgroups);
			else
				Initgroups = NULL;
		if ((SVR4_Signals = defread(defltfp, "SVR4_SIGNALS")) != NULL)
			if (*SVR4_Signals)
				SVR4_Signals = strdup(SVR4_Signals);
			else 
				SVR4_Signals = NULL;
		if ((Def_hertz = defread(defltfp, "HZ")) != NULL)
			if (*Def_hertz)
				Def_hertz = strdup(Def_hertz);
			else
				Def_hertz = NULL;
		if ((Def_path = defread(defltfp, "PATH")) != NULL)
			if (*Def_path)
				Def_path = strdup(Def_path);
			else
				Def_path = NULL;
#ifdef AUX_SECURITY
		if ((Def_sitepath = defread(defltfp, "SITECHECK")) != NULL)
			if (*Def_sitepath)
				Def_sitepath = strdup(Def_sitepath);
			else
				Def_sitepath = NULL;
#endif /* AUX_SECURITY */

		/*
		 * have to setlocale() here, to get msgs in LANG
		 */
		if((Def_lang = getenv("LANG")) == NULL) {
		    if((Def_lang = defread(defltfp, "LANG")) != NULL)
			Def_lang = *Def_lang? strdup(Def_lang) : NULL;
		}
		if(Def_lang)
		    (void)setlocale(LC_ALL, Def_lang);

		if ((Def_supath = defread(defltfp, "SUPATH")) != NULL)
			if (*Def_supath)
				Def_supath = strdup(Def_supath);
			else
				Def_supath = NULL;

		if ((Def_syslog = defread(defltfp, "SYSLOG")) != NULL)
			if (Def_syslog && *Def_syslog) {
				if (strcmp (Def_syslog, "FAIL") == 0)
					syslog_fail = 1;
				else if (strcmp (Def_syslog, "ALL") == 0)
					syslog_success = syslog_fail = 1;
			}

		if ((Def_notlockout = defread(defltfp, "LOCKOUTEXEMPT")) != NULL)
			if (*Def_notlockout)
			        Def_notlockout = strdup(Def_notlockout);
			else
			        Def_notlockout = NULL;

		if ((ptr = defread(defltfp, "TIMEOUT")) != NULL)
		    Def_timeout = (unsigned) atoi(ptr);
		if ((ptr = defread(defltfp, "SLEEPTIME")) != NULL)
		    Def_slptime =  atol(ptr);
		if ((ptr = defread(defltfp, "DISABLETIME")) != NULL)
		    Def_distime =  atol(ptr);
		if ((ptr = defread(defltfp, "MAXTRYS")) != NULL)
		    Def_maxtrys =  atol(ptr);
		if ((ptr = defread(defltfp, "LOGFAILURES")) != NULL)
		    Def_failures = atol(ptr);
		if ((ptr = defread(defltfp, "LOCKOUT")) != NULL)
		    Lockout =  atol(ptr);
		if ((ptr = defread(defltfp, "UMASK")) != NULL)
			if (sscanf(ptr, "%lo", &Umask) != 1)
				Umask = DEFUMASK;
		if ((ptr = defread(defltfp, "IDLEWEEKS")) != NULL)
			Idleweeks = atoi(ptr);
		if ((ptr = defread(defltfp, "MACREMOTE")) != NULL)
			if (strcmp(ptr, "CLEARANCE") == 0)
				Mac_Remote = MAC_CLEARANCE;
				

		(void) defclose(defltfp);
	}

	if (((mode_t) 0777) < Umask)
		Umask = DEFUMASK;

	(void) umask(Umask);


	if (!Def_tz || (Def_tz && !*Def_tz))
		Def_tz = getenv("TZ");

	if (!Def_tz)
		(void) strcat(timez, DEF_TZ);
	else 
		ENVSTRNCAT(timez, Def_tz);

	(void) putenv(timez);

	if (Def_timeout > MAX_TIMEOUT)
		Def_timeout = MAX_TIMEOUT;
	if (Def_slptime > DEF_TIMEOUT)
		Def_slptime = DEF_TIMEOUT;
	if (Def_failures < 0 ) 
		Def_failures = LOGFAILURES;
	if (Def_failures > MAX_FAILURES)
		Def_failures = MAX_FAILURES;
	if (Def_maxtrys < 0 )
		Def_maxtrys = MAXTRYS;

	return;
}

/*
 * Procedure:	exec_pass
 *
 * Notes:	This routine forks, changes the uid of the forked process
 *		to the user logging in, and execs the "/usr/bin/passwd"
 *		command.  It returns the status of the "exec" to the
 *		parent process.  All "working" privileges of the forked
 *		(child) process are cleared.  Also, P_SYSOPS is cleared
 *		from the maximum set to indicate to "passwd" that this
 *		"exec" originated from the login scheme.
*/
static int
exec_pass(char *usernam)
{
	int	status, w;
	pid_t	pid;
	cap_t	ocap;
	cap_value_t capv;

	if ((pid = fork()) == 0) {
		if (ia_uid > 0) {
			cap_value_t cv[] = {CAP_SETUID, CAP_SETGID};

			ocap = cap_acquire (ia_gid > 0 ? 2 : 1, cv);
			if (ia_gid > 0 && setgid(ia_gid) == -1) {
				cap_surrender (ocap);
				exit(127);
			}
			if (setuid(ia_uid) == -1) {
				cap_surrender (ocap);
				exit(127);
			}
			cap_surrender (ocap);
		}
		(void) execl(ia_pwdpgm, ia_pwdpgm, usernam, (char *)NULL);
		exit(127);
	}

	while ((w = (int) wait(&status)) != pid && w != -1)
		;

	if (w != -1 && status > 0) {
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "password program returned error");
		cap_surrender(ocap);
	}
	if (w < 0 || status < 0) {
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "error execing password program");
		cap_surrender(ocap);
	}

	return (w == -1) ? w : status;
}


#ifdef EXECSH

extern int _getpwent_no_shadow;
static int
doremotelogin(char *host)
{
	struct passwd *pw;

#ifdef sgi
	(void) alarm(Def_timeout);
#endif
	/* caller already read remote and local usernames */
	getstr(terminal, sizeof(terminal)-1, "Terminal type");
#ifdef sgi
	(void) alarm(0);
#endif

	/*
	 * handle args to login name
	 */
	if( !(remenvp = getargs2(luser)))
	    lusername[0] = 0;
	else
	    (void)strncpy(lusername, *remenvp++, sizeof(lusername)-1);
	(void)set_lang(remenvp);

	SCPYN(u_name, lusername);
	if (getuid())
		return(-1);
	_getpwent_no_shadow = 1;
	pw = getpwnam(lusername);
	_getpwent_no_shadow = 0;
	if (pw == NULL)
		return -1;
	return(__ruserok_x(host, (pw->pw_uid == 0), rusername, lusername,
		pw->pw_uid, pw->pw_dir));
}

/* ARGSUSED */
static void
getstr(char *buf, int cnt, char *err)
{
	char c;

	do {
		if (read(0, &c, 1) != 1)
			exit(1);
		if (--cnt < 0) {
			buf[-1] = '\0';
			return;
		}
		*buf++ = c;
	} while (c != 0);
}

static int
doremoteterm(char *term)
{

	struct termio tp;
	register char *cp = strchr(term, '/');
	char *speed;

	ioctl(0, TCGETA, &tp);

	if (cp) {
		*cp++ = '\0';
		speed = cp;
		cp = strchr(speed, '/');
		if (cp)
			*cp++ = '\0';
		tp.c_ospeed = atoi(speed);
	}
	tp.c_lflag |= ISIG|ICANON|ECHO|ECHOE|ECHOK;
	tp.c_oflag |= OPOST|ONLCR;
	tp.c_iflag |= BRKINT|IGNPAR|ISTRIP|ICRNL|IXON;
	tp.c_cc[VEOL] = CEOL;
	tp.c_cc[VEOF] = CEOF;

	return ioctl(0, TCSETA, &tp);

}

#endif /* EXECSH */

/*
 * Procedure:	get_options
 *
 * Notes:	get_options parses the command line.  It returns 0
 *		if successful, -1 if failed.
*/
extern int _check_rhosts_file;  /* used by ruserok */

static	int
get_options(int argc, char **argv)
{
	int	c;
	int	errflg = 0;

	while ((c = getopt(argc, argv, "d:r:R:h:t:u:l:s:M:U:S:p")) != -1) {
		switch (c) {
#ifdef EXECSH
		/*
		 * XXX The (IRIX/BSD login) -p option tells login not to
		 * destroy the environment. But SVR4 login has a -p option
		 * for calling "/usr/bin/passwd". Currently the SVR4 -p
		 * option is not supported on the commmand line. If and
		 * when support is added/required, the current -p should
		 * be changed to something like "-P" and client applications
		 * (telnetd, 4DDN sethostd) must be changed.
		 */
		case 'p':
			pflag++;
			break;
		case 't':
			strncpy(terminal, optarg, sizeof(terminal)-1);
			break;
		case 'r':
		case 'R':
			_check_rhosts_file = c == 'r' ? 1 : 0;
                        if (hflag || rflag) {
				pfmt(stderr, MM_ERROR,
					":310:Only one of -r and -h allowed\n");
				exit(1);
			}
			rflag_set = ++rflag;
			strncpy (remotehost, optarg, sizeof(remotehost)-1);
			break;
		case 'h':
                        if (hflag || rflag) {
				pfmt(stderr, MM_ERROR,
					":310:Only one of -r and -h allowed\n");
				exit(1);
			}
			hflag++;
			strncpy (remotehost, optarg, sizeof(remotehost)-1);
			break;
#else /* !EXECSH */
		/*
		 * no need to continue login since the -r option
		 * is not allowed.
		 */
		case 'r':
			return -1;
		/*
		 * the ability to specify the -d option at the "login: "
		 * prompt with an argument is still supported however it
		 * has no effect.
		 */
#endif /* EXECSH */
		case 'd':
			/* ignore the following options for IAF reqts */
		case 'u':
		case 'l':
		case 's':
		case 'M':
		case 'U':
		case 'S':
			break;
		default:
			errflg++;
			break;
		} /* end switch */
	} /* end while */
	if (errflg)
		return -1;
	return 0;
}


/*
 * Procedure:	usage
 *
 * Notes:	prints the usage message.
*/
static	void
usage(void)
{
	pfmt(stderr, MM_ACTION,
		":670:Usage: login [[ -p ] name [ env-var ... ]]\n");
}




/*
 * Procedure:	do_lastlog
 *
 * Notes:	gets the information for the last time the user logged
 *		on and also sets up the information for this login
 *		session so it can be reported at a subsequent login.
 *		The original code does this in a file indexed by uid.
 *		This works badly on efs since hole-y files are not supported,
 *		so the cypress scheme is implemented by default.
 */
static	int
do_lastlog(struct utmpx *utmp)
{
	int	fd1,
		lastlogok = 0, 
		exists;
	long	ia_inact;
	struct	stat	f_buf;
	struct	lastlog	newll;
	char	*fname;
	cap_t	ocap;
	cap_value_t cap_mac_grade[] = {CAP_MAC_DOWNGRADE, CAP_MAC_UPGRADE};
	cap_value_t capv;

	exists = stat(LASTLOG, &f_buf) == 0;
	if (exists && !S_ISDIR(f_buf.st_mode)) {
		unlink(LASTLOG);
		exists = 0;
	}
	if (!exists) {
		(void) mkdir(LASTLOG, 0);
		(void) chmod(LASTLOG, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
		(void) stat(LASTLOG, &f_buf);
	}
	if (!S_ISDIR(f_buf.st_mode))
		return 0;

	fname = (char *)malloc(strlen(LASTLOG) + strlen(utmp->ut_user) + 1);
	if (!fname)
		return 0;
	sprintf(fname, "%s/%s", LASTLOG, utmp->ut_user);
	if (stat(fname, &f_buf) < 0) {
		capv = CAP_MAC_WRITE, ocap = cap_acquire (1, &capv);
		(void) close(creat(fname, (mode_t) 0));
		cap_surrender (ocap);
		(void) chmod(fname, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH));

		if (sysconf (_SC_MAC) > 0) {
			mac_t dblow_label = mac_from_text ("dblow");

			if (dblow_label == NULL) {
				free(fname);
				return 0;
			}

			ocap = cap_acquire(2, cap_mac_grade);
			if (mac_set_file (fname, dblow_label) == -1) {
				cap_surrender(ocap);
				mac_free(dblow_label);
				free(fname);
				return 0;
			}
			cap_surrender(ocap);
			mac_free(dblow_label);
		}
	}
	capv = CAP_MAC_WRITE, ocap = cap_acquire (1, &capv);
	if ((fd1 = open(fname, O_RDWR)) < 0) {
		cap_surrender (ocap);
		free(fname);
		return 0;
	}
	cap_surrender (ocap);
	free(fname);
	(void) fstat(fd1, &f_buf);
	if (!S_ISREG(f_buf.st_mode))
		return 0;
	switch (f_buf.st_size) {
	case 0:
		break;
	case sizeof(ll):
		if (read(fd1, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
			ll.ll_time != 0)
			lastlogok = 1;
		break;
	case sizeof(struct lastlog4): {
		struct lastlog4 ll4;

		if (read(fd1, (char *)&ll4, sizeof(ll4)) == sizeof(ll4) &&
			ll4.ll_time != 0 && ll4.ll_status) {
			lastlogok = 1;
			ll.ll_time = ll4.ll_time;
			/* line & host smaller than current */
			strncpy(ll.ll_line, ll4.ll_line, sizeof(ll4.ll_line)-1);
			strncpy(ll.ll_host, ll4.ll_host, sizeof(ll4.ll_host)-1);
			ll.ll_level = 0;
		}
		ftruncate(fd1, 0L);
		break;
	    }
	case sizeof(struct lastlog5a): {
		struct lastlog5a ll5a;

		if (read(fd1, (char *)&ll5a, sizeof(ll5a)) == sizeof(ll5a) &&
			ll5a.ll_time != 0) {
			lastlogok = 1;
			ll.ll_time = ll5a.ll_time;
			/* line & host smaller than current */
			strncpy(ll.ll_line, ll5a.ll_line, sizeof(ll5a.ll_line)-1);
			strncpy(ll.ll_host, ll5a.ll_host, sizeof(ll5a.ll_host)-1);
			ll.ll_level = ll5a.ll_level;
		}
		ftruncate(fd1, 0L);
		break;
	    }
	default:
		ftruncate(fd1, 0L);
		break;
	}

	(void) lseek(fd1, 0, 0);
	(void) time(&newll.ll_time);
	if (utmp->ut_host[0])
		SCPYN(newll.ll_line, rusername);
	else
		SCPYN(newll.ll_line, (rttyn + sizeof("/dev/")-1));
	SCPYN(newll.ll_host, remotehost);

	/* Check for login inactivity */

	ia_get_loginact(uinfo, &ia_inact);
	if ((ia_inact > 0) && ll.ll_time)
		if((( ll.ll_time / DAY ) + ia_inact) < DAY_NOW ) {
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0,
				"Account inactive too long");
			cap_surrender(ocap);
			pfmt(stderr, MM_ERROR|MM_NOGET,
				gettxt(incorrectmsgid, incorrectmsg));
			(void) close(fd1);
			exit(1);
		}

	(void) write(fd1, (char * )&newll, sizeof(newll));
	(void) close(fd1);

	return lastlogok;
}


/*
 * Procedure:	setup_environ
 *
 * Restrictions: 
 *		access(2):	None
 *
 * Notes:	Set up the basic environment for the exec.  This
 *		includes HOME, PATH, LOGNAME, SHELL, TERM, HZ, TZ,
 *		and MAIL.
*/
static	void
setup_environ(char **envp, char **renvp, char *dirp, char **shellp)
{
	static int basicenv;
	static char envblk[MAXENV];
	register int	i, j, k,
			l_index, length;
	char	*ptr, *endptr;

	/*
	 * login will only set the environment variable "TERM" if it
	 * already exists in the environment.  This allows features
	 * such as doconfig with the port monitor to work correctly
	 * if an administrator specifies a particular terminal for a
	 * particular port.
	 *
	 */
#ifdef EXECSH
        if (terminal[0] != '\0') {		/* from -r or -t option */
		(void) strcat(term, "TERM=");
		ENVSTRNCAT(term, terminal)
	} else
#endif /* EXECSH */
	{
		if (!Def_term || !*Def_term) {
			if ((Def_term = getenv("TERM")) != NULL) {
				(void) strcpy(term, "TERM=");
				ENVSTRNCAT(term, Def_term);
			}
		} else {
			(void) strcpy(term, "TERM=");
			ENVSTRNCAT(term, Def_term);
		}
	}

	if (!Def_hertz || !*Def_hertz) {
		if ((Def_hertz = getenv("HZ")) != NULL) {
			ENVSTRNCAT(hertz, Def_hertz);
		} else
			(void) strcat(hertz, DEF_HZ);
	} else {
		ENVSTRNCAT(hertz, Def_hertz);
	}

	{
	int fd;
	char *nlp, uhlang[MAXPATHLEN + 7];

	    if( !Def_lang || !*Def_lang)
		(void)strcat(lang, DEF_LANG);
	    else {
		ENVSTRNCAT(lang, Def_lang);
	    }

	    /*
	     * .lang overwrites all LANG settings
	     * but not those from username args
	     */
	    (void)strncpy(uhlang, dirp, MAXPATHLEN);
	    (void)strcat(uhlang, LANG_FILE);
	    if((fd = open(uhlang, O_RDONLY)) >= 0) {
		int nrd;
		if((nrd=read(fd, uhlang, NL_LANGMAX + 2)) > 0) {
		    if(nlp = strchr(uhlang, '\n'))
			 *nlp = 0;
		    else uhlang[nrd]= 0;
		    uhlang[NL_LANGMAX] = 0;
		    (void)strcpy(lang, "LANG=");
		    ENVSTRNCAT(lang, uhlang);
		}
		(void)close(fd);
	    }
	}

	{
	int fd;
	char *nlp, uhtz[MAXPATHLEN + 11];

	    /*
	     * .timezone overwrites all LANG settings
	     * but not those from username args
	     */
	    (void)strncpy(uhtz, dirp, MAXPATHLEN);
	    (void)strcat(uhtz, TZ_FILE);
	    if((fd = open(uhtz, O_RDONLY)) >= 0) {
		int nrd;
		/* -4 to leave space for NUL and "TZ=" */
		if((nrd=read(fd, uhtz, sizeof(timez) - 4)) > 0) {
		    if(nlp = strchr(uhtz, '\n'))
			 *nlp = 0;
		    else uhtz[nrd]= 0;
		    uhtz[sizeof(timez)-4] = 0;
		    (void)strcpy(timez, "TZ=");
		    ENVSTRNCAT(timez, uhtz);
		    putenv(timez);
		}
		(void)close(fd);
	    }
	}

	if (ia_uid == 0) {
		if (!(Def_path = Def_supath) || !*Def_path)
			Def_path = DEF_SUPATH;
	} else {
		if (!Def_path || !*Def_path)
			Def_path = DEF_PATH;
	}

	ENVSTRNCAT(path, Def_path);

	ENVSTRNCAT(home, dirp);
	ENVSTRNCAT(user, u_name);
	ENVSTRNCAT(logname, u_name);

	/* Find the end of the basic environment */

	for (basicenv = 0; envinit[basicenv] != NULL; basicenv++);

	if (*shellp[0] == '\0') { 
		/*
		 * If possible, use the primary default shell,
		 * otherwise, use the secondary one.
		 */
		if (access(SHELL, X_OK) == 0)
			*shellp = SHELL;
		else
			*shellp = SHELL2;

	} else 
		if (Altshell && *Altshell && 
		    strncasecmp(Altshell, "YES", 3) == 0)
			envinit[basicenv++] = shell;

	ENVSTRNCAT(shell, *shellp);

        if (remotehost[0] != '\0') {     /* install remote host name */
                envinit[basicenv++] = env_remotehost;
				ENVSTRNCAT(env_remotehost,remotehost);
		}
        else if (0 != (ptr=getenv("REMOTEHOST")))
                envinit[basicenv++]=strncat(env_remotehost,ptr,MAXHOSTNAMELEN);
		if (rusername[0] != '\0') {       /* and remote user name */
                envinit[basicenv++] = env_remotename;
                ENVSTRNCAT(env_remotename,rusername);
		}
        else if (0 != (ptr=getenv("REMOTEUSER")))
                envinit[basicenv++] = strncat(env_remotename,ptr,NMAX);
        else envinit[basicenv++] = strcat(env_remotename, "UNKNOWN");

#ifndef	NO_MAIL
	envinit[basicenv++] = mail;
	(void) strcat(mail,_PATH_MAILDIR);
	ENVSTRNCAT(mail,u_name);
#endif

#ifdef DCE
	if (dfs_authentication) {
		envinit[basicenv++] = dce_tktfile;
		ENVSTRNCAT(dce_tktfile, getenv("KRB5CCNAME"));
	}
#endif /* DCE */

#ifdef EXECSH
	/*
	 * Add/replace environment variables from telnetd.
	 */
	if (pflag && renvp != NULL) {
		for (j=0; *renvp && j < MAXARGS-1; j++,renvp++) {

			    /* 
			     * Ignore if it doesn't have the format xxx=yyy 
			     * or it is not an alterable variable.
			     */

			    if ((endptr = strchr(*renvp,'=')) == NULL ||
				 !legalenvvar(*renvp))
				    continue;

			    /* 
			     * Replace any previously-defined string or
			     * append it to the list.
			     */

			    length = endptr + 1 - *renvp;
			    for (i = 0; i < basicenv; i++) {
				    if (!strncmp(*renvp, envinit[i], length)) {
					    envinit[i] = *renvp;
					    break;
				    }
			    }
			    if (i == basicenv)
				    envinit[basicenv++] = *renvp;
		}
	}


#endif /* EXECSH */

	/*
	 * Add in all the environment variables picked up from the
	 * argument list to "login" or from the user response to the
	 * "login" request.
	*/

	for (j = 0,k = 0,l_index = 0,ptr = &envblk[0]; *envp && j < (MAXARGS-1);
		j++, envp++) {

	/* Scan each string provided.  If it doesn't have the format */
	/* xxx=yyy, then add the string "Ln=" to the beginning. */

		if ((endptr = strchr(*envp,'=')) == (char*)NULL) {
			envinit[basicenv+k] = ptr;
			(void) snprintf(ptr, (int)MAXENV, "L%d=%s",l_index,*envp);

	/* Advance "ptr" to the beginning of the next argument.	*/

			while(*ptr++);
			k++;
			l_index++;
		}

	/* Is this an environmental variable we permit?	*/

		else if (!legalenvvar(*envp))
			continue;

	/* Check to see whether this string replaces any previously- */
	/* defined string. */

		else {
			for (i = 0, length = endptr+1-*envp; i < basicenv+k; i++ ) {
				if (strncmp(*envp, envinit[i], length) == 0) {
					envinit[i] = *envp;
					break;
				}
			}

	/* If it doesn't, place it at the end of environment array. */

			if (i == basicenv+k) {
				envinit[basicenv+k] = *envp;
				k++;
			}
		}
	}
#ifdef EXECSH
	environ = envinit;
#endif /* EXECSH */
	(void)set_lang(envinit);
}


/*
 * Procedure:	pr_msgs
 *
 *
 * Notes:	prints any advisory messages such as the Copyright
*/
static	void
pr_msgs(int lastlog_msg)
{
	struct utsname un;

#ifndef sgi
	(void) alarm(0);
#endif

	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGINT, SIG_DFL);
	(void) nuname(&un);

	pfmt(stdout, MM_NOSTD,
		"uxsgicore:704:IRIX Release %s %s %s\n\
Copyright 1987-2000 Silicon Graphics, Inc. All Rights Reserved.\n",
		un.release, un.machine, un.nodename);

	/*	
	 *	Advise the user the time and date that this login-id
	 *	was last used. 
 	*/

	if (lastlog_msg && (access(".hushlogin", F_OK) != 0)) {
		char	timebuf[256];
		int	size;
		struct	tm	*ltime;

		ltime = localtime(&ll.ll_time);
		size = strftime(timebuf, sizeof(timebuf), "%KC", ltime);

		if (ll.ll_host[0] && ll.ll_line[0])
			pfmt(stdout, MM_NOSTD,
				":748:Last login: %.*s by %.*s@%.*s\n",
				size, timebuf, sizeof(ll.ll_line), ll.ll_line,
				sizeof(ll.ll_host), ll.ll_host);
		else if (ll.ll_host[0])
			pfmt(stdout, MM_NOSTD,
				":329:Last login: %.*s from %.*s\n",
				    size, timebuf, sizeof(ll.ll_host), ll.ll_host);
		else
			pfmt(stdout, MM_NOSTD,
				":330:Last login: %.*s on %.*s\n",
				    size, timebuf, sizeof(ll.ll_line), ll.ll_line);
	}
}


/*
 * Procedure:	update_utmp
 *
 * Restrictions:
 *		pututxline:	None
 *		getutxent:	P_MACREAD
 *		updwtmpx:	P_MACREAD
 *
 * Notes:	updates the utmpx and wtmpx files.
*/
static	void
update_utmp(struct utmpx *utmp)
{
	register struct	utmpx	*u;
	cap_t ocap;
	cap_value_t caps[] = {CAP_DAC_WRITE, CAP_MAC_WRITE};

	(void) time(&utmp->ut_tv.tv_sec);
#ifdef EXECSH
	utmp->ut_pid = getpid();
#else
	utmp->ut_pid = getppid();
#endif /* EXECSH */

	/*
	 * Find the entry for this pid in the utmp file.
	*/

	ocap = cap_acquire (2, caps);
	while ((u = getutxent()) != NULL) {
		if (((u->ut_type == INIT_PROCESS ||
			u->ut_type == LOGIN_PROCESS)  &&
			(u->ut_pid == utmp->ut_pid)) ||
			((u->ut_type == USER_PROCESS) &&
			((u->ut_pid  == utmp->ut_pid) ||
			!strncmp(u->ut_line,basename(ttyn),
				sizeof(u->ut_line))))) {

	/* Copy in the name of the tty minus the "/dev/", the id, and set */
	/* the type of entry to USER_PROCESS.				  */
			SCPYN(utmp->ut_line,(ttyn + sizeof("/dev/")-1));
			utmp->ut_id[0] = u->ut_id[0];
			utmp->ut_id[1] = u->ut_id[1];
			utmp->ut_id[2] = u->ut_id[2];
			utmp->ut_id[3] = u->ut_id[3];
			utmp->ut_type = USER_PROCESS;

	/* Write the new updated utmp file entry. */

			pututxline(utmp);
			break;
		}
	}
	endutxent();		/* Close utmp file */
	cap_surrender (ocap);

	if (u == (struct utmpx *)NULL) {
#ifndef EXECSH
		cap_value_t capv = CAP_AUDIT_WRITE;
		ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0,
			"cannot execute login at shell level");
		cap_surrender(ocap);
		pfmt(stderr, MM_ERROR, ":666:cannot execute login at shell level.\n");
		exit(1);
#endif /* EXECSH */
	}
	else {
		/*
		 * Now attempt to write out this entry to the wtmp file
		 * if we were successful in getting it from the utmp file
		 *  and the wtmp file exists.
		*/
		ocap = cap_acquire (2, caps);
		updwtmpx(WTMPX_FILE, utmp);
		cap_surrender (ocap);
	}

}



/*
 * Procedure:	verify_pwd
 *
 * Restrictions:
 *		exec_pass():	none
 *
 * Notes:	execute "/usr/bin/passwd" if passwords are required
 *		for the system, the user does not have a password,
 *		AND the user's NULL password can be changed accord-
 *		ing to its password aging information.
 *
 *		It also calls the program "/usr/bin/passwd" if the
 *		"-p" flag is present on the input line indicating the
 *		user wishes to modify their password.
*/
static	void
verify_pwd(int nopass, uid_t priv_uid)
{
	time_t	now;
	long	ia_lstchg, ia_min,
			ia_max, ia_warn;
	register int	n,
			paschg = 0;
	char	*badpasswd = ":148:Cannot execute %s: %s\n";
	cap_t ocap;
	cap_value_t capv;

#ifndef sgi
	(void) alarm(0);	/*give user time to come up with new password */
#endif

	now = DAY_NOW;

	/* get the aging info */

	ia_get_logmin(uinfo, &ia_min);
	ia_get_logmax(uinfo, &ia_max);
	ia_get_logchg(uinfo, &ia_lstchg);
	ia_get_logwarn(uinfo, &ia_warn);

	if((rflag && (usererr == -1)) || !rflag){
		if (nopass && (ia_uid != priv_uid)) {
			if (Passreq && *Passreq && 
			    !strncasecmp("YES", Passreq, 3)  &&
			    ((ia_max == -1) || (ia_lstchg > now) ||
			     ((now >= ia_lstchg + ia_min) &&
			      (ia_max >= ia_min)))) {
				pfmt(stderr, MM_ERROR,
					":322:You don't have a password.\n");
				pfmt(stderr, MM_ACTION, ":323:Choose one.\n");
				(void) fflush(stderr);
				n = exec_pass(u_name);
				if (n > 0) {
					exit(9);
				}
				if (n < 0) {
					pfmt(stderr, MM_ERROR, badpasswd,
						ia_pwdpgm, strerror(errno));
					exit(9); 
				}
				paschg = 1;
				ia_lstchg = now;
			}
		}
	}

	/* Is the age of the password to be checked? */

	if ((ia_lstchg == 0) ||
	    (ia_lstchg > now) ||
	    ((ia_max >= 0) && (now > (ia_lstchg + ia_max)) && (ia_max >= ia_min))) {
		/* don't make the idleweeks tests if last change == 0, which happens when
		 * the admin expires the passwd via passwd -f (bug fix)
		 */
		if ((ia_lstchg != 0) && 
		    ((Idleweeks == 0) ||
		     ((Idleweeks > 0) && (now > (ia_lstchg + (7 * Idleweeks)))))) {
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0,
				 "password has been expired for too long");
			cap_surrender(ocap);
			pfmt(stderr, MM_ERROR,
			     ":324:Your password has been expired for too long\n");
			pfmt(stderr, MM_ACTION,
			     ":133:Consult your system administrator\n");
			exit(1);
		}
		else {
			pfmt(stderr, MM_ERROR, ":325:Your password has expired.\n");
			pfmt(stderr, MM_ACTION, ":326:Choose a new one\n");
			n = exec_pass(u_name);
			if (n > 0) {
				exit(9);
			}
			if (n < 0) {
				pfmt(stderr, MM_ERROR, badpasswd, ia_pwdpgm,
					strerror(errno));
				exit(9);
			}
		}
		paschg = 1;
		ia_lstchg = now;
	}

	if (pwflag) {
		if (!paschg) {
			n = exec_pass(u_name);
			if (n > 0)
				pfmt(stderr, MM_WARNING, ":677:Password unchanged\n");
			if (n < 0)
				pfmt(stderr, MM_WARNING, badpasswd, ia_pwdpgm,
					strerror(errno));
		}
		ia_lstchg = now;
	}
	/* Warn user that password will expire in n days */

	if ((ia_warn > 0) && (ia_max > 0) &&
	            (now + ia_warn) >= (ia_lstchg + ia_max)) {

		int	xdays = (ia_lstchg + ia_max) - now;

		if (xdays) {
			if (xdays == 1)
				(void) pfmt(stderr, MM_INFO,
				":678:Your password will expire in 1 day\n");
			else
				(void) pfmt(stderr, MM_INFO,
				":327:Your password will expire in %d days\n", xdays);
		}
		else {
			pfmt(stderr, MM_ERROR, ":325:Your password has expired.\n");
			pfmt(stderr, MM_ACTION, ":326:Choose a new one\n");
			n = exec_pass(u_name);
			if (n > 0) {
				exit(9);
			}
			if (n < 0) {
				pfmt(stderr, MM_ERROR, badpasswd, ia_pwdpgm,
					strerror(errno));
				exit(9);
			}
			paschg = 1;
		}
	}
}




/*
 * Procedure:	init_badtry
 *
 * Restrictions:
 *		stat(2):	none
 *
 * Notes:	if the logfile exist, turn on attempt logging and
 *	 	initialize the string storage area.
*/
static	int
init_badtry(char **log_entry)
{
	register int	i, dolog = 0;
	struct	stat	dbuf;

	if (stat(LOGINLOG, &dbuf) == 0) {
		dolog = 1;
		for (i = 0; i < Def_failures; i++) {
			if (!(log_entry[i] = (char *) malloc((unsigned)ENT_SIZE))) {
				dolog = 0 ;
				break ;
			}
			*log_entry[i] = '\0';
		}	
	}
	return dolog;
}


/*
 * Procedure:	logbadtry
 *
 * Notes:	Writes the failed login attempt to the storage area.
*/
static	void
logbadtry(int trys, char **log_entry)
{
	long	timenow;

	if (trys && (trys <= Def_failures)) {
		(void) time(&timenow);
		(void) strncat(log_entry[trys-1], u_name, LNAME_SIZE);
		(void) strncat(log_entry[trys-1], ":", (size_t) 1);
		(void) strncat(log_entry[trys-1], rttyn, TTYN_SIZE);
		(void) strncat(log_entry[trys-1], ":", (size_t) 1);
		(void) strncat(log_entry[trys-1], ctime(&timenow), TIME_SIZE);
	}
}


/*
 * Procedure:	on_console
 *
 * Notes: 	If the "priv_uid" is equal to the user's uid, the login
 *		will be disallowed if the user is NOT on the system
 *		console.
*/
static	int
on_console(uid_t priv_uid)
{
	cap_t ocap;
	cap_value_t capv;

	if (ia_uid == priv_uid) {
		if (Console && *Console && (strcmp(rttyn, Console) != 0)) {
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0,
				"Root login on other than system console");
			cap_surrender(ocap);
			return 0;
		}
		if (Def_supath && *Def_supath)
			Def_path = Def_supath;
		else
			Def_path = DEF_SUPATH;
	}
	return 1;
}


/*
 * Procedure:	read_pass
 *
 * Notes:	gets user password and checks if MANDPASS is required.
 *		returns 1 on failure, 0 on success.
*/
static	int
read_pass(uid_t priv_uid, int *nopass)
{
	char	*ia_pwdp,
		*pwdmsgid = ":308",
		*pwdmsg = "Password:";
	int	mandatory = 0;
	cap_t	ocap;
	cap_value_t capv;

	ia_get_logpwd(uinfo, &ia_pwdp);

	/*
	 * if the user doesn't have a password check if the privilege
	 * mechanism is ID-based.  If so, its OK for a privileged user
	 * not to have a password.
	 *
	 * If, however, the MANDPASS flag is set and this user doesn't
	 * have a password, set a flag and continue on to get the
	 * user's password.  Otherwise, return success because its OK
	 * not to have a password.
	*/
	if (*ia_pwdp == '\0') {
		if (ia_uid == priv_uid)
			return 0;
		if (Mandpass && *Mandpass && 
		    !strncasecmp("YES", Mandpass, 3)) {
			mandatory = 1;
		}
		else {
			return 0;
		}
	}
#ifdef DCE
	dfs_authentication = 0;
	if (strcmp(ia_pwdp,"-DCE-") == 0) {
		void *handle=NULL;
		/*
		 * Try to load the DCE library and ensure that
		 * dce_verify exists.
		 */
		if (handle=sgidladd("libdce.so",RTLD_LAZY)) {
			if (dlsym(handle,"dce_verify")) {
				dfs_authentication = 1;
			} else {
				capv = CAP_AUDIT_WRITE;
				ocap = cap_acquire (1, &capv);
				ia_audit("LOGIN", u_name, 0,
					 "DCE integrated login failed, incorrect DCE library");
				cap_surrender(ocap);
			}
		} else {
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0,
				 "DCE integrated login failed, unable to load DCE library");
			cap_surrender(ocap);
		}
	}
#endif /* DCE */
	/*
	 * get the user's password, turning off echoing.
	*/
	if (gpass(gettxt(pwdmsgid, pwdmsg), ia_pwdp, priv_uid)) {
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "Invalid password");
		cap_surrender(ocap);
		return 1;
	}
	/*
	 * Doesn't matter if the user entered No password.  Since MANDPASS
	 * was set, make it look like a bad login attempt.
	*/
	if (mandatory) {
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0,
			"Passwords are mandatory, but this account has none");
		cap_surrender(ocap);
		return 1;
	}
	/*
	 * everything went fine, so indicate that the user had a password
	 * and return success.
	*/
	else {
		*nopass = 0;
		return 0;
	}
}


/*
 * Procedure:	get_logoffval
 *
 * Notes:	The following is taken directly from the SVR4.1 require-
 *		ments relating to the functionality of the MAXTRYS and
 *		LOGFAILURES tuneables:
 *
 *		1.  Users will be allowed LOGFAILURES (will be set  to  5)
 *		    attempts  to successfully log in at each invocation of
 *		    login.
 *
 *		2.  If  the  file  LOGINLOG  (will  be   defined   to   be
 *		    /var/adm/loginlog) exists, all LOGFAILURES consecutive
 *		    unsuccessful  login  attempts  will   be   logged   in
 *		    LOGINLOG.   After  LOGFAILURES  unsuccessful attempts,
 *		    login will sleep for DISABLETIME before  dropping  the
 *		    line.  In  other  words, if a person tried five times,
 *		    unsuccessfully, to log in  at  a  terminal,  all  five
 *		    attempts  will  be  logged  in /var/adm/loginlog if it
 *		    exists.   The  login  command  will  then  sleep   for
 *		    DISABLETIME  seconds  and  drop the line. On the other
 *		    hand,  if  a  person  has  one  or  two   unsuccessful
 *		    attempts, none of them will be logged.
 *
 *		      => Note: Since LOGFAILURES can now  be  set  by  the
 *		      administrator, it may be set to 1 so that any number
 *		      of failed login attempts are recorded.  When  either
 *		      MAXTRYS  or  LOGFAILURES  is reached login will exit
 *		      and the user will be disconnected from  the  system.
 *		      The   difference   being   that   in   the  case  of
 *		      LOGFAILURES, a record  of  bad  login  attempts  are
 *		      recorded in the system logs.
 *
 *		3.  by default, MAXTRYS and LOGFAILURES will be set to 5,
 *
 *		4.  if set, MAXTRYS  must  be  >=  0.   If  MAXTRYS=0  and
 *		    LOGFAILURES  is  not set, then login will not kick the
 *		    user  off  the  system  (unlimited  attempts  will  be
 *		    allowed).
 *
 *		5.  if set, LOGFAILURES must be within the range of  0-20.
 *		    If  LOGFAILURES  is  = 0, and MAXTRYS is not set, then
 *		    login will not kick off the user  (unlimited  attempts
 *		    will be allowed).
 *
 *		6.  if  LOGFAILURES  or  MAXTRYS   are   not   set,   then
 *		    respectively,  the  effect will be as if the item were
 *		    set to 0,
 *
 *		7.  the lowest positive number of MAXTRYS and  LOGFAILURES
 *		    will  be  the  number of failed login attempts allowed
 *		    before  the  appropriate  action  is  taken  (e.g.  1,
 *		    MAXTRYS  =  3  and LOGFAILURES=6 then the user will be
 *		    kicked off the system after 3 bad login  attempts  and
 *		    IN  NO  CASE  shall  bad  login  records end up in the
 *		    system log file (var/adm/loginlog).  e.g. 2, MAXTRYS=6
 *		    and  LOGFAILURES=3,  then  the user will be kicked off
 *		    the system after 3 bad  login  attempts  and  at  that
 *		    point  in  time,  3  records  will  be recorded in the
 *		    system log file.)
 *
 *		8.  in the case when  both  values  are  equal,  then  the
 *		    action of LOGFAILURES will dominate (i.e., a record of
 *		    the bad login attempts will be  recorded  in  the  log
 *		    files).
*/
static	long
get_logoffval(void)
{
	if (Def_maxtrys == Def_failures)		/* #8 */
		return Def_failures;

	if (!Def_maxtrys && (Def_failures < 2))		/* #4, #5, and #6 */
		return 0;

	if (Def_maxtrys < Def_failures) {		/* #7, example 1 */
		Def_failures = 0;
		return Def_maxtrys;
	}
	/*
	 * Def_maxtrys MUST be greater than Def_failures so
	 * return Def_failures!
	*/
	return Def_failures;				/* #7, example 2 */
}


/*
 * Procedure:	at_shell_level
 *
 * Restrictions:
 *		getutxent:	P_MACREAD
 *
 * Notes:	determines if the login scheme was called by the user at
 *		the shell level.  If so, this is forbidden.
*/
static	int
at_shell_level(void)
{
#ifndef EXECSH
	register struct	utmpx	*u;
	pid_t			tmppid;

	tmppid = getppid();

	/*
	 * Find the entry for this pid in the utmp file.
	*/

	while ((u = getutxent()) != NULL) {
		if (((u->ut_type == INIT_PROCESS ||
			u->ut_type == LOGIN_PROCESS)  &&
			(u->ut_pid == tmppid)) ||
			((u->ut_type == USER_PROCESS) &&
			(u->ut_pid  == tmppid) &&
			(!strncmp(u->ut_user, ".", 1)))) {

			break;
		}
	}
	endutxent();		/* Close utmp file */

	if (u == (struct utmpx *)NULL)
		return 1;
#endif /* !EXECSH */	 
	return 0;
}

/*
 * Put string in SYSLOG on failure
 */

static void
failure_log(void)
{
	if (syslog_fail) {
		openlog("login", LOG_PID, LOG_AUTH);
		if (rflag_set || hflag) {
			syslog(LOG_INFO|LOG_AUTH, "failed: %s@%s as %s",
				hflag ? "?" : rusername, remotehost, u_name);
		} else
			syslog(LOG_NOTICE|LOG_AUTH, "failed: %s on %s",
				u_name, ttyn);
		closelog();
	}
}

/*
 * display pre-prompt messages
 */

static void
early_advice(int rflag, int hflag)
{
	FILE	*fp;
	char	inputline[MAXLINE];
	cap_t	ocap;
	cap_value_t capv;

	if (rflag || hflag) {
		/* If rlogins are disabled, print out the message. */
		if ((fp = fopen(_PATH_NOLOGIN,"r")) != NULL) {
			while (fgets(inputline,sizeof(inputline),fp) != NULL) {
				fputs(inputline,stdout);
				putc('\r',stdout);
			}
			fflush(stdout);
			capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
			ia_audit("LOGIN", u_name, 0, "Remote logins disabled");
			cap_surrender(ocap);
			sleep(5);
			exit(1);
		}

		/* Print out the issue file. */

		if ((fp = fopen(_PATH_ISSUE,"r")) != NULL) {
			while (fgets(inputline,sizeof(inputline),fp) != NULL) {
				fputs(inputline,stdout);
				putc('\r',stdout);
			}
			/* 
		 	 * Insert extra newline otherwise gpass prints 
		 	 * passwdmsg and puts cursor at beginning of the line.
		 	 */
			putc('\n',stdout);
			fclose(fp);
		}
	}
}

/*
 * set_uthost() is called only for remote logins.  It constructs the
 * ut_host string for the current utmpx entry.
 *   - If the remote and local usernames are different, it copies the
 *     remote username followed by an '@', then the remotehostname into
 *     ut_hostp.  The entire remote hostname is saved; the remote user 
 *     is truncated as necessary.
 *   - if the local and remote usernames match, only the remotehostname
 *     is copied.
 *
 * Note that `rusername', `luser',  and `remotehost' must be initialized
 * before set_uthost() is invoked.
 */
static int
set_uthost(
    char *ut_hostp,	/* constructed ut_host string is copied here */
    int uthlen)		/* max strlen(ut_hostp) allowed */
{
	int rhostlen, ccnt;
	int rumaxlen;		/* max # of chars to grab from ruser */

	ut_hostp[0] = '\0';
	if (remotehost[0] == '\0') {	/* ???? */
		return(0);
	}

	rhostlen = strlen(remotehost);
	rumaxlen = 0;
	if (rhostlen < (uthlen-1) &&	/* only room for '@' if 1 shorter; */
	    rusername[0]!='\0' && luser[0]!='\0' && strcmp(rusername,luser)) {
		rumaxlen = (uthlen - rhostlen - 1);
	}

	ccnt = sprintf(ut_hostp, "%.*s%s%.*s", rumaxlen, rusername,
		(rumaxlen > 0 ? "@" : ""), uthlen, remotehost);
	return(ccnt);
}





#ifdef AUX_SECURITY

static unsigned char
dositecheck(void)
{
	auto int	wstatus;
	struct stat	sbuf;
	char		*newargv[5];
	pid_t		sitepid, rv;
	int		i;
	cap_t		ocap;
	cap_value_t	capv;

	i = 0;
	newargv[i++] = Def_sitepath;
	newargv[i++] = u_name;
	if (remotehost[0] != '\0') {
		newargv[i++] = (char *) remotehost;
	}
	if (rusername[0] != '\0')  {
		newargv[i++] = (char *) rusername;
	}

	newargv[i] = NULL;

	/*
	 * Enforce security items.  Check that the program is owned by root,
	 * and is not world-writable.  Return SITE_CONTINUE if it is not,
	 * thus causing a traditional unix authentication.
	 */

	if (stat(Def_sitepath,&sbuf) != 0) {
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "cannot access sitecheck");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}

	if (sbuf.st_uid != 0) {
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "bad sitecheck ownership");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}

	if (sbuf.st_mode & 0022) {
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "bad sitecheck permissions");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}
	/* fork, exec, and wait for return code of siteprog */

	switch (sitepid = fork()) {
	case -1:
		return(SITE_FAIL);
	case 0:
		signal(SIGALRM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		execv(Def_sitepath, newargv);
		capv = CAP_AUDIT_WRITE, ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", u_name, 0, "can't exec sitecheck program");
		cap_surrender(ocap);
		return(SITE_CONTINUE);
	}

	rv = waitpid(sitepid, &wstatus, 0);
	if (rv < 0)
		return SITE_FAIL;
	else if (WIFEXITED(wstatus))
		return WEXITSTATUS(wstatus);
	else {
		if (WCOREDUMP(wstatus))
			return SITE_CONTINUE;
		else
			return SITE_FAIL;
	}
}
#endif /* AUX_SECURITY */

/*
 * Return the value of the requested variable in the environment string.
 */
static char *
getsenv(char *varname, char **envp)
{
	int s = strlen(varname);
	char *ep;

	for (ep = *envp; ep = *envp; envp++) {
		if (strncmp(varname, ep, s))
			continue;
		if (ep[s] != '=')
			continue;
		return (ep + s + 1);
	}
	return (NULL);
}


#define BL_WRITE		1
#define BL_LOCK			2
#define BL_BADFILE		3
#define BL_BADLOCK		4
#define BL_LOCK_BADFILE		5

/*
 *  Call update_count and log result to syslog and system audit trail.
 */
static void
count_badlogins(int outcome, char *username)
{
	cap_t ocap;
	cap_value_t capv = CAP_AUDIT_WRITE;

	if (Lockout <= 0)
		return;

	switch(update_count(outcome, username)) {
	case BL_LOCK:
		syslog(LOG_NOTICE|LOG_AUTH, "Locked %s account\n", username);
		ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", username, 1, "Locked account");
		cap_surrender(ocap);
		break;
	case BL_BADFILE:
		ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", username, 0, "Can't update badlogin file");
		cap_surrender(ocap);
		syslog(LOG_ALERT|LOG_AUTH, "Can't update %s badlogin count", 
		       username);
		break;
	case BL_BADLOCK:
		ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", username, 0, "Can't lock account");
		cap_surrender(ocap);
		syslog(LOG_ALERT|LOG_AUTH, "Can't  lock %s account", username);
		break;
	case BL_LOCK_BADFILE:
		ocap = cap_acquire (1, &capv);
		ia_audit("LOGIN", username, 0, 
			 "Locked account but can't update badlogin file");
		cap_surrender(ocap);
		syslog(LOG_ALERT|LOG_AUTH, 
		       "Can't update %s badlogin count", username);
		break;
	default:
		break;
	}
}

#define MATCH 1
#define NOMATCH 0
#define SKIP_DELIM(a)            {while (*a && (*a == NAME_DELIM)) a++;}

static int
notlockout(char *user)
{
        char notlockname[LNAME_SIZE+1];
	char * name_begin_ptr;
	char * name_delim_ptr;
	int    namelen=0;

	if (Def_notlockout == NULL)
	  return (NOMATCH);

	notlockname[0]='\0';
	name_begin_ptr = Def_notlockout;
	SKIP_DELIM(name_begin_ptr);

	/* Iterate through the list of names until we reach \0 */
	while (*name_begin_ptr != NULL) {
	  name_delim_ptr = strchr(name_begin_ptr,NAME_DELIM);
	  
	  /* strchr didn't find any more delimiters, but we still
	   * have to deal with the last name in the list
	   */
	  if (name_delim_ptr != NULL)
            namelen = name_delim_ptr - name_begin_ptr;
	  else
            namelen = strlen(name_begin_ptr);

	  /* name must be a valid length <= LNAME_SIZE,
	   * we don't want to overflow on the stack, and we
	   * don't want to just chop off the name at LNAME_SIZE 
	   * since that would invalidate the identity.
	   */
	  if ((namelen > 0) && (namelen <=LNAME_SIZE)){
	    strncpy(notlockname, name_begin_ptr, namelen);
	    notlockname[namelen]='\0';
	    if (strcmp(user, notlockname) == 0)
	      return (MATCH);
          }
	  /* Increment the name_begin_ptr, and handle  
	   * termination of the loop is there isn't
	   * a match in the list.
	   */
	  if (name_delim_ptr != NULL) {
	    name_begin_ptr=name_delim_ptr;
	    SKIP_DELIM(name_begin_ptr);
	  }
	  else
	    name_begin_ptr = name_begin_ptr + namelen; 
	}
	return (NOMATCH);
}



/*
 * On successful login (outcome=1), reset count to zero.
 * Otherwise, increment count, test whether count has reached
 * Lockout value, and lock user's account if it has.
 */
static int
update_count(int outcome, char *user)
{
	char *badlogfile;
	int fd, retval, wstatus;
	short new = 0;
	short lockok = 0;
	char count;
	struct passwd *pw;
	struct stat  bl_buf;

	/* Validate that this user exists, we don't use uinfo (iaf)
	 * structure here, because we're not guaranteed that it exists
	 * when this routine is called. BUG #491422
	 */
	pw = getpwnam(user);
	
	/* Don't process LOCKOUT if the user doesn't exist.
	 * BUG #491422
	 */
	if (pw != NULL) {

		/* 
		 * Open user's badlog file.
		 */
		if (access(BADLOGDIR, 0) < 0) {
			if (mkdir(BADLOGDIR, 0700) < 0)
				return(BL_BADFILE);
		}
		if ((badlogfile = (char *)malloc(sizeof(BADLOGDIR) + 
						 strlen(user) + 3)) == NULL) {
			return(BL_BADFILE);
		}
		sprintf(badlogfile, "%s/%s", BADLOGDIR, user);
		/*
		 * If file is new, count is zero.
		 * Use stat instead of access to avoid the real uid/gid
		 * problems when login not started as root.  BUG #506487
		 */
		if (stat(badlogfile, &bl_buf) < 0) {
			new = 1;
			count = '\0';
		}
		fd =open(badlogfile, O_RDWR | O_CREAT, 0600);
		free(badlogfile);
		if (fd < 0 || flock(fd, LOCK_EX) < 0) {
			close(fd);
			return(BL_BADFILE);
		}
		if (outcome == 0) { 	
			/* 
			 * Failed login: read count and seek back to start.
			 */
			if (!new && read(fd, &count, 1) != 1) {
				close(fd);
				return(BL_BADFILE);
			}
			if (lseek(fd, 0, SEEK_SET) < 0) {
				close(fd);
				return(BL_BADFILE);
			}
			/*
			 * If updated count has reached Lockout value,
			 * invoke passwd -l and reset count to zero - UNLESS
			 * (UNLESS!) the user is on the LOCKOUTEXEMPT list in
			 * the options file.  Such a user will have his count
			 * updated when a failed login attempt is made, but
			 * will not be locked out when the Lockout count is
			 * exceeded. This feature is the result of Bug 491422,
			 * to address the denial of service attacks possible
			 * when the LOCKOUT option is in use.
			 */
			if ((++count >= Lockout) &&
			    (notlockout(user) == NULL)) {  
				retval = fork();
				switch (retval) {
					case -1:
						close(fd);
						return (BL_BADLOCK);
					case 0:
						close(fd);
						signal(SIGALRM, SIG_DFL);
						signal(SIGHUP, SIG_DFL);
						execl("/bin/passwd", "passwd",
						      "-l", user, (char *)0);
						exit(1);
				}
				if (waitpid(retval, &wstatus, 0) < 0 ||
				    wstatus != 0) {
					close(fd);
					return (BL_BADLOCK);
				}
				count = '\0';
				lockok = 1;
			}
		}
		else { 		
			/* 
			 * On successful login, reset count to zero. 
			 */
			count = '\0';
		}
		/*
		 * Write back updated count.
		 */
		if (write(fd, &count, 1) != 1) {
			close(fd);
			if (lockok)
				return(BL_LOCK_BADFILE);
			return(BL_BADFILE);
		}
		else {
			close(fd);
			if (lockok)
				return(BL_LOCK);
			return(BL_WRITE);
		}
	}
	return(BL_LOCK);
}

/*
 * Determine if `user' is cleared for capability state `cap'.
 * This function is dependent upon the internal representation
 * of cap_t.
 */
static int
cap_user_cleared (const char *user, const cap_t cap)
{
	struct user_cap *clp;
	cap_t pcap;
	int result;

	if ((clp = sgi_getcapabilitybyname(user)) == NULL)
		pcap = cap_init();
	else
		pcap = cap_from_text(clp->ca_allowed);

	result = CAP_ID_ISSET(cap->cap_effective, pcap->cap_effective) &&
	    CAP_ID_ISSET(cap->cap_permitted, pcap->cap_permitted) &&
	    CAP_ID_ISSET(cap->cap_inheritable, pcap->cap_inheritable);

	(void) cap_free(pcap);

	return result;
}
