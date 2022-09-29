/*************************************************************************
*                                                                        *
*               Copyright (C) 1995 Silicon Graphics, Inc.       	 *
*                                                                        *
*  These coded instructions, statements, and computer programs  contain  *
*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
*  are protected by Federal copyright law.  They  may  not be disclosed  *
*  to  third  parties  or copied or duplicated in any form, in whole or  *
*  in part, without the prior written consent of Silicon Graphics, Inc.  *
*                                                                        *
**************************************************************************/
#ident "$Id: mq_open.c,v 1.25 1999/04/27 16:00:22 roe Exp $"

#include <sgidefs.h>    /* for _MIPS_SIM defs */

#ifdef __STDC__
#pragma weak mq_open = _mq_open
#pragma weak mq_close = _mq_close
#pragma weak mq_unlink = _mq_unlink
#pragma weak mq_send = _mq_send
#pragma weak mq_receive = _mq_receive
#pragma weak mq_notify = _mq_notify
#pragma weak mq_setattr = _mq_setattr
#pragma weak mq_getattr = _mq_getattr
#endif

#include <synonyms.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <abi_mutex.h>
#include <mqueue.h>
#include <assert.h>
#include <sys/usync.h>
#include <sigevtop.h>
#include "mq_local.h"
#include "mplib.h"

/*
 * Posix.1b-1993
 * Message Queue interfaces
 */

#define PMQ_LOCK_INIT(pmq) \
	sem_init(&(pmq)->pmq_semlock, 1, 1); \
	sem_mode(&(pmq)->pmq_semlock, SEM_MODE_SPINSET, 10000); \
	sem_mode(&(pmq)->pmq_semlock, SEM_MODE_NOCNCL) \

#define PMQ_LOCK(pmq) 		sem_wait(&(pmq)->pmq_semlock)
#define PMQ_UNLOCK(pmq)		sem_post(&(pmq)->pmq_semlock)

#ifdef DEBUG

#define PMQ_DBG_OPEN		0x1
#define PMQ_DBG_CLOSE		0x2
#define PMQ_DBG_UNLINK		0x4
#define PMQ_DBG_SEND		0x8
#define PMQ_DBG_RECV		0x10
#define PMQ_DBG_GETATTR		0x20
#define PMQ_DBG_SETATTR		0x40
#define PMQ_DBG_NOTIFY		0x80

static int _pmq_debug; /*  = PMQ_DBG_OPEN; */

#define PMQ_DPRINT(debug_lvl,arg) if (_pmq_debug & debug_lvl) printf arg

#else	/* DEBUG */
#define PMQ_DPRINT(debug_lvl,arg) 
#endif	/* DEBUG */

#undef FILE_READ
#undef FILE_WRITE

#define FILE_READ	0x1
#define FILE_WRITE	0x2

static int _mqs_max = 0;
static int _mqs_open = 0;

static mqd_desc_t descs[NDESCS];
static mqd_link_t desc_table = {
	&descs[0],
	NDESCS,
	0
};

/*
 * local functions
 */
static void _init_mq(pmq_t *, long, long);
static void _add_message(pmq_t *, const char *, int , unsigned int );
static void _setup_freelist(pmq_t *);/*link buffers into free list */
static int _highest_bit_set(char *, int);
static int _remove_message(pmq_t *, char *, int, unsigned int *);
static mqd_desc_t *_get_mqd_desc(void);
static int _lock_file(int, int, int);
static int _unlock_file(int, int);
static void _print_pmq(pmq_t *pmq);
static int mq_notify_main(pmq_t *, const struct sigevent *);


/*
 * mq_open: open/create a message queue
 */
