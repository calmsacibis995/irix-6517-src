/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Pseudo-Tty Emulation Module (PTEM)
 *
 * Added for SVR4 compatibility.  IRIX ptys understand and acknowledge
 * tty ioctls, so this is a "fake" module.  It does nothing except pass
 * messages through in either direction and waste time.  It's only here
 * so that SVR4 zealots can do an I_PUSH of "ptem" without having their
 * world view shattered.
 */

#ident "$Revision: 1.8 $"

#include <sys/cred.h>
#include <sys/strids.h>
#include <sys/strsubr.h>

static struct module_info stm_info = {
	STRID_PTEM,			/* module ID */
	"ptem",				/* module name */
	0,				/* minimum packet size */
	INFPSZ,				/* infinite maximum packet size	*/
	256,				/* hi-water mark */
	0,				/* lo-water mark */
};

static int ptem_rput(queue_t*, mblk_t*);
static int ptem_rsrv(queue_t*);
static int ptem_open(queue_t*, dev_t *, int, int, cred_t *);
static int ptem_close(queue_t*, int, cred_t *);

static struct qinit ptem_rinit = {
	ptem_rput, ptem_rsrv, ptem_open, ptem_close, NULL, &stm_info, NULL
};

static int ptem_wput(queue_t *, mblk_t *);
static int ptem_wsrv(queue_t *);

static struct qinit ptem_winit = {
	ptem_wput, ptem_wsrv, NULL, NULL, NULL, &stm_info, NULL
};

struct streamtab pteminfo = {&ptem_rinit, &ptem_winit, NULL, NULL};
int ptemdevflag = 0;

/* ARGSUSED */
static int
ptem_open(queue_t *rq, dev_t *devp, int flag, int sflag, cred_t *crp)
{
	rq->q_ptr = (void *)1L;
	WR(rq)->q_ptr = (void *)1L;
	return 0;
}

/* close the stream
 *	This is called when the stream is being dismantled or when this
 *	module is being popped.
 */
/* ARGSUSED */
static int
ptem_close(queue_t *rq, int flag, cred_t *crp)
{
	return 0;
}

/* accept a new output message
 */
static int
ptem_wput(register queue_t *wq, register mblk_t *bp)
{
	putnext(wq, bp);
	return(0);
}


/* work on the output queue
 */
static int
ptem_wsrv(register queue_t *wq)		/* our write queue */
{
	register mblk_t *wmp;

	while(wmp = getq(wq))		/* just send everything */
		putnext(wq, wmp);
	return(0);
}


/* accept a new input message
 */
static int
ptem_rput(queue_t *rq, mblk_t *bp)
{
	putnext(rq, bp);
	return(0);
}


/* work on the input queue
 */
static int
ptem_rsrv(queue_t *rq)			/* our read queue */
{
	register mblk_t *rmp;

	while(rmp = getq(rq))		/* just send everything */
		putnext(rq, rmp);
	return(0);
}
