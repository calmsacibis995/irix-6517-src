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
static char sccsid[] = "@(#)rlogin.c	5.12 (Berkeley) 9/19/88";
#endif /* not lint */

#ifdef sgi
#define _BSD_SIGNALS
#endif
/*
 * rlogin - remote login
 */
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <stdio.h>
#include <sgtty.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <setjmp.h>
#include <netdb.h>

# ifndef TIOCPKT_WINDOW
# define TIOCPKT_WINDOW 0x80
# endif /* TIOCPKT_WINDOW */

static void doit(int);
static void setsignal(int, void (*)());
static void done(int);
static void writer(void);
static void echo(char);
static void stop(char);
static void sendwindow(void);
static void mode(int);
void prf(char *f, ...);

/* concession to sun */
# ifndef SIGUSR1
# define SIGUSR1 30
# endif /* SIGUSR1 */

#ifdef sgi
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
extern int _getpwent_no_shadow;
#else
char	*index(), *rindex(), *malloc(), *getenv(), *strcat(), *strcpy();
struct	passwd *getpwuid();
#endif
char	*name;
int	rem;
char	cmdchar = '~';
int	eight;
int	litout;
#ifndef sgi
char	*speeds[] =
    { "0", "50", "75", "110", "134", "150", "200", "300",
      "600", "1200", "1800", "2400", "4800", "9600", "19200", "38400" };
#endif
char	term[267] = "network";	/* 256 + slop for speed; 
				 * assumes speed < 10 digits
				 */
extern	int errno;
#ifdef sgi
void	lostpeer();
#else
int	lostpeer();
#endif
int	dosigwinch = 0;
#ifndef sigmask
#define sigmask(m)	(1 << ((m)-1))
#endif
#ifdef sun
struct winsize {
	unsigned short ws_row, ws_col;
	unsigned short ws_xpixel, ws_ypixel;
};
#endif /* sun */
struct	winsize winsize;
#ifdef sgi
void	sigwinch(), oob();
#else
int	sigwinch(), oob();
#endif
#ifdef sgi
extern int rcmd();
int (*rcmdp)() = rcmd;
#endif
#ifdef AFS
extern int (*__afs_getsym())();
int afsdebug;
int *ta_rauthp;
#endif

/*
 * The following routine provides compatibility (such as it is)
 * between 4.2BSD Suns and others.  Suns have only a `ttysize',
 * so we convert it to a winsize.
 */
#ifdef sun
int
get_window_size(fd, wp)
	int fd;
	struct winsize *wp;
{
	struct ttysize ts;
	int error;

	if ((error = ioctl(0, TIOCGSIZE, &ts)) != 0)
		return (error);
	wp->ws_row = ts.ts_lines;
	wp->ws_col = ts.ts_cols;
	wp->ws_xpixel = 0;
	wp->ws_ypixel = 0;
	return (0);
}
#else /* sun */
#define get_window_size(fd, wp)	ioctl(fd, TIOCGWINSZ, wp)
#endif /* sun */

main(argc, argv)
	int argc;
	char **argv;
{
	char *host, *cp;
#ifdef	sgi
	extern char *optarg;
	extern int optind;
	struct termio ttyb;
#else
	struct sgttyb ttyb;
#endif /* sgi */
	struct passwd *pwd;
	struct servent *sp;
	int port;
#ifdef sgi
	uid_t uid;
	int options = 0, oldmask;
#else
	int uid, options = 0, oldmask;
#endif
	int on = 1;

#ifdef sgi
	int ch;
	int argoff = 0;
#endif

	host = rindex(argv[0], '/');
	if (host)
		host++;
	else
		host = argv[0];

#ifdef sgi
	if (!strcmp(host, "rlogin"))
		host = NULL;

        /* handle "rlogin host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

	while ((ch = getopt(argc - argoff, argv + argoff, "8Lde:l:")) != EOF)
		switch(ch) {
		case '8':
			eight = 1;
			break;
		case 'L':
			litout = 1;
			break;
		case 'd':
			options |= SO_DEBUG;
#ifdef AFS
			afsdebug = 1;
#endif
			break;
		case 'e':
			cmdchar = optarg[0];
			break;
		case 'l':
			name = optarg;
			break;
		case '?':
		default:
			goto usage;
		}
	optind += argoff;
	argc -= optind;
	argv += optind;

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = *argv++))
		goto usage;
	
	if (*argv)
		goto usage;

#else	/* ! sgi */
	argv++, --argc;
	if (!strcmp(host, "rlogin"))
		host = *argv++, --argc;
another:
	if (argc > 0 && !strcmp(*argv, "-d")) {
		argv++, argc--;
		options |= SO_DEBUG;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-l")) {
		argv++, argc--;
		if (argc == 0)
			goto usage;
		name = *argv++; argc--;
		goto another;
	}
	if (argc > 0 && !strncmp(*argv, "-e", 2)) {
		cmdchar = argv[0][2];
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-8")) {
		eight = 1;
		argv++, argc--;
		goto another;
	}
	if (argc > 0 && !strcmp(*argv, "-L")) {
		litout = 1;
		argv++, argc--;
		goto another;
	}
	if (host == 0)
		goto usage;
	if (argc > 0)
		goto usage;
#endif	/* sgi */

#ifdef sgi
	/*
	** Since we are a setuid root application and getpwuid will map
	** /etc/shadow, we just tell it to ignore shadow.  We don't need
	** it anyway.
	*/
	_getpwent_no_shadow = 1;
#endif
	pwd = getpwuid(getuid());
	if (pwd == 0) {
		fprintf(stderr, "Who are you?\n");
		exit(1);
	}
#ifdef sgi
	port = 513;
#else
	sp = getservbyname("login", "tcp");
	if (sp == 0) {
		fprintf(stderr, "rlogin: login/tcp: unknown service\n");
		exit(2);
	}
	port = sp->s_port;
#endif

#ifdef	sgi
	/* Accept user1@host format, though "-l user2" overrides user1 */
	cp = strchr(host, '@');
	if (cp) {
		*cp = '\0';
		if (name == NULL && cp > host)
			name = host;
		host = cp + 1;
		if (*host == '\0')
			goto usage;
	}
	cp = getenv("TERM");
	if (cp) {
		if (!strcmp(cp, "wsiris"))
			cp = "rwsiris";
		else if (!strcmp(cp, "iris-ansi"))
			cp = "iris-ansi-net";
		/* leave some room for sprintf() below */
		strncpy(term, cp, sizeof(term) - 10);
		term[sizeof(term) - 11] = '\0';	/* make sure */
	}
	if (ioctl(0, TCGETA, &ttyb) == 0) {
		strcat(term, "/");
		sprintf(term+strlen(term), "%d",ttyb.c_ospeed);
	}
#else
	cp = getenv("TERM");
	if (cp)
		(void) strcpy(term, cp);
	if (ioctl(0, TIOCGETP, &ttyb) == 0) {
		(void) strcat(term, "/");
		(void) strcat(term, speeds[ttyb.sg_ospeed]);
	}
#endif /* sgi */
#ifdef AFS
	if ((rcmdp = __afs_getsym("rcmd")) == NULL)
		rcmdp = rcmd;
	if (afsdebug && (ta_rauthp = (int *)(__afs_getsym("ta_debug"))))
		*ta_rauthp = 1;
#endif
	(void) get_window_size(0, &winsize);
	(void) signal(SIGPIPE, lostpeer);
	/* will use SIGUSR1 for window size hack, so hold it off */
	oldmask = sigblock(sigmask(SIGURG) | sigmask(SIGUSR1));
#ifdef sgi
	/* set a signal handler so we do not lose an early SIGURG */
	{
		void copytochild();
		(void) signal(SIGURG, copytochild);
	}
#endif
        rem = (*rcmdp)(&host, port, pwd->pw_name,
	    name ? name : pwd->pw_name, term, 0);
        if (rem < 0)
                exit(1);
	if (options & SO_DEBUG &&
	    setsockopt(rem, SOL_SOCKET, SO_DEBUG, &on, sizeof (on)) < 0)
		perror("rlogin: setsockopt (SO_DEBUG)");
	on = IPTOS_LOWDELAY;
	if (setsockopt(rem, IPPROTO_IP, IP_TOS, &on, sizeof(on)) < 0)
		perror("rlogin: setsockopt TOS (ignored)");

	uid = getuid();
	if (setuid(uid) < 0) {
		perror("rlogin: setuid");
		exit(1);
	}
	doit(oldmask);
	/*NOTREACHED*/
usage:
	fprintf(stderr,
	    "usage: rlogin host [ -ex ] [ -l username ] [ -8 ] [ -L ]\n");
#ifdef sgi
	fprintf(stderr,
	    "       rlogin [ -ex ] [ -l username ] [ -8 ] [ -L ] host\n");
	fprintf(stderr,
	    "       rlogin username@host [ -ex ] [ -8 ] [ -L ]\n");
#endif
	exit(1);
}

#define CRLF "\r\n"

int	child;
#ifdef sgi
void	catchild();
void	copytochild(), writeroob();
#else
int	catchild();
int	copytochild(), writeroob();
#endif

#ifdef	sgi
struct	termio	def_termio;
#define deftc	def_termio
#define defltc	def_termio
#define t_eofc	c_cc[VEOF]
#define t_intrc	c_cc[VINTR]
#define t_suspc	c_cc[VSUSP]
#define t_dsuspc c_cc[VSUSP]		/* XXX */
#define defkill	def_termio.c_cc[VKILL]

#else
int	defflags, tabflag;
int	deflflags;
char	deferase, defkill;
struct	tchars deftc;
struct	ltchars defltc;
struct	tchars notc =	{ -1, -1, -1, -1, -1, -1 };
struct	ltchars noltc =	{ -1, -1, -1, -1, -1, -1 };
#endif /* sgi */

static void
doit(int oldmask)
{
#ifdef	sgi
	struct termio sb;
#else
	int exit();
	struct sgttyb sb;
#endif /* sgi */

#ifdef	sgi
	ioctl(0, TCGETA, (char *)&sb);
	def_termio = sb;
#else
	(void) ioctl(0, TIOCGETP, (char *)&sb);
	defflags = sb.sg_flags;
	tabflag = defflags & TBDELAY;
	defflags &= ECHO | CRMOD;
	deferase = sb.sg_erase;
	defkill = sb.sg_kill;
	(void) ioctl(0, TIOCLGET, (char *)&deflflags);
	(void) ioctl(0, TIOCGETC, (char *)&deftc);
	notc.t_startc = deftc.t_startc;
	notc.t_stopc = deftc.t_stopc;
	(void) ioctl(0, TIOCGLTC, (char *)&defltc);
#endif /* sgi */
	(void) signal(SIGINT, SIG_IGN);
	setsignal(SIGHUP, exit);
	setsignal(SIGQUIT, exit);
	child = fork();
	if (child == -1) {
		perror("rlogin: fork");
		done(1);
	}
	if (child == 0) {
		mode(1);
		if (reader(oldmask) == 0) {
			prf("Connection closed.");
			exit(0);
		}
		sleep(1);
		prf("\007Connection closed.");
		exit(3);
	}

	/*
	 * We may still own the socket, and may have a pending SIGURG
	 * (or might receive one soon) that we really want to send to
	 * the reader.  Set a trap that simply copies such signals to
	 * the child.
	 */
	(void) signal(SIGURG, copytochild);
	(void) signal(SIGUSR1, writeroob);
	(void) sigsetmask(oldmask);
	(void) signal(SIGCHLD, catchild);
	writer();
	prf("Closed connection.");
	done(0);
}

/*
 * Trap a signal, unless it is being ignored.
 */
static void
setsignal(int sig, void (*act)())
{
	int omask = sigblock(sigmask(sig));

	if (signal(sig, act) == SIG_IGN)
		(void) signal(sig, SIG_IGN);
	(void) sigsetmask(omask);
}

static void
done(int status)
{
	int w;

	mode(0);
	if (child > 0) {
		/* make sure catchild does not snap it up */
		(void) signal(SIGCHLD, SIG_DFL);
		if (kill(child, SIGKILL) >= 0)
#ifdef sgi
			while ((w = wait((int *)0)) > 0 && w != child)
#else
			while ((w = wait((union wait *)0)) > 0 && w != child)
#endif
				/*void*/;
	}
	exit(status);
}

/*
 * Copy SIGURGs to the child process.
 */
