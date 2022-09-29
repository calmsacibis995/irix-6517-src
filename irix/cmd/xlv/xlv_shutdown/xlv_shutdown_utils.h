/**************************************************************************
 *                                                                        *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.2 $"


/*
 * A Generic Queue Object
 * 
 * Has only the basic capabilities ..
 * Not thread safe.
 */

typedef void (* xlv_queue_cb_t) (void *);

typedef struct qentry_s {
	void 		*value;
	struct qentry_s *next;
} xlv_queue_entry_t;

typedef struct {
	xlv_queue_entry_t 	*head;
	xlv_queue_entry_t 	*tail;
	u_int			nentries;
	xlv_queue_cb_t		delentry_cb;
} xlv_queue_t;



/*
 * The second argument to q_init() is a pointer to a func that
 * will free up an enqueued entry. This gets called
 * back if there are entries left at the time the
 * queue is destroyed.
 */
void  	q_init(xlv_queue_t *q, xlv_queue_cb_t delentry);
void	q_destroy(xlv_queue_t	*q);
void 	q_enqueue(xlv_queue_t *q, void *ent);
void 	q_dequeue(xlv_queue_t *q, void **entpp);

#define 	q_is_empty(q) 	((q)->nentries == 0)
