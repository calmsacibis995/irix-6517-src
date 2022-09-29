/* msgQLibP.h - private message queue library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,19jul92,pme  added external declaration of shared msgQ show routine.
01a,04jul92,jcf  created.
*/

#ifndef __INCmsgQLibPh
#define __INCmsgQLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "msgqlib.h"
#include "classlib.h"
#include "qjoblib.h"
#include "private/objlibp.h"


typedef struct msg_q		/* MSG_Q */
    {
    OBJ_CORE		objCore;	/* object management */
    Q_JOB_HEAD		msgQ;		/* message queue head */
    Q_JOB_HEAD		freeQ;		/* free message queue head */
    int			options;	/* message queue options */
    int			maxMsgs;	/* max number of messages in queue */
    int			maxMsgLength;	/* max length of message */
    int			sendTimeouts;	/* number of send timeouts */
    int			recvTimeouts;	/* number of receive timeouts */
    } MSG_Q;

typedef struct			/* MSG_NODE */
    {
    Q_JOB_NODE		node;		/* queue node */
    int			msgLength;	/* number of bytes of data */
    } MSG_NODE;

#define MSG_NODE_DATA(pNode)   (((char *) pNode) + sizeof (MSG_NODE))

/* variable definitions */

extern CLASS_ID msgQClassId;		/* message queue class id */

/* shared memory objects function pointers */

extern FUNCPTR  msgQSmSendRtn;
extern FUNCPTR  msgQSmReceiveRtn;
extern FUNCPTR  msgQSmNumMsgsRtn;
extern FUNCPTR  msgQSmShowRtn;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	msgQTerminate (MSG_Q_ID msgQId);
extern STATUS	msgQInit (MSG_Q *pMsgQ, int maxMsgs, int maxMsgLength,
			  int options, void *pMsgPool);

#else	/* __STDC__ */

extern STATUS	msgQTerminate ();
extern STATUS	msgQInit ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmsgQLibPh */
