#ifndef _Q_H_
#define _Q_H_

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include "common.h"

#define Q_DECLARE(que)		q_t que = { &que, &que }
#define Q_INIT(que)		((que)->next = (que)->prev = que)

#define Q_HEAD(que, typ)	Q_NEXT(que, typ)
#define Q_NEXT(que, typ)	((typ)(((q_t *)(que))->next))
#define Q_TAIL(que)		((que)->prev)
#define Q_END(que, elt)		((que) == (q_t *)(elt))

#define Q_EMPTY(que)		((que)->next == (que))

#define Q_ADD_FIRST(que, elt)						\
	MACRO_BEGIN							\
		((q_t *)(elt))->next = (que)->next;			\
		((q_t *)(elt))->prev = (que);				\
		(que)->next->prev = (q_t *)(elt);			\
		(que)->next = (q_t *)(elt);				\
	MACRO_END

#define Q_ADD_LAST(que, elt)						\
	MACRO_BEGIN							\
		((q_t *)(elt))->next = (que);				\
		((q_t *)(elt))->prev = (que)->prev;			\
		(que)->prev->next = (q_t *)(elt);			\
		(que)->prev = (q_t *)(elt);				\
	MACRO_END

#define Q_REM_FIRST(que, elt, typ)					\
	MACRO_BEGIN							\
		if ((que)->next == (que)) {				\
			(elt) = 0;					\
		} else {						\
			(elt) = (typ)((que)->next);			\
			(que)->next = (que)->next->next;		\
			(que)->next->prev = (que);			\
		}							\
	MACRO_END

#define Q_UNLINK(elt)							\
	MACRO_BEGIN							\
		q_t *_q = (q_t *)(elt);					\
		_q->next->prev = _q->prev;				\
		_q->prev->next = _q->next;				\
	MACRO_END

#define Q_INSERT_TAIL(que, elt, type, fld)				\
	MACRO_BEGIN							\
		q_t	*_prev;						\
									\
		for (_prev = (que)->prev;				\
		     _prev != (que) && ((type*)_prev)->fld < (elt)->fld;\
		     _prev = _prev->prev) {				\
		}							\
		Q_ADD_FIRST(_prev, elt);				\
	MACRO_END

#define Q_INSERT_HEAD(que, elt, type, fld)				\
	MACRO_BEGIN							\
		q_t	*_next;						\
									\
		for (_next = (que)->next;				\
		     _next != (que) && ((type*)_next)->fld > (elt)->fld;\
		     _next = _next->next) {				\
		}							\
		Q_ADD_LAST(_next, elt);					\
	MACRO_END

#define Q_PRINT(que) do {						\
	MACRO_BEGIN							\
		q_t *_q = (&(que))->next;				\
		dbg_printf("%s:\n", #que);				\
		dbg_printf("\t0x%x: 0x%x, 0x%x\n",			\
			   &que, (&(que))->next, (&(que))->prev);	\
		while (_q != &(que)) {					\
			dbg_printf("\t0x%x: 0x%x, 0x%x\n",		\
				   _q, _q->next, _q->prev);		\
			_q = _q->next;					\
		}							\
	MACRO_END


typedef struct q {
	struct q	*next;
	struct q	*prev;
} q_t;



/* Singly linked queues.
 *
 * These lists do not require locks but should be only used when
 * the caller cannot be preempted as they spin to resolve collisions.
 */
#define QS_DECLARE(qs)	qs_t * volatile qs
#define QS_INIT(que)	(*(que) = 0)

#define QS_BUSY		((qs_t *)1uL)

#define QS_ADD_FIRST(head, elt)						\
	MACRO_BEGIN							\
	ASSERT(sched_entered());					\
	for (;;) {							\
		((qs_t *)(elt))->qs_next = *(head);			\
		if (((qs_t *)(elt))->qs_next != QS_BUSY			\
		    && cmp_and_swap((void *)head,			\
				    ((qs_t *)(elt))->qs_next, elt)) {	\
			break;						\
		}							\
	}								\
	MACRO_END

#define QS_REM_FIRST(head, elt, typ)					\
	MACRO_BEGIN							\
	ASSERT(sched_entered());					\
	for (;;) {							\
		if (!(elt = (typ)*(head))) {				\
			break;						\
		}							\
		if (elt != (typ)QS_BUSY					\
		    && cmp_and_swap((void *)head, elt, QS_BUSY)) {	\
			*(head) = ((qs_t *)(elt))->qs_next;		\
			break;						\
		}							\
	}								\
	MACRO_END

typedef struct qs {
	struct qs	*qs_next;
} qs_t;


#endif /* !_Q_H_ */
