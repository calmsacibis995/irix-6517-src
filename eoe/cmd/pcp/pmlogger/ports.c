/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: ports.c,v 2.31 1998/11/15 08:35:24 kenmcd Exp $"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "pmapi.h"
#include "pmapi_dev.h"
#include "impl.h"
#include "./logger.h"

/* The logger will try to allocate port numbers beginning with the number
 * defined below.  If that is in use it will keep adding one and trying again
 * until it allocates a port.
 */
#define PORT_BASE	4330		/* Base of range for port numbers */

static char	*ctlfile = NULL;	/* Control directory/portmap name */
static char	*linkfile = NULL;	/* Link name for primary logger */

int		ctlfd;			/* fd for control port */
int		ctlport;		/* pmlogger control port number */
int		wantflush = 0;		/* flush via SIGUSR1 flag */

static void
cleanup(void)
{
    if (linkfile != NULL)
	unlink(linkfile);
    if (ctlfile != NULL)
	unlink(ctlfile);
}

/*ARGSUSED*/
static void
sigexit_handler(int sig)
{
#ifdef PCP_DEBUG
    fprintf(stderr, "pmlogger: Signalled (signal=%d), exiting\n", sig);
#endif
    cleanup();
    exit(1);
    /*NOTREACHED*/
}

/*ARGSUSED*/
static void
sigcore_handler(int sig)
{
#ifdef PCP_DEBUG
    fprintf(stderr, "pmlogger: Signalled (signal=%d), exiting (core dumped)\n", sig);
#endif
    cleanup();
    signal(SIGABRT, SIG_DFL);		/* Don't come back here on SIGABRT */
    abort();
    /*NOTREACHED*/
}

/*ARGSUSED*/
static void
sighup_handler(int sig)
{
    /* SIGHUP is used to force a log volume change */
    signal(SIGHUP, SIG_IGN);
    newvolume(VOL_SW_SIGHUP);
    signal(SIGHUP, sighup_handler);
}

/*ARGSUSED*/
static void
sigpipe_handler(int sig)
{
    /*
     * just ignore the signal, the write() will fail, and the PDU
     * xmit will return with an error
     */
    signal(SIGPIPE, sigpipe_handler);
}

/*ARGSUSED*/
static void
sigusr1_handler(int sig)
{
    /* set the flag ... flush occurs in x */
    wantflush = 1;
    signal(SIGUSR1, sigusr1_handler);
}

/* This is used to set the dispositions for the various signals received.
 * Try to do the right thing for the various STOP/CONT signals.
 */
