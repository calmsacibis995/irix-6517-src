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
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>

#ident	"$Revision: 3.144 $"

/*
 * NOTE: this file is NOT included in strict ANSI mode anymore.
 */

/*
 * Signal Numbers. THESE ARE REPLICATED in signal.h
 */
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
#define SIGUME  35	/* Uncorrectable memory error */
/* Signals defined for Posix 1003.1c. */
#define SIGPTINTR	47
#define SIGPTRESCHED	48
/* Posix 1003.1b signals */
#define SIGRTMIN	49	/* Posix 1003.1b signals */
#define SIGRTMAX	64	/* Posix 1003.1b signals */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include <sys/types.h>
/*
 * Basic defines and types. Ok for ANSI
 */
#if defined(_LANGUAGE_C_PLUS_PLUS) || !_SGIAPI
#define __sigargs	int
#else
#define __sigargs	
#endif /* _LANGUAGE_C_PLUS_PLUS) || !_SGIAPI */

/*
 * Almost everyone has gone to a void return type. This lets
 * old programs define this and get the old 'int' behviour
 */
#ifndef __sigret_t
#define	__sigret_t	void
#endif

#ifndef SIG_ERR
/* might have been defined in signal.h */
typedef __sigret_t	(*SIG_PF) (__sigargs);

#define SIG_ERR		((SIG_PF)-1L)
#define	SIG_IGN		((SIG_PF)1L)
#define	SIG_DFL		((SIG_PF)0L)
#endif
#define SIG_HOLD	((SIG_PF)2L)

#if _POSIX93 || _ABIAPI || _XOPEN5
/*
 * POSIX 1003.1b extensions
 */
#include <sys/sigevent.h>
#endif /* _POSIX93 || _ABIAPI || _XOPEN5 */

#if _POSIX93 || _XOPEN4UX || _XOPEN5
/*
 * POSIX 1003.1b / XPG4-UX extension for signal handler arguments
 */
#include <sys/siginfo.h>
#endif /* _POSIX93 || _XOPEN4UX || _XOPEN5 */

/*
 * POSIX90 added types
 */
#if !defined(_SIGSET_T)
#define _SIGSET_T
typedef struct {                /* signal set type */
        __uint32_t __sigbits[4];
} sigset_t;
#endif

/* these aren't technically in POSIX90, but are permitted.. */
typedef union __sighandler {
    	__sigret_t (*sa_handler1)(__sigargs); /* SIG_DFL, SIG_IGN, or *fn */
#if _POSIX93 || _XOPEN4UX
    	void (*sa_sigaction1)(int, siginfo_t *, void *);
#endif
} __sighandler_t;
#if _SGIAPI
#define __sa_handler	sa_handler1
#define __sa_sigaction	sa_sigaction1
#endif

typedef struct sigaction {
	int sa_flags;			/* see below for values		*/
    	__sighandler_t sa_sighandler;	/* function to handle signal */
	sigset_t sa_mask;		/* additional set of sigs to be	*/
					/* blocked during handler execution */
	int sa_resv[2];
} sigaction_t;
/*
 * Posix defined two types of handlers. sa_handler is the default type
 * for use by programs that are not requesting SA_SIGINFO.  Programs
 * requesting SA_SIGINFO need to use a handler of type sa_sigaction.
 */
#define sa_handler	sa_sighandler.sa_handler1
#define sa_sigaction	sa_sighandler.sa_sigaction1

/*
 * Definitions for the "how" parameter to sigprocmask():
 *
 * The parameter specifies whether the bits in the incoming mask are to be
 * added to the presently-active set for the process, removed from the set,
 * or replace the active set.
 */
#define SIG_NOP		0	/* Not using 0 will catch some user errors. */
#define SIG_BLOCK	1
#define SIG_UNBLOCK	2	
#define SIG_SETMASK	3
#define SIG_SETMASK32	256	/* SGI added so that BSD sigsetmask won't 
				   affect the upper 32 sigal set */

/* definitions for the sa_flags field */
/*
 * IRIX5/SVR4 ABI definitions
 */
#define SA_ONSTACK	0x00000001	/* handle this signal on sigstack */
#define SA_RESETHAND    0x00000002	/* reset handler */
#define SA_RESTART      0x00000004	/* restart interrupted system call */
#define SA_SIGINFO      0x00000008	/* provide siginfo to handler */
#define SA_NODEFER      0x00000010	/* do not block current signal */
/* The next 2 are only meaningful for SIGCHLD */
#define SA_NOCLDWAIT    0x00010000	/* don't save zombie children */
#define SA_NOCLDSTOP	0x00020000	/* if set don't send SIGCLD	*/
					/* to parent when child stop	*/

