/* @(#)rexd.c	1.3 87/08/13 3.2/4.3NFSSRC */
# ifdef lint
static char sccsid[] = "@(#)rexd.c 1.1 86/09/25 Copyright 1985, 1987 Sun Microsystems, Inc.";
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
 * rexd - a remote execution daemon based on SUN Remote Procedure Calls
 *
 * Copyright (c) 1985, 1987 Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <rpc/rpc.h>
#include <stdio.h>
#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <syslog.h>
# include "rexioctl.h"
#include <mntent.h>
#include <errno.h>
#include <syslog.h>

#include "rex.h"

# define ListnerTimeout 300	/* seconds listner stays alive */
# define WaitLimit 10		/* seconds to wait after io is closed */

SVCXPRT *ListnerTransp;		/* non-null means still a listner */
static char **Argv;		/* saved argument vector (for ps) */
static char *LastArgv;		/* saved end-of-argument vector */

/*
 * The original code uses fprintfs to log messages. The following
 * hacks change the printfs and perrors into syslog calls.
 *
 * Note: to avoid "rexd: rexd: " syslog entries, the leading "rexd:"
 * was removed from all error messages in this file. No ifdef's were used.
 */

extern int using_syslog;
#define fprintf	syslog
#undef stderr
#define stderr  LOG_ERR

static void
perror(msg)
    const char *msg;
{
    syslog(LOG_ERR, "%s: %m", msg);
}

#define killpg(pid, sig)	 kill(-pid, sig);
fd_set HelperMask;		/* files for interactive mode */


main(argc, argv)
	int argc;
	char **argv;
{
	  /*
	   * the server is a typical RPC daemon, except that we only
	   * accept TCP connections.
	   */
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);
	void dorex();
	void ListnerTimer(), CatchChild();
	fd_set readfds;

	/*
	 * Remember the start and extent of argv for setproctitle().
	 * Open the console for error printouts, but don't let it be
	 * our controlling terminal.
	 */
	Argv = argv;

	if (argc > 0)
		LastArgv = argv[argc-1] + strlen(argv[argc-1]);
	else
		LastArgv = NULL;

	openlog("rexd", LOG_PID, LOG_AUTH);
	using_syslog = 1;
	signal(SIGPIPE, SIG_IGN);
	NoControl();
 	signal(SIGCHLD, CatchChild);
	signal(SIGALRM, ListnerTimer);
# ifdef NOINETD
	if ((ListnerTransp = svctcp_create(RPC_ANYSOCK, 0, 0)) == NULL) {
		fprintf(stderr, "svctcp_create: error\n");
		exit(1);
	}
	pmap_unset(REXPROG, REXVERS);
	if (!svc_register(ListnerTransp, REXPROG, REXVERS, 
			dorex, IPPROTO_TCP)) {
		fprintf(stderr, "service rpc register: error\n");
		exit(1);
	}
# else NOINETD
	if ((ListnerTransp = svctcp_create(0, 0, 0)) == NULL) {
		fprintf(stderr, "svctcp_create: error\n");
		exit(1);
	}
	if (!svc_register(ListnerTransp, REXPROG, REXVERS, dorex, 0)) {
		fprintf(stderr, "service rpc register: error\n");
		exit(1);
	}
	alarm(ListnerTimeout);
# endif NOINETD

/*
 * normally we would call svc_run() at this point, but we need to be informed
 * of when the RPC connection is broken, in case the other side crashes.
 */
	while (TRUE) {
		extern fd_set svc_fdset;
		extern int errno;
		fd_set	tmp_fd;

		if ( !test_fd(&svc_fdset) ) {
			char *foo;

			(void) rex_wait(&foo);
			exit(1);
		}
		readfds = svc_fdset;
		fd_or(&readfds, &HelperMask);
		switch (select(FD_SETSIZE, &readfds, (fd_set *)0, (fd_set *)0, 0)) {
		case -1:  if (errno == EINTR) continue;
			perror("select failed");
			exit(1);
		case 0: 
			fprintf(stderr,"Select returned zero\r\n");
			continue;
		default:
			tmp_fd = readfds;
			if (test_fd(&HelperMask)) {
				fd_and(&tmp_fd, &HelperMask);
				HelperRead(tmp_fd);
			}
			fd_and(&readfds, &svc_fdset);
			svc_getreqset(&readfds);
		}
	}
}


/*
 * This function gets called after the listner has timed out waiting
 * for any new connections coming in.
 */
void
ListnerTimer()
{
  svc_destroy(ListnerTransp);
  exit(0);
}


/*
 * dorex - handle one of the rex procedure calls, dispatching to the 
 *	correct function.
 */
