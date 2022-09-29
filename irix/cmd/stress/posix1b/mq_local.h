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
#ident  "$Revision: 1.3 $ $Author: joecd $"
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
 * These restrictions do not apply to the per-process data structure, the
 * message-queue descriptor table.
 */


/*
 * Shared Data Structures
 */
#define PMQ_VERSION_1 1001
#define	PMQ_MAGIC 0x76543210

/* Message list */

typedef struct pmq_list {
	int pmql_first;                 /* index of first msg in the list */
	int pmql_last;                  /* index of last message          */
} pmq_list_t;

/* Message header */

typedef struct pmq_hdr {
	int pmqh_next;                /* index of next message in the list */
	int pmqh_size;       	      /* size of the message */
				      /* size_t is not used, because it's  */
				      /* length varies in the various ABIs */
} pmq_hdr_t;

#define MQ_PRIO_MAX	32		/* max priority levels for msgs */

/*
 * A sigevent structure needs to be attached to the queue to support
 * the mq_notify interface.
 * However, the sigevent structure cannot be included in the queue header,
 * because it's size varies between the ABIs.
 * The pmq_sigevent_t structure, whose size is fixed across ABIs, is
 * defined to include the fields passed via the siginfo structure  to a
 * signal handler.
 */
typedef struct pmq_sigevent {
	int		pmq_sigev_notify;
	int 		pmq_sigev_signo;
	__uint64_t 	pmq_sigev_value;
} pmq_sigevent_t;

/* 
 * Posix Message Queue
 *	The MQ magic number is stored at the beginning and the end
 * 	of the queue header.
 *	NOTE: use the first word for the magic number. The sycnronizing
 *	addresses used by PMQ_SENDER and PMQ_RECEIVER fall at offsets
 *	0 and 1.
 */
struct posix_mq { 
	int pmq_magic1;                    /* Magic number for Posix msg */
	int pmq_version;		   /* version of the implementation */
	int pmq_curpid;			   /* curproc; for debugging	*/
	sem_t pmq_semlock;                 /* to protect accesses to queue */  
	int pmq_flags;                     /* notification flag 	   */
	int pmq_receivers;                 /* num of blocked receivers     */
	int pmq_senders;                   /* num of blocked senders       */
	int pmq_curmsgs;                   /* current number of msgs       */
	int pmq_maxmsg;                    /* max number of msgs           */
	int pmq_msgsize;          	   /* max msg size                 */
	pmq_sigevent_t pmq_sigevent;
	pmq_list_t pmq_list[MQ_PRIO_MAX + 1];/* array of msg lists         */
	char pmq_map[MQ_PRIO_MAX/NBBY];    /* bitmap for non-empty lists   */
	__uint64_t reserved[4];
	int pmq_magic2;                    /* 2nd magic number		   */
	pmq_hdr_t pmq_hdr[1];              /* array of headers             */

	/* An array of pmq_hdr elements,       */
	/* followed by an array of             */
	/* msg buffers, will be overlaid here  */
};

/* pmq_flags	*/
#define	PMQ_NOTIFY_REGISTERED	0x1

typedef struct posix_mq posix_mq_t;

#define NPMQS		32	/* initial number of pmqs */
#define NPMQS_INCR	16	/* incremental number of pmqs */

/*
 * Per-process private data structures
 */
typedef struct pmqd {
	posix_mq_t 	*pmqd_mq;	/* MQ pointer 			*/
	int 		pmqd_flags;	/* for RD, WR and O_NONBLOCK flags */
} pmqd_t;

/* pmqs_flags	*/
#define	PMQ_READ	0x1
#define	PMQ_WRITE	0x2
#define	PMQ_NONBLOCK	0x4

typedef struct pmqs_link pmqs_link_t;

struct pmqs_link {
	pmqd_t		*pmql_pmqs;	/* pointer to array of pmqs 	*/
	int 		pmql_npmqs;	/* number of pmqs in the array	*/
	pmqs_link_t 	*pmql_next;	/* next in the lin chain 	*/
};


#define PMQ_SENDER_Q(pmq)	(pmq)
#define PMQ_RECEIVER_Q(pmq)	((char *)pmq + 1)
#define PMQ_FULL(pmq)		(pmq->pmq_curmsgs == pmq->pmq_maxmsg)
#define PMQ_EMPTY(pmq)		(pmq->pmq_curmsgs == 0)
#define PMQ_NOTIFICATION(pmq)	(pmq->pmq_flags & PMQ_NOTIFY_REGISTERED)
#define PMQ_SET_NOTIFICATION(pmq) (pmq->pmq_flags |= PMQ_NOTIFY_REGISTERED)
#define PMQ_CLR_NOTIFICATION(pmq) (pmq->pmq_flags &= ~PMQ_NOTIFY_REGISTERED)
#define PMQ_NULL_MSG		-1

#define PMQ_OK(pmq)	 (((pmq)->pmq_magic1 == PMQ_MAGIC)		\
				&& ((pmq)->pmq_magic2 == PMQ_MAGIC))
#define MQ_OK(mqd)	 ((((pmqd_t *) mqd)->pmqd_mq->pmq_magic1 == PMQ_MAGIC)\
		&& (((pmqd_t *) mqd)->pmqd_mq->pmq_magic2 == PMQ_MAGIC))

#define MQ_READ_OK(mqd)	 (((pmqd_t *) mqd)->pmqd_flags & PMQ_READ)
#define MQ_WRITE_OK(mqd) (((pmqd_t *) mqd)->pmqd_flags & PMQ_WRITE)
#define MQ_BLOCK(mqd)	 ((((pmqd_t *) mqd)->pmqd_flags & PMQ_NONBLOCK) == 0)


#define SET_MQ_FLAGS_CLR(mqd)	 (((pmqd_t *) mqd)->pmqd_flags = 0)
#define SET_MQ_FLAGS(mqd, flag)	 (((pmqd_t *) mqd)->pmqd_flags |= flag)

/*
 * default values for maxmsg and msgsize attrbutes
 *
 * NOTE: These values are documented in the man page for mq_open; when
 *	modifying these values, modify the man page also.
 *	
 */
#define MQ_DEF_MAXMSG   32
#define MQ_DEF_MSGSIZE  64

/*
 * Locking macro for mq descriptor list/table
 *
 * The underlying pmq lock is automatically initialized when
 * a process does an initial sproc(2).
 */
#include <ulocks.h>

extern int __us_rsthread_pmq;
#ifdef _ABI_SOURCE
#define __LOCK_MALLOC()		1
#define __UNLOCK_MALLOC()	0
#else
extern int __libc_lockpmq(void);
extern int __libc_unlockpmq(void);
#define __LOCK_PMQ()	__us_rsthread_pmq ?  __libc_lockpmq() : 1
#define __UNLOCK_PMQ() __us_rsthread_pmq ?  __libc_unlockpmq() : 0
#endif

