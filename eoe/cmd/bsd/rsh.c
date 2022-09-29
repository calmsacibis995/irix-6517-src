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
static char sccsid[] = "@(#)rsh.c	5.7 (Berkeley) 9/20/88";
#endif /* not lint */

#ifdef sgi
#define _BSD_SIGNALS
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <netinet/in.h>

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>
#include <netdb.h>

#ifdef sgi
#include <getopt.h>
#endif

#ifdef sgi
#include <strings.h>
#include <malloc.h>
#define _PATH_RLOGIN	"/usr/bsd/rlogin"	/* look for rlogin(1) here */
#endif /* sgi */

/*
 * rsh - remote shell
 */
/* VARARGS */
int	error();
#ifndef sgi
char	*index(), *rindex(), *malloc(), *getpass(), *strcpy();

struct	passwd *getpwuid();
#endif

#ifdef sgi
extern	int	errno;
#else
int	errno;
#endif
int	options;
int	rfd2;
int	nflag;
#ifdef sgi
void	sendsig();
#else
int	sendsig();
#endif
#ifdef sgi
extern int rcmd();
int (*rcmdp)() = rcmd;
#endif
#ifdef AFS
extern int (*__afs_getsym())();
#endif

#define	mask(s)	(1 << ((s) - 1))

main(argc, argv0)
	int argc;
	char **argv0;
{
	int rem, pid;
#ifdef sgi
#define	NBUFS	5
#define BUFSIZE	(64 * 1024)
	char *buf[NBUFS];
	char *host, *cp, **ap, *args, **argv = argv0, *user = 0;
	int argoff = 0, ch = 0;
	register char *p;
	int i, b = 0;
#else
	char *host, *cp, **ap, buf[BUFSIZ], *args, **argv = argv0, *user = 0;
#endif
	register int cc;
	int asrsh = 0;
	struct passwd *pwd;
	int readfrom, ready;
	int one = 1;
	struct servent *sp;
	int port;
	int omask;
	int had_error = 0;

#ifdef sgi
	/*
	 * use memalign to set up page-aligned buffer
	 */
	for (i = 0; i < NBUFS; i++) {
		buf[i] = memalign(16*1024, BUFSIZE);
		if (!buf[i]) {
			perror("rsh");
			exit(1);
		}
	}

	/* work around sh bug which leaves lots of open files */
	{ int maxfd = getdtablehi();
	for (cc = 3; cc < maxfd; cc++)
		(void)close(cc);
	}

	nflag = options = 0;

	/* if called as something other than "rsh", use it as the host name */
	if (p = rindex(argv[0], '/'))
		++p;
	else
		p = argv[0];
	if (strcmp(p, "rsh"))
		host = p;
	else {
		asrsh = 1;
		host = NULL;
	}

	/* handle "rsh host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}
#else
	host = rindex(argv[0], '/');
	if (host)
		host++;
	else
		host = argv[0];
	argv++, --argc;
	if (!strcmp(host, "rsh")) {
		host = *argv++, --argc;
		asrsh = 1;
	}
#endif

#ifdef sgi
#define	OPTIONS	"8Ldel:nw"
	while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != EOF)
		switch(ch) {
		case 'L':	/* -8Lew are ignored to allow rlogin aliases */
		case 'e':
		case '8':
			break;
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'l':
			user = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case '?':
		default:
			goto usage;
		}
	optind += argoff;
#else	/* !sgi */
another:
	if (argc > 0 && !strcmp(*argv, "-l")) {
		argv++, argc--;
		if (argc > 0)
			user = *argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-n")) {
		argv++, argc--;
		nflag++;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-d")) {
		argv++, argc--;
		options |= SO_DEBUG;
		goto another;
	}
	/*
	 * Ignore the -L, -w, -e and -8 flags to allow aliases with rlogin
	 * to work
	 *
	 * There must be a better way to do this! -jmb
	 */
	if (argc > 0 && !strncmp(*argv, "-L", 2)) {
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strncmp(*argv, "-w", 2)) {
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strncmp(*argv, "-e", 2)) {
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strncmp(*argv, "-8", 2)) {
		argv++, argc--;
		goto another;
	}
#endif	/* sgi */
#ifdef sgi
	/* if haven't gotten a host yet, do so */
	if (!host && !(host = argv[optind++]))
		goto usage;
#else
	if (host == 0)
		goto usage;
#endif
	
#ifdef sgi
	/* if no further arguments, must have been called as rlogin. */
	if (!argv[optind]) {
		if (asrsh)
			*argv0 = "rlogin";
		execv(_PATH_RLOGIN, argv0);
		perror(_PATH_RLOGIN);
		exit(1);
	}

	argc -= optind;
	argv += optind;
#else	/* !sgi */
	if (argv[0] == 0) {
		if (asrsh)
			*argv0 = "rlogin";
		execv("/usr/ucb/rlogin", argv0);
		perror("/usr/ucb/rlogin");
		exit(1);
	}
#endif
#ifdef sgi
	/* Accept user1@host format, though "-l user2" overrides user1 */
	cp = strchr(host, '@');
	if (cp) {
		*cp = '\0';
		if (user == NULL && cp > host)
			user = host;
		host = cp + 1;
		if (*host == '\0')
			goto usage;
	}
#endif
	pwd = getpwuid(getuid());
	if (pwd == 0) {
		fprintf(stderr, "who are you?\n");
		exit(1);
	}
#ifdef AFS
	if ((rcmdp = __afs_getsym("rcmd")) == NULL)
		rcmdp = rcmd;
