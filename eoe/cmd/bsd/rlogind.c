/*
 * Copyright (c) 1983, 1988 The Regents of the University of California.
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
"@(#) Copyright (c) 1983, 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rlogind.c	5.22.1.7 (Berkeley) 9/11/89";
#endif /* not lint */

/*
 * remote login server:
 *	\0
 *	remuser\0
 *	locuser\0
 *	terminal_type/speed\0
 *	data
 */

#define _BSD_SIGNALS

#include <stdio.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>
#include <arpa/inet.h>
#if BSD > 43
#include <sys/termios.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <syslog.h>
#include <strings.h>
#include <sys/sysmacros.h>
#include <sys/stream.h>
#include <paths.h>
#include <sys/sat.h>

#include <unistd.h>
#include <utmpx.h>

extern char * _getpty(int*, int, mode_t, int);

static void doit(int, struct sockaddr_in *);
static void protocol(int, int);
static void fatal(int, char *);
static void fatalperror(int, char *);
static void adut(void);
static void rmut(void);
static void setup_term(int);


#ifndef TIOCPKT_WINDOW
#define TIOCPKT_WINDOW 0x80
#endif

int	keepalive = 1;
int	check_all = 0;
int	check_rhosts_file = 1;

main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	int fromlen, on;
	struct sockaddr_in from;

	openlog("rlogind", LOG_PID | LOG_CONS, LOG_AUTH);

	opterr = 0;
	while ((ch = getopt(argc, argv, "aln")) != EOF)
		switch (ch) {
		case 'a':
			check_all = 1;
			break;
		case 'l':
			check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
		case '?':
		default:
			syslog(LOG_ERR, "usage: rlogind [-a] [-l] [-n]");
			break;
		}
	argc -= optind;
	argv += optind;

	fromlen = sizeof (from);
	if (getpeername(0, &from, &fromlen) < 0) {
		syslog(LOG_ERR, "Couldn't get peer name of remote host: %m");
		fatalperror(0, "Can't get peer name of remote host");
	}
	on = 1;
	if (keepalive &&
	    setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof (on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	on = IPTOS_LOWDELAY;
	if (setsockopt(0, IPPROTO_IP, IP_TOS, &on, sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
	doit(0, &from);
}


int	child;
extern	void cleanup();
int	netf;
char	*line;

struct winsize win = { 0, 0, 0, 0 };

static void
doit(int f, struct sockaddr_in *fromp)
{
	int i, p, r, t, pid, on = 1;
	char *remotehost;
	char c;
	cap_t ocap;
	cap_value_t cap_device_mgt = CAP_DEVICE_MGT;
	cap_value_t cap_fowner = CAP_FOWNER;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	cap_value_t cap_dac_override[] = {CAP_DAC_READ_SEARCH, CAP_DAC_WRITE};
	extern struct hostent * __getverfhostent(struct in_addr, int);

	alarm(60);
	read(f, &c, 1);
	if (c != 0)
		exit(1);

	alarm(0);
	fromp->sin_port = ntohs((u_short)fromp->sin_port);
	remotehost = __getverfhostent(fromp->sin_addr, check_all)->h_name;

	if (fromp->sin_family != AF_INET ||
	    fromp->sin_port >= IPPORT_RESERVED ||
	    fromp->sin_port < IPPORT_RESERVED/2) {
		char msg[256];
		sprintf(msg, "Connection from %s on illegal port %u",
			inet_ntoa(fromp->sin_addr), fromp->sin_port);
		syslog(LOG_NOTICE, msg);
		ocap = cap_acquire(1, &cap_audit_write);
		satvwrite(SAT_AE_IDENTITY, SAT_FAILURE,
			  "rlogind|-|?|%s", msg);
		cap_surrender(ocap);
		fatal(f, "Permission denied");
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
		    "Connection from %s using IP options (ignored):%s",
		    remotehost, lbuf);
		if (setsockopt(0, ipproto, IP_OPTIONS,
		    (char *)NULL, optsize) != 0) {
			syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
			exit(1);
		}
	}
      }
#endif
	write(f, "", 1);

	line = _getpty(&p, O_RDWR|O_NDELAY, 0, 0);
	if (line == 0)
		fatalperror(f, "Out of ptys");
	(void) ioctl(p, TIOCSWINSZ, &win);
	netf = f;
	ocap = cap_acquire(2, cap_dac_override);
	t = open(line, O_RDWR);
	cap_surrender(ocap);
	if (t < 0)
		fatalperror(f, line);
	ocap = cap_acquire(1, &cap_fowner);
	r = fchmod(t, 0);
	cap_surrender(ocap);
	if (r)
		fatalperror(f, line);
	(void)signal(SIGHUP, SIG_IGN);
	ocap = cap_acquire(1, &cap_device_mgt);
	vhangup();
	cap_surrender(ocap);
	(void)signal(SIGHUP, SIG_DFL);
	setpgrp();		/* clear controlling tty */
	ocap = cap_acquire(2, cap_dac_override);
	t = open(line, O_RDWR);
	cap_surrender(ocap);
	if (t < 0)
		fatalperror(f, line);
	/*
	 * Make the pty world-writable so programs like write & talk work.
	 * XXX should be protected, with "approved" setgid programs allowed
	 * to write to ptys.
	 */
	ocap = cap_acquire(1, &cap_fowner);
	r = fchmod(t, 0622);
	cap_surrender(ocap);
	if (r)
		fatalperror(f, line);
	setup_term(t);

	pid = fork();
	if (pid < 0)
		fatalperror(f, "");
	if (pid == 0) {
#if BSD > 43
		if (setsid() < 0)
			fatalperror(f, "setsid");
		if (ioctl(t, TIOCSCTTY, 0) < 0)
			fatalperror(f, "ioctl(sctty)");
#endif
		close(f), close(p);
		dup2(t, 0), dup2(t, 1), dup2(t, 2);
		adut();
		closelog();		/* so syslog() can work in login */
		for (t = getdtablehi(); --t > 2; )
			(void)close(t);
		execl(_PATH_SCHEME, "login",
			check_rhosts_file ? "-r" : "-R", remotehost, 0);
		syslog(LOG_ERR,"rlogind: failed to exec %s: %d",
						_PATH_SCHEME, errno);
		execl("/usr/bin/login", "login",
			check_rhosts_file ? "-r" : "-R", remotehost, 0);
		syslog(LOG_ERR,"rlogind: failed to exec /usr/bin/login: %d",
						errno);
		execl("/bin/login", "login",
			check_rhosts_file ? "-r" : "-R", remotehost, 0);
		syslog(LOG_ERR,"rlogind: failed to exec /bin/login: %d",errno);
		fatalperror(2, "/bin/login");
		/*NOTREACHED*/
	}
	close(t);

	ioctl(f, FIONBIO, &on);
	ioctl(p, FIONBIO, &on);
	ioctl(p, TIOCPKT, &on);
	/* protect against /bin/sh */
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGCHLD, cleanup);
	protocol(f, p);
	signal(SIGCHLD, SIG_IGN);
	cleanup();
}