#ifdef sgi
void
#endif
copytochild()
{

	(void) kill(child, SIGURG);
}

/*
 * This is called when the reader process gets the out-of-band (urgent)
 * request to turn on the window-changing protocol.
 */
#ifdef sgi
void
#endif
writeroob()
{

	if (dosigwinch == 0) {
		get_window_size(0, &winsize);	/* avoid xwsh race */
		sendwindow();
		(void) signal(SIGWINCH, sigwinch);
	}
	dosigwinch = 1;
}

#ifdef sgi
void
#endif
catchild()
{
	union wait status;
	int pid;

again:
	pid = wait3((int *)&status, WNOHANG|WUNTRACED, (struct rusage *)0);
	if (pid == 0)
		return;
	/*
	 * if the child (reader) dies, just quit
	 */
	if (pid < 0 || pid == child && !WIFSTOPPED(status))
		done((int)(status.w_termsig | status.w_retcode));
	goto again;
}

/*
 * writer: write to remote: 0 -> line.
 * ~.	terminate
 * ~^Z	suspend rlogin process.
 * ~^Y  suspend rlogin process, but leave reader alone.
 */
static void
writer(void)
{
	char c;
	register int n;
	register int bol = 1;               /* beginning of line */
	register int local = 0;

	for (;;) {
		n = read(0, &c, 1);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			break;
		}
		/*
		 * If we're at the beginning of the line
		 * and recognize a command character, then
		 * we echo locally.  Otherwise, characters
		 * are echo'd remotely.  If the command
		 * character is doubled, this acts as a 
		 * force and local echo is suppressed.
		 */
		if (bol) {
			bol = 0;
			if (c == cmdchar) {
				bol = 0;
				local = 1;
				continue;
			}
		} else if (local) {
			local = 0;
			if (c == '.' || c == deftc.t_eofc) {
				echo(c);
				break;
			}
#ifdef sgi
		    /* Make sure the line discipline supports job control
		     * and job control is activated. */
			if (def_termio.c_line == LDISC1 && 
			    defltc.t_suspc != CNSWTCH && 
			    (c == defltc.t_suspc || c == defltc.t_dsuspc)) {
				bol = 1;
				echo(c);
				stop(c);
				continue;
			}
#else /* orig */
			if (c == defltc.t_suspc || c == defltc.t_dsuspc) {
				bol = 1;
				echo(c);
				stop(c);
				continue;
			}
#endif /* sgi */
			if (c != cmdchar)
				(void) write(rem, &cmdchar, 1);
		}
		if (write(rem, &c, 1) == 0) {
			prf("line gone");
			break;
		}
		bol = c == defkill || c == deftc.t_eofc ||
		    c == deftc.t_intrc || c == defltc.t_suspc ||
		    c == '\r' || c == '\n';
	}
}

static void
echo(register char c)
{
	char buf[8];
	register char *p = buf;

	c &= 0177;
	*p++ = cmdchar;
	if (c < ' ') {
		*p++ = '^';
		*p++ = c + '@';
	} else if (c == 0177) {
		*p++ = '^';
		*p++ = '?';
	} else
		*p++ = c;
	*p++ = '\r';
	*p++ = '\n';
	(void) write(1, buf, p - buf);
}

static void
stop(char cmdc)
{
	mode(0);
	(void) signal(SIGCHLD, SIG_IGN);
	(void) kill(cmdc == defltc.t_suspc ? 0 : getpid(), SIGTSTP);
	(void) signal(SIGCHLD, catchild);
	mode(1);
	sigwinch();			/* check for size changes */
}

#ifdef sgi
void
#endif
sigwinch()
{
	struct winsize ws;

	if (dosigwinch && get_window_size(0, &ws) == 0 &&
	    bcmp(&ws, &winsize, sizeof (ws))) {
		winsize = ws;
		sendwindow();
	}
}

/*
 * Send the window size to the server via the magic escape
 */
