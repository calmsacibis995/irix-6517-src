/*************************************************************************
*                                                                        *
*               Copyright (C) 1992, 1993, 1994 Silicon Graphics, Inc.  	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident  "$Revision: 1.12 $ $Author: roe $"
#include <sgidefs.h>    /* for _MIPS_SIM defs */
#include <assert.h>
#include <bstring.h>
#include <ulocks.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
#include <malloc.h> 

/*
 * Data Structures for Posix.1b Message Queues
 *
 * There are two types of structures; those that are shared between
 * processes and those that are used within a process.
 *
 * Because the message queues are use-able between applications belonging
 * to different ABIs, the shared data structures, which are overlaid
 * on top of memory-mapped files, should not contain any fields whose
 * sizes and offsets vary between ABIs.
 *
 * These restrictions do not apply to the per-process data structure, the
 * message-queue descriptor table.
 */

#define PMQ_VERSION_1	1001
#define	PMQ_MAGIC	0x76543210

#define MQ_DEF_MAXMSG   32	/* default message queue size */
#define MQ_DEF_MSGSIZE  64	/* default size of a message */

#define MQ_PRIO_MAX	32	/* max priority levels for msgs */


/* Message list */

typedef struct pmq_list {
	int pmql_first;		/* index of first msg in the list */
	int pmql_last;		/* index of last message          */
} pmq_list_t;

/* Message header */

typedef struct pmq_hdr {
	int pmqh_next;		/* index of next message in the list */
	int pmqh_size;		/* size of the message               */
				/* size_t is not used, because it's  */
				/* length varies in the various ABIs */
} pmq_hdr_t;

/*
 * A sigevent structure needs to be attached to the queue to support
 * the mq_notify interface. However, the sigevent structure cannot be
 * included in the queue header because it's size varies between the ABIs.
 *
 * The pmq_sigevent_t structure, whose size is fixed across ABIs, is
 * defined to include the fields passed via the siginfo structure to a
 * signal handler.
 */
typedef struct pmq_sigevent {
	int		pmq_sigev_notify;
	int 		pmq_sigev_signo;
	__uint64_t 	pmq_sigev_value;
} pmq_sigevent_t;

/* 
 * Posix Message Queue
 *
 * A magic number is used to determine whether a message queue is a
 * valid POSIX message queue or not. The magic number is stored at
 * the beginning and the end of the queue header.
 *
 * NOTE: use the first word for the magic number. The sycnronizing
 *	 addresses used by PMQ_SENDER and PMQ_RECEIVER fall at offsets
 *	 0 and 1.
 */
typedef struct pmq { 
	int pmq_magic1;                    /* Magic number for Posix msg   */
	int pmq_version;		   /* version of the implementation */
	int pmq_curpid;			   /* curproc; for debugging	   */
	sem_t pmq_semlock;                 /* to protect accesses to queue */  
	int pmq_flags;                     /* notification flag 	   */
	int pmq_receivers;                 /* num of blocked receivers     */
	int pmq_senders;                   /* num of blocked senders       */
	int pmq_curmsgs;                   /* current number of msgs       */
	int pmq_maxmsg;                    /* max number of msgs           */
	int pmq_msgsize;          	   /* max msg size                 */
	pmq_sigevent_t pmq_sigevent;
	pmq_list_t pmq_list[MQ_PRIO_MAX + 1];    /* array of msg lists     */
	char pmq_map[MQ_PRIO_MAX/NBBY];    /* bitmap for non-empty lists   */
	__uint64_t reserved[4];
	int pmq_magic2;                    /* 2nd magic number		   */
	pmq_hdr_t pmq_hdr[1];              /* array of headers             */

	/*
	 * An array of pmq_hdr elements, followed by
	 * an array of msg buffers will be overlaid here
	 */
} pmq_t;

/*
 * Message queue flags
 */
#define PMQ_FLAG_NOTIFY_REGISTERED	0x1
#define PMQ_FLAG_IN_CRITICAL		0x2
#define PMQ_FLAG_KEEP_NOTIFICATION	0x4

/*
 * Message queue descriptor
 */
typedef struct mqd_desc {
	pmq_t 	*mqd_mq;	/* MQ pointer */
	int	mqd_attr;	/* MQ desciptor attributes */
} mqd_desc_t;

/*
 * Descriptor attribute flags
 */
