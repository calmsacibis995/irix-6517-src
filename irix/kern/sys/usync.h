/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1991 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * sys/usync.h -- structures and defines for usync interface
 */
#ifndef __SYS_USYNC_H__
#define __SYS_USYNC_H__

#ident "$Revision: 1.14 $"

/*
 * usync commands.
 */

#define USYNC_BLOCK           	1	/* block the process		*/ 
#define USYNC_INTR_BLOCK      	2	/* interruptible block		*/ 
#define USYNC_UNBLOCK_ALL    	3	/* unblock all processes	*/ 
#define USYNC_UNBLOCK         	4	/* unblock a process		*/ 
#define USYNC_NOTIFY_REGISTER 	5	/* register notification	*/ 
#define USYNC_NOTIFY          	6	/* send notification		*/ 
#define USYNC_NOTIFY_DELETE     7	/* remove the proc's notification */ 
#define USYNC_NOTIFY_CLEAR      8	/* remove any proc's notification */ 
#define USYNC_GET_STATE  	11	/* get waiter count or prepost value */
#define	USYNC_HANDOFF		12	/* interruptible block with handoff */

typedef struct usync_sigevent {
        int             ss_signo;	/* signal to be sent */
        int      	ss_si_code;	/* si_code for siginfo */
        __uint64_t     	ss_value;	/* holds the value for union sigval */
} usync_sigevent_t;

/*
 * usync_arg
 *
 * Data structure used to pass arguments to usync_cntl for various commands.
 * Defined so that its size and field-offsets are the same in all MIPS 
 * ABIs: o32, n32 and 64.
 */

typedef struct usync_arg {
	int		 ua_version;	/* version of the usync interface */
	__uint64_t	 ua_addr;	/* address of object */
	ushort_t	 ua_policy;	/* queuing policy */
	ushort_t	 ua_flags;
	usync_sigevent_t ua_sigev;
	__uint64_t	 ua_handoff;	/* secondary object for handoff */
	__uint64_t	 ua_sec;	/* timeout - seconds */
	__uint64_t	 ua_nsec;	/* timeout - nanoseconds */
	int		 ua_count;
	int		 ua_userpri;	/* user priority override value */
	int		 ua_res[4];	/* reserved for future use */
} usync_arg_t;

#define USYNC_POLICY_PRIORITY		0x01
#define USYNC_POLICY_FIFO		0x02

#define USYNC_FLAGS_PREPOST_CONSUME	0x01
#define USYNC_FLAGS_PREPOST_NONE	0x02
#define USYNC_FLAGS_TIMEOUT		0x04
#define USYNC_FLAGS_USERPRI		0x08

#define USYNC_FLAGS_NOPREPOST		USYNC_FLAGS_PREPOST_NONE

#define	USYNC_VERSION_1	1001            /* Obsolete version pre-IRIX 6.5 */
#define	USYNC_VERSION_2	1002

#ifdef CKPT
typedef struct usync_ckpt {
	off_t	off;		/* semaphore region offset */
	off_t	voff;		/* offset relative to uvaddr */
	pid_t	notify;		/* pid to get notification */
	int	value;		/* semaphore value */
	int	handoff;	/* handoff value */
	ushort_t policy;	/* queuing policy */
} usync_ckpt_t;
#endif

#ifndef _KERNEL
extern int	usync_cntl(int, void *);
#endif

#endif	/* __SYS_USYNC_H__ */