void
dorex(rqstp, transp)
	register struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	struct rex_start *rst;
	struct rex_result result;
	void rex_cleanup();
	
	if (ListnerTransp) {
		  /*
		   * First call - fork a server for this connection
		   */
		extern fd_set svc_fdset;
		int fd, pid;

		pid = fork();
		if (pid > 0) {
		    /*
		     * Parent - return to service loop to accept further
		     * connections.
		     */
			alarm(ListnerTimeout);
			xprt_unregister(transp);
		     	close(transp->xp_sock);
			FD_CLR(transp->xp_sock, &svc_fdset);
			return;
		}
		  /*
		   * child - close listner transport to avoid confusion
		   * Also need to close all other service transports
		   * besides the one we are interested in.
		   */
		alarm(0);
		if (transp != ListnerTransp) {
			close(ListnerTransp->xp_sock);
			xprt_unregister(ListnerTransp);
		}
		ListnerTransp = NULL;
		for (fd=0; fd<FD_SETSIZE; fd++)
		  if (fd != transp->xp_sock && FD_ISSET(fd, &svc_fdset) ) {
			close(fd);
			FD_CLR(fd, &svc_fdset);
		  }
	}

	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (svc_sendreply(transp, xdr_void, 0) == FALSE) {
			fprintf(stderr, "nullproc err");
			exit(1);
		}
		return;

	case REXPROC_START:
		rst = (struct rex_start *)malloc(sizeof (struct rex_start));
		bzero((char *)rst, sizeof *rst);
		if (svc_getargs(transp, xdr_rex_start, rst) == FALSE) {
			svcerr_decode(transp);
			exit(1);
		}
		if (rqstp->rq_cred.oa_flavor != AUTH_UNIX) {
			svcerr_auth(transp, AUTH_BADCRED);
			exit(1);
		}
		result.rlt_stat = rex_start(rst,
			(struct authunix_parms *)rqstp->rq_clntcred,
			&result.rlt_message, transp->xp_sock);
		if (svc_sendreply(transp, xdr_rex_result, &result) == FALSE) {
			fprintf(stderr, "reply failed\n");
			rex_cleanup();
			exit(1);
		}
		if (result.rlt_stat) {
			rex_cleanup();
			exit(0);
		}
		return;

	case REXPROC_MODES:
		{
		    struct rex_ttymode mode;
		    if (svc_getargs(transp, xdr_rex_ttymode, &mode)==FALSE) {
			svcerr_decode(transp);
			exit(1);
		    }
		    SetPtyMode(&mode);
		    if (svc_sendreply(transp, xdr_void, 0) == FALSE) {
			fprintf(stderr, "mode reply failed");
			exit(1);
		    }
		}
		return;

	case REXPROC_WINCH:
		{
		    struct ttysize size;
		    if (svc_getargs(transp, xdr_rex_ttysize, &size)==FALSE) {
			svcerr_decode(transp);
			exit(1);
		    }
		    SetPtySize(&size);
		    if (svc_sendreply(transp, xdr_void, 0) == FALSE) {
			fprintf(stderr, "window change reply failed");
			exit(1);
		    }
		}
		return;

	case REXPROC_SIGNAL:
		{
		    int sigNumber;

		    if (svc_getargs(transp, xdr_int, &sigNumber)==FALSE) {
			svcerr_decode(transp);
			exit(1);
		    }
		    SendSignal(sigNumber);
		    if (svc_sendreply(transp, xdr_void, 0) == FALSE) {
			fprintf(stderr, "signal reply failed");
			exit(1);
		    }
		}
		return;

	case REXPROC_WAIT:
		result.rlt_stat = rex_wait(&result.rlt_message);
		if (svc_sendreply(transp, xdr_rex_result, &result) == FALSE) {
			fprintf(stderr, "reply failed\n");
			exit(1);
		}
		exit(0);
		/* NOTREACHED */

	default:
		svcerr_noproc(transp);
		exit(1);
	}
}

int HasHelper = 0;		/* must kill helpers (interactive mode) */
int child = 0;			/* pid of the executed process */
int ChildStatus = 0;		/* saved return status of child */
int ChildDied = 0;		/* true when above is valid */
char nfsdir[MAXPATHLEN];	/* file system we mounted */
char *tmpdir;			/* where above is mounted, NULL if none */

/*
 * signal handler for SIGCHLD - called when user process dies
 */
void
CatchChild()
{
  int		pid;
  wait_t	status;
  
  pid = wait3(&status, WNOHANG, NULL);
  if (pid==child) {
      ChildStatus = status.w_status;
      ChildDied = 1;
      if (HasHelper) KillHelper(child);
      HasHelper = 0;
  }
  signal(SIGCHLD, CatchChild);
}

/*
 * rex_wait - wait for command to finish, unmount the file system,
 * and return the exit status.
 * message gets an optional string error message.
 */
rex_wait(message)
	char **message;
{
	static char error[1024];
	int count;

	*message = error;
	strcpy("",error);
	if (child == 0) {
		errprintf(error,"No process to wait for!\n");
		rex_cleanup();
		return (1);
	}
	kill(child, SIGHUP);
	for (count=0;!ChildDied && count<WaitLimit;count++)
		sleep(1);
	rex_cleanup();
	if (ChildStatus & 0xFF)
		return (ChildStatus);
	return (ChildStatus >> 8);
}


/*
 * cleanup - unmount and remove our temporary directory
 */
void
rex_cleanup()
{
	if (tmpdir) {
		chdir("/");
		if (umount_nfs(nfsdir, tmpdir))
			fprintf(stderr,
				 "couldn't umount %s\n", nfsdir);
		rmdir(tmpdir);
		tmpdir = NULL;
	}
      if (HasHelper) KillHelper(child);
      HasHelper = 0;
}


/*
 * This function does the server work to get a command executed
 * Returns 0 if OK, nonzero if error
 */

rex_start(rst, ucred, message, sock)
	struct rex_start *rst;
	struct authunix_parms *ucred;
	char **message;
	int sock;
{
	char hostname[255];
	char *p, *wdhost, *fsname, *subdir;
	char dirbuf[1024];
	static char error[1024];
	char defaultShell[1024];
	struct sockaddr_in sin;
	int len;
	int fd0, fd1, fd2;
	uid_t euid, egid;
	extern char **environ;

	if (child) {	/* already started */
		killpg(child, SIGKILL);
		return (1);
	}
	*message = error;
	strcpy("",error);
	signal(SIGCHLD, CatchChild);

	if (ValidUser(ucred->aup_machname,ucred->aup_uid,error,defaultShell))
		return(1);
	wdhost = rst->rst_host;
	fsname = rst->rst_fsname;
	subdir = rst->rst_dirwithin;
	gethostname(hostname, 255);
	if (strcmp(wdhost, hostname) == 0) {
		  /*
		   * The requested directory is local to our machine,
		   * so just change to it.
		   */
		strcpy(dirbuf,fsname);
		strcat(dirbuf,subdir);
		if (chdir(dirbuf)) {
			errprintf(error, "can't chdir to %s\n", dirbuf);
			return (1);
		}
	} else {
		strcpy(dirbuf,wdhost);
		strcat(dirbuf,":");
		strcat(dirbuf,fsname);
		if (AlreadyMounted(dirbuf)) {
		  /*
		   * The requested directory is already mounted, so
		   * just change to it. It might be mounted in a
		   * different place, so be careful.
		   * (dirbuf is modified in place!)
		   */
			strcat(dirbuf, subdir);
			egid = getegid();
			setegid(ucred->aup_gid);
			euid = geteuid();
			seteuid(ucred->aup_uid);
			if (chdir(dirbuf)) {
			  	errprintf(error, 
				  "can't chdir to %s\n", dirbuf);
				setegid(egid);
				seteuid(euid);
				return (1);
			}
			setegid(egid);
			seteuid(euid);
		}
		else {
		  /*
		   * The requested directory is not mounted anywhere,
		   * so try to mount our own copy of it.  We set nfsdir
		   * so that it gets unmounted later, and tmpdir so that
		   * it also gets removed when we are done.
		   */
			tmpdir = mktemp("/var/tmp_rex/rexdXXXXXX");
			if (mkdir(tmpdir, 0777)) {
			  	errprintf(error, 
				    "can't create %s\n", tmpdir);
				perror(tmpdir);
				return(1);
			}
			if (mount_nfs(dirbuf, tmpdir, error)) {
				return(1);
			}
			strcpy(nfsdir, dirbuf);
			strcpy(dirbuf, tmpdir);
			strcat(dirbuf, subdir);
			egid = getegid();
			setegid(ucred->aup_gid);
			euid = geteuid();
			seteuid(ucred->aup_uid);
			if (chdir(dirbuf)) {
			  	errprintf(error, 
				    "can't chdir to %s\n", dirbuf);
				setegid(egid);
				seteuid(euid);
				return (1);
			}
			setegid(egid);
			seteuid(euid);
		}
	}

	len = sizeof sin;
	if (getpeername(sock, &sin, &len)) {
		perror("getpeername");
		exit(1);
	}
	fd0 = socket(AF_INET, SOCK_STREAM, 0);
	fd0 = doconnect(&sin, rst->rst_port0,fd0);
	if (rst->rst_port0 == rst->rst_port1) {
	  /*
	   * use the same connection for both stdin and stdout
	   */
		fd1 = fd0;
	}
	if (rst->rst_flags & REX_INTERACTIVE) {
	  /*
	   * allocate a pseudo-terminal pair if necessary
	   */
	   if (AllocatePty(fd0,fd1)) {
	   	errprintf(error,"cannot allocate a pty\n");
		return (1);
	   }
	   HasHelper = 1;
	}
	child = fork();
	if (child < 0) {
		errprintf(error, "can't fork\n");
		return (1);
	}
	if (child) {
	    /*
	     * parent rexd: close network connections if needed,
	     * then return to the main loop.
	     */
		if ((rst->rst_flags & REX_INTERACTIVE)==0) {
			close(fd0);
			close(fd1);
		}
		return (0);
	}
	if (rst->rst_port0 != rst->rst_port1) {
		fd1 = socket(AF_INET, SOCK_STREAM, 0);
		shutdown(fd0, 1);
		fd1 = doconnect(&sin, rst->rst_port1,fd1);
		shutdown(fd1, 0);
	}
	if (rst->rst_port1 == rst->rst_port2) {
	  /*
	   * Use the same connection for both stdout and stderr
	   */
		fd2 = fd1;
	} else {
		fd2 = socket(AF_INET, SOCK_STREAM, 0);
		fd2 = doconnect(&sin, rst->rst_port2,fd2);
		shutdown(fd2, 0);
	}
	if (rst->rst_flags & REX_INTERACTIVE) {
	  /*
	   * use ptys instead of sockets in interactive mode
	   */
	   DoHelper(&fd0, &fd1, &fd2);
	}
	dup2(fd0, 0);
	dup2(fd1, 1);
	dup2(fd2, 2);
	for (fd0 = 3; fd0 < getdtablesize(); fd0++)
		close(fd0);
	environ = rst->rst_env;
	setgid(ucred->aup_gid);
	setuid(ucred->aup_uid);
	signal( SIGINT, SIG_DFL);
	signal( SIGHUP, SIG_DFL);
	signal( SIGQUIT, SIG_DFL);
	if (rst->rst_cmd[0]==NULL) {
	  /*
	   * Null command means execute the default shell for this user
	   */
	    char *args[2];

	    args[0] = defaultShell;
	    args[1] = NULL;
	    execvp(defaultShell, args);
	    fprintf(stderr, "can't exec shell %s\n", defaultShell);
	    exit(1);
	}
	execvp(rst->rst_cmd[0], rst->rst_cmd);
	fprintf(stderr, "can't exec %s\n", rst->rst_cmd[0]);
	exit(1);
	/* NOTREACHED */
}