mqd_t
mq_open(mq_name, oflag, mode, create_attrs)
const char *mq_name;
int oflag;
mode_t mode;
mq_attr_t *create_attrs;
{
	struct pmq tmp_pmq;
	int tmp_oflag;
	int fd;
	void *mq_addr;
	int mq_size;
	int prot;
	long tmp_maxmsg, tmp_msgsize;
	mqd_desc_t *mqd;

	PMQ_DPRINT(PMQ_DBG_OPEN,
		("sizeof pmq_t = %4d\n",(sizeof(tmp_pmq))));
	PMQ_DPRINT(PMQ_DBG_OPEN,
		("sizeof sigevent = %4d\n",(sizeof(pmq_sigevent_t))));
	PMQ_DPRINT(PMQ_DBG_OPEN,
		("offset(pmq_version) = 0x%x\n",
			&((pmq_t *) 0)->pmq_version));
	PMQ_DPRINT(PMQ_DBG_OPEN,
		("offset(pmq_curmsgs) = 0x%x\n",
			&((pmq_t *) 0)->pmq_curmsgs));
	PMQ_DPRINT(PMQ_DBG_OPEN,
		("offset(pmq_msgsize) = 0x%x\n",
			&((pmq_t *) 0)->pmq_msgsize));
	PMQ_DPRINT(PMQ_DBG_OPEN,
		("offset(pmq_list) = 0x%x\n",
			&((pmq_t *) 0)->pmq_list[0]));
	PMQ_DPRINT(PMQ_DBG_OPEN,
		("offset(pmq_map) = 0x%x\n",
			&((pmq_t *) 0)->pmq_map[0]));
	PMQ_DPRINT(PMQ_DBG_OPEN,
		("offset(pmq_hdr) = 0x%x\n",
			&((pmq_t *) 0)->pmq_hdr[0]));

	/*
	 * File locks are used to avoid the race conditions that exist when
	 * creating/opening message queues simultaneously.
	 *
	 * When creating a queue with O_EXCL flag, the file must
	 * be created and initialized to be a queue for mq_open to
	 * succeed.
	 *
	 * When opening a queue, but not creating it, the file is locked
	 * and the queue header read.
	 *
	 * Completion of queue initialization is flagged by the setting
	 * of the magic number(s) in the queue header
	 */

	tmp_oflag = (oflag & ~(O_WRONLY)) | O_RDWR;
	if (oflag & O_CREAT) {
		fd = open(mq_name, tmp_oflag, mode);
		if (fd < 0) {
			if (oserror() == ENOENT)
				setoserror(EINVAL);
 			return ((mqd_t)-1);
		}

		if (_lock_file(fd, 0, FILE_WRITE) != 0) {
			close(fd);
			setoserror(EINVAL);
			return ((mqd_t)-1);
		}

		/*
		 * read the queue header to check if the queue exists
		 */
		if ((read(fd, &tmp_pmq, sizeof(tmp_pmq)) == sizeof(tmp_pmq))
			       && (PMQ_IS_VALID(&tmp_pmq)))	{
			(void) _unlock_file(fd, 0);
			/*
			 * Queue exists
			 */
			if (oflag & O_EXCL) {
				close(fd);
				setoserror(EEXIST);
				return ((mqd_t)-1);
			}
			/*
			 * non-exclusive case; do normal open
			 */
			goto open;
		}
		
		assert(fd >= 0);

		/*
		 * create the queue by writing the queue header to the
		 * file; wrlock for the file is held
		 */

		if (create_attrs) {
			tmp_maxmsg = create_attrs->mq_maxmsg;
			tmp_msgsize = create_attrs->mq_msgsize;
		} else {
			tmp_maxmsg = MQ_DEF_MAXMSG;
			tmp_msgsize = MQ_DEF_MSGSIZE;
		}

		mq_size = (int) (tmp_maxmsg * tmp_msgsize);
		mq_size += (tmp_maxmsg - 1) * sizeof(pmq_hdr_t);
		mq_size += sizeof(pmq_t);

		if (ftruncate(fd, mq_size) != 0) {
			(void) _unlock_file(fd, 0);
			close(fd);
			setoserror(EINVAL);
			return((mqd_t)-1);
		}
		prot = PROT_READ | PROT_WRITE;
		mq_addr = mmap(NULL, mq_size, prot, MAP_SHARED, fd, 0); 
		if (mq_addr == MAP_FAILED) {
			(void) _unlock_file(fd, 0);
			close(fd);
			setoserror(EINVAL);
			return((mqd_t)-1);
		}
		_init_mq((pmq_t *) mq_addr, tmp_maxmsg, tmp_msgsize);
		/*
		 * XXX
		 * 	should the file be sync'd to the disk ?
		 */
		(void) _unlock_file(fd, 0);
		goto open_done;
	} else {
		/*
		 * Message queue being opened, not created
		 */
		if ((fd = open(mq_name, tmp_oflag, mode)) < 0) {
			return((mqd_t)-1);
		}
		/*
		 * lock the file, read the queue header and check for a valid
		 * queue header
		 */

		if (_lock_file(fd, 0, FILE_READ) != 0) {
			close(fd);
			setoserror(EINVAL);
			return((mqd_t)-1);
		}

		if ((read(fd, &tmp_pmq, sizeof(tmp_pmq)) != sizeof(tmp_pmq))
			       || PMQ_IS_BOGUS(&tmp_pmq))	{
			(void) _unlock_file(fd, 0);
			close(fd);
			setoserror(ENOENT);
			return((mqd_t)-1);
		}
		(void) _unlock_file(fd, 0);
	}
open:

	mq_size = tmp_pmq.pmq_maxmsg * tmp_pmq.pmq_msgsize; 
	/*
	 * Note: first pmq_hdr_t is part of the pmq_t
	 */
	mq_size += (tmp_pmq.pmq_maxmsg - 1) * sizeof(pmq_hdr_t);
	mq_size += sizeof(pmq_t);
	prot = PROT_READ | PROT_WRITE;
	mq_addr = mmap(NULL, mq_size, prot, MAP_SHARED, fd, 0); 
	if (mq_addr == MAP_FAILED) {
		close(fd); 
		setoserror(EINVAL);
		return((mqd_t)-1);
	}

open_done:
	/*
	 * close the queue file
	 */
	close(fd); 
	mqd = _get_mqd_desc();
	if (mqd) {
		mqd->mqd_mq = (pmq_t *) mq_addr;
		MQD_DESC_ACLEAR(mqd, MQD_DESC_ALL);
		if (oflag & O_NONBLOCK)
			MQD_DESC_ASET(mqd, MQD_DESC_NONBLOCK);
		if (oflag & O_RDWR)
			MQD_DESC_ASET(mqd, (MQD_DESC_READ|MQD_DESC_WRITE));
		else if (oflag & O_WRONLY)
			MQD_DESC_ASET(mqd, MQD_DESC_WRITE);
		else
			MQD_DESC_ASET(mqd, MQD_DESC_READ);
		return ((mqd_t) mqd);
	}

	return ((mqd_t)-1); 
}

mqd_desc_t *
_get_mqd_desc()
{
	mqd_link_t *scan, **prev;
	mqd_desc_t *desc;
	int i;
	int size;

	scan = &desc_table;
	
	__LOCK_PMQ();

	if (_mqs_max == 0)
		_mqs_max = (int) sysconf(_SC_MQ_OPEN_MAX);

	if (_mqs_open >= _mqs_max) {
		__UNLOCK_PMQ();
		setoserror(EMFILE);
		return(NULL);
	}

	_mqs_open++;

	do {
		desc = scan->desc;
		for (i = 0; i < scan->ndescs; i++) {
			if (desc[i].mqd_mq == 0) {	/* found one */
				desc[i].mqd_mq = (pmq_t *) 1L;
				__UNLOCK_PMQ();
				return(&desc[i]);
			}
		}
		prev = &scan->next;
	} while ((scan = scan->next) != 0);

	/*
	 * allocate another chunk of pmqs and put them in the linked list
	 */
	size = sizeof(mqd_link_t) + sizeof(mqd_desc_t) * NDESCS_INCR;
	scan = (mqd_link_t *) malloc(size);
	if (scan == NULL) {
		_mqs_open--;
		__UNLOCK_PMQ();
		setoserror(ENOSPC);
		return(NULL);
	}

	(void)memset(scan, 0, size);
	scan->desc = (mqd_desc_t *) ((char *)scan + sizeof(*scan));
	scan->ndescs = NDESCS_INCR;
	*prev = scan;
	__UNLOCK_PMQ();

	return(&scan->desc[0]);
}

void
_remove_mqd_desc(mqd_desc_t *desc)
{
	__LOCK_PMQ();
	desc->mqd_mq = 0;
	_mqs_open--;
	__UNLOCK_PMQ();
}

void
_init_mq(pmq_t *pmq, long maxmsg, long msgsize)
{
	PMQ_LOCK_INIT(pmq);
	(void) memset(pmq->pmq_map, 0, sizeof(pmq->pmq_map));
	pmq->pmq_maxmsg = (int) maxmsg;
	pmq->pmq_msgsize = (int) msgsize;
	pmq->pmq_senders = 0;
	pmq->pmq_receivers = 0;
	pmq->pmq_curmsgs = 0;
	_setup_freelist(pmq);       /* link all msg buffers into free list */
	pmq->pmq_version = PMQ_VERSION_1;
	pmq->pmq_magic1 = PMQ_MAGIC;   /* flags initialization completion */
	pmq->pmq_magic2 = PMQ_MAGIC;   /* flags initialization completion */
}

/*
 * link all msg buffers into free list
 */
void
_setup_freelist(pmq_t *pmq)
{
	int index;

	for (index=0; index < (pmq->pmq_maxmsg - 1); index++) {
		pmq->pmq_hdr[index].pmqh_next = index + 1;
		pmq->pmq_hdr[index].pmqh_size = 0;
	}
	pmq->pmq_hdr[pmq->pmq_maxmsg - 1].pmqh_next = PMQ_NULL_MSG;
	pmq->pmq_list[MQ_PRIO_MAX].pmql_first = 0;
	pmq->pmq_list[MQ_PRIO_MAX].pmql_last = pmq->pmq_maxmsg - 1;
	for(index=0; index < MQ_PRIO_MAX; index++) {
		pmq->pmq_list[index].pmql_first =
			pmq->pmq_list[index].pmql_last = PMQ_NULL_MSG;
	}
}

/*
 * mq_close: close a message queue
 *
 * mq is closed by unmapping the queue file.
 * If a notify request is present for this process,
 * it is removed upon close.
 */
