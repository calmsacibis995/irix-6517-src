/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rexecd.c	5.7 (Berkeley) 1/4/89";
#endif /* not lint */

#define _BSD_SIGNALS

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <stdio.h>
#include <errno.h>
#include <ia.h>
#include <signal.h>
#include <netdb.h>
#include <limits.h>
#include <paths.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <strings.h>
#include <stdlib.h>
#include <proj.h>
#include <crypt.h>
#include <sys/capability.h>
#if defined(_SHAREII) || defined(DCE)
#include	<dlfcn.h>
#endif /* defined(_SHAREII) || defined(DCE) */
#ifdef _SHAREII
#include	<shareIIhooks.h>
SH_DECLARE_HOOK(SETMYNAME);
SH_DECLARE_HOOK(REXEC);
#endif /* _SHAREII */

int lflag = 0;

#if sgi && !defined(NCARGS)
#define	NCARGS	10240		/* # characters in exec arglist */
#endif

#ifdef DCE
int dce_verify(char *user, uid_t uid, gid_t gid, char *passwd, char **msg);
#endif /* DCE */

static void	error(const char *, ...);
static void	doit(int, struct sockaddr_in *);
static void	getstr(char *, int, char *);

void
usage(void)
{
	syslog(LOG_ERR, "usage: rexecd [-l]");
	exit(1);
}

/*
 * remote execute server:
 *	username\0
 *	password\0
 *	command\0
 *	data
 */

main(argc, argv)
	int argc;
	char **argv;
{
	struct sockaddr_in from;
	int fromlen;
	int c;

	while ((c = getopt(argc, argv, "l")) != -1) {
		switch (c) {
		case '?':
		default:
			usage();
		case 'l':
			lflag++;
			break;
		}
	}

	openlog("rexecd", LOG_PID, LOG_DAEMON);
	fromlen = sizeof (from);
	if (getpeername(0, &from, &fromlen) < 0) {
		fprintf(stderr, "%s: ", argv[0]);
		perror("getpeername");
		exit(1);
	}
	doit(0, &from);
	/*NOTREACHED*/
}

#define NMAX    64
char	homedir[5+PATH_MAX] = "HOME=";
char	shell[6+PATH_MAX] = "SHELL=";
char	path[5+PATH_MAX] = "PATH=";
char	logname[8+NMAX] = "LOGNAME=";
char	username[5+NMAX] = "USER=";
char	env_hostname[12+MAXHOSTNAMELEN] = { "REMOTEHOST=" };
char	tz[256] = "TZ=";
#define TZ_INDEX  6
#ifdef DCE
char	dce_tktfile[1024] = { "KRB5CCNAME=" };
#define DCE_INDEX 7
char	*envinit[] = {
	homedir,
	shell,
	path,
	username,
	logname,
	env_hostname,
	tz,
	dce_tktfile,
	0};
#else /* DCE */
char	*envinit[] = {
	homedir,
	shell,
	path,
	username,
	logname,
	env_hostname,
	tz,
	0};
#endif /* DCE */
extern	char	**environ;

struct	sockaddr_in asin;

/* Global buffers are page aligned by ld, which is a Good Thing if you like
 *	page flipping.
 */
char	buf[16 * 1024];

static void
doit(f, fromp)
	int f;
	struct sockaddr_in *fromp;
{
	char cmdbuf[NCARGS+1], *cp, *namep;
	char user[16], pass[16];
	uinfo_t uinfo;
	int s;
	u_short port;
	int pv[2], pid, ready, readfrom, cc;
	char sig;
	char *host;
	extern struct hostent * __getverfhostent(struct in_addr, int);
	int one = 1;
#ifdef DCE
	int dfs_authentication = 0;
#endif /* DCE */
	cap_t ocap;
	cap_value_t cap_setuid = CAP_SETUID;
	cap_value_t cap_setgid = CAP_SETGID;

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
	{ int t = open("/dev/tty", 2);
	  if (t >= 0) {
		ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	  }
	}
#endif
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if (read(f, &c, 1) != 1)
			exit(1);
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}
	(void) alarm(0);
	if (port != 0) {
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
			exit(1);
		asin.sin_family = AF_INET;
		if (bind(s, &asin, sizeof (asin)) < 0)
			exit(1);
		(void) alarm(60);
		fromp->sin_port = htons(port);
		if (connect(s, fromp, sizeof (*fromp)) < 0)
			exit(1);
		(void) alarm(0);
	}
	host = __getverfhostent(fromp->sin_addr, 1)->h_name;
	getstr(user, sizeof(user), "username");
	getstr(pass, sizeof(pass), "password");
	getstr(cmdbuf, sizeof(cmdbuf), "command");

	if (ia_openinfo(user, &uinfo)) {
		syslog(LOG_NOTICE|LOG_AUTH, "from %s: %s login unknown",
			 host, user);
		error("Login incorrect.\n");
		exit(1);
	}
