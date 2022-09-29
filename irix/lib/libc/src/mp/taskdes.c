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
#ident "$Revision: 1.17 $ $Author: jwag $"

#ifdef __STDC__
	#pragma weak taskdestroy = _taskdestroy
#endif

#include "synonyms.h"
#include "sys/types.h"
#include "shlib.h"
#include "ulocks.h"
#include "task.h"
#include "errno.h"
#include "stdio.h"
#include "signal.h"
#include <malloc.h>

/*
 * taskdestroy - destroy a task
 * Returns:
 *	0 Success
 *	<0 Error - errno set
 *	Unless destroying oneself - always successful!
 */

int
taskdestroy(tid_t tid)
{
	register tskblk_t *tp;
	auto tskblk_t *ptp = NULL;
	register pid_t pid;
	int rv;

	ussetlock(_taskheader.t_listlock);

	if (_taskheader.t_list == NULL) {
		usunsetlock(_taskheader.t_listlock);
		setoserror( ESRCH );
		return(-1);
	}

	/* find task to destroy */
	if ((tp = _findtask(tid, &ptp)) == NULL) {
		usunsetlock(_taskheader.t_listlock);
		if (tid == get_tid())
			if (_utrace)
				fprintf(stderr,
				"taskdestroy = self not on list!\n");
		setoserror( ESRCH );
		return(-1);
	}
	/* remove from task list */
	if (ptp == NULL)
		_taskheader.t_list = tp->t_link;
	else
		ptp->t_link = tp->t_link;
	pid = tp->t_pid;
	free((void *)tp->t_name);
	free((void *)tp);

	/* 
	 * If we're killing someone else - hold lock around kill - that way
	 * they can't get the lock and then die - leaving the lock locked.
	 * If we're killing ourselves this doesn't matter.
	 */
	if (pid == get_pid()) {
		usunsetlock(_taskheader.t_listlock);
		rv = kill(pid, SIGKILL);
	} else {
		rv = kill(pid, SIGKILL);
		usunsetlock(_taskheader.t_listlock);
	}

	/* should re-link up on error?? */
	return(rv);
}
