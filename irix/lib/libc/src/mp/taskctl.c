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
#ident "$Revision: 1.16 $ $Author: jwag $"

#ifdef __STDC__
	#pragma weak taskctl = _taskctl
#endif

#include "synonyms.h"
#include "sys/types.h"
#include "shlib.h"
#include "ulocks.h"
#include "task.h"
#include "errno.h"
#include "stdio.h"
#include "stdarg.h"

/*
 * taskctl - ctl function for tasks
 * Returns:
 *	0 Success
 *	<0 Error - errno set
 */

/* VARARGS2 */
int
taskctl(tid_t tid, unsigned option, ...)
{
#ifdef LATER
	va_list ap;
#endif
	int rval = 0;
	register tskblk_t *tp;

#ifdef LATER
	va_start(ap, option);
#endif
	ussetlock(_taskheader.t_listlock);
	/* find task with tid */
	if ((tp = _findtask(tid, NULL)) == NULL) {
		usunsetlock(_taskheader.t_listlock);
		setoserror(ESRCH);
		return(-1);
	}

	switch(option) {
	case TSK_ISBLOCKED:
		rval = (int)prctl(PR_ISBLOCKED, tp->t_pid);
		break;
	default:
		setoserror(EINVAL);
		rval = -1;
		break;
	}
	usunsetlock(_taskheader.t_listlock);
	return(rval);
}
