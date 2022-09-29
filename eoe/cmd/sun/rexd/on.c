/* @(#)on.c	1.2 87/08/13 3.2/4.3NFSSRC */
# ifdef lint
static char sccsid[] = "@(#)on.c 1.1 86/09/25 Copyright 1985, 1987 Sun Microsystems, Inc.";
/*
 *
 * NFSSRC 3.2/4.3 for the VAX*
 * Copyright (C) 1987 Sun Microsystems, Inc.
 * 
 * (*)VAX is a trademark of Digital Equipment Corporation
 *
 */
# endif lint

/*
 * on - user interface program for remote execution service
 *
 * Copyright (c) 1985, 1987 Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include "rexioctl.h"
#include <rpc/rpc.h>
#include <netdb.h>
#include <errno.h>

#include "rex.h"

# define CommandName "on"	/* given as argv[0] */
# define AltCommandName "dbon"

extern int errno;

/*
 * Note - the following must be long enough for at least two portmap
 * timeouts on the other side.
 */
struct timeval LongTimeout = { 123, 0 };

int Debug = 0;			/* print extra debugging information */
int Only2 = 0;			/* stdout and stderr are the same */
int Interactive = 0;		/* use a pty on server */
int NoInput = 0;		/* don't read standard input */

void Die();
void doaccept(int *fdp);
struct termio newFlags;
struct termio OldFlags;
CLIENT *Client;			/* RPC client handle */
struct ttysize WindowSize;	/* saved window size */

void
onsusp ()		/* stolen from more */
{
    /* ignore SIGTTOU so we don't get stopped if csh grabs the tty */
    signal(SIGTTOU, SIG_IGN);
    ioctl(0, TCSETAW, &OldFlags);
    signal(SIGTTOU, SIG_DFL);
    /* Send the TSTP signal to suspend our process group */
    signal(SIGTSTP, SIG_DFL);
    sigrelse(SIGTSTP);
    sigrelse(SIGTTOU);
    kill (0, SIGTSTP);
    /* Pause for station break */

    /* We're back */
    signal (SIGTSTP, onsusp);
    ioctl(0, TCSETAW, &newFlags);
}

#ifdef SIGWINCH
/*
 * window change handler - propagate to remote server 
 */
void
sigwinch()
{
     struct ttysize size;
     enum clnt_stat clstat;

     ioctl(0, TIOCGWINSZ, &size);
     if (bcmp(&size,&WindowSize,sizeof size)==0) return;
     WindowSize = size;
     if (clstat = clnt_call(Client, REXPROC_WINCH,
    	xdr_rex_ttysize, &size, xdr_void, NULL, LongTimeout)) {
		fprintf(stderr, "on (size): ");
		clnt_perrno(clstat);
		fprintf(stderr, "\r\n");
     }
     signal(SIGWINCH, sigwinch);
}
#endif SIGWINCH

/*
 * interrupt signal handler - propagate to remote server 
 */
void
interrupt(sig)
    int sig;
{
     enum clnt_stat clstat;

     if (clstat = clnt_call(Client, REXPROC_SIGNAL,
    	xdr_int, &sig, xdr_void, NULL, LongTimeout)) {
		fprintf(stderr, "on (signal): ");
		clnt_perrno(clstat);
		fprintf(stderr, "\r\n");
     }
     signal(sig, interrupt);
}

main(argc, argv)
	int argc;
	char **argv;
{
	char *rhost, **cmdp;
	char curdir[MAXPATHLEN];
	char wdhost[MAXPATHLEN];
	char fsname[MAXPATHLEN];
	char dirwithin[MAXPATHLEN];
	struct rex_start rst;
	struct rex_result result;
	extern char **environ, *rindex();
	enum clnt_stat clstat;
	struct hostent *hp;
	struct sockaddr_in server_addr;
	int sock = RPC_ANYSOCK;
	int fd0, fd2;
	fd_set selmask, zmask, remmask;
	int nfds, cc;
	static char buf[4096];

	    /*
	     * we check the invoked command name to see if it should
	     * really be a host name.
	     */
	if ( (rhost = rindex(argv[0],'/')) == NULL) {
		rhost = argv[0];
	}
	else {
		rhost++;
	}

	while (argc > 1 && argv[1][0] == '-') {
	    switch (argv[1][1]) {
	      case 'd': Debug = 1;
	      		break;
	      case 'i': Interactive = 1;
	      		break;
	      case 'n': NoInput = 1;
	      		break;
	      default:
	      		printf("Unknown option %s\n",argv[1]);
	    }
	    argv++;
	    argc--;
	}
	if (strcmp(rhost,CommandName) && strcmp(rhost,AltCommandName)) {
	    cmdp = &argv[1];
	    Interactive = 1;
	} else {
	    if (argc < 2) {
		fprintf(stderr, 
   "Usage: on [-i] [-d] [-n] machine cmd [args]...\n");
		exit(1);
	    }
	    rhost = argv[1];
	    cmdp = &argv[2];
	}

	if ((hp = gethostbyname(rhost)) == NULL) {
		fprintf(stderr, "on: unknown host %s\n", rhost);
		exit(1);
	}
	bcopy(hp->h_addr, (caddr_t)&server_addr.sin_addr, hp->h_length);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = 0;	/* use pmapper */
	if (Debug) printf("Got the host named %s (%s)\n",
			rhost,inet_ntoa(server_addr.sin_addr));
	if ((Client = clnttcp_create(&server_addr, REXPROG, REXVERS, &sock,
	    0, 0)) == NULL) {
		fprintf(stderr, "on: cannot connect to server on %s\n",
			rhost);
		exit(1);
	}
	if (Debug) printf("TCP RPC connection created\n");
	Client->cl_auth = authunix_create_default();

	  /*
	   * Now that we have created the TCP connection, we do some
	   * work while the server daemon is being swapped in.
	   */
	if (getwd(curdir) == 0) {
		fprintf(stderr, "on: can't find . (%s)\n",curdir);
		exit(1);
	}
	if (findmount(curdir,wdhost,fsname,dirwithin) == 0) {
		fprintf(stderr, "on: can't locate mount point for %s (%s)\n",
				curdir, dirwithin);
		exit(1);
	}
	if (Debug) printf("wd host %s, fs %s, dir within %s\n",
		wdhost, fsname, dirwithin);

	Only2 = samefd(1,2);
	rst.rst_cmd = cmdp;
	rst.rst_env = environ;
	rst.rst_host = wdhost;
	rst.rst_fsname = fsname;
	rst.rst_dirwithin = dirwithin;
	rst.rst_port0 = makeport(&fd0);
	rst.rst_port1 =  rst.rst_port0;		/* same port for stdin */
	rst.rst_flags = 0;
	if (Interactive) {

		rst.rst_flags |= REX_INTERACTIVE;
		ioctl(0, TCGETA, &OldFlags);
		newFlags = OldFlags;
		newFlags.c_lflag &= ~(ICANON|ECHO);
		newFlags.c_iflag &= ~(ISTRIP|ICRNL|INLCR|IGNCR|IXON|BRKINT|IXOFF);
		newFlags.c_oflag &= ~(TABDLY|OPOST);
		newFlags.c_oflag |= TAB3;
		newFlags.c_cc[VTIME]=  1;
		newFlags.c_cc[VMIN] = 1;
		ioctl(0, TCSETAW, &newFlags);
		signal(SIGINT, Die);
		signal(SIGTSTP, onsusp);
	}
	if (Only2) {
		rst.rst_port2 = rst.rst_port1;
	} else {
		rst.rst_port2 = makeport(&fd2);
	}
	result.rlt_message = NULL;
	if (clstat = clnt_call(Client, REXPROC_START,
	    xdr_rex_start, &rst, xdr_rex_result, &result, LongTimeout)) {
		fprintf(stderr, "on %s: ", rhost);
		clnt_perrno(clstat);
		fprintf(stderr, "\n");
		Die(1);
	}
	if (result.rlt_stat != 0) {
		fprintf(stderr, "on %s: %s\r",rhost,result.rlt_message);
		Die(1);
	}
	if (Debug) printf("Client call was made\r\n");
	if (Interactive) {
	  /*
	   * Pass the tty modes along to the server 
	   */
	     struct rex_ttymode mode;

	     trans_tty_mode(&mode, &OldFlags);
	     if (clstat = clnt_call(Client, REXPROC_MODES,
	    	xdr_rex_ttymode, &mode, xdr_void, NULL, LongTimeout)) {
			fprintf(stderr, "on (modes) %s: ", rhost);
			clnt_perrno(clstat);
			fprintf(stderr, "\r\n");
	     }
	     ioctl(0, TIOCGWINSZ, &WindowSize);
	     if (clstat = clnt_call(Client, REXPROC_WINCH,
	    	xdr_rex_ttysize, &WindowSize, xdr_void, NULL, LongTimeout)) {
			fprintf(stderr, "on (size) %s: ", rhost);
			clnt_perrno(clstat);
			fprintf(stderr, "\r\n");
	     }
#ifdef SIGWINCH
	     signal(SIGWINCH, sigwinch);
#endif SIGWINCH
	     signal(SIGINT, interrupt);
	     signal(SIGTERM, interrupt);
	     signal(SIGQUIT, interrupt);
	}
	doaccept(&fd0);
	FD_ZERO(&remmask);
	FD_SET(fd0, &remmask);
	if (Debug) printf("accept on stdout\r\n");
	if (!Only2) {
		doaccept(&fd2);
		shutdown(fd2, 1);
		if (Debug) printf("accept on stderr\r\n");
		FD_SET(fd2, &remmask);
	}
	if (NoInput) {
		  /*
		   * no input - simulate end-of-file instead
		   */
		FD_ZERO(&zmask);
		shutdown(fd0, 1);
	}
	else {
		  /*
		   * set up to read standard input, send to remote
		   */
		FD_ZERO(&zmask);
		FD_SET(0, &zmask);
	}
	while (test_fd(&remmask)) {
		selmask = remmask;
		fd_or(&selmask, &zmask);
		nfds = select(FD_SETSIZE, &selmask, 0, 0, 0);
		if (nfds <= 0) {
			if (errno == EINTR) continue;
			perror("on: select");
			Die(1);
		}
		if (FD_ISSET(fd0, &selmask)) {
			cc = read(fd0, buf, sizeof buf);
			if (cc > 0)
				write(1, buf, cc);
			else
				FD_CLR(fd0, &remmask);
		}
		if (!Only2 && FD_ISSET(fd2, &selmask)) {
			cc = read(fd2, buf, sizeof buf);
			if (cc > 0)
				write(2, buf, cc);
			else
				FD_CLR(fd2, &remmask);
		}
		if (!NoInput && FD_ISSET(0, &selmask)) {
			cc = read(0, buf, sizeof buf);
			if (cc > 0)
				write(fd0, buf, cc);
			else {
				/*
				 * End of standard input - shutdown outgoing
				 * direction of the TCP connection.
				 */
				FD_ZERO(&zmask);
				shutdown(fd0, 1);
			}
		}
	}
	close(fd0);
	if (!Only2)
	    close(fd2);
	if (clstat = clnt_call(Client, REXPROC_WAIT,
	    xdr_void, 0, xdr_rex_result, &result, LongTimeout)) {
		fprintf(stderr, "on: ");
		clnt_perrno(clstat);
		fprintf(stderr, "\r\n");
		Die(1);
	}
	Die(result.rlt_stat);
}


/*
 * like exit, but resets the terminal state first 
 */
void
Die(stat)
{
  if (Interactive) {
      ioctl(0, TCSETA, &OldFlags);
      printf("\r\n");
  }
  exit(stat);
}

void
remstop()
{
	exit(23);
	/* NOTREACHED */
}

  /*
   * returns true if we can safely say that the two file descriptors
   * are the "same" (both are same file).
   */
samefd(a,b)
{
    struct stat astat, bstat;
    if (fstat(a,&astat) || fstat(b,&bstat)) return(0);
    if (astat.st_ino == 0 || bstat.st_ino == 0) return(0);
    return( !bcmp( &astat, &bstat, sizeof(astat)) );
}


/*
 * accept the incoming connection on the given
 * file descriptor, and return the new file descritpor
 */
void
doaccept(fdp)
	int *fdp;
{
	int fd;

	fd = accept(*fdp, 0, 0);
	if (fd < 0) {
		perror("accept");
		remstop();
		exit(1);
	}
	close(*fdp);
	*fdp = fd;
}

/*
 * create a socket, and return its the port number.
 */
makeport(fdp)
	int *fdp;
{
	struct sockaddr_in sin;
	int fd, len;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}
	bzero((char *)&sin, sizeof sin);
	sin.sin_family = AF_INET;
	bind(fd, &sin, sizeof sin);
	len = sizeof sin;
	getsockname(fd, &sin, &len);
	listen(fd, 1);
	*fdp = fd;
	return (htons(sin.sin_port));
}
