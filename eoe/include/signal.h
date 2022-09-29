#ifndef __SIGNAL_H__
#define __SIGNAL_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.57 $"
/*
 *
 * Copyright 1992-1996 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * WARNING - this is an ANSI/POSIX/XPG4 header. In ANSI mode only
 * ANSI symbols are permitted
 * Since sys/signal.h is used by alot of other headers, and we need
 * some of the symbols in sys/signal.h even in ANSI mode for those headers
 * we do NOT include sys/signal.h here in ANSI mode. Instead we duplicate
 * the few required defines that ANSI has.
 */
#include <standards.h>

#if !_NO_ANSIMODE
/*
 * Put ALL required ANSI symbols here
 * NOTE: these are duplicated in sys/signal.h
 */

#ifndef _KERNEL
extern void      (*signal(int, void (*)( )))( );
#endif

#define	SIGHUP	1	/* hangup */
#define	SIGINT	2	/* interrupt (rubout) */
#define	SIGQUIT	3	/* quit (ASCII FS) */
#define	SIGILL	4	/* illegal instruction (not reset when caught)*/
#define	SIGTRAP	5	/* trace trap (not reset when caught) */
#define	SIGIOT	6	/* IOT instruction */
#define SIGABRT 6	/* used by abort, replace SIGIOT in the  future */
#define	SIGEMT	7	/* EMT instruction */
#define	SIGFPE	8	/* floating point exception */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	SIGBUS	10	/* bus error */
#define	SIGSEGV	11	/* segmentation violation */
#define	SIGSYS	12	/* bad argument to system call */
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGALRM	14	/* alarm clock */
#define	SIGTERM	15	/* software termination signal from kill */
#define	SIGUSR1	16	/* user defined signal 1 */
#define	SIGUSR2	17	/* user defined signal 2 */
#define	SIGCLD	18	/* death of a child */
#define SIGCHLD 18	/* 4.3BSD's/POSIX name */
#define	SIGPWR	19	/* power-fail restart */
#define	SIGWINCH 20	/* window size changes */
#define	SIGURG	21	/* urgent condition on IO channel */
#define SIGPOLL 22	/* pollable event occurred */
#define	SIGIO	22	/* input/output possible signal */
#define	SIGSTOP	23	/* sendable stop signal not from tty */
#define	SIGTSTP	24	/* stop signal from tty */
#define	SIGCONT	25	/* continue a stopped process */
#define	SIGTTIN	26	/* to readers pgrp upon background tty read */
#define	SIGTTOU	27	/* like TTIN for output if (tp->t_local&LTOSTOP) */
#define SIGVTALRM 28	/* virtual time alarm */
#define SIGPROF	29	/* profiling alarm */
#define SIGXCPU	30	/* Cpu time limit exceeded */
#define SIGXFSZ	31	/* Filesize limit exceeded */
#define	SIGK32	32	/* Reserved for kernel usage */
#define	SIGCKPT	33	/* Checkpoint warning */
#define	SIGRESTART 34	/* Restart warning  */
/* Signals defined for Posix 1003.1c. */
#define SIGPTINTR	47
#define SIGPTRESCHED	48
/* Posix 1003.1b signals */
#define SIGRTMIN	49	/* Posix 1003.1b signals */
#define SIGRTMAX	64	/* Posix 1003.1b signals */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#ifndef SIG_ERR
typedef void	(*SIG_PF) (int);

#define SIG_ERR		((SIG_PF)-1L)
#define	SIG_IGN		((SIG_PF)1L)
#define	SIG_DFL		((SIG_PF)0L)
#endif

typedef int 	sig_atomic_t;
extern int raise(int);
#endif /* _LANGUAGE_C */

#else /* !_NO_ANSIMODE */
#include <sys/signal.h>

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/* ANSI */
typedef int 	sig_atomic_t;
extern int raise(int);

#if _POSIX90

#if !defined(_BSD_COMPAT) && !defined(_BSD_SIGNALS) 
extern int kill(pid_t, int);
#endif /* !BSD */
extern int sigaction(int, const struct sigaction *, struct sigaction *);
extern int sigpending(sigset_t *);
extern int sigprocmask(int, const sigset_t *, sigset_t *);
extern int sigsuspend(const sigset_t *);
extern int sigaddset(sigset_t *, int);
extern int sigdelset(sigset_t *, int);
extern int sigemptyset(sigset_t *);
extern int sigfillset(sigset_t *);
extern int sigismember(const sigset_t *, int);
#endif	/* _POSIX90 */

#if  _POSIX93 || _XOPEN5
/*
 * POSIX1003.1b additions
 */
#include <sys/timespec.h>

extern int sigqueue(pid_t , int, const union sigval);
extern int sigwaitinfo(const sigset_t *, siginfo_t *);
extern int sigtimedwait(const sigset_t *, siginfo_t *, const timespec_t *);
extern int sigwait(const sigset_t *, int *);
#endif /* _POSIX93 || _XOPEN5 */

#if _POSIX1C
#include <sys/types.h>
extern int pthread_kill(pthread_t, int);
extern int pthread_sigmask(int, const sigset_t *, sigset_t *);
#endif /* _POSIX1C */

#if _XOPEN4UX || _XOPEN5
/*
 * XOPEN with UX extensions
 */
#if !defined(_BSD_COMPAT) && !defined(_BSD_SIGNALS) 
extern int sigpause(int);
#endif /* !BSD */
extern __sigret_t (*sigset(int,__sigret_t (*)(__sigargs)))(__sigargs);

extern int sighold(int);
extern int sigrelse(int);
extern int sigignore(int);
extern int siginterrupt(int, int);
#if (_NO_XOPEN4 && _NO_XOPEN5) || _ABIAPI
extern int sigaltstack(const stack_t *, stack_t *);
#else
extern int __xpg4_sigaltstack(const stack_t *, stack_t *);
/*REFERENCED*/
static int sigaltstack(const stack_t *nss, stack_t *oss)
{
	return __xpg4_sigaltstack(nss, oss);
}
#endif	/* _NO_XOPEN4 */
#endif /* _XOPEN4UX || _XOPEN5 */

#if _SGIAPI || _ABIAPI
extern char *_sys_siglist[];
extern int _sys_nsig;

#include <sys/types.h>
#include <sys/procset.h>
extern int (*ssignal(int, int (*)(int)))(int);
extern int gsignal(int);
extern int sigsend(idtype_t, id_t, int);
extern int sigsendset(const procset_t *, int);

/*
 * The signal-masking routine sgi_altersigs() contains an action 
 * parameter; the routine can either add or delete a set of signals.
 */
#define ADDSIGS         1
#define DELSIGS         2

/*
 * The following macro returns true if any of the signals in the sigset_t
 * pointed to by "setptr" are set.
 */
#define sgi_siganyset(setptr)   (sgi_sigffset(setptr,0) != 0)

extern int sgi_altersigs(int, sigset_t *, int[]);
extern int sgi_sigffset(sigset_t *, int);
extern int sgi_dumpset(sigset_t *);

#endif 	/* _SGIAPI || _ABIAPI */

#endif  /* _LANGUAGE_C */
#endif /* _NO_ANSIMODE */

#ifdef __cplusplus
}
#endif
#endif /* !__SIGNAL_H__ */