AlreadyMounted(fsname)
    char *fsname;
{
  /*
   * Search the mount table to see if the given file system is already
   * mounted.  If so, return the place that it is mounted on.
   */
   FILE *table;
   register struct mntent *mt;

   table = setmntent(MOUNTED,"r");
   while ( (mt = getmntent(table)) != NULL) {
   	if (strcmp(mt->mnt_fsname,fsname) == 0) {
	    strcpy(fsname,mt->mnt_dir);
	    endmntent(table);
	    return(1);
	}
   }
   endmntent(table);
   return(0);
}


/*
 * connect to the indicated IP address/port, and return the 
 * resulting file descriptor.  
 */
doconnect(sin, port, fd)
	struct sockaddr_in *sin;
	short port;
	int fd;
{
	sin->sin_port = ntohs(port);
	if (connect(fd, sin, sizeof *sin)) {
		perror("connect");
		exit(1);
	}
	return (fd);
}

/*
*  SETPROCTITLE -- set the title of this process for "ps"
*
*		none.
*
*	Side Effects:
*		Clobbers argv[] of our main procedure.
*/

void
setproctitle(user, host)
	char *user, *host;
{
	register char *tohere;

	tohere = Argv[0];
	if (LastArgv == NULL ||
	    strlen(user) + strlen(host)+3 > (LastArgv - tohere))
		return;
	*tohere++ = '-';	/* So ps prints (rpc.rexd) */
	sprintf(tohere, "%s@%s", user, host);
	while (*tohere++) ;		/* Skip to end of printf output */
	while (tohere < LastArgv) *tohere++ = ' ';  /* Avoid confusing ps */
}


/* 
 * NFSSRC 3.2/4.3 for the VAX*
 * Copyright (C) 1987 Sun Microsystems, Inc.
 * 
 * (*)VAX is a trademark of Digital Equipment Corporation
 */
