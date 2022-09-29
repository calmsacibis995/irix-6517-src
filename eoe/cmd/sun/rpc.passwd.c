#ifndef lint
static char sccsid[] = 	"@(#)rpc.yppasswdd.c	1.6 88/08/15 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <sys/file.h>
#include <rpc/rpc.h>
#include <pwd.h>
#include <rpcsvc/yppasswd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <strings.h>
#include <syslog.h>
#include <limits.h>
#include <unistd.h>
#include <cap_net.h>
#include <sys/types.h>
#include <sys/stat.h>

extern int __svc_label_agile;

char ypmake[]	= "/var/yp/ypmake";
char ypdir[]	= "/var/yp";
char ptmp[]	= "ptmp";
char pwdlock[]	= ".pwd.lock";

#define pwmatch(name, line, cnt) \
	(line[cnt] == ':' && strncmp(name, line, cnt) == 0)

char *file;			/* file in which passwd's are found */
char temp[PATH_MAX];		/* temporary file for modifying 'file' */
char lock[PATH_MAX];		/* new flock-style lockfile */
int mflag;			/* do a make */

static void boilerplate(struct svc_req *, SVCXPRT *);
static void checkfield(char *);
static void changepasswd(struct svc_req *, SVCXPRT *);
static int Argc;
static char **Argv;

#define dup2(x,y)	BSDdup2(x,y)
static int rresvport(void);
static struct passwd *mygetpwnam(const char *);

static int allow_gecos = 1;
static int allow_dir = 1;
static int allow_shell = 1;
static int allow_pw = 1;

void
usage(char *program)
{
	syslog(LOG_ERR,
    "usage: %s file [-nogecos][-nodir][-noshell][-nopw][-m arg1 arg2 ...]",
	    program);
	exit(1);
}

main(argc, argv)
	char **argv;
{
	SVCXPRT *transp;
	int s;
	char *cp;
	char *program;

	__svc_label_agile = (sysconf(_SC_IP_SECOPTS) > 0);

	Argc = argc;
	Argv = argv;
	
	program = strrchr(argv[0], '/');
	if (program) {
		program++;
	} else {
		program = argv[0];
	}

	/*
	 * Openlog so we don't have to id each message.  Delay connection
	 * because we might never log anything.
	 */
	openlog("rpc.passwd", LOG_PID|LOG_ODELAY, LOG_AUTH);

	if (argc < 2) {
		usage(program);
	}
	file = argv[1];
	for (Argc -= 2, Argv += 2; Argc > 0; Argc--, Argv++) {
		if (Argv[0][0] != '-') {
			usage(program);
		}
		if (Argv[0][1] == 'm') {
			mflag++;
			break;
		} else if (strncmp(*Argv, "-nogecos",
		    sizeof("-nogecos") - 1) == 0) {
			allow_gecos = 0;
		} else if (strncmp(*Argv, "-nodir",
		    sizeof("-nodir") - 1) == 0) {
			allow_dir = 0;
		} else if (strncmp(*Argv, "-noshell",
		    sizeof("-noshell") - 1) == 0) {
			allow_shell = 0;
		} else if (strncmp(*Argv, "-nopw",
		    sizeof("-nopw") - 1) == 0) {
			allow_pw = 0;
		} else {
			usage(program);
		}
	}

	if (chdir(ypdir) < 0) {
		syslog(LOG_ERR, "can't chdir to %s", ypdir);
		exit(1);
	}

	/*
	 * Check write access after the chdir, so that if the user screwed up
	 * and gave a relative pathname we catch it (unless of course it's in
	 * the NIS directory).
	 */
	if (access(file, W_OK) < 0) {
		syslog(LOG_ERR, "can't write %s: %m", file);
		exit(1);
	}

	if ((s = rresvport()) < 0) {
		syslog(LOG_ERR, "can't bind to a privileged socket");
		exit(1);
	}
	transp = svcudp_create(s);
	if (transp == NULL) {
		syslog(LOG_ERR, "couldn't create an RPC server");
		exit(1);
	}
	pmap_unset(YPPASSWDPROG, YPPASSWDVERS);
	if (!svc_register(transp, YPPASSWDPROG, YPPASSWDVERS_ORIG,
	    boilerplate, IPPROTO_UDP)) {
		syslog(LOG_ERR, "couldn't register yppasswdd");
		exit(1);
	}
	pmap_unset(YPPASSWDPROG, YPPASSWDVERS_NEWSGI);
	if (!svc_register(transp, YPPASSWDPROG, YPPASSWDVERS_NEWSGI,
	    boilerplate, IPPROTO_UDP)) {
		syslog(LOG_ERR, "couldn't register new version of yppasswdd");
	}

	if (fork())
		exit(0);
	{ int t;
	for (t = getdtablesize(); --t >= 0; )
		if (t != s)
			(void) close(t);
	}
	(void) open("/", O_RDONLY);
	(void) dup2(0, 1);
	(void) dup2(0, 2);
	{ int tt = open("/dev/tty", O_RDWR);
	  if (tt > 0) {
		ioctl(tt, TIOCNOTTY, 0);
		close(tt);
	  }
	}

	/*
	 * Put the tmp and lock files in the same dir as the given file, so we
	 * don't have cross-device rename failures and name collisions.
	 */
	strcpy(temp, file);
	strcpy(lock, file);
	if (cp = strrchr(temp, '/')) {
		strcpy(++cp, ptmp);
		strcpy(lock + (cp - temp), pwdlock);
	} else {
		strcpy(temp, ptmp);
		strcpy(lock, pwdlock);
	}

	/*
	 * If the filename has a dot-suffix, append it to the tmp and lock
	 * filenames to avoid name collisions.
	 */
	if ((cp = strrchr(file, '.')) && strchr(cp, '/') == NULL) {
		strcat(temp, cp);
		strcat(lock, cp);
	}

	/*
	 * Ignore SIGCHLD so we never get zombies that we have to
	 * wait for.  Clean up after a crash.
	 */
	(void) signal(SIGCHLD, SIG_IGN);
	(void) signal(SIGTSTP, SIG_IGN);
	(void) unlink(temp);
	svc_run();
	syslog(LOG_ALERT, "svc_run shouldn't have returned: %m");
}

