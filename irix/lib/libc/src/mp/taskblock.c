/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.14 $ $Author: jwag $"

#ifdef __STDC__
	#pragma weak taskblock = _taskblock
	#pragma weak taskunblock = _taskunblock
	#pragma weak tasksetblockcnt = _tasksetblockcnt
#endif

#include "synonyms.h"
#include "sys/types.h"
#include "shlib.h"
#include "ulocks.h"
#include "task.h"
#include "errno.h"
#include "stdio.h"
#include "signal.h"

/*
 * taskblock - blocking primitives for tasks
 * Returns:
 *	0 Success
 *	<0 Error - errno set
 */

int
taskblock(tid_t tid)
{
	register tskblk_t *tp;
	register pid_t pid;

	ussetlock(_taskheader.t_listlock);

	/* find task to block */
	if ((tp = _findtask(tid, NULL)) == NULL) {
		usunsetlock(_taskheader.t_listlock);
		if (tid == get_tid())
			if (_utrace)
				fprintf(stderr, 
				"taskblock = self not on list!\n");
		setoserror( ESRCH );
		return(-1);
	}
	pid = tp->t_pid;
	usunsetlock(_taskheader.t_listlock);

	return(blockproc(pid));
}

int
taskunblock(tid_t tid)
{
	register tskblk_t *tp;
	register pid_t pid;

	ussetlock(_taskheader.t_listlock);

	/* find task to block */
	if ((tp = _findtask(tid, NULL)) == NULL) {
		usunsetlock(_taskheader.t_listlock);
		setoserror( ESRCH );
		return(-1);
	}
	pid = tp->t_pid;
	usunsetlock(_taskheader.t_listlock);

	return(unblockproc(pid));
}

int
tasksetblockcnt(tid_t tid, int cnt)
{
	register tskblk_t *tp;
	register pid_t pid;

	ussetlock(_taskheader.t_listlock);

	/* find task to block */
	if ((tp = _findtask(tid, NULL)) == NULL) {
		usunsetlock(_taskheader.t_listlock);
		setoserror( ESRCH );
		return(-1);
	}
	pid = tp->t_pid;
	usunsetlock(_taskheader.t_listlock);

	return(setblockproccnt(pid, cnt));
}
