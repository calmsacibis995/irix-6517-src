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
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rcp.c	5.21 (Berkeley) 7/17/89";
#endif /* not lint */

#define vfork fork
#define _BSD_SIGNALS

/*
 * rcp
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <pwd.h>
#include <netdb.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <paths.h>

/*
 * rsh lives in /usr/bsd but we can't use absolute path names
 * if we want interoperability with non-sgi systems.
 */
#define _PATH_RSH	"rsh"
#define _PATH_CP	"/bin/cp"

extern int errno;
struct passwd *pwd;
uid_t userid;
int errs, pflag, port, rem;
/*
 * use off64_t, *stat64 and (non-bsd) readdir64 for large files and filesystems
 */
#define stat	stat64
#define fstat	fstat64
#define readdir	readdir64
#define off_t	off64_t
int iamremote, iamrecursive, targetshouldbedirectory;
extern int rcmd();
int (*rcmdp)() = rcmd;
#ifdef AFS
extern int (*__afs_getsym())();
#endif

#define	CMDNEEDS	20
char cmd[CMDNEEDS];		/* must hold "rcp -r -p -d\0" */


int verbose;
/*
 * need to dynamically allocate net_buf to page-align it
 */
#define NBUFS	3
char *net_buf[NBUFS];
#define NETBUFSIZE	(64 * 1024)
#define NETBUFALIGN	(16 * 1024)

static void nospace(void);
static void usage(void);
static void toremote(char *, int, char **);
static void safe_setuid(int);
static void safe_setreuid(int, int);
static void tolocal(int, char **);
static void verifydir(char *);
static void source(int, char **);
static void rsource(char *, struct stat *);
static void sink(int, char **);
static void error(char *, ...);



main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	struct servent *sp;
	int ch, fflag, tflag;
	char *targ, *colon();
	void lostconn();
	int i;

	/*
	 * use memalign to set up page-aligned buffer
	 */
	for (i = 0; i < NBUFS; i++) {
		if ((net_buf[i] = memalign(NETBUFALIGN, NETBUFSIZE)) == NULL) {
			perror("rcp");
			exit(1);
		}
	}

	port = 514;

	if (!(pwd = getpwuid(userid = getuid()))) {
		(void)fprintf(stderr, "rcp: unknown user %d.\n", userid);
		exit(1);
	}

	fflag = tflag = 0;
	while ((ch = getopt(argc, argv, "dfprtv")) != EOF)
		switch(ch) {
		case 'd':
			targetshouldbedirectory = 1;
			break;
		case 'f':			/* "from" */
			fflag = 1;
			break;
		case 'p':			/* preserve access/mod times */
			++pflag;
			break;
		case 'r':
			++iamrecursive;
			break;
		case 't':			/* "to" */
			tflag = 1;
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (fflag) {
		iamremote = 1;
		(void)response();
		safe_setuid(userid);
		source(argc, argv);
		exit(errs);
	}

	if (tflag) {
		iamremote = 1;
		safe_setuid(userid);
		sink(argc, argv);
		exit(errs);
	}

	if (argc < 2)
		usage();
	if (argc > 2)
		targetshouldbedirectory = 1;

#ifdef AFS
	if ((rcmdp = __afs_getsym("rcmd")) == NULL)
		rcmdp = rcmd;
#endif
	rem = -1;
	(void)sprintf(cmd, "rcp%s%s%s", iamrecursive ? " -r" : "",
	    pflag ? " -p" : "", targetshouldbedirectory ? " -d" : "");

	(void)signal(SIGPIPE, lostconn);

	if (targ = colon(argv[argc - 1]))
		toremote(targ, argc, argv);
	else {
		tolocal(argc, argv);
		if (targetshouldbedirectory)
			verifydir(argv[argc - 1]);
	}
	exit(errs);
}

