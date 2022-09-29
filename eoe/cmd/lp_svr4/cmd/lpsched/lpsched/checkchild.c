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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpsched/lpsched/RCS/checkchild.c,v 1.1 1992/12/14 11:32:06 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include	"lpsched.h"

/**
 ** ev_checkchild() - CHECK FOR DECEASED CHILDREN
 **/

#if	defined(CHECK_CHILDREN)

void
#if	defined(__STDC__)
ev_checkchild (
	void
)
#else
ev_checkchild ()
#endif
{
	ENTRY ("ev_checkchild")

	register EXEC		*ep	= &Exec_Table[0],
				*epend	= &Exec_Table[ET_Size];


	/*
	 * This routine is necessary to find out about child
	 * processes that disappear without a trace. An example
	 * of how they might disappear: kill -9 pid.
	 * To minimize a race condition with a dying child,
	 * we don't mark the child as gone unless it hasn't been
	 * seen for two cycles.
	 */
	for ( ; ep < epend; ep++)
		if (ep->pid > 0 && kill(ep->pid, 0) == -1)
			if (ep->flags & EXF_GONE) {
				ep->pid = -99;
				ep->status = SIGTERM;
				ep->errno = 0;
				DoneChildren++;
#if	defined(DEBUG)
				if (debug & (DB_EXEC|DB_DONE)) {
					execlog (
						"LOST! slot %d pid %d\n",
						ep - Exec_Table,
						ep->pid
					);
					execlog ("%e", ep);
				}
#endif
			} else
				ep->flags |= EXF_GONE;

	schedule (EV_LATER, WHEN_CHECKCHILD, EV_CHECKCHILD);
	return;
}

#endif
