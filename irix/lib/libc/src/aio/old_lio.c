/*************************************************************************
*                                                                        *
*               Copyright (C) 1992-1994, Silicon Graphics, Inc.       	 *
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
        #pragma weak old_lio_listio = _old_lio_listio
#endif
#define _POSIX_4SOURCE
#include "synonyms.h"
#include "old_aio.h"
#include "old_local.h"
#include <limits.h>

static void lio_slave(void *);
static int lio_wait(struct old_aiocb **, int );
static int lio_nowait(struct old_aiocb **, int, old_sigevent_t *);

/*
 * Posix.4 draft 12 async I/O lio_listio
 * XXX multiple thread doing lio_listio won't work.
 * switch to using normal semaphore(add itimer for aio_suspend)
 */
int
old_lio_listio(int mode, old_aiocb_t *list[], int cnt, old_sigevent_t *sig)
{
	register int i;
	register old_aiocb_t *a;

	if (cnt > _OLD_AIO_LISTIO_MAX || 
			(mode != OLD_LIO_WAIT && mode != OLD_LIO_NOWAIT)) {
		setoserror(EINVAL);
		return(-1);
	}
	/* queue requests */
	for (i = 0; i < cnt; i++) {
		if (!(a = list[i]))
			continue;
 		if (a->aio_lio_opcode == OLD_LIO_NOP)
			continue;
		if (a->aio_lio_opcode != OLD_LIO_READ && 
			a->aio_lio_opcode != OLD_LIO_WRITE) {  
			setoserror(EINVAL);
			return(-1);
		}
		/* ignore any signal request */
		a->aio_sigevent.sigev_signo = 0;
		if (_old_aqueue(a, a->aio_lio_opcode))  
			return(-1);
	}
	if (mode == OLD_LIO_WAIT) 
		/* wait for all to be done, return on 1st failure  */
		return(lio_wait(list, cnt));
	else
		return(lio_nowait(list, cnt, sig));
}

/*
 * copy all the parameter from the stack
 * spawn an sproc() to do lio_wait() 
 * then send signal to parent
 */
static int
lio_nowait(struct old_aiocb *list[], int cnt, old_sigevent_t *sig)
{
	struct old_liocb *lio;

	lio = (struct old_liocb *)malloc(sizeof(*lio));
	lio->cnt = cnt;
	lio->sig = (old_sigevent_t *)malloc(sizeof(*sig));
	if (sig) 
		*(lio->sig) = *sig;
	else
		lio->sig = (old_sigevent_t *)NULL;
	lio->list = (struct old_aiocb **)malloc((int)(sizeof(struct old_aiocb))*cnt);
	bcopy((void *)list, (void *)lio->list, (int)(sizeof(struct old_aiocb))*cnt);
	lio->ppid = get_pid();		/* use cached pid, for speed */
	if (sproc(lio_slave, OLD_AIO_SPROC, (void *)lio) < 0) {
		perror("aio:sproc");
		exit(-1);
	}
	return(0);
}

static void
lio_slave(void *liop)
{
	old_sigevent_t *sig;
	register struct old_liocb *lio = (struct old_liocb *)liop;

	lio_wait(lio->list, lio->cnt);
	/* XXX use sigqueue */
	if ((sig = lio->sig) && sig->sigev_signo) {
		if (kill(lio->ppid, sig->sigev_signo) < 0) {
			perror("aio: kill");
			exit(-1);
		}
	}
	free((void *)lio->sig);
	free((void *)lio->list);
	free((void *)lio);
}
		
static int
lio_wait(struct old_aiocb *list[], int cnt)
{
	register int i, rv;
	register const old_aiocb_t *a;
	int ninprog, error;
	fd_set lfd;
	static int liowait;	/* already have a wait pending?? */

	for ( ;; ) {
		/*
                 * Hold lock while scanning so that any completing
                 * I/O can decide whether to wake us up or not
                 */
                ussetlock(_old_susplock);
		for (i = 0, error = 0, ninprog = 0; i < cnt; i++) {
			if ((a = list[i]) == NULL || 
				a->aio_lio_opcode == OLD_LIO_NOP)
				continue;
			if (a->aio_errno != EINPROGRESS) {
				if (a->aio_errno) {
					error++;
					break;
				}
				else
					continue;
			}
			else {
				/* at least one is still in progress */
				ninprog++;
				break;
			}
		}
		if (error) {
			/* 
			* application checks error status of each aiocb
			* to determine the failed request.
			*/
			usunsetlock(_old_susplock);
			setoserror(EIO);
			return(-1);
		}
		if (!ninprog) {
			/* 
			* no error, none in progress, must be done
			*/
			usunsetlock(_old_susplock);
			return(0);
		}
		else {
			/*
		 	* not all done yet.
		 	*/
			_old_needwake++;
			usunsetlock(_old_susplock);
			if (!liowait) {
				/* no outstanding wait yet */
				if (uspsema(_old_suspsema) == 1) {
					/* fast service! */
					assert(_old_needwake == 0);
					continue;
				}
				liowait++;
			}
		}
		lfd = _old_suspset;
		rv = select(_old_suspfd+1, &lfd, NULL, NULL, NULL);
		if (rv < 0) {
			if (oserror() != EINTR) {
				perror("aio:select error");
				exit(-1);
			}
			return(-1);
		} else if (rv != 1) {
			fprintf(stderr, "aio: select returned %d!\n", rv);
			exit(-1);
		}
		liowait = 0;
		assert(_old_needwake == 0);
		/* something now ready - look again */
	}
	/* NOT REACHED */
}