static void
sendwindow(void)
{
	char obuf[4 + sizeof (struct winsize)];
	struct winsize *wp = (struct winsize *)(obuf+4);

	obuf[0] = 0377;
	obuf[1] = 0377;
	obuf[2] = 's';
	obuf[3] = 's';
	wp->ws_row = htons(winsize.ws_row);
	wp->ws_col = htons(winsize.ws_col);
	wp->ws_xpixel = htons(winsize.ws_xpixel);
	wp->ws_ypixel = htons(winsize.ws_ypixel);
	(void) write(rem, obuf, sizeof(obuf));
}

/*
 * reader: read from remote: line -> 1
 */
#define	READING	1
#define	WRITING	2

char	rcvbuf[8 * 1024];
int	rcvcnt;
int	rcvstate;
int	ppid;
jmp_buf	rcvtop;

#ifdef sgi
void
#endif
oob()
{
	int out = FWRITE, atmark, n;
	int rcvd = 0;
	char waste[BUFSIZ], mark;
#ifdef sgi
	struct termio sb;
#else
	struct sgttyb sb;
#endif /* sgi */

	while (recv(rem, &mark, 1, MSG_OOB) < 0)
		switch (errno) {
		
		case EWOULDBLOCK:
			/*
			 * Urgent data not here yet.
			 * It may not be possible to send it yet
			 * if we are blocked for output
			 * and our input buffer is full.
			 */
			if (rcvcnt < sizeof(rcvbuf)) {
				n = read(rem, rcvbuf + rcvcnt,
					sizeof(rcvbuf) - rcvcnt);
				if (n <= 0)
					return;
				rcvd += n;
				rcvcnt += n;
			} else {
				n = read(rem, waste, sizeof(waste));
				if (n <= 0)
					return;
			}
			continue;
				
		default:
			return;
	}
	if (mark & TIOCPKT_WINDOW) {
		/*
		 * Let server know about window size changes
		 */
		(void) kill(ppid, SIGUSR1);
	}
	if (!eight && (mark & TIOCPKT_NOSTOP)) {
#ifdef sgi
		if (def_termio.c_iflag & IXON) {
			ioctl(0, TCGETA, (char *)&sb);
			sb.c_iflag &= ~IXON;
			ioctl(0, TCSETA, (char *)&sb);
		}
#else
		(void) ioctl(0, TIOCGETP, (char *)&sb);
		sb.sg_flags &= ~CBREAK;
		sb.sg_flags |= RAW;
		(void) ioctl(0, TIOCSETN, (char *)&sb);
		notc.t_stopc = -1;
		notc.t_startc = -1;
		(void) ioctl(0, TIOCSETC, (char *)&notc);
#endif /* sgi */
	}
	if (!eight && (mark & TIOCPKT_DOSTOP)) {
#ifdef sgi
		if (def_termio.c_iflag & IXON) {
			ioctl(0, TCGETA, (char *)&sb);
			sb.c_iflag |= IXON;
			ioctl(0, TCSETA, (char *)&sb);
		}
#else
		(void) ioctl(0, TIOCGETP, (char *)&sb);
		sb.sg_flags &= ~RAW;
		sb.sg_flags |= CBREAK;
		(void) ioctl(0, TIOCSETN, (char *)&sb);
		notc.t_stopc = deftc.t_stopc;
		notc.t_startc = deftc.t_startc;
		(void) ioctl(0, TIOCSETC, (char *)&notc);
#endif /* sgi */
	}
	if (mark & TIOCPKT_FLUSHWRITE) {
		(void) ioctl(1, TIOCFLUSH, (char *)&out);
		for (;;) {
			if (ioctl(rem, SIOCATMARK, &atmark) < 0) {
				perror("ioctl");
				break;
			}
			if (atmark)
				break;
			n = read(rem, waste, sizeof (waste));
			if (n <= 0)
				break;
		}
		/*
		 * Don't want any pending data to be output,
		 * so clear the recv buffer.
		 * If we were hanging on a write when interrupted,
		 * don't want it to restart.  If we were reading,
		 * restart anyway.
		 */
		rcvcnt = 0;
		longjmp(rcvtop, 1);
	}

	/*
	 * oob does not do FLUSHREAD (alas!)
	 */

	/*
	 * If we filled the receive buffer while a read was pending,
	 * longjmp to the top to restart appropriately.  Don't abort
	 * a pending write, however, or we won't know how much was written.
	 */
	if (rcvd && rcvstate == READING)
		longjmp(rcvtop, 1);
}

