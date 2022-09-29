/*
 * Copyright (c) 1985, 1988, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1985, 1988, 1990 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ftpd.c	5.40 (Berkeley) 7/2/91";
#endif /* not lint */

/*
 * FTP server.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#define	FTP_NAMES
#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>

#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#include <time.h>
#include <pwd.h>
#include <setjmp.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>
#ifndef sgi
#include <varargs.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <cap_net.h>
/* patch 1211 - use ia_* to find out about expired passwords */
#include <shadow.h>
#include <ia.h>
static uinfo_t uinfo;
long ia_expire, ia_lstchg, now;

#include "pathnames.h"
#if defined(_SHAREII) || defined(DCE)
#include <dlfcn.h>
#endif /* defined(_SHAREII) || defined(DCE) */
#ifdef _SHAREII
#include <shareIIhooks.h>
SH_DECLARE_HOOK(SETMYNAME);
SH_DECLARE_HOOK(FTP);
#endif /* _SHAREII */
/*
 * File containing login names
 * NOT to be used on this machine.
 * Commonly used to disallow uucp.
 */

#ifdef sgi
/*
 * Global buffers are page aligned using memalign(), which
 * is a Good Thing if you like page flipping.
 */
#define NBUFS 3
#define FTPBUFSIZ (48*1024)
#define FTPBUFALIGN (16*1024)
long ibufsize, obufsize;
char *ftp_inbuf[NBUFS];
char *ftp_outbuf[NBUFS];
extern	char *crypt();
#include <strings.h>
#include <sys/file.h>
#include <sys/sat.h>
#endif /* sgi */

#ifdef sgi
/*
 * use off64_t, *stat64 and irix (non-bsd) readdir64 for
 * large files and filesystems
 */
#define stat	stat64
#define fstat	fstat64
#define off_t	off64_t
#define readdir	readdir64
#endif

extern	char version[];
extern	FILE *ftpd_popen();
extern	int  ftpd_pclose();
extern	char *getline();
extern  char *ftpd_tilde(char*, char*);
extern	char cbuf[];
extern	off_t restart_point;

struct	sockaddr_in ctrl_addr;
struct	sockaddr_in data_source;
struct	sockaddr_in data_dest;
struct	sockaddr_in his_addr;
struct	sockaddr_in pasv_addr;

int	data;
jmp_buf	errcatch, urgcatch;
int	logged_in;
struct	passwd *pw;
int	debug;
int	timeout = 900;  	/* timeout after 15 minutes of inactivity */
int	portcheck = 0;		/* check that return address = client address */
int	maxtimeout = 7200;/* don't allow idle time to be set beyond 2 hours */
int	logging;
int	guest;
int	type;
int	form;
int	stru;			/* avoid C keyword */
int	mode;
int	usedefault = 1;		/* for data transfers */
int	pdata = -1;		/* for passive mode */
int	transflag;
off_t	file_size;
off_t	byte_count;
#if !defined(CMASK) || CMASK == 0
#undef CMASK
#ifdef sgi
#define CMASK 022
#else
#define CMASK 027
#endif
#endif
int	defumask = CMASK;		/* default umask value */
char	tmpline[7];
char	hostname[MAXHOSTNAMELEN];
char	remotehost[MAXHOSTNAMELEN];
int	big_pipe;
int	odirect;
extern	size_t	splice(int, int);
#define	ENOUGH	(2<<20)			/* file transfer size (1M) */

#ifdef AFS
int (*afs_verify)();
#endif
#ifdef DCE
int dce_verify(char *user, uid_t uid, gid_t gid, char *passwd, char **msg);
#endif /* DCE */

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

void	lostconn(), myoob();
FILE	*getdatasock(), *dataconn(char *name, off_t size, char *mode);
void send_data(FILE *instr, FILE *outstr, off_t blksize);
void reply(int n, char *fmt, ...);

#ifdef SETPROCTITLE
char	**Argv = NULL;		/* pointer to argument vector */
char	*LastArgv = NULL;	/* end of argv */
char	proctitle[BUFSIZ];	/* initial part of title */
#endif /* SETPROCTITLE */

#define MAXLINE         256

/*
 * Restricted accounts require a password but file access is restricted to
 * the account's home directory tree.  
 *
 * The account name must be listed in the ftpusers file with the keyword
 * "restrict" after the name. If the name isn't in the file then it has
 * normal access to the full directory structure.
 */

int restricted;				/* Special login that needs a chroot */
#define RESTRICT_KEY	"restrict"
int safer_guest;			/* hide symbolic links */


#define LOGCMD(cmd,file) \
	if (logging > 1) \
	    syslog(LOG_INFO,"%s %s%s", cmd, \
		*(file) == '/' ? "" : curdir(), file);
#define LOGCMD2(cmd,file1,file2) \
	 if (logging > 1) \
	    syslog(LOG_INFO,"%s %s%s %s%s", cmd, \
		*(file1) == '/' ? "" : curdir(), file1, \
		*(file2) == '/' ? "" : curdir(), file2);
#ifdef sgi
#define LOGBYTES(cmd, file, cnt) \
	if (logging > 1) { \
		if (logging == 2 || cnt == (off_t)-1) \
		    syslog(LOG_INFO,"%s %s%s", cmd, \
			*(file) == '/' ? "" : curdir(), file); \
		else \
		    syslog(LOG_INFO, "%s %s%s = %lld bytes", \
			cmd, (*(file) == '/') ? "" : curdir(), file, cnt); \
	}
#else
	if (logging > 1) { \
		if (logging == 2 || cnt == (off_t)-1) \
		    syslog(LOG_INFO,"%s %s%s", cmd, \
			*(file) == '/' ? "" : curdir(), file); \
		else \
		    syslog(LOG_INFO, "%s %s%s = %ld bytes", \
			cmd, (*(file) == '/') ? "" : curdir(), file, cnt); \
	}
#endif

static int
cap_seteuid(uid_t uid)
{
	cap_t ocap;
	const cap_value_t cv = CAP_SETUID;
	int r;

	ocap = cap_acquire(1, &cv);
	r = seteuid(uid);
	cap_surrender(ocap);
	return (r);
}

static int
cap_setegid(gid_t gid)
{
	cap_t ocap;
	const cap_value_t cv = CAP_SETGID;
	int r;

	ocap = cap_acquire(1, &cv);
	r = setegid(gid);
	cap_surrender(ocap);
	return (r);
}

static int
cap_initgroups(char *name, gid_t gid)
{
	cap_t ocap;
	const cap_value_t cv = CAP_SETGID;
	int r;

	ocap = cap_acquire(1, &cv);
	r = initgroups(name, gid);
	cap_surrender(ocap);
	return (r);
}

static char *
curdir(void)
{
        static char path[MAXPATHLEN + 2];

        if (getwd(path) == NULL)
                return("");
	if (path[1] != '\0')		/* special case for root dir. */
		strcat(path, "/");
	if (guest || restricted)
		return(path+1);		/* skip / since it's chrooted */
	else
		return(path);
}

