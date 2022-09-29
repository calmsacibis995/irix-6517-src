/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/q.h>

struct q_element_s *
popq(struct q_element_s *q)
{
	struct q_element_s *qe;

	if (q_empty(q))
		qe = 0;
	else {
		qe = q->qe_forw;
		q->qe_forw = qe->qe_forw;
		qe->qe_forw->qe_back = q;
		qe->qe_forw = qe->qe_back = qe;
	}
	return qe;
}

void
pushq(struct q_element_s *q, struct q_element_s *qe)
{
	qe->qe_forw = q->qe_forw;
	q->qe_forw = qe;
	qe->qe_forw->qe_back = qe;
	qe->qe_back = q;
}

void
rmq(struct q_element_s *qe)
{
	qe->qe_back->qe_forw = qe->qe_forw;
	qe->qe_forw->qe_back = qe->qe_back;
	qe->qe_forw = qe->qe_back = qe;
}

void
q_insert_before(struct q_element_s *qet, struct q_element_s *qem)
{
	/* insert qem before qet */
	qem->qe_forw = qet;
	qem->qe_back = qet->qe_back;
	qet->qe_back->qe_forw = qem;
	qet->qe_back = qem;
}

void
q_move_after(struct q_element_s *qet, struct q_element_s *qem)
{
	/* remove qem */
	qem->qe_back->qe_forw = qem->qe_forw;
	qem->qe_forw->qe_back = qem->qe_back;
	/* insert qem behind qet */
	qem->qe_forw = qet->qe_forw;
	qem->qe_back = qet;
	qet->qe_forw = qem;
	qem->qe_forw->qe_back = qem;
}

void
init_q_element(struct q_element_s *qe, void *parent)
{
	qe->qe_forw = qe;
	qe->qe_back = qe;
	qe->qe_parent = parent;
}
