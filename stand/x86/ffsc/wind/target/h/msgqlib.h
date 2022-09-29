/* msgQLib.h - message queue library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02j,13jul93,wmd  use MEM_ROUND_UP to determine MSG_NODE_SIZE.
02i,22sep92,rrr  added support for c++
02h,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01e,05oct90,dnw  changed MSG_Q_INFO structure.
01d,05oct90,dnw  changed function declarations for new interface.
01c,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01b,07aug90,shl  moved function declarations to end of file.
01a,10may90,dnw  written
*/

#ifndef __INCmsgQLibh
#define __INCmsgQLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"


/* generic status codes */

#define S_msgQLib_INVALID_MSG_LENGTH		(M_msgQLib | 1)
#define S_msgQLib_NON_ZERO_TIMEOUT_AT_INT_LEVEL	(M_msgQLib | 2)
#define S_msgQLib_INVALID_QUEUE_TYPE		(M_msgQLib | 3)

/* message queue options */

#define MSG_Q_TYPE_MASK	0x01	/* mask for pend queue type in options */
#define MSG_Q_FIFO	0x00	/* tasks wait in FIFO order */
#define MSG_Q_PRIORITY	0x01	/* tasks wait in PRIORITY order */


/* message send priorities */

#define MSG_PRI_NORMAL	0	/* normal priority message */
#define MSG_PRI_URGENT	1	/* urgent priority message */

/* message queue typedefs */

typedef struct msg_q *MSG_Q_ID;	/* message queue ID */

typedef struct			/* MSG_Q_INFO */
    {
    int     numMsgs;		/* OUT: number of messages queued */
    int     numTasks;		/* OUT: number of tasks waiting on msg q */

    int     sendTimeouts;	/* OUT: count of send timeouts */
    int     recvTimeouts;	/* OUT: count of receive timeouts */

    int     options;		/* OUT: options with which msg q was created */
    int     maxMsgs;		/* OUT: max messages that can be queued */
    int     maxMsgLength;	/* OUT: max byte length of each message */

    int     taskIdListMax;	/* IN: max tasks to fill in taskIdList */
    int *   taskIdList;		/* PTR: array of task ids waiting on msg q */

    int     msgListMax;		/* IN: max msgs to fill in msg lists */
    char ** msgPtrList;		/* PTR: array of msg ptrs queued to msg q */
    int *   msgLenList;		/* PTR: array of lengths of msgs */

    } MSG_Q_INFO;

/* macros */

/* The following macro determines the number of bytes needed to buffer
 * a message of the specified length.  The node size is rounded up for
 * efficiency.  The total buffer space required for a pool for
 * <maxMsgs> messages each of up to <maxMsgLength> bytes is:
 *
 *    maxMsgs * MSG_NODE_SIZE (maxMsgLength)
 */

#define MSG_NODE_SIZE(msgLength) \
	(MEM_ROUND_UP((sizeof (MSG_NODE) + msgLength)))

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	msgQLibInit (void);
extern MSG_Q_ID msgQCreate (int maxMsgs, int maxMsgLength, int options);
extern STATUS 	msgQDelete (MSG_Q_ID msgQId);
extern STATUS 	msgQSend (MSG_Q_ID msgQId, char *buffer, UINT nBytes,
			  int timeout, int priority);
extern int 	msgQReceive (MSG_Q_ID msgQId, char *buffer, UINT maxNBytes,
			     int timeout);
extern STATUS 	msgQInfoGet (MSG_Q_ID msgQId, MSG_Q_INFO *pInfo);
extern int 	msgQNumMsgs (MSG_Q_ID msgQId);
extern void 	msgQShowInit (void);
extern STATUS 	msgQShow (MSG_Q_ID msgQId, int level);

#else	/* __STDC__ */

extern STATUS 	msgQLibInit ();
extern MSG_Q_ID 	msgQCreate ();
extern STATUS 	msgQDelete ();
extern STATUS 	msgQSend ();
extern int 	msgQReceive ();
extern STATUS 	msgQInfoGet ();
extern int 	msgQNumMsgs ();
extern void 	msgQShowInit ();
extern STATUS 	msgQShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmsgQLibh */
