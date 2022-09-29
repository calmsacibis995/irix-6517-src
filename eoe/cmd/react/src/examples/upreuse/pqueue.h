/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __PQUEUE_H__
#define __PQUEUE_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <ulocks.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>

#include "pdescdef.h"
#include "sema.h"
#include "config.h"

/*
 * Definition of Queue Elements
 */

typedef struct qelem {
        pdesc_t*      pdesc;
        struct qelem* fp;
        struct qelem* bp;
} qelem_t;

/*
 * Buffer Element Queue (self-consistency checking version)
 */

typedef struct queue {
        qelem_t* hd;
        int      nelem;
        usema_t* mutex;
        usema_t* count;
} queue_t;


/*
 * Queue header pdesc
 */
#define QHDPDESC   ((pdesc_t*)(1))

queue_t* queue_create();
void queue_enqueue(queue_t* q, qelem_t* qe);
qelem_t* queue_dequeue(queue_t* q);
void queue_destroy(queue_t* q);


#endif /* __PQUEUE_H__ */