/* IRIX5 additions */
#define _SA_BSDCALL	0x10000000	/* don't scan for dead children when */
					/* setting SIGCHLD */
					/* SJCTRL bit in proc struct.	*/

#if _XOPEN4UX || _XOPEN5 || defined(_BSD_COMPAT) || defined(_BSD_SIGNALS)
/*
 * Structure used in BSD sigstack call and in X/Open XPG4.
 */
struct sigstack {
	void	*ss_sp;			/* signal stack pointer */
	int	ss_onstack;		/* current status */
};

/* sigaltstack info */
#define MINSIGSTKSZ	512
#define SIGSTKSZ	8192

/*
 * stack_t is now defined in sys/ucontext.h so that includers of
 * sys/ucontext.h always get the definition regardless of compilation mode
 */

/* defines for ss_flags */
#define SS_ONSTACK	0x00000001
#define SS_DISABLE	0x00000002

#include <sys/ucontext.h>
#endif	/* _XOPEN4UX */

#if defined(_BSD_COMPAT) || defined(_BSD_SIGNALS)
/*
 * The next section contains declarations and typedefs for the BSD signal
 * library routines built on top of the POSIX system calls.  Two of them,
 * signal() and sigpause(), share names with their SysV counterparts, yet
 * have different semantics.  By default, the SysV versions are used.
 * In order to use the BSD versions, you may either:
 *  1) explicitly call BSDsignal() and BSDsigpause() in the program, or
 *  2) set one of the _BSD_SIGNALS or _BSD_COMPAT cpp constants before
 *     including the signal header file. There are 2 methods:
 *     a) modify the source file, e.g.,
 *	    #ifdef sgi
 *          #define _BSD_SIGNALS
 *          #endif
 *          #include <signal.h>
 *     b) use the cc(1) option -D_BSD_SIGNALS or -D_BSD_COMPAT.
 *        e.g., cc -D_BSD_SIGNALS foo.c -o foo
 * Note that _BSD_COMPAT affects other header files that deal with BSD
 * compatibility.
 */

struct sigvec {
  __sigret_t (*sv_handler)(__sigargs);	/* SIG_DFL, SIG_IGN, or *fn */
  int sv_mask;		/* mask to use during handler execution	*/
  int sv_flags;		/* see signal options below */
};


#define SV_ONSTACK	0x0001
#define SV_INTERRUPT	0x0002		/* not supported */
#define sv_onstack	sv_flags	/* compatibility with BSD */

#define NUMBSDSIGS	(32)  /* can't be expanded */
/* Convert signal number to bit mask representation */
#define sigmask(sig)	(1L << ((sig)-1))

#define signal		BSDsignal
#define sigpause	BSDsigpause

extern int	sigpause(int);
extern int	sigvec(int,struct sigvec *, struct sigvec *);
extern int	sigstack(struct sigstack *, struct sigstack *);
extern int	sigblock(int);
extern int	sigsetmask(int);
extern int	killpg(pid_t, int);
extern int	kill(pid_t, int);
#endif /* BSD */

#ifndef _KERNEL
#if _XOPEN4UX && (!defined(_BSD_COMPAT) && !defined(_BSD_SIGNALS))
/*
 * XPG4-UX adds a few BSD things
 */
#define sigmask(sig)	(1L << ((sig)-1))
extern void	(*bsd_signal(int, void (*)(int)))(int);
extern int	killpg(pid_t, int);
extern int	sigstack(struct sigstack *, struct sigstack *);

#endif	/* _XOPEN4UX */
#endif	/* !_KERNEL */

#ifndef _KERNEL
extern __sigret_t	(*signal(int,__sigret_t (*)(__sigargs)))(__sigargs);
#endif

#if _SGIAPI
/*
 * Information pushed on stack when a signal is delivered. This is used by
 * the kernel to restore state following execution of the signal handler.
 * It is also made available to the handler to allow it to properly restore
 * state if a non-standard exit is performed.
 *
 * sc_regmask is examined by the kernel when doing sigreturn()'s
 * and indicates which registers to restore from sc_regs
 * bit 0 == 1 indicates that all coprocessor state should be restored
 *	for each coprocessor that has been used
 * bits 1 - 31 == 1 indicate registers 1 to 31 should be restored by
 *	sigcleanup from sc_regs.
 */

