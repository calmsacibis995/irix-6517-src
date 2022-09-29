/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_KSYS_KQUEUE_H_
#define	_KSYS_KQUEUE_H_	1
#ident "$Id: kqueue.h,v 1.7 1997/07/17 14:12:57 leedom Exp $"

#include <stddef.h>
#include <sys/sema.h>


/*
 * This file contains definitions for manipulating doubly-linked queue.
 * Inline functions and macros are used whenever possible to make these
 * as fast as possible.  Note that many of the machinations below are
 * essentially base and derived class inheritance, etc.  Of course all of
 * this would easier in C++ ...
 */


/*
 * Simple, non-MP safe queues:
 * ===========================
 */

typedef struct kqueue {	/* doubly linked queue element and head structure */
	struct kqueue	*kq_next;	/* next link in queue */
	struct kqueue	*kq_prev;	/* previous link in queue */
} kqueue_t, kqueuehead_t;

static __inline void
kqueue_init(kqueuehead_t *queue)
    /*
     * Initialize a queue to the empty list.
     */
{
    queue->kq_prev = queue;
    queue->kq_next = queue;
}

#if defined(_KERNEL)

static __inline void
kqueue_enter(kqueuehead_t *queue, kqueue_t *element)
    /*
     * Insert an element, onto the front of a queue.  The element is
     * represented by a pointer to a kqueue_t field within the element.  The
     * insert is not MP safe.
     */
{
    element->kq_next = queue->kq_next;
    element->kq_prev = queue;
    queue->kq_next->kq_prev = element;
    queue->kq_next = element;
}

static __inline void
kqueue_enter_last(kqueuehead_t *queue, kqueue_t *element)
    /*
     * Append an element, element, onto the end of queue, queue.  The
     * element is represented by a pointer to a kqueue_t field within the
     * element.  The append is not MP safe.
     */
{
    element->kq_next = queue;
    element->kq_prev = queue->kq_prev;
    queue->kq_prev->kq_next = element;
    queue->kq_prev = element;
}

static __inline void
kqueue_remove(kqueue_t *element)
    /*
     * Remove element, element, from the list which it is threaded on.  The
     * element is represented by a pointer to a kqueue_t field within the
     * element.  The element's next and previous fields are not reset to
     * NULL.  The remove is not MP safe.
     */
{
    element->kq_prev->kq_next = element->kq_next;
    element->kq_next->kq_prev = element->kq_prev;
}

static __inline void
kqueue_null(kqueue_t *queue)
    /*
     * NULL-out the next and previous fields on a kqueue_t.  Typically this
     * is used on a queue element after removing it from a queue.  Perhaps
     * we should do this as part of the remove operation above or offer two
     * different flavors of the remove ...
     */
{
    queue->kq_prev = queue->kq_next = NULL;
}


/*
 * Miscelaneous access methods and predicates:
 * ===========================================
 */

static __inline int
kqueue_isnull(kqueue_t *queue)
{
    return queue->kq_prev == NULL;
}

static __inline int
kqueue_isempty(kqueue_t *queue)
{
    return queue->kq_next == queue;
}

static __inline kqueue_t *
kqueue_first(kqueue_t *queue)
{
    return queue->kq_next;
}

static __inline kqueue_t *
kqueue_last(kqueue_t *queue)
{
    return queue->kq_prev;
}

static __inline kqueue_t *
kqueue_next(kqueue_t *queue)
{
    return queue->kq_next;
}

static __inline kqueue_t *
kqueue_prev(kqueue_t *queue)
{
    return queue->kq_prev;
}

static __inline kqueue_t *
kqueue_end(kqueue_t *queue)
{
    return queue;
}

#endif /* _KERNEL */


/*
 * MP safe queues:
 * ===============
 */

typedef struct { /* head of a lock protected queue */
	kqueue_t	head;		/* queue head */
#if MP
	lock_t		lock;		/* lock to protect the queue */
#else
	/* Use splhi()/splx() */
#endif
} mpkqueuehead_t;

#if defined(_KERNEL)

static __inline void
mpkqueue_init(mpkqueuehead_t *queue)
    /*
     * Initialize a lock protected queue.
     */
{
    kqueue_init(&queue->head);
#if MP
    spinlock_init(&queue->lock, NULL);
#endif
}

/*ARGSUSED*/
static __inline int
mpkqueue_lock(mpkqueuehead_t *queue)
    /*
     * Lock an MP queue.
     */
{
#ifdef MP
    return mutex_spinlock(&queue->lock);
#else
    return splhi();
#endif
}

/*ARGSUSED*/
static __inline void
mpkqueue_unlock(mpkqueuehead_t *queue, int s)
    /*
     * Unlock an MP queue.
     */
{
#ifdef MP
    mutex_spinunlock(&queue->lock, s);
#else
    splx(s);
#endif
}

static __inline void
mpkqueue_enter(mpkqueuehead_t *queue, kqueue_t *element)
    /*
     * Instert an element onto the front of an MP queue.
     */
{
    int s = mpkqueue_lock(queue);
    kqueue_enter(&queue->head, element);
    mpkqueue_unlock(queue, s);
}

static __inline void
mpkqueue_enter_last(mpkqueuehead_t *queue, kqueue_t *element)
    /*
     * Append an element onto the end of an MP queue.
     */
{
    int s = mpkqueue_lock(queue);
    kqueue_enter_last(&queue->head, element);
    mpkqueue_unlock(queue, s);
}

static __inline void
mpkqueue_remove(mpkqueuehead_t *queue, kqueue_t *element)
    /*
     * Remove an element from an MP queue.
     */
{
    int s = mpkqueue_lock(queue);
    kqueue_remove(element);
    mpkqueue_unlock(queue, s);
}

#endif /* _KERNEL */


/*
 * Miscelaneous ...
 * ================
 */

#if defined(_KERNEL)

/*
 * Return a pointer to the containing base type given a pointer to one of
 * the base type's fields.  Often used to translate a pointer to a linked
 * list link within a structure to the enclosing structure.  This really
 * ought to go into <stddef.h> ...
 */
#define	baseof(type, field, fieldp) \
	(type *)(void *)((char *)(fieldp) - offsetof(type, field))

#endif /* _KERNEL */

#endif	/* _KSYS_KQUEUE_H_ */
