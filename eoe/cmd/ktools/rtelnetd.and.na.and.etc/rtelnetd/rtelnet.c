/*
 *****************************************************************************
 *
 *        Copyright 1989, 1990, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use. 
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Description::
 *
 * 	Annex Reverse Telnet Daemon
 *	modified telnetd to provide host-pty/annex-port association
 *
 * Original Author: Paul Mattes		Created on: an impulse
 *

   Modified by curtis@sgi.com during 6/91
      Added support for inetd-like operation --
	Now takes as arguments --
	    a directory name,
	    a data file of relations between --
		pseudo-tty names,
		annex IP addresses,
		and port numbers;
	create the directory (if required);
	allocate enough pseudo-ttys for all the names in the data file;
	create new ptty slaves in "directory" with names from the data file;
	wait for an open on any one of those pttys;
	fork a child with the appropriate arguments to act as a telnet
	    transparent pipe to the indicated port on the indicated annex box;
	when the child exits, go back to listening for an open on that ptty.
    This program is used to provide access to many annex ports from a
    single machine without having an rtelnet process associated with the
    currently inactive ports.
    All code for non-SGI machines (ie: under #ifdef) has been removed.

 *
 *****************************************************************************
 */

#include "rtelnetd.h"
#define TELOPTS
#include <arpa/telnet.h>


/*
 *****************************************************************************
 *
 * The rest of the file is a rip-off from the telnetd daemon.
 *
 *****************************************************************************
 */
#define SENT	0
#define RCVD	1

#define	BELL	'\007'

#define T_RAW	1	/* control pre- and post-processing of chars on pty */
#define T_ECHO	2	/* control echoing of chars on pty */
#define T_CRMOD	4	/* control whether CR is echoed at CR-NL */

char	hisopts[256];
char	myopts[256];

char	doopt[] = { IAC, DO, '%', 'c', 0 };
char	dont[] = { IAC, DONT, '%', 'c', 0 };
char	will[] = { IAC, WILL, '%', 'c', 0 };
char	wont[] = { IAC, WONT, '%', 'c', 0 };

/*
 * I/O data buffers, pointers, and counters.
 */
char	ptyibuf[BUFSIZ], *ptyip = ptyibuf;
char	ptyobuf[BUFSIZ], *pfrontp = ptyobuf, *pbackp = ptyobuf;
char	netibuf[BUFSIZ], *netip = netibuf;
char	netobuf[BUFSIZ], *nfrontp = netobuf, *nbackp = netobuf;

int	net, pty;		/* fd's for net connection and for pty */
int	port_num;		/* port number requested on annex box */

struct	sockaddr_in sin = { AF_INET };
struct	sockaddr_in sin2 = { AF_INET };

int	pcc, ncc;
int	saved_errno;


start_connection(info)
	struct portdb_struct *info;
{
	struct hostent *host;

	if (info->child_pid = fork()) {
		if (debug)
			fprintf(stderr, "start_connection: child forked: %d\n",
					info->child_pid);
		info->state = PORTDB_ACTIVE;
		return;				/* to parent code */
	}
	(void)sginap(HZ);

	if (!debug) {
		sigset(SIGINT,  SIG_IGN);
		sigset(SIGCLD,  SIG_IGN);
		sigset(SIGHUP,  SIG_IGN);
	}

	sin.sin_addr.s_addr = inet_addr(info->annex_addr);
	if (sin.sin_addr.s_addr != -1) {
		sin.sin_family = AF_INET;
	} else {
		host = gethostbyname(info->annex_addr);
		if (host) {
			sin.sin_family = host->h_addrtype;
			bcopy(host->h_addr,
			      (caddr_t)&sin.sin_addr,
			      host->h_length);
			if (debug)
			  fprintf(stderr, "addr:%s\n", inet_ntoa(sin.sin_addr));
		} else {
			fprintf(stderr, "%s: unknown host: %s\n",
					progname, info->annex_addr);
			exit(1);
		}
	}

	pty = info->master_fd;		/* for easy access in child code */

	if (info->flags & PORTDB_TCP)
		port_num = info->port_number + PORT_BASE_TCP;
	else
		port_num = info->port_number + PORT_BASE_TELNET;

	sin.sin_port = htons((u_short)port_num);
 	if (debug)
		fprintf(stderr, "port_num:%d\n", port_num);
	if ((net = make_connection(info->slave_name)) < 0)
		exit(1);

	if (info->flags & PORTDB_TCP)
		do_tcp();
	else
		do_telnet();
	exit(0);
}

make_connection(name)
	char *name;
{
	int s, on = 1;
	char errmsg[128];

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("rtelnetd: socket");
		exit(1);
	}

	if (options & SO_DEBUG)
		if (setsockopt(s, SOL_SOCKET, SO_DEBUG, &on, sizeof(on)) < 0)
			perror("rtelnetd: setsockopt (SO_DEBUG)");

	if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0)
		perror("rtelnetd: setsockopt (SO_KEEPALIVE)");

	if (bind(s, (struct sockaddr *)&sin2, sizeof (struct sockaddr)) < 0)
		perror("rtelnetd: bind");

	if (connect(s, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
		sprintf(errmsg, "rtelnetd: connect(%s)", name);
		perror(errmsg);
		(void)close(s);
		return -1;
	}

#ifdef USE_FIONBIO
	(void)ioctl(s, FIONBIO, (caddr_t)&on);
#endif

	ptyip = ptyibuf;
	pfrontp = ptyobuf;
	pbackp = ptyobuf;

	netip = netibuf;
	nfrontp = netobuf;
	nbackp = netobuf;

	return s;
}



/*
 * Main loop for TELNET.  Select from pty and network, and
 * hand data to telnet receiver finite state machine.
 */
do_telnet()
{
	int on = 1;
	struct termio term;
	int net_errno, pty_errno;

#ifdef TIOCPKT
	if (ioctl(pty, TIOCPKT, (caddr_t)&on) < 0) {
		perror("ioctl(TIOCPKT on master pty)");
		exit(1);
	}
#endif

	telrcv(1);

#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif

	/*
	 * Negotiate binary mode, if requested
	 */
	if (binary) {
		dooption(TELOPT_BINARY);
		myopts[TELOPT_BINARY] = 1;
		willoption(TELOPT_BINARY);
	}

	pcc = 0; ncc = 0;

	if (debug > 2)
		fprintf(stderr,"fd: net=%d, pty=%d\n", net, pty);

	for (;;) {
		fd_set ibits, obits, xbits;
		register int c;
		int n_found;

		/*
		 * Never look for input if there's still
		 * stuff in the corresponding output buffer
		 */
		FD_ZERO(&ibits);
		FD_ZERO(&obits);
		FD_ZERO(&xbits);
		FD_SET(net, &xbits);
		FD_SET(pty, &xbits);
		if ((nfrontp - nbackp) || (pcc > 0))
			FD_SET(net, &obits);
		else
			FD_SET(pty, &ibits);

		if (pfrontp - pbackp)
			FD_SET(pty, &obits);
		else
			FD_SET(net, &ibits);

		if (ncc < 0 && pcc < 0)
			break;

		if (debug > 2)
			fprintf(stderr, "selecting: i=%#x, o=%#x, x=%#x\n",
				ibits.fds_bits[0], obits.fds_bits[0],
				xbits.fds_bits[0]);
		n_found = select(FD_SETSIZE, &ibits, &obits, &xbits,
					     (struct timeval *)0);
		if (n_found < 0) {
			if (errno == EINTR) {
				fprintf(stderr,"select interrupted\n");
				continue;
			}
			perror("rtelnetd: select");
			exit(1);
		}
		if (debug > 2)
			fprintf(stderr,"%d found: i=%#x, o=%#x, x=%#x\n",
					   n_found, ibits.fds_bits[0],
					   obits.fds_bits[0],
					   xbits.fds_bits[0]);
		if (n_found == 0)
			continue;

		/*
		 * Something to read from the network...
		 */
		if (FD_ISSET(net, &ibits) || FD_ISSET(net, &xbits)) {
			if (debug > 1)
				fprintf(stderr, "net: ");
			ncc = read(net, netibuf, BUFSIZ);
			if (debug > 1) {
				saved_errno = errno;
				fprintf(stderr, "returns %d errno %d\n",
						ncc, errno);
				errno = saved_errno;
			}
			if (ncc < 0 && errno == EWOULDBLOCK)
				ncc = 0;
			else {
				if (ncc <= 0) {
					net_errno = (ncc < 0) ? errno : 0;
					pty_errno = 0;
					break;
				}
				netip = netibuf;
			}
			if (debug > 4 && ncc > 0)
				datadump(netip, ncc);
		}

		/*
		 * Something to read from the pty...
		 */
		if (FD_ISSET(pty, &ibits) || FD_ISSET(pty, &xbits)) {
			if (debug > 1)
				fprintf(stderr, "pty: pcc was %d ", pcc);
			pcc = read(pty, ptyibuf, BUFSIZ);
			if (debug > 1) {
				saved_errno = errno;
				fprintf(stderr, "returns %d errno %d\n",
						pcc, errno);
				errno = saved_errno;
			}
			if (pcc < 0 && errno == EWOULDBLOCK)
				pcc = 0;
			else {
				if (pcc <= 0) {
					net_errno = 0;
					pty_errno = (pcc < 0) ? errno : 0;
					break;
				}
#ifdef TIOCPKT
				pcc--;
				ptyip = &ptyibuf[1];
#else
				ptyip = &ptyibuf[0];
#endif

				if (debug > 2) {
					fprintf(stderr, "pkt- %x, cnt- %d\n",
							ptyibuf[0], pcc);
					if (ptyibuf[0]) {
						ioctl(pty, TCGETA,
							   (caddr_t)&term);
						fprintf(stderr, "if- %x of- %x cf- %x lf- %x li- %d er- %x ki- %x\n",
							term.c_iflag,
							term.c_oflag,
							term.c_cflag,
							term.c_lflag,
							term.c_line,
							term.c_cc[VERASE],
							term.c_cc[VKILL]);
					}
				}
			}
			if (debug > 4 && pcc > 0)
				datadump(ptyip, pcc);
		}

		while (pcc > 0) {
			if ((&netobuf[BUFSIZ] - nfrontp) < 2)
				break;
			c = *ptyip++ & 0377, pcc--;
			*nfrontp++ = c;
			if (c == IAC)
				*nfrontp++ = c;
			else if (c == '\r' && !myopts[TELOPT_BINARY])
				*nfrontp++ = '\0';
		}

		if (FD_ISSET(net, &obits) && (nfrontp - nbackp) > 0)
			netflush();
		if (ncc > 0)
			telrcv(0);
		if ((pfrontp - pbackp) > 0)
			ptyflush();
		if (FD_ISSET(pty, &obits) && (pfrontp - pbackp) > 0)
			ptyflush();
	}

	if (debug > 1) {
		fprintf(stderr,
			"out of loop pcc=%d pty_errno=%d ncc=%d net_errno=%d\n",
			pcc, pty_errno, ncc, net_errno);
	}

	/*
	 * I/O error from pty or it looks like somebody closed the slave device.
	 * Simply exit.
	 */
	if (pty_errno
#ifdef BROKEN_NET
/*
 * some systems don't correctly indicate close of the connection!
 */
	    || !net_errno
#endif
	) {
		send_tm(net);
#ifdef RESET_ANNEX_PORT
		if (hangup) {		/* we also need to set_global_passwd */
			(void)reset_line( (struct sockaddr_in *)&sin,
					  (u_short)SERIAL_DEV,
					  (u_short)port_num);
		}
#endif
	}
	if (shutdown(net, 2) == -1)	/* Telnet connection down */
		perror("shutdown");
	(void)close(net);
}

/*
 * State for recv fsm
 */
#define	TS_DATA		0	/* base state */
#define	TS_IAC		1	/* look for double IAC's */
#define	TS_CR		2	/* CR-LF ->'s CR */
#define	TS_BEGINNEG	3	/* throw away begin's... */
#define	TS_ENDNEG	4	/* ...end's (suboption negotiation) */
#define	TS_WILL		5	/* will option negotiation */
#define	TS_WONT		6	/* wont " */
#define	TS_DO		7	/* do " */
#define	TS_DONT		8	/* dont " */

telrcv(init_it)
int init_it;
{
	register int c;
	static int state = TS_DATA;
	struct termio term;

	if (init_it) {
		state = TS_DATA;
		return;
	}

	while (ncc > 0) {
		if ((&ptyobuf[BUFSIZ] - pfrontp) < 2)
			return;
		c = *netip++ & 0377, ncc--;

		switch (state) {

		case TS_DATA:
			if (c == IAC) {
				state = TS_IAC;
				break;
			}
			*pfrontp++ = c;
			if (!hisopts[TELOPT_BINARY] && c == '\r')
				state = TS_CR;
			break;

		case TS_CR:
			if (c && c != 0)
				*pfrontp++ = c;
			state = TS_DATA;
			break;

		case TS_IAC:
			switch (c) {

			/*
			 * Send the process on the pty side an
			 * interrupt.  Do this with a NULL or
			 * interrupt char; depending on the tty mode.
			 */
			case BREAK:
			case IP:
				interrupt();
				break;

			/*
			 * Are You There?
			 */
			case AYT:
				*pfrontp++ = BELL;
				break;

			/*
			 * Erase Character and
			 * Erase Line
			 */
			case EC:
			case EL:
				ptyflush();	/* half-hearted */
				ioctl(pty, TCGETA, (caddr_t)&term);
				*pfrontp++ = (c == EC) ?
					term.c_cc[VERASE] : term.c_cc[VKILL];
				break;

			/*
			 * Check for urgent data...
			 */
			case DM:
				break;

			/*
			 * Begin option subnegotiation...
			 */
			case SB:
				state = TS_BEGINNEG;
				continue;

			case WILL:
			case WONT:
			case DO:
			case DONT:
				state = TS_WILL + (c - WILL);
				continue;

			case IAC:
				*pfrontp++ = c;
				break;
			}
			state = TS_DATA;
			break;

		case TS_BEGINNEG:
			if (c == IAC)
				state = TS_ENDNEG;
			break;

		case TS_ENDNEG:
			state = c == SE ? TS_DATA : TS_BEGINNEG;
			break;

		case TS_WILL:
			printoption(RCVD, "will", c, !hisopts[c]);
			if (!hisopts[c])
				willoption(c);
			state = TS_DATA;
			continue;

		case TS_WONT:
			printoption(RCVD, "wont", c, hisopts[c]);
			if (hisopts[c])
				wontoption(c);
			state = TS_DATA;
			continue;

		case TS_DO:
			printoption(RCVD, "do", c, !myopts[c]);
			if (!myopts[c])
				dooption(c);
			state = TS_DATA;
			continue;

		case TS_DONT:
			printoption(RCVD, "dont", c, myopts[c]);
			if (myopts[c]) {
				myopts[c] = 0;
				sprintf(nfrontp, wont, c);
				nfrontp += sizeof (wont) - 2;
			}
			state = TS_DATA;
			continue;

		default:
			fprintf(stderr,"rtelnetd: panic: state=%d\n", state);
			exit(1);
		}
	}
}