/*
 * The IRIX5 version
 * sigcontext is not part of the ABI - so this version is used to
 * handle 32 and 64 bit applications - it is a constant size regardless
 * of compilation mode, and always returns 64 bit register values
 */
typedef struct sigcontext {
	__uint32_t	sc_regmask;	/* regs to restore in sigcleanup */
	__uint32_t	sc_status;	/* cp0 status register */
	__uint64_t	sc_pc;		/* pc at time of signal */
	/*
	 * General purpose registers
	 */
	__uint64_t	sc_regs[32];	/* processor regs 0 to 31 */
	/*
	 * Floating point coprocessor state
	 */
	__uint64_t	sc_fpregs[32];	/* fp regs 0 to 31 */
	__uint32_t	sc_ownedfp;	/* fp has been used */
	__uint32_t	sc_fpc_csr;	/* fpu control and status reg */
	__uint32_t	sc_fpc_eir;	/* fpu exception instruction reg */
					/* implementation/revision */
	__uint32_t	sc_ssflags;	/* signal stack state to restore */
	__uint64_t	sc_mdhi;	/* Multiplier hi and low regs */
	__uint64_t	sc_mdlo;
	/*
	 * System coprocessor registers at time of signal
	 */
	__uint64_t	sc_cause;	/* cp0 cause register */
	__uint64_t	sc_badvaddr;	/* cp0 bad virtual address */
	__uint64_t	sc_triggersave;	/* state of graphics trigger (SGI) */
	sigset_t	sc_sigset;	/* signal mask to restore */
	__uint64_t	sc_fp_rounded_result;	/* for Ieee 754 support */
	__uint64_t	sc_pad[31];
} sigcontext_t;

/* manifest to use to determine whether cause register contains the
 * branch delay bit. Some processors (R8K) have the bit in bit 63 while
 * most of the others have it in bit 31
 */
#define SC_CAUSE_BD	0x8000000080000000LL
 

#if !defined(_KERNEL) && !defined(_KMEMUSER)
/* minor compatibility - sc_mask is the first 32 bits (for BSD sigsetmask) */
#define sc_mask	sc_sigset.__sigbits[0]
#endif

#ifdef _LINT
#undef SIG_ERR
#define SIG_ERR (void(*)())0
#undef SIG_IGN
#define	SIG_IGN	(void (*)())0
#undef SIG_HOLD
#define	SIG_HOLD (void (*)())0
#endif /* _LINT */

#endif /* _SGIAPI */

#else /* !(_LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS) */

/* Define these for ASSEMBLY and FORTRAN */
#define SIG_ERR         (-1)
#define SIG_IGN         (1)
#define SIG_HOLD        (2)
#define SIG_DFL         (0)

#endif /* !(_LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS) */

#if _SGIAPI || _ABIAPI
#define NSIG            65      /* valid signal numbers are from 1 to NSIG-1 */
#define MAXSIG		(NSIG-1)    /* actual # of signals */
#define	NUMSIGS		(NSIG-1)    /* for POSIX array sizes, true # of sigs */

#define	BRK_USERBP	0	/* user bp (used by debuggers) */
#define	BRK_KERNELBP	1	/* kernel bp (used by prom) */
#define	BRK_ABORT	2	/* abort(3) uses to cause SIGIOT */
#define	BRK_BD_TAKEN	3	/* for taken bd emulation */
#define	BRK_BD_NOTTAKEN	4	/* for not taken bd emulation */
#define	BRK_SSTEPBP	5	/* user bp (used by debuggers) */
#define	BRK_OVERFLOW	6	/* overflow check */
#define	BRK_DIVZERO	7	/* divide by zero check */
#define	BRK_RANGE	8	/* range error check */

#define BRK_PSEUDO_OP_BIT 0x80
#define BRK_PSEUDO_OP_MAX 0x3	/* number of pseudo-ops */

#define BRK_CACHE_SYNC	0x80	/* synchronize icache with dcache */

#define	BRK_MULOVF	1023	/* multiply overflow detected */
#endif /* _SGIAPI || _ABIAPI */

#ifdef __cplusplus
}
#endif

#endif /* !_SYS_SIGNAL_H */
