/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.31 $"

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)subr_log.c	7.1 (Berkeley) 6/5/86
 */

/*
** klog.c
**   This module is the driver for kernel error logging. All kernel output
**   is also placed in the error logging area, which user processes
**   may access using open, close, read, ioctl and poll (select).
**   NOTES:
**	0. Defining KLOG_DEBUG will cause debugging info to be printed
**	1. This is a mutual exclusive device.
*/

#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/ddi.h"
#include "sys/errno.h"
#include "ksys/vfile.h"
#include "sys/pda.h"
#include "sys/poll.h"
#include "sys/signal.h"
#include "sys/termio.h"
#include "sys/uio.h"
#include "sys/klog.h"
#include <sys/conf.h>
#ifdef	CELL_IRIX
# include "ksys/sthread.h"
#endif	/* CELL_IRIX */

int klogdevflag = D_MP;

#ifdef KLOG_DEBUG
#define	cmn_dbg_err(a)	cmn_err a
#else
#define	cmn_dbg_err(a)
#endif


/*
 * klog_need_action is set if klogwakeup needs to be called.
 */
int	klog_need_action;
typedef struct {
	int		kli_state;	/* holds software state of the driver*/
	struct pollhead kli_inpq;	/* input queue for polling processes */
	pid_t		kli_pgid;	/* process group for ASYNC I/O	     */
	sema_t		kli_iodone;	/* semaphore for blocking I/O	     */
} kloginfo_t;

/*
** Various values kli_state field can contain
*/
#define	STATE_NULL	0	/* Initial value and following a close	*/
#define	STATE_OPEN	0x01	/* device is open			*/
#define	STATE_NDELAY	0x02	/* Non-blocking I/O should occur	*/
#define	STATE_ASYNC	0x04	/* async I/O should occur		*/
#define	STATE_POLL	0x08	/* polling is occurring			*/
#define	STATE_FIRSTIME	0x10	/* first time flag			*/
#define	STATE_IOWAIT	0x20	/* waiting for buffers to drain		*/

#define	PRI_READ	(PZERO+1)	/* priority for blocking reads	*/


/* Holds the device info */
static kloginfo_t	kloginfo =
{
	STATE_FIRSTIME
};

extern klogmsgs_t	klogmsgs;	/* holds associated logging buffer info
					*/

#ifdef	CELL_IRIX
sema_t		klogwake_sema;		/* v'd in klog_unlocked_wakeup	*/
#endif	/* CELL_IRIX */

/*
** klogopen
**   Perform open processing for the device. Note device cannot be
**   opened for writing or appending.
*/
/* ARGSUSED */
int
klogopen(dev_t *devp, int flags, int otyp, struct cred *crp)
{
	int	s;
	unsigned long lp;

	/* single thread requests */
	PUTBUF_LOCK(s);

	/* If first time open, we must init the iodone semaphore.	*/
	if (kloginfo.kli_state & STATE_FIRSTIME) {
		initnsema(&kloginfo.kli_iodone, 0, "kliod");
		initpollhead(&kloginfo.kli_inpq);
		kloginfo.kli_state = STATE_NULL;

#ifdef	CELL_IRIX
	{
		void klog_thread(void *arg);
		extern int klogthrd_pri;

		initnsema(&klogwake_sema, 1, "klogwake_sema");

		sthread_create("klogthrd", NULL, 4096, 0, klogthrd_pri, KT_PS, 
						 klog_thread, 0, 0, 0, 0);
	}
#endif	/* CELL_IRIX */
	}

	/*
	** If device is already open, sorry, this is a single use device
	*/
	if (kloginfo.kli_state & STATE_OPEN) {
		PUTBUF_UNLOCK(s);
		return EBUSY;
	}

	/*
	** device is READ ONLY, cannot be opened for writing or appending
	*/
	if (flags & (FWRITE | FAPPEND)) {
		PUTBUF_UNLOCK(s);
		return EACCES;
	}

	/*
	** device is now "opened", initialize the various fields in the
	** info structure:
	**	Mark as open, No process waiting on select, and save
	**	the current process group (used for ASYNC I/O)
	*/
	kloginfo.kli_state |= STATE_OPEN;

	drv_getparm(PPGRP, &lp);
	kloginfo.kli_pgid = (pid_t)lp;

	/*
	** Based on open flags, we adjust for Non-blocking I/O,
	** This is a set operation on an open.
	*/
	if (flags & (FNONBLK|FNDELAY)) {
		kloginfo.kli_state |= STATE_NDELAY;
	}

	PUTBUF_UNLOCK(s);
	return 0;
}

