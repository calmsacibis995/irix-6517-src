/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: async_queue.h,v $
 * Revision 65.1  1997/10/20 19:19:54  jdoak
 * *** empty log message ***
 *
BINRevision 1.1.2.1  1996/10/02  17:48:17  damon
BIN	Newest DFS from Transarc
BIN
 * $EndLog$
 */
/*
*/
/*
 * async_queue.h
 *
 * (C) Copyright 1996 Transarc Corporation
 * All Rights Reserved.
 *
 */

#ifndef _TRANSARC_ASYNC_QUEUE_H
#define _TRANSARC_ASYNC_QUEUE_H

#include <dcedfs/lock.h>

/* elements in async queue */
typedef struct asyncElement {
    long size;			/* size of data buffer */
    long count;			/* amount of data in buffer */
    long offset;		/* offset into buffer */
    long error;			/* application error code */
    long last;			/* indicates last buffer on queue */
    long hold;			/* set to place a hold input buffer */
    char *data;			/* buffer pointer */
    struct asyncElement *next;	/* next element in queue */
} asyncElement_t;

/* async queue structure */
typedef struct asyncQueue {
    osi_dlock_t qLock;		/* controls access to pipe structure */
    asyncElement_t *inHead;	/* head of input queue */
    asyncElement_t *inTail;	/* tail of input queue */
    asyncElement_t *outHead;	/* head of output queue */
    asyncElement_t *outTail;	/* tail of output queue */
    asyncElement_t *curElem;	/* current queue element */
    void *threadState;		/* thread state data */
    void *callerState;		/* caller state data */
    int threadEnd;		/* tells thead to terminate */
    int threadDone;		/* set when thread terminates */
    void (*asyncRtn)();		/* routine to call with output elements */
    int allocSize;		/* anount of data allocated for queue */
} asyncQueue_t;

asyncElement_t *getAsyncElement _TAKES((asyncQueue_t *));

void relAsyncElement _TAKES((asyncQueue_t *, asyncElement_t *));

asyncQueue_t *initAsyncQueue _TAKES((long, long, void (*)(), void *, void *));

void termAsyncQueue _TAKES((asyncQueue_t *));

#endif /* _TRANSARC_ASYNC_QUEUE_H */
