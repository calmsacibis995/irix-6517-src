/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
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
static char sccsid[] = "@(#)ftp.c	5.36 (Berkeley) 6/29/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/file.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <pwd.h>
#include <varargs.h>
#ifdef sgi
#include <stdlib.h>
#endif

#include "ftp_var.h"

#ifdef sgi
/*
 * use off64_t and *stat64 and for large files and filesystems
 */
#define stat	stat64
#define fstat	fstat64
#define off_t	off64_t
#endif

struct	sockaddr_in hisctladdr;
struct	sockaddr_in data_addr;
int	data = -1;
int	abrtflag = 0;
int	ptflag = 0;
int	connected;
struct	sockaddr_in myctladdr;
uid_t	getuid();
sig_t	lostpeer();
off_t	restart_point = 0;

#ifndef sgi
extern char *strerror();
#endif
extern int errno;
extern int big_pipe, odirect;

FILE	*cin, *cout;
FILE	*dataconn();

#ifdef sgi

/* SYSV version doesn't allow input from stdin */
#define getpass	mygetpass

/*
 * Global buffers are page aligned using memalign(), which
 * is a Good Thing if you like page flipping.
 */
#define NBUFS	3
#define FTPBUFSIZ (32*1024)
#define FTPBUFALIGN (16*1024)
long ibufsize, obufsize;
char *ftp_inbuf[NBUFS];
char *ftp_outbuf[NBUFS];

static int sockbufsize = 60*1024;

#define	ENOUGH	(2<<20)
#define	min(a, b)	((a) < (b) ? (a) : (b))

void
setup_ftpbufs(int s)
{
	int i;
	int len = sizeof(ibufsize);

	(void) getsockopt(s, SOL_SOCKET, SO_RCVBUF, &ibufsize, &len);
	len = sizeof(obufsize);
	(void) getsockopt(s, SOL_SOCKET, SO_SNDBUF, &obufsize, &len);

	if (ibufsize < FTPBUFSIZ)
		ibufsize = FTPBUFSIZ;
	
	if (obufsize < FTPBUFSIZ)
		obufsize = FTPBUFSIZ;

	if (ibufsize > 2*1024*1024)
		ibufsize = 2*1024*1024;
	if (obufsize > 2*1024*1024)
		obufsize = 2*1024*1024;

	/*
	 * use memalign to set up page-aligned buffers
	 */
	for (i = 0; i < NBUFS; i++) {
		if ((ftp_inbuf[i] = memalign(FTPBUFALIGN, ibufsize)) == NULL) {
			perror("ftp");
			exit(1);
		}
		if ((ftp_outbuf[i] = memalign(FTPBUFALIGN, obufsize)) == NULL){
			perror("ftp");
			exit(1);
		}
	}
}
#endif /* sgi */

char *
hookup(host, port)
	char *host;
	int port;
{
	register struct hostent *hp = 0;
	int s,len, tos;
#ifdef sgi
	int sdup = -1;
#endif
	static char hostnamebuf[80];

	bzero((char *)&hisctladdr, sizeof (hisctladdr));
	hisctladdr.sin_addr.s_addr = inet_addr(host);
	if (hisctladdr.sin_addr.s_addr != -1) {
		hisctladdr.sin_family = AF_INET;
		(void) strncpy(hostnamebuf, host, sizeof(hostnamebuf));
	} else {
		hp = gethostbyname(host);
		if (hp == NULL) {
			fprintf(stderr, "ftp: %s: ", host);
			herror((char *)NULL);
			code = -1;
			return((char *) 0);
		}
		hisctladdr.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr_list[0],
		    (caddr_t)&hisctladdr.sin_addr, hp->h_length);
		(void) strncpy(hostnamebuf, hp->h_name, sizeof(hostnamebuf));
	}
	hostname = hostnamebuf;
	s = socket(hisctladdr.sin_family, SOCK_STREAM, 0);
	if (s < 0) {
		perror("ftp: socket");
		code = -1;
		return (0);
	}
	hisctladdr.sin_port = port;
	while (connect(s, (struct sockaddr *)&hisctladdr, sizeof (hisctladdr)) < 0) {
		if (hp && hp->h_addr_list[1]) {
			int oerrno = errno;
			extern char *inet_ntoa();

			fprintf(stderr, "ftp: connect to address %s: ",
				inet_ntoa(hisctladdr.sin_addr));
			errno = oerrno;
			perror((char *) 0);
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0],
			     (caddr_t)&hisctladdr.sin_addr, hp->h_length);
			fprintf(stdout, "Trying %s...\n",
				inet_ntoa(hisctladdr.sin_addr));
			(void) close(s);
			s = socket(hisctladdr.sin_family, SOCK_STREAM, 0);
			if (s < 0) {
				perror("ftp: socket");
				code = -1;
				return (0);
			}
			continue;
		}
		perror("ftp: connect");
		code = -1;
		goto bad;
	}
	len = sizeof (myctladdr);
	if (getsockname(s, (struct sockaddr *)&myctladdr, &len) < 0) {
		perror("ftp: getsockname");
		code = -1;
		goto bad;
	}
#ifdef IP_TOS
	tos = IPTOS_LOWDELAY;
#ifdef notyet
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		perror("ftp: setsockopt TOS (ignored)");
#endif
#endif
	cin = fdopen(s, "r");
#ifdef sgi	
	/* 
	 * SYSV stdio routines use fd as an index into the iob, hence we need 
	 * unique fd's to for cin and cout.
	 */
	if ((sdup = dup(s)) < 0) {
	    perror("ftp: dup");
	    code = -1;
	    return (0);
	}
	cout = fdopen(sdup, "w");
#else
	cout = fdopen(s, "w");
#endif
	if (cin == NULL || cout == NULL) {
		fprintf(stderr, "ftp: fdopen failed.\n");
		if (cin)
			(void) fclose(cin);
		if (cout)
			(void) fclose(cout);
		code = -1;
		goto bad;
	}
	if (verbose)
		printf("Connected to %s.\n", hostname);
	if (getreply(0) > 2) { 	/* read startup message from server */
		if (cin)
			(void) fclose(cin);
		if (cout)
			(void) fclose(cout);
		code = -1;
		goto bad;
	}
#ifdef SO_OOBINLINE
	{
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on))
		< 0 && debug) {
			perror("ftp: setsockopt");
		}
	}
#endif /* SO_OOBINLINE */

	setup_ftpbufs(s);
	return (hostname);
bad:
#ifdef sgi
	if (sdup >= 0)
		(void) close(sdup);
#endif
	(void) close(s);
	return ((char *)0);
}

login(host)
	char *host;
{
	char tmp[80];
	char *user, *pass, *acct, *getlogin(), *getpass();
	int n, aflag = 0;

	user = pass = acct = 0;
	if (ruserpass(host, &user, &pass, &acct) < 0) {
		code = -1;
		return(0);
	}
	while (user == NULL) {
		char *myname = getlogin();

		if (myname == NULL) {
			struct passwd *pp = getpwuid(getuid());

			if (pp != NULL)
				myname = pp->pw_name;
		}
		if (myname)
			printf("Name (%s:%s): ", host, myname);
		else
			printf("Name (%s): ", host);
		(void) fgets(tmp, sizeof(tmp) - 1, stdin);
		tmp[strlen(tmp) - 1] = '\0';
		if (*tmp == '\0')
			user = myname;
		else
			user = tmp;
	}
	n = command("USER %s", user);
	if (n == CONTINUE) {
		if (pass == NULL)
			pass = getpass("Password:");
		n = command("PASS %s", pass);
		memset(pass, 'x', strlen(pass));
	}
	if (n == CONTINUE) {
		aflag++;
		if (acct == NULL)
			acct = getpass("Account:");
		n = command("ACCT %s", acct);
		memset(acct, 'x', strlen(acct));
	}
	if (n != COMPLETE) {
		fprintf(stderr, "Login failed.\n");
		return (0);
	}
	if (!aflag && acct != NULL)
		(void) command("ACCT %s", acct);
	if (proxy)
		return(1);
	for (n = 0; n < macnum; ++n) {
		if (!strcmp("init", macros[n].mac_name)) {
			(void) strcpy(line, "$init");
			makeargv();
			domacro(margc, margv);
			break;
		}
	}
	return (1);
}

sig_t
cmdabort()
{
	extern jmp_buf ptabort;

	printf("\n");
	(void) fflush(stdout);
	abrtflag++;
#ifdef sgi
	(void)signal(SIGINT,cmdabort);
#endif
	if (ptflag)
		longjmp(ptabort,1);
}

/*VARARGS*/
command(va_alist)
va_dcl
{
	va_list ap;
	char *fmt;
	int r;
	sig_t (*oldintr)(), cmdabort();

	abrtflag = 0;
	if (debug) {
		printf("---> ");
		va_start(ap);
		fmt = va_arg(ap, char *);
		if (strncmp("PASS ", fmt, 5) == 0)
			printf("PASS XXXX");
		else 
			vfprintf(stdout, fmt, ap);
		va_end(ap);
		printf("\n");
		(void) fflush(stdout);
	}
	if (cout == NULL) {
		perror ("No control connection for command");
		code = -1;
		return (0);
	}
	oldintr = signal(SIGINT, cmdabort);
	va_start(ap);
	fmt = va_arg(ap, char *);
	vfprintf(cout, fmt, ap);
	va_end(ap);
	fprintf(cout, "\r\n");
	(void) fflush(cout);
	cpend = 1;
	r = getreply(!strcmp(fmt, "QUIT"));
	if (abrtflag && oldintr != SIG_IGN)
		(*oldintr)();
	(void) signal(SIGINT, oldintr);
	return(r);
}

char reply_string[BUFSIZ];		/* last line of previous reply */

#include <ctype.h>

getreply(expecteof)
	int expecteof;
{
	register int c, n;
	register int dig;
	register char *cp;
	int originalcode = 0, continuation = 0;
	sig_t (*oldintr)(), cmdabort();
	int pflag = 0;
	int ptvalid;
	char *pt = pasv, *ptend = pasv + sizeof(pasv) - 1;

	oldintr = signal(SIGINT, cmdabort);
	for (;;) {
		dig = n = code = 0;
		cp = reply_string;
		while ((c = getc(cin)) != '\n') {
			if (c == IAC) {     /* handle telnet commands */
				switch (c = getc(cin)) {
				case WILL:
				case WONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, DONT, c);
					(void) fflush(cout);
					break;
				case DO:
				case DONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, WONT, c);
					(void) fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void) signal(SIGINT,oldintr);
					code = 221;
					return (0);
				}
				lostpeer();
				if (verbose) {
					printf("421 Service not available, remote server has closed connection\n");
					(void) fflush(stdout);
				}
				code = 421;
				return(4);
			}
			if (c != '\r' && (verbose > 0 ||
			    (verbose > -1 && n == '5' && dig > 4))) {
				if (proxflag &&
				   (dig == 1 || dig == 5 && verbose == 0))
					printf("%s:",hostname);
				(void) putchar(c);
			}
			if (dig < 4 && isdigit(c))
				code = code * 10 + (c - '0');
			if (!pflag && code == 227)
				pflag = 1;
			if (dig > 4 && pflag == 1 && isdigit(c))
				pflag = 2;
			if (pflag == 2) {
				ptvalid = ( c != '\r' && c != ')' );
				if (ptvalid && pt < ptend)
					*pt++ = c;
				else if (!ptvalid) {
					*pt = '\0';
					pflag = 3;
				}
			}
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
			if (cp < &reply_string[sizeof(reply_string) - 1])
				*cp++ = c;
		}
		if (verbose > 0 || verbose > -1 && n == '5') {
			(void) putchar(c);
			(void) fflush (stdout);
		}
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		*cp = '\0';
		if (n != '1')
			cpend = 0;
		(void) signal(SIGINT,oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer();
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
			(*oldintr)();
		return (n - '0');
	}
}

empty(mask, sec)
	struct fd_set *mask;
	int sec;
{
	struct timeval t;

	t.tv_sec = (long) sec;
	t.tv_usec = 0;
	return(select(32, mask, (struct fd_set *) 0, (struct fd_set *) 0, &t));
}

jmp_buf	sendabort;

#ifdef sgi
sig_t
#endif
abortsend()
{

	mflag = 0;
	abrtflag = 0;
	printf("\nsend aborted\nwaiting for remote to finish abort\n");
	(void) fflush(stdout);
#ifdef sgi
	(void)signal(SIGINT, abortsend);
#endif
	longjmp(sendabort, 1);
}

#define HASHBYTES 1024

sendrequest(cmd, local, remote, printnames)
	char *cmd, *local, *remote;
	int printnames;
{
#ifdef sgi
	/* volatile variables are referenced in abort logic
	   which is entered via longjmp so we must guarantee
	   that references to these use the stack address. Hopefully
	   this will not slow things down significantly.             */

	FILE * volatile fin, * volatile dout = 0;
	int (* volatile closefunc)();
	sig_t (* volatile oldintr)(), (* volatile oldintp)();
	sig_t abortsend();
	char *bufp;
	static int bufsize = FTPBUFSIZ;
	int b = 0;
	volatile off_t bytes = 0;
#else
	FILE *fin, *dout = 0, *popen();
	int (*closefunc)(), pclose(), fclose();
	sig_t (*oldintr)(), (*oldintp)();
	int abortsend();
	char buf[BUFSIZ], *bufp;
	static int bufsize = BUFSIZ;
	off_t bytes = 0;
#endif
	off_t hashbytes = HASHBYTES;
	register int c, d;
	struct stat st;
	struct timeval start, stop;
	char *mode;

	if (verbose && printnames) {
		if (local && *local != '-')
			printf("local: %s ", local);
		if (remote)
			printf("remote: %s\n", remote);
	}
	if (proxy) {
		proxtrans(cmd, local, remote);
		return;
	}
	if (curtype != type)
		changetype(type, 0);
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	mode = "w";
	if (setjmp(sendabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT,oldintr);
		if (oldintp)
			(void) signal(SIGPIPE,oldintp);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortsend);
	if (strcmp(local, "-") == 0)
		fin = stdin;
	else if (*local == '|') {
		oldintp = signal(SIGPIPE,SIG_IGN);
		fin = popen(local + 1, "r");
		if (fin == NULL) {
			perror(local + 1);
			(void) signal(SIGINT, oldintr);
			(void) signal(SIGPIPE, oldintp);
			code = -1;
			return;
		}
		closefunc = pclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 ||
		    (st.st_mode&S_IFMT) != S_IFREG) {
			fprintf(stdout, "%s: not a plain file.\n", local);
			(void) signal(SIGINT, oldintr);
			fclose(fin);
			code = -1;
			return;
		}
	}
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		if (oldintp)
			(void) signal(SIGPIPE, oldintp);
		code = -1;
		if (closefunc != NULL)
			(*closefunc)(fin);
		return;
	}
	if (setjmp(sendabort))
		goto abort;

	if (restart_point &&
	    (strcmp(cmd, "STOR") == 0 || strcmp(cmd, "APPE") == 0)) {
		if (fseek(fin, (long) restart_point, 0) < 0) {
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
			restart_point = 0;
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
		if (command("REST %ld", (long) restart_point)
			!= CONTINUE) {
			restart_point = 0;
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
		restart_point = 0;
		mode = "r+w";
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
	dout = dataconn(mode);
	if (dout == NULL)
		goto abort;
	(void) gettimeofday(&start, (struct timezone *)0);
	oldintp = signal(SIGPIPE, SIG_IGN);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
/*
 * Commands are read via stdio (ie. buffered) functions while the data is read
 * with read().  For performance this is the best way to do it.  However,
 * when using '-' as the local-file arg to 'put', the data comes from the
 * same source as the commands.  If the input is not a tty this causes problems
 * because some of the data is now located in the stdio buffer.  We fix this by
 * calling fread instead of read if our input is stdin.  It would be nice
 * to only read what is in the buffer with fread and then switch to read but
 * we don't know how much to read.  Also, if the data is comming from stdin
 * there probably isn't very much of it.  When calling fread we derive no
 * benefit from using ftp_outbuf but it is there so I am using it.
 */

		if (big_pipe || odirect) {
			/* XXX - breaks hash */
			c = splice(fileno(fin), fileno(dout));
			if (c == -1) {		/* read error */
				fprintf(stderr, "local: %s: %s\n", local,
				    strerror(errno));
			} else if (c == -2) {	/* write error */
				if (errno != EPIPE) 
					perror("netout");
				bytes = -1;
			} else {
				bytes += c;
				d = c;
			}
			if (hash && bytes > 0) {
				if (bytes < HASHBYTES)
					(void) putchar('#');
				(void) putchar('\n');
				(void) fflush(stdout);
			}
			break;
		}
		if (fin == stdin && !isatty(fileno(stdin))) {
			while ((c = fread(ftp_outbuf[b], sizeof (char),
			  obufsize, fin)) > 0) {
				bytes += c;
				for (bufp = ftp_outbuf[b]; c > 0; c -= d,
				  bufp += d)
					if ((d = write(fileno(dout), bufp,
					  c)) <= 0)
						break;
				if (hash) {
					while (bytes >= hashbytes) {
						(void) putchar('#');
						hashbytes += HASHBYTES;
					}
					(void) fflush(stdout);
				}
				b++;
				if (b == NBUFS) {
					b = 0;
				}
			}
		} else {
			while ((c = read(fileno(fin), ftp_outbuf[b],
			  obufsize)) > 0) {
				bytes += c;
				for (bufp = ftp_outbuf[b]; c > 0; c -= d,
				  bufp += d)
					if ((d = write(fileno(dout),
					  bufp, c)) <= 0)
						break;
				if (hash) {
					while (bytes >= hashbytes) {
						(void) putchar('#');
						hashbytes += HASHBYTES;
					}
					(void) fflush(stdout);
				}
				b++;
				if (b == NBUFS) {
					b = 0;
				}
			}
		}
		if (hash && bytes > 0) {
			if (bytes < HASHBYTES)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0)
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
		if (d <= 0) {
			if (d == 0)
				fprintf(stderr, "netout: write returned 0?\n");
			else if (errno != EPIPE) 
				perror("netout");
			bytes = -1;
		}
		break;

	case TYPE_A:
		while ((c = getc(fin)) != EOF) {
			if (c == '\n') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += HASHBYTES;
				}
				if (ferror(dout))
					break;
				(void) putc('\r', dout);
				bytes++;
			}
			(void) putc(c, dout);
			bytes++;
	/*		if (c == '\r') {			  	*/
	/*		(void)	putc('\0', dout);  /* this violates rfc */
	/*			bytes++;				*/
	/*		}                          			*/	
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (ferror(fin))
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
		if (ferror(dout)) {
			if (errno != EPIPE)
				perror("netout");
			bytes = -1;
		}
		break;
	}
	(void) gettimeofday(&stop, (struct timezone *)0);
	if (closefunc != NULL)
		(*closefunc)(fin);
	(void) fclose(dout); data = -1;
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (bytes > 0)
		ptransfer("sent", bytes, &start, &stop);
	return;
abort:
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return;
	}
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (dout)
		(void) fclose(dout);
	(void) getreply(0);
	code = -1;
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
	if (bytes > 0)
		ptransfer("sent", bytes, &start, &stop);
}

