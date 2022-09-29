/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1997 Silicon Graphics, Inc.		  *
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

#ifndef _SYS_SIGINFO_H
#define _SYS_SIGINFO_H

#ident "$Revision: 1.26 $"

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>
#if _POSIX93 || _XOPEN5
#include <sys/sigevent.h>	/* for sigval */
#endif

/*
 * This header is included in POSIX93 and XOPEN4UX mode.
 * the siginfo_t struct is required in procfs as well.
 * So this header must be safe to include under XPG as well as POSIX93
 * as well as still provide the basic definitions for POSIX90 if
 * included from an non-POSIX header.
 */
/*
 * POSIX 1003.1b / XPG4-UX extension for signal handler arguments
 * Note the double '_' - this is for XPG/POSIX symbol space rules. This
 * is different than MIPS ABI, but applications shouldn't be using anything
 * but the si_* names.
 */

#define SI_MAXSZ	128
#define SI_PAD		((SI_MAXSZ / sizeof(__int32_t)) - 3)

#if _SGIAPI
#define siginfo __siginfo
#endif

/*
 * ABI version of siginfo
 * Some elements change size in the 64 bit environment
 */
typedef struct __siginfo {
	int	si_signo;		/* signal from signal.h	*/
	int 	si_code;		/* code from above	*/
	int	si_errno;		/* error from errno.h	*/
	union {

		int	si_pad[SI_PAD];	/* for future growth	*/

		struct {			/* kill(), SIGCLD	*/
			pid_t	__pid;		/* process ID		*/
			union {
				struct {
					uid_t	__uid;
				} __kill;
				struct {
					clock_t __utime;
					int __status;
					clock_t __stime;
					int __swap;
				} __cld;
			} __pdata;
		} __proc;			

		struct {	/* SIGSEGV, SIGBUS, SIGILL and SIGFPE	*/
			void 	*__addr;	/* faulting address	*/
		} __fault;

		struct {			/* SIGPOLL, SIGXFSZ	*/
		/* fd not currently available for SIGPOLL */
			int	__fd;	/* file descriptor	*/
			long	__band;
		} __file;
#if _POSIX93 || _XOPEN5
		union sigval	__value;
#define si_value	__data.__value
#endif

	} __data;

} siginfo_t;

#define si_pid		__data.__proc.__pid
#define si_status	__data.__proc.__pdata.__cld.__status
#define si_stime	__data.__proc.__pdata.__cld.__stime
#define si_utime	__data.__proc.__pdata.__cld.__utime
#define si_swap		__data.__proc.__pdata.__cld.__swap
#define si_uid		__data.__proc.__pdata.__kill.__uid
#define si_addr		__data.__fault.__addr
#define si_fd		__data.__file.__fd
#define si_band		__data.__file.__band

/* for si_code, things that we use now */
#define SI_USER		0	/* user generated signal */
#define SI_KILL		SI_USER		/* kill system call */
#define SI_QUEUE	-1		/* sigqueue system call */
#define SI_ASYNCIO	-2		/* posix 1003.1b aio */
#define SI_TIMER	-3		/* posix 1003.1b timers */
#define SI_MESGQ	-4		/* posix 1003.1b messages */

/*
 * negative signal codes are reserved for future use for user generated
 * signals 
 */
#define SI_FROMUSER(__sip)	((__sip)->si_code <= 0)
#define SI_FROMKERNEL(__sip)	((__sip)->si_code > 0)

#if _XOPEN4UX
/* 
 * SIGILL signal codes 
 */
#define	ILL_ILLOPC	1	/* illegal opcode */
#define	ILL_ILLOPN	2	/* illegal operand */
#define	ILL_ILLADR	3	/* illegal addressing mode */
#define	ILL_ILLTRP	4	/* illegal trap */
#define	ILL_PRVOPC	5	/* privileged opcode */
#define	ILL_PRVREG	6	/* privileged register */
#define	ILL_COPROC	7	/* co-processor */
#define	ILL_BADSTK	8	/* bad stack */
#if _SGIAPI
#define NSIGILL		8
#endif

/* 
 * SIGFPE signal codes 
 */
#define	FPE_INTDIV	1	/* integer divide by zero */
#define	FPE_INTOVF	2	/* integer overflow */
#define	FPE_FLTDIV	3	/* floating point divide by zero */
#define	FPE_FLTOVF	4	/* floating point overflow */
#define	FPE_FLTUND	5	/* floating point underflow */
#define	FPE_FLTRES	6	/* floating point inexact result */
#define	FPE_FLTINV	7	/* invalid floating point operation */
#define FPE_FLTSUB	8	/* subscript out of range */
#if _SGIAPI
#define NSIGFPE		8
#endif

/* 
 * SIGSEGV signal codes 
 */
#define	SEGV_MAPERR	1	/* address not mapped to object */
#define	SEGV_ACCERR	2	/* invalid permissions */
#if _SGIAPI
#define NSIGSEGV	2
#endif

/* 
 * SIGBUS signal codes 
 */
#define	BUS_ADRALN	1	/* invalid address alignment */
#define	BUS_ADRERR	2	/* non-existent physical address */
#define	BUS_OBJERR	3	/* object specific hardware error */
#if _SGIAPI
#define NSIGBUS		3
#endif

/* 
 * SIGTRAP signal codes 
 */
#define TRAP_BRKPT	1	/* process breakpoint */
#define TRAP_TRACE	2	/* process trace */
#if _SGIAPI
#define NSIGTRAP	2
#endif

/* 
 * SIGCLD signal codes 
 */
#define	CLD_EXITED	1	/* child has exited */
#define	CLD_KILLED	2	/* child was killed */
#define	CLD_DUMPED	3	/* child has coredumped */
#define	CLD_TRAPPED	4	/* traced child has stopped */
#define	CLD_STOPPED	5	/* child has stopped on signal */
#define	CLD_CONTINUED	6	/* stopped child has continued */
#if _SGIAPI
#define NSIGCLD		6
#endif

/*
 * SIGPOLL signal codes
 */
#define POLL_IN		1	/* input available */
#define	POLL_OUT	2	/* output buffers available */
#define	POLL_MSG	3	/* output buffers available */
#define	POLL_ERR	4	/* I/O error */
#define	POLL_PRI	5	/* high priority input available */
#define	POLL_HUP	6	/* device disconnected */
#if _SGIAPI
#define NSIGPOLL	6
#endif

/*
 * SIGUME signal codes
 */
#if _SGIAPI
#define UME_ECCERR	1	/* uncorrectable ECC error */
#define NSIGUME		1
#endif

#endif	/* _XOPEN4UX */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_SIGINFO_H */