/*
** klogclose
**   Perform close processing for the device
*/
/* ARGSUSED */
int
klogclose(dev_t dev, int flags, int otyp, struct cred *crp)
{
	int	s;

	/* single thread requests */
	PUTBUF_LOCK(s);

	/* Reset the state field */
	kloginfo.kli_state = STATE_NULL;

	PUTBUF_UNLOCK(s);
	return 0;
}

/*
** klogread
**   Read the number of characters specified by the user
*/
/* ARGSUSED */
int
klogread(dev_t dev, uio_t *uiop, cred_t *crp)
{
	register int	readloc;
	register int	writeloc;
	register int	amt;
	int		s;
	int		error;

	/* single thread requests */
	PUTBUF_LOCK(s);

	while ((klogmsgs.klm_readloc % KLM_BUFSZ) ==
		(klogmsgs.klm_writeloc % KLM_BUFSZ))
	{
		if (kloginfo.kli_state & STATE_NDELAY)
		{
			PUTBUF_UNLOCK(s);
			return EWOULDBLOCK;
		}
		kloginfo.kli_state |= STATE_IOWAIT;
		PUTBUF_UNLOCK(s);
		if (psema(&kloginfo.kli_iodone, PRI_READ) == -1)
			return EINTR;
		PUTBUF_LOCK(s);
	}
	PUTBUF_UNLOCK(s);

	/*
	** While the user wants info, we pass it to 'em.  Care must
	** be taken due the nature of a ring buffer, the readloc could
	** be after the writeloc
	*/
	writeloc = klogmsgs.klm_writeloc % KLM_BUFSZ;

	while (uiop->uio_resid) {
		readloc = klogmsgs.klm_readloc % KLM_BUFSZ;
		amt = writeloc - readloc;

		if (amt < 0)
			amt = KLM_BUFSZ - readloc;

		amt = MIN(amt, uiop->uio_resid);

		/* if the minimum amt left to read is 0, no more left! */
		if (amt == 0)
			break;

		error = uiomove(&klogmsgs.klm_buffer[readloc], amt,
				UIO_READ, uiop);
		if (error)
			return error;

		klogmsgs.klm_readloc += amt;
	}
	return 0;
}

/*
** klogpoll
**   Adjust 'eventptr' for INPUT, and queue up process if no input at
**   this time.
*/
/* ARGSUSED */
int
klogpoll(
	dev_t dev,
	register short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	int	s;

	/*
	** if polling on input, and none available:
	**	update event info, and queue up the process.
	*/
	if (events & (POLLIN | POLLRDNORM)) {
		/* single thread requests */
		PUTBUF_LOCK(s);

		if ((klogmsgs.klm_writeloc % KLM_BUFSZ) ==
		     (klogmsgs.klm_readloc % KLM_BUFSZ)) {
			events &= ~(POLLIN | POLLRDNORM);
			kloginfo.kli_state |= STATE_POLL;
			if (!anyyet) {
				*phpp = &kloginfo.kli_inpq;
				/* snapshot generation while we hold lock */
				*genp = POLLGEN(&kloginfo.kli_inpq);
			}
		}

		PUTBUF_UNLOCK(s);
	}

	*reventsp = events;
	return 0;
}

#ifdef	CELL_IRIX
/*
 * In Cellular Irix, the klogwakeup() routine could call pollwakeup()
 * which may need to obtain a behavior read-lock on a vproc.
 * We can't do this out of the clock()/tick_actions() code, there's
 * no thread that could sleep on a behavior lock, in this instance,
 * we've created a "helper" thread.
 */
void
klog_unlocked_wakeup(void)
{
	vsema(&klogwake_sema);		/* Kick our "helper"	*/
}


void
klog_unlocked_real_wakeup(void)
{
	int s;
	
	PUTBUF_LOCK(s);
	klogwakeup();
	PUTBUF_UNLOCK(s);
}


/* ARGSUSED */
void
klog_thread(void *arg)
{
	for (;;) {
		psema(&klogwake_sema, PZERO);

		klog_unlocked_real_wakeup();
	}
}

#else	/* ! CELL_IRIX */

void
klog_unlocked_wakeup(void)
{
	int s;
	
	if (PUTBUF_TRYLOCK(s)) {
		klogwakeup();
		PUTBUF_UNLOCK(s);
	} else {
		klog_need_action++;
	}
}
#endif	/* ! CELL_IRIX */

/*
** klogwakeup
**   Issue appropriate signals and/or wakeup processes waiting on I/O.
**   NOTE: This routine should be called with `putbuflck' locked.
*/
void
klogwakeup(void)
{

	if (!(kloginfo.kli_state & STATE_OPEN))
		return;

	/*
	** if a process is waiting on polling, wake it up and reset
	**    waiting process.
	** if ASYNC I/O is occurring, signal the process group that I/O
	**    has occurred.
	** if we had blocked waiting for input, wake the process up.
	*/
	if (kloginfo.kli_state & STATE_POLL) {
		pollwakeup(&kloginfo.kli_inpq, POLLIN|POLLRDNORM);
		kloginfo.kli_state &= ~STATE_POLL;
	}

	if (kloginfo.kli_state & STATE_ASYNC)
		signal(kloginfo.kli_pgid, SIGIO); 

	/*
	 * This is a single-use driver.  A single vsema is sufficient.
	 */
	if (kloginfo.kli_state & STATE_IOWAIT) {
		kloginfo.kli_state &= ~STATE_IOWAIT;
		vsema(&kloginfo.kli_iodone);
	}
}

/*
** klogioctl
**   Perform ioctl's which the device supports.  Rather than holding the
**   spin lock for the entire function, we give it back up as soon as
**   possible.
*/

/* ARGSUSED */
int
klogioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *crp, int *rvalp)
{
	register int	readloc;
	register int	writeloc;
	int		pgrp;
	int		amt;
	int		s;

	/* make sure that only one processor has use */
	PUTBUF_LOCK(s);

	switch (cmd) {
	   case FIONREAD:
		/* Return number of bytes available to read */

		readloc = klogmsgs.klm_readloc % KLM_BUFSZ;
		writeloc = klogmsgs.klm_writeloc % KLM_BUFSZ;

		PUTBUF_UNLOCK(s);

		amt = writeloc - readloc;
		if (amt < 0)
			amt += KLM_BUFSZ;
		
		if (copyout((caddr_t)&amt, (caddr_t)arg, sizeof (amt)))
			return EFAULT;
		break;

	   case FIONBIO:
		/* Set/Clear non-blocking I/O (flip/flop) */
		if (arg)
			kloginfo.kli_state |= STATE_NDELAY;
		else
			kloginfo.kli_state &= ~STATE_NDELAY;
			
		PUTBUF_UNLOCK(s);
		break;

	   case FIOASYNC:
		/* Set/Clear async I/O (flip/flop) */
		if (arg)
			kloginfo.kli_state |= STATE_ASYNC;
		else
			kloginfo.kli_state &= ~STATE_ASYNC;

		PUTBUF_UNLOCK(s);
		break;

	   case TIOCSPGRP:
		/*
		** set process group for async I/O. Note that NO checking is
		** done for a valid process group!
		*/
		kloginfo.kli_pgid = arg;
		PUTBUF_UNLOCK(s);
		break;

	   case TIOCGPGRP:
		/* get process group for async I/O */
		pgrp = kloginfo.kli_pgid;

		PUTBUF_UNLOCK(s);

		if (copyout((caddr_t)&pgrp, (caddr_t)arg, sizeof (pgrp)))
			return EFAULT;
		break;

	   default:
		PUTBUF_UNLOCK(s);
		return EINVAL;
	}

	return 0;
}
