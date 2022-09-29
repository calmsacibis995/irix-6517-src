/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpsched/lpsched/RCS/terminate.c,v 1.1 1992/12/14 11:34:50 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "lpsched.h"

/**
 ** terminate() - STOP A CHILD PROCESS
 **/

void
#if	defined(__STDC__)
terminate (
	register EXEC *		ep
)
#else
terminate (ep)
	register EXEC		*ep;
#endif
{
	ENTRY ("terminate")

	if (ep->pid > 0) {

#if	defined(DEBUG)
		if (debug & DB_EXEC)
			execlog (
				"KILL: pid %d%s%s\n",
				ep->pid,
				((ep->flags & EXF_KILLED)?
					  ", second time"
					: ""
				),
				(kill(ep->pid, 0) == -1?
					  ", but child is GONE!"
					: ""
				)
			);
#endif

		if (ep->flags & EXF_KILLED)
			return;
		ep->flags |= EXF_KILLED;

		/*
		 * Theoretically, the following "if-then" is not needed,
		 * but there's some bug in the code that occasionally
		 * prevents us from hearing from a finished child.
		 * (Kill -9 on the child would do that, of course, but
		 * the problem has occurred in other cases.)
		 */
		if (kill(-ep->pid, SIGTERM) == -1 && errno == ESRCH) {
			ep->pid = -99;
			ep->status = SIGTERM;
			ep->errno = 0;
			DoneChildren++;
		}
	}
	return;
}
