/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_POLL_H	/* wrapper symbol for kernel use */
#define _SYS_POLL_H	/* subject to change without notice */

#ident "$Revision: 1.41 $"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long nfds_t;

/*
 * Structure of file descriptor/event pairs supplied in
 * the poll arrays.
 */
struct pollfd {
	int fd;				/* file desc to poll */
	short events;			/* events of interest on fd */
	short revents;			/* events that occurred on fd */
};

/*
 * Testable select events
 */
#define POLLIN		0x0001		/* fd is readable */
#define POLLPRI		0x0002		/* high priority info at fd */
#define	POLLOUT		0x0004		/* fd is writeable (won't block) */
#define POLLRDNORM	0x0040		/* normal data is readable */
#define POLLRDBAND	0x0080		/* out-of-band data is readable */
#define POLLWRBAND	0x0100		/* out-of-band data is writeable */

#define POLLWRNORM	POLLOUT
#define POLLNORM	POLLRDNORM

/*
 * Flags to pollwakeup().  These are not testable select()/poll() events.
 */
#define POLLWAKEUPONE	0x0200 		/* pollwakup() only one waiter */
#define POLLCONN	POLLWAKEUPONE	/* paranoid compatibility */

/*
 * Non-testable poll events (may not be specified in events field,
 * but may be returned in revents field).
 */
#define POLLERR		0x0008		/* fd has error condition */
#define POLLHUP		0x0010		/* fd has been hung up on */
#define POLLNVAL	0x0020		/* invalid pollfd entry */

#if defined(_KERNEL) || defined(_KMEMUSER)
#include <sys/sema.h>

/*
 * Data necessary to notify process sleeping in poll(2)
 * when an event has occurred.
 */
struct polldat {
	struct polldat *pd_next;	/* next in poll list */
	short		pd_events;	/* events being polled */
	short		pd_rotorhint;	/* used in pollwakeup() */
	struct pollhead *pd_headp;	/* backpointer to head of list */
	void		*pd_arg;	/* data saved for pollwakeup */
#if CELL_IRIX
	pid_t		pd_pid;		/* for distributed pollwakeup */
	ushort_t	pd_tid;		/* for distributed pollwakeup */
#endif
};

/*
 * Poll list head structure.  A pointer to this is passed to pollwakeup()
 * from the caller indicating the event has occurred.
 *
 * We need to keep track of the ``greneration count'' of each pollhead.
 * This is incremented once for each call to pollwakeup().  In dopoll() we
 * snapshot this value when we probe an object to see if it currently has
 * any of the events pending that we're interested in.  Later, if we didn't
 * find any such events pending, we pass this snapshot value to polladd() as
 * we're adding ourselves to the pollhead's list of ``interested processes''
 * (represented by a polldat structure per process).  If polladd() notices
 * that the generation number has changed it returns with an ``add failure''
 * notification which tells dopoll() that it needs to retry the probe of the
 * object.
 *
 * The lock ph_lock protects both ph_list and ph_gen from simultaneous access.
 */
struct pollhead {
	struct polldat	*ph_list;	/* list of pollers */
	lock_t		ph_lock;	/* protects list and events */
	volatile unsigned int ph_gen;	/* generation number */
	short		ph_events;	/* events pending on list */
	short		ph_user;	/* scratch for owner of handle */
	struct polldat	*ph_next;		/* distributed poll */
};

/*
 * We define an access macro for the ph_gen field in order to maintain a
 * semblence of opaqueness for the pollhead data structure.  We really
 * don't want drivers diving into this data structure but do want to give
 * them high performance access to the pollhead's generation number.  This
 * macro is obviously a compromise.
 */
#define POLLGEN(php)	((php)->ph_gen)

#endif /* _KERNEL || _KMEMUSER */

#if defined(_KERNEL) && !defined(_STANDALONE)
#include <sys/uthread.h>

/*
 * Data structure used by dopoll() and fdt_select_convert().
 */
struct pollvobj {
	void		*pv_vobj;	/* vnode or vsocket pointer */
	short		pv_vtype;	/* vobj type: 1=vnode, 2=vsocket */
};

/*
 * Number of pollfd entries to read in at a time in poll.
 * The larger the value then potentially the better the performance, 
 * except that larger numbers use more kernel stack space.  
 */
#define NPOLLFILE 64

/*
 * Routine called to notify a process of the occurrence
 * of an event.
 */
extern void pollwakeup	(struct pollhead *, short);
extern void initpollhead	(struct pollhead *);
extern void destroypollhead	(struct pollhead *);
extern int distributed_polladd(struct pollhead *, short, unsigned int);
extern void pollwakeup_thread(uthread_t *, short);
#ifdef CELL_IRIX
extern void pollrestart(struct pollhead *);
#endif

#else	/* !_KERNEL */

extern int poll(struct pollfd *, nfds_t, int);

#endif	/* !_KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_POLL_H */