static void
toremote(char *targ, int argc, char **argv)
{
	int i;
	char *bp, *host, *src, *suser, *thost, *tuser;
	char *colon();

	*targ++ = 0;
	if (*targ == 0)
		targ = ".";

	if (thost = index(argv[argc - 1], '@')) {
		/* user@host */
		*thost++ = 0;
		tuser = argv[argc - 1];
		if (*tuser == '\0')
			tuser = NULL;
		else if (!okname(tuser))
			exit(1);
	} else {
		thost = argv[argc - 1];
		tuser = NULL;
	}

	for (i = 0; i < argc - 1; i++) {
		src = colon(argv[i]);
		if (src) {			/* remote to remote */
			*src++ = 0;
			if (*src == 0)
				src = ".";
			host = index(argv[i], '@');
			if (!(bp = malloc((u_int)(strlen(_PATH_RSH) +
				    strlen(argv[i]) + strlen(src) +
				    (tuser? strlen(tuser) : 0) + strlen(thost) +
				    strlen(targ)) + CMDNEEDS + 20)))
					nospace();
			if (host) {
				*host++ = 0;
				suser = argv[i];
				if (*suser == '\0')
					suser = pwd->pw_name;
				else if (!okname(suser))
					continue;
				(void)sprintf(bp,
				    "%s %s -l %s -n %s %s '%s%s%s:%s'",
				    _PATH_RSH, host, suser, cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			} else
				(void)sprintf(bp, "%s %s -n %s %s '%s%s%s:%s'",
				    _PATH_RSH, argv[i], cmd, src,
				    tuser ? tuser : "", tuser ? "@" : "",
				    thost, targ);
			(void)susystem(bp);
			(void)free(bp);
		} else {			/* local to remote */
			if (rem == -1) {
				if (!(bp = malloc((u_int)strlen(targ) +
				    CMDNEEDS + 20)))
					nospace();
				(void)sprintf(bp, "%s -t %s", cmd, targ);
				host = thost;
					rem = (*rcmdp)(&host, port, pwd->pw_name,
					    tuser ? tuser : pwd->pw_name,
					    bp, 0);
				if (rem < 0)
					exit(1);
				if (response() < 0)
					exit(1);
				(void)free(bp);
				safe_setuid(userid);
			}
			source(1, argv+i);
		}
	}
}

static void
tolocal(int argc, char **argv)
{
	int i;
	char *bp, *host, *src, *suser;
	char *colon();

	for (i = 0; i < argc - 1; i++) {
		if (!(src = colon(argv[i]))) {	/* local to local */
			if (!(bp = malloc((u_int)(strlen(_PATH_CP) +
			    strlen(argv[i]) + strlen(argv[argc - 1])) + 20)))
				nospace();
			(void)sprintf(bp, "%s%s%s %s %s", _PATH_CP,
			    iamrecursive ? " -r" : "", pflag ? " -p" : "",
			    argv[i], argv[argc - 1]);
			if (susystem(bp))
				errs++;
			(void)free(bp);
			continue;
		}
		*src++ = 0;
		if (*src == 0)
			src = ".";
		host = index(argv[i], '@');
		if (host) {
			*host++ = 0;
			suser = argv[i];
			if (*suser == '\0')
				suser = pwd->pw_name;
			else if (!okname(suser))
				continue;
		} else {
			host = argv[i];
			suser = pwd->pw_name;
		}
		if (!(bp = malloc((u_int)(strlen(src)) + CMDNEEDS + 20)))
			nospace();
		(void)sprintf(bp, "%s -f %s", cmd, src);
			rem = (*rcmdp)(&host, port, pwd->pw_name, suser, bp, 0);
		(void)free(bp);
		if (rem < 0) {
			errs++;
			continue;
		}
		safe_setreuid(0, userid);
		sink(1, argv + argc - 1);
		safe_setreuid(userid, 0);
		(void)close(rem);
		rem = -1;
	}
}


static void
verifydir(char *cp)
{
	struct stat stb;

	if (stat(cp, &stb) >= 0) {
		if ((stb.st_mode & S_IFMT) == S_IFDIR)
			return;
		errno = ENOTDIR;
	}
	error("rcp: %s: %s.\n", cp, strerror(errno));
	exit(1);
}

char *
colon(cp)
	register char *cp;
{
	if (*cp == ':')		/* leading colon is part of file name */
		return (0);
	for (; *cp; ++cp) {
		if (*cp == ':')
			return(cp);
		if (*cp == '/')
			return(0);
	}
	return(0);
}

okname(cp0)
	char *cp0;
{
	register char *cp = cp0;
	register int c;

	do {
		c = *cp;
		if (c & 0200)
			goto bad;
		if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
			goto bad;
	} while (*++cp);
	return(1);
bad:
	(void)fprintf(stderr, "rcp: invalid user name %s\n", cp0);
	return(0);
}

susystem(s)
	char *s;
{
	pid_t pid, w;
	int status;
	register SIG_PF istat, qstat;

	if ((pid = vfork()) == 0) {
		safe_setuid(userid);
		execl(_PATH_BSHELL, "sh", "-c", s, (char *)0);
		_exit(127);
	}
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	while ((w = wait(&status)) != pid && w != -1)
		;
	if (w == -1)
		status = -1;
	(void)signal(SIGINT, istat);
	(void)signal(SIGQUIT, qstat);
	return(status);
}

static void
source(int argc, char **argv)
{
	struct stat stb;
	int b;
	off_t i;
	int x, readerr, f, amt;
	char *last, *name, buf[BUFSIZ];

	for (x = 0; x < argc; x++) {
		name = argv[x];
		if ((f = open(name, O_RDONLY, 0)) < 0) {
			error("rcp: %s: %s\n", name, strerror(errno));
			continue;
		}
		if (fstat(f, &stb) < 0)
			goto notreg;
		if (verbose && !iamremote)
			printf("> %s\n", name);
		switch (stb.st_mode&S_IFMT) {

		case S_IFREG:
			break;

		case S_IFDIR:
			if (iamrecursive) {
				(void)close(f);
				rsource(name, &stb);
				continue;
			}
			/* FALLTHROUGH */
		default:
notreg:			(void)close(f);
			error("rcp: %s: not a plain file\n", name);
			continue;
		}
		last = rindex(name, '/');
		if (last == 0)
			last = name;
		else
			last++;
		if (pflag) {
			/*
			 * Make it compatible with possible future
			 * versions expecting microseconds.
			 */
			(void)sprintf(buf, "T%ld 0 %ld 0\n", stb.st_mtime,
			    stb.st_atime);
			(void)write(rem, buf, strlen(buf));
			if (response() < 0) {
				(void)close(f);
				continue;
			}
		}
		/*
		 * st_size is a long long in the stat64 structure
		 */
		(void)sprintf(buf, "C%04o %lld %s\n", stb.st_mode&07777,
		    stb.st_size, last);
		(void)write(rem, buf, strlen(buf));
		if (response() < 0) {
			(void)close(f);
			continue;
		}
		readerr = 0;
		b = 0;
		for (i = 0; i < stb.st_size; i += NETBUFSIZE) {
			register int result;

			amt = NETBUFSIZE;
			if (i + amt > stb.st_size)
				amt = stb.st_size - i;
			if (!readerr) {
				/* do not read after disk has failed once */
				result = read(f, net_buf[b], amt);
				if (result != amt)
					readerr = (result>=0 ? EIO : errno);
			}
			result = write(rem, net_buf[b], amt);
			b++;
			if (b == NBUFS) {
				b = 0;
			}
			if (result != amt && !readerr)
				readerr = (result>=0 ? EIO : errno);
		}
		if (close(f) < 0 && !readerr)
			readerr = errno;
		if (readerr == 0)
			(void)write(rem, "", 1);
		else
			error("rcp: %s: %s\n", name, strerror(readerr));
		(void)response();
	}
}

static void
rsource(char *name, struct stat *statp)
{
	DIR *d;
	struct dirent64 *dp;
	char *newname = ".";
	char *last, *vect[1], path[MAXPATHLEN];

	/*
	 * we do this silliness because bsd opendir treats "" as "."
	 */
	if (name != NULL && *name != '\0')
		newname = name;
	if (!(d = opendir(newname))) {
		error("rcp: %s: %s\n", name, strerror(errno));
		return;
	}
	last = rindex(name, '/');
	if (last == 0)
		last = name;
	else
		last++;
	if (pflag) {
		(void)sprintf(path, "T%ld 0 %ld 0\n", statp->st_mtime,
		    statp->st_atime);
		(void)write(rem, path, strlen(path));
		if (response() < 0) {
			closedir(d);
			return;
		}
	}
	(void)sprintf(path, "D%04o %d %s\n", statp->st_mode&07777, 0, last);
	(void)write(rem, path, strlen(path));
	if (response() < 0) {
		closedir(d);
		return;
	}
	while (dp = readdir(d)) {
		if (dp->d_ino == 0)
			continue;
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (strlen(name) + 1 + strlen(dp->d_name) >= MAXPATHLEN - 1) {
			error("%s/%s: name too long.\n", name, dp->d_name);
			continue;
		}
		(void)sprintf(path, "%s/%s", name, dp->d_name);
		vect[0] = path;
		source(1, vect);
	}
	closedir(d);
	(void)write(rem, "E\n", 2);
	(void)response();
}

response()
{

	register char *cp;
	char ch, resp, rbuf[BUFSIZ];
	void lostconn();

	if (read(rem, &resp, sizeof(resp)) != sizeof(resp))
		lostconn();

	cp = rbuf;
	switch(resp) {
	case 0:				/* ok */
		return(0);
	default:
		*cp++ = resp;
		/* FALLTHROUGH */
	case 1:				/* error, followed by err msg */
	case 2:				/* fatal error, "" */
		do {
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
				lostconn();
			*cp++ = ch;
		} while (cp < &rbuf[BUFSIZ] && ch != '\n');

		if (!iamremote)
			(void)write(2, rbuf, cp - rbuf);
		++errs;
		if (resp == 1)
			return(-1);
		exit(1);
	}
	/*NOTREACHED*/
}

void
lostconn()
{
	if (!iamremote)
		(void)fprintf(stderr, "rcp: lost connection\n");
	exit(1);
}

static void
sink(int argc, char **argv)
{
	register char *cp;
	int omode, wrerrno;
	int b = 0;
	struct stat stb;
	struct timeval tv[2];
	enum { YES, NO, DISPLAYED } wrerr;
	off_t i, j;
	char ch, *targ, *why;
	int amt, count, exists, first, mask, mode;
	/*
	 * make size an off_t (off64_t) for large files
	 */
	off_t size;
	int ofd, setimes, targisdir;
	char *np, *vect[1], buf[BUFSIZ];

#define	atime	tv[0]
#define	mtime	tv[1]
#define	SCREWUP(str)	{ why = str; goto screwup; }

	setimes = targisdir = 0;
	mask = umask(0);
	if (!pflag)
		(void)umask(mask);
	if (argc != 1) {
		error("rcp: ambiguous target\n");
		exit(1);
	}
	targ = *argv;
	if (targetshouldbedirectory)
		verifydir(targ);
	(void)write(rem, "", 1);
	if (stat(targ, &stb) == 0 && (stb.st_mode & S_IFMT) == S_IFDIR)
		targisdir = 1;
	for (first = 1;; first = 0) {
		cp = buf;
		if (read(rem, cp, 1) <= 0)
			return;
		if (*cp++ == '\n')
			SCREWUP("unexpected <newline>");
		do {
			if (read(rem, &ch, sizeof(ch)) != sizeof(ch))
				SCREWUP("lost connection");
			*cp++ = ch;
		} while (cp < &buf[BUFSIZ - 1] && ch != '\n');
		*cp = 0;

		if (buf[0] == '\01' || buf[0] == '\02') {
			if (iamremote == 0)
				(void)write(2, buf + 1, strlen(buf + 1));
			if (buf[0] == '\02')
				exit(1);
			errs++;
			continue;
		}
		if (buf[0] == 'E') {
			(void)write(rem, "", 1);
			return;
		}

		if (ch == '\n')
			*--cp = 0;

#define getnum(t) (t) = 0; while (isdigit(*cp)) (t) = (t) * 10 + (*cp++ - '0');
		cp = buf;
		if (*cp == 'T') {
			setimes++;
			cp++;
			getnum(mtime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("mtime.sec not delimited");
			getnum(mtime.tv_usec);
			if (*cp++ != ' ')
				SCREWUP("mtime.usec not delimited");
			getnum(atime.tv_sec);
			if (*cp++ != ' ')
				SCREWUP("atime.sec not delimited");
			getnum(atime.tv_usec);
			if (*cp++ != '\0')
				SCREWUP("atime.usec not delimited");
			(void)write(rem, "", 1);
			continue;
		}
		if (*cp != 'C' && *cp != 'D') {
			/*
			 * Check for the case "rcp remote:foo\* local:bar".
			 * In this case, the line "No match." can be returned
			 * by the shell before the rcp command on the remote is
			 * executed so the ^Aerror_message convention isn't
			 * followed.
			 */
			if (first) {
				error("%s\n", cp);
				exit(1);
			}
			SCREWUP("expected control record");
		}
		mode = 0;
		for (++cp; cp < buf + 5; cp++) {
			if (*cp < '0' || *cp > '7')
				SCREWUP("bad mode");
			mode = (mode << 3) | (*cp - '0');
		}
		if (*cp++ != ' ')
			SCREWUP("mode not delimited");
		size = 0;
		while (isdigit(*cp))
			size = size * 10 + (*cp++ - '0');
		if (*cp++ != ' ')
			SCREWUP("size not delimited");
		if (targisdir) {
			static char *namebuf;
			static int cursize;
			int need;

			need = strlen(targ) + strlen(cp) + 250;
			if (need > cursize) {
				if (!(namebuf = malloc((u_int)need)))
					error("out of memory\n");
			}
			(void)sprintf(namebuf, "%s%s%s", targ,
			    *targ ? "/" : "", cp);
			np = namebuf;
		}
		else
			np = targ;
		if (verbose && !iamremote)
			printf("> %s\n", np);
		exists = stat(np, &stb) == 0;
		if (buf[0] == 'D') {
			int mod_flag = pflag;
			if (exists) {
				if ((stb.st_mode&S_IFMT) != S_IFDIR) {
					errno = ENOTDIR;
					goto bad;
				}
			/* handle copying from a read-only directory */
			} else {
				mod_flag++;
				if (mkdir(np, mode|0700) < 0)
					goto bad;
			}
			vect[0] = np;
			sink(1, vect);
			if (setimes) {
				setimes = 0;
				if (utimes(np, tv) < 0)
				    error("rcp: can't set times on %s: %s\n",
					np, strerror(errno));
			}
			if (mod_flag)
				(void) chmod(np, mode);
			continue;
		}
		omode = mode;
		mode |= S_IWRITE;
		if ((ofd = open(np, O_WRONLY|O_CREAT, mode)) < 0) {
bad:			error("rcp: %s: %s\n", np, strerror(errno));
			continue;
		}
		(void)write(rem, "", 1);
		wrerr = NO;
		i = 0;
		while (i < size) {
			amt = NETBUFSIZE;
			cp = net_buf[b];
			count = 0;
			if (i + amt > size)
				amt = size - i;
			do {
				j = read(rem, cp, amt);
				if (j <= 0) {
					error("rcp: %s\n",
					    j ? strerror(errno) :
					    "dropped connection");
					exit(1);
				}
				amt -= j;
				count += j;
				cp += j;

				/*
				 * If other side is slow, go ahead and empty
				 * our buffer to the disk.
				 */
			} while (amt > 0 && count < 2048);

			/* Do not write after disk has failed once */
			if (wrerr == NO) {
				j = write(ofd, net_buf[b], count);
				if (j != count) {
					wrerr = YES;
					wrerrno = (j>=0 ? EIO : errno);
				}
			}
			b++;
			if (b == NBUFS) {
				b = 0;
			}
			i += count;
		}
		/*
		 * if we've had write errors, truncating the file to
		 * "size" will make it appear artificially larger
		 * than it really is
		 *
		 * ftruncate is only valid for regular files.
		 */

		if (wrerr == NO &&
		    (fstat(ofd, &stb) == 0) &&
		    S_ISREG(stb.st_mode) &&
		    ftruncate64(ofd, size)) {
			error("rcp: can't truncate %s: %s\n",
			    np, strerror(errno));
			wrerr = DISPLAYED;
		}
		if (pflag) {
			if (exists || omode != mode)
				(void) fchmod(ofd, omode);
		} else {
			if (!exists && omode != mode)
				(void) fchmod(ofd, omode & ~mask);
		}
		(void)close(ofd);
		(void)response();
		if (setimes && wrerr == NO) {
			setimes = 0;
			if (utimes(np, tv) < 0) {
				error("rcp: can't set times on %s: %s\n",
				    np, strerror(errno));
				wrerr = DISPLAYED;
			}
		}
		switch(wrerr) {
		case YES:
			error("rcp: %s: %s\n", np, strerror(wrerrno));
			break;
		case NO:
			(void)write(rem, "", 1);
			break;
		case DISPLAYED:
			break;
		}
	}
screwup:
	error("rcp: protocol screwup: %s\n", why);
	exit(1);
}


static void
error(char *fmt, ...)
{
	static FILE *fp;
	va_list args;

	va_start(args, fmt);
	++errs;
	if (!fp && !(fp = fdopen(rem, "w")))
		return;
	(void)fprintf(fp, "%c", 0x01);
	(void)vfprintf(fp, fmt, args);
	(void)fflush(fp);
	if (!iamremote)
		(void)vfprintf(stderr, fmt, args);
	va_end(args);
}

static void
safe_setuid(int userid)
{
    if (setuid(userid) < 0) {
	    error("rcp: bad user id: %s\n", strerror(errno));
	    exit(1);
    }
}

static void
safe_setreuid(int r, int e)
{
    if (setreuid(r, e) < 0) {
	    error("rcp: bad user id: %s\n", strerror(errno));
	    exit(1);
    }
}

static void
nospace(void)
{
	(void)fprintf(stderr, "rcp: out of memory.\n");
	exit(1);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: rcp [-pv] f1 f2; or: rcp [-rpv] f1 ... fn directory\n");
	exit(1);
}