char	magic[2] = { 0377, 0377 };
char	oobdata[] = {TIOCPKT_WINDOW};

/*
 * Handle a "control" request (signaled by magic being present)
 * in the data stream.  For now, we are only willing to handle
 * window size changes.
 */
control(pty, cp, n)
	int pty;
	char *cp;
	int n;
{
	struct winsize w;

	if (n < 4+sizeof (w) || cp[2] != 's' || cp[3] != 's')
		return (0);
	oobdata[0] &= ~TIOCPKT_WINDOW;	/* we know he heard */
	bcopy(cp+4, (char *)&w, sizeof(w));
	w.ws_row = ntohs(w.ws_row);
	w.ws_col = ntohs(w.ws_col);
	w.ws_xpixel = ntohs(w.ws_xpixel);
	w.ws_ypixel = ntohs(w.ws_ypixel);
	(void)ioctl(pty, TIOCSWINSZ, &w);
	return (4+sizeof (w));
}

/*
 * rlogin "protocol" machine.
 */
static void
protocol(int f, int p)
{
	char pibuf[MAXBSIZE], fibuf[1024], *pbp, *fbp;
	register pcc = 0, fcc = 0;
	int cc, nfd, pmask, fmask;
	char cntl;

	/*
	 * Must ignore SIGTTOU, otherwise we'll stop
	 * when we try and set slave pty's window shape
	 * (our controlling tty is the master pty).
	 */
	(void) signal(SIGTTOU, SIG_IGN);
	send(f, oobdata, 1, MSG_OOB);	/* indicate new rlogin */
	if (f > p)
		nfd = f + 1;
	else
		nfd = p + 1;
	fmask = 1 << f;
	pmask = 1 << p;
	for (;;) {
		int ibits, obits, ebits;

		ibits = 0;
		obits = 0;
		if (fcc)
			obits |= pmask;
		else
			ibits |= fmask;
		if (pcc >= 0)
			if (pcc)
				obits |= fmask;
			else
				ibits |= pmask;
		ebits = pmask;
		if (select(nfd, (fd_set *)&ibits, obits ? (fd_set *)&obits : (fd_set *)NULL,
		    (fd_set *)&ebits, 0) < 0) {
			if (errno == EINTR)
				continue;
			fatalperror(f, "select");
		}
		if (ibits == 0 && obits == 0 && ebits == 0) {
			/* shouldn't happen... */
			sleep(5);
			continue;
		}
#define	pkcontrol(c)	((c)&(TIOCPKT_FLUSHWRITE|TIOCPKT_NOSTOP|TIOCPKT_DOSTOP))
		if (ebits & pmask) {
			cc = read(p, &cntl, 1);
			if (cc == 1 && pkcontrol(cntl)) {
				cntl |= oobdata[0];
				send(f, &cntl, 1, MSG_OOB);
				if (cntl & TIOCPKT_FLUSHWRITE) {
					pcc = 0;
					ibits &= ~pmask;
				}
			}
		}
		if (ibits & fmask) {
			fcc = read(f, fibuf, sizeof(fibuf));
			if (fcc < 0 && errno == EWOULDBLOCK)
				fcc = 0;
			else {
				register char *cp;
				int left, n;

				if (fcc <= 0)
					break;
				fbp = fibuf;

			top:
				for (cp = fibuf; cp < fibuf+fcc-1; cp++)
					if (cp[0] == magic[0] &&
					    cp[1] == magic[1]) {
						left = fcc - (cp-fibuf);
						n = control(p, cp, left);
						if (n) {
							left -= n;
							if (left > 0)
								bcopy(cp+n, cp, left);
							fcc -= n;
							goto top; /* n^2 */
						}
					}
				obits |= pmask;		/* try write */
			}
		}

		if ((obits & pmask) && fcc > 0) {
			cc = write(p, fbp, fcc);
			if (cc > 0) {
				fcc -= cc;
				fbp += cc;
			}
		}

		if (ibits & pmask) {
			pcc = read(p, pibuf, sizeof (pibuf));
			pbp = pibuf;
			if (pcc < 0 && errno == EWOULDBLOCK)
				pcc = 0;
			else if (pcc <= 0)
				break;
			else if (pibuf[0] == 0) {
				pbp++, pcc--;
				obits |= fmask;	/* try a write */
			} else {
				if (pkcontrol(pibuf[0])) {
					pibuf[0] |= oobdata[0];
					send(f, &pibuf[0], 1, MSG_OOB);
				}
				pcc = 0;
			}
		}
		if ((obits & fmask) && pcc > 0) {
			cc = write(f, pbp, pcc);
			if (cc < 0 && errno == EWOULDBLOCK) {
				/*
				 * Also shouldn't happen, but sometimes can
				 * if we have so much to write that the 1st
				 * time into sosend() it fails.
				 * Used to sleep for 5 seconds but now only
				 * for 1, and only if there is no more data
				 * to read from the tty.
				 */
				if (!(ibits & p)) {	/* 4.4lite */
					sleep(1);
				}
				continue;
			}
			if (cc > 0) {
				pcc -= cc;
				pbp += cc;
			}
		}
	}
}

