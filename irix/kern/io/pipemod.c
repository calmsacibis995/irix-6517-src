/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

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

#ident	"$Revision: 1.2 $"

/*
 * This module switches the read and write flush bits for each
 * M_FLUSH control message it receives. It's intended usage is to
 * properly flush a STREAMS-based pipe.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/stropts.h>

static int pipeopen(queue_t *rqp, dev_t *devp, int flag, int sflag, struct cred *crp);
static int pipeclose(queue_t *q, int flag, struct cred *crp);
static int pipeput(queue_t *qp, mblk_t *mp);

int pipedevflag = 0;

static struct module_info pipe_info = {
	1003,
	"pipe",
	0,
	INFPSZ,
	STRHIGH,
	STRLOW };
static struct qinit piperinit = { 
	pipeput,
	NULL,
	pipeopen,
	pipeclose,
	NULL,
	&pipe_info,
	NULL };
static struct qinit pipewinit = { 
	pipeput,
	NULL,
	NULL,
	NULL,
	NULL,
	&pipe_info,
	NULL};
struct streamtab pipeinfo = { &piperinit, &pipewinit };

/*ARGSUSED*/
static int
pipeopen(queue_t *rqp, dev_t *devp, int flag, int sflag, struct cred *crp)
{
	return (0);
}

/*ARGSUSED*/
static int
pipeclose(queue_t *q, int flag, struct cred *crp)
{
	return (0);
}

/*
 * Use same put procedure for write and read queues.
 * If mp is an M_FLUSH message, switch the FLUSHW to FLUSHR and
 * the FLUSHR to FLUSHW and send the message on.  If mp is not an
 * M_FLUSH message, send it on with out processing.
 */
static int
pipeput(queue_t *qp, mblk_t *mp)
{
	switch(mp->b_datap->db_type) {
		case M_FLUSH:
			if (!(*mp->b_rptr & FLUSHR && *mp->b_rptr & FLUSHW)) {
				if (*mp->b_rptr & FLUSHW) {
					*mp->b_rptr |= FLUSHR;
					*mp->b_rptr &= ~FLUSHW;
				} else {
					*mp->b_rptr |= FLUSHW;
					*mp->b_rptr &= ~FLUSHR;
				}
			}
			break;

		default:
			break;
	}
	putnext(qp,mp);
	return (0);
}