static void
boilerplate(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	switch(rqstp->rq_proc) {
		case NULLPROC:
			if (!svc_sendreply(transp, xdr_void, 0))
				syslog(LOG_ERR, "couldn't reply to RPC call");
			break;
		case YPPASSWDPROC_UPDATE:
			changepasswd(rqstp, transp);
			break;
	}
}

static void
changepasswd(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	int tempfd, i, len, good=0;
	static int ans;
	FILE *tempfp, *filefp, *fp;
	char buf[256], *p;
	char cmdbuf[BUFSIZ];
	void (*f1)(), (*f2)(), (*f3)();
	struct passwd *oldpw, *newpw;
	struct yppasswd yppasswd;
	int status;
	char *crypt(const char *, const char *);
	struct stat statbuf;

	bzero(&yppasswd, sizeof(yppasswd));
	if (!svc_getargs(transp, xdr_yppasswd, &yppasswd)) {
		svcerr_decode(transp);
		return;
	}
	/*
	 * Clean up from previous changepasswd() call
	 */
	while (wait3(&status, WNOHANG, 0) > 0)
		continue;

	newpw = &yppasswd.newpw;
	ans = 1;
	oldpw = mygetpwnam(newpw->pw_name);
	if (oldpw == NULL) {
		syslog(LOG_ERR, "no passwd for %s", newpw->pw_name);
		goto done;
	}
	switch (rqstp->rq_vers) {
	case YPPASSWDVERS_NEWSGI:
		if (rqstp->rq_cred.oa_flavor == AUTH_UNIX &&
		    ((struct authunix_parms *)rqstp->rq_clntcred)->aup_uid
			== oldpw->pw_uid &&
		    strcmp(newpw->pw_passwd, oldpw->pw_passwd) == 0) {
			break;
		}
		/* FALL THROUGH */
	case YPPASSWDVERS_ORIG:
		if (oldpw->pw_passwd && *oldpw->pw_passwd &&
		    strcmp(crypt(yppasswd.oldpass,oldpw->pw_passwd),
			   oldpw->pw_passwd) != 0) {
			syslog(LOG_ERR, "%s: bad passwd", newpw->pw_name);
			goto done;
		}
	}
	(void) umask(0);

	statbuf.st_mode=0;
	if (stat(file, &statbuf) < 0) {
		statbuf.st_mode=0644;
	} 
	statbuf.st_mode |= 0600;

	f1 = signal(SIGHUP, SIG_IGN);
	f2 = signal(SIGINT, SIG_IGN);
	f3 = signal(SIGQUIT, SIG_IGN);
	tempfd = open(temp, O_WRONLY|O_CREAT|O_EXCL, statbuf.st_mode);
	if (tempfd < 0 || lockpwf() < 0) {
		if (errno == EEXIST || errno == EAGAIN)
			syslog(LOG_ERR, "password file busy");
		else
			syslog(LOG_ERR, "%s: %m", temp);
		goto cleanup;
	}
	if ((tempfp = fdopen(tempfd, "w")) == NULL) {
		syslog(LOG_ERR, "fdopen failed: %m?");
		goto cleanup;
	}
	/*
	 * Copy passwd to temp, replacing matching lines
	 * with new password.
	 */
	if ((filefp = fopen(file, "r")) == NULL) {
		syslog(LOG_ERR, "fopen of %s failed: %m?", file);
		goto cleanup;
	}
	checkfield(newpw->pw_passwd);
	if (rqstp->rq_vers == YPPASSWDVERS_NEWSGI) {
		checkfield(newpw->pw_gecos);
		checkfield(newpw->pw_dir);
		checkfield(newpw->pw_shell);
	}

	len = strlen(newpw->pw_name);
	while (fgets(buf, sizeof(buf), filefp)) {
		p = index(buf, ':');
		if (p && p - buf == len
		    && strncmp(newpw->pw_name, buf, len) == 0) {
#define	PICK(check, field) \
((rqstp->rq_vers == YPPASSWDVERS_NEWSGI) && (check) ? newpw : oldpw)->field
			good = fprintf(tempfp,"%s:%s:%d:%d:%s:%s:%s\n",
			    oldpw->pw_name,
			    (allow_pw) ? newpw->pw_passwd : oldpw->pw_passwd,
			    oldpw->pw_uid,
			    oldpw->pw_gid,
			    PICK(allow_gecos, pw_gecos),
			    PICK(allow_dir, pw_dir),
			    PICK(allow_shell, pw_shell));
			if (good < 0) {
				break;
			}
#undef	PICK
		} else
			good = fputs(buf, tempfp);
			if (good == EOF) {
				good = -1;
				break;
			}
	}
	fclose(filefp);
	fclose(tempfp);
	if ((good == -1) || (rename(temp, file) < 0)) {
		syslog(LOG_ERR,"rename: %m");
		unlink(temp);
		goto cleanup;
	}
	ans = 0;
	/*
	 * Reset SIGCHLD status to SIG_DFL before forking, so that
	 * descendants of this process (e.g. the shell) can wait for kids.
	 */
	(void) signal(SIGCHLD, SIG_DFL);
	if (mflag && fork() == 0) {
		strcpy(cmdbuf, ypmake);
		for (i = 1; i < Argc; i++) {
			strcat(cmdbuf, " ");
			strcat(cmdbuf, Argv[i]);
		}
#ifdef DEBUG
		syslog(LOG_DEBUG, "about to execute %s", cmdbuf);
#else
		system(cmdbuf);
#endif
		exit(0);
	}
cleanup:
	/*
	 * Reset SIGCHLD status to SIG_IGN in the parent process, so
	 * that zombies will not be created.
	 */
	(void) signal(SIGCHLD, SIG_IGN);
	fclose(tempfp);
	(void) unlockpwf();
	signal(SIGHUP, f1);
	signal(SIGINT, f2);
	signal(SIGQUIT, f3);
done:
	if (!svc_sendreply(transp, xdr_int, &ans))
		syslog(LOG_ERR, "couldnt reply to RPC call");
}

