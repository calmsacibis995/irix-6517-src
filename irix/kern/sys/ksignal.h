/* Copyright (C) 1989-1992 Silicon Graphics, Inc. All rights reserved.  */

#ifndef _SYS_KSIGNAL_H
#define _SYS_KSIGNAL_H

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.69 $" 

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include <sys/ktypes.h>
#include <sys/procset.h>
#include <sys/signal.h>
#include <sys/siginfo.h>

/*
 * This header contains internal kernel signal handling data structures
 * Do not use __mips macro here
 * Do not use machreg_t, k_machreg_t for interface definitions
 * Use ONLY definitions that are constant whether compiled mips2 or 3
 * Also includes prototypes and ucontext.h info for kernel
 */
/* sigaction interface */

/*
 * IRIX5/SVR4 ABI sigaction
 */
typedef struct irix5_sigaction {
  app32_int_t sa_flags;		/* see below for values                 */
  app32_ptr_t k_sa_handler;	/* SIG_DFL, SIG_IGN, BSD_SIG_IGN or *fn */	
  /* sigset_t is constant size */
  sigset_t sa_mask;		/* additional set of sigs to be         */
				/* blocked during handler execution     */
  app32_int_t sa_resv[2];
} irix5_sigaction_t;

/*
 * IRIX5 64 sigaction
 */
typedef struct irix5_64_sigaction {
  app64_int_t sa_flags;		/* see below for values                 */
  app64_ptr_t k_sa_handler;	/* SIG_DFL, SIG_IGN, BSD_SIG_IGN or *fn */	
  /* sigset_t is constant size */
  sigset_t sa_mask;		/* additional set of sigs to be         */
				/* blocked during handler execution     */
  app64_int_t sa_resv[2];
} irix5_64_sigaction_t;

typedef struct k_sigaction {
  void (*k_sa_handler)();		/* SIG_DFL, SIG_IGN, or *fn */
  k_sigset_t sa_mask;		/* additional set of sigs to be		*/
				/* blocked during handler execution	*/
  __int32_t sa_flags;		/* see sys/signal.h			*/
} k_sigaction_t;

#if	(_MIPS_SIM != _ABIO32)	

/* Convert signal number to bit mask representation */
#define sigmask(n)	((__uint64_t)1 << (n-1))

/* sigoutok - old (32 bit mask) to k_sigset_t */
#define sigktoou(ks,i)         ((i) = *(ks))
#define sigoutok(i,ks)         (*(ks) = (i))

#define sigutok(us,ks) \
		(*(ks) = ((k_sigset_t)(us)->__sigbits[1] << 32)	\
		        | ((k_sigset_t)(us)->__sigbits[0]));

#define sigktou(ks,us)  \
		((us)->__sigbits[0] = (__uint32_t)(*(ks)),	\
		 (us)->__sigbits[1] = (__uint32_t)(*(ks) >> 32),\
		 (us)->__sigbits[2] = 0,			\
		 (us)->__sigbits[3] = 0);

#else	/* (_MIPS_SIM == _ABIO32) */

#define MAXBITNO (32)
#define sigword(n) (((uint)(n)-1)/MAXBITNO)
/* bit mask position within a word, to be used with sigword() or word 0 */
#define sigmask(n) (1<<(((n)-1)%MAXBITNO))

/* sigoutok - old (32 bit mask) to k_sigset_t */
#define sigktoou(ks,i)         ((i) = (ks)->sigbits[0])
#define sigoutok(i,ks)         ((ks)->sigbits[0] = i,  \
                                 (ks)->sigbits[1] = 0)

#define sigutok(us,ks) \
                ((ks)->sigbits[0] = (us)->__sigbits[0],  \
                 (ks)->sigbits[1] = (us)->__sigbits[1]);

#define sigktou(ks,us)  \
                ((us)->__sigbits[0] = (ks)->sigbits[0],   \
                 (us)->__sigbits[1] = (ks)->sigbits[1],   \
                 (us)->__sigbits[2] = 0,  \
                 (us)->__sigbits[3] = 0);

#endif	/* (_MIPS_SIM != _ABIO32) */

#if defined(_KERNEL)
extern k_sigset_t	stopdefault,
			jobcontrol,
			ignoredefault,
			cantmask,
			cantreset,
			fillset,
			emptyset;

extern struct zone *sigqueue_zone;
#define sigqueue_alloc(f)	kmem_zone_alloc(sigqueue_zone, f)
#define sigqueue_free(s)	kmem_zone_free(sigqueue_zone, s)

enum idtype;
struct irix5_ucontext;
struct irix5_sigcontext;
struct irix5_siginfo;
struct irix5_64_siginfo;
struct irix5_64_ucontext;
struct irix5_n32_ucontext;
struct k_siginfo;
struct proc;
struct uthread_s;
struct rusage;
union rval;

#if	(_MIPS_SIM != _ABIO32)	

#define sigemptyset(s)		((void)(*(s) = 0))
#define sigfillset(s)		((void)(*(s) = fillset))
#define sigaddset(s,i)		((void)(*(s) |= sigmask(i)))
#define sigbitaddset(s,b,n)	((void)(*(s) |= (b)))
#define sigdelset(s,i)		((void)(*(s) &= ~sigmask(i)))
#define sigbitdelset(s,b,n)	((void)(*(s) &= ~(b)))
#define sigismember(s,i)	(*(s) & sigmask(i))
#define sigbitismember(s,b,n)	(*(s) & (b))
#define sigsetismember(s1,s2)	(*(s1) & *(s2))
#define sigisempty(s)		(*(s) == 0)
#define sigorset(s1,s2)		((void)(*(s1) |= *(s2)))
#define sigandset(s1,s2)	((void)(*(s1) &= *(s2)))
#define sigdiffset(s1,s2)	((void)(*(s1) &= ~(*(s2))))

#else	/* (_MIPS_SIM != _ABIO32) */

#define sigemptyset(s)  ((void)((s)->sigbits[0] = (s)->sigbits[1] = 0))
#define sigfillset(s)   ((void)(*(s) = fillset))
#define sigaddset(s,i)  ((void)((s)->sigbits[sigword(i)] |= sigmask(i)))
#define sigdelset(s,i)  ((void)((s)->sigbits[sigword(i)] &= ~sigmask(i)))
#define sigismember(s,i) ((s)->sigbits[sigword(i)] & sigmask(i))
#define sigsetismember(s1,s2) (((s1)->sigbits[0] & (s2)->sigbits[0]) | \
                               ((s1)->sigbits[1] & (s2)->sigbits[1]))
#define sigisempty(s)   (((s)->sigbits[0] | (s)->sigbits[1]) == 0)

#define sigorset(s1,s2) ((void)(((s1)->sigbits[0] |= (s2)->sigbits[0]), \
                                ((s1)->sigbits[1] |= (s2)->sigbits[1])))
#define sigandset(s1,s2) ((void)(((s1)->sigbits[0] &= (s2)->sigbits[0]), \
                                ((s1)->sigbits[1] &= (s2)->sigbits[1])))
#define sigdiffset(s1,s2) ((void)(((s1)->sigbits[0] &= ~(s2)->sigbits[0]), \
                                  ((s1)->sigbits[1] &= ~(s2)->sigbits[1])))
#define sigbitaddset(s,b,n)	sigaddset((s),(n))
#define sigbitdelset(s,b,n)	sigdelset((s),(n))
#define sigbitismember(s,b,n)	sigismember((s),(n))

#endif	/* (_MIPS_SIM != _ABIO32) */

struct k_siginfo;
struct uthread_s;
struct sigqueue;
struct sigvec_s;

extern void sigqfree(struct sigqueue **);
extern int sigaddq(struct sigqueue **, struct sigqueue *, struct sigvec_s *);
extern void sigdelq(struct sigqueue **, int, struct sigvec_s *);
extern void sigsetcur(struct uthread_s *, int, struct k_siginfo *, int);
extern int sigispend(struct proc *, int);
extern int sigisready(void);