jmp_buf	recvabort;

sig_t
abortrecv()
{

	mflag = 0;
	abrtflag = 0;
	printf("\nreceive aborted\nwaiting for remote to finish abort\n");
	(void) fflush(stdout);
#ifdef sgi
	(void)signal(SIGINT, abortrecv);
#endif
	longjmp(recvabort, 1);
}

recvrequest(cmd, local, remote, mode, printnames)
	char *cmd, *local, *remote, *mode;
{
#ifdef sgi
	/* volatile variables are referenced in abort logic
	   which is entered via longjmp so we must guarantee
	   that references to these use the stack address.  Hopefully
	   this will not slow things down significantly. Fixes 644301  */

	FILE * volatile fout, * volatile din = 0;
	int (* volatile closefunc)();
	sig_t (* volatile oldintr)(), (* volatile oldintp)();
	volatile int is_retr;
	volatile off_t bytes = 0;
	int b = 0;
#else
	FILE *fout, *din = 0, *popen();
	int (*closefunc)(), pclose(), fclose();
	sig_t (*oldintr)(), (*oldintp)();
	int is_retr;
	off_t bytes = 0;
#endif
	sig_t abortrecv();
	int tcrflag, bare_lfs = 0;
	char *gunique();
#ifdef sgi
	/* use a global buffer to allow page flipping.
	 */
	static int bufsize = FTPBUFSIZ;
#else
	static int bufsize;
	static char *buf;
#endif
	off_t hashbytes = HASHBYTES;
	register int c, d;
	struct timeval start, stop;
	struct stat st;
#ifndef sgi
	extern char *malloc();
#endif

	is_retr = strcmp(cmd, "RETR") == 0;
	if (is_retr && verbose && printnames) {
		if (local && *local != '-')
			printf("local: %s ", local);
		if (remote)
			printf("remote: %s\n", remote);
	}
	if (proxy && is_retr) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && is_retr;
	if (setjmp(recvabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortrecv);
	if (strcmp(local, "-") && *local != '|') {
		if (access(local, 2) < 0) {
			char *dir = rindex(local, '/');

			if (errno != ENOENT && errno != EACCES) {
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (dir != NULL)
				*dir = 0;
			d = access(dir ? local : ".", 2);
			if (dir != NULL)
				*dir = '/';
			if (d < 0) {
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (!runique && errno == EACCES &&
			    chmod(local, 0600) < 0) {
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
				(void) signal(SIGINT, oldintr);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (runique && errno == EACCES &&
			   (local = gunique(local)) == NULL) {
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
		}
		else if (runique && (local = gunique(local)) == NULL) {
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
	}
	if (!is_retr) {
		if (curtype != TYPE_A)
			changetype(TYPE_A, 0);
	} else if (curtype != type)
		changetype(type, 0);
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	if (setjmp(recvabort))
		goto abort;
	if (is_retr && restart_point &&
	    command("REST %ld", (long) restart_point) != CONTINUE)
		return;
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			return;
		}
	} else {
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			return;
		}
	}
	din = dataconn("r");
	if (din == NULL)
		goto abort;
	if (strcmp(local, "-") == 0)
		fout = stdout;
	else if (*local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fout = popen(local + 1, "w");
		if (fout == NULL) {
			perror(local+1);
			goto abort;
		}
		closefunc = pclose;
	} else {
		fout = fopen(local, mode);
		if (fout == NULL) {
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
			goto abort;
		}
		closefunc = fclose;
	}
#ifndef sgi
	if (fstat(fileno(fout), &st) < 0 || st.st_blksize == 0)
		st.st_blksize = BUFSIZ;
	if (st.st_blksize > bufsize) {
		if (buf)
			(void) free(buf);
		buf = malloc((unsigned)st.st_blksize);
		if (buf == NULL) {
			perror("malloc");
			bufsize = 0;
			goto abort;
		}
		bufsize = st.st_blksize;
	}
#endif
	(void) gettimeofday(&start, (struct timezone *)0);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		if (restart_point &&
		    lseek(fileno(fout), (long) restart_point, L_SET) < 0) {
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
			if (closefunc != NULL)
				(*closefunc)(fout);
			return;
		}
		errno = d = 0;

		if (big_pipe || odirect) {
			c = splice(fileno(din), fileno(fout));
			if (c >= 0) {
				bytes += c;
			}
			if (errno != ENOTTY) {
				break;
			}
			/* fall through */
		}
		while ((c = read(fileno(din), ftp_inbuf[b], ibufsize)) > 0) {
			if ((d = write(fileno(fout), ftp_inbuf[b], c)) != c)
				break;
			b++;
			if (b == NBUFS) {
				b = 0;
			}
			bytes += c;
			if (hash) {
				while (bytes >= hashbytes) {
					(void) putchar('#');
					hashbytes += HASHBYTES;
				}
				(void) fflush(stdout);
			}
		}
		if (hash && bytes > 0) {
			if (bytes < HASHBYTES)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0) {
			if (errno != EPIPE)
				perror("netin");
			bytes = -1;
		}
		if (d < c) {
			if (d < 0)
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
			else
				fprintf(stderr, "%s: short write\n", local);
		}
		break;

	case TYPE_A:
		if (restart_point) {
			register int i, n, c;

			if (fseek(fout, 0L, L_SET) < 0)
				goto done;
			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fout)) == EOF)
					goto done;
				if (c == '\n')
					i++;
			}
			if (fseek(fout, 0L, L_INCR) < 0) {
done:
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
				if (closefunc != NULL)
					(*closefunc)(fout);
				return;
			}
		}
		while ((c = getc(din)) != EOF) {
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += HASHBYTES;
				}
				bytes++;
				if ((c = getc(din)) != '\n' || tcrflag) {
					if (ferror(fout))
						goto break2;
					(void) putc('\r', fout);
					if (c == '\0') {
						bytes++;
						goto contin2;
					}
					if (c == EOF)
						goto contin2;
				}
			}
			(void) putc(c, fout);
			bytes++;
	contin2:	;
		}
break2:
		if (bare_lfs) {
			printf("WARNING! %d bare linefeeds received in ASCII mode\n", bare_lfs);
			printf("File may not have transferred correctly.\n");
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (ferror(din)) {
			if (errno != EPIPE)
				perror("netin");
			bytes = -1;
		}
		if (ferror(fout))
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
		break;
	}
	if (closefunc != NULL)
		(*closefunc)(fout);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) fclose(din); data = -1;
	(void) getreply(0);
	if (bytes > 0 && is_retr)
		ptransfer("received", bytes, &start, &stop);
	return;
abort:

/* abort using RFC959 recommended IP,SYNC sequence  */

	(void) gettimeofday(&stop, (struct timezone *)0);
	if (oldintp)
		(void) signal(SIGPIPE, oldintr);
	(void) signal(SIGINT, SIG_IGN);
	if (!cpend) {
		code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}

	abort_remote(din);
	code = -1;
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din)
		(void) fclose(din);
	if (bytes > 0)
		ptransfer("received", bytes, &start, &stop);
	(void) signal(SIGINT, oldintr);
}

/*
 * Need to start a listen on the data channel
 * before we send the command, otherwise the
 * server's connect may fail.
 */
int sendport;

initconn()
{
	register char *p, *a;
	int result, len, tmpno = 0;
	int on = 1;

noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.sin_port = 0;	/* let system pick one */ 
	if (data != -1)
		(void) close(data);
	data = socket(AF_INET, SOCK_STREAM, 0);
	if (data < 0) {
		perror("ftp: socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}

	if (big_pipe) {		/* this one is needed */
		int	sz = big_pipe;

		setsockopt(data, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
		setsockopt(data, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
  	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0) {
			perror("ftp: setsockopt (reuse address)");
			goto bad;
		}
	if (bind(data, (struct sockaddr *)&data_addr, sizeof (data_addr)) < 0) {
		perror("ftp: bind");
		goto bad;
	}
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on, sizeof (on)) < 0)
		perror("ftp: setsockopt (ignored)");
	len = sizeof (data_addr);
	if (getsockname(data, (struct sockaddr *)&data_addr, &len) < 0) {
		perror("ftp: getsockname");
		goto bad;
	}
	if (listen(data, 1) < 0)
		perror("ftp: listen");
	if (sendport) {
		a = (char *)&data_addr.sin_addr;
		p = (char *)&data_addr.sin_port;
#define	UC(b)	(((int)b)&0xff)
		result =
		    command("PORT %d,%d,%d,%d,%d,%d",
		      UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
		      UC(p[0]), UC(p[1]));
		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
#ifdef IP_TOS
	on = IPTOS_THROUGHPUT;
#ifdef notyet
	if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
		perror("ftp: setsockopt TOS (ignored)");
#endif
#endif
	return (0);
bad:
	(void) close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
}

FILE *
dataconn(mode)
	char *mode;
{
	struct sockaddr_in from;
	int s, fromlen = sizeof (from), tos;

	s = accept(data, (struct sockaddr *) &from, &fromlen);
	if (s < 0) {
		perror("ftp: accept");
		(void) close(data), data = -1;
		return (NULL);
	}
	(void) close(data);
	data = s;
#ifdef IP_TOS
	tos = IPTOS_THROUGHPUT;
#ifdef notyet
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		perror("ftp: setsockopt TOS (ignored)");
#endif
#endif
	return (fdopen(data, mode));
}

ptransfer(direction, bytes, t0, t1)
	char *direction;
	off_t bytes;
	struct timeval *t0, *t1;
{
	struct timeval td;
	float s, bs;

	if (verbose) {
		tvsub(&td, t1, t0);
		s = td.tv_sec + (td.tv_usec / 1000000.);
#define	nz(x)	((x) == 0 ? 1 : (x))
		bs = bytes / nz(s);
#ifdef sgi
		printf("%lld bytes %s in %.2f seconds (%.2f Kbytes/s)\n",
#else
		printf("%lld bytes %s in %.2g seconds (%.2g Kbytes/s)\n",
#endif
		    bytes, direction, s, bs / 1024.);
	}
}

/*tvadd(tsum, t0)
	struct timeval *tsum, *t0;
{

	tsum->tv_sec += t0->tv_sec;
	tsum->tv_usec += t0->tv_usec;
	if (tsum->tv_usec > 1000000)
		tsum->tv_sec++, tsum->tv_usec -= 1000000;
} */

tvsub(tdiff, t1, t0)
	struct timeval *tdiff, *t1, *t0;
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

sig_t
psabort()
{
	extern int abrtflag;

	abrtflag++;
}

pswitch(flag)
	int flag;
{
	extern int proxy, abrtflag;
	sig_t (*oldintr)();
	static struct comvars {
		int connect;
		char name[MAXHOSTNAMELEN];
		struct sockaddr_in mctl;
		struct sockaddr_in hctl;
		FILE *in;
		FILE *out;
		int tpe;
		int curtpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[MAXPATHLEN];
		char mo[MAXPATHLEN];
	} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = signal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname) {
		(void) strncpy(ip->name, hostname, sizeof(ip->name) - 1);
		ip->name[strlen(ip->name)] = '\0';
	} else
		ip->name[0] = 0;
	hostname = op->name;
	ip->hctl = hisctladdr;
	hisctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = cin;
	cin = op->in;
	ip->out = cout;
	cout = op->out;
	ip->tpe = type;
	type = op->tpe;
	ip->curtpe = curtype;
	curtype = op->curtpe;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void) strncpy(ip->nti, ntin, 16);
	(ip->nti)[strlen(ip->nti)] = '\0';
	(void) strcpy(ntin, op->nti);
	(void) strncpy(ip->nto, ntout, 16);
	(ip->nto)[strlen(ip->nto)] = '\0';
	(void) strcpy(ntout, op->nto);
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void) strncpy(ip->mi, mapin, MAXPATHLEN - 1);
	(ip->mi)[strlen(ip->mi)] = '\0';
	(void) strcpy(mapin, op->mi);
	(void) strncpy(ip->mo, mapout, MAXPATHLEN - 1);
	(ip->mo)[strlen(ip->mo)] = '\0';
	(void) strcpy(mapout, op->mo);
	(void) signal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)();
	}
}

