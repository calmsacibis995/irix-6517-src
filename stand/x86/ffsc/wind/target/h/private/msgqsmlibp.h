/* msgQSmLibP.h - private shared message queue library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,29jan93,pme  added little endian support
01d,29sep92,pme  added msgQSmNumMsgs(), msgQSmSend(), msgQSmReceived()
		 and msgQSmInfoGet() functions prototypes. 
01c,22sep92,rrr  added support for c++
01b,17sep92,pme  added shared message queue info get support.
01a,19jul92,pme  written.
*/

#ifndef __INCmsgQSmLibPh
#define __INCmsgQSmLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "msgqsmlib.h"
#include "smdlllib.h"
#include "private/semsmlibp.h"

/* defines */

#define MSG_Q_TYPE_SM 	10		/* shared message queue */

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

/* typedefs */

typedef struct sm_msg_q                 /* SM_MSG_Q */
    {
    UINT32              verify;         /* msgQ verification */
    UINT32		objType;	/* msgQ type */

    SM_SEMAPHORE        msgQSem;        /* msgQ shared counting semaphore */
    UINT32              msgQLock;       /* spinlock for msgQ */
    SM_DL_LIST          msgQ;           /* message queue head */

    SM_SEMAPHORE        freeQSem;       /* freeQ shared counting semaphore */
    UINT32              freeQLock;      /* spinlock for freeQ */
    SM_DL_LIST          freeQ;          /* free message queue head */

    UINT32              options;        /* message queue options */
    UINT32              maxMsgs;        /* max number of messages in queue */
    UINT32              maxMsgLength;   /* max length of message */
    UINT32              sendTimeouts;   /* number of send timeouts */
    UINT32              recvTimeouts;   /* number of receive timeouts */
    } SM_MSG_Q;

typedef struct  sm_msg_node             /* SM_MSG_NODE */
    {
    SM_DL_NODE          node;           /* queue node */
    int                 msgLength;      /* number of bytes of data */
    } SM_MSG_NODE;

#if (defined (CPU_FAMILY) && (CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

#define SM_MSG_NODE_DATA(pNode)   (((char *) pNode) + sizeof (SM_MSG_NODE))


/* variable declarations */

extern FUNCPTR msgQSmShowRtn;		/* shared msgQ show routine ptr */
extern FUNCPTR msgQSmInfoGetRtn;	/* shared msgQ info get routine ptr */


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void   msgQSmLibInit (void);
extern STATUS msgQSmInit (SM_MSG_Q * pMsgQ, int maxMsgs, int maxMsgLength,
			  int options, void * pMsgPool);
extern STATUS msgQSmShow (SM_MSG_Q_ID msgQId, int level);
extern STATUS msgQSmInfoGet (SM_MSG_Q_ID smMsgQId, MSG_Q_INFO * pInfo);
extern int    msgQSmNumMsgs (SM_MSG_Q_ID msgQId);
extern int    msgQSmReceive (SM_MSG_Q_ID msgQId, char *buffer,
                             UINT maxNBytes, int timeout);
extern STATUS msgQSmSend (SM_MSG_Q_ID msgQId, char *buffer, UINT nBytes,
                          int timeout, int priority);

#else   /* __STDC__ */

extern void   msgQSmLibInit ();
extern STATUS msgQSmInit ();
extern STATUS msgQSmShow ();
extern STATUS msgQSmInfoGet ();
extern int    msgQSmNumMsgs ();
extern int    msgQSmReceive ();
extern STATUS msgQSmSend ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmsgQSmLibPh */
