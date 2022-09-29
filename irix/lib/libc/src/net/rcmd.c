/*
 * Copyright (c) 1983 Regents of the University of California.
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

#ifdef __STDC__
	#pragma weak rcmd      = _rcmd
	#pragma weak rresvport = _rresvport
	#pragma weak ruserok   = _ruserok
#endif
#include "synonyms.h"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rcmd.c	5.20 (Berkeley) 1/24/89";
#endif /* LIBC_SCCS and not lint */

#define _BSD_SIGNALS

#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#undef signal			/* redefined to BSDsignal in signal.h */
#undef sigpause			/* redefined to BSDsigpause in signal.h */
#include <sys/param.h>		/* #include signal.h */
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>		/* prototypes several system calls */
#include <fcntl.h>		/* prototype for fcntl() */
#include <bstring.h>		/* prototype for bcopy() */
#include <stdlib.h>
#include <sys/time.h>

#include <netdb.h>
#include <errno.h>
#include <unistd.h>	/* for sysconf() */
#include <sys/capability.h>

/* forward references */
int rresvport(int *);
int _validuser(FILE *, char *, char *, char *, int);
static int _checkhost(char *, char *, int);
static char *topdomain(char *);
static int local_domain(char *);

int
rcmd(char **ahost, 
	u_short rport, 
	char *locuser, 
	char *remuser, 
	char *cmd, 
	int *fd2p)
{
	int s, timo = 1, pid, retval;
	int oldmask;
	struct sockaddr_in sin, from;
	char c;
	int lport = IPPORT_RESERVED - 1;
	struct hostent *hp;
	fd_set reads;

	pid = getpid();
	hp = gethostbyname(*ahost);
	if (hp == 0) {
		herror(*ahost);
		return (-1);
	}
	*ahost = hp->h_name;
	oldmask = sigblock(sigmask(SIGURG));
	for (;;) {
		s = rresvport(&lport);
		if (s < 0) {
			if (_oserror() == EAGAIN)
				fprintf(stderr, "socket: All ports in use\n");
			else
				perror("rcmd: socket");
			sigsetmask(oldmask);
			return (-1);
		}
		fcntl(s, F_SETOWN, pid);
		sin.sin_family = (short)hp->h_addrtype;
		bcopy(hp->h_addr_list[0], (caddr_t)&sin.sin_addr, hp->h_length);
		sin.sin_port = rport;
		if (connect(s, (caddr_t)&sin, sizeof (sin)) >= 0)
			break;
		(void) close(s);
		if (_oserror() == EADDRINUSE) {
			lport--;
			continue;
		}
		if (_oserror() == ECONNREFUSED && timo <= 16) {
			sleep((unsigned int)timo);
			timo *= 2;
			continue;
		}
		if (hp->h_addr_list[1] != NULL) {
			int oerrno = _oserror();

			fprintf(stderr,
			    "connect to address %s: ", inet_ntoa(sin.sin_addr));
			_setoserror(oerrno);
			perror(0);
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0], (caddr_t)&sin.sin_addr,
			    hp->h_length);
			fprintf(stderr, "Trying %s...\n",
				inet_ntoa(sin.sin_addr));
			continue;
		}
		perror(hp->h_name);
		sigsetmask(oldmask);
		return (-1);
	}
	lport--;
	if (fd2p == 0) {
		write(s, "", 1);
		lport = 0;
	} else {
		char num[8];
		int s2 = rresvport(&lport), s3;
		int len = sizeof (from);

		if (s2 < 0)
			goto bad;
		listen(s2, 1);
		(void) sprintf(num, "%d", lport);
		if (write(s, num, strlen(num)+1) != (int)strlen(num)+1) {
			perror("write: setting up stderr");
			(void) close(s2);
			goto bad;
		}
		FD_ZERO(&reads);
		FD_SET(s, &reads);
		FD_SET(s2, &reads);
		_setoserror(0);/* XXX bad thing to do */
		if (select(MAX(s, s2) + 1, &reads, 0, 0, 0) < 1 ||
		    !FD_ISSET(s2, &reads)) {
			if (_oserror() != 0)
				perror("select: setting up stderr");
			else
			    fprintf(stderr,
				"select: protocol failure in circuit setup.\n");
			(void) close(s2);
			goto bad;
		}
		s3 = accept(s2, &from, &len);
		(void) close(s2);
		if (s3 < 0) {
			perror("accept");
			lport = 0;
			goto bad;
		}
		*fd2p = s3;
		from.sin_port = ntohs((u_short)from.sin_port);
		if (from.sin_family != AF_INET ||
		    from.sin_port >= IPPORT_RESERVED ||
		    from.sin_port < IPPORT_RESERVED / 2) {
			fprintf(stderr,
			    "socket: protocol failure in circuit setup.\n");
			goto bad2;
		}
	}
	(void) write(s, locuser, strlen(locuser)+1);
	(void) write(s, remuser, strlen(remuser)+1);
	(void) write(s, cmd, strlen(cmd)+1);
	retval = (int)read(s, &c, 1);
	if (retval != 1) {
		if (retval == 0) {
		    fprintf(stderr,
		      "Protocol error, %s closed connection\n", *ahost);
		} else if (retval < 0) {
		    perror(*ahost);
		} else {
		    fprintf(stderr,
		      "Protocol error, %s sent %d bytes\n", *ahost, retval);
		}
		goto bad2;
	}
	if (c != 0) {
		while (read(s, &c, 1) == 1) {
			(void) write(2, &c, 1);
			if (c == '\n')
				break;
		}
		goto bad2;
	}
	sigsetmask(oldmask);
	return (s);
bad2:
	if (lport)
		(void) close(*fd2p);
bad:
	(void) close(s);
	sigsetmask(oldmask);
	return (-1);
}

int
rresvport(int *alport)
{
	struct sockaddr_in sin;
	int s;
	cap_t ocap;
	cap_value_t capv = CAP_PRIV_PORT;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return (-1);
	ocap = cap_acquire (1, &capv);
	for (;;) {
		sin.sin_port = htons((u_short)*alport);
		if (bind(s, (caddr_t)&sin, sizeof (sin)) >= 0) {
			cap_surrender (ocap);
			return (s);
		}
		if (_oserror() != EADDRINUSE) {
			cap_surrender (ocap);
			(void) close(s);
			return (-1);
		}
		(*alport)--;
		if (*alport == IPPORT_RESERVED/2) {
			cap_surrender (ocap);
			(void) close(s);
			_setoserror(EAGAIN);		/* close */
			return (-1);
		}
	}
}

int	_check_rhosts_file = 1;

int
ruserok(char *rhost, 
	int superuser, 
	char *ruser, 
	char *luser)
{
	FILE *hostf;
	char *sp;
	int baselen = -1;
	int ok;

	struct stat sbuf;
	struct passwd *pwd;
	char pbuf[MAXPATHLEN];

	cap_t ocap;
	cap_value_t capv = CAP_SETUID;

	if ((sp = index(rhost, '.')) != NULL)
		baselen = (int)(sp - rhost);

	/* check /etc/hosts.equiv */
	if (!superuser) {
		if ((hostf = fopen("/etc/hosts.equiv", "r")) != NULL) {
			if (!_validuser(hostf, rhost, luser, ruser, baselen)) {
				(void) fclose(hostf);
				return(0);
		        }
			(void) fclose(hostf);
		}
	}

	/* check ~/.rhosts */

	ok = -1;
	if (_check_rhosts_file || superuser) {
		extern int _getpwent_no_shadow;
		uid_t ruid, euid;
		gid_t rgid, egid;
		int oshadow = _getpwent_no_shadow;

		_getpwent_no_shadow = 1;
		if ((pwd = getpwnam(luser)) == NULL) {
			_getpwent_no_shadow = oshadow;
			return(-1);
		}
		_getpwent_no_shadow = oshadow;
		(void)strcpy(pbuf, pwd->pw_dir);
		(void)strcat(pbuf, "/.rhosts");

		/* 
		 * If root, read .rhosts as the local user to avoid NFS
		 * mapping the root uid to something that can't read .rhosts.
		 */
		if ((euid = geteuid()) == 0) {
			ruid = getuid();
			rgid = getgid();
			egid = getegid();
			ocap = cap_acquire (1, &capv);
			if (setregid(0, pwd->pw_gid) < 0)
				euid = -1;
			if (setreuid(0, pwd->pw_uid) < 0)
				euid = -1;
			cap_surrender (ocap);
		}
		if ((hostf = fopen(pbuf, "r")) != NULL) {
			/*
			 * If owned by someone other than user or root or if
			 * writable by anyone but the owner, don't use it.
			 *
			 * Assume _validuser returns 0 or -1.
			 */
			if (fstat(fileno(hostf), &sbuf) >= 0
			    && (sbuf.st_uid == 0 || sbuf.st_uid == pwd->pw_uid)
			    && ((sbuf.st_mode & (S_IWGRP|S_IWOTH)) == 0)) {
				ok = _validuser(hostf, rhost, luser, ruser, 
						baselen);
			}
			(void) fclose(hostf);
		}
		if (euid == 0) {
			ocap = cap_acquire (1, &capv);
			(void) setreuid(0, 0);
			(void) setregid(rgid, egid);
			if (ruid)
				(void) setreuid(ruid, 0);
			cap_surrender (ocap);
		}
	}
	return (ok);
}

/*
 * alternate ruserok - except that callers already have passwd info so no need
 * to look it up again. Used for example by login/su to avoid multiple
 * passwd scans
 */
int
__ruserok_x(
	char *rhost,
	int superuser,
	char *ruser,
	char *luser,
	uid_t id,
	char *dir)
{
	FILE *hostf;
	char *sp;
	int baselen = -1;
	int ok;

	struct stat sbuf;
	char pbuf[MAXPATHLEN];

	cap_t ocap;
	cap_value_t capv = CAP_SETUID;

	if ((sp = index(rhost, '.')) != NULL)
		baselen = (int)(sp - rhost);

	/* check /etc/hosts.equiv */
	if (!superuser) {
		if ((hostf = fopen("/etc/hosts.equiv", "r")) != NULL) {
			if (!_validuser(hostf, rhost, luser, ruser, baselen)) {
				(void) fclose(hostf);
				return(0);
		        }
			(void) fclose(hostf);
		}
	}

	/* check ~/.rhosts */

	ok = -1;
	if (_check_rhosts_file || superuser) {
		uid_t ruid, euid;

		(void)strcpy(pbuf, dir);
		(void)strcat(pbuf, "/.rhosts");

		/* 
		 * If root, read .rhosts as the local user to avoid NFS
		 * mapping the root uid to something that can't read .rhosts.
		 */
		if ((euid = geteuid()) == 0) {
			ruid = getuid();
			ocap = cap_acquire (1, &capv);
			if (setreuid(0, id) < 0)
				euid = -1;
			cap_surrender (ocap);
		}
		if ((hostf = fopen(pbuf, "r")) != NULL) {
			/*
			 * If owned by someone other than user or root or if
			 * writable by anyone but the owner, don't use it.
			 *
			 * Assume _validuser returns 0 or -1.
			 */
			if (fstat(fileno(hostf), &sbuf) >= 0
			    && (sbuf.st_uid == 0 || sbuf.st_uid == id)
			    && ((sbuf.st_mode & (S_IWGRP|S_IWOTH)) == 0)) {
				ok = _validuser(hostf, rhost, luser, ruser, 
						baselen);
			}
			(void) fclose(hostf);
		}
		if (euid == 0) {
			ocap = cap_acquire (1, &capv);
			(void) setreuid(0, 0);
			if (ruid)
				(void) setreuid(ruid, 0);
			cap_surrender (ocap);
		}
	}
	return (ok);
}

/* 
 * Returns 0 if user is ok, else returns -1.
 * N.B. these return values are used by ruserok().
 *
 * Don't make static, used by lpd(8) 
 */
int
_validuser(FILE *hostf, 
	char *rhost, 
	char *luser, 
	char *ruser, 
	int baselen)
{
	char *user;
	char ahost[MAXHOSTNAMELEN];
	register char *p;
	int hostmatch, usermatch;
	char domain[256];		/* XXX maybe split _yellowup */

	if (getdomainname(domain, sizeof(domain)) < 0) {
		domain[0] = '\0';       /* what else can we do? */
	}

	while (fgets(ahost, sizeof (ahost), hostf)) {
		p = ahost;
		while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0') {
			p++;
		}
		if (*p == ' ' || *p == '\t') {
			*p++ = '\0';
			while (*p == ' ' || *p == '\t')
				p++;
			user = p;
			while (*p != '\n' && *p != ' ' && *p != '\t' && *p != '\0')
				p++;
		} else
			user = p;
		*p = '\0';

		/*
		 *  +        - anything goes
		 *  -<name>  - host/user <name> disallowed
		 *  +@<name> - group <name> allowed
		 *  -@<name> - group <name> disallowed
		 */
		if (ahost[0] == '+' && ahost[1] == 0) {
			hostmatch = 1;
		} else if (ahost[0] == '+' && ahost[1] == '@') {
			hostmatch = innetgr(ahost + 2, rhost,
			    NULL, domain);
		} else if (ahost[0] == '-' && ahost[1] == '@') {
			if (innetgr(ahost + 2, rhost, NULL, domain))
				break;
			hostmatch = 0;
		} else if (ahost[0] == '-') {
			if (_checkhost(rhost, ahost+1, baselen))
				break;
			hostmatch = 0;
		} else
			hostmatch = _checkhost(rhost, ahost, baselen);
		/* 
		 * Second field ignored on trusted systems
		 */		
		if (user[0] && sysconf(_SC_MAC) != 1 &&
		    sysconf(_SC_IP_SECOPTS) != 1) {
			if (user[0] == '+' && user[1] == 0) {
				usermatch = 1;
			} else if (user[0] == '+' && user[1] == '@') {
				usermatch = innetgr(user+2, NULL,
				    ruser, domain);
			} else if (user[0] == '-' && user[1] == '@') {
				if (hostmatch && innetgr(user+2, NULL,
				    ruser, domain))
					break;
				usermatch = 0;
			} else if (user[0] == '-') {
				if (hostmatch && !strcmp(user+1, ruser))
					break;
				usermatch = 0;
			} else
				usermatch = !strcmp(user, ruser);
		} else
			usermatch = !strcmp(ruser, luser);
		if (hostmatch && usermatch)
			return (0);
	}
	return (-1);
}

static int
_checkhost(char *rhost, char *lhost, int len)
{
#ifdef _LIBC_NONSHARED
	static char ldomain[MAXHOSTNAMELEN + 1];
	static char *domainp;
	static int nodomain;
#else
	static char *ldomain = NULL;
	static char *domainp = NULL;
	static int nodomain = 0;
#endif

	if (len == -1)
		return(!strcasecmp(rhost, lhost));
	if (strncasecmp(rhost, lhost, (size_t)len))
		return(0);
	if (!strcasecmp(rhost, lhost))
		return(1);
	if (*(lhost + len) != '\0')
		return(0);
	if (nodomain)
		return(0);
#ifndef _LIBC_NONSHARED
	if ((ldomain == NULL) &&
	    ((ldomain = (char *)calloc(1, MAXHOSTNAMELEN + 1)) == NULL))
		return(0);
#endif
	if (!domainp) {
		if (gethostname(ldomain, MAXHOSTNAMELEN + 1) == -1) {
			nodomain = 1;
			return(0);
		}
		ldomain[MAXHOSTNAMELEN] = NULL;
		if ((domainp = index(ldomain, '.')) == (char *)NULL) {
			nodomain = 1;
			return(0);
		}
		domainp++;
	}
	return(!strcasecmp(domainp, rhost + len +1));
}

