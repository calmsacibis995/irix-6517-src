/*
 * Copyright (c) 1983, 1988, 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rshd.c	5.34 (Berkeley) 6/29/90";
#endif /* not lint */

/*
 * remote shell server:
 *	[port]\0
 *	remuser\0
 *	locuser\0
 *	command\0
 *	data
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <errno.h>
#include <ia.h>
#include <netdb.h>
#include <syslog.h>
#include <limits.h>
#include <paths.h>
#include <proj.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sat.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <clearance.h>
#include <capability.h>
#include <unistd.h>
#ifdef _SHAREII
#include <dlfcn.h>
#include <shareIIhooks.h>
SH_DECLARE_HOOK(SETMYNAME);
SH_DECLARE_HOOK(RSH);
#endif /* _SHAREII */

#ifdef	sgi
#define	AUX_LANG			/* switch LANG handling on */
#endif	/* sgi */

#ifdef	AUX_LANG
#include <locale.h>
#define	LANG_FILE	"/.lang"	/* default lang file */
#define	TZ_FILE		"/.timezone"	/* default timezone file */
#define	MAX_LUSER	256		/* max size of user name + args */
#define	MAXARGS		64		/* max # of args */

char	envbuf[MAX_LUSER], envblk[MAX_LUSER];
char	*args[MAXARGS];
#endif	/* AUX_LANG */

#if sgi && !defined(NCARGS)
#define	NCARGS	10240		/* # characters in exec arglist */
#endif
void getstr( char *, int, char *, int);

#define setpgrp(a,b)		BSDsetpgrp(a,b)
static int log_success = 0;	/* If TRUE, log all successful accesses */
extern struct hostent * __getverfhostent(struct in_addr, int);