int
mq_close(mqd_t mqd)
{
	pmq_t *pmq;
	int mq_size;
	usync_arg_t uarg;
	int		key;
	evtop_data_t	data;

	/*
	 * Check message queue validity
	 */

	if (MQD_IS_BOGUS(mqd)) {
		setoserror(EBADF);
		return (-1); 
	}
		
	pmq = MQD_DESC_GET_PMQ(mqd);

	if (PMQ_IS_BOGUS(pmq)) {
    		setoserror(EBADF);
		return (-1); 
	}

	if (PMQ_LOCK(pmq)) {
    		setoserror(EINTR);
		return (-1); 
	}

	/*
	 * check and cancel the notification request of this process
	 */
	if (PMQ_NOTIFICATION(pmq)) {
		/*
		 * remove the notification
		 */
		uarg.ua_version = USYNC_VERSION_2;
		uarg.ua_addr = (__uint64_t) PMQ_SENDER_Q(pmq); 
		uarg.ua_flags = 0;
		if (usync_cntl(USYNC_NOTIFY_DELETE, &uarg) == 0) {
			/*
			 * this proc's notification is deleted
			 */
			PMQ_NOTIFICATION_CLEAR(pmq);
		}
	}

	mq_size = pmq->pmq_maxmsg * pmq->pmq_msgsize;
	mq_size += (pmq->pmq_maxmsg - 1) * sizeof(pmq_hdr_t);
	mq_size += sizeof(pmq_t);
	PMQ_UNLOCK(pmq);

	if (munmap((void *) pmq, mq_size) == -1) {
    		setoserror(EBADF);
		return (-1); 
	}

	/*
	 * close pmq descriptor
	 */
	_remove_mqd_desc((mqd_desc_t*) mqd);

	/*
	 * locate any message event and delete it
	 */
	data.evtop_mqid = mqd;
	if (__evtop_lookup(EVTOP_MQ, &data, &key) == 0) {
		__evtop_free(key);
	}

    	return (0);
}

/*
 * mq_unlink: unlink a message queue
 */
int
mq_unlink(const char *name)
{
	int fd, err;
	pmq_t tmp_pmq;
	ssize_t rval;

	/*
	 * verify that "name" does name a message queue, before
	 * unlinking it
	 */
	if ((fd = open(name, O_RDONLY)) < 0) {
		if ((oserror() != ENAMETOOLONG) && (oserror() != EACCES)
				&& (oserror() != EINTR))
    			setoserror(ENOENT);
		return (-1); 
	}

	rval = read(fd, &tmp_pmq, sizeof(tmp_pmq));
	close(fd);

	if (rval == -1) {
		if ((err = oserror()) != EINTR)
    			err = ENOENT;
    		setoserror(err);
		return (-1); 
	}

	if ((rval != sizeof(tmp_pmq)) || PMQ_IS_BOGUS(&tmp_pmq)) {
    		setoserror(ENOENT);
		return (-1); 
	}

	/*
	 * Ok, unlink it.
	 */

	if (unlink(name) == -1) {
		if ((oserror() != EACCES) && (oserror() != ENOENT) &&
				(oserror() != EINTR))
    			setoserror(EACCES);
		return (-1); 
	}

    	return (0);
}

/*
 * mq_send: send messages to a queue
 */
int
mq_send(mqd_t mqd, const char *msg_ptr, size_t msg_len, unsigned int msg_prio)
{
	pmq_t *pmq;
	usync_arg_t uarg;
	pmq_sigevent_t *event;
	int rval;

	MTLIB_CNCL_TEST();

	/*
	 * Check message queue validity,
	 * and make sure it's open for writing
	 */

	if (MQD_IS_BOGUS(mqd)) {
		setoserror(EBADF);
		return (-1); 
	}
		
	pmq = MQD_DESC_GET_PMQ(mqd);

	if (PMQ_IS_BOGUS(pmq) || !MQD_DESC_ACHECK(mqd, MQD_DESC_WRITE)) {
    		setoserror(EBADF);
		return (-1); 
	}

	if (msg_prio >= MQ_PRIO_MAX) {
    		setoserror(EINVAL);
		return (-1); 
	}

	if (msg_len > pmq->pmq_msgsize) {
    		setoserror(EMSGSIZE);
		return (-1); 
	}

	if (PMQ_LOCK(pmq)) {
    		setoserror(EINTR);
		return (-1); 
	}

retry_send:

	if (PMQ_FULL(pmq)) {

		if (!MQD_DESC_ACHECK(mqd, MQD_DESC_NONBLOCK)) {
			int rv;
			extern int __usync_cntl(int, void *);

			pmq->pmq_senders++; 
			PMQ_UNLOCK(pmq);

			uarg.ua_version = USYNC_VERSION_2;
			uarg.ua_addr = (__uint64_t) PMQ_SENDER_Q(pmq); 
			uarg.ua_policy = USYNC_POLICY_PRIORITY;

			uarg.ua_userpri = MTLIB_VAL( (MTCTL_PRI_HACK), -1 );
			if (uarg.ua_userpri >= 0)
				uarg.ua_flags = USYNC_FLAGS_USERPRI;
			else
				uarg.ua_flags = 0;

			MTLIB_BLOCK_CNCL_VAL( rv,
				__usync_cntl(USYNC_INTR_BLOCK, &uarg) );

			while (PMQ_LOCK(pmq)); /* spin */
			pmq->pmq_senders--; 
			if (rv) {
				PMQ_UNLOCK(pmq);
    				setoserror(EINTR);
				return (-1); 
			}

			goto retry_send;
		}

		/*
		 * This is a non-blocking operation
		 */
		PMQ_UNLOCK(pmq);
		setoserror(EAGAIN);
		return (-1); 
	} 

	_add_message(pmq, msg_ptr, (int) msg_len, msg_prio); 

	/*
	 * wake up a waiting receiver, if any.
	 * if no waiting receivers, send notification, if registered,
	 * only if the queue is transitioning from empty to non-empty state.
	 */
	if (pmq->pmq_receivers) {
		PMQ_UNLOCK(pmq);
		uarg.ua_version = USYNC_VERSION_2;
		uarg.ua_addr = (__uint64_t) PMQ_RECEIVER_Q(pmq); 
		uarg.ua_flags = 0;
		usync_cntl(USYNC_UNBLOCK, &uarg); 
		/*
		pmq->pmq_receivers-- is done elsewhere 
		*/
		return (0);
	}

	if ((pmq->pmq_curmsgs == 1) && (PMQ_NOTIFICATION(pmq))) {
		event = &pmq->pmq_sigevent;
		uarg.ua_version = USYNC_VERSION_2;
		uarg.ua_addr = (__uint64_t) PMQ_SENDER_Q(pmq); 

		if (event->pmq_sigev_notify == SIGEV_SIGNAL) {
			uarg.ua_sigev.ss_signo = event->pmq_sigev_signo;
			uarg.ua_sigev.ss_value = event->pmq_sigev_value;
			uarg.ua_sigev.ss_si_code = SI_MESGQ;
			uarg.ua_flags = 0;
			PMQ_IN_CRITICAL_SET(pmq);
			PMQ_UNLOCK(pmq);
			rval = usync_cntl(USYNC_NOTIFY, &uarg);
			if (rval == 0) {
				/*
				 * signal sent
				 */
			} else {
				/*
				 * XXX what should happen when usync() fails?
				 */
			}
		} else {
			PMQ_IN_CRITICAL_SET(pmq);
			PMQ_UNLOCK(pmq);
			(void) usync_cntl(USYNC_NOTIFY_CLEAR, &uarg);
		}

		while(PMQ_LOCK(pmq)); /* spin */
		PMQ_IN_CRITICAL_CLEAR(pmq);

		/*
		 * Another notification could happen between the time we
		 * release the pmq and the time we re-lock it in the 
		 * "critical" section above.   If this is the case, we must
		 * ensure we do not accidentally clear the flag indicating
		 * the new notification here.
		 */
		if (PMQ_KEEP_NOTIFICATION(pmq))
			PMQ_KEEP_NOTIFICATION_CLEAR(pmq);
		else
			PMQ_NOTIFICATION_CLEAR(pmq);
	}

	PMQ_UNLOCK(pmq);

	return (0);
}

