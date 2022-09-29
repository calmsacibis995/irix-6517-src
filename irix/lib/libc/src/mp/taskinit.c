/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, 1989, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.25 $ $Author: doucette $"

#include "synonyms.h"
#include "sys/types.h"
#include "string.h"
#include "shlib.h"
#include "ulocks.h"
#include "errno.h"
#include "stdio.h"
#include <malloc.h>
#include "us.h"

/*
 * taskinit - initialize task things:
 *	alloc a lock for create/end
 *	set up thread 1 (every process has 1 task)
 * Returns:
 *	0 Success
 *	<0 Error - errno set (ENOMEM set if malloc fails)
 */

/* task header block */
tskhdr_t _taskheader;

int
_taskinit(int maxtasks)
{
	char tmpname[24];
	register tskblk_t *tp;
	ptrdiff_t osize, oatype;
	int ousers;

	get_tid() = 0;	/* KLUDGE */
	if (_taskheader.t_list)
		return(0);
	
	/* config locks */

	/* initialize user sync header */
	strcpy(tmpname, "/usr/tmp/taskXXXXXX");
	mktemp(tmpname);
	oatype = usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	ousers = (int)usconfig(CONF_INITUSERS, maxtasks);
	osize = usconfig(CONF_INITSIZE, maxtasks * 1024);
	_taskheader.t_usptr = usinit(tmpname);

	usconfig(CONF_ARENATYPE, oatype);
	usconfig(CONF_INITUSERS, ousers);
	usconfig(CONF_INITSIZE, osize);
	if (_taskheader.t_usptr == NULL)
		return(-1);

	/* alloc lock */
	if ((_taskheader.t_listlock = usnewlock(_taskheader.t_usptr)) == NULL)
		return(-1);
	
	ussetlock(_taskheader.t_listlock);

	if (_taskheader.t_list) {
		/* someone beat me */
		usunsetlock(_taskheader.t_listlock);
		return(0);
	}

	/* alloc a tskblk for task 1 */
	if ((tp = (tskblk_t *) malloc(sizeof(tskblk_t))) == NULL) {
		usunsetlock(_taskheader.t_listlock);
		setoserror(ENOMEM);
		return(-1);
	}
	if ((tp->t_name = (char *) malloc(strlen("MASTER")+1)) == NULL) {
		usunsetlock(_taskheader.t_listlock);
		free((void *)tp);
		setoserror(ENOMEM);
		return(-1);
	}
	strcpy(tp->t_name, "MASTER");
	tp->t_link = NULL;
	tp->t_tid = 0;
	tp->t_pid = (pid_t)(PRDA->t_sys.t_pid);
	_taskheader.t_list = tp;
	
	usunsetlock(_taskheader.t_listlock);
	return(0);
}

/*
 * findtask - search for a task on the tasklist
 * the _tasklistlock MUST be locked
 *	pptp - filled in with previous pointer if none null
 */
tskblk_t *
_findtask(tid_t tid, tskblk_t **pptp)
{
	register tskblk_t *tp, *ptp;

	ptp = NULL;
	/* find task to destroy */
	for (tp = _taskheader.t_list; tp; tp = tp->t_link) {
		if (tp->t_tid == tid)
			break;
		ptp = tp;
	}
	if (pptp)
		*pptp = ptp;
	return(tp);
}