void
cleanup()
{
	rmut();
	shutdown(netf, 2);
	exit(1);
}

static void
fatal(int f, char *msg)
{
	char buf[BUFSIZ];

	buf[0] = '\01';		/* error indicator */
	(void) sprintf(buf + 1, "rlogind: %s.\r\n", msg);
	(void) write(f, buf, strlen(buf));
	exit(1);
}

static void
fatalperror(int f, char *msg)
{
	char buf[BUFSIZ];

	(void) sprintf(buf, "%s: %s", msg, strerror(errno));
	fatal(f, buf);
}


#define SCPYN(a, b)	strncpy(a, b, sizeof(a))
#define SCMPN(a, b)	strncmp(a, b, sizeof(a))

static struct utmpx entry;

/* add ourself to utmp */
static void
adut(void)
{
	int f;
	cap_t ocap;
	cap_value_t cap_dac_write = CAP_DAC_WRITE;
	SCPYN(entry.ut_user, "rlogin");
	SCPYN(entry.ut_id, &line[(sizeof("/dev/tty") - 1)]);
	SCPYN(entry.ut_line, line+sizeof("/dev/")-1);
	entry.ut_pid = getpid();
	entry.ut_type = LOGIN_PROCESS;
	(void) time(&entry.ut_tv.tv_sec);

	setutxent();
	ocap = cap_acquire (1, &cap_dac_write);
	pututxline(&entry);
	cap_surrender (ocap);
	endutxent();

	ocap = cap_acquire (1, &cap_dac_write);
	updwtmpx(WTMPX_FILE, &entry);
	cap_surrender (ocap);
}

static void
rmut(void)
{
	int f;
	cap_t ocap;
	cap_value_t cap_dac_write = CAP_DAC_WRITE;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	static char satid[5], satline[33];

	SCPYN(entry.ut_user, "rlogin");
	SCPYN(entry.ut_id, &line[(sizeof("/dev/tty") - 1)]);
	SCPYN(entry.ut_line, line+sizeof("/dev/")-1);
	entry.ut_pid = getpid();
	entry.ut_type = DEAD_PROCESS;
	(void) time(&entry.ut_tv.tv_sec);

	setutxent();
	ocap = cap_acquire (1, &cap_dac_write);
	pututxline(&entry);
	cap_surrender(ocap);
	endutxent();

	ocap = cap_acquire (1, &cap_dac_write);
	updwtmpx(WTMPX_FILE, &entry);
	cap_surrender(ocap);
	strncpy(satid, entry.ut_id, sizeof(entry.ut_id));
	strncpy(satline, line, strlen(line));
	ocap = cap_acquire(1, &cap_audit_write);
	satvwrite(SAT_AE_IDENTITY, SAT_SUCCESS,
		  "rlogind|+|?|Remote logout id %s line %s",
		   satid, satline);
	cap_surrender(ocap);
}

static void
setup_term(int t)
{ 
	struct termio b;

	ioctl(t, TCGETA, &b);
	b.c_iflag &= ~(ISTRIP|ICRNL|INLCR|IGNCR|IXON|IXOFF|BRKINT);
	b.c_lflag &= ~(ICANON|ECHO|ISIG);
	b.c_oflag &= ~OPOST;
	b.c_oflag |= TAB3;
	b.c_cc[VMIN] = 1;
	b.c_cc[VTIME] = 1;
	b.c_line = LDISC1;
	ioctl(t, TCSETA, &b);
}
