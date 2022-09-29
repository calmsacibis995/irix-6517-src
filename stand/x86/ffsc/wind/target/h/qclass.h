/* qClass.h - queue object class header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,15oct93,cd   added #ifndef _ASMLANGUAGE.
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01e,06apr91,gae  added NOMANUAL to avoid fooling mangen.
01d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01c,05jul90,jcf   added calibrate routine.
01b,26jun90,jcf   remove qClass/qType definitions of multi-way queues
		  added Q_CLASS_ID.
01a,21oct89,jcf   written.
*/

#ifndef __INCqClassh
#define __INCqClassh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"

#ifndef _ASMLANGUAGE

/* HIDDEN */

typedef struct q_class		/* Q_CLASS */
    {
    FUNCPTR createRtn;		/* create and initialize a queue */
    FUNCPTR initRtn;		/* initialize a queue */
    FUNCPTR deleteRtn;		/* delete and terminate a queue */
    FUNCPTR terminateRtn;	/* terminate a queue */
    FUNCPTR putRtn;		/* insert a node into q with insertion key */
    FUNCPTR getRtn;		/* return and remove lead node routine */
    FUNCPTR removeRtn;		/* remove routine */
    FUNCPTR resortRtn;		/* resort node to new priority */
    FUNCPTR advanceRtn;		/* advance queue by one tick routine */
    FUNCPTR getExpiredRtn;	/* return and remove an expired Q_NODE */
    FUNCPTR keyRtn;		/* return insertion key of node */
    FUNCPTR calibrateRtn;	/* calibrate every node in queue by an offset */
    FUNCPTR infoRtn;		/* return array of nodes in queue */
    FUNCPTR eachRtn;		/* call a user routine for each node in queue */
    struct q_class *valid;	/* valid == pointer to queue class */
    } Q_CLASS;

typedef Q_CLASS *Q_CLASS_ID;	/* Q_CLASS_ID */

/* END HIDDEN */

/*******************************************************************************
*
* Q_CLASS_VERIFY - check the validity of an queue class pointer
*
* This macro verifies the existence of the specified queue class by comparing
* the queue classes valid field to the pointer to the queue class.
*
* RETURNS: OK or ERROR if invalid class pointer
*
* NOMANUAL
*/

#define Q_CLASS_VERIFY(pQClass)						\
    (									\
    (((Q_CLASS *)(pQClass))->valid == (Q_CLASS *) pQClass) ?		\
	    OK								\
	:								\
	    ERROR							\
    )

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCqClassh */
