/*
 * TCP/IP server for uucico derived from 4.2BSD.
 *
 * uucico's TCP channel causes this server to be run at the remote end.
 */

#include "uucp.h"
#include <syslog.h>
#include <netdb.h>
#define _BSD_SIGNALS
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <unistd.h>
#include <utmpx.h>
#include <getopt.h>
#include <paths.h>
#include <crypt.h>


extern struct hostent * __getverfhostent(struct in_addr, int);
extern int initgroups(char *, gid_t);

static char *remotehost;

struct sockaddr_in from;
int fromlen;

int check_all = 0;
int strip_mask = 0x7f;
char *x_arg, *x_num;
FILE *logfile = 0;

char *uucico_test = UUCICO;

#define NMAX 64
static char Username[5+NMAX] = { "USER=" };
static char timez[64] = { "TZ=" };
static char Rhost[12+MAXHOSTNAMELEN+1] = { "REMOTEHOST=" };
static char Ruser[] = { "REMOTEUSER=UNKNOWN"};
#define TZ_INDEX 3
char *nenv[] = {
	Username,
	Ruser,
	Rhost,
	timez,
	0,
};
extern char **environ;

static int readline(char *p, int n);
static void dologin(struct passwd *pw);
static void dologout(void);


void
main(int argc, char **argv)
{
	char user[NMAX], passwd[NMAX];
	char *cp;
	int i;
	struct passwd *pw;
	int on = 1;
	pid_t pid;
	int status;


	openlog("uucpd", LOG_PID | LOG_CONS, LOG_AUTH);

	opterr = 0;
	while ((i = getopt(argc, argv, "anx:L:u:")) != EOF) {
		switch (i) {
		case 'a':
			check_all = 1;
			break;
		case 'n':		/* "no strip" */
			strip_mask = 0xff;
			break;
		case 'x':
			x_arg = "-x";
			x_num = optarg;
			break;
		case 'L':
			logfile = fopen(optarg, "a+");
			if (!logfile)
				syslog(LOG_ERR, "fopen(%s): %m", optarg);
			break;
		case 'u':
			uucico_test = optarg;
			break;
		case '?':
		default:
			syslog(LOG_ERR,
			       "usage: uucpd [-an] [-x num] [-L file]"
			       " [-u pgm], not %s", argv[1]);
			exit(1);
		}
	}


	fromlen = sizeof(from);
	if (getpeername(0, &from, &fromlen) < 0) {
		(void)fprintf(stderr, "%s: ", argv[0]);
		syslog(LOG_ERR, "getpeername: %m");
		exit(1);
	}

	from.sin_port = ntohs((u_short)from.sin_port);
	remotehost = __getverfhostent(from.sin_addr, check_all)->h_name;
	strncat(Rhost,remotehost,sizeof(Rhost)-13);

	if (logfile)
		(void)fprintf(logfile, "%s %d starting\n",
			      remotehost,getpid());

	if (!access(_PATH_NOLOGIN, F_OK)) {
		syslog(LOG_NOTICE, "\"%s\" existed when starting",
		       _PATH_NOLOGIN);
		exit(1);
	}

	if (0 > setsockopt(0, SOL_SOCKET,SO_KEEPALIVE, &on, sizeof(on)))
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");

	alarm(60);
	do {
		printf("\nlogin: "); fflush(stdout);
		if (readline(user, sizeof(user)) < 0) {
			syslog(LOG_AUTH,"login failed: from %s",remotehost);
			exit(1);
		}
	} while (user[0] == '\0');

	pw = getpwnam(user);
	if (pw)
		cp = pw->pw_passwd;

	if (!pw || (cp && *cp != '\0')) {
		printf("Password: "); fflush(stdout);
		if (readline(passwd, sizeof(passwd)) < 0)
			exit(1);
		if (pw && strcmp(crypt(passwd, cp), cp))
			pw = 0;
	}
	alarm(0);

	if (!pw) {
		syslog(LOG_AUTH, "login failed: %s from %s",
		       user, remotehost);
	} else if (pw && strcmp(pw->pw_shell, UUCICO)
		   && strcmp(pw->pw_shell, uucico_test)) {
		pw = 0;
		syslog(LOG_AUTH, "bad shell: user %s from %s",
		       user, remotehost);
	}
	/*
	 * give few clues about the reason for the login failure
	 */
	if (!pw) {
		(void)fprintf(stderr, "Login incorrect.\n");
		exit(1);
	}

	cp = getenv("TZ");
	if (cp != 0)
		strncat(timez, cp, sizeof(timez)-4);
	environ = nenv;

	(void)strcat(Username, user);
	dologin(pw);

	pid = fork();
	if (pid < 0) {
		syslog(LOG_ERR, "fork: %m");

	} else if (!pid) {
		setgid(pw->pw_gid);
		initgroups(pw->pw_name, pw->pw_gid);
		if (0 > chdir(pw->pw_dir)) {
			syslog(LOG_ERR,
			       "Unable to change directory to \"%s\": %m",
			       pw->pw_dir);
			exit(1);
		}
		if (0 > setuid(pw->pw_uid)) {
			syslog(LOG_ERR,
			       "Unable to change UID to %d: %m",
			       pw->pw_uid);
			exit(1);
		}

		closelog();

		execl(pw->pw_shell, "uucico",
		      "-u", user, x_arg, x_num, (char *)0);

		openlog("uucpd", LOG_PID | LOG_CONS, LOG_AUTH);
		syslog(LOG_ERR, "execl: %m");
		exit(1);
	}

	(void)waitpid(pid, &status, 0);
	dologout();
	exit(0);
}


static void
log_c(char c)
{
	c &= 0x7f;
	if (c == 0x7f)
		(void)fputs("^?",logfile);
	else if (iscntrl(c))
		(void)fprintf(logfile, "^%c", (c&0x1f)+'@');
	else
		(void)putc(c,logfile);
}

static void
log_s(char *ptr, int c)
{
	char *p;

	(void)fputs(" \"", logfile);
	for (p = ptr; *p != '\0'; p++)
		log_c(*p);
	if (c >= 0)
		log_c(c);
	(void)fputs("\" <",logfile);
	for (p = ptr; *p != '\0'; p++)
		(void)fprintf(logfile, "%02x", *p);
	if (c >= 0)
		(void)fprintf(logfile, "%02x>\n", c);
	else
		(void)fputs(">\n", logfile);
	fflush(logfile);
}


static int
readline(char *ptr, int n)
{
	char *p;
	char c, c1;

	if (logfile)
		(void)fprintf(logfile, "%s %d:", remotehost,getpid());
	p = ptr;
	while (--n > 0) {
		if (read(0, &c, 1) <= 0) {
			if (logfile) {
				*p = '\0';
				log_s(ptr,-1);
				(void)fputs("--end of connection\n",logfile);
			}
			return(-1);
		}
		c1 = c & strip_mask;
		if (c1 == '\n' || c1 == '\r' || c1 == '\0') {
			*p = '\0';
			if (logfile)
				log_s(ptr,c);
			/* strip it after logging it */
			for (p = ptr; *p != '\0'; p++)
				*p &= strip_mask;
			return(0);
		}
		*p++ = c;
	}

	if (logfile) {
		*--p = '\0';
		log_s(ptr,-1);
		(void)fputs("--shutting off babble\n", logfile);
	}
	return(-1);
}


/*
 * Record login in wtmp and utmp files.
 */

#define	SCPYN(a, b)	(void) strncpy(a, b, sizeof (a))

static struct utmpx utmp;

static char id[] = "ucpA";
#define LINE_ID 3
static char line_nc[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static int line_num = 0;
#define MAX_LINE_NUM (sizeof(line_nc)-1)
#define UTHLEN (sizeof(utmp.ut_host)-1)

static void
dologin(struct passwd *pw)
{
	register struct utmpx *ufound;
	int rh_len, ru_len;

	rh_len = strlen(remotehost);
	if (rh_len > UTHLEN)
		rh_len = UTHLEN;
	/*
	 * If remotehost is short enough, store something in ut_host field to
	 * prevent interpretting the missing remote user info to mean that the
	 * local and remote usernames are the same, which IS what this means
	 * for rlogin utmpx entries.
	 * Compute max # of chars we can grab from "UNKNOWN"
	 * without truncating a remote host string of length 'rhlen'
	 * (-1 for the '@' separator).
	 */
	ru_len = (UTHLEN - rh_len - 1);
	sprintf(&(utmp.ut_host[0]), "%.*s%s%.*s", ru_len,
		"UNKNOWN", (ru_len > 0 ? "@" : ""), rh_len,
		remotehost);
	SCPYN(utmp.ut_name, pw->pw_name);
	utmp.ut_pid = getpid();
	utmp.ut_type = USER_PROCESS;
	utmp.ut_xtime = time(0);
	sprintf(utmp.ut_line, "ucp%d", utmp.ut_pid);

	/* In order to provide additional info, we attempt to add a line
	 * to utmp - note that BSD doesn't do this. Since the 'id' field
	 * is only 4 chars long and must be unique - we can only do this
	 * for MAX_LINE_NUM UUCP connections. Regardless of whether we
	 * find a utmp entry, we need to log this for wtmp.
	 */
	while (line_num <= MAX_LINE_NUM) {  /* try until we get a slot */
		id[LINE_ID] = line_nc[line_num];
		SCPYN(utmp.ut_id, id);
		setutxent();
		ufound = getutxid(&utmp);
		if (!ufound || USER_PROCESS != ufound->ut_type) {
			(void)pututxline(&utmp);
			continue;	/* check that it is not stolen */
		}
		if (ufound->ut_pid == getpid())
			break;
		line_num++;
	}
	if (line_num > MAX_LINE_NUM)
		SCPYN(utmp.ut_id, "ucp?");

	/*
	 * updwtmpx attempts to update both wtmp and wtmpx,
	 * which must be kept in sync for 'last' to produce
	 * any helpful remote-host info.
	 */
	updwtmpx(WTMPX_FILE, &utmp);
	endutxent();
}


/* Record logout in wtmp and utmp files
 */
static void
dologout(void)
{
	utmp.ut_type = DEAD_PROCESS;
	utmp.ut_xtime = time(0);
	utmp.ut_host[0] = '\0';
	if (line_num <= MAX_LINE_NUM) {
		setutxent();
		(void)pututxline(&utmp);
		endutxent();
	}
	/* update wtmp and wtmpx files */
	utmp.ut_user[0] = '\0';
	updwtmpx(WTMPX_FILE, &utmp);
}