main(argc, argv, envp)
	int argc;
	char *argv[];
	char **envp;
{
	int addrlen, on = 1, tos;
	char *cp, line[MAXLINE];
	FILE *fd;
	int len;
	int i;

	int s;

	len = sizeof(ibufsize);
	(void)getsockopt(0, SOL_SOCKET, SO_RCVBUF, &ibufsize, &len);
	len = sizeof(obufsize);
	(void)getsockopt(0, SOL_SOCKET, SO_SNDBUF, &obufsize, &len);

	if (ibufsize < FTPBUFSIZ) 
		ibufsize = FTPBUFSIZ;

	if (obufsize < FTPBUFSIZ) 
		obufsize = FTPBUFSIZ;

	if (ibufsize > 2*1024*1024)
		ibufsize = 2*1024*1024;
	if (obufsize > 2*1024*1024)
		obufsize = 2*1024*1024;

#ifdef sgi
	/*
	 * use memalign to set up page-aligned buffers
	 */
	for (i = 0; i < NBUFS; i++) {
		if ((ftp_inbuf[i] = memalign(FTPBUFALIGN, ibufsize)) == NULL) {
			syslog(LOG_ERR,"ftpd:  couldn't allocate I/O buffer\n");
			exit(1);
		}
		if ((ftp_outbuf[i] = memalign(FTPBUFALIGN, obufsize)) == NULL){
			syslog(LOG_ERR,"ftpd:  couldn't allocate I/O buffer\n");
			exit(1);
		}
	}
#endif /* sgi */

	/*
	 * LOG_NDELAY sets up the logging connection immediately,
	 * necessary for anonymous ftp's that chroot and can't do it later.
	 */
	openlog("ftpd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
	addrlen = sizeof (his_addr);
	if (getpeername(0, (struct sockaddr *)&his_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getpeername (%s): %m",argv[0]);
		exit(1);
	}
	addrlen = sizeof (ctrl_addr);
	if (getsockname(0, (struct sockaddr *)&ctrl_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getsockname (%s): %m",argv[0]);
		exit(1);
	}
#ifdef IP_TOS
	tos = IPTOS_LOWDELAY;
	if (setsockopt(0, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
	data_source.sin_port = htons(ntohs(ctrl_addr.sin_port) - 1);
	debug = 0;
#ifdef SETPROCTITLE
	/*
	 *  Save start and extent of argv for setproctitle.
	 */
	Argv = argv;
	while (*envp)
		envp++;
	LastArgv = envp[-1] + strlen(envp[-1]);
#endif /* SETPROCTITLE */

	argc--, argv++;
	while (argc > 0 && *argv[0] == '-') {
		for (cp = &argv[0][1]; *cp; cp++) switch (*cp) {

		case 'v':
			debug = 1;
			break;

		case 'd':
			debug = 1;
			break;

		case 'l':
			logging++;	/* > 1 == extra logging */
			break;

		case 'p':
			portcheck = 1;	/* Prevent PORT command redirection */
			break;

		case 't':
			timeout = atoi(++cp);
			if (maxtimeout < timeout)
				maxtimeout = timeout;
			goto nextopt;

		case 'T':
			maxtimeout = atoi(++cp);
			if (timeout > maxtimeout)
				timeout = maxtimeout;
			goto nextopt;

		case 'u':
		    {
			int val = 0;

			while (*++cp && *cp >= '0' && *cp <= '9')
				val = val*8 + *cp - '0';
			if (*cp)
				fprintf(stderr, "ftpd: Bad value for -u\n");
			else
				defumask = val;
			goto nextopt;
		    }

#ifdef sgi
	    case 'S':
		    safer_guest = 1;
		    goto nextopt;
		    
#endif
		default:
			fprintf(stderr, "ftpd: Unknown flag -%c ignored.\n",
			     *cp);
			break;
		}
nextopt:
		argc--, argv++;
	}
	(void) freopen(_PATH_DEVNULL, "w", stderr);
	(void) signal(SIGPIPE, (SIG_PF)lostconn);
	(void) signal(SIGCHLD, SIG_IGN);
	if ((int)signal(SIGURG, (SIG_PF)myoob) < 0)
		syslog(LOG_ERR, "signal: %m");

	/* Try to handle urgent data inline */
#ifdef SO_OOBINLINE
	if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0)
		syslog(LOG_ERR, "setsockopt: %m");
#endif

#ifdef	F_SETOWN
	if (fcntl(fileno(stdin), F_SETOWN, getpid()) == -1)
		syslog(LOG_ERR, "fcntl F_SETOWN: %m");
#endif
	dolog(&his_addr);
	/*
	 * Set up default state
	 */
	data = -1;
	type = TYPE_A;
	form = FORM_N;
	stru = STRU_F;
	mode = MODE_S;
	tmpline[0] = '\0';

	/* If logins are disabled, print out the message. */
	if ((fd = fopen(_PATH_NOLOGIN,"r")) != NULL) {
		while (fgets(line, sizeof (line), fd) != NULL) {
			if ((cp = index(line, '\n')) != NULL)
			    *cp = '\0';
			lreply(530, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fd);
		reply(530, "System not available.");
		exit(0);
	}
#ifdef AFS
	afs_verify = (int (*)())__afs_getsym("afs_verify");
#endif
	if ((fd = fopen(_PATH_WELCOME, "r")) != NULL) {
		while (fgets(line, sizeof (line), fd) != NULL) {
			if ((cp = index(line, '\n')) != NULL)
			    *cp = '\0';
			lreply(220, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fd);
		/* reply(220,) must follow */
	}
	(void) gethostname(hostname, sizeof (hostname));
#ifdef sgi
	{  /* Canonicalize hostname. */
	    struct hostent *hp;
	    if ((hp = gethostbyname(hostname)) != NULL) {
		    strncpy(hostname, hp->h_name, sizeof(hostname)-1);
		    hostname[sizeof(hostname)-1] = '\0';
	    }
	}
	reply(220, "%s FTP server ready.", hostname);
#else
	reply(220, "%s FTP server (%s) ready.", hostname, version);
#endif
	(void) setjmp(errcatch);
	for (;;)
		(void) yyparse();
	/* NOTREACHED */
}

void
lostconn()
{
	if (debug)
		syslog(LOG_DEBUG, "lost connection");
	dologout(-1);
}

#ifdef sgi
#define ttyline ""
#else
static char ttyline[20];
#endif

/*
 * Helper function for sgetpwnam().
 */
char *
sgetsave(s)
	char *s;
{
	char *new = malloc((unsigned) strlen(s) + 1);

	if (new == NULL) {
		perror_reply(421, "Local resource failure: malloc");
		dologout(1);
		/* NOTREACHED */
	}
	(void) strcpy(new, s);
	return (new);
}

/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
struct passwd *
sgetpwnam(name)
	char *name;
{
	static struct passwd save;
	register struct passwd *p;
	char *sgetsave();

	if ((p = getpwnam(name)) == NULL)
		return (p);
	if (save.pw_name) {
		free(save.pw_name);
		free(save.pw_passwd);
		free(save.pw_gecos);
		free(save.pw_dir);
		free(save.pw_shell);
	}
	save = *p;
	save.pw_name = sgetsave(p->pw_name);
	save.pw_passwd = sgetsave(p->pw_passwd);
	save.pw_gecos = sgetsave(p->pw_gecos);
	save.pw_dir = sgetsave(p->pw_dir);
	save.pw_shell = sgetsave(p->pw_shell);
	return (&save);
}

int login_attempts;		/* number of failed login attempts */
int askpasswd;			/* had user command, ask for passwd */
char curname[10];		/* current USER name */

/*
 * USER command.
 * Sets global passwd pointer pw if named account exists and is acceptable;
 * sets askpasswd if a PASS command is expected.  If logged in previously,
 * need to reset state.  If name is "ftp" or "anonymous", the name is not in
 * _PATH_FTPUSERS, and ftp account exists, set guest and pw, then just return.
 * If account doesn't exist, ask for passwd anyway.  Otherwise, check user
 * requesting login privileges.  Disallow anyone who does not have a standard
 * shell as returned by getusershell().  Disallow anyone mentioned in the file
 * _PATH_FTPUSERS to allow people such as root and uucp to be avoided.
 */
user(name)
	char *name;
{
	register char *cp;
#ifndef sgi
	char *shell;
	char *getusershell();
#endif

	if (logged_in) {
		if (guest || restricted) {
			reply(530, "Can't change user from guest login.");
			return;
		}
		end_login();
	}

	guest = restricted = 0;
	if (strcmp(name, "ftp") == 0 || strcmp(name, "anonymous") == 0) {
		if (checkuser("ftp", NULL) || checkuser("anonymous", NULL))
			reply(530, "User %s access denied.", name);
		else if ((pw = sgetpwnam("ftp")) != NULL) {
			guest = 1;
			askpasswd = 1;
			reply(331,
			    "Guest login ok, type your name as password.");
		} else
			reply(530, "User %s unknown.", name);
		if (!askpasswd && logging)
			syslog(LOG_NOTICE|LOG_AUTH,
			    "ANONYMOUS FTP LOGIN REFUSED FROM %s", remotehost);
		return;
	}
	if (pw = sgetpwnam(name)) {
#ifdef sgi
		cp = "";
#else
		if ((shell = pw->pw_shell) == NULL || *shell == 0)
			shell = _PATH_BSHELL;
		while ((cp = getusershell()) != NULL)
			if (strcmp(cp, shell) == 0)
				break;
		endusershell();
#endif

		if (cp == NULL || checkuser(name, &restricted)) {
			reply(530, "User %s access denied.", name);
			if (logging)
				syslog(LOG_NOTICE|LOG_AUTH,
				    "FTP LOGIN REFUSED FROM %s, %s",
				    remotehost, name);
			pw = (struct passwd *) NULL;
			return;
		}
	}
	if (logging)
		strncpy(curname, name, sizeof(curname)-1);
	reply(331, "Password required for %s.", name);
	askpasswd = 1;
	/*
	 * Delay before reading passwd after first failed
	 * attempt to slow down passwd-guessing programs.
	 */
	if (login_attempts)
		sleep((unsigned) login_attempts);
}

/*
 * Check if a user is in the file _PATH_FTPUSERS
 */
checkuser(name, restrict)
	char *name;
	int *restrict;
{
	register FILE *fd;
	register char *p;
	char line[BUFSIZ];
	int access = 0;

	if ((fd = fopen(_PATH_FTPUSERS, "r")) != NULL) {
		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((p = index(line, '\n')) != NULL) {
				*p = '\0';
				if (line[0] == '#')
					continue;
				/* Strip trailing white space */
				if ((p = strtok(line, " \t")) &&
				    strcmp(p, name) == 0) {
					/*
					 * If the line contains an account name
					 * followed by "restrict", then allow 
					 * restricted access to the account.
					 */
					if ((p = strtok(NULL, " \t")) &&
					    strcasecmp(p, RESTRICT_KEY) == 0) {
						if (restrict)
							*restrict = 1;
					} else 
						access = 1;
					break;
				}
			}
		}
		(void) fclose(fd);
	}
	return (access);
}

/*
 * Terminate login as previous user, if any, resetting state;
 * used when USER command is given or login fails.
 */
end_login()
{
	(void) cap_seteuid((uid_t)0);
	if (logged_in)
		logwtmp(ttyline, "", "");
	pw = NULL;
	logged_in = 0;
	guest = 0;
	restricted = 0;
}

pass(passwd)
	char *passwd;
{
	char *xpasswd, *salt;
	FILE *fd;

	if (logged_in || askpasswd == 0) {
		reply(503, "Login with USER first.");
		return;
	}

	askpasswd = 0;
	if (!guest) {		/* "ftp" is only account allowed no password */
		if (pw == NULL)
			salt = "xx";
		else
			salt = pw->pw_passwd;
#ifdef AFS
		if (pw != NULL && afs_verify)
			if ((*afs_verify)(pw->pw_name, passwd, NULL, 1) == 0)
				goto good;
#endif
#ifdef DCE
		if (pw != NULL && strcmp(pw->pw_passwd,"-DCE-") == 0) {
			void *handle=NULL;
			char *dce_err;
			if (!(handle=sgidladd("libdce.so",RTLD_LAZY))) {
				if (logging) {
					syslog(LOG_NOTICE|LOG_AUTH,
					       "FTP DCE LOGIN FAILED, NO DCE LIBRARY");
				}
				goto login_failed;
			}
			if (!dlsym(handle,"dce_verify")) {
				if (logging) {
					syslog(LOG_NOTICE|LOG_AUTH,
					       "FTP DCE LOGIN FAILED, WRONG DCE LIBRARY");
				}
				goto login_failed;
			}
			if (dce_verify(pw->pw_name, pw->pw_uid, pw->pw_gid,
				       passwd, &dce_err)) {
				memset(passwd, 'x', strlen(passwd));
				if (dce_err) {
					if (logging) {
						syslog(LOG_NOTICE|LOG_AUTH,
						       "FTP DCE LOGIN FAILED, %s",
						       dce_err);
					}
					free(dce_err);
				} else {
					if (logging) {
						syslog(LOG_NOTICE|LOG_AUTH,
						       "FTP DCE LOGIN FAILED");
					}
				}
				goto login_failed;
			}
			memset(passwd, 'x', strlen(passwd));
			goto password_verified;
		}
#endif /* DCE */
		xpasswd = crypt(passwd, salt);
		memset(passwd, 'x', strlen(passwd));
		/* The strcmp does not catch null passwords! */
		if (pw == NULL || *pw->pw_passwd == '\0' ||
		    strcmp(xpasswd, pw->pw_passwd)) {
login_failed:
			reply(530, "Login incorrect.");
			if (logging)
				syslog(LOG_NOTICE|LOG_AUTH,
				    "FTP LOGIN FAILED FROM %s, %s",
				    remotehost, curname);
			pw = NULL;
			if (login_attempts++ >= 5) {
				syslog(LOG_NOTICE|LOG_AUTH,
				    "repeated login failures from %s",
				    remotehost);
				exit(0);
			}
			return;
		}
	}
	
password_verified:
#ifdef _SHAREII
        /*
	 *	Perform Share II resource limit checks.
	 */
	if (sgidladd(SH_LIMITS_LIB, RTLD_LAZY))
	{
		static const char *Myname = "ftpd";

		SH_HOOK_SETMYNAME(Myname);

		if (SH_HOOK_FTP(pw->pw_uid))
		{
			
			syslog(LOG_NOTICE|LOG_AUTH,
			       "Share II resource limit checks for user '%s' failed",
			       pw->pw_name);
			return;
		}
	}
#endif /* _SHAREII */
	
good:
	/* patch 1211 - add in ia_userinfo use to check for expired
	 * passwords.  This should be better integrated.
	 */
	if ( ia_openinfo(pw->pw_name, &uinfo) || (uinfo == NULL) ) {
	    reply(530, "Unexpected login failure - contact system administrator");
	    syslog(LOG_ALERT|LOG_AUTH, "ia_openinfo failed for valid user!");
	    exit(1);
	}
	/* check for passwd expiration */
	ia_get_logexpire(uinfo, &ia_expire);
	ia_get_logchg(uinfo, &ia_lstchg);
	now = DAY_NOW;
	if ( ((ia_expire > 0)  && (ia_expire < now)) 
	   ||((ia_lstchg == 0) || (ia_lstchg > now)) ){
		reply(530, 
		   "Your password has expired, please login to change it.");
		syslog(LOG_NOTICE|LOG_AUTH, 
		   "%s tried to login with an expired password", pw->pw_name);
		exit(0);
	}
/* end of 1211 changes */

	login_attempts = 0;		/* this time successful */
	if (cap_setegid((gid_t)pw->pw_gid) < 0) {
		reply(550, "Can't set gid.");
		return;
	}
	(void) cap_initgroups(pw->pw_name, pw->pw_gid);

#ifdef sgi
	/* update utmp,wtmp before chroot */
	dologin(pw, remotehost, guest || restricted);
#else
	/* open wtmp before chroot */
	(void)sprintf(ttyline, "ftp%d", getpid());
	logwtmp(ttyline, pw->pw_name, remotehost);
#endif
	logged_in = 1;

	if (guest || restricted) {
		const cap_value_t cap_chroot = CAP_CHROOT;
		cap_t ocap = cap_acquire(1, &cap_chroot);
		if (chroot(pw->pw_dir) < 0) {
			cap_surrender(ocap);
			reply(550, "Can't set guest privileges.");
			goto bad;
		}
		cap_surrender(ocap);
	}

	if (cap_seteuid((uid_t)pw->pw_uid) < 0) {
		reply(550, "Can't set uid.");
		goto bad;
	}

	if (guest || restricted) {
		/*
		 * We MUST do a chdir() after the chroot. Otherwise
		 * the old current directory will be accessible as "."
		 * outside the new root!
		 */
		if (chdir("/") < 0) {
			reply(550, "Can't set guest privileges.");
			goto bad;
		}

		/*
		 * Display a login message, if it exists.
		 * N.B. reply(230,) must follow the message.
		 */
		if ((fd = fopen(_PATH_FTPLOGINMESG, "r")) != NULL) {
			char *cp, line[MAXLINE];

			while (fgets(line, sizeof (line), fd) != NULL) {
				if ((cp = index(line, '\n')) != NULL)
				    *cp = '\0';
				lreply(230, "%s", line);
			}
			(void) fflush(stdout);
			(void) fclose(fd);
		}
	} else {
		if (chdir(pw->pw_dir) < 0) {
			if (chdir("/") < 0) {
				reply(530, "User %s: can't change directory to %s.", pw->pw_name, pw->pw_dir);
				goto bad;
			} else {
				lreply(230, "No directory! Logging in with home=/");
			}
		}
	}

	if (guest) {
		reply(230, "Guest login ok, access restrictions apply.");
#ifdef SETPROCTITLE
		sprintf(proctitle, "%s: anonymous/%.*s", remotehost,
		    sizeof(proctitle) - sizeof(remotehost) -
		    sizeof(": anonymous/"), passwd);
		setproctitle(proctitle);
#endif /* SETPROCTITLE */
		if (logging)
			syslog(LOG_INFO|LOG_AUTH, "ANONYMOUS FTP LOGIN FROM %s, %s",
			    remotehost, passwd);
	} else if (restricted) {
		reply(230, "User %s logged in, access restrictions apply.", 
				pw->pw_name);
		if (logging)
			syslog(LOG_INFO|LOG_AUTH,
			    "RESTRICTED FTP LOGIN FROM %s as %s",
			    remotehost, pw->pw_name);
#ifdef SETPROCTITLE
		sprintf(proctitle, "%s: restricted/%s", remotehost,pw->pw_name);
		setproctitle(proctitle);
#endif /* SETPROCTITLE */
	} else {
		reply(230, "User %s logged in.", pw->pw_name);
#ifdef SETPROCTITLE
		sprintf(proctitle, "%s: %s", remotehost, pw->pw_name);
		setproctitle(proctitle);
#endif /* SETPROCTITLE */
		if (logging)
			syslog(LOG_INFO|LOG_AUTH, "FTP LOGIN FROM %s as %s",
			    remotehost, pw->pw_name);
	}

	(void) umask(defumask);
	return;
bad:
	/* Forget all about it... */
	end_login();
}

retrieve(cmd, name)
	char *cmd, *name;
{
	FILE *fin, *dout;
	struct stat st;
	int (*closefunc)();

	if (cmd == 0) {
		fin = fopen(name, "r"), closefunc = fclose;
		st.st_size = 0;
	} else {
		char line[BUFSIZ];

		(void) sprintf(line, cmd, name), name = line;
		fin = ftpd_popen(line, "r"), closefunc = ftpd_pclose;
		st.st_size = -1;
#ifndef sgi	/* Not defined in SYSV. */
		st.st_blksize = BUFSIZ;
#endif
	}
	if (fin == NULL) {
		if (errno != 0) {
			perror_reply(550, name);
			if (cmd == 0) {
				LOGCMD("get", name);
			}
		}
		return;
	}
	byte_count = -1;
	if (cmd == 0 &&
	    (fstat(fileno(fin), &st) < 0 || (st.st_mode&S_IFMT) != S_IFREG)) {
		reply(550, "%s: not a plain file.", name);
		goto done;
	}
	if (restart_point) {
		if (type == TYPE_A) {
			register int i, n, c;

			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fin)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}	
		} else if (lseek(fileno(fin), restart_point, L_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	dout = dataconn(name, st.st_size, "w");
	if (dout == NULL)
		goto done;
#ifdef sgi
	send_data(fin, dout, (off_t) obufsize);
#else
	send_data(fin, dout, st.st_blksize);
#endif
	(void) fclose(dout);
	data = -1;
	pdata = -1;
done:
	if (cmd == 0)
		LOGBYTES("get", name, byte_count);
	(*closefunc)(fin);
}

store(name, mode, unique)
	char *name, *mode;
	int unique;
{
	FILE *fout, *din;
	struct stat st;
	int (*closefunc)();
	char *gunique();

	if (unique && stat(name, &st) == 0 &&
	    (name = gunique(name)) == NULL) {
		LOGCMD(*mode == 'w' ? "put" : "append", name);
		return;
	}

	if (restart_point)
		mode = "r+w";
	fout = fopen(name, mode);
	closefunc = fclose;
	if (fout == NULL) {
		perror_reply(553, name);
		LOGCMD(*mode == 'w' ? "put" : "append", name);
		return;
	}
	byte_count = -1;
	if (restart_point) {
		if (type == TYPE_A) {
			register int i, n, c;

			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fout)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}	
			/*
			 * We must do this seek to "current" position
			 * because we are changing from reading to
			 * writing.
			 */
			if (fseek(fout, 0L, L_INCR) < 0) {
				perror_reply(550, name);
				goto done;
			}
		} else if (lseek(fileno(fout), restart_point, L_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	din = dataconn(name, (off_t)-1, "r");
	if (din == NULL)
		goto done;
	if (receive_data(din, fout) == 0) {
		if (unique)
			reply(226, "Transfer complete (unique file name:%s).",
			    name);
		else
			reply(226, "Transfer complete.");
	}
	(void) fclose(din);
	data = -1;
	pdata = -1;
done:
	LOGBYTES(*mode == 'w' ? "put" : "append", name, byte_count);
	(*closefunc)(fout);
}

FILE *
getdatasock(mode)
	char *mode;
{
	int s, on = 1, tries;
	int t;

	if (data >= 0)
		return (fdopen(data, mode));
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return (NULL);
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    (char *) &on, sizeof (on)) < 0)
		goto bad;
	/* anchor socket to avoid multi-homing problems */
	data_source.sin_family = AF_INET;
	data_source.sin_addr = ctrl_addr.sin_addr;
	for (tries = 1; ; tries++) {
		if (cap_bind(s, (struct sockaddr *)&data_source,
			     sizeof (data_source)) >= 0)
			break;
		if (errno != EADDRINUSE || tries > 10)
			goto bad;
		sleep(tries);
	}
#ifdef IP_TOS
	on = IPTOS_THROUGHPUT;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
#endif
	if (big_pipe) {
		int sz = big_pipe;

		(void)setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
		(void)setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
	}

	return (fdopen(s, mode));
bad:
	/* Return the real value of errno (close may change it) */
	t = errno;
	(void) close(s);
	errno = t;
	return (NULL);
}

FILE *
dataconn(char *name, off_t size, char *mode)
{
	char sizebuf[32];
	FILE *file;
	int retry = 0, tos;
	extern void toolong(int);

	file_size = size;
	byte_count = 0;
	if (size != (off_t) -1)
#ifdef sgi
		(void) sprintf (sizebuf, " (%lld bytes)", size);
#else
		(void) sprintf (sizebuf, " (%ld bytes)", size);
#endif
	else
		(void) strcpy(sizebuf, "");
	if (pdata >= 0) {
		struct sockaddr_in from;
		int s, fromlen = sizeof(from);

		signal(SIGALRM, toolong);
		(void) alarm((unsigned)timeout);
		s = accept(pdata, (struct sockaddr *)&from, &fromlen);
		if (s < 0) {
			(void) alarm(0);
			reply(425, "Can't open data connection.");
			(void) close(pdata);
			pdata = -1;
			return(NULL);
		}
		(void) alarm(0);
		(void) close(pdata);
		pdata = s;
#ifdef IP_TOS
		tos = IPTOS_LOWDELAY;
		(void) setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos,
		    sizeof(int));
#endif

		if (big_pipe) {
			int sz = big_pipe;

			(void)setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sz, 
				sizeof(sz));
			(void)setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sz,
				sizeof(sz));
		}
		reply(150, "Opening %s mode data connection for '%s'%s.",
		     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
		return(fdopen(pdata, mode));
	}
	if (data >= 0) {
		reply(125, "Using existing data connection for '%s'%s.",
		    name, sizebuf);
		usedefault = 1;
		return (fdopen(data, mode));
	}
	if (usedefault)
		data_dest = his_addr;
	if (portcheck && data_dest.sin_addr.s_addr != his_addr.sin_addr.s_addr) {
		reply(425, "Can't redirect to third party");
		return (NULL);
	}
	usedefault = 1;
	file = getdatasock(mode);
	if (file == NULL) {
		reply(425, "Can't create data socket (%s,%d): %s.",
		    inet_ntoa(data_source.sin_addr),
		    ntohs(data_source.sin_port), strerror(errno));
		return (NULL);
	}
	data = fileno(file);
	while (connect(data, (struct sockaddr *)&data_dest,
	    sizeof (data_dest)) < 0) {
		if (errno == EADDRINUSE && retry < swaitmax) {
			sleep((unsigned) swaitint);
			retry += swaitint;
			continue;
		}
		perror_reply(425, "Can't build data connection");
		(void) fclose(file);
		data = -1;
		return (NULL);
	}
	if (big_pipe) {
		int sz = big_pipe;

		(void)setsockopt(data, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
		(void)setsockopt(data, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
	}
	reply(150, "Opening %s mode data connection for '%s'%s.",
	     type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
	return (file);
}

/*
 * Tranfer the contents of "instr" to
 * "outstr" peer using the appropriate
 * encapsulation of the data subject
 * to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
void
send_data(FILE *instr, FILE *outstr, off_t blksize)
{
	register int c, cnt;
	int b = 0;
#ifndef sgi
	register char *buf;
#endif
	int netfd, filefd;

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return;
	}
	switch (type) {

	case TYPE_A:
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			if (c == '\n') {
				if (ferror(outstr))
					goto data_err;
				(void) putc('\r', outstr);
			}
			(void) putc(c, outstr);
		}
		fflush(outstr);
		transflag = 0;
		if (ferror(instr))
			goto file_err;
		if (ferror(outstr))
			goto data_err;
		reply(226, "Transfer complete.");
		return;

	case TYPE_I:
	case TYPE_L:
#ifdef sgi
		netfd = fileno(outstr);
		filefd = fileno(instr);

		if (odirect || big_pipe) {
			cnt = splice(filefd, netfd);
			transflag = 0;
			if (cnt == -2) {
				goto data_err;
			} else if (cnt == -1) {
				goto file_err;
			}
			byte_count += cnt;
			goto out;
		}
		while ((cnt = read(filefd, ftp_outbuf[b], obufsize)) > 0 &&
		    write(netfd, ftp_outbuf[b], cnt) == cnt) {
			byte_count += cnt;
			b++;
			if (b == NBUFS) {
				b = 0;
			}
		}
		transflag = 0;
#else
		if ((buf = malloc((u_int)blksize)) == NULL) {
			transflag = 0;
			perror_reply(451, "Local resource failure: malloc");
			return;
		}
		netfd = fileno(outstr);
		filefd = fileno(instr);
		while ((cnt = read(filefd, buf, (u_int)blksize)) > 0 &&
		    write(netfd, buf, cnt) == cnt)
			byte_count += cnt;
		transflag = 0;
		(void)free(buf);
#endif
		if (cnt != 0) {
			if (cnt < 0)
				goto file_err;
			goto data_err;
		}
out:
		reply(226, "Transfer complete.");
		return;
	default:
		transflag = 0;
		reply(550, "Unimplemented TYPE %d in send_data", type);
		return;
	}

data_err:
	transflag = 0;
	perror_reply(426, "Data connection");
	return;

file_err:
	transflag = 0;
	perror_reply(551, "Error on input file");
}

/*
 * Transfer data from peer to
 * "outstr" using the appropriate
 * encapulation of the data subject
 * to Mode, Structure, and Type.
 *
 * N.B.: Form isn't handled.
 */
receive_data(instr, outstr)
	FILE *instr, *outstr;
{
	register int c;
	int cnt, bare_lfs = 0;
#ifndef sgi
	char buf[BUFSIZ];
#endif
	int b = 0;

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return (-1);
	}
	switch (type) {

	case TYPE_I:
	case TYPE_L:
#ifdef sgi
		if (odirect || big_pipe) {
			cnt = splice(fileno(instr), fileno(outstr));
			if (cnt == -1) {
				goto data_err;
			} else if (cnt == -2) {
				goto file_err;
			}
			byte_count += cnt;
			return 0;
		}
		while ((cnt = read(fileno(instr), ftp_inbuf[b], ibufsize))
		      > 0) {
			if (write(fileno(outstr), ftp_inbuf[b], cnt) != cnt)
				goto file_err;
			byte_count += cnt;
			b++;
			if (b == NBUFS) {
				b = 0;
			}
		}
#else
		while ((cnt = read(fileno(instr), buf, sizeof buf)) > 0) {
			if (write(fileno(outstr), buf, cnt) != cnt)
				goto file_err;
			byte_count += cnt;
		}
#endif
		if (cnt < 0)
			goto data_err;
		transflag = 0;
		return (0);

	case TYPE_E:
		reply(553, "TYPE E not implemented.");
		transflag = 0;
		return (-1);

	case TYPE_A:
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				if (ferror(outstr))
					goto data_err;
				if ((c = getc(instr)) != '\n') {
					(void) putc ('\r', outstr);
					if (c == '\0' || c == EOF)
						goto contin2;
				}
			}
			(void) putc(c, outstr);
	contin2:	;
		}
		fflush(outstr);
		if (ferror(instr))
			goto data_err;
		if (ferror(outstr))
			goto file_err;
		transflag = 0;
		if (bare_lfs) {
			lreply(230, "WARNING! %d bare linefeeds received in ASCII mode", bare_lfs);
			printf("   File may not have transferred correctly.\r\n");
		}
		return (0);
	default:
		reply(550, "Unimplemented TYPE %d in receive_data", type);
		transflag = 0;
		return (-1);
	}

data_err:
	transflag = 0;
	perror_reply(426, "Data Connection");
	return (-1);

file_err:
	transflag = 0;
	perror_reply(452, "Error writing file");
	return (-1);
}

statfilecmd(filename)
	char *filename;
{
	char line[BUFSIZ];
	FILE *fin;
	int c;

#ifdef sgi
	(void) sprintf(line, "/bin/ls -lA %s", filename);
#else
	(void) sprintf(line, "/bin/ls -lgA %s", filename);
#endif
	fin = ftpd_popen(line, "r");
	lreply(211, "status of %s:", filename);
	while ((c = getc(fin)) != EOF) {
		if (c == '\n') {
			if (ferror(stdout)){
				perror_reply(421, "control connection");
				(void) ftpd_pclose(fin);
				dologout(1);
				/* NOTREACHED */
			}
			if (ferror(fin)) {
				perror_reply(551, filename);
				(void) ftpd_pclose(fin);
				return;
			}
			(void) putc('\r', stdout);
		}
		(void) putc(c, stdout);
	}
	(void) ftpd_pclose(fin);
	reply(211, "End of Status");
}

statcmd()
{
	struct sockaddr_in *sin;
	u_char *a, *p;

	lreply(211, "%s FTP server status:", hostname, version);
	printf("     %s\r\n", version);
	printf("     Connected to %s", remotehost);
	if (!isdigit(remotehost[0]))
		printf(" (%s)", inet_ntoa(his_addr.sin_addr));
	printf("\r\n");
	if (logged_in) {
		if (guest)
			printf("     Logged in anonymously\r\n");
		else
			printf("     Logged in as %s\r\n", pw->pw_name);
	} else if (askpasswd)
		printf("     Waiting for password\r\n");
	else
		printf("     Waiting for user name\r\n");
	printf("     TYPE: %s", typenames[type]);
	if (type == TYPE_A || type == TYPE_E)
		printf(", FORM: %s", formnames[form]);
	if (type == TYPE_L)
#if NBBY == 8
		printf(" %d", NBBY);
#else
		printf(" %d", bytesize);	/* need definition! */
#endif
	printf("; STRUcture: %s; transfer MODE: %s\r\n",
	    strunames[stru], modenames[mode]);
	if (data != -1)
		printf("     Data connection open\r\n");
	else if (pdata != -1) {
		printf("     in Passive mode");
		sin = &pasv_addr;
		goto printaddr;
	} else if (usedefault == 0) {
		printf("     PORT");
		sin = &data_dest;
printaddr:
		a = (u_char *) &sin->sin_addr;
		p = (u_char *) &sin->sin_port;
#define UC(b) (((int) b) & 0xff)
		printf(" (%d,%d,%d,%d,%d,%d)\r\n", UC(a[0]),
			UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
#undef UC
	} else
		printf("     No data connection\r\n");
	reply(211, "End of status");
}

fatal(s)
	char *s;
{
	reply(451, "Error in server: %s\n", s);
	reply(221, "Closing connection due to server error.");
	dologout(0);
	/* NOTREACHED */
}

void
reply(int n, char *fmt, ...)
{
	va_list args;
	printf("%d ", n);
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\r\n");
	(void)fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d ", n);
		va_start(args, fmt);
		vsyslog(LOG_DEBUG, fmt, args);
		va_end(args);
	}
}

/* VARARGS2 */
lreply(n, fmt, p0, p1, p2, p3, p4, p5)
	int n;
	char *fmt;
{
	printf("%d- ", n);
	printf(fmt, p0, p1, p2, p3, p4, p5);
	printf("\r\n");
	(void)fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d- ", n);
		syslog(LOG_DEBUG, fmt, p0, p1, p2, p3, p4, p5);
	}
}

ack(s)
	char *s;
{
	reply(250, "%s command successful.", s);
}

nack(s)
	char *s;
{
	reply(502, "%s command not implemented.", s);
}

/* ARGSUSED */
yyerror(s)
	char *s;
{
	char *cp;

	if (cp = index(cbuf,'\n'))
		*cp = '\0';
	reply(500, "'%s': command not understood.", cbuf);
}

delete(name)
	char *name;
{
	struct stat st;

	LOGCMD("delete", name);
	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return;
	}
	if ((st.st_mode&S_IFMT) == S_IFDIR) {
		if (rmdir(name) < 0) {
			perror_reply(550, name);
			return;
		}
		goto done;
	}
	if (unlink(name) < 0) {
		perror_reply(550, name);
		return;
	}
done:
	ack("DELE");
}

cwd(path)
	char *path;
{
	if (chdir(path) < 0)
		perror_reply(550, path);
	else
		ack("CWD");
}

makedir(name)
	char *name;
{
	LOGCMD("mkdir", name);
	if (mkdir(name, 0777) < 0)
		perror_reply(550, name);
	else
		reply(257, "MKD command successful.");
}

removedir(name)
	char *name;
{
	LOGCMD("rmdir", name);
	if (rmdir(name) < 0)
		perror_reply(550, name);
	else
		ack("RMD");
}

pwd()
{
	char path[MAXPATHLEN + 1];

	if (getwd(path) == (char *)NULL)
		reply(550, "%s.", path);
	else
		reply(257, "\"%s\" is current directory.", path);
}

char *
renamefrom(name)
	char *name;
{
	struct stat st;

	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return ((char *)0);
	}
	reply(350, "File exists, ready for destination name");
	return (name);
}