/*
 * reader: read from remote: line -> 1
 */
reader(oldmask)
	int oldmask;
{
#ifdef sgi
	int pid = getpid();
#else
#if !defined(BSD) || BSD < 43
	int pid = -getpid();
#else
	int pid = getpid();
#endif
#endif /* sgi */
	int n, remaining;
	char *bufp = rcvbuf;

	(void) signal(SIGTTOU, SIG_IGN);
	(void) signal(SIGURG, oob);
	ppid = getppid();
	if ( fcntl(rem, F_SETOWN, pid) < 0) {
		perror("rlogin: F_SETOWN fcntl failed");
	}
	(void) setjmp(rcvtop);
	(void) sigsetmask(oldmask);
	for (;;) {
		while ((remaining = rcvcnt - (bufp - rcvbuf)) > 0) {
			rcvstate = WRITING;
			n = write(1, bufp, remaining);
			if (n < 0) {
				if (errno != EINTR)
					return (-1);
				continue;
			}
			bufp += n;
		}
		bufp = rcvbuf;
		rcvcnt = 0;
		rcvstate = READING;
		rcvcnt = read(rem, rcvbuf, sizeof (rcvbuf));
		if (rcvcnt == 0)
			return (0);
		if (rcvcnt < 0) {
			if (errno == EINTR)
				continue;
			perror("read");
			return (-1);
		}
	}
}

#ifdef	sgi
static void
mode(int f)
{
	struct termio sb;

	ioctl(0, TCGETA, &sb);
	switch (f) {
	case 0:				/* cooked */
		sb = def_termio;
		break;
	case 1:				/* rawish */
		sb.c_iflag &= ~(ISTRIP|ICRNL|INLCR|IGNCR|IXON|BRKINT|IXOFF);
		sb.c_lflag &= ~(ICANON|ECHO|ISIG);
		sb.c_oflag &= ~OPOST;
		sb.c_oflag &= ~TABDLY; sb.c_oflag |= TAB3;
		sb.c_cc[VTIME] = 1;
		sb.c_cc[VMIN] = 1;
		break;
	  default:
		return;
	}
	ioctl(0, TCSETAW, &sb);
}

void
prf(char *f, ...)
{
	va_list args;

	va_start(args, f);
	(void)vfprintf(stderr, f, args);
	va_end(args);
	fprintf(stderr, CRLF);
}
#else
mode(f)
{
	struct tchars *tc;
	struct ltchars *ltc;
	struct sgttyb sb;
	int	lflags;

	(void) ioctl(0, TIOCGETP, (char *)&sb);
	(void) ioctl(0, TIOCLGET, (char *)&lflags);
	switch (f) {

	case 0:
		sb.sg_flags &= ~(CBREAK|RAW|TBDELAY);
		sb.sg_flags |= defflags|tabflag;
		tc = &deftc;
		ltc = &defltc;
		sb.sg_kill = defkill;
		sb.sg_erase = deferase;
		lflags = deflflags;
		break;

	case 1:
		sb.sg_flags |= (eight ? RAW : CBREAK);
		sb.sg_flags &= ~defflags;
		/* preserve tab delays, but turn off XTABS */
		if ((sb.sg_flags & TBDELAY) == XTABS)
			sb.sg_flags &= ~TBDELAY;
		tc = &notc;
		ltc = &noltc;
		sb.sg_kill = sb.sg_erase = -1;
		if (litout)
			lflags |= LLITOUT;
		break;

	default:
		return;
	}
	(void) ioctl(0, TIOCSLTC, (char *)ltc);
	(void) ioctl(0, TIOCSETC, (char *)tc);
	(void) ioctl(0, TIOCSETN, (char *)&sb);
	(void) ioctl(0, TIOCLSET, (char *)&lflags);
}

/*VARARGS*/
prf(f, a1, a2, a3, a4, a5)
	char *f;
{

	fprintf(stderr, f, a1, a2, a3, a4, a5);
	fprintf(stderr, CRLF);
}
#endif /* sgi */

#ifdef sgi
void
#endif
lostpeer()
{

	(void) signal(SIGPIPE, SIG_IGN);
	prf("\007Connection closed.");
	done(1);
}