/*
 * add_message:	add a message to the appopriate priority list
 */
void
_add_message(pmq_t *pmq,
	     const char *msgbuf,
	     int msgsize,
	     unsigned int msgprio)
{
	int msg_index, last_msg_index;
	char *cp;

	/* get a free msg hdr/buffer */

	msg_index = pmq->pmq_list[MQ_PRIO_MAX].pmql_first;
	if (!(msg_index >= 0 && msg_index < pmq->pmq_maxmsg)) {
		fprintf(stderr, "add_mesg: msg_index = %d\n", msg_index);
		_print_pmq(pmq);
		/*
		 * XXX
		 *	change exit() to abort or something
		 */
		exit(2);
	}
	assert(msg_index >= 0 && msg_index < pmq->pmq_maxmsg);
	pmq->pmq_list[MQ_PRIO_MAX].pmql_first =
					pmq->pmq_hdr[msg_index].pmqh_next;
	cp = (char *) &pmq->pmq_hdr[pmq->pmq_maxmsg];
	/* cp is now ptr to first msg buffer  */

	cp = cp + pmq->pmq_msgsize * msg_index;
	/* cp is now ptr to the free buffer chosen earlier */

	PMQ_DPRINT(PMQ_DBG_SEND,
	("add_msg: msg_index = 0x%x msg_size = 0x%x\n",
						msg_index, pmq->pmq_msgsize));
	PMQ_DPRINT(PMQ_DBG_SEND,
	("add_msg: 1_hdr = 0x%x 1_msg = 0x%x cur_msg = 0x%x msg_buf = 0x%x\n",
		&pmq->pmq_hdr[0],
		(char *) &pmq->pmq_hdr[pmq->pmq_maxmsg], cp, msgbuf));	

	(void) memcpy(cp, msgbuf, msgsize);
	pmq->pmq_hdr[msg_index].pmqh_size = (int) msgsize;
	pmq->pmq_curmsgs++;
	assert(pmq->pmq_curmsgs >= 0 && pmq->pmq_curmsgs <= pmq->pmq_maxmsg);

	/* now add the new message at the tail of the appropriate msg list */
	last_msg_index = pmq->pmq_list[msgprio].pmql_last;
	if (last_msg_index == PMQ_NULL_MSG) { /* empty list */
		pmq->pmq_list[msgprio].pmql_first =
		pmq->pmq_list[msgprio].pmql_last = msg_index;
		/* set the bit for this list in pmq_map */
		cp = &pmq->pmq_map[msgprio/NBBY];
		*cp |= (1 << (msgprio % NBBY));
		PMQ_DPRINT(PMQ_DBG_SEND,
			("map = 0x%x\n",(*(unsigned int *)pmq->pmq_map)));
	} else { /* non-empty list */
		pmq->pmq_hdr[last_msg_index].pmqh_next = msg_index;
		pmq->pmq_list[msgprio].pmql_last = msg_index;
	}
	pmq->pmq_hdr[msg_index].pmqh_next = PMQ_NULL_MSG;
}