#define	MQD_DESC_READ		0x1
#define	MQD_DESC_WRITE		0x2
#define	MQD_DESC_NONBLOCK	0x4
#define MQD_DESC_ALL		0x7  /* OR of all attr flags */

#define MQD_DESC_GET_PMQ(mqd)		(((mqd_desc_t *) mqd)->mqd_mq)
#define MQD_DESC_CLOSE(mqd)		(((mqd_desc_t *) mqd)->mqd_mq = 0)
#define MQD_DESC_ASET(mqd, attr)  	(((mqd_desc_t *) mqd)->mqd_attr |= attr)
#define MQD_DESC_ACLEAR(mqd, attr)	(((mqd_desc_t *) mqd)->mqd_attr &= ~attr)
#define MQD_DESC_ACHECK(mqd, attr)	(((mqd_desc_t *) mqd)->mqd_attr & attr)

#define MQD_IS_BOGUS(mqd)		((mqd) < 4096 ? 1 : 0)

/*
 * Each process has a linked list of message queue descriptors.
 * This list is maintained so that read, write, and nonblock
 * attributes may be set on a per descriptor basis, instead of
 * a per message queue basis.
 */

typedef struct mqd_link mqd_link_t;
struct mqd_link {
	mqd_desc_t	*desc;	/* pointer to array of descriptors */
	int 		ndescs;	/* number of descriptors in the array */
	mqd_link_t 	*next;	/* next descriptor in the chain	*/
};

#define NDESCS		2	/* initial number of descriptors */
#define NDESCS_INCR	16	/* incremental number of descriptors */

#define PMQ_SENDER_Q(pmq)	(pmq)
#define PMQ_RECEIVER_Q(pmq)	((char *)pmq + 1)
#define PMQ_FULL(pmq)		((pmq)->pmq_curmsgs == pmq->pmq_maxmsg)
#define PMQ_EMPTY(pmq)		((pmq)->pmq_curmsgs == 0)

#define PMQ_NOTIFICATION(pmq) \
	(pmq->pmq_flags & PMQ_FLAG_NOTIFY_REGISTERED)

#define PMQ_NOTIFICATION_SET(pmq) \
	(pmq->pmq_flags |= PMQ_FLAG_NOTIFY_REGISTERED)

#define PMQ_NOTIFICATION_CLEAR(pmq) \
	(pmq->pmq_flags &= ~PMQ_FLAG_NOTIFY_REGISTERED)

#define PMQ_IN_CRITICAL(pmq) \
	(pmq->pmq_flags & PMQ_FLAG_IN_CRITICAL)

#define PMQ_IN_CRITICAL_SET(pmq) \
	(pmq->pmq_flags |= PMQ_FLAG_IN_CRITICAL)

#define PMQ_IN_CRITICAL_CLEAR(pmq) \
	(pmq->pmq_flags &= ~PMQ_FLAG_IN_CRITICAL)

#define PMQ_KEEP_NOTIFICATION(pmq) \
	(pmq->pmq_flags & PMQ_FLAG_KEEP_NOTIFICATION)

#define PMQ_KEEP_NOTIFICATION_SET(pmq) \
	(pmq->pmq_flags |= PMQ_FLAG_KEEP_NOTIFICATION)

#define PMQ_KEEP_NOTIFICATION_CLEAR(pmq) \
	(pmq->pmq_flags &= ~PMQ_FLAG_KEEP_NOTIFICATION)

#define PMQ_IS_VALID(pmq) \
	(((pmq)->pmq_magic1 == PMQ_MAGIC) && ((pmq)->pmq_magic2 == PMQ_MAGIC))

#define PMQ_IS_BOGUS(pmq) \
	(!(pmq) || ((pmq)->pmq_magic1 != PMQ_MAGIC) || ((pmq)->pmq_magic2 != PMQ_MAGIC))

#define PMQ_NULL_MSG		-1

/*
 * Locking macro for mq descriptor list/table
 *
 * The underlying pmq lock is automatically initialized when
 * a process does an initial sproc(2).
 */
#include <ulocks.h>

extern int __us_rsthread_pmq;
#if _ABIAPI
#define __LOCK_MALLOC()		1
#define __UNLOCK_MALLOC()	0
#else
extern int __libc_lockpmq(void);
extern int __libc_unlockpmq(void);
#define __LOCK_PMQ()	__us_rsthread_pmq ?  __libc_lockpmq() : 1
#define __UNLOCK_PMQ() __us_rsthread_pmq ?  __libc_unlockpmq() : 0
#endif

