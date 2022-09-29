/*                                                                        *
 *               Copyright (C) 1989, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.6 $"
#ifndef _BPQUEUE_H_
#define _BPQUEUE_H_

typedef struct bp_queue {
	buf_t	*queuep;
	uint_t	queuebits;
	sv_t	queuewakeup;
} bp_queue_t;

#define BPQ_LOCKBIT	0x1000000
#define BPQ_WAITBITS	0x0ffffff

#define BPQ_QUEUE( bdqp )	(&bdqp->queuep)
#define BPQ_QUEUELOCK( bdqp )	(&bdqp->queuebits)
#define BPQ_QUEUEWAITCNT( bdqp )	(bdqp->queuebits)
#define BPQ_QUEUEWAKEUP( bdqp )	(&bdqp->queuewakeup)

extern void bp_enqueue(buf_t *, bp_queue_t *);
extern void bpqueue_init( bp_queue_t *);
extern void bpqueuedaemon( bp_queue_t *, void(*)(buf_t *));
extern buf_t *bp_dequeue_wait( bp_queue_t *);

#endif /* _GRIO_H_ */
