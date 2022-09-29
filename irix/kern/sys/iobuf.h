/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/iobuf.h	10.2"*/
#ident	"$Revision: 3.7 $"
/*
 * The iobuf if used by drivers to keep track of certain statistics
 * and to maintain a list of requests in a queue.
 */
typedef struct iobuf
{
	struct buf	*prio_head;	/* head of Priority I/O queue */
	struct buf	*prio_tail;	/* tail of Priority I/O queue */
	struct buf	*io_head;	/* head of I/O queue */
	struct buf	*io_tail;	/* tail of I/O queue */
	union
	{
		char	active;		/* busy flag */
		long	prevblk;	/* block # of previous command */
	} io_state;
	int		seq_count;	/* count of sequential requests */
	daddr_t		seq_next;	/* next sequential request */
} iobuf_t;


extern void		macsisort(struct iobuf *, struct buf *);
extern void		nosort(struct iobuf *, struct buf *);
extern struct buf *	fair_disk_dequeue(struct iobuf *);

/*
 * Default limit for number of sequential requests allowed
 * by fairdequeue.
 */
#define	FAIR_DEQUEUE_LIMIT		4
