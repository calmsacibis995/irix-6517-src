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
#ident "$Revision: 1.5 $"

#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/debug.h>
#include <sys/syssgi.h>
#include <sys/sysmacros.h>
#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dvh.h>
#include <sys/xlv_vh.h>
#include <xlv_oref.h>
#include <xlv_lab.h>
#include <sys/xlv_attr.h>
#include <xlv_utils.h>
#include "xlv_shutdown_utils.h"

/*
 * A Generic Queue Object
 * 
 * Has only the basic capabilities ..
 * Not thread safe.
 */


void
q_enqueue( 
	xlv_queue_t 	*q, 
	void 		*ent )
{
	xlv_queue_entry_t *qe;
	ASSERT(q);

	qe = malloc (sizeof (xlv_queue_entry_t));
	
	qe->value = ent;
	qe->next = NULL;

	if (q->head != NULL) {
		q->tail->next = qe;
		q->tail = qe;
	}		
	else
		q->tail = q->head = qe;
	q->nentries++;

}




void
q_dequeue( 
	xlv_queue_t 	*q, 
	void 		**entpp )
{	
	*entpp = NULL;
	ASSERT(q);

	if (! q->nentries) {
		return;
	}

	ASSERT (q->head);
	{
		xlv_queue_entry_t *qe;
		qe = q->head;
		q->head = qe->next;
		if (q->tail == qe)
			q->tail = NULL;

		*entpp = qe->value;
		free (qe);
	}

	--(q->nentries);
}


/*
 * The second argument is a pointer to a func that
 * will free up an enqueued entry. This gets called
 * back if there are entries left at the time the
 * queue is destroyed.
 */
void
q_init(
	xlv_queue_t 	*q, 
	xlv_queue_cb_t 	delentry)
{
	ASSERT(q);
	q->head = q->tail = NULL;
	q->nentries = 0;
	q->delentry_cb = delentry;
}

void
q_destroy(
	xlv_queue_t	*q)
{
	void *ent;
	ASSERT(q);
	while(! q_is_empty(q)) {
		ent = NULL;
		q_dequeue(q, &ent);
		ASSERT(ent);

		/* 
		 * free the enqueued entry 
		 */
		(*(q->delentry_cb))(ent);
	}
}



