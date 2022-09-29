/* mqueueP.h - private header for POSIX 1003.4 message passing */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,06apr93,smb written
*/

#ifndef __INCmqueuePh
#define __INCmqueuePh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "mqueue.h"
#include "time.h"
#include "signal.h"
#include "msgqlib.h"
#include "siglib.h"

typedef struct mq_info	/* message queue description */
    {
    MSG_Q_ID            msgQId;		  /* message queue identifier */
    int			close;		  /* number of attachments */
    int			taskId;		  /* task to notify */
    struct sigpend      signalInfo;	  /* notification information */
    long                attrMaxmsg;	  /* maximum number of messages */
    long        	attrMsgsize;      /* maximum message size */
    long        	attrCurmsgs;      /* number of messages queued */
    char *              mq_name;	  /* message queue name */
    } MQ_INFO;

#ifdef __cplusplus
}
#endif

#endif /* __INCmqueuePh */
