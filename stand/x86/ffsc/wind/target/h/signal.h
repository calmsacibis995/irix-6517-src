/* signal.h - signal facility library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
02i,11nov94,kdl  provide paramless func ptrs in structs, for non-ANSI (SPR 3742)
02h,10nov94,caf  adjusted placement of _ASMLANGUAGE conditional.
02g,06oct93,yao  added _ASMLANGUAGE conditional.
02h,12jan94,kdl  added sigqueue() prototype.
02g,09nov93,rrr  update to posix 1003.4 draft 14
02f,05feb93,rrr  fixed spr 1986 (SIG_ERR ... prototype) and
		 spr 1906 (signal numbers to match sun os)
02e,15oct92,rrr  silenced warnings
02d,22sep92,rrr  added support for c++
02c,22aug92,rrr  added bsd prototypes.
02b,27jul92,smb  added prototype for raise().
02a,04jul92,jcf  cleaned up.
01b,26may92,rrr  the tree shuffle
01a,19feb92,rrr  written from posix spec
*/

#ifndef __INCsignalh
#define __INCsignalh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE
#include "sigevent.h"

struct timespec;
#endif	/* _ASMLANGUAGE */

#define SIGEV_NONE	0
#define SIGEV_SIGNAL	1

/*
 * Signal Numbers:
 * 	Required .1 signals	 1-13
 * 	Job Control signals	14-19 (not implemented but must be defined)
 * 	Realtime signals	20-27
 */
#define	SIGHUP	1	/* hangup */
#define	SIGINT	2	/* interrupt */
#define	SIGQUIT	3	/* quit */
#define	SIGILL	4	/* illegal instruction (not reset when caught) */
#define	SIGTRAP	5	/* trace trap (not reset when caught) */
#define	SIGABRT 6	/* used by abort, replace SIGIOT in the future */
#define	SIGEMT	7	/* EMT instruction */
#define	SIGFPE	8	/* floating point exception */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	SIGBUS	10	/* bus error */
#define	SIGSEGV	11	/* segmentation violation */
#define SIGFMT	12	/* STACK FORMAT ERROR (not posix) */
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGALRM	14	/* alarm clock */
#define	SIGTERM	15	/* software termination signal from kill */

#define	SIGSTOP	17	/* sendable stop signal not from tty */
#define	SIGTSTP	18	/* stop signal from tty */
#define	SIGCONT	19	/* continue a stopped process */
#define	SIGCHLD	20	/* to parent on child stop or exit */
#define	SIGTTIN	21	/* to readers pgrp upon background tty read */
#define	SIGTTOU	22	/* like TTIN for output if (tp->t_local&LTOSTOP) */

#define	SIGUSR1 30	/* user defined signal 1 */
#define	SIGUSR2 31	/* user defined signal 2 */

#define SIGRTMIN	23	/* Realtime signal min */
#define SIGRTMAX	29	/* Realtime signal max */


#define _NSIGS		31

#ifndef _ASMLANGUAGE
/*
 * ANSI Args and returns from signal()
 */
#if defined(__STDC__) || defined(__cplusplus)

#define SIG_ERR         (void (*)(int))-1
#define SIG_DFL         (void (*)(int))0
#define SIG_IGN         (void (*)(int))1

#else

#define SIG_ERR         (void (*)())-1
#define SIG_DFL         (void (*)())0
#define SIG_IGN         (void (*)())1

#endif

/*
 * The sa_flags in struct sigaction
 */
#define SA_NOCLDSTOP	0x0001	/* Do not generate SIGCHLD when children stop */
#define SA_SIGINFO	0x0002	/* Pass additional siginfo structure */
#define SA_ONSTACK	0x0004	/* (Not posix) Run on sigstack */
#define SA_INTERRUPT	0x0008	/* (Not posix) Don't restart the function */
#define SA_RESETHAND	0x0010	/* (Not posix) Reset the handler, like sysV */

/*
 * The how in sigprocmask()
 */
#define SIG_BLOCK	1
#define SIG_UNBLOCK	2
#define SIG_SETMASK	3

/*
 * The si_code returned in siginfo
 */
#define SI_SYNC		0	/* (Not posix) gernerated by hardware */
#define SI_KILL		1	/* signal from .1 kill() function */
#define SI_QUEUE	2	/* signal from .4 sigqueue() function */
#define SI_TIMER	3	/* signal from expiration of a .4 timer */
#define SI_ASYNCIO	4	/* signal from completion of an async I/O */
#define SI_MESGQ	5	/* signal from arrival of a message */

typedef unsigned long sigset_t;
typedef unsigned char sig_atomic_t;

typedef struct siginfo
    {
    int			si_signo;
    int			si_code;
    union sigval	si_value;
    } siginfo_t;

struct sigaction
    {
    union
	{
#if defined(__STDC__) || defined(__cplusplus)
	void		(*__sa_handler)(int);
	void		(*__sa_sigaction)(int, siginfo_t *, void *);
#else
	void		(*__sa_handler)();
	void		(*__sa_sigaction)();
#endif /* __STDC__ */
	}		sa_u;
#define sa_handler	sa_u.__sa_handler
#define sa_sigaction	sa_u.__sa_sigaction
    sigset_t		sa_mask;
    int			sa_flags;
    };

#if defined(__STDC__) || defined(__cplusplus)

extern void 	(*signal(int __sig, void (*__handler)(int)))(int);
extern int      raise(int __signo);
extern int	kill(int __tid, int __signo);

extern int 	sigemptyset(sigset_t *__set);
extern int 	sigfillset(sigset_t *__set);
extern int 	sigaddset(sigset_t *__set, int __signo);
extern int 	sigdelset(sigset_t *__set, int __signo);
extern int 	sigismember(const sigset_t *__set, int __signo);
extern int 	sigaction(int __sig, const struct sigaction *__act,
			  struct sigaction *__oact);
extern int 	sigprocmask(int __how, const sigset_t *__set, sigset_t *__oset);
extern int 	sigpending(sigset_t *__set);
extern int 	sigsuspend(const sigset_t *__sigmask);
extern int 	sigwait(const sigset_t *__set);
extern int 	sigwaitinfo(const sigset_t *__set, struct siginfo *__value);
extern int 	sigtimedwait(const sigset_t *__set, struct siginfo *__value,
				const struct timespec *);
extern int	sigqueue (int tid, int signo, const union sigval value);

#else

extern void 	(*signal())();
extern int      raise();
extern int	kill();
extern int 	sigemptyset();
extern int 	sigfillset();
extern int 	sigaddset();
extern int 	sigdelset();
extern int 	sigismember();
extern int 	sigaction();
extern int 	sigprocmask();
extern int 	sigpending();
extern int 	sigsuspend();
extern int 	sigwait();
extern int 	sigwaitinfo();
extern int 	sigtimedwait();
extern int	sigqueue ();

#endif /* __STDC__ */


/*
 * From here to the end is not posix, it is for bsd compatibility.
 */
#define SV_ONSTACK	SA_ONSTACK
#define SV_INTERRUPT	SA_INTERRUPT
#define SV_RESETHAND	SA_RESETHAND

#define sigmask(m)	(1 << ((m)-1))
#define SIGMASK(m)	(1 << ((m)-1))

struct sigvec
    {
#if defined(__STDC__) || defined(__cplusplus)
    void (*sv_handler)(int);	/* signal handler */
#else
    void (*sv_handler)();	/* signal handler */
#endif /* __STDC__ */
    int sv_mask;		/* signal mask to apply */
    int sv_flags;		/* see signal options */
    };

struct sigcontext;

#if defined(__STDC__) || defined(__cplusplus)

extern int 	sigvec(int __sig, const struct sigvec *__vec,
		       struct sigvec *__ovec);
extern void 	sigreturn(struct sigcontext *__context);
extern int 	sigsetmask(int __mask);
extern int 	sigblock(int __mask);

#else

extern int 	sigvec();
extern void 	sigreturn();
extern int 	sigsetmask();
extern int 	sigblock();

#endif

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCsignalh */
