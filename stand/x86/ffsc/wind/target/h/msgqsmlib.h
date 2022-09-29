/* msgQSmLib.h - shared message queue library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,29sep92,pme  removed msgQSmNumMsgs(), msgQSmSend(), msgQSmReceived()
		 and msgQSmInfoGet() functions prototypes.
01b,22sep92,rrr  added support for c++
01a,19jul92,pme  cleaned up according to code review.
                 written.
*/

#ifndef __INCmsgQSmLibh
#define __INCmsgQSmLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "msgqlib.h"


/* typedefs */

typedef struct sm_msg_q * SM_MSG_Q_ID;		/* shared message queue ID */


/* macros */

/* The following macro determines the number of bytes needed to buffer
 * a message of the specified length.  The node size is rounded up for
 * efficiency an alignement.  The total buffer space required for a pool for
 * <maxMsgs> messages each of up to <maxMsgLength> bytes is:
 *
 *     maxMsgs * SM_MSG_NODE_SIZE (maxMsgLength)
 */

#define SM_MSG_NODE_SIZE(msgLength)	\
	(((sizeof (SM_MSG_NODE) + msgLength) + 3) & ~3)

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern MSG_Q_ID  msgQSmCreate (int maxMsgs, int maxMsgLength, int options);
extern void      msgQSmShowInit ();

#else   /* __STDC__ */

extern MSG_Q_ID  msgQSmCreate ();
extern void      msgQSmShowInit ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmsgQSmLibh */
