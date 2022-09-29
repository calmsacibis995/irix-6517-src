
/*
 * This is an rcmd() replacement originally by 
 * Chris Siebenmann <cks@utcc.utoronto.ca>.
 */

#ifndef lint
static char RCSid[] = 
"$Id: rshrcmd.c,v 1.3 1995/09/22 07:37:11 ack Exp $";
#endif

#include	"defs.h"

#if	!defined(DIRECT_RCMD)

#include      <sys/types.h>
#include      <sys/socket.h>
#include      <sys/wait.h>
#include      <signal.h>
#include      <errno.h>
#include      <netdb.h>
#include      <stdio.h>

/*
 * This is a replacement rcmd() function that uses the rsh(1c)
 * program in place of a direct rcmd() function call so as to
 * avoid having to be root.
 */
rshrcmd(ahost, port, luser, ruser, cmd, fd2p)
	char  	**ahost;
	u_short	port;
	char	*luser, *ruser, *cmd;
	int	*fd2p;
{
	int             cpid;
	struct hostent  *hp;
	int             sp[2];

	/* insure that we are indeed being used as we thought. */
	if (fd2p != 0)
		return -1;
	/* validate remote hostname. */
	hp = gethostbyname(*ahost);
	if (hp == 0) {
		error("%s: unknown host", *ahost);
		return -1;
	}
	/* *ahost = hp->h_name; *//* This makes me nervous. */

	/* get a socketpair we'll use for stdin and stdout. */
	if (getsocketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0) {
		error("socketpair(AF_UNIX, SOCK_STREAM, 0) failed: %s.", 
		      SYSERR);
		return -1;
	}

	cpid = fork();
	if (cpid < 0) {
		error("fork failed: %s.", SYSERR);
		return -1;      /* error. */
	}
	if (cpid == 0) {
		/* child. we use sp[1] to be stdin/stdout, and close
		   sp[0]. */
		(void) close(sp[0]);
		if (dup2(sp[1], 0) < 0 || dup2(0,1) < 0 || dup2(0, 2) < 0) {
			error("dup2 failed: %s.", SYSERR);
			_exit(255);
		}
		/* fork again to lose parent. */
		cpid = fork();
		if (cpid < 0) {
			error("fork to lose parent failed: %s.", SYSERR);
			_exit(255);
		}
		if (cpid > 0)
			_exit(0);
		/* in grandchild here. */

		/*
		 * If we are rdist'ing to "localhost" as the same user
		 * as we are, then avoid running remote shell for efficiency.
		 */
		if (strcmp(*ahost, "localhost") == 0 &&
		    strcmp(luser, ruser) == 0) {
			execlp(_PATH_BSHELL, basename(_PATH_BSHELL), "-c",
			       cmd, (char *) NULL);
			error("execlp %s failed: %s.", _PATH_BSHELL, SYSERR);
		} else {
			execlp(path_remsh, basename(path_remsh), 
			       *ahost, "-l", ruser, cmd, (char *) NULL);
			error("execlp %s failed: %s.", path_remsh, SYSERR);
		}
		_exit(255);
	}
	if (cpid > 0) {
		/* parent. close sp[1], return sp[0]. */
		(void) close(sp[1]);
		/* reap child. */
		(void) wait(0);
		return sp[0];
	}
	/*NOTREACHED*/
}

#endif	/* !DIRECT_RCMD */