#endif
	cc = 0;
	for (ap = argv; *ap; ap++)
		cc += strlen(*ap) + 1;
	cp = args = malloc(cc);
	for (ap = argv; *ap; ap++) {
		(void) strcpy(cp, *ap);
		while (*cp)
			cp++;
		if (ap[1])
			*cp++ = ' ';
	}
#ifdef sgi
	port = 514;
#else
	sp = getservbyname("shell", "tcp");
	if (sp == 0) {
		fprintf(stderr, "rsh: shell/tcp: unknown service\n");
		exit(1);
	}
	port = sp->s_port;
#endif
        rem = (*rcmdp)(&host, port, pwd->pw_name,
	    user ? user : pwd->pw_name, args, &rfd2);
        if (rem < 0) {
		fprintf(stderr, "rsh: connection failed\n");
                exit(1);
	}
	if (rfd2 < 0) {
		fprintf(stderr, "rsh: can't establish stderr\n");
		exit(2);
	}
	if (options & SO_DEBUG) {
		if (setsockopt(rem, SOL_SOCKET, SO_DEBUG, &one, sizeof (one)) < 0)
			perror("setsockopt (stdin)");
		if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG, &one, sizeof (one)) < 0)
			perror("setsockopt (stderr)");
	}
	(void) setuid(getuid());
	omask = sigblock(mask(SIGINT)|mask(SIGQUIT)|mask(SIGTERM));
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, sendsig);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		signal(SIGQUIT, sendsig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, sendsig);
	if (nflag == 0) {
		pid = fork();
		if (pid < 0) {
			perror("fork");
			exit(1);
		}
	}
	ioctl(rfd2, FIONBIO, &one);
	ioctl(rem, FIONBIO, &one);
        if (nflag == 0 && pid == 0) {
		char *bp; int rembits, wc;
		(void) close(rfd2);
	reread:
		errno = 0;
		cc = read(0, buf[b], BUFSIZE);
#ifdef sgi
		if (cc <= 0) {
			if (cc == 0)
				goto done;
			if (errno == EINTR || errno == EAGAIN)
				goto reread;
			perror("read");
			exit(1);
		}
		bp = buf[b];
#else
		if (cc <= 0)
			goto done;
		bp = buf;
#endif
	rewrite:
		rembits = 1<<rem;
		if (select(16, 0, &rembits, 0, 0) < 0) {
			if (errno != EINTR) {
				perror("select");
				exit(1);
			}
			goto rewrite;
		}
		if ((rembits & (1<<rem)) == 0)
			goto rewrite;
		wc = write(rem, bp, cc);
		if (wc < 0) {
			if (errno == EWOULDBLOCK)
				goto rewrite;
#ifdef sgi
			if (errno == EINTR || errno == EAGAIN)
				goto rewrite;
#endif /* sgi */
			perror("write");
			exit(1);
		}
		cc -= wc; bp += wc;
		if (cc == 0) {
			b++;
			if (b == NBUFS) {
				b = 0;
			}
			goto reread;
		}
		goto rewrite;
	done:
		(void) shutdown(rem, 1);
		exit(0);
	}
	sigsetmask(omask);
	readfrom = (1<<rfd2) | (1<<rem);
	b = 0;
	do {
		ready = readfrom;
		if (select(16, &ready, 0, 0, 0) < 0) {
			if (errno != EINTR) {
				perror("select");
				exit(1);
			}
			continue;
		}
		if (ready & (1<<rfd2)) {
			errno = 0;
			cc = read(rfd2, buf[b], BUFSIZE);
			if (cc <= 0) {
#ifdef sgi
				if (errno != EWOULDBLOCK && errno != EAGAIN
				    && errno != EINTR) {
					if (cc < 0) {
						perror("read\n");
						had_error = 1;
					}
					readfrom &= ~(1<<rfd2);
					shutdown(rfd2,0);
				}
#else
				if (errno != EWOULDBLOCK)
					readfrom &= ~(1<<rfd2);
#endif
			} else {
				(void) write(2, buf[b], cc);
				b++;
				if (b == NBUFS) {
					b = 0;
				}
			}
		}
		if (ready & (1<<rem)) {
			errno = 0;
			cc = read(rem, buf[b], BUFSIZE);
			if (cc <= 0) {
#ifdef sgi
				if (errno != EWOULDBLOCK && errno != EAGAIN
				    && errno != EINTR) {
					if (cc < 0) {
						perror("read\n");
						had_error = 1;
					}
					readfrom &= ~(1<<rem);
					shutdown(rem,0);
				}
#else
				if (errno != EWOULDBLOCK)
					readfrom &= ~(1<<rem);
#endif
			} else {
				(void) write(1, buf[b], cc);
				b++;
				if (b == NBUFS) {
					b = 0;
				}
			}
		}
        } while (readfrom);
	if (nflag == 0)
		(void) kill(pid, SIGKILL);
	if (had_error)
		exit(1);
	exit(0);
usage:
#ifdef sgi
	fprintf(stderr,
	    "usage: rsh [ -l username ] [ -n ] host command\n");
	fprintf(stderr,
	    "usage: rsh host [ -l username ] [ -n ] command\n");
	fprintf(stderr,
	    "       rsh [ -n ] username@host command\n");
	fprintf(stderr,
	    "       rsh username@host [ -n ] command\n");
#else
	fprintf(stderr,
	    "usage: rsh host [ -l login ] [ -n ] command\n");
#endif
	exit(1);
}

#ifdef sgi
void
#endif
sendsig(signo)
	char signo;
{

	(void) write(rfd2, &signo, 1);
}
