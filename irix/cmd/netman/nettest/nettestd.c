static char USMID[] = "@(#)tcp/usr/etc/nettest/nettestd.c	61.1	09/13/90 09:04:50";

#include "nettest.h"

#include <sys/errno.h>
#include <signal.h>
#ifdef	WAIT3CODE
#include <sys/wait.h>
#endif
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef	CRAY2
#include <sys/sysmacros.h>
#endif
#include <netinet/tcp.h>
#include <sys/termio.h>
#include <sys/syslog.h>

#ifdef	WAIT3CODE
dochild()
{
	int pid;

	while ((pid = wait3(0, WNOHANG, 0)) > 0)
		;
}
#else
#define	dochild	SIG_IGN
#endif

int dflag;
#define	debug(x)	if(dflag>1)fprintf x

#define	D_PIPE	1
#define	D_UNIX	2
#define	D_INET	3
#define	D_FILE	4

main(argc, argv)
int argc;
char **argv;
{
	register int	s, s2, mode, dev1, dev2;
	char		*portname = UNIXPORT;
	short		port = PORTNUMBER;
	int		domain = D_UNIX;
	int		type = SOCK_STREAM;
	int		namesize;
	int		on = 1;
	int		c;
	int		i;
	int		pid;
	char		*f1, *f2;
	union {
		struct sockaddr		d_gen;
		struct sockaddr_un	d_unix;
		struct sockaddr_in	d_inet;
	} name;
	char		buf[256];
	extern int	optind;
	extern char	*optarg;

	while ((c = getopt(argc, argv, "p:d")) != EOF) {
		switch(c) {
		case 'd':
			++dflag;
			break;
		case 'p':
			if (!strcmp(optarg, "unix")) {
				domain = D_UNIX;
				type = SOCK_STREAM;
			} else if (!strcmp(optarg, "tcp")) {
				domain = D_INET;
				type = SOCK_STREAM;
			} else if (!strcmp(optarg, "udp")) {
				domain = D_INET;
				type = SOCK_DGRAM;
			} else if (!strcmp(optarg, "file")) {
				domain = D_FILE;
#ifdef	NAMEDPIPES
			} else if (!strcmp(optarg, "pipe")) {
				domain = D_PIPE;
#endif	/* NAMEDPIPES */
			} else {
				printf("Unknown protocol: %s\n", optarg);
				usage();
			}
			break;

		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (domain == D_FILE) {
		if (argc != 2)
			usage();
		f1 = *argv++;
		f2 = *argv++;
		argc -= 2;
	} else if (argc > 1)
		usage();
	else if (argc == 1) {
		portname = *argv++;
		argc--;
		port = atoi(portname);
	}

        /*
         * get rid of the controlling tty so we can become a daemon
         */
        if (setpgrp() < 0)
                perror("setpgrp");
        if ((i = open("/dev/tty", O_RDWR)) >= 0) {
                (void)ioctl(i, TIOCNOTTY, (char *)0);
                (void)close(i);
        }
        /*
         * become a daemon
         */
        if((pid = fork()) < 0) {
                perror("nettestd: fork:");
                exit(1);
        }
        if (pid)                                /* parent */
                exit(0);
        /*
         * Child; now in background and owned by init().
         * Close open file descriptors 
         */
        for (i = getdtablesize(); --i >= 0; )
                (void)close(i);


	switch (domain) {
#ifdef	NAMEDPIPES
	case D_PIPE:
		mode = S_IFIFO|0666;
		dev1 = dev2 = 0;
		sprintf(buf, "R%s", portname);
		umask(0);
		for(;;) {
			unlink(buf);
			if (mknod(buf, mode, dev1) < 0) {
				syslog(LOG_ERR, "nettestd: mknod %m");
				exit(1);
			}
			buf[0] = 'W';
			unlink(buf);
			if (mknod(buf, mode, dev2) < 0) {
				syslog(LOG_ERR, "nettestd: mknod %m");
				goto err1;
			}
			buf[0] = 'W';
			if ((s2 = open(buf, O_RDONLY)) < 0) {
				syslog(LOG_ERR, buf);
				goto err2;
			}
			buf[0] = 'R';
			if ((s = open(buf, O_WRONLY)) < 0) {
				syslog(LOG_ERR, buf);
				close(s2);
			err2:	buf[0] = 'R';
				unlink(buf);
			err1:	buf[0] = 'W';
				unlink(buf);
				exit(1);
			}
			data_stream(s2, s);
			close(s2);
			close(s);
		}
		break;
#endif	/* NAMEDPIPES */
	case D_FILE:
		for(;;) {
			s = open(f1, 0);
			if (s < 0) {
				syslog(LOG_ERR, f1);
				exit(1);
			}
			s2 = open(f2, 1);
			if (s2 < 0) {
				syslog(LOG_ERR, f2);
				exit(1);
			}
			data_stream(s, s2);
			close(s2);
			close(s);
			sleep(1);
		}
		break;

	case D_UNIX:
		name.d_unix.sun_family = AF_UNIX;
		strcpy(name.d_unix.sun_path, portname);
		namesize = sizeof(name.d_unix) - sizeof(name.d_unix.sun_path)
				+ strlen(name.d_unix.sun_path);
		(void) unlink(portname);
		goto dosock;
		break;

	case D_INET:
		name.d_inet.sin_family = AF_INET;
		if (port <= 0) {
			fprintf(stderr, "bad port number\n");
			exit(1);
		}
		name.d_inet.sin_port = htons(port);
#if	!defined(CRAY) || defined(s_addr)
		name.d_inet.sin_addr.s_addr = 0;
#else
		name.d_inet.sin_addr = 0;
#endif
		namesize = sizeof(name.d_inet);

	dosock:
		if ((s = socket(name.d_inet.sin_family, type, 0)) < 0) {
			syslog(LOG_ERR, "nettestd: socket %m");
			exit(1);
		}
		if (dflag && setsockopt(s, SOL_SOCKET, SO_DEBUG, &on, sizeof(on)) < 0)
			syslog(LOG_ERR, "nettestd: setsockopt - SO_DEBUG %m");
		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (bind(s, (char *)&name, namesize) < 0) {
			syslog(LOG_ERR, "nettestd: bind %m");
			exit(1);
		}
		if (type == SOCK_DGRAM)
			do_dgram(s);
		else
			do_stream(s);
		/*NOTREACHED*/
		break;
	}
}

do_stream(s)
register int s;
{
	register int		i, s2;
	struct sockaddr_in	name;
	int			namesize;
	int			kbufsize;
	
	listen(s, 5);

	signal(SIGCHLD, dochild);
	for (;;) {
		namesize = sizeof(name);
		s2 = accept(s, (char *)&name, &namesize);
		if (s2 < 0) {
			extern int errno;
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "nettestd: accept %m");
		} else {
			if ((i = fork()) == 0) {
				close(s);
				i = data_stream(s2, s2);
				shutdown(s2, 2);
				exit(i);
			} else if (i < 0)
				syslog(LOG_ERR, "nettestd: fork %m");
			close(s2);
		}
	}
}

data_stream(in, out)
register in, out;
{
	register int	i, t, offset = 0;
	register char	*cp, *data;
	char		buf[128], *malloc();
	int		chunks = 0, chunksize = 0, fullbuf = 0, kbufsize = 0;
	int		tos = 0, nodelay = 0;

	for (cp = buf; ; ) {
		i = read(in, cp, 1);
		if (i != 1) {
			if (i < 0)
				syslog(LOG_ERR, "nettestd: read %m");
			else
				fprintf(stderr, "nettestd: Read returned %d, expected 1\n", i);
			exit(1);
		}
		if (*cp++ == '\n')
			break;
	}
	*cp = '\0';
	sscanf(buf, "%d %d %d %d %d %d", &chunks, &chunksize, &fullbuf,
					&kbufsize, &tos, &nodelay);
	/*
	 * If fullbuf is set, allocate a buffer twice as big.  This
	 * is so that we can always read a full size buffer, from
	 * the offset of the last read.  This keeps the data in
	 * the first chunksize consistent in case the remote side
	 * is trying to verify the contents.
	 */
	data = malloc(fullbuf ? 2*chunksize : chunksize);
	if (data == NULL) {
		sprintf(buf, "0 malloc() failed\n");
		write(out, buf, strlen(buf));
		return(1);
	}
	strcpy(buf, "1");
	if (kbufsize) {
#ifdef	SO_SNDBUF
		if ((setsockopt(out, SOL_SOCKET, SO_SNDBUF, &kbufsize,
						sizeof(kbufsize)) < 0) ||
		    (setsockopt(in, SOL_SOCKET, SO_RCVBUF, &kbufsize,
						sizeof(kbufsize)) < 0))
#endif
			strcat(buf, " Cannot set buffers sizes.");
	}
	if (tos) {
#ifdef	IP_TOS
		if (setsockopt(out, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0)
#endif
			strcat(buf, " Cannot set TOS bits.");
	}
	if (nodelay) {
#ifdef	TCP_NODELAY
		if (setsockopt(out, IPPROTO_TCP, TCP_NODELAY, &nodelay,
							sizeof(nodelay)) < 0)
#endif
			strcat(buf, " Cannot set TCP_NODELAY.");
	}
	strcat(buf, " \n");
	write(out, buf, strlen(buf));
	for (i = 0; i < chunks || offset; i++) {
		if ((t = read(in, data + offset, chunksize)) < 0) {
			sprintf(buf, "read #%d.%d\n", i+1, chunksize);
			goto bad;
		}
		if (t == 0) {
			fprintf(stderr, "server: EOF on read, block # %d\n", i);
			break;
		}
/*@*/		debug((stderr, "server: %d: read %d\n", i, t));
		if (fullbuf) {
			offset += t;
			if (offset >= chunksize)
				offset -= chunksize;
			else
				--i;
		} else while (t != chunksize) {
			register int t2;
			t2 = read(in, data+t, chunksize-t);
			if (t2 < 0) {
				sprintf(buf, "read #%d.%d\n", i+1, chunksize-t);
				goto bad;
			}
			if (t2 == 0) {
				fprintf(stderr, "server: EOF on read, block # %d\n", i);
				break;
			}
			t += t2;
/*@*/			debug((stderr, "server: %d: partial read %d (%d)\n", i, t2, t));
		}
	}
	for (i = 0; i < chunks; i++) {
		if ((t = write(out, data, chunksize)) < 0) {
			sprintf(buf, "write #%d\n", i+1);
			goto bad;
		}
		if (t != chunksize)
			fprintf(stderr, "server: write: %d vs %d\n", t, chunksize);
/*@*/		else
/*@*/			debug((stderr, "server: %d: write %d\n", i, t));
	}
	free(data);
	return(0);
bad:
	syslog(LOG_ERR, buf);
	free(data);
	return(1);
}

#define	MAXSIZE	(64*1024)

do_dgram(s)
int s;
{
	register int		t, t2;
	register char		*cp, *data;
	register struct hostent	*hp;
	char			*inet_ntoa(), *malloc();
	struct sockaddr_in	name;
	int			namesize;

	data = malloc(MAXSIZE);
	if (data == NULL) {
		fprintf(stderr, "no malloc\n");
		shutdown(s, 2);
		exit(1);
	}
	for (;;) {
		namesize = sizeof(name);
		t = recvfrom(s, data, MAXSIZE, 0, (char *)&name, &namesize);
		if (t < 0) {
			syslog(LOG_ERR, "nettestd: recvfrom %m");
			continue;
		}
#ifdef	notdef
#if	!defined(CRAY) || defined(s_addr)
		hp = gethostbyaddr((char *)&name.sin_addr,
			sizeof(name.sin_addr), name.sin_family);
#else	/* CRAY */
		{
			long t;
			t = name.sin_addr;
			hp = gethostbyaddr((char *)&t,
				sizeof(name.sin_addr), name.sin_family);
		}
#endif	/* CRAY */
		if (hp != NULL)
			cp = hp->h_name;
		else
#endif	/* notdef */
			cp = inet_ntoa(name.sin_addr);
		printf("got %d bytes from %s\n", t, cp);
#ifdef	notdef
		t2 = sendto(s, data, t, 0, (char *)&name, namesize);
		if (t2 < 0)
			syslog(LOG_ERR, "nettestd: sendto %m");
#endif	/* notdef */
	}
}

usage()
{
	fprintf(stderr, "%s\n%s\n%s\n",
		"Usage: nettestd [-d] [-p tcp|udp] [port]",
		"       nettestd [-d] -p unix|pipe [filename]",
		"       nettestd [-d] -p file readfile writefile");
	exit(1);
}