static void (*sig_handler[32])(int) = {
    sigexit_handler,	/*	       0		 not used */
    sighup_handler,	/* SIGHUP      1       Exit      Hangup [see termio(7)] */
    sigexit_handler,	/* SIGINT      2       Exit      Interrupt [see termio(7)] */
    sigcore_handler,	/* SIGQUIT     3       Core      Quit [see termio(7)] */
    sigcore_handler,	/* SIGILL      4       Core      Illegal Instruction */
    sigcore_handler,	/* SIGTRAP     5       Core      Trace/Breakpoint Trap */
    sigcore_handler,	/* SIGABRT     6       Core      Abort */
    sigcore_handler,	/* SIGEMT      7       Core      Emulation Trap */
    sigcore_handler,	/* SIGFPE      8       Core      Arithmetic Exception */
    sigexit_handler,	/* SIGKILL     9       Exit      Killed */
    sigcore_handler,	/* SIGBUS      10      Core      Bus Error */
    sigcore_handler,	/* SIGSEGV     11      Core      Segmentation Fault */
    sigcore_handler,	/* SIGSYS      12      Core      Bad System Call */
    sigpipe_handler,	/* SIGPIPE     13      Exit      Broken Pipe */
    sigexit_handler,	/* SIGALRM     14      Exit      Alarm Clock */
    sigexit_handler,	/* SIGTERM     15      Exit      Terminated */
    sigusr1_handler,	/* SIGUSR1     16      Exit      User Signal 1 */
    sigexit_handler,	/* SIGUSR2     17      Exit      User Signal 2 */
    SIG_DFL,		/* SIGCHLD     18      Ignore    Child Status Changed */
    SIG_DFL,		/* SIGPWR      19      Ignore    Power Fail/Restart */
    SIG_DFL,		/* SIGWINCH    20      Ignore    Window Size Change */
    SIG_DFL,		/* SIGURG      21      Ignore    Urgent Socket Condition */

					/* man page inaccuracy: SIGPOLL -> Exit */
    sigexit_handler,	/* SIGPOLL     22      Ignore    Pollable Event [see streamio(7)] */
    SIG_DFL,		/* SIGSTOP     23      Stop      Stopped (signal) */
    SIG_DFL,		/* SIGTSTP     24      Stop      Stopped (user) [see termio(7)] */
    SIG_DFL,		/* SIGCONT     25      Ignore    Continued */
    SIG_DFL,		/* SIGTTIN     26      Stop      Stopped (tty input) [see termio(7)] */
    SIG_DFL,		/* SIGTTOU     27      Stop      Stopped (tty output) [see termio(7)] */
    sigexit_handler,	/* SIGVTALRM   28      Exit      Virtual Timer Expired */
    sigexit_handler,	/* SIGPROF     29      Exit      Profiling Timer Expired */
    sigcore_handler,	/* SIGXCPU     30      Core      CPU time limit exceeded [see getrlimit(2)] */
    sigcore_handler	/* SIGXFSZ     31      Core      File size limit exceeded [see getrlimit(2)] */
};
#define	MIN_SIG	1			/* Usable extent of array above */
#define MAX_SIG 31

/* Create socket for incoming connections and bind to it an address for
 * clients to use.  Only returns if it succeeds (exits on failure).
 */

static int
GetPort(char *file)
{
    int			fd;
    int			mapfd;
    FILE		*mapstream;
    int			i, sts;
    struct sockaddr_in	myAddr;
    struct linger	noLinger = {1, 0};
    static int		port_base = -1;
    struct hostent	*hep;
    extern char	    	*archBase;		/* base name for log files */
    extern char		*pmcd_host;		/* collecting from PMCD on this host */

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
	perror("socket");
	exit(1);
	/*NOTREACHED*/
    }
    i = 0;	/* for purify! */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &i,
		   sizeof(i)) < 0) {
	perror("setsockopt(nodelay)");
	exit(1);
	/*NOTREACHED*/
    }
    /* Don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &noLinger, sizeof(noLinger)) < 0) {
	perror("setsockopt(nolinger)");
	exit(1);
	/*NOTREACHED*/
    }

    if (port_base == -1) {
	/*
	 * get optional stuff from environment ...
	 *	PMLOGGER_PORT
	 */
	char	*env_str;
	if ((env_str = getenv("PMLOGGER_PORT")) != NULL) {
	    char	*end_ptr;

	    port_base = strtol(env_str, &end_ptr, 0);
	    if (*end_ptr != '\0' || port_base < 0) {
		fprintf(stderr, 
			 "GetPort: ignored bad PMLOGGER_PORT = '%s'\n", env_str);
		port_base = PORT_BASE;
	    }
	}
	else
	    port_base = PORT_BASE;
    }

    /*
     * try to allocate ports from port_base.  If port already in use, add one
     * and try again.
     */
    for (ctlport = port_base; ; ctlport++) {
	memset(&myAddr, 0, sizeof(myAddr));
	myAddr.sin_family = AF_INET;
	myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myAddr.sin_port = htons(ctlport);
	sts = bind(fd, (struct sockaddr*)&myAddr, sizeof(myAddr));
	if (sts < 0) {
	    if (errno != EADDRINUSE) {
		fprintf(stderr, "bind(%d): %s\n", ctlport, strerror(errno));
		exit(1);
		/*NOTREACHED*/
	    }
	}
	else
	    break;
    }
    sts = listen(fd, 5);	/* Max. of 5 pending connection requests */
    if (sts == -1) {
	perror("listen");
	exit(1);
	/*NOTREACHED*/
    }

    /* create and initialize the port map file */
    mapfd = open(file, O_WRONLY | O_EXCL | O_CREAT,
		 S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (mapfd == -1) {
	fprintf(stderr, "%s: error creating port map file %s: %s.  Exiting.\n",
		pmProgname, file, strerror(errno));
	exit(1);
	/*NOTREACHED*/
    }
    /* write the port number to the port map file */
    if ((mapstream = fdopen(mapfd, "w")) == NULL) {
	perror("GetPort: fdopen");
	exit(1);
	/*NOTREACHED*/
    }
    /* first the port number */
    fprintf(mapstream, "%d\n", ctlport);

    /* then the PMCD host */
    hep = gethostbyname(pmcd_host);
    fprintf(mapstream, "%s\n", hep == NULL ? "" : hep->h_name);

    /* and finally the full pathname to the archive base */
    if (*archBase == '/')
	fprintf(mapstream, "%s\n", archBase);
    else {
	char		path[MAXPATHLEN];
	if (getcwd(path, MAXPATHLEN) == NULL)
	    fprintf(mapstream, "\n");
	else
	    fprintf(mapstream, "%s/%s\n", path, archBase);
    }


    fclose(mapstream);
    close(mapfd);

    return fd;
}

/* Create the control port for this pmlogger and the file containing the port
 * number so that other programs know which port to connect to.
 * If this is the primary pmlogger, create the special symbolic link to the
 * control file.
 */
void
init_ports(void)
{
    int		i, n, sts;
    int		extlen, baselen;
    pid_t	mypid = getpid();
    extern int	primary;		/* Non-zero for primary logger */

    /* make sure control port files are removed when pmlogger terminates */
    for (i = MIN_SIG; i <= MAX_SIG; i++)
	signal(i, sig_handler[i]);
#if defined(HAVE_ATEXIT)
    for (i = MAX_SIG + 1; i <= SIGRTMAX; i++)
	    signal(i, sigexit_handler);
    if (atexit(cleanup) != 0) {
	perror("atexit");
	fprintf(stderr, "%s: unable to register atexit cleanup function.  Exiting\n",
		pmProgname);
	cleanup();
	exit(1);
	/*NOTREACHED*/
    }
#endif

    /* create the control port file (make the directory if necessary). */

    /* count digits in mypid */
    for (n = mypid, extlen = 1; n ; extlen++)
	n /= 10;
    /* baselen is directory + trailing / */
    baselen = strlen(PM_LOG_PORT_DIR) + 1;
    n = baselen + extlen + 1;
    ctlfile = (char *)malloc(n);
    if (ctlfile == NULL) {
	__pmNoMem("port file name", n, PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    strcpy(ctlfile, PM_LOG_PORT_DIR);
    
    /* try to create the port file directory. OK if it already exists */
    sts = mkdir(ctlfile, S_IRWXU | S_IRWXG | S_IRWXO);
    if (sts < 0 && errno != EEXIST) {
	fprintf(stderr, "%s: error creating port file dir %s: %s\n",
		pmProgname, ctlfile, strerror(errno));
	exit(1);
    }
    chmod(ctlfile, S_IRWXU | S_IRWXG | S_IRWXO | S_ISVTX);

    /* remove any existing port file with my name (it's old) */
    strcat(ctlfile, "/");
    sprintf(ctlfile + baselen, "%d", mypid);
    sts = unlink(ctlfile);
    if (sts == -1 && errno != ENOENT) {
	fprintf(stderr, "%s: error removing %s: %s.  Exiting.\n",
		pmProgname, ctlfile, strerror(errno));
	exit(1);
	/*NOTREACHED*/
    }

    /* get control port and write port map file */
    ctlfd = GetPort(ctlfile);

    /*
     * If this is the primary logger, make the special symbolic link for
     * clients to connect specifically to it.
     */
    if (primary) {
	extlen = strlen(PM_LOG_PRIMARY_LINK);
	n = baselen + extlen + 1;
	linkfile = (char *)malloc(n);
	if (linkfile == NULL) {
	    __pmNoMem("primary logger link file name", n, PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
	strcpy(linkfile, PM_LOG_PORT_DIR);
	strcat(linkfile, "/");
	strcat(linkfile, PM_LOG_PRIMARY_LINK);
	if (symlink(ctlfile, linkfile) != 0) {
	    if (errno == EEXIST)
		fprintf(stderr, "%s: there is already a primary pmlogger running\n", pmProgname);
	    else
		fprintf(stderr, "%s: error creating primary logger link %s: %s\n",
			pmProgname, linkfile, strerror(errno));
	    exit(1);
	    /*NOTREACHED*/
	}
    }
}

/* Service a request on the control port  Return non-zero if a new client
 * connection has been accepted.
 */

int		clientfd = -1;
unsigned int	clientops = 0;		/* for access control (deny ops) */
char		pmlc_host[MAXHOSTNAMELEN];
int		connect_state = 0;

int
control_req(void)
{
    int			fd, sts;
    struct sockaddr_in	addr;
    struct hostent	*hp;
    int			addrlen;
    __pmIPC		ipc = { UNKNOWN_VERSION, NULL };

    addrlen = sizeof(addr);
    fd = accept(ctlfd, (struct sockaddr *)&addr, &addrlen);
    if (fd == -1) {
	perror("error accepting client");
	return 0;
    }
    if (clientfd != -1) {
	sts = __pmSendError(fd, PDU_BINARY, -EADDRINUSE);
	if (sts < 0)
	    fprintf(stderr, "error sending connection NACK to client: %s\n",
			 pmErrStr(sts));
	__pmResetIPC(fd);
	close(fd);
	return 0;
    }

    if ((sts = __pmAddIPC(fd, ipc)) < 0) {
	__pmSendError(fd, PDU_BINARY, sts);
	fprintf(stderr, "error connecting to client: %s\n", pmErrStr(sts));
	close(fd);
	return 0;
    }

    hp = gethostbyaddr((void *)&addr.sin_addr.s_addr, sizeof(addr.sin_addr.s_addr), AF_INET);
    if (hp == NULL || strlen(hp->h_name) > MAXHOSTNAMELEN-1) {
	char	*p = (char *)&addr.sin_addr.s_addr;

	sprintf(pmlc_host, "%d.%d.%d.%d",
		p[0] & 0xff, p[1] & 0xff, p[2] & 0xff, p[3] & 0xff);
    }
    else
	/* this is safe, due to strlen() test above */
	strcpy(pmlc_host, hp->h_name);

    if ((sts = __pmAccAddClient(&addr.sin_addr, &clientops)) < 0) {
	if (sts == PM_ERR_CONNLIMIT || sts == PM_ERR_PERMISSION)
	    sts = XLATE_ERR_2TO1(sts);	/* connect - send these as down-rev */
	sts = __pmSendError(fd, PDU_BINARY, sts);
	if (sts < 0)
	    fprintf(stderr, "error sending connection access NACK to client: %s\n",
			 pmErrStr(sts));
	__pmResetIPC(fd);
	close(fd);
	return 0;
    }

    /* encode pdu version in the acknowledgement */
    sts = __pmSendError(fd, PDU_BINARY, LOG_PDU_VERSION);
    if (sts < 0) {
	fprintf(stderr, "error sending connection ACK to client: %s\n",
		     pmErrStr(sts));
	__pmResetIPC(fd);
	close(fd);
	return 0;
    }
    clientfd = fd;
    return 1;
}