#ifdef DCE
	dfs_authentication = 0;
	if (strlen(uinfo->ia_pwdp) && !strcmp(uinfo->ia_pwdp,"-DCE-")) {
		void *handle;
		char *dce_err;
		dfs_authentication = 1;
		if (!(handle=sgidladd("libdce.so",RTLD_LAZY))) {
			syslog(LOG_NOTICE|LOG_AUTH, 
				"from %s: %s DCE validation error, unable to load DCE library",
		 		host, user);
			error("DCE validation error.\n");
			exit(1);
		}
		if (!dlsym(handle,"dce_verify")) {
			syslog(LOG_NOTICE|LOG_AUTH, 
				"from %s: %s DCE validation error, incorrect DCE library",
		 		host, user);
			error("DCE validation error.\n");
			exit(1);
		}
		if (dce_verify(user, uinfo->ia_uid, uinfo->ia_gid,
			       pass, &dce_err)) {
			if (dce_err) {
				syslog(LOG_NOTICE|LOG_AUTH, 
					"from %s: %s DCE validation error, %s",
			 		host, user, dce_err);
				free(dce_err);
				error("DCE validation error.\n");
			} else {
				syslog(LOG_NOTICE|LOG_AUTH, 
					"from %s: %s DCE password incorrect",
			 		host, user);
				error("DCE password incorrect.\n");
			}
			exit(1);
		}
	} else if (strlen(uinfo->ia_pwdp)) {
#else /* DCE */
	if (*uinfo->ia_pwdp != '\0') {
#endif /* DCE */
		namep = crypt(pass, uinfo->ia_pwdp);
		if (strcmp(namep, uinfo->ia_pwdp)) {
			syslog(LOG_NOTICE|LOG_AUTH, 
				"from %s: %s password incorrect",
				host, user);
			error("Password incorrect.\n");
			exit(1);
		}
	}
	/*
	 * _PATH_NOLOGIN is normally defined as "/etc/nologin"
	 * so it ***SHOULD*** be accessible to root. (unless
	 * someone linked it into DFS!)
	 */
	if (uinfo->ia_uid && !access(_PATH_NOLOGIN, F_OK)) {
		error("Logins currently disabled.\n");
		exit(1);
	}
	/* 
	 * According to the rexecd(8) man page, the null byte is returned
	 * after successful completion of the validation steps.  Check
	 * the setgid/setuid status before saying things are OK.
	 */
	ocap = cap_acquire(1, &cap_setgid);
	if (setgid(uinfo->ia_gid)) {
		cap_surrender(ocap);
		error("Bad group ID.\n");
		syslog(LOG_NOTICE|LOG_AUTH, "user '%s' has invalid gid %d", 
		    uinfo->ia_name, uinfo->ia_gid);
		exit(1);
	}

#ifdef _SHAREII
	/*
	 *	Perform Share II resource limit checks.
	 */
	if (sgidladd(SH_LIMITS_LIB, RTLD_LAZY))
	{
		static const char *Myname = "rexecd";

		SH_HOOK_SETMYNAME(Myname);
		if (SH_HOOK_REXEC(uinfo->ia_uid))
		{
			syslog(LOG_NOTICE|LOG_AUTH,
			       "Share II resource limit checks for user '%s' failed",
			       uinfo->ia_name);
			exit(1);
		}
	}
#endif /* _SHAREII */

	cap_surrender(ocap);

	/*
	 * Start a new array session and set up this user's default
	 * project ID while we still have root privileges
	 */
	ocap = cap_acquire(1, &cap_setuid);
	newarraysess();
	setprid(getdfltprojuser(uinfo->ia_name));
	cap_surrender(ocap);

	ocap = cap_acquire(1, &cap_setgid);
	(void) initgroups(uinfo->ia_name, uinfo->ia_gid);
	cap_surrender(ocap);
	ocap = cap_acquire(1, &cap_setuid);
	if (setuid(uinfo->ia_uid) < 0) {
		cap_surrender(ocap);
		error("Bad user ID.\n");
		syslog(LOG_NOTICE|LOG_AUTH, "user '%s' has invalid uid %d", 
		    uinfo->ia_name, uinfo->ia_uid);
		exit(1);
	}
	cap_surrender(ocap);

	/* Change to the user's home directory before we declare success. */
	if (chdir(uinfo->ia_dirp) < 0) {
		error("No remote directory.\n");
		exit(1);
	}

	if (lflag) {
		syslog(LOG_INFO|LOG_AUTH, "%s from %s: cmd='%.80s'",
			user, host, cmdbuf);
	}

	(void) write(2, "\0", 1);
	if (port) {
		(void) pipe(pv);
		pid = fork();
		if (pid == -1)  {
			error("Try again.\n");
			exit(1);
		}
		if (pid) {
			(void) close(0); (void) close(1); (void) close(2);
			(void) close(f); (void) close(pv[1]);
			readfrom = (1<<s) | (1<<pv[0]);
			ioctl(pv[1], FIONBIO, (char *)&one);
			/* should set s nbio! */
			do {
				ready = readfrom;
				(void) select(16, (fd_set *)&ready, (fd_set *)0,
				    (fd_set *)0, (struct timeval *)0);
				if (ready & (1<<s)) {
					if (read(s, &sig, 1) <= 0)
						readfrom &= ~(1<<s);
					else
						killpg(pid, sig);
				}
				if (ready & (1<<pv[0])) {
					cc = read(pv[0], buf, sizeof (buf));
					if (cc <= 0) {
						shutdown(s, 1+1);
						readfrom &= ~(1<<pv[0]);
					} else
						(void) write(s, buf, cc);
				}
			} while (readfrom);
			exit(0);
		}
		setpgrp();
		(void) close(s); (void)close(pv[0]);
		dup2(pv[1], 2);
	}
	if (*uinfo->ia_shellp == '\0')
		uinfo->ia_shellp = "/bin/sh";
	closelog();
	for (f = getdtablehi(); --f > 2; )
		fcntl(f,F_SETFD,FD_CLOEXEC);
	cp = getenv("TZ");		/* set the time zone */
	if (!cp)
		envinit[TZ_INDEX] = 0;
	else
		strncat(tz, cp, sizeof(tz)-4);
#ifdef DCE
	if (dfs_authentication) {
		envinit[DCE_INDEX] = dce_tktfile;
		strncat(dce_tktfile, getenv("KRB5CCNAME"),
			(sizeof(dce_tktfile) - 11));
	}
#endif /* DCE */
	strncat(logname, uinfo->ia_name, sizeof(logname)-9);
	strncat(env_hostname, host, sizeof(env_hostname)-12);
	strncat(path, uinfo->ia_uid == 0 ? _PATH_ROOTPATH : _PATH_USERPATH,
		    sizeof(path)-5);
	environ = envinit;
	strncat(homedir, uinfo->ia_dirp, sizeof(homedir)-6);
	strncat(shell, uinfo->ia_shellp, sizeof(shell)-7);
	strncat(username, uinfo->ia_name, sizeof(username)-6);
	cp = rindex(uinfo->ia_shellp, '/');
	if (cp)
		cp++;
	else
		cp = uinfo->ia_shellp;
	if (sysconf(_SC_CAP) > 0) {
		cap_t pcap = cap_init();
		(void) cap_set_proc(pcap);
		cap_free(pcap);
	}
	execl(uinfo->ia_shellp, cp, "-c", cmdbuf, 0);
	perror(uinfo->ia_shellp);
	exit(1);
}

static void
error(const char *fmt, ...)
{
	va_list args;
	char ebuf[BUFSIZ];

	va_start(args, fmt);
	ebuf[0] = 1;
	(void) vsprintf(ebuf+1, fmt, args);
	(void) write(2, ebuf, strlen(ebuf));
	va_end(args);
}

static void
getstr(buf, cnt, err)
	char *buf;
	int cnt;
	char *err;
{
	char c;

	do {
		if (read(0, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0) {
			error("%s too long\n", err);
			exit(1);
		}
	} while (c != 0);
}