int	keepalive = 1;
int	check_all = 0;
void	error(const char *, ...);
void	doit(struct sockaddr_in *);
int	sent_null;
#define NBUFS	3
#define BUFSIZE	(64 * 1024)
char	*rshdbuf[NBUFS];

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char **argv;
{
	extern int opterr, optind;
	extern int _check_rhosts_file;
	struct linger linger;
	int ch, on = 1, fromlen;
	int i;
	struct sockaddr_in from;

	openlog("rshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

	if (cap_envl(0, CAP_SETUID, CAP_SETGID, CAP_AUDIT_WRITE,
		     CAP_PRIV_PORT, CAP_SETPCAP, CAP_NOT_A_CID) == -1) {
		syslog(LOG_ERR, "insufficient privilege");
		exit(1);
	}

	/*
	 * use memalign to set up page-aligned buffer
	 */
	for (i = 0; i < NBUFS; i++) {
		rshdbuf[i] = memalign(16*1024, BUFSIZE);
		if (!rshdbuf[i]) {
			syslog(LOG_ERR, "memalign(): %m");
			exit(1);
		}
	}

	opterr = 0;
	while ((ch = getopt(argc, argv, "alnL")) != EOF)
		switch (ch) {
		case 'a':
			check_all = 1;
			break;
		case 'l':
			_check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
		case 'L':
			log_success = 1;
			break;
		case '?':
		default:
			syslog(LOG_ERR, "usage: rshd [-alnL]");
			break;
		}

	argc -= optind;
	argv += optind;


	fromlen = sizeof (from);
	if (getpeername(0, &from, &fromlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		_exit(1);
	}
	if (keepalive &&
	    setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	linger.l_onoff = 1;
	linger.l_linger = 60;			/* XXX */
	if (setsockopt(0, SOL_SOCKET, SO_LINGER, (char *)&linger,
	    sizeof (linger)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
	doit(&from);
	/*NOTREACHED*/
}

#ifdef	AUX_LANG
/*
 * handle quoted chars
 */
static char *
quotec(s, cp)
register char *s;
int *cp;
{
	register int c, i, num;

	switch(c = *s++) {
	    case 'n' :
		c = '\n';
		break;
	    case 'r' :
		c = '\r';
		break;
	    case 'v' :
		c = '\013';
		break;
	    case 'b' :
		c = '\b';
		break;
	    case 't' :
		c = '\t';
		break;
	    case 'f' :
		c = '\f';
		break;
	    case '0' : case '1' : case '2' : case '3' :
	    case '4' : case '5' : case '6' : case '7' :
		for(num=0, i=0; i<3; i++) {
		    num = num * 8 + (c - '0');
		    c = *s++;
		    if(c < '0' || c > '7')
			break;
		}
		s--;
		c = num & 0377;
		break;
	    default :
		break;
	}
	*cp = c;
	return(s);
}

/*
 * get the args from login name
 */
static char **
getargs(s)
register char *s;
{
	int c, llen = MAX_LUSER - 1;
	register char *ptr = envbuf, **reply = args;
	enum {
		WHITESPACE, ARGUMENT
	} state = WHITESPACE;

	while(c = *s++) {
	    if(llen > 0)
		--llen;
	    switch(c) {
		case ' ' :
		case '\t' :
		    if(state == ARGUMENT) {
			*ptr++ = '\0';
			state = WHITESPACE;
		    }
		    break;
		case '\\' :
		    s = quotec(s, &c);
		default:
		    if(state == WHITESPACE) {
			*reply++ = ptr;
			state = ARGUMENT;
		    }
		    *ptr++ = (char)c;
	    }
	    /* chec kfor overflow */

	    if(ptr >= envbuf + MAX_LUSER - 1
		|| reply >= args + MAXARGS - 1 && state == WHITESPACE) {
			break;
	    }
	}
	*ptr = '\0';
	*reply = NULL;
	return((reply == args)? NULL : args);
}

/*
 * list of illegal environment variables
 */
static char *illegal[] = {
	"HOME=", "SHELL=", "PATH=", "USER=", "LOGNAME=",
	"REMOTEHOST=", "REMOTEUSER=", "TZ=",
	0,
};

/*
 * check if variable is legal
 */
static int
legalenvvar(s)
register char *s;
{
	register char **p;

	for(p = illegal; *p; p++)
	    if( !strncmp(s, *p, strlen(*p)))
		return(0);
	return(1);
}

/*
 * search for LANG= in environment list and switch to that locale
 */
char *
set_lang(envp)
char **envp;
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
#endif	/* AUX_LANG */

#define NMAX    64
char	tz[256] = "TZ=";
char	logname[8+NMAX] = "LOGNAME=";
char	username[5+NMAX] = "USER=";
char	homedir[5+PATH_MAX] = "HOME=";
char	shell[6+PATH_MAX] = "SHELL=";
char	env_hostname[12+MAXHOSTNAMELEN] = { "REMOTEHOST=" };
char	env_remuser[12+NMAX] = { "REMOTEUSER=" };
char	path[5+PATH_MAX] = "PATH=";
#define TZ_INDEX 7

#ifdef	AUX_LANG
char	lang[NL_LANGMAX + 6] = "LANG=";
char	*envinit[9 + MAXARGS] =
	{ homedir, shell, path, username, logname,
	  env_hostname, env_remuser, tz, lang, 0 };
#else	/* AUX_LANG */
char	*envinit[10 + MAXARGS] =
	{homedir, shell, path, username,
		logname, env_hostname, env_remuser, tz, 0};
#endif	/* AUX_LANG */

extern char	**environ;

#ifdef	AUX_LANG
/*
 * setup environment from extended user name
 */
void
setup_env(envp, home)
register char **envp;
char *home;
{
	register char *ep, *ptr;
	register int benv, i, k;
	int fd, j, li, length;
	char uhcfg[MAXPATHLEN + 11];

	/*
	 * read LANG_FILE
	 */
	(void)strncpy(uhcfg, home, MAXPATHLEN);
	(void)strcat(uhcfg, LANG_FILE);
	if((fd = open(uhcfg, O_RDONLY)) >= 0) {
	    int nrd;
	    if((nrd=read(fd, uhcfg, NL_LANGMAX + 2)) > 0) {
		if(ptr = strchr(uhcfg, '\n'))
		     *ptr = 0;
		else uhcfg[nrd]= 0;
		uhcfg[NL_LANGMAX] = 0;
		(void)strcpy(lang + 5, uhcfg);
	    }
	    (void)close(fd);
	}

	/*
	 * read TZ_FILE
	 */
	(void)strncpy(uhcfg, home, MAXPATHLEN);
	(void)strcat(uhcfg, TZ_FILE);
	if((fd = open(uhcfg, O_RDONLY)) >= 0) {
	    int nrd;
	    if((nrd=read(fd, uhcfg, sizeof(tz)-1)) > 0) {
		if(ptr = strchr(uhcfg, '\n'))
		     *ptr = 0;
		else uhcfg[nrd]= 0;
		uhcfg[sizeof(tz)-1]= 0;
		(void)strcpy(tz + 3, uhcfg);
	    }
	    (void)close(fd);
	    envinit[TZ_INDEX]= tz;
	}

	/*
	 * seek to end of basic environment
	 * add all the environment variables from login name
	 */
	for(benv = 0; envinit[benv]; benv++);
	ptr = envblk;
	for(j = k = li = 0; *envp && (j < (MAXARGS-1)); j++, envp++) {
	    if( !(ep = strchr(*envp, '='))) {
		envinit[benv + k] = ptr;
		i = sprintf(ptr, "L%d=%s", li, *envp);
		if(i >= 0) {
		    ptr += i;
		    *ptr++ = 0;
		}
		k++;
		li++;
	    } else if( !legalenvvar(*envp))
		continue;
	    /*
	     * check if this overwrites previous definition
	     * if not place it at the end
	     */
	    else {
		for(i = 0, length = ep + 1 - *envp; i < benv + k; i++) {
		    if( !strncmp(*envp, envinit[i], length)) {
			envinit[i] = *envp;
			break;
		    }
		}
		if(i == (benv + k)) {
		    envinit[benv + k] = *envp;
		    k++;
		}
	    }
	}
}
#endif	/* AUX_LANG */

int
connectfromresvport(struct sockaddr_in *sin, int sinlen)
{
	int s;
	cap_t ocap;
	cap_value_t capv = CAP_PRIV_PORT;
	int on = 1;
	struct sockaddr_in sinb;
	u_short lport = IPPORT_RESERVED - 1;
	int r;

	sinb.sin_family = AF_INET;
	sinb.sin_addr.s_addr = INADDR_ANY;
	for (;;) {
		sinb.sin_port = htons(lport);
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0) {
			return (-1);
		}
		r = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (r < 0) {
			close(s);
			return r;
		}
		ocap = cap_acquire (1, &capv);
		if (bind(s, (caddr_t)&sinb, sizeof (sinb)) >= 0) {
			cap_surrender (ocap);
			r = connect(s, sin, sinlen);
			if (r >= 0) {
				return s;
			}
		} else {
			cap_surrender (ocap);
		}
		if (_oserror() != EADDRINUSE) {
			(void) close(s);
			return (-1);
		}
		(void) close(s);
		lport--;

		if (lport == IPPORT_RESERVED/2) {
			_setoserror(EAGAIN);		/* close */
			return (-1);
		}
	}
}

void
doit(fromp)
	struct sockaddr_in *fromp;
{
	char cmdbuf[NCARGS+1], *cp;
	char locuser[16], remuser[16];
	uinfo_t uinfo;
	int s;
	char *hostname;
	u_short port;
	int pv[2], pid, cc;
	int nfd;
	fd_set ready, readfrom;
	char sig;
	int one = 1;
	char luser[MAX_LUSER];
#ifdef	AUX_LANG
	char *envlang, **remenvp;
#endif	/* AUX_LANG */
	int b = 0;
	cap_t ocap;
	const cap_value_t cap_setuid = CAP_SETUID;
	const cap_value_t cap_setgid = CAP_SETGID;
	const cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	const cap_value_t cap_setpcap = CAP_SETPCAP;

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
	{ int t = open(_PATH_TTY, 2);
	  if (t >= 0) {
		ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	  }
	}
#endif
	fromp->sin_port = ntohs((u_short)fromp->sin_port);
	if (fromp->sin_family != AF_INET) {
		syslog(LOG_ERR, "malformed \"from\" address (af %d)\n",
		    fromp->sin_family);
		exit(1);
	}
#ifdef IP_OPTIONS
      {
	u_char optbuf[BUFSIZ/3], *cp;
	char lbuf[BUFSIZ], *lp;
	int optsize = sizeof(optbuf), ipproto;
	struct protoent *ip;

	if ((ip = getprotobyname("ip")) != NULL)
		ipproto = ip->p_proto;
	else
		ipproto = IPPROTO_IP;
	if (getsockopt(0, ipproto, IP_OPTIONS, (char *)optbuf, &optsize) == 0 &&
	    optsize != 0) {
		lp = lbuf;
		for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
			sprintf(lp, " %2.2x", *cp);
		syslog(LOG_NOTICE,
		    "Connection received from %s using IP options (ignored):%s",
		    inet_ntoa(fromp->sin_addr), lbuf);
		if (setsockopt(0, ipproto, IP_OPTIONS,
		    (char *)NULL, optsize) != 0) {
			syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
			exit(1);
		}
	}
      }
#endif

	if (fromp->sin_port >= IPPORT_RESERVED ||
	    fromp->sin_port < IPPORT_RESERVED/2) {
		char msg[256];
		sprintf(msg, "Connection from %s on illegal port %u",
			inet_ntoa(fromp->sin_addr), fromp->sin_port);
		syslog(LOG_NOTICE|LOG_AUTH, msg);
		ocap = cap_acquire(1, &cap_audit_write);
		satvwrite(SAT_AE_IDENTITY, SAT_FAILURE, "rshd|-|?|%s", msg);
		cap_surrender(ocap);
		exit(1);
	}

	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if ((cc = read(0, &c, 1)) != 1) {
			if (cc < 0)
				syslog(LOG_NOTICE, "read: %m");
			shutdown(0, 1+1);
			exit(1);
		}
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}

	(void) alarm(0);
	if (port != 0) {
#if 1
		fromp->sin_port = htons((u_short)port);
		s = connectfromresvport(fromp, sizeof(*fromp));
		if (s < 0) {
#else
		int lport = IPPORT_RESERVED - 1;
		s = rresvport(&lport);
		if (s < 0) {
			syslog(LOG_ERR, "can't get stderr port: %m");
			exit(1);
		}
		if (port >= IPPORT_RESERVED) {
			syslog(LOG_ERR, "2nd port not reserved\n");
			exit(1);
		}
		fromp->sin_port = htons((u_short)port);
		if (connect(s, fromp, sizeof (*fromp)) < 0) {
#endif
			syslog(LOG_INFO, "connect second port %d: %m", port);
			exit(1);
		}
	} else
		s = 0;

#ifdef notdef
	/* from inetd, socket is already on 0, 1, 2 */
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
#endif
	hostname = __getverfhostent(fromp->sin_addr, check_all)->h_name;

	getstr(remuser, sizeof(remuser), "remuser", s);
	getstr(luser, sizeof(luser), "locuser", s);
	getstr(cmdbuf,  sizeof(cmdbuf),  "command", s);

#ifdef	AUX_LANG
	/*
	 * handle args to login name
	 */
	if(envlang = getenv("LANG"))
	    (void)strncpy(lang + 5, envlang, NL_LANGMAX);
	if( !(remenvp = getargs(luser)))
	    locuser[0] = 0;
	else
	    (void)strcpy(locuser, *remenvp++);
	(void)set_lang(remenvp);
#else	/* AUX_LANG */
	(void)strncpy(locuser, luser, sizeof(luser));
#endif	/* AUX_LANG */
	
	if (ia_openinfo(locuser, &uinfo)) {
		syslog(LOG_NOTICE|LOG_AUTH, 
		    "%s@%s as %s: unknown login. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		error("Login incorrect.\n");
		exit(1);
	}
	/*
	 * first try chdir as root user.
	 */
	if (chdir(uinfo->ia_dirp) < 0) {
		/*
		 * now try as the user
		 */
		setregid(-1, uinfo->ia_gid);
		setreuid(-1, uinfo->ia_uid);
		if (chdir(uinfo->ia_dirp) < 0) {
			syslog(LOG_NOTICE|LOG_AUTH,
				"%s@%s as %s: no home directory. cmd='%.80s'",
				remuser, hostname, locuser, cmdbuf);
			error("No remote directory.\n");
			exit(1);
		}
		setregid(-1, 0);
		setreuid(-1, 0);
	}

	if (uinfo->ia_pwdp != 0 && *uinfo->ia_pwdp != '\0' &&
	    ruserok(hostname, uinfo->ia_uid == 0, remuser, locuser) < 0) {
		syslog(LOG_NOTICE|LOG_AUTH, 
		    "%s@%s as %s: permission denied. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		error("Permission denied.\n");
		exit(1);
	}

	if (uinfo->ia_uid && !access(_PATH_NOLOGIN, F_OK)) {
		error("Logins currently disabled.\n");
		exit(1);
	}
	ocap = cap_acquire(1, &cap_setgid);
	if (setgid(uinfo->ia_gid) < 0) {
		cap_surrender(ocap);
		error("Bad group id.\n");
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
		static const char *Myname = "rshd";

		SH_HOOK_SETMYNAME(Myname);
		if (SH_HOOK_RSH(uinfo->ia_uid))
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
	setprid(getdfltprojuser(locuser));
	cap_surrender(ocap);

	/* audit remote shell */
	ocap = cap_acquire(1, &cap_audit_write);
	satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
	    "RSHD|+|%s|%s@%s: cmd='%.80s'",
	    locuser, remuser, hostname, cmdbuf);
	cap_surrender(ocap);
	ocap = cap_acquire(1, &cap_setgid);
	(void) initgroups(uinfo->ia_name, uinfo->ia_gid);
	cap_surrender(ocap);
	ocap = cap_acquire(1, &cap_setuid);
	if (setuid(uinfo->ia_uid) < 0) {
		cap_surrender(ocap);
		error("Bad user id.\n");
		syslog(LOG_NOTICE|LOG_AUTH, "user '%s' has invalid uid %d", 
		    uinfo->ia_name, uinfo->ia_uid);
		exit(1);
	}
	cap_surrender(ocap);
	if (log_success) {
		syslog(LOG_INFO|LOG_AUTH, "%s@%s as %s: cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
	}
	(void) write(2, "\0", 1);
	closelog();
	sent_null = 1;

	if (port) {
		if (pipe(pv) < 0) {
			error("Can't make pipe.\n");
			exit(1);
		}
		pid = fork();
		if (pid == -1)  {
			error("Can't fork; try again.\n");
			exit(1);
		}
		if (pv[0] > s)
			nfd = pv[0];
		else
			nfd = s;
		nfd++;
		if (pid) {
			(void) close(0); (void) close(1); (void) close(2);
			(void) close(pv[1]);
			FD_ZERO(&readfrom);
			FD_SET(s, &readfrom);
			FD_SET(pv[0], &readfrom);
			ioctl(pv[0], FIONBIO, (char *)&one);
			/* should set s nbio! */
			do {
				ready = readfrom;
				if (select(nfd, &ready, (fd_set *)0,
				    (fd_set *)0, (struct timeval *)0) < 0)
					break;
				if (FD_ISSET(s, &ready)) {
					if (read(s, &sig, 1) <= 0)
						FD_CLR(s, &readfrom);
					else
						killpg(pid, sig);
				}
				if (FD_ISSET(pv[0], &ready)) {
					errno = 0;
					cc = read(pv[0], rshdbuf[b], BUFSIZE);
					if (cc <= 0) {
						shutdown(s, 1+1);
						FD_CLR(pv[0], &readfrom);
					} else
						(void) write(s, rshdbuf[b], cc);
					b++;
					if (b == NBUFS) {
						b = 0;
					}
				}
			} while (FD_ISSET(s, &readfrom) ||
			    FD_ISSET(pv[0], &readfrom));
			exit(0);
		}
		setpgrp(0, getpid());
		(void) close(s); (void) close(pv[0]);
		dup2(pv[1], 2);
		close(pv[1]);
	}
	if (*uinfo->ia_shellp == '\0')
		uinfo->ia_shellp = _PATH_BSHELL;
	for (nfd = getdtablehi(); --nfd > 2; )
		(void)close(nfd);

	strncat(path, uinfo->ia_uid == 0 ? _PATH_ROOTPATH : _PATH_USERPATH,
		    sizeof(path)-5);
	strncat(logname, uinfo->ia_name, sizeof(logname)-9);
	strncat(env_hostname, hostname, sizeof(env_hostname)-12);
	strncat(env_remuser, remuser, sizeof(env_remuser)-12);
	cp = getenv("TZ");
	if (!cp)
		envinit[TZ_INDEX] = 0;
	else
		strncat(tz, cp, sizeof(tz)-4);
	environ = envinit;
	strncat(homedir, uinfo->ia_dirp, sizeof(homedir)-6);
	strncat(shell, uinfo->ia_shellp, sizeof(shell)-7);
	strncat(username, uinfo->ia_name, sizeof(username)-6);
#ifdef	AUX_LANG
	(void)setup_env(remenvp, uinfo->ia_dirp);
#endif	/* AUX_LANG */
	cp = rindex(uinfo->ia_shellp, '/');
	if (cp)
		cp++;
	else
		cp = uinfo->ia_shellp;
	if (sysconf(_SC_MAC) > 0) {
		mac_t u_mac;
		struct clearance *clp;

		/* get clearance information for this user */
		clp = sgi_getclearancebyname(uinfo->ia_name);
		if (clp == NULL)
			exit(1);

		/* mac label of current process is used for clearance check */
		u_mac = mac_get_proc();
		if (u_mac == NULL)
			exit(1);

		/* perform clearance check */
		if (mac_clearedlbl(clp, u_mac) != MAC_CLEARED) {
			mac_free(u_mac);
			exit(1);
		}
		mac_free(u_mac);
	}
	{
		cap_t u_cap;
		struct user_cap *clp;

		/* get capability range information for this user */
		clp = sgi_getcapabilitybyname(uinfo->ia_name);
		if (clp != NULL)
			u_cap = cap_from_text(clp->ca_default);
		else
			u_cap = cap_init();
		if (u_cap == NULL)
			exit(1);

		/* set our capability set to the default for this user */
		ocap = cap_acquire(1, &cap_setpcap);
		if (cap_set_proc(u_cap) == -1) {
			cap_surrender(ocap);
			cap_free(u_cap);
			exit(1);
		}
		cap_free(ocap);
		cap_free(u_cap);
	}
	execl(uinfo->ia_shellp, cp, "-c", cmdbuf, 0);
	perror(uinfo->ia_shellp);
	exit(1);
}

/*
 * Report error to client.
 * Note: can't be used until second socket has connected
 * to client, or older clients will hang waiting
 * for that connection first.
 */
void
error(const char *fmt, ...)
{
	va_list args;
	char buf[BUFSIZ], *bp = buf;

	va_start(args, fmt);
	if (sent_null == 0)
		*bp++ = 1;
	(void) vsprintf(bp, fmt, args);
	(void) write(2, buf, strlen(buf));
	va_end(args);
}

void
getstr(
	char *buf,	/* buffer to receive string */
	int cnt,	/* maximum string length */
	char *err,	/* string name string */
	int sfd)	/* second socket fd, zero if none */
{
	char c;
	fd_set reads;

	if (sfd != 0) {		/* two ports to track */
		FD_ZERO(&reads);
		FD_SET(0, &reads);
		FD_SET(sfd, &reads);
		if (select(sfd + 1, &reads, 0,0,0) < 1 || 
		    FD_ISSET(sfd, &reads)) {
			/* 
			 * Detect loss of the second connection 
			 * instead of hanging.
			 */
			syslog(LOG_INFO, "select: second connection failure.");
			exit(1);
		}
	}

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