renamecmd(from, to)
	char *from, *to;
{
	LOGCMD2("rename", from, to);
	if (rename(from, to) < 0)
		perror_reply(550, "rename");
	else
		ack("RNTO");
}

dolog(sin)
	struct sockaddr_in *sin;
{
	struct hostent *hp = gethostbyaddr((char *)&sin->sin_addr,
		sizeof (struct in_addr), AF_INET);

	if (hp)
		(void) strncpy(remotehost, hp->h_name, sizeof (remotehost));
	else
		(void) strncpy(remotehost, inet_ntoa(sin->sin_addr),
		    sizeof (remotehost));
#ifdef SETPROCTITLE
	sprintf(proctitle, "%s: connected", remotehost);
	setproctitle(proctitle);
#endif /* SETPROCTITLE */

	if (logging) {
		syslog(LOG_INFO, "connection from %s", remotehost);
	}
}

#ifdef sgi
/*
 * Record login in wtmp and wtmpx files.
 */
#include <utmpx.h>
extern int __keepwtmps_open;
extern int __keeputmp_open;

#define LOCK_TRIES 2

/* lock_utmpx
 *
 * file lock the entire utmpx file.
 *
 * This lock could try and sleep for a finite number of times,
 * but this indefinite sleep locking provides greater workload
 * throughput when the workload is dominated by ftp sessions.
 */
static int
lock_utmpx(void)
{
  int fd;

  if ((fd = open(UTMPX_FILE, O_RDWR, 0644)) != -1 &&
      lockf(fd, F_LOCK, 0) != -1)
    return fd;
  else
    return -1;
}

static void
cap_pututxline(struct utmpx *utmp)
{
	cap_t ocap;
	const cap_value_t cv = CAP_DAC_WRITE;

	ocap = cap_acquire(1, &cv);
	(void) pututxline(utmp);
	cap_surrender(ocap);
}

static void
cap_updwtmpx(const char *file, struct utmpx *utmp)
{
	cap_t ocap;
	const cap_value_t cv = CAP_DAC_WRITE;

	ocap = cap_acquire(1, &cv);
	updwtmpx(file, utmp);
	cap_surrender(ocap);
}

#define unlock_utmpx(fd) \
    lockf(fd, F_ULOCK, 0); \
    close(fd)

#define	SCPYN(a, b)	(void) strncpy(a, b, sizeof (a))

static struct utmpx	utmp;
static int line_num = 0;
static int didutmp;
#define MAX_LINE_NUM (sizeof(line_nc)-1)
#define LINE_ID id[3]
#define UTHLEN (sizeof(utmp.ut_host)-1)
#define NUM_SLOTS 254
#define FIRST_ASCII_PRINTABLE 33

