static char USMID[] = "@(#)tcp/usr/etc/nettest/nettest.c	80.6	11/03/92 17:12:29";
/*
 * Copyright 1992 Cray Research, Inc.
 * All Rights Reserved.
 */
/*
 * Permission to use, copy, modify and distribute this software, in
 * source and binary forms, and its documentation, without fee is
 * hereby granted, provided that:  1) the above copyright notice and
 * this permission notice appear in all source copies of this
 * software and its supporting documentation; 2) distributions
 * including binaries display the following acknowledgement:  ``This
 * product includes software developed by Cray Research, Inc.'' in
 * the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of
 * this software; 3) the name Cray Research, Inc. may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission; 4) the USMID revision line and
 * binary copyright notice are retained without modification in all
 * source and binary copies of this software; 5) the software is
 * redistributed only as part of a bundled package and not as a
 * separate product (except that it may be redistibuted separately if
 * if no fee is charged); and 6) this software is not renamed in any
 * way and is referred to as Nettest.
 *
 * THIS SOFTWARE IS PROVIDED AS IS AND CRAY RESEARCH, INC.
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL CRAY RESEARCH, INC. BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
char copyright[] =
"@(#) Copyright 1992 Cray Research, Inc.\n\
 All rights reserved.\n";

#define SRCRT
#include "nettest.h"
#ifdef BSD44
#include <machine/endian.h>
#endif
#if defined(BSD44) || defined(sun) || defined(ultrix) || defined(sgi)
#include <netinet/in_systm.h>
#endif
#include <sys/wait.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <sys/uio.h>
#include <sys/signal.h>

#ifndef NO_ISO
#include <netiso/iso.h>
#include <netiso/tp_user.h>
#endif /* NO_ISO */

#ifdef	CRAY
# include <time.h>
# ifndef HZ
#  define HZ CLK_TCK
# endif
#endif

#define	D_PIPE	1
#define	D_UNIX	2
#define	D_INET	3
#define	D_FILE	4
#define D_ISO	5

union {
	struct sockaddr		d_gen;
	struct sockaddr_in	d_inet;
	struct sockaddr_un	d_unix;
#ifndef NO_ISO
	struct sockaddr_iso	d_iso;
#endif /* NO_ISO */
} name;
int	namesize;
int	domain = D_INET;

int	nchunks = NCHUNKS;		/* 100 */
int	chunksize = CHUNK;		/* 4096 */
int	dflag = 0;
int	checkdata = 0;
int	seqdata = 0;
int	hash = 0;
int	fullbuf = 0;
int	kbufsize = 0;
int	nodelay = 0;
int	mesghdr = 0;

long	times();
#if	!defined(CRAY) && !defined(SYSV) && !defined(sgi)
#define	GETTIMES(a, b)	ftime(&a); times(&b);
#define	TIMETYPE	struct timeb
#else
#define	GETTIMES(a, b)	a = times(&b);
#define	TIMETYPE	long
#endif
#ifdef	TCP_WINSHIFT
int	winshift;
int	usewinshift;
#endif
int	tos;
int	maxchildren;
int	waitall;
int	sync_children = 0;
char	UT_MAGIC[8] = {	'U', 't', 'i', 'm', 'e', 's', '.', '.' };
struct magic_tms {
	char mt_magic[8];
	struct tms mt_tms;
	TIMETYPE mt_start;
	TIMETYPE mt_end;
};

struct in_addr hisaddr;

char	*hisname, _myname[256], *myname = _myname;

#ifndef	NO_ISO
struct	sockaddr_iso to_s = {sizeof(to_s), AF_ISO};
#endif

void do_children(), do_stream(), usage(), do_dgram(), prtimes();

int read(), recv();

int (*rfunc)() = read;

main(argc, argv)
	int argc;
	char **argv;
{
	register int	s, s2, port = PORTNUMBER;
	struct hostent	*gethostbyname(), *hp;
	char		*destname;
	register char	*portname = 0;
	int		type = SOCK_STREAM;
	int		i;
	int		on = 1;
	int		nconnections = 1;
#if	defined(SRCRT) && defined(IPPROTO_IP)
	char *srp = 0, *strrchr();
	unsigned long sourceroute(), srlen;
#endif
#ifndef	NO_ISO
	struct	sockaddr_iso *to = &to_s;
	union {
		int	port;
		char	data[sizeof(int)];
	} portnumber;
	struct hostinfo *hi;
#endif
	extern char	*optarg;
	extern int	optind;

	/*
	 * Set max children to allow for all the pipes to be
	 * created.  We assume that we will have one pipe open
	 * per child, plus stdin/stdout/stderr, plus one extra
	 * file descriptor for when we create the pipe.  So, we
	 * subract 4 from the maximum number of file descriptors
	 * to get the maximum number of children.
	 */
	maxchildren = getdtablesize() - 4;

	while ((i = getopt(argc, argv, "b:cCdfFhmn:p:s:S:t:Vw?")) != EOF) {
		switch(i) {
		case 'b':	/* kernel socket buffer size */
			kbufsize = atoval(optarg);
			break;
		case 'c':	/* check the data for correctness */
			++checkdata;
			break;
		case 'C':	/* use a sequential data pattern */
			++checkdata;	/* implies -c option */
			++seqdata;
			break;
		case 'd':	/* turn on socket debugging */
			dflag++;
			break;
		case 'f':	/* always post full buffers for reads */
			fullbuf++;
			break;
		case 'F':	/* turn off the NODELAY bit for TCP */
#ifdef	TCP_NODELAY
			nodelay++;
#else
			fprintf(stderr, "TCP nodelay option not supported\n");
			usage();
#endif
			break;
		case 'h':	/* show progress of data transfer */
			++hash;
			break;
		case 'm':	/* use sendmsg() instead of sendto() */
			++mesghdr;
			break;
		case 'n':	/* # connections to the destination */
			nconnections = atoi(optarg);
			if (nconnections < 1 || nconnections > maxchildren) {
				fprintf(stderr,
					"-n: value must be between 1 and %d\n",
					maxchildren);
				usage();
			}
			break;
		case 'p':	/* protocol to use */
			if (!strcmp(optarg, "tcp")) {
				domain = D_INET;
				type = SOCK_STREAM;
			} else if (!strcmp(optarg, "iso")) {
#ifndef	NO_ISO
				domain = D_ISO;
				type = SOCK_SEQPACKET;
#else	/* NO_ISO */
				fprintf(stderr, "iso protocol not supported\n");
				usage();
#endif /* NO_ISO */
			} else if (!strcmp(optarg, "udp")) {
				domain = D_INET;
				type = SOCK_DGRAM;
				nchunks = 1;
			} else if (!strcmp(optarg, "unix")) {
				domain = D_UNIX;
				type = SOCK_STREAM;
			} else if (!strcmp(optarg, "unixd")) {
				domain = D_UNIX;
				type = SOCK_DGRAM;
			} else if (!strcmp(optarg, "pipe")) {
				domain = D_PIPE;
				type = SOCK_STREAM;
			} else if (!strcmp(optarg, "file")) {
				domain = D_FILE;
				type = SOCK_STREAM;
			} else {
				fprintf(stderr, "Unknown protocol: %s\n", optarg);
				usage();
			}
			break;
		case 's':	/* Use the TCP window shift option */
#ifdef	TCP_WINSHIFT
			usewinshift++;
			winshift = atoi(optarg);
			if (winshift < -1 || winshift > 14) {
				fprintf(stderr, "window shift (-s) must be beteen -1 and 14\n");
				usage();
			}
#else
			fprintf(stderr, "window shift option not supported\n");
			usage();
#endif
			break;
		case 'S':	/* Set the IP Type-of-Service bits */
		case 't':	/* old flag for setting TOS... */
#ifdef	IP_TOS
			if ((tos = parsetos(optarg, "*")) < 0) {
				fprintf(stderr, "Bad tos argument: '%s'\n", optarg);
				usage();
			}

			break;
#else
			fprintf(stderr, "TOS (-%c) option not supported\n", (char)i);
			usage();
#endif
		case 'V':	/* Print the version of this program */
			printf("%s\n%s", &USMID[4], &copyright[4]);
			exit(0);

		case 'w':	/* use the MSG_WAITALL flag on recv() */
#ifdef	MSG_WAITALL
			waitall = MSG_WAITALL;
			rfunc = recv;
#else
			fprintf(stderr, "%s%s\n",
				"MSG_WAITALL (-w) not supported locally, ",
					"but will be passed on to server.");
			waitall = 1;	/* will be re-set to 0 below */
#endif
			break;

		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	/*
	 * Verify consitency of the specified options
	 */
	if (nodelay && ((domain != D_INET) || type != SOCK_STREAM)) {
		fprintf(stderr, "-F flag ignored (only valid for tcp\n");
		nodelay = 0;
	}
	if (mesghdr && (type != SOCK_DGRAM)) {
		fprintf(stderr,
			"-m flag ignored (only valid for udp and unixd\n");
	}
	if (tos && (domain != D_INET)) {
		fprintf(stderr,
			"tos value ignored (only valid for tcp and udp)\n");
		tos = 0;
	}
	if (waitall && ((domain == D_PIPE) ||
			(domain == D_FILE) ||
			(type == SOCK_DGRAM))) {
		fprintf(stderr,
#ifndef	NO_ISO
		    "-w flag ignored (only valid for tcp, iso and unix)\n"
#else
		    "-w flag ignored (only valid for tcp and unix)\n"
#endif
		);
	}
	if (checkdata &&
#ifndef	NO_ISO
			(type != SOCK_SEQPACKET) &&
#endif
			(type != SOCK_STREAM)) {
		fprintf(stderr, "-%c flag ignored (only valid for %s)\n",
		    seqdata ? 'C' : 'c',
#ifndef	NO_ISO
		    "tcp, iso, unix, pipe, and file"
#else
		    "tcp, unix, pipe, and file"
#endif
		);
	}
		

	/*
	 * Pick up the port/file name(s)
	 */
	if ((domain == D_INET) || (domain == D_ISO)) {
		hisname = myname;
		if (argc) {
			if (strcmp(*argv, "-"))
				hisname = *argv;
			--argc; ++argv;
		}
	} else if (domain == D_FILE) {
		if (argc < 2)
			usage();
		myname = *argv++;
		hisname = *argv++;
		argc -= 2;
	}

	if (argc) {
		if (strcmp(*argv, "-"))
			nchunks = atoval(*argv);
		--argc; ++argv;
		if (argc) {
			if (strcmp(*argv, "-"))
				chunksize = atoval(*argv);
			--argc; ++argv;
			if (argc) {
				if (strcmp(*argv, "-")) {
					port = atoi(*argv);
					portname = *argv;
				}
				if (argc > 1)
					usage();
			}
		}
	}


	switch(domain) {
	case D_PIPE:
		if (portname == 0)
			portname = PIPENAME;
		if (nconnections > 1) {
			fprintf(stderr, "-n flag not supported for pipe\n");
			usage();
		}
		sprintf(myname, "%sW", portname);
		if ((s2 = open(myname, 1)) < 0) {
			perror(myname);
			exit(1);
		}
		sprintf(myname, "%sR", portname);
		if ((s = open(myname, 0)) < 0) {
			perror(myname);
			close(s2);
			exit(1);
		}
		break;
	case D_FILE:
		if (nconnections > 1) {
			fprintf(stderr, "-n flag not supported for file\n");
			usage();
		}
		s2 = open(myname, 1);
		if (s2 < 0) {
			perror(myname);
			exit(1);
		}
		s = open(hisname, 0);
		if (s < 0) {
			perror(hisname);
			exit(1);
		}
		break;
	case D_UNIX:
		if (portname == 0)
			portname = (type == SOCK_DGRAM) ? UNIXDPORT : UNIXPORT;
		name.d_unix.sun_family = AF_UNIX;
		strcpy(name.d_unix.sun_path, portname);
		namesize = sizeof(name.d_unix) - sizeof(name.d_unix.sun_path)
			+ strlen(name.d_unix.sun_path);
		goto dosock;
		break;
	case D_INET:
		if (nconnections > 1 && type != SOCK_STREAM) {
			fprintf(stderr, "-n flag not supported for udp\n");
			usage();
		}
		namesize = sizeof(name.d_inet);
		name.d_inet.sin_family = AF_INET;
		gethostname(_myname, sizeof(_myname));
#if	defined(SRCRT) && defined(IPPROTO_IP)
		if (*hisname == '@' || *hisname == '!') {
			int temp;
			if ((destname = strrchr(hisname, ':')) == NULL)
				destname = strrchr(hisname, '@');
			destname++;
			srp = 0;
			temp = sourceroute(hisname, &srp, &srlen);
			if (temp == 0) {
#ifndef sun
				herror(srp);
#else
				printf("Can't get source route: %s\n", srp);
#endif
				return 0;
			} else if (temp == -1) {
				printf("Bad source route option: %s\n",
					hisname);
				return 0;
			} else {
#if	!defined(CRAY) || defined(s_addr)
				name.d_inet.sin_addr.s_addr = temp;
#else
				name.d_inet.sin_addr = temp;
#endif
			}
		} else {
#endif
			if ((hp = gethostbyname(hisname)) == NULL) {
				long tmp;

				tmp = inet_addr(hisname);
				if (tmp == -1) {
					fprintf(stderr, "no host entry fo %s\n", hisname);
					exit(1);
				}
#if	!defined(CRAY) || defined(s_addr)
				name.d_inet.sin_addr.s_addr = tmp;
#else
				name.d_inet.sin_addr = tmp;
#endif
			} else {
#if	!defined(CRAY) || defined(s_addr)
				bcopy(hp->h_addr, (char *)&name.d_inet.sin_addr,
								hp->h_length);
#else
				long	tmp;
				bcopy(hp->h_addr, (char *)&tmp, hp->h_length);
				name.d_inet.sin_addr = tmp;
#endif
			}
		}
		name.d_inet.sin_port = htons(port);
		goto dosock;
		break;
#ifndef NO_ISO
	case D_ISO:
		gethostname(_myname, sizeof(_myname));
		if (hisname != myname) {
			if ((hi = gethostinfo(hisname, 0, AF_ISO, 0, 0)) == 0) {
				perror("gethostinfo(AF_ISO)");
				exit(1);
			}
			to = (struct sockaddr_iso *)(*hi->h_addr_serv)->hs_addr;
		}
		name.d_iso = *to;
		namesize = sizeof(name.d_iso);
		name.d_iso.siso_tlen = 2;
		portnumber.port = htons(port);
		bcopy(&(portnumber.data[(sizeof(int)-2)]),
						TSEL(&(name.d_iso)), 2);
#endif /* NO_ISO */
	dosock:
		if (nconnections > 1)
			do_children(nconnections);
		s = socket(name.d_gen.sa_family, type, 0);
		if (s < 0) {
			perror("socket");
			exit(1);
		}
		if (dflag)
		   if (setsockopt(s, SOL_SOCKET, SO_DEBUG, &on, sizeof(on)) < 0)
			perror("setsockopt - SO_DEBUG");
#ifdef	IP_TOS
		if (tos)
		   if (setsockopt(s, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
			perror("setsockopt - IP_TOS");
#endif
		if (kbufsize) {
#ifdef	SO_SNDBUF
			if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &kbufsize,
							sizeof(kbufsize)) < 0)
				perror("setsockopt - SO_SNDBUF");
			if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &kbufsize,
							sizeof(kbufsize)) < 0)
				perror("setsockopt - SO_RCVBUF");
#else	/* !SO_SNDBUF */
			printf("-b: cannot set local buffer sizes\n");
#endif	/* SO_SNDBUF */
		}
#ifdef	TCP_WINSHIFT
		if (usewinshift) {
			if (setsockopt(s, IPPROTO_TCP, TCP_WINSHIFT, &winshift,
							sizeof(winshift)) < 0)
				perror("setsockopt - TCP_WINSHIFT");
		}
#endif
#ifdef	TCP_NODELAY
		if (nodelay) {
			if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &nodelay,
							sizeof(nodelay)) < 0)
				perror("setsockopt - TCP_NODELAY");
		}
#endif
#if	defined(SRCRT) && defined(IPPROTO_IP)
		if (srp && setsockopt(s, IPPROTO_IP, IP_OPTIONS,
						(char *)srp, srlen) < 0)
			perror("setsockopt (IP_OPTIONS)");
#endif
		if (type == SOCK_DGRAM) {
			do_dgram(s);
			shutdown(s, 2);
			exit(0);
		}
		if (connect(s, (char *)&name, namesize) < 0) {
			perror("connect");
			exit(1);
		}
		s2 = s;
		break;
	}

	printf("Transfer: %d*%d bytes", nchunks, chunksize);
	if ((domain == D_INET) || (domain == D_ISO))
		printf(" from %9s to %9s\n", myname, hisname);
	else
		printf("\n");

	do_stream(s, s2);
	if (s == s2)
		shutdown(s, 2);
	else {
		close(s);
		close(s2);
	}
}

struct children {
	int	c_read;
	int	c_write;
	int	c_pid;
	int	c_ready;
	TIMETYPE c_start, c_end;
};

#define	CHILD_READ(n)	(childrenp[n].c_read)
#define	CHILD_WRITE(n)	(childrenp[n].c_write)
#define	CHILD_PID(n)	(childrenp[n].c_pid)
#define	CHILD_READY(n)	(childrenp[n].c_ready)
#define	CHILD_START(n)	(childrenp[n].c_start)
#define	CHILD_END(n)	(childrenp[n].c_end)
#define	ARRAY_SIZE(n)	((n)*sizeof(struct children))

/*
 * When called, this routine will fork off "nconnections" children, and
 * return in each of the children.  The parent program will hang around
 * for the children to die, printing out their statistics (read off of
 * a pipe set up beforehand).
 */
	void
do_children(nconnections)
	int nconnections;
{
	register int i;
	int n;
	struct children *childrenp;
	char *malloc();
	int status, child_error = 0;
	int notready = 0, nchildren;
	struct tms	tms1, tms2;
	TIMETYPE	start, end;
	char buf[1024];
	int gottimes;

	bzero((char *)&start, sizeof(start));
	bzero((char *)&end, sizeof(end));
	bzero((char *)&tms1, sizeof(tms1));
	bzero((char *)&tms2, sizeof(tms2));

	sync_children = getpid();

	if (sync_children != getpgrp()) {
		(void) setpgid(sync_children, sync_children);
		sync_children = getpgrp();
	}

	childrenp = (struct children *)malloc(ARRAY_SIZE(nconnections));
	if (childrenp == NULL) {
		fprintf(stderr,
			"malloc() for data to keep track of children failed\n");
		exit(1);
	}
	bzero(childrenp, ARRAY_SIZE(nconnections));

	for (i = 0; i < nconnections; i++) {
		if (pipe(&CHILD_READ(i)) < 0) {
			fprintf(stderr, "Cannot create pipe for child %d\n", i);
			CHILD_READ(i) = -1;
			continue;
		}
		switch(n = fork()) {
		case 0:
			/*
			 * In the child.  Close down all the pipes, dup
			 * our pipe to stdout and stderr, and return.
			 */
			for (n = 0; n <= i; n++) {
				if (CHILD_READ(n) >= 0)
					close(CHILD_READ(n));
			}
			dup2(CHILD_WRITE(i), 1);
			dup2(CHILD_WRITE(i), 2);
			close(CHILD_WRITE(i));
			return;

		case -1:
			close(CHILD_READ(i));
			CHILD_READ(i) = -1;
			close(CHILD_WRITE(i));
			CHILD_WRITE(i) = -1;
			fprintf(stderr, "Child %d not started\n", i);
			break;

		default:
			close(CHILD_WRITE(i));
			CHILD_PID(i) = n;
			notready++;
			break;
		}
	}
	nchildren = notready;
	while ((n = waitpid(-1, &status, WUNTRACED)) >= 0) {

		for (i = 0; i < nconnections; i++) {
			if (CHILD_PID(i) == n)
				break;
		}
		if (i == nconnections) {
			fprintf(stderr, "Unknown child [pid %d] died.\n", n);
			continue;
		}
		/*
		 * Check for stopped children.  When they are all
		 * stopped, send the SIGCONT to all of them to
		 * fire them up.
		 */
		if (WIFSTOPPED(status)) {
			if (CHILD_READY(i) == 0) {
				CHILD_READY(i) = 1;
				if (--notready == 0)
					killpg(sync_children, SIGCONT);
			}
			continue;
		}
		/*
		 * Check for children that have died before
		 * suspending themselves.
		 */
		if (CHILD_READY(i) == 0) {
			CHILD_READY(i) = 1;
			if (--notready == 0)
				killpg(sync_children, SIGCONT);
		}
		/*
		 * Print out the childs statistics.
		 */

		printf("Child %d statistics:\n", i);
		fflush(stdout);
		gottimes = 0;
		while ((n = read(CHILD_READ(i), buf, sizeof(buf))) > 0) {
			gottimes += extract_times(buf, &n, &tms2, &start, &end,
					&CHILD_START(i), &CHILD_END(i));
			if (n > 0)
				(void) write(1, buf, n);
		}
		close(CHILD_READ(i));
		if (!gottimes || !WIFEXITED(status) || WEXITSTATUS(status))
			child_error++;
		CHILD_PID(i) = -1;
	}

	/*
	 * Print the aggregate statistics.
	 */
	printf("--------------------\n");
	printf("Aggregate statistics for %d of %d children:\n",
					nchildren, nconnections);
	if (child_error)
		printf("  (May not be accurate due to errors in children)\n");
	printf("Transfer: %d*%d bytes over %d streams",
				nchunks, chunksize, nchildren);
	if ((domain == D_INET) || (domain == D_ISO))
		printf(" from %9s to %9s\n", myname, hisname);
	else
		printf("\n");
	nchunks *= nchildren;	/* for total amount of data transfered */
	prtimes(&start, &end, 0, &tms1, &tms2, 0);

	printf("\nSyncronization information:\n");
	printf("Child:    Start Duration      End\n");
	for (i = 0; i < nconnections; i++) {
		float	offset, duration;
		if (CHILD_READY(i) == 0)
			continue;

#if	!defined(CRAY) && !defined(SYSV) && !defined(sgi)
		offset = (CHILD_START(i).time - start.time)*1000L
			 + CHILD_START(i).millitm - start.millitm;
		duration = (CHILD_END(i).time - CHILD_START(i).time)*1000L
			 + CHILD_END(i).millitm - CHILD_START(i).millitm;
#else
		offset = (float)(CHILD_START(i) - start)*1000.0/HZ;
		duration = (float)(CHILD_END(i) - CHILD_START(i))*1000.0/HZ;
#endif
		printf("%5d: %8.4f %8.4f %8.4f\n", i, offset/1000.0,
			duration/1000.0, (offset + duration)/1000.0);
	}

	exit(0);
}

/*
 * Extract client timing information from the
 * data stream.  It returns non-zero or zero to
 * inicate whether or not timing information was
 * extracted from the buffer.
 *
 * This routine assumes that if we get all the timing
 * information, or we get none of it.  If the data is
 * split across read()s it will not be extracted.
 */
	int
extract_times(buf, np, tmsp, startp, endp, cstartp, cendp)
	char *buf;
	int *np;
	struct tms *tmsp;
	TIMETYPE *startp, *endp, *cstartp, *cendp;
{
	static struct magic_tms tms;
	register char *c1;

	/*
	 * Scan the buffer for the magic string that
	 * designates the beginning of the timing information.
	 */
	for (c1 = buf; c1 <= buf + *np - sizeof(tms); c1++) {
		/*
		 * Quick check to aviod overhead of strncmp()
		 */
		if (*c1 != UT_MAGIC[0])
			continue;
		if (strncmp(c1, UT_MAGIC, 8) == 0) {
			goto gotit;
		}
	}
	/* indicate that we did not get timing information */
	return(0);

gotit:
	/*
	 * Pull the timing data out of the buffer, and
	 * adjust the length value that was passed in.
	 */
	bcopy(c1, (char *)&tms, sizeof(tms));
	bcopy(c1 + sizeof(tms), c1, *np - (c1 + sizeof(tms) - buf));
	*np -= sizeof(tms);

	/*
	 * Add in the timing information, and update the ending
	 * time if it is later than what we've gotten so far.
	 */
	tmsp->tms_utime += tms.mt_tms.tms_utime;
	tmsp->tms_stime += tms.mt_tms.tms_stime;
#if	!defined(CRAY) && !defined(SYSV) && !defined(sgi)
	if ((tms.mt_start.time < startp->time) ||
	    ((tms.mt_start.time == startp->time) &&
	     (tms.mt_start.millitm < startp->millitm)) ||
		((startp->time == 0) && (startp->millitm == 0)))
		*startp = tms.mt_start;
	if ((tms.mt_end.time > endp->time) ||
	    ((tms.mt_end.time == endp->time) &&
	     (tms.mt_end.millitm > endp->millitm)))
		*endp = tms.mt_end;
#else
	if ((tms.mt_start < *startp) || (*startp == 0))
		*startp = tms.mt_start;
	if (tms.mt_end > *endp)
		*endp = tms.mt_end;
#endif
	*cstartp = tms.mt_start;
	*cendp = tms.mt_end;
	/* indicate that we extracted the timing information */
	return(1);
}

	int
atoval(s)
	register char *s;
{
	register int retval;

	retval = atoi(s);
	while (*s >= '0' && *s <= '9')
		++s;
	if (*s == 'k' || *s == 'K')
		retval *= 1024;
	return(retval);
}

	void
do_stream(in, out)
	register int in, out;
{
	register int	i, t, j, offset = 0, t2;
	register char	*cp;
	char		buf[128], *data, *malloc(), *orgdata;
	long		*cnts;
	register long	*ldp;
	struct tms	tms1, tms2, tms3;
	TIMETYPE	start, turnaround, end;
	register unsigned long loval;
#ifndef	CRAY
	register unsigned long hival;
#endif

#ifndef	NO_ISO
	/* read ISO CC - 0 bytes */
	if (domain == D_ISO) {
		if ((i = read(in, buf, sizeof(buf))) != 0) {
			fprintf(stderr, "read(ISO CC) failed\n");
			exit(1);
		}
	}
#endif	/* NO_ISO */
	if (seqdata && chunksize&0x7) {
		printf("data size must be multiple of 8.  %d rounded to %d\n",
			chunksize, chunksize & ~0x7);
		chunksize &= ~0x7;
	}
	sprintf(buf, "%d %d %d %d %d %d %d %d\n", nchunks, chunksize, fullbuf,
		kbufsize, tos, nodelay, seqdata, waitall);
#ifndef	MSG_WAITALL
	waitall = 0;	/* so we don't screw up the recv() */
#endif
	if (write(out, buf, strlen(buf)) != strlen(buf)) {
		perror("write1");
		exit(1);
	}
	for (cp = buf; ; ) {
		i = read(in, cp, 1);
		if (i != 1) {
			if (i < 0)
				perror("read (waiting to verify setup)");
			else
				fprintf(stderr, "nettest:  Read returned %d, expected 1\n", i);
			exit(1);
		}
		if (*cp == '\n')
			break;
		cp++;
	}
	*cp = '\0';
	if (cp - buf > 2)
		printf("remote server: %s\n", &buf[1]);
	if (cp - buf > 1 && buf[0] == '0')
		exit(1);
	orgdata = data = malloc(chunksize+8);
	if (data == NULL) {
		fprintf(stderr, "cannot malloc enough data space\n");
		exit(1);
	}
	cnts = (long *)malloc((chunksize+1)*sizeof(long));
	if (cnts == NULL) {
		fprintf(stderr, "cannot malloc enough stats space\n");
		exit(1);
	}
	bzero(cnts, (chunksize+1)*sizeof(long));

	if (checkdata && !seqdata)
		for (cp = data, i = 0; i < chunksize; )
			*cp++ = *("abcdefghijklmnopqrstuvwxyzABCDEF" + (i++/8)%32);
	if (hash)
		write(0, "\r\nWrite: ", 9);

	loval = 0;
#ifndef	CRAY
	hival = 0;
#endif

	if (sync_children) {
		/*
		 * Don't start until all the children
		 * are ready to go.  The master client
		 * will restart us.
		 */
		kill(getpid(), SIGSTOP);	/* suspend ourself */
	}
	GETTIMES(start, tms1);

	for (i = 0; i < nchunks; i++) {
		if (seqdata) {
			ldp = (long *)data;
			for (j = 0; j < chunksize/8; j++) {
#ifndef	CRAY
				*ldp++ = htonl(hival);
				*ldp++ = htonl(loval);
				if (++loval == 0)
					++hival;
#else
				*ldp++ = loval++;
#endif
			}
		}
		if ((t = write(out, data, chunksize)) < 0) {
			sprintf(buf, "write #%d:", i+1);
			goto bad;
		}
		if (t != chunksize)
			fprintf(stderr, "write: %d got back %d\n", chunksize, t);
		if (hash)
			write(1, "#", 1);
	}

	GETTIMES(turnaround, tms2);

	if (hash)
		write(0, "\r\nRead:  ", 9);

	loval = 0;
#ifndef	CRAY
	hival = 0;
#endif

	for (i = 0; i < nchunks || offset; i++) {
		if ((t = (*rfunc)(in, data, chunksize, waitall)) < 0) {
			sprintf(buf, "read #%d.%d", i+1, chunksize);
			goto bad;
		}
		if (t == 0) {
			fprintf(stderr, "EOF on file, block # %d\n", i);
			break;
		}
		cnts[t]++;
		if (fullbuf) {
			t2 = offset;
			offset += t;
			if (offset >= chunksize) {
				offset -= chunksize;
				if (hash)
					write(1, "#", 1);
			} else {
				--i;
				if (hash)
					write(1, ".", 1);
			}
		} else {
			while (t != chunksize) {

				if (hash)
					write(1, ".", 1);
				if ((t2 = (*rfunc)(in, data+t, chunksize-t,
							waitall)) < 0) {
					sprintf(buf, "read #%d.%d", i,
								chunksize-t);
					goto bad;
				}
				if (t2 == 0) {
					fprintf(stderr,
						"EOF on file, block # %d\n", i);
					break;
				}
				cnts[t2]++;
				t += t2;
			}
			if (hash)
				write(1, "#", 1);
			t2 = 0;
		}
		if (!checkdata)
			continue;
		if (!seqdata) {
		    for (cp = data, j = 0; j < t; cp++, j++) {
			if (*cp != *("abcdefghijklmnopqrstuvwxyzABCDEF" +
							((j+t2)/8)%32)) {
			    fprintf(stderr, "%d/%d(%d): got %d, expected %d\n",
				i, (j+t2), i*chunksize + (j+t2), *cp,
				*("abcdefghijklmnopqrstuvwxyzABCDEF" +
							((j+t2)/8)%32));
			}
		    }
		    continue;
		}
		t += data - orgdata;
		data = orgdata;
		ldp = (long *)data;
		for (j = 0; j < t/8; j++) {
#ifndef	CRAY
			t2 = ntohl(*ldp++) != hival;
			if ((ntohl(*ldp++) != loval) || t2) {
				printf("expected %8x%8x, got %8x%8x\n",
					hival, loval, ntohl(*(ldp-2)),
						ntohl(*(ldp-1)));
				hival = ntohl(*(ldp-2));
				loval = ntohl(*(ldp-1));
			}
			if (++loval == 0)
				++hival;
#else
			if (*ldp++ != loval) {
				printf("expected %16x, got %16x\n",
					loval, *(ldp-1));
				loval = *(ldp-1);
			}
			++loval;
#endif
		}
		t -= j*8;
		if (t) {
			*(long *)orgdata = *ldp;
#ifndef	CRAY
			*(((long *)orgdata) + 1) = *(ldp+1);
#endif
			data = orgdata + t;
		}
	}

	GETTIMES(end, tms3);

	if (sync_children) {
		/*
		 * We are a slave of the master client.  Write out
		 * a magic cookie and then the time it took to do
		 * the data transfer.
		 *
		 * Note that we do this before printing out the
		 * statistics, so that it it will be at the beginning
		 * of the data read by the master client, and it
		 * won't have to worry about this data getting
		 * split across read()s.
		 */
		struct magic_tms td;

		strncpy(td.mt_magic, UT_MAGIC, 8);
		td.mt_tms.tms_utime = tms3.tms_utime - tms1.tms_utime;
		td.mt_tms.tms_stime = tms3.tms_stime - tms1.tms_stime;
		td.mt_tms.tms_cutime = 0;
		td.mt_tms.tms_cstime = 0;
		td.mt_start = start;
		td.mt_end = end;
		write(1, &td, sizeof(td));
	}

	prtimes(&start, &turnaround, &end, &tms1, &tms2, &tms3);

	j = 0;
	for (i = 0; i <= chunksize; i++)
		if (cnts[i]) {
			printf("%6d: %5d ", i, cnts[i]);
			if (++j == 4) {
				printf("\n");
				j = 0;
			}
		}
	if (j)
		printf("\n");
#ifndef	NO_ISO
	/* send out ISO sync data to server */
	if (domain == D_ISO) {
		strncpy(data, "DONE", 4);
		write(out, data, 4);
		sleep(1);	/* wait for packet to reach server */
	}
#endif /* NO_ISO */
	return;
bad:
	perror(buf);
	exit(1);
}

	void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