send_tm(f)
	int f;
{
	register int c, cc;
	int off = 0;
	int state = TS_DATA;
	char buff[1024], *p;

	sprintf(buff, doopt, TELOPT_TM);
	cc = strlen(buff);
#ifdef USE_FIONBIO
	ioctl(f, FIONBIO, (caddr_t)&off);	/* blocking reads on closing */
#endif
	if (debug > 1)
		fprintf(stderr,"sending tm\n");
	write(f, buff, cc);
	if (debug > 1)
		fprintf(stderr,"sent tm\n");
	while ((cc = read(f, buff, sizeof(buff))) > 0) {
		p = buff;
		while (cc > 0) {
			c = *p++ & 0377, cc--;
			switch (state) {

			case TS_DATA:
				if (c == IAC) {
					state = TS_IAC;
					break;
				}
				break;

			case TS_IAC:
				switch (c) {
				case WILL:
				case WONT:
				case DO:
				case DONT:
					state = TS_WILL + (c - WILL);
					continue;
				}
				state = TS_DATA;
				break;

			case TS_WILL:
			case TS_WONT:
				if (c == TELOPT_TM)
				return;

			case TS_DO:
			case TS_DONT:
				state = TS_DATA;
				continue;

			default:
				fprintf(stderr, "send_tm: panic state=%d\n",
					state);
				exit(1);
			}
		}
	}
}

willoption(option)
	int option;
{
	char *fmt;

	switch (option) {

	case TELOPT_BINARY:
		mode(T_RAW, 0);
		goto common;

	case TELOPT_ECHO:
		mode(0, T_ECHO|T_CRMOD);
		/*FALL THRU*/

	case TELOPT_SGA:
	common:
		hisopts[option] = 1;
		fmt = doopt;
		break;

	case TELOPT_TM:
		fmt = dont;
		break;

	default:
		fmt = dont;
		break;
	}
	printoption(SENT, fmt == doopt ? "do" : "don't", option, 0);
	sprintf(nfrontp, fmt, option);
	nfrontp += sizeof (dont) - 2;
}

wontoption(option)
	int option;
{
	char *fmt;

	switch (option) {

	case TELOPT_ECHO:
		mode(T_ECHO|T_CRMOD, 0);
		goto common;

	case TELOPT_BINARY:
		mode(0, T_RAW);
		/*FALL THRU*/

	case TELOPT_SGA:
	common:
		hisopts[option] = 0;
		fmt = dont;
		break;

	default:
		fmt = dont;
	}
	printoption(SENT, fmt == doopt ? "do" : "don't", option, 0);
	sprintf(nfrontp, fmt, option);
	nfrontp += sizeof (doopt) - 2;
}

dooption(option)
	int option;
{
	char *fmt;

	switch (option) {

	case TELOPT_TM:
		fmt = wont;
		break;

	case TELOPT_ECHO:
		mode(T_ECHO|T_CRMOD, 0);
		goto common;

	case TELOPT_BINARY:
		mode(T_RAW, 0);
		/*FALL THRU*/

	case TELOPT_SGA:
	common:
		fmt = will;
		break;

	default:
		fmt = wont;
		break;
	}
	printoption(SENT, fmt == will ? "will" : "won't", option, 0);
	sprintf(nfrontp, fmt, option);
	nfrontp += sizeof (doopt) - 2;
}

mode(on, off)
	int on, off;
{
	struct termio term;

	ptyflush();
	ioctl(pty, TCGETA, (caddr_t)&term);
	if (on & T_RAW) {
		term.c_lflag |= ICANON;
		term.c_oflag |= OPOST;
	}
	if (on & T_ECHO) {
		term.c_lflag |= ECHO;
	}
	if (on & T_CRMOD) {
		term.c_iflag |= ICRNL;
	}
	if (off & T_RAW) {
		term.c_lflag &= ~ICANON;
		term.c_oflag &= ~OPOST;
	}
	if (off & T_ECHO) {
		term.c_lflag &= ~ECHO;
	}
	if (off & T_CRMOD) {
		term.c_iflag &= ~ICRNL;
	}
	ioctl(pty, TCSETA, (caddr_t)&term);
}

/*
 * Send interrupt to process on other side of pty.
 * If it is in raw mode, just write NULL;
 * otherwise, write intr char.
 */
interrupt()
{
	struct termio term;

	ptyflush();	/* half-hearted */
	ioctl(pty, TCGETA, &term);
	if (!(term.c_lflag & ISIG) ) {
		*pfrontp++ = '\0';
		return;
	}
	*pfrontp++ = term.c_cc[VQUIT];
}

ptyflush()
{
	int n;

	if ((n = pfrontp - pbackp) > 0)
		n = write(pty, pbackp, n);

	if (n < 0)
		return;
	pbackp += n;
	if (pbackp == pfrontp)
		pbackp = pfrontp = ptyobuf;
}

netflush()
{
	int n;

	if ((n = nfrontp - nbackp) > 0) {
		n = write(net, nbackp, n);
	}
	if (n < 0) {
		if (errno == EWOULDBLOCK)
			return;
		/* should blow this guy away... */
		return;
	}
	nbackp += n;
	if (nbackp == nfrontp)
		nbackp = nfrontp = netobuf;
}

printoption(dir, what, which, reply)
int dir;
char *what;
char which;
int reply;
{
	char *s_witch, ssb[32];

	if (debug<2)
		return;

	sprintf(ssb, "%d", which);

	if (which < 0 || which >= sizeof(telopts) / sizeof(telopts[0]))
		s_witch = ssb;
	else
		s_witch = telopts[which];

	if (dir == SENT)
		fprintf(stderr,"SENT %s %s\n", what, s_witch);
	else
		fprintf(stderr,"RCVD %s %s (%sreply)\n",
			what, s_witch, reply ? "" : "don't ");
}

void
datadump(p, len)
u_char *p;
int len;
{
	int i;
	u_char *s = (u_char *)(~0xf & (u_long)p);

	len += p-s;
	while (len > 0) {
		u_char *s1 = s;
		fprintf(stderr, (s<=p) ? "%6x:%-3x" : "%6x:   ",
				(s<=p) ? p : s, len - (p-s));
		for (i = 0; i < 16; i++, s++)
			fprintf(stderr, ((s<p)||(i>=len)) ? "%s   " : "%s%.2x ",
					(i % 8) ? "" : " ", *s);
		s = s1;
		fprintf(stderr," |");
		for (i = 0; i < 16; i++, s++)
			fprintf(stderr, "%c", ((s<p)||(i>=len))
					? ' '
					: ((*s<' ')||(*s>'~'))
					    ? '.' : *s);
		len -= 16;
		fprintf(stderr,"|\n");
	}
}