static int
rresvport(void)
{
	struct sockaddr_in sin;
	int s, alport = IPPORT_RESERVED - 1;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return (-1);
	for (;;) {
		sin.sin_port = htons((u_short)alport);
		if (cap_bind(s, (struct sockaddr *)&sin, sizeof (sin)) >= 0)
			return (s);
		if (errno != EADDRINUSE && errno != EADDRNOTAVAIL)
			return (-1);
		alport--;
		if (alport == IPPORT_RESERVED/2)
			return (-1);
	}
}

static void
checkfield(p)
	char *p;
{
	while (*p != '\0') {
		if (*p == ':' || !isprint(*p))
			*p = '$';
		p++;
	}
}

static char *
pwskip(p)
register char *p;
{
	while( *p && *p != ':' && *p != '\n' )
		++p;
	if( *p ) *p++ = 0;
	return(p);
}

static struct passwd *
mygetpwnam(const char *name)
{
	FILE *pwf;
	int cnt;
	char *p;
	static char line[BUFSIZ+1];
	static struct passwd passwd;

	pwf = fopen(file, "r");
	if (pwf == NULL)
		return (NULL);
	cnt = strlen(name);
	while ((p = fgets(line, BUFSIZ, pwf)) && !pwmatch(name, line, cnt))
		;
	if (p == NULL) {
		fclose(pwf);
		return (NULL);
	}
	passwd.pw_name = p;
	p = pwskip(p);
	passwd.pw_passwd = p;
	p = pwskip(p);
	passwd.pw_uid = atoi(p);
	p = pwskip(p);
	passwd.pw_gid = atoi(p);
	passwd.pw_comment = "";
	p = pwskip(p);
	passwd.pw_gecos = p;
	p = pwskip(p);
	passwd.pw_dir = p;
	p = pwskip(p);
	passwd.pw_shell = p;
	while(*p && *p != '\n') p++;
	*p = '\0';
	fclose(pwf);
	return (&passwd);
}


/*
 * The following were stolen from passmgmt(1M).
 */
struct flock	rp_flock;
int		lockfd = -1;

static int
lockpwf()
{
	int retval;

	lockfd = open(lock, O_CREAT|O_WRONLY, 0600);
	if (lockfd < 0)
		return -1;
	rp_flock.l_type = F_WRLCK;
	retval = fcntl(lockfd, F_SETLK, &rp_flock);
	if (retval < 0) {
		(void) close(lockfd);
		lockfd = -1;
	}
	return retval;
}

static int
unlockpwf()
{
	if (lockfd < 0)
		return -1;
	rp_flock.l_type = F_UNLCK;
	(void) fcntl(lockfd, F_SETLK, &rp_flock);
	(void) close(lockfd);
	lockfd = -1;
	return 0;
}
