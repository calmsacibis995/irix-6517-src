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
#ident "$Revision: 1.25 $ $Author: jwag $"

#ifdef __STDC__
	#pragma weak taskcreate = _taskcreate
#endif

#include "synonyms.h"
#include "sys/types.h"
#include "sys/prctl.h"
#include "unistd.h"
#include "string.h"
#include "shlib.h"
#include "stdlib.h"
#include "ulocks.h"
#include "task.h"
#include "errno.h"
#include "stdio.h"
#include <malloc.h>
#include "mp_extern.h"

static void slave(void *);

/*
 * taskcreate - create a new task
 * Returns:
 *	>0 Success - task id of new task
 *	<0 Error - errno set (ENOMEM set if malloc fails)
 */

 /* ARGSUSED3 */
tid_t
taskcreate(
 char *name,			/* "name" of task */
 void (*entry)(void *),		/* entry point for task */
 void *arg,			/* arbitrary argument */
 int sched)			/* ?? */
{
	register tskblk_t *tp, *ttp;
	register tid_t tid;
	register int found;
	register pid_t pid;

	/* on first task call - initialize */
	if (_taskheader.t_list == NULL) {
		if (_taskinit(_MAXTASKS) < 0) {
			return(-1);
		}
	}

	if ((tp = (tskblk_t *) malloc(sizeof(tskblk_t))) == NULL) {
		setoserror(ENOMEM);
		return(-1);
	}
	if ((tp->t_name = malloc(strlen(name)+1)) == NULL) {
		free((void *)tp);
		setoserror(ENOMEM);
		return(-1);
	}
	strcpy(tp->t_name, name);
	tp->t_entry = entry;
	tp->t_arg = arg;

	/* lock tasklist before new task can run */
	ussetlock(_taskheader.t_listlock);

	/* find a tid */
	for (tid = 0; ; tid++) {
		found = 0;
		for (ttp = _taskheader.t_list; ttp; ttp = ttp->t_link) {
			if (ttp->t_tid == tid) {
				found++;
				break;
			}
		}
		if (!found)
			break;
	}
	tp->t_tid = tid;

	/* create new task */
	if ((pid = sproc(slave, PR_SALL, (void *)tp)) < 0) {
		free((void *)tp->t_name);
		free((void *)tp);
		usunsetlock(_taskheader.t_listlock);
		return(-1);
	}
	tp->t_pid = pid;

	/* link onto list */
	tp->t_link = (struct tskblk *)(_taskheader.t_list);
	_taskheader.t_list = tp;
	usunsetlock(_taskheader.t_listlock);
	return(tp->t_tid);
}

static void
slave(void *t)
{
	register tskblk_t *tp = t;

	/* set our task id */
	get_tid() = tp->t_tid;

	(*(tp->t_entry))(tp->t_arg);
	taskdestroy(tp->t_tid);
	abort();
}