jmp_buf ptabort;
int ptabflg;

sig_t
abortpt()
{
	printf("\n");
	(void) fflush(stdout);
#ifdef sgi
	(void)signal(SIGINT, abortpt);
#endif
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	longjmp(ptabort, 1);
}

proxtrans(cmd, local, remote)
	char * volatile cmd, *local, *remote;
{
#ifdef sgi
	/* volatile variables are referenced in abort logic
	   which is entered via longjmp so we must guarantee
	   that references to these use the stack address.  Hopefully
	   this will not slow things down significantly.               */
	
	sig_t (* volatile oldintr)();
	volatile int secndflag = 0;
	char * volatile cmd2;
#else
	sig_t (*oldintr)();
	int secndflag = 0;
	char *cmd2;
#endif
	sig_t abortpt();
	int prox_type, nfnd;
	extern jmp_buf ptabort;
	struct fd_set mask;

	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if ((prox_type = type) == 0) {
		if (unix_server && unix_proxy)
			prox_type = TYPE_I;
		else
			prox_type = TYPE_A;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PASV") != COMPLETE) {
		printf("proxy server does not support third party transfers.\n");
		return;
	}
	pswitch(0);
	if (!connected) {
		printf("No primary connection\n");
		pswitch(1);
		code = -1;
		return;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PORT %s", pasv) != COMPLETE) {
		pswitch(1);
		return;
	}
	if (setjmp(ptabort))
		goto abort;
	oldintr = signal(SIGINT, abortpt);
	if (command("%s %s", cmd, remote) != PRELIM) {
		(void) signal(SIGINT, oldintr);
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if (command("%s %s", cmd2, local) != PRELIM)
		goto abort;
	ptflag++;
	(void) getreply(0);
	pswitch(0);
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	pswitch(1);
	ptflag = 0;
	printf("local: %s remote: %s\n", local, remote);
	return;
abort:
	(void) signal(SIGINT, SIG_IGN);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if (!strcmp(cmd, "RETR") && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote((FILE *) NULL);
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}
	if (cpend)
		abort_remote((FILE *) NULL);
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote((FILE *) NULL);
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void) signal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend)
		abort_remote((FILE *) NULL);
	pswitch(!proxy);
	if (cpend) {
		FD_ZERO(&mask);
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 10)) <= 0) {
			if (nfnd < 0) {
				perror("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer();
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	if (proxy)
		pswitch(0);
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void) signal(SIGINT, oldintr);
}

reset()
{
	struct fd_set mask;
	int nfnd = 1;

	FD_ZERO(&mask);
	while (nfnd > 0) {
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask,0)) < 0) {
			perror("reset");
			code = -1;
			lostpeer();
		}
		else if (nfnd) {
			(void) getreply(0);
		}
	}
}