/*
 * mq_receive: read messages from a queue
 */
ssize_t
mq_receive(mqd_t mqd, char *msg_ptr, size_t msg_len, unsigned int *msg_prio)
{
	pmq_t *pmq;
	usync_arg_t uarg;

	MTLIB_CNCL_TEST();

	/*
	 * Check message queue validity,
	 * and make sure it's open for reading
	 */

	if (MQD_IS_BOGUS(mqd)) {
		setoserror(EBADF);
		return (-1); 
	}
		
	pmq = MQD_DESC_GET_PMQ(mqd);

	if (PMQ_IS_BOGUS(pmq) || !MQD_DESC_ACHECK(mqd, MQD_DESC_READ)) {
    		setoserror(EBADF);
		return (-1); 
	}

	if (msg_len < pmq->pmq_msgsize) {
    		setoserror(EMSGSIZE);
		return (-1); 
	}

	if (PMQ_LOCK(pmq)) {
    		setoserror(EINTR);
		return (-1); 
	}

retry_receive:

	if (PMQ_EMPTY(pmq)) {

		if (!MQD_DESC_ACHECK(mqd, MQD_DESC_NONBLOCK)) {
			int rv;
			extern int __usync_cntl(int, void *);

			pmq->pmq_receivers++; 
			PMQ_UNLOCK(pmq);

			uarg.ua_version = USYNC_VERSION_2;
			uarg.ua_addr = (__uint64_t) PMQ_RECEIVER_Q(pmq); 
			uarg.ua_policy = USYNC_POLICY_PRIORITY;

			uarg.ua_userpri = MTLIB_VAL( (MTCTL_PRI_HACK), -1 );
			if (uarg.ua_userpri >= 0)
				uarg.ua_flags = USYNC_FLAGS_USERPRI;
			else
				uarg.ua_flags = 0;

			MTLIB_BLOCK_CNCL_VAL( rv,
				__usync_cntl(USYNC_INTR_BLOCK, &uarg) );

			while (PMQ_LOCK(pmq)); /* spin */
			pmq->pmq_receivers--; 
			if (rv) {
				PMQ_UNLOCK(pmq);
    				setoserror(EINTR);
				return (-1); 
			}

			goto retry_receive;
		}

		/*
		 * This is a non-blocking operation
		 */
		PMQ_UNLOCK(pmq);
		setoserror(EAGAIN);
		return (-1); 
	} 

	msg_len = _remove_message(pmq, msg_ptr, (int) msg_len, msg_prio); 

	if (pmq->pmq_senders) {
		PMQ_UNLOCK(pmq);
		uarg.ua_version = USYNC_VERSION_2;
		uarg.ua_addr = (__uint64_t) PMQ_SENDER_Q(pmq); 
		uarg.ua_flags = 0;
		usync_cntl(USYNC_UNBLOCK, &uarg); 
	} else
		PMQ_UNLOCK(pmq);

	return ((ssize_t) msg_len);
}

/*
 * highest_bit_set: return the index of the highest bit set in the bitmap
 */
int
_highest_bit_set(char *bitmap, int count)
{
	int idx, bit;

	PMQ_DPRINT(PMQ_DBG_RECV,
	("hbs: count = %d map = 0x%x\n",count, (*(unsigned int *)bitmap)));
	for(idx = count - 1; idx >= 0; idx--) {
	PMQ_DPRINT(PMQ_DBG_RECV,("hbs: 0x%x\n",bitmap[idx]));
		if (bitmap[idx]) {
			for(bit = NBBY - 1; bit >= 0; bit--)
				if (bitmap[idx] & (1 << bit))
					return(idx*NBBY + bit);
		}	
	}
	return (-1);
}

/*
 * remove_message: remove a message to the highest priority list
 */
