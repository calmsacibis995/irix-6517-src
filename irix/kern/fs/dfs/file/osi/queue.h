/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: queue.h,v $
 * Revision 65.1  1997/10/20 19:17:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.9.1  1996/10/02  18:12:03  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:45:20  damon]
 *
 * Revision 1.1.4.1  1994/06/09  14:17:27  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:36  annie]
 * 
 * Revision 1.1.2.2  1993/01/21  14:53:32  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:57:23  cjd]
 * 
 * Revision 1.1  1992/01/19  02:42:46  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/* Copyright (C) 1989, 1990 Transarc Corporation - All rights reserved */

#ifndef	_QUEUEH_
#define	_QUEUEH_

/* 
 * "Simple Queues" implemented with both pointers and short offsets into 
 * a disk file. 
 */
struct squeue {
    struct squeue *next;
    struct squeue *prev;
};

/*
 * Operations on circular queues implemented with pointers.  Note: these queue objects 
 * are always located at the beginning of the structures they are linking.
 */
#define	QInit(q)    ((q)->prev = (q)->next = (q))
#define	QAdd(q,e)   ((e)->next=(q)->next,(e)->prev=(q),(q)->next->prev=(e),(q)->next=(e))
#define	QAddT(q,e)  ((e)->next=(q), (e)->prev=(q)->prev, (q)->prev->next=(e), (q)->prev=(e))
#define	QRemove(e)  ((e)->next->prev = (e)->prev, (e)->prev->next = (e)->next)
#define	QNext(e)    ((e)->next)
#define QPrev(e)    ((e)->prev)

#endif /* _QUEUEH_ */
