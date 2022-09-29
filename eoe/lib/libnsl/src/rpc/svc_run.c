/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)librpc:svc_run.c	1.3.4.1"

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989,1990  AT&T.
*	(c) 1990,1991  UNIX System Laboratories, Inc.
*          All rights reserved.
*/ 
#if defined(__STDC__) 
        #pragma weak svc_run	= _svc_run
#endif
#include <libc_synonyms.h>
#include <libnsl_synonyms.h>

/*
 * This is the rpc server side idle loop
 * Wait for input, call server program.
 */
#include <rpc/rpc.h>
#include <sys/errno.h>
#ifdef SYSLOG
#include <sys/syslog.h>
#else
#define	LOG_ERR 3
#endif /* SYSLOG */

void
svc_run()
{
	fd_set readfds;
	int dtbsize = _rpc_dtbsize();
	int i;
	extern int errno;

	for (;;) {
		/*
		 * Check whether there is any server fd on which we may
		 * to wait.
		 */
		for (i = 0; i < dtbsize; i++)
			if (FD_ISSET(i, &svc_fdset))
				break;
		if (i == dtbsize)
			break;	/* None waiting, hence quit */

		readfds = svc_fdset;
		switch (select(dtbsize, &readfds, (fd_set *)0,
			(fd_set *)0, (struct timeval *)0)) {
		case -1:
			/*
			 * We ignore all other errors except EBADF.  For all
			 * other errors, we just continue with the assumption
			 * that it was set by the signal handlers (or any
			 * other outside event) and not caused by select().
			 */
			if (errno != EBADF) {
				continue;
			}
			(void) syslog(LOG_ERR, "svc_run: - select failed: %m");
			return;
		case 0:
			continue;
		default:
			svc_getreqset(&readfds);
		}
	}
}