int
_remove_message(pmq_t *pmq,
		char *msgbuf,
		int msgsize,
		unsigned int *msg_prio)
{
	int msg_index;
	unsigned int msgprio;
	char *cp;

	/*  get index of  highest priority message */

	msgprio = _highest_bit_set(pmq->pmq_map, MQ_PRIO_MAX/NBBY);
	msg_index = pmq->pmq_list[msgprio].pmql_first;
	assert(msg_index >= 0 && msg_index < pmq->pmq_maxmsg);

	PMQ_DPRINT(PMQ_DBG_RECV,
		("remove_msg: prio = %d index = %d\n",msgprio, msg_index));

	/* de-queue the message from the priority list */
	pmq->pmq_list[msgprio].pmql_first =
				pmq->pmq_hdr[msg_index].pmqh_next;
	/* check if list empty */
	if (pmq->pmq_list[msgprio].pmql_first == PMQ_NULL_MSG) {
		pmq->pmq_list[msgprio].pmql_last = PMQ_NULL_MSG;
		cp = &pmq->pmq_map[msgprio/NBBY];
		*cp &= ~(1 << (msgprio % NBBY));
	}
		

	cp = (char *) &pmq->pmq_hdr[pmq->pmq_maxmsg];
	/* cp is now ptr to first msg buffer  */

	cp = cp + pmq->pmq_msgsize * msg_index;
	/* cp is now ptr to the msg buffer chosen earlier */

	msgsize = pmq->pmq_hdr[msg_index].pmqh_size;
	(void) memcpy(msgbuf, cp, msgsize);
	pmq->pmq_curmsgs--;
	assert(pmq->pmq_curmsgs >= 0 && pmq->pmq_curmsgs <= pmq->pmq_maxmsg);

	/*
	 * now put the message hdr/buffer into the free list, at the
 	 * head of the list (LIFO)
	 */

	pmq->pmq_hdr[msg_index].pmqh_next =
				pmq->pmq_list[MQ_PRIO_MAX].pmql_first;
	pmq->pmq_list[MQ_PRIO_MAX].pmql_first = msg_index;
	if (msg_prio)
		*msg_prio = msgprio;
	return(msgsize);
}

/*
 * mq_notify: register notification for a mq
 */
int
mq_notify(mqd_t mqid, const struct sigevent *sev)
{
	pmq_t		*pmq;
	int		e;
	evtop_data_t	data;
	int		key;
	sigevent_t	sev_kern;

	/*
	 * Check message queue validity
	 */
	if (MQD_IS_BOGUS(mqid)) {
		setoserror(EBADF);
		return (-1); 
	}
	pmq = MQD_DESC_GET_PMQ(mqid);
		
	/*
	 * Check for magic number
	 */
	if (PMQ_IS_BOGUS(pmq)) {
    		setoserror(EBADF);
		return (-1); 
	}

	/* A null sev unregisters the event */
	data.evtop_mqid = mqid;
	if (!sev) {

		/* We cannot tell whether there is an event thread
		 */
		if (__evtop_lookup(EVTOP_MQ, &data, &key) == 0) {
			__evtop_free(key);
		}

		return (mq_notify_main(pmq, sev));
	}

	/* Go straight to the kernel if no event thread */
	if (sev->sigev_notify != SIGEV_THREAD) {
		return (mq_notify_main(pmq, sev));
	}

	/* Make sure event code is initialised and get a key */
	if (e = __evtop_alloc(sev, EVTOP_MQ|EVTOP_FFREE, &data, &key)) {
		setoserror(e);
		return (-1);
	}

	/* Set up kernel sigevent part */
	sev_kern = *sev;			/* copy user input */
	sev_kern.sigev_signo = SIGPTINTR;	/* special event signal */
	sev_kern.sigev_notify = SIGEV_SIGNAL;
	sev_kern.sigev_value.sival_int = key;	/* passed back by signal */

	/* Create disarmed kernel timer */
	if (mq_notify_main(pmq, &sev_kern)) {
		e = oserror();
		__evtop_free(key);
		setoserror(e);
		return (-1);
	}

	return (0);
}


static int
mq_notify_main(pmq_t *pmq, const struct sigevent *notification)
{
	usync_arg_t uarg;
	int rval;

	if (PMQ_LOCK(pmq)) {
    		setoserror(EINTR);
		return (-1); 
	}

	uarg.ua_version = USYNC_VERSION_2;
	uarg.ua_addr = (__uint64_t) PMQ_SENDER_Q(pmq); 
	uarg.ua_flags = 0;

	if (notification) {
		rval = usync_cntl(USYNC_NOTIFY_REGISTER, &uarg);
		if (rval == -1) {
			PMQ_UNLOCK(pmq);
			if (oserror() == EBUSY) {
				/*
				 * notification already registered
				 */
				return (-1); 
			} else {
				setoserror(EBADF);
				return (-1); 
			}
		}
		/*
		 * save the sigevent information; pid is already registered
		 * with the usync driver
		 */
		pmq->pmq_sigevent.pmq_sigev_notify = notification->sigev_notify;
		pmq->pmq_sigevent.pmq_sigev_value = (__uint64_t)
			notification->sigev_value.sival_ptr;
		pmq->pmq_sigevent.pmq_sigev_signo = notification->sigev_signo;
		PMQ_NOTIFICATION_SET(pmq);

		/*
		 * Ensure that mq_send() does not clear this notification if it
		 * is waiting for the pmq lock in its "critical" stage.
		 */
		if (PMQ_IN_CRITICAL(pmq))
			PMQ_KEEP_NOTIFICATION_SET(pmq);
	} else {
		/*
		 * remove the notification
		 */
		rval = usync_cntl(USYNC_NOTIFY_DELETE, &uarg);
		if (rval == -1) {
			PMQ_UNLOCK(pmq);
			if (oserror() == EBUSY) {
				/*
				 * notification registered by another process;
				 * posix.1b spec doesn't say that an error
				 * should be returned in this case.
				 */
				return (0); 
			} else {
				setoserror(EBADF);
				return (-1); 
			}
		}
		PMQ_NOTIFICATION_CLEAR(pmq);
	}
	PMQ_UNLOCK(pmq);
	return(0);
}

/*
 * mq_setattr:	set attributes for a mq
 */
int
mq_setattr(mqd_t mqd, const struct mq_attr *mqstat, struct mq_attr *omqstat)
{
	pmq_t *pmq;

	/*
	 * Check message queue validity
	 */

	if (MQD_IS_BOGUS(mqd)) {
		setoserror(EBADF);
		return (-1); 
	}

	pmq = MQD_DESC_GET_PMQ(mqd);

	if (PMQ_IS_BOGUS(pmq)) {
    		setoserror(EBADF);
		return (-1); 
	}

	if (PMQ_LOCK(pmq)) {
    		setoserror(EINTR);
		return (-1); 
	}

	/*
	 * Currently, the only user-modifiable flag is O_NONBLOCK
	 */
	if (omqstat) {
		if (MQD_DESC_ACHECK(mqd, MQD_DESC_NONBLOCK))
			omqstat->mq_flags = O_NONBLOCK;
		else
			omqstat->mq_flags = 0;
		omqstat->mq_maxmsg = (long) pmq->pmq_maxmsg;
		omqstat->mq_msgsize = (long) pmq->pmq_msgsize;
		omqstat->mq_curmsgs = (long) pmq->pmq_curmsgs;
	}

	if (mqstat->mq_flags & O_NONBLOCK)
		MQD_DESC_ASET(mqd, MQD_DESC_NONBLOCK);
	else
		MQD_DESC_ACLEAR(mqd, MQD_DESC_NONBLOCK);

	PMQ_UNLOCK(pmq);
	return (0);
}

/*
 * mq_getattr:	get attributes of a mq
 */
int
mq_getattr(mqd_t mqd, struct mq_attr *mqstat)
{
	pmq_t *pmq;

	/*
	 * Check message queue validity
	 */

	if (MQD_IS_BOGUS(mqd)) {
		setoserror(EBADF);
		return (-1); 
	}

	pmq = MQD_DESC_GET_PMQ(mqd);

	if (PMQ_IS_BOGUS(pmq)) {
    		setoserror(EBADF);
		return (-1); 
	}

	if (PMQ_LOCK(pmq)) {
    		setoserror(EINTR);
		return (-1); 
	}

	if (MQD_DESC_ACHECK(mqd, MQD_DESC_NONBLOCK))
		mqstat->mq_flags = O_NONBLOCK;
	else
		mqstat->mq_flags = 0;

	mqstat->mq_maxmsg = pmq->pmq_maxmsg;
	mqstat->mq_msgsize = pmq->pmq_msgsize;
	mqstat->mq_curmsgs = pmq->pmq_curmsgs;

	PMQ_UNLOCK(pmq);
	return (0);
}

/*
 * lock_file: locks the file, fd, for writing for 1 byte starting at 'where'
 *
 * returns : 0,  when successful
 *	    -1,  if unable to aquire the lock
 */
int
_lock_file(int fd, int where, int lock_type)
{
	struct flock flock;
	register int rv;

	do {
		if (lock_type == FILE_READ)
			flock.l_type= F_RDLCK; 
		else
			flock.l_type= F_WRLCK; 
		flock.l_whence = 0; 
		flock.l_start= where; 
		flock.l_len= 1; 
		rv = fcntl(fd, F_SETLKW, &flock);
	} while (rv == -1 && oserror() == EINTR);

	if (rv == -1)
		return(-1);

	return(0);
}

/*
 * unlock_file: unlocks the file lock. 
 *
 * returns : 0,  when successful
 *	    -1,  if unable to unlock the lock
 */
int
_unlock_file(int fd, int where)
{
	struct flock unsetlock;

	unsetlock.l_type = F_UNLCK; 
	unsetlock.l_whence = 0; 
	unsetlock.l_start = where; 
	unsetlock.l_len = 1;
	if ((fcntl (fd, F_SETLK, &unsetlock)) == -1)
		return(-1);

	return(0);
}

/*
 * print_pmq: info for debugging
 */
void
_print_pmq(pmq_t *pmq)
{
	int msg_index;

	fprintf(stderr,"mq_maxmsg = 0x%x mq_msgsize = 0x%x pmq_senders = %d\n",
			pmq->pmq_maxmsg, pmq->pmq_msgsize,pmq->pmq_senders);
	fprintf(stderr,"pmq_receivers = %d pmq_curmsgs = %d\n",
			pmq->pmq_receivers, pmq->pmq_curmsgs);
	
	fprintf(stderr,"free_list\n");
	
	msg_index = pmq->pmq_list[MQ_PRIO_MAX].pmql_first;
	while (msg_index != PMQ_NULL_MSG) {
		fprintf(stderr,"\t%3d\n",msg_index);
		msg_index = pmq->pmq_hdr[msg_index].pmqh_next;
	}
}