"Usage: nettest [-cdfFh] [-b bufsize] [-s winshift] [-S tos] [-n #streams]",
"               [-p tcp|udp|iso] [host [count [size [port]]]]",
"       nettest [-cdfh] [-b bufsize] -p unix|pipes [-n #streams]",
"               [count [size [filename]]]",
"       nettest [-cdfh] [-b bufsize] -p file file1 file2 [count [size]]",
"       nettest -V\n");
	exit(1);
}

	void
do_dgram(s)
	register int s;
{
	register int	ret, i;
	register char	*data;
	char		*malloc();
	struct tms	tms1, tms2;
	TIMETYPE	start, end;
	struct msghdr	outmsg;
	struct iovec	iov;

	data = malloc(chunksize);
	if (data == NULL) {
		fprintf(stderr, "cannot malloc enough space\n");
		exit(1);
	}

	iov.iov_base = data;
	iov.iov_len = chunksize;
	bzero((char *)&outmsg, sizeof(outmsg));
	outmsg.msg_iov = &iov;
	outmsg.msg_iovlen = 1;
	outmsg.msg_name = (caddr_t)&name;
	outmsg.msg_namelen = namesize;
	GETTIMES(start, tms1);


	*data = 0;
	for (i = 0; i < nchunks; i++) {
		ret = mesghdr ? sendmsg(s, &outmsg, 0)
			: sendto(s, data, chunksize, 0, (caddr_t)&name,
						namesize);

		if (ret < 0) {
			perror(mesghdr ? "sendmsg" : "sendto");
			printf("%d out of %d sent\n", i, nchunks);
			exit(1);
		}
		if (ret != chunksize)
			printf("%s returned %d, expected %d\n",
				mesghdr ? "sendmsg" : "sendto", ret, chunksize);
		(*data)++;
	}

	GETTIMES(end, tms2);

	/*
	 * Pass in same middle and end times because we only do writes for udp.
	*/
	prtimes(&start, &end, &end, &tms1, &tms2, &tms2);
}

	void
prtimes(p0, p1, p2, tms1, tms2, tms3)
	TIMETYPE	*p0, *p1, *p2;
	struct tms	*tms1, *tms2, *tms3;
{
	float	t1, t2;

#if	!defined(CRAY) && !defined(SYSV) && !defined(sgi)
	t1 = (p1->time - p0->time)*1000L
		 + p1->millitm - p0->millitm;
	t2 = (p2 ? ((p2->time - p1->time)*1000L
		 + p2->millitm - p1->millitm) : 0.0);
#else
	t1 = (float)(*p1 - *p0)*1000.0/HZ;
	t2 = (p2 ? ((float)(*p2 - *p1)*1000.0/HZ) : 0.0);
#endif
	tms1->tms_utime = tms2->tms_utime - tms1->tms_utime;
	tms1->tms_stime = tms2->tms_stime - tms1->tms_stime;
	tms2->tms_utime = tms3 ? (tms3->tms_utime - tms2->tms_utime) : 0;
	tms2->tms_stime = tms3 ? (tms3->tms_stime - tms2->tms_stime) : 0;
	printf("           Real  System            User          Kbyte   Mbit(K^2) mbit(1+E6)\n");
#define FORMAT "%7s %7.4f %7.4f (%4.1f%%) %7.4f (%4.1f%%) %7.2f %7.3f   %7.3f\n"
    if (t1 && p2)
	printf(FORMAT, "write", t1/1000.0,
		(float)tms1->tms_stime/HZ,
		(float)tms1->tms_stime/t1*100000.0/HZ,
		(float)tms1->tms_utime/HZ,
		(float)tms1->tms_utime/t1*100000.0/HZ,
		(chunksize*nchunks)/(1024.0*(t1/1000.0)),
		(chunksize*nchunks)/(128.0*1024.0*(t1/1000.0)),
		(chunksize*nchunks)/(125.0*t1));
    if (t2 && p2)
	printf(FORMAT, "read", t2/1000.0,
		(float)tms2->tms_stime/HZ,
		(float)tms2->tms_stime/t2*100000.0/HZ,
		(float)tms2->tms_utime/HZ,
		(float)tms2->tms_utime/t2*100000.0/HZ,
		(chunksize*nchunks)/(1024.0*(t2/1000.0)),
		(chunksize*nchunks)/(128.0*1024.0*(t2/1000.0)),
		(chunksize*nchunks)/(125.0*t2));
    if ((t1 && t2) || (!p2 && (t1 + t2)))
	printf(FORMAT, "r/w", (t2+t1)/1000.0,
		(float)(tms1->tms_stime + tms2->tms_stime)/HZ,
		(float)(tms1->tms_stime + tms2->tms_stime)/(t1+t2)*100000.0/HZ,
		(float)(tms1->tms_utime + tms2->tms_utime)/HZ,
		(float)(tms1->tms_utime + tms2->tms_utime)/(t1+t2)*100000.0/HZ,
		(chunksize*nchunks)/(512.0*((t1 + t2)/1000.0)),
		(chunksize*nchunks)/(64.0*1024.0*((t1 + t2)/1000.0)),
		(chunksize*nchunks)/(62.5*(t1 + t2)));
}
#if	defined(SRCRT) && defined(IPPROTO_IP)

/*
 * Source route is handed in as
 *	[!]@hop1@hop2...[@|:]dst
 * If the leading ! is present, it is a
 * strict source route, otherwise it is
 * assmed to be a loose source route.
 *
 * We fill in the source route option as
 *	hop1,hop2,hop3...dest
 * and return a pointer to hop1, which will
 * be the address to connect() to.
 *
 * Arguments:
 *	arg:	pointer to route list to decipher
 *
 *	cpp: 	If *cpp is not equal to NULL, this is a
 *		pointer to a pointer to a character array
 *		that should be filled in with the option.
 *
 *	lenp:	pointer to an integer that contains the
 *		length of *cpp if *cpp != NULL.
 *
 * Return values:
 *
 *	Returns the address of the host to connect to.  If the
 *	return value is -1, there was a syntax error in the
 *	option, either unknown characters, or too many hosts.
 *	If the return value is 0, one of the hostnames in the
 *	path is unknown, and *cpp is set to point to the bad
 *	hostname.
 *
 *	*cpp:	If *cpp was equal to NULL, it will be filled
 *		in with a pointer to our static area that has
 *		the option filled in.  This will be 32bit aligned.
 *
 *	*lenp:	This will be filled in with how long the option
 *		pointed to by *cpp is.
 *
 */
	unsigned long
sourceroute(arg, cpp, lenp)
	char	*arg;
	char	**cpp;
	int	*lenp;
{
	static char lsr[44];
	char *cp, *cp2, *lsrp, *lsrep, *index();
	register int tmp;
	struct in_addr sin_addr;
	register struct hostent *host = 0;
	register char c;

	/*
	 * Verify the arguments, and make sure we have
	 * at least 7 bytes for the option.
	 */
	if (cpp == NULL || lenp == NULL)
		return((unsigned long)-1);
	if (*cpp != NULL && *lenp < 7)
		return((unsigned long)-1);
	/*
	 * Decide whether we have a buffer passed to us,
	 * or if we need to use our own static buffer.
	 */
	if (*cpp) {
		lsrp = *cpp;
		lsrep = lsrp + *lenp;
	} else {
		*cpp = lsrp = lsr;
		lsrep = lsrp + 44;
	}

	cp = arg;

	/*
	 * Next, decide whether we have a loose source
	 * route or a strict source route, and fill in
	 * the begining of the option.
	 */
	if (*cp == '!') {
		cp++;
		*lsrp++ = IPOPT_SSRR;
	} else
		*lsrp++ = IPOPT_LSRR;

	if (*cp != '@')
		return((unsigned long)-1);

	lsrp++;		/* skip over length, we'll fill it in later */
	*lsrp++ = 4;

	cp++;

	sin_addr.s_addr = 0;

	for (c = 0;;) {
		if (c == ':')
			cp2 = 0;
		else for (cp2 = cp; c = *cp2; cp2++) {
			if (c == ',') {
				*cp2++ = '\0';
				if (*cp2 == '@')
					cp2++;
			} else if (c == '@') {
				*cp2++ = '\0';
			} else if (c == ':') {
				*cp2++ = '\0';
			} else
				continue;
			break;
		}
		if (!c)
			cp2 = 0;

		if ((tmp = inet_addr(cp)) != -1) {
			sin_addr.s_addr = tmp;
		} else if (host = gethostbyname(cp)) {
#if	defined(h_addr)
			memcpy((caddr_t)&sin_addr,
				host->h_addr_list[0], host->h_length);
#else
			memcpy((caddr_t)&sin_addr, host->h_addr, host->h_length);
#endif
		} else {
			*cpp = cp;
			return(0);
		}
		memcpy(lsrp, (char *)&sin_addr, 4);
		lsrp += 4;
		if (cp2)
			cp = cp2;
		else
			break;
		/*
		 * Check to make sure there is space for next address
		 */
		if (lsrp + 4 > lsrep)
			return((unsigned long)-1);
	}
	if ((*(*cpp+IPOPT_OLEN) = lsrp - *cpp) <= 7) {
		*cpp = 0;
		*lenp = 0;
		return((unsigned long)-1);
	}
	*lsrp++ = IPOPT_NOP; /* 32 bit word align it */
	*lenp = lsrp - *cpp;
	return(sin_addr.s_addr);
}
#endif

#ifndef	HAS_PARSETOS

#define	MIN_TOS	0
#define	MAX_TOS	255

/* ARGSUSED */
	int
parsetos(name, proto)
	char	*name;
	char	*proto;
{
	register char	*c;
	int		tos;
#if !defined(sgi)
#ifdef	IP_TOS
	struct tosent	*tosp;

	tosp = gettosbyname(name, proto);
	if (tosp) {
		tos = tosp->t_tos;
	} else {
#endif
#endif
		for (c = name; *c; c++) {
			if (*c < '0' || *c > '9') {
				return (-1);
			}
		}
		tos = (int)strtol(name, (char **)NULL, 0);
#if !defined(sgi)
#ifdef	IP_TOS
	}
#endif
#endif
	if (tos < MIN_TOS || tos > MAX_TOS) {
		return (-1);
	}
	return (tos);
}
#endif