char *
gunique(local)
	char *local;
{
	static char new[MAXPATHLEN];
	char *cp = rindex(local, '/');
	int d, count=0;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp ? local : ".", 2);
	if (cp)
		*cp = '/';
	if (d < 0) {
		fprintf(stderr, "local: %s: %s\n", local, strerror(errno));
		return((char *) 0);
	}
	(void) strcpy(new, local);
	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			printf("runique: can't find unique file name.\n");
			return((char *) 0);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, 0)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return(new);
}

abort_remote(din)
FILE *din;
{
	char buf[BUFSIZ];
	int nfnd;
	struct fd_set mask;

	/*
	 * send IAC in urgent mode instead of DM because 4.3BSD places oob mark
	 * after urgent byte rather than before as is protocol now
	 */
	sprintf(buf, "%c%c%c", IAC, IP, IAC);
	if (send(fileno(cout), buf, 3, MSG_OOB) != 3)
		perror("abort");
	fprintf(cout,"%cABOR\r\n", DM);
	(void) fflush(cout);
	FD_ZERO(&mask);
	FD_SET(fileno(cin), &mask);
	if (din) { 
		FD_SET(fileno(din), &mask);
	}
	if ((nfnd = empty(&mask, 10)) <= 0) {
		if (nfnd < 0) {
			perror("abort");
		}
		if (ptabflg)
			code = -1;
		lostpeer();
	}
	if (din && FD_ISSET(fileno(din), &mask)) {
		while (read(fileno(din), buf, BUFSIZ) > 0)
			/* LOOP */;
	}
	if (getreply(0) == ERROR && code == 552) {
		/* 552 needed for nic style abort */
		(void) getreply(0);
	}
	(void) getreply(0);
}
