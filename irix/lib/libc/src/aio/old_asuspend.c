/*************************************************************************
*                                                                        *
*               Copyright (C) 1992, Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.2 $ $Author: jeffreyh $"

#ifdef __STDC__
	#pragma weak old_aio_suspend = _old_aio_suspend
#endif
#define _POSIX_4SOURCE
#include "synonyms.h"
#include "old_aio.h"
#include <sys/time.h>
#include "old_local.h"

/*
 * Posix.4 draft 12 async I/O suspend
 * XXX multiple thread doing aio_suspend won't work.
 * switch to using normal semaphore(add itimer for aio_suspend)
 */
int
old_aio_suspend(const old_aiocb_t *list[], int cnt, timespec_t *ts)
{
	register int i, rv;
	register const old_aiocb_t *a;
	fd_set lfd;
	int ninprog;
	static int suspwait;	/* already have a wait pending?? */
	struct timeval tv;
	int timeout = 0;

	if (ts) {
		tv.tv_sec = ts->tv_sec;
		tv.tv_usec = ts->tv_nsec/1000;
	}

	for (;;) {
		/*
		 * Hold lock while scanning so that any completing
		 * I/O can decide whether to signal us or not
		 */
		ussetlock(_old_susplock);
		for (i = 0, ninprog = 0; i < cnt; i++) {
			if ((a = list[i]) == NULL)
				continue;
			if (a->aio_errno != EINPROGRESS) {
				usunsetlock(_old_susplock);
				return(0);
			}
			ninprog++;
		}
		if (!ninprog) {
			/* POSIX doesn't call this an error but
			 * why wait for nothing??
			 */
			usunsetlock(_old_susplock);
			setoserror(ESRCH);
			return(-1);
		}
		if (timeout) {
			usunsetlock(_old_susplock);
			setoserror(EAGAIN);
			return(-1);
		}
		_old_needwake++;
		usunsetlock(_old_susplock);

		/*
		 * nothing ready yet
		 * If already have a wait pending, just go back to select
		 */
		if (!suspwait) {
			/* no outstanding wait yet */
			if (uspsema(_old_suspsema) == 1) {
				/* fast service! */
				assert(_old_needwake == 0);
				continue;
			}
			suspwait++;
		}
		lfd = _old_suspset;
		rv = select(_old_suspfd+1, &lfd, NULL, NULL, (ts) ? &tv : NULL);
		/* 
		 * can be either signal or timeout expired or some completion 
		 */
		if (rv < 0) {
			if (oserror() != EINTR) {
				perror("aio:select error");
				exit(-1);
			}
			return(-1);
		} else if (rv != 1 && !ts) {
			fprintf(stderr, "select returned %d!\n", rv);
			exit(-1);
		}
		if (rv == 0 && ts) {
			/* case where timing out */
			ussetlock(_old_susplock);
			_old_needwake--;
                	usunsetlock(_old_susplock);
			timeout = 1;
		}
		suspwait = 0;
		assert(_old_needwake == 0);
		/* something now ready - look again */
	}
	/* NOTREACHED */
}
