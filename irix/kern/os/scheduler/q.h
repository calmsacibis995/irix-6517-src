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
#ifndef _Q_H
#define _Q_H

struct q_element_s {
	struct q_element_s *qe_forw;
	struct q_element_s *qe_back;
	void *qe_parent;
};

#define	q_empty(q)	((q)->qe_forw == (q))
#define	q_onq(q)	((q)->qe_forw != (q))

struct q_element_s *popq(struct q_element_s *);
void pushq(struct q_element_s *, struct q_element_s *);
void rmq(struct q_element_s *);
void q_insert_before(struct q_element_s *, struct q_element_s *);
void q_move_after(struct q_element_s *, struct q_element_s *);
void init_q_element(struct q_element_s *, void *);

#endif /* _Q_H */