extern int waitsys(enum idtype, id_t, struct k_siginfo *, int, struct rusage *,
		   union rval *);
extern int write_utext(void *, u_int);

extern int irix5_restorecontext(struct irix5_ucontext *);
extern int irix5_64_restorecontext(struct irix5_64_ucontext *);
extern int irix5_n32_restorecontext(struct irix5_n32_ucontext *);
extern void irix5_savecontext(struct irix5_ucontext *, k_sigset_t *);
extern void irix5_n32_savecontext(struct irix5_n32_ucontext *, k_sigset_t *);
extern void irix5_siginfoktou(struct k_siginfo *, struct irix5_siginfo *);
extern void irix5_64_siginfoktou(struct k_siginfo *, struct irix5_64_siginfo *);
extern void irix5_64_savecontext(struct irix5_64_ucontext *, k_sigset_t *);

extern int sigdelete(struct uthread_s *, void *);

#ifdef CKPT
#if _MIPS_SIM == _ABI64
extern int irix5_64_siginfoutok(struct irix5_64_siginfo *, struct k_siginfo *);
#else
extern int irix5_siginfoutok(struct irix5_siginfo *, struct k_siginfo *);
#endif
#endif /* CKPT */
#endif /* _KERNEL */

/*
 * The IRIX5 version
 * sigcontext is not part of the ABI - so this version is used to
 * handle 32 and 64 bit applications
 */
typedef struct irix5_sigcontext {
	__uint32_t	sc_regmask;	/* regs to restore in sigcleanup */
	__uint32_t	sc_status;	/* status register */
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
	__uint64_t	sc_triggersave;	/* OBSOLETE! */
	/* sigset_t is constant size */
	sigset_t	sc_sigset;	/* signal mask to restore */
	__uint64_t	sc_fp_rounded_result;	/* for Ieee 754 support */

	/* sc_link is similar to the uc_link field of ucontext_t.
	 * We link together a chain of ucontext_t & sigcontext_t entries
	 * from the prxy->prxy_oldcontext field when a sequence of signals
	 * occurs.  ucontext_t entries are used when the process specifies
	 * SA_SIGINFO, otherwise sigcontext_t is used.
	 */
	__uint64_t	sc_link;
	__uint64_t	sc_pad[30];
} irix5_sigcontext_t;

/*
 * Siginfo interfaces
 */

/*
 * IRIX5/SVR4 ABI version
 */

typedef union irix5_sigval {
	app32_int_t	sival_int;
	app32_ptr_t	sival_ptr;
} irix5_sigval_t;

typedef struct irix5_siginfo {
	app32_int_t	si_signo;		/* signal from signal.h	*/
	app32_int_t 	si_code;		/* code from above	*/
	app32_int_t	si_errno;		/* error from errno.h	*/
	union {

		app32_int_t	si_pad[SI_PAD];	/* for future growth	*/

		struct {			/* kill(), SIGCLD	*/
			irix5_pid_t	__pid;	/* process ID		*/
			union {
				struct {
					irix5_uid_t	__uid;
				} __kill;
				struct {
					irix5_clock_t __utime;
					app32_int_t __status;
					irix5_clock_t __stime;
					app32_int_t __swap;
				} __cld;
			} __pdata;
		} __proc;			

		struct {	/* SIGSEGV, SIGBUS, SIGILL and SIGFPE	*/
			app32_ptr_t	__addr;	/* faulting address	*/
		} __fault;

		struct {			/* SIGPOLL, SIGXFSZ	*/
		/* fd not currently available for SIGPOLL */
			app32_int_t	__fd;	/* file descriptor	*/
			app32_long_t	__band;
		} __file;

		union irix5_sigval __value;	/* Posix 1003.1b signals */

	} __data;

} irix5_siginfo_t;

/*
 * IRIX5_64 version
 */
typedef union irix5_64_sigval {
	app64_int_t	sival_int;
	app64_ptr_t	sival_ptr;
} irix5_64_sigval_t;