typedef struct table {
  unsigned char character;
  char used;
} table_t;

/* if remotehost is short enough, store something in ut_host field to 
 * prevent interpretting the missing remote user info to mean that the
 * local and remote usernames are the same, which IS what this means for
 * rlogin utmpx entries.
 */
static char *noknowstr = "UNKNOWN";

dologin(pw, remotehost, chr)
	struct passwd *pw;
	char *remotehost;
	int chr;
{
	int rh_len, ru_len;
	table_t slots_char_table[NUM_SLOTS];
	char *char_p;
	int found_ftp_id;
	int wtmpx_lock_fd;
	int i, j;
	struct utmpx *utmpx_ptr;

        /* initialize the slots table starting with the first ascii
           printable character. */
	char_p = (char *)&slots_char_table;
	for (i = 0, j = FIRST_ASCII_PRINTABLE; i < NUM_SLOTS; i++) {
	  *char_p++ = (char)j;
	  *char_p++ = (char)0;
	  j = ++j % 256;
	  if (j == 0) j++;
	}
	rh_len = strlen(remotehost);
	if (rh_len > UTHLEN)
		rh_len = UTHLEN;
	/* 
	 * compute max # of chars we can grab from noknowstr array
	 * without truncating a remote host string of length 'rhlen'.
	 * -1 for the '@' separator.
	 */
	ru_len = (UTHLEN - rh_len - 1);
	sprintf(&(utmp.ut_host[0]), "%.*s%s%.*s", ru_len,
		noknowstr, (ru_len > 0 ? "@" : ""), rh_len, remotehost);
	SCPYN(utmp.ut_name, pw->pw_name);
	utmp.ut_pid = getpid();
	utmp.ut_type = USER_PROCESS;
	utmp.ut_xtime = time(0);
	sprintf(utmp.ut_line, "ftp%d", utmp.ut_pid);

	/*
	 * After the slot is found, don't call "endutent" to close the utmp
	 * file. It must kept open so dologout will work after chroot.
	 * Ditto for wtmp.
	 * In order to provide additional info, we attempt to add a line
	 * to utmp - note that BSD doesn't do this. Since the 'id' field
	 * is only 4 chars long and must be unique - we can only do this
	 * for MAX_LINE_NUM ftp connections. Regardless of whether we
	 * find a utmp entry, we need to log this for wtmp.
	 */
	if (chr)
		__keeputmp_open = 1;
	/* Try to lock the utmpx file. */
	if ((wtmpx_lock_fd = lock_utmpx()) != -1) {
	  /* Locked the utmpx file so look for an available slot. */
	  i = 0;
	  while ((utmpx_ptr = getutxent()) != NULL) {
	    char_p = utmpx_ptr->ut_id;
	    /* Does the entry's ID start with "ftp?" */
	    if ((*char_p++ == 'f') && (*char_p++ == 't') &&
		(*char_p++ == 'p'))
	      /* If the entry is for a dead process, grab it and then,
		 further down in this function, log this entry to the
		 wtmp. */
	      if (utmpx_ptr->ut_type == DEAD_PROCESS) {
		utmp.ut_id[3] = *char_p;
		cap_pututxline(&utmp);
		didutmp = 1;
		i = NUM_SLOTS;
		break;
	      } else if (utmpx_ptr->ut_type == USER_PROCESS) {
		/* The entry is for a user process, so note that it is
		 * used.
		 */
		slots_char_table[((int)*char_p)-FIRST_ASCII_PRINTABLE].used
		  = 1;
		i++;
	      }
	  }
	  if (i < NUM_SLOTS) {
	    /* If a slot is available, find it in the table */
	    for (i = 0; (i < NUM_SLOTS); i++)
	      if (!(slots_char_table[i].used)) {
		utmp.ut_id[3] = slots_char_table[i].character;
		break;
	      }
	    cap_pututxline(&utmp);
	    didutmp = 1;
	  }
	  /* unlock the utmpx file */
	  unlock_utmpx(wtmpx_lock_fd);
	}
	if (!didutmp)
		SCPYN(utmp.ut_id, "ftp?");

	/*
	 * updwtmpx attempts to update both wtmp and wtmpx,
	 * which must be kept in sync for 'last' to produce
	 * any helpful remote-host info.
	 */
	if (chr)
		__keepwtmps_open = 1;
	cap_updwtmpx(WTMPX_FILE, &utmp);
}

/*
 * Kludge used by end_login and dologout.
 */
/*ARGSUSED*/
logwtmp(a, b, c)
    char *a,*b,*c;
{
    static char satid[5];
    const cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
    cap_t ocap;
	
    utmp.ut_type = DEAD_PROCESS;
    utmp.ut_xtime = time(0);
    utmp.ut_host[0] = '\0';
    if (didutmp) {
	    setutxent();
	    cap_pututxline(&utmp);
	    endutxent();
    }
    /* update wtmp and wtmpx files */
    /* last likes this! */
    utmp.ut_user[0] = '\0';
    cap_updwtmpx(WTMPX_FILE, &utmp);
    strncpy(satid, utmp.ut_id, sizeof(utmp.ut_id));
    ocap = cap_acquire(1, &cap_audit_write);
    satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
	      "ftpd|+|?|Remote ftp logout id %s", satid);
    cap_surrender(ocap);
}
#endif	/* sgi */

/*
 * Record logout in wtmp file
 * and exit with supplied status.
 */
dologout(status)
	int status;
{
	if (logged_in) {
		(void) cap_seteuid((uid_t)0);
		logwtmp(ttyline, "", "");
	}
	/* beware of flushing buffers after a SIGPIPE */
	_exit(status);
}

void
myoob()
{
	char *cp;

	/* only process if transfer occurring */
	if (!transflag)
		return;
	cp = tmpline;
	if (getline(cp, 7, stdin) == NULL) {
		reply(221, "You could at least say goodbye.");
		dologout(0);
	}
	upper(cp);
	if (strcmp(cp, "ABOR\r\n") == 0) {
		tmpline[0] = '\0';
		reply(426, "Transfer aborted. Data connection closed.");
		reply(226, "Abort successful");
		longjmp(urgcatch, 1);
	}
	if (strcmp(cp, "STAT\r\n") == 0) {
		if (file_size != (off_t) -1)
			reply(213, "Status: %llu of %llu bytes transferred",
			    byte_count, file_size);
		else
			reply(213, "Status: %llu bytes transferred", byte_count);
	}
}

