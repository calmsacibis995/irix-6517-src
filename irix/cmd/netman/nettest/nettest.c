static char USMID[] = "@(#)tcp/usr/etc/nettest/nettest.c	61.1	09/13/90 09:04:50";
#include "nettest.h"
#ifdef BSD44
#include <machine/endian.h>
#include <netinet/in_systm.h>
#endif
#include <sys/un.h>
#include <netinet/tcp.h>
/* Undefine IP_TOS since we don't have the gettosbyname function it requires */
#undef  IP_TOS
#ifdef	IP_TOS
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#ifdef	CRAY
# include <time.h>
# ifndef HZ
#  define HZ CLK_TCK
# endif
#endif

union {
	struct sockaddr		d_gen;
	struct sockaddr_in	d_inet;
	struct sockaddr_un	d_unix;
} name;
int	nchunks = NCHUNKS;		/* 100 */
int	chunksize = CHUNK;		/* 4096 */
int	dflag = 0;
int	checkdata = 0;
int	hash = 0;
int	fullbuf = 0;
int	kbufsize = 0;
int	nodelay = 0;

#define	D_PIPE	1
#define	D_UNIX	2
#define	D_INET	3
#define	D_FILE	4

long	times();
#define	GETTIMES(a,b)	a = times(&b);
#define	TIMETYPE	long
#ifdef	TCP_WINSHIFT
int	winshift;
int	usewinshift;
#endif
int	tos;
int	maxchildren;

struct in_addr hisaddr;