/* 
 * Routines to fix a security hole in rshd and rlogind. Derived from 
 * "@(#)rshd.c    5.17.1.3 (Berkeley) 9/11/89" and
 * "@(#)rlogind.c 5.22.1.7 (Berkeley) 9/11/89"
 */

/*
 * Check whether host h is in our local domain,
 * defined as sharing the last two components of the domain part,
 * or the entire domain part if the local domain has only one component.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */

static char *
topdomain(char *h)
{
	register char *p;
	char *maybe = NULL;
	int dots = 0;

	for (p = h + strlen(h); p >= h; p--) {
		if (*p == '.') {
			if (++dots == 2)
				return (p);
			maybe = p;
		}
	}
	return (maybe);
}

static int
local_domain(char *h)
{
	char localhost[MAXHOSTNAMELEN];
	char *p1, *p2;

	localhost[0] = 0;
	(void) gethostname(localhost, sizeof(localhost));
	p1 = topdomain(localhost);
	p2 = topdomain(h);
	if (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2))
		return(1);
	return(0);
}

/*
 * Attempt to verify that we haven't been fooled by someone
 * in a remote net; look up the name corresponding to the given address
 * and check that the address is in the list of addresses for the name. 
 * Returns the hostent if verified, else returns a "dummy" hostent with 
 * the name pointing to the address in dotted-decimal notation.
 */

#include <syslog.h>
#include <arpa/nameser.h>
#include <resolv.h>

struct hostent *
__getverfhostent(struct in_addr addr, int check_all)
{
	register struct hostent *hp;
	char remotehost[2 * MAXHOSTNAMELEN + 1];
	static char *aliases = NULL;
	static struct in_addr uaddr = {0};
	static char *addrs[2] = { (char *) &uaddr, NULL };
	static struct hostent unknown = { 
		NULL, &aliases, AF_INET, sizeof(struct in_addr), addrs,
	};

	hp = gethostbyaddr(&addr, sizeof(addr), AF_INET);
	if (hp) {
	    if (check_all || local_domain(hp->h_name)) {
		u_long dnsearch = _res.options & RES_DNSRCH;
		strncpy(remotehost, hp->h_name, sizeof(remotehost) - 1);
		remotehost[sizeof(remotehost) - 1] = '\0';

		/* 
		 * Gethostbyaddr returns fully-qualified name, hence don't
		 * need the domain search.
		 *
		 * N.B. if interrupted & longjmp, DNSRCH will not be 
		 * restored (if set).
		 */
		_res.options &= ~RES_DNSRCH;
		hp = gethostbyname(remotehost);
		_res.options |= dnsearch;

		if (!hp) {
		    syslog(LOG_NOTICE|LOG_AUTH, "Can't find hostname for %s",
				inet_ntoa(addr));
		} else {
		    for (; hp->h_addr_list[0]; hp->h_addr_list++) {
			if (!bcmp(hp->h_addr_list[0], (caddr_t)&addr,
			    sizeof(addr))) {
				return hp;
			}
		    }
		    syslog(LOG_NOTICE|LOG_AUTH, 
				"Host address %s not listed for host %s",
				inet_ntoa(addr),
				hp->h_name);
		}
	    } else {
		return hp;
	    }
	}
	unknown.h_name = inet_ntoa(addr);
	uaddr = addr;
	return &unknown;
}