/*
 * Note: a response of 425 is not mentioned as a possible response to
 * 	the PASV command in RFC959. However, it has been blessed as
 * 	a legitimate response by Jon Postel in a telephone conversation
 *	with Rick Adams on 25 Jan 89.
 */
passive()
{
	int len;
	register char *p, *a;

	pdata = socket(AF_INET, SOCK_STREAM, 0);
	if (pdata < 0) {
		perror_reply(425, "Can't open passive connection");
		return;
	}
	pasv_addr = ctrl_addr;
	pasv_addr.sin_port = 0;
	if (cap_bind(pdata, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr)) < 0) {
		goto pasv_error;
	}
	len = sizeof(pasv_addr);
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr, &len) < 0)
		goto pasv_error;
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	a = (char *) &pasv_addr.sin_addr;
	p = (char *) &pasv_addr.sin_port;

#define UC(b) (((int) b) & 0xff)

	reply(227, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", UC(a[0]),
		UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
	return;

pasv_error:
	(void) close(pdata);
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

/*
 * Generate unique name for file with basename "local".
 * The file named "local" is already known to exist.
 * Generates failure reply on error.
 */
char *
gunique(local)
	char *local;
{
	static char new[MAXPATHLEN];
	struct stat st;
	char *cp = rindex(local, '/');
	int count = 0;

	if (cp)
		*cp = '\0';
	if (stat(cp ? local : ".", &st) < 0) {
		perror_reply(553, cp ? local : ".");
		return((char *) 0);
	}
	if (cp)
		*cp = '/';
	(void) strcpy(new, local);
	cp = new + strlen(new);
	*cp++ = '.';
	for (count = 1; count < 100; count++) {
		(void) sprintf(cp, "%d", count);
		if (stat(new, &st) < 0)
			return(new);
	}
	reply(452, "Unique file name cannot be created.");
	return((char *) 0);
}

/*
 * Format and send reply containing system error number.
 */
perror_reply(code, string)
	int code;
	char *string;
{
	reply(code, "%s: %s.", string, strerror(errno));
}

static char *onefile[] = {
	"",
	0
};

send_file_list(whichfiles)
	char *whichfiles;
{
	struct stat st;
	DIR *dirp = NULL;
#ifdef sgi
	struct dirent64 *dir;
	char *ndirname = ".";
#else
	struct direct *dir;
#endif
	FILE *dout = NULL;
	register char **dirlist, *dirname;
	int simple = 0;
	int freeglob = 0;
	char *ppat, patbuf[MAXPATHLEN+1];
	glob_t gl;


	if (strstr(whichfiles, "../*/..")) {
 		reply(550, "Arguments too long");
		return;
 	}

	if (strchr(whichfiles, '~') != NULL) {
		ppat = ftpd_tilde(whichfiles,patbuf);
		if (ppat != patbuf)
			strcpy(patbuf, ppat);
	} else
		strcpy(patbuf,whichfiles);

	if (strpbrk(patbuf, "{[*?") != NULL) {
		int flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE;

		memset(&gl, 0, sizeof(gl));
		freeglob = 1;
		if (glob(patbuf, flags, 0, &gl)) {
			reply(550, "not found");
			goto out;
		} else if (gl.gl_pathc == 0) {
			errno = ENOENT;
			perror_reply(550, whichfiles);
			goto out;
		}
		dirlist = gl.gl_pathv;
	} else {
		onefile[0] = patbuf;
		dirlist = onefile;
		simple = 1;
	}

	if (setjmp(urgcatch)) {
		transflag = 0;
		return;
	}
	while (dirname = *dirlist++) {
		if (stat(dirname, &st) < 0) {
			/*
			 * If user typed "ls -l", etc, and the client
			 * used NLST, do what the user meant.
			 */
			if (dirname[0] == '-' && *dirlist == NULL &&
			    transflag == 0) {
				if ((guest || restricted) && safer_guest)
					retrieve("/bin/ls -L %s", dirname);
				else
					retrieve("/bin/ls %s", dirname);
				return;
			}
			perror_reply(550, whichfiles);
			if (dout != NULL) {
				(void) fclose(dout);
				transflag = 0;
				data = -1;
				pdata = -1;
			}
			return;
		}

		if ((st.st_mode&S_IFMT) == S_IFREG) {
			if (dout == NULL) {
				dout = dataconn("file list", (off_t)-1, "w");
				if (dout == NULL)
					return;
				transflag++;
			}
			fprintf(dout, "%s%s\n", dirname,
				type == TYPE_A ? "\r" : "");
			byte_count += strlen(dirname) + 1;
			continue;
		} else if ((st.st_mode&S_IFMT) != S_IFDIR)
			continue;

#ifdef sgi
		/*
		 * we do this silliness because bsd opendir treats "" as "."
		 */
		if (dirname != NULL && *dirname != '\0')
			ndirname = dirname;
		if ((dirp = opendir(ndirname)) == NULL)
#else
		if ((dirp = opendir(dirname)) == NULL)
#endif
			continue;

		while ((dir = readdir(dirp)) != NULL) {
			char nbuf[MAXPATHLEN];
#ifdef sgi
			/*
			 * since dirent doesn't have namelen and dirent names
			 * are guaranteed to be null-terminated
			 */
			if (dir->d_name[0] == '.' && dir->d_name[1] == '\0')
				continue;
			if (dir->d_name[0] == '.' && dir->d_name[1] == '.' &&
			    dir->d_name[2] == '\0')
				continue;
#else
			if (dir->d_name[0] == '.' && dir->d_namlen == 1)
				continue;
			if (dir->d_name[0] == '.' && dir->d_name[1] == '.' &&
			    dir->d_namlen == 2)
				continue;
#endif /* sgi */
			sprintf(nbuf, "%s/%s", dirname, dir->d_name);

			/*
			 * We have to do a stat to insure it's
			 * not a directory or special file.
			 */
			if (simple || (stat(nbuf, &st) == 0 &&
			    (st.st_mode&S_IFMT) == S_IFREG)) {
				if (dout == NULL) {
					dout = dataconn("file list", (off_t)-1,
						"w");
					if (dout == NULL)
						return;
					transflag++;
				}
				if (nbuf[0] == '.' && nbuf[1] == '/')
					fprintf(dout, "%s%s\n", &nbuf[2],
						type == TYPE_A ? "\r" : "");
				else
					fprintf(dout, "%s%s\n", nbuf,
						type == TYPE_A ? "\r" : "");
				byte_count += strlen(nbuf) + 1;
			}
		}
		(void) closedir(dirp);
	}

	if (dout == NULL)
		reply(550, "No files found.");
	else if (ferror(dout) != 0)
		perror_reply(550, "Data connection");
	else
		reply(226, "Transfer complete.");

	transflag = 0;
	if (dout != NULL)
		(void) fclose(dout);
	data = -1;
	pdata = -1;
out:
	if (freeglob) {
		freeglob = 0;
		globfree(&gl);
	}
}

#ifdef SETPROCTITLE
/*
 * clobber argv so ps will show what we're doing.
 * (stolen from sendmail)
 * warning, since this is usually started from inetd.conf, it
 * often doesn't have much of an environment or arglist to overwrite.
 */

/*VARARGS1*/
setproctitle(fmt, a, b, c)
char *fmt;
{
	register char *p, *bp, ch;
	register int i;
	char buf[BUFSIZ];

	(void) sprintf(buf, fmt, a, b, c);

	/* make ps print our process name */
	p = Argv[0];
	*p++ = '-';

	i = strlen(buf);
	if (i > LastArgv - p - 2) {
		i = LastArgv - p - 2;
		buf[i] = '\0';
	}
	bp = buf;
	while (ch = *bp++)
		if (ch != '\n' && ch != '\r')
			*p++ = ch;
	while (p < LastArgv)
		*p++ = ' ';
}
#endif /* SETPROCTITLE */

#define EOS	'\0'
#define TILDE	'~'
#define SLASH	'/'
/*
 * expand tilde from the passwd file.
 */
char *
ftpd_tilde(char *pattern, char *patbuf)
{
	struct passwd *pwd;
	char *h;
	const char *p;
	char *b;

	if (*pattern != TILDE)
		return pattern;

	/* Copy up to the end of the string or / */
	for (p = pattern + 1, h = (char *) patbuf; *p && *p != SLASH; 
	     *h++ = *p++)
		continue;

	*h = EOS;

	if (((char *) patbuf)[0] == EOS) {
		/* 
		 * handle a plain ~ or ~/ by expanding the password file
		 */
		if (pw == NULL)
			return pattern;
		else
			h = pw->pw_dir;
	}
	else {
		/*
		 * Expand a ~user
		 */
		if ((pwd = getpwnam((char*) patbuf)) == NULL)
			return pattern;
		else
			h = pwd->pw_dir;
	}

	/* Copy the home directory */
	for (b = patbuf; *h; *b++ = *h++)
		continue;
	
	/* Append the rest of the pattern */
	while ((*b++ = *p++) != EOS)
		continue;

	return patbuf;
}

win(int size)
{
	big_pipe = size ? size : ENOUGH;
	reply(250, "TCP windows set to %d", big_pipe);
}

direct(int size)
{
	switch (size) {
	    case -1:
		odirect = ENOUGH;
		break;
	    default:
		odirect = size;
		break;
	} 
	reply(250, "Remote server direct I/O buffer set to %d", odirect);
}