main(argc, argv)
int argc;
char **argv;
{
	register int	s, s2, port = PORTNUMBER;
	struct hostent	*gethostbyname(), *hp;
	char		*hisname, _myname[256], *myname = _myname;
	register char	*portname = UNIXPORT;
	int		type = SOCK_STREAM;
	int		domain = D_INET;
	int		i;
	int		namesize;
	int		on = 1;
	int		nconnections = 1;
#ifdef	IP_TOS
	struct tosent	*tp;
#endif
	extern char	*optarg;
	extern int	optind;
	
	maxchildren = getdtablesize() - 4;

	while ((i = getopt(argc, argv, "b:cdfFhn:p:s:t:?")) != EOF) {
		switch(i) {
		case 'b':				/* socket buffer size */
			kbufsize = atoval(optarg);
			break;
		case 'c':
			++checkdata;
			break;
		case 'd':
			dflag++;
			break;
		case 'f':
			fullbuf++;
			break;
		case 'F':			
#ifdef	TCP_NODELAY
			nodelay++;
#else
			fprintf(stderr, "TCP nodelay option not supported\n");
			usage();
#endif
			break;
		case 'h':
			++hash;
			break;
		case 'n':			/* connections per dest */
			nconnections = atoi(optarg);
			if (nconnections < 1 || nconnections > maxchildren) {
				fprintf(stderr,
					"-n: value must be between 1 and %d\n",
					maxchildren);
				usage();
			}
			break;
		case 'p':				/* protocol */
			if (!strcmp(optarg, "tcp")) {
				domain = D_INET;
				type = SOCK_STREAM;
			} else if (!strcmp(optarg, "udp")) {
				domain = D_INET;
				type = SOCK_DGRAM;
				nchunks = 1;
			} else if (!strcmp(optarg, "unix")) {
				domain = D_UNIX;
				type = SOCK_STREAM;
			} else if (!strcmp(optarg, "pipe")) {
				domain = D_PIPE;
			} else if (!strcmp(optarg, "file")) {
				domain = D_FILE;
			} else {
				printf("Unknown protocol: %s\n", optarg);
				usage();
			}
			break;
		case 's':				/* window shift */
#ifdef	TCP_WINSHIFT
			usewinshift++;
			winshift = atoval(optarg);
			if (winshift < -1 || winshift > 14) {
				fprintf(stderr, "window shift (-s) must be beteen -1 and 14\n");
				usage();
			}
#else
			fprintf(stderr, "window shift option not supported\n");
			usage();
#endif
			break;
		case 't':
#ifdef	IP_TOS
			tp = gettosbyname(optarg);
			if (tp == NULL)
				fprintf(stderr, "Bad tos name: %s\n", *argv);
			else
				tos = tp->t_tos;
			break;
#else
			fprintf(stderr, "TOS (-s) option not supported\n");
			usage();
#endif
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (domain == D_INET) {
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
		if (nconnections > 1) {
			fprintf(stderr, "-n flag not supported for pipe\n");
			usage();
		}
		sprintf(myname, "W%s", portname);
		
		if (((s2 = open(myname, 1)) < 0) ||
		    ((myname[0] = 'R'),((s = open(myname, 0)) < 0))) {
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
		name.d_inet.sin_port = htons(port);
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
		if (type == SOCK_DGRAM) {
			do_dgram(s, &name);
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
	if (domain == D_INET)
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

/*
 * When called, this routine will fork off "nconnections" children, and
 * return in each of the children.  The parent program will hang around
 * for the children to die, printing out their statistics (read off of
 * a pipe set up beforehand).
 */
do_children(nconnections)
int nconnections;
{
	int *children;
	register int i, n;
	char *malloc();
	int status;

	children = (int *)malloc(nconnections * sizeof(int) * 2);
	if (children == NULL) {
		fprintf(stderr,
			"malloc() for data to keep track of children failed\n");
		exit(1);
	}

	for (i = 0; i < nconnections; i++) {
		if (pipe(&children[i*2+0]) < 0) {
			fprintf(stderr, "Cannot create pipe for child %d\n", i);
			continue;
		}
		switch(n = fork()) {
		case 0:
			/*
			 * In the child.  Close down all the pipes, dup
			 * our pipe to stdout and stderr, and return.
			 */
			for (n = 0; n <= i; n++)
				close(children[n*2+0]);
			dup2(children[i*2+1], 1);
			dup2(1, 2);
			close(children[i*2+1]);
			return;

		case -1:
			fprintf(stderr, "Child %d not started\n", i);
			break;

		default:
			close(children[i*2+1]);
			children[i*2+1] = n;
			break;
		}
	}
	while ((n = wait(&status)) >= 0) {
		char buf[512];

		for (i = 0; i < nconnections; i++) {
			if (children[i*2+1] == n)
				break;
		}
		if (i == nconnections) {
			fprintf(stderr, "Unknown child [pid %d] died.\n", n);
			continue;
		}
		printf("Child %d statistics:\n", i);
		fflush(stdout);
		while ((n = read(children[i*2+0], buf, sizeof(buf))) > 0)
			write(1, buf, n);
		close(children[i*2+0]);
		children[i*2+1] = -1;
	}
	exit(0);
}

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

do_stream(in, out)
register int in, out;
{
	register int	i, t, j, offset = 0, t2;
	register char	*cp;
	char		buf[128], *data, *malloc();
	long		*cnts;
	struct tms	tms1, tms2, tms3;
	TIMETYPE	start, turnaround, end;

	sprintf(buf, "%d %d %d %d %d %d\n", nchunks, chunksize, fullbuf,
		kbufsize, tos, nodelay);
	if (write(out, buf, strlen(buf))  != strlen(buf)) {
		perror("write1");
		exit(1);
	}
	if ((i = read(in, buf, sizeof(buf))) < 0) {
		perror("read (waiting to verify setup)");
		exit(1);
	}
	buf[i] = '\0';
	if (buf[i-1] == '\n')
		buf[--i] = '\0';
	if (i > 2)
		printf("remote server: %s\n", &buf[1]);
	if (i > 1 && buf[0] == '0')
		exit(1);
	data = malloc(chunksize);
	if (data == NULL) {
		fprintf(stderr, "cannot malloc enough space\n");
		exit(1);
	}
	cnts = (long *)malloc((chunksize+1)*sizeof(long));
	bzero(cnts, (chunksize+1)*sizeof(long));

	if (checkdata)
		for (cp = data, i = 0; i < chunksize; )
			*cp++ = *("abcdefghijklmnopqrstuvwxyzABCDEF" + (i++/8)%32);
	if (hash)
		write(0, "\r\nWrite: ", 9);

	GETTIMES(start, tms1);

	for (i = 0; i < nchunks; i++) {
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

	for (i = 0; i < nchunks || offset; i++) {
		if ((t = read(in, data, chunksize)) < 0) {
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
				if ((t2 = read(in, data+t, chunksize-t)) < 0) {
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
		for (cp = data, j = 0; j < t; cp++, j++) {
		    if (*cp != *("abcdefghijklmnopqrstuvwxyzABCDEF" +
							((j+t2)/8)%32)) {
			fprintf(stderr, "%d/%d(%d): got %d, expected %d\n",
			    i, (j+t2), i*chunksize + (j+t2), *cp,
		    	    *("abcdefghijklmnopqrstuvwxyzABCDEF" +
							((j+t2)/8)%32));
		    }
		}
	}

	GETTIMES(end, tms3);

	prtimes(&start, &turnaround, &end, &tms1, &tms2, &tms3);

	j = 0;
	for (i = 0; i <= chunksize; i++)
		if (cnts[i]) {
			printf("%6d: %5d ", i, cnts[i]);
			if (++j == 4) {
				printf("\n");
				j=0;
			}
		}
	if (j)
		printf("\n");
	return;
bad:
	perror(buf);
	exit(1);
}

usage()
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
"Usage: nettest [-cdfFho] [-b bufsize] [-s winshift] [-t tos] [-n #streams]",
"               [-p tcp|udp] [host [count [size [port]]]]",
"       nettest [-cdfh] [-b bufsize] -p unix|pipes [-n #streams]",
"               [count [size [filename]]]",
"       nettest [-cdfh] [-b bufsize] -p file file1 file2 [count [size]]");
	exit(1);
}

do_dgram(s, name)
register int s;
struct sockaddr_in *name;
{
	register int	ret, i;
	register char	*data;
	char		*malloc();
	struct tms	tms1, tms2, tms3;
	TIMETYPE	start, turnaround, end;
	
	data = malloc(chunksize);
	if (data == NULL) {
		fprintf(stderr, "cannot malloc enough space\n");
		exit(1);
	}

	GETTIMES(start, tms1);

	*data = 0;
	for (i = 0; i < nchunks; i++) {
		if ((ret = sendto(s, data, chunksize, 0, name, sizeof(*name))) < 0) {
			perror("sendto");
			printf("%d out of %d sent\n", i, nchunks);
			exit(1);
		}
		if (ret != chunksize)
			printf("sendto returned %d, expected %d\n", ret, chunksize);
		(*data)++;
	}

	GETTIMES(turnaround, tms2);

	GETTIMES(end, tms3);

	prtimes(&start, &turnaround, &end, &tms1, &tms2, &tms3);

}

prtimes(p0, p1, p2, tms1, tms2, tms3)
TIMETYPE	*p0, *p1, *p2;
struct tms	*tms1, *tms2, *tms3;
{
	long	t1, t2;

	t1 = (*p1 - *p0)*1000/HZ;
	t2 = (*p2 - *p1)*1000/HZ;
	tms1->tms_utime = tms2->tms_utime - tms1->tms_utime;
	tms1->tms_stime = tms2->tms_stime - tms1->tms_stime;
	tms2->tms_utime = tms3->tms_utime - tms2->tms_utime;
	tms2->tms_stime = tms3->tms_stime - tms2->tms_stime;
	printf("           Real  System            User          Kbyte   Mbit(K^2) mbit(1+E6)\n");
#define FORMAT "%7s %7.4f %7.4f (%4.1f%%) %7.4f (%4.1f%%) %7.2f %7.3f   %7.3f\n"
    if (t1)
	printf(FORMAT, "write", t1/1000.0,
		(float)tms1->tms_stime/HZ,
		(float)tms1->tms_stime/t1*100000.0/HZ,
		(float)tms1->tms_utime/HZ,
		(float)tms1->tms_utime/t1*100000.0/HZ,
		(chunksize*nchunks)/(1024.0*(t1/1000.0)),
		(chunksize*nchunks)/(128.0*1024.0*(t1/1000.0)),
		(chunksize*nchunks)/(125.0*t1));
    if (t2)
	printf(FORMAT, "read", t2/1000.0,
		(float)tms2->tms_stime/HZ,
		(float)tms2->tms_stime/t2*100000.0/HZ,
		(float)tms2->tms_utime/HZ,
		(float)tms2->tms_utime/t2*100000.0/HZ,
		(chunksize*nchunks)/(1024.0*(t2/1000.0)),
		(chunksize*nchunks)/(128.0*1024.0*(t2/1000.0)),
		(chunksize*nchunks)/(125.0*t2));
    if (t1 && t2)
	printf(FORMAT, "r/w", (t2+t1)/1000.0,
		(float)(tms1->tms_stime + tms2->tms_stime)/HZ,
		(float)(tms1->tms_stime + tms2->tms_stime)/(t1+t2)*100000.0/HZ,
		(float)(tms1->tms_utime + tms2->tms_utime)/HZ,
		(float)(tms1->tms_utime + tms2->tms_utime)/(t1+t2)*100000.0/HZ,
		(chunksize*nchunks)/(512.0*((t1 + t2)/1000.0)),
		(chunksize*nchunks)/(64.0*1024.0*((t1 + t2)/1000.0)),
		(chunksize*nchunks)/(62.5*(t1 + t2)));
}