typedef struct irix5_64_siginfo {
	app64_int_t	si_signo;		/* signal from signal.h	*/
	app64_int_t 	si_code;		/* code from above	*/
	app64_int_t	si_errno;		/* error from errno.h	*/
	union {

		app64_int_t	si_pad[SI_PAD];	/* for future growth	*/

		struct {			/* kill(), SIGCLD	*/
			irix5_64_pid_t	__pid;	/* process ID		*/
			union {
				struct {
					irix5_64_uid_t	__uid;
				} __kill;
				struct {
					irix5_64_clock_t __utime;
					app64_int_t __status;
					irix5_64_clock_t __stime;
					app64_int_t __swap;
				} __cld;
			} __pdata;
		} __proc;			

		struct {	/* SIGSEGV, SIGBUS, SIGILL and SIGFPE	*/
			app64_ptr_t	__addr;	/* faulting address	*/
		} __fault;

		struct {			/* SIGPOLL, SIGXFSZ	*/
		/* fd not currently available for SIGPOLL */
			app64_int_t	__fd;	/* file descriptor	*/
			app64_long_t	__band;
		} __file;

		union irix5_64_sigval __value; /* Posix 1003.1b signals */

	} __data;

} irix5_64_siginfo_t;

/*
 * internal version is identical to siginfo_t but without the padding.
 * This must be maintained in sync with it.
 */
typedef struct k_siginfo {
	__int32_t	si_signo;		/* signal from signal.h	*/
	__int32_t 	si_code;		/* code from above	*/
	__int32_t	si_errno;		/* error from errno.h	*/
	union {
		struct {			/* kill(), SIGCLD	*/
			pid_t	__pid;		/* process ID		*/
			union {
				struct {
					uid_t	__uid;
				} __kill;
				struct {
					clock_t __utime;
					__int32_t __status;
					clock_t __stime;
					__int32_t __swap;
				} __cld;
			} __pdata;
		} __proc;			

		struct {	/* SIGSEGV, SIGBUS, SIGILL and SIGFPE	*/
			void *__addr;	/* faulting address	*/
		} __fault;

		struct {			/* SIGPOLL, SIGXFSZ	*/
		/* fd not currently available for SIGPOLL */
			__int32_t	__fd;	/* file descriptor	*/
			__int64_t	__band;
		} __file;

		union sigval __value;		/* Posix 1003.1b signals */

	} __data;
} k_siginfo_t;

/*
 * Structure used in BSD sigstack call.
 */
/* IRIX5 ABI version */
typedef struct irix5_sigstack {
	app32_ptr_t	ss_sp;		/* signal stack pointer */
	app32_int_t	ss_onstack;	/* current status */
} irix5_sigstack_t;

/* sigaltstack interface */
/*
 * IRIX5/SVR4 ABI version
 */
typedef struct irix5_sigaltstack {
	app32_ptr_t	ss_sp;	/* faulting address	*/
	app32_int_t	ss_size;
	app32_int_t	ss_flags;
} irix5_stack_t;

/*
 * IRIX5_64 ABI version
 */
typedef struct irix5_64_sigaltstack {
	app64_ptr_t	ss_sp;	/* faulting address	*/
	app64_int_t	ss_size;
	app64_int_t	ss_flags;
} irix5_64_stack_t;

typedef struct sigqueue {
	struct sigqueue		*sq_next;
	struct k_siginfo	sq_info;
} sigqueue_t;

typedef struct sigpend {
	k_sigset_t	s_sig;		/* signals pending to this thread */
	struct sigqueue	*s_sigqueue;	/* queued siginfo structures */
} sigpend_t;

#endif /* _LANGUAGE_C */

#define SIGNO_MASK	0xff
#define SIGDEFER	0x100
#define SIGHOLD		0x200
#define SIGRELSE	0x400
#define SIGIGNORE	0x800
#define SIGPAUSE	0x1000

#endif /* _SYS_KSIGNAL_H_ */
