/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/system.c	1.22"
/*	3.0 SID #	1.4	*/
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak system = _system
#endif
#include "synonyms.h"
#include "libc_interpose.h"
#include <priv_extern.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wait.h>
#include <paths.h>
#include <mutex.h>
#include "mplib.h"

#define BIN_SH _PATH_BSHELL

int
system(const char *s)
{
	int status = -1;
	pid_t pid;
	struct sigaction ocstat, cstat;
	struct stat buf;
	int do_sigaction = !MTLIB_ACTIVE();	/* not for threads */

	MTLIB_CNCL_TEST();

	s = __tp_system_pre(s); /* permit perf package to change */
	if (s == NULL) {
		if (stat(BIN_SH, &buf) != 0) {
			return(0);
		} else if (getuid() == buf.st_uid) {
			if ((buf.st_mode & 0100) == 0)
				return(0);
		} else if (getgid() == buf.st_gid) {
			if ((buf.st_mode & 0010) == 0)
				return(0);
		} else if ((buf.st_mode & 0001) == 0) {
			return(0);
		}
		return(1);
	}

	/*
	 * We need to set SIGCLD to DFL since caller may have old sysV
	 * style signal set for SIGCLD (== SIGIGN). If so our
	 * new child won't zombie and we won't be able to wait for it.
	 * Since for POSIX, we always re-scan upon setting SIGCLD
	 * if caller had SIGCLD ignored, any children that die while
	 * we are waiting will be freed when we restore the old mode.
	 * The only case that breaks is a caller that is catching
	 * SIGCLD and wants stopped children
	 */
	if (do_sigaction) {
		sigemptyset(&cstat.sa_mask);
		cstat.sa_handler = SIG_DFL;
		cstat.sa_flags = 0;
		sigaction(SIGCLD, &cstat, &ocstat);
	}

	if ((pid = fork()) == 0) {
		(void) execl(BIN_SH, "sh", (const char *)"-c", s, (char *)0);
		_exit(127);
	}

	if (pid > 0) {
		siginfo_t	info;
		extern int	_wstat(int, int);
		extern int	__waitsys(idtype_t, id_t, siginfo_t *, int,
					  struct rusage *);
		do {
			MTLIB_BLOCK_CNCL_VAL( status,
			  __waitsys(P_PID, pid, &info, WEXITED|WTRAPPED, 0) );
		} while (status < 0 && oserror() == EINTR);
		if (status >= 0) {
			status = _wstat(info.si_code, info.si_status & 0377);
		}
	}

	if (do_sigaction)
		sigaction(SIGCLD, &ocstat, NULL);

	return(status);
}
