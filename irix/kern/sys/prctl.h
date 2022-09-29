/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * prctl.h - struct for private process data area and prctl and sproc
 */
#ifndef __PRCTL_H__
#define __PRCTL_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 3.71 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
/*
 * PRDA area - mapped privately into each process/thread
 *	Some of these fields may be modified by user code
 * 	to pass information into the kernel; other fields
 * 	may be read by user code to obtain current information
 * 	about the process. See schedctl(2), SETHINTS  for details.
 */
/* address of the process data area */
#define PRDA		((struct prda *)0x00200000L)

/* the system portion of the prda */
struct prda_sys {
	pid_t		t_pid;		/* epid */
	__uint32_t	t_hint;		/* Share group scheduling hint */
					/* see schedctl(2), SETHINTS   */
	__uint32_t	t_dlactseq;	/* Deadline scheduler activation seq# */
	__uint32_t	t_fpflags;	/* Current p_fpflags (SMM, precise) */
	__uint32_t	t_prid;		/* Processor type */
	__uint32_t	t_dlendseq;     /* Deadline sched allocation seq # */
	__uint64_t	t_unused1[5];
	pid_t		t_rpid;		/* pid */
	__int32_t	t_resched;	/* send pthread resched signal */
	__int32_t	t_syserror;	/* creation errors */

	/*
	 * Following fields used by pthread/nanothread libraries to manage
	 * the RSAs.
	 */
	__int32_t	t_nid;		/* Nanothread ID */
	__int32_t	t_affinity_nid;	/* Nanothread ID whose kernel
					   structures resize with this EV */

	__uint32_t	t_unused[5];
	
	/*
	 * The following field is used to hold the cpu number that the
	 * process is currently or last ran on. See schedctl(2), SETHINTS.
	 */
    	__uint32_t	t_cpu;		/* Last/current cpu */

	/*
	 * The following fields are used by sigprocmask if the hold mask is
	 * to be in user space.
	 */
	__uint32_t	t_flags;	/* t_hold mask is being used? */
	k_sigset_t	t_hold;		/* Hold signal bit mask */
};

struct prda {
	char unused[2048];	/* used as exit stack by pthreads */
				/* last 128 bytes used by pixie/rld */
	union {
		char fill[512];
		uint32_t rsvd[8];
	} sys2_prda;
	union {
		char fill[512];
	} lib2_prda;
	union {
		char fill[512];
	} usr2_prda;

	union {
		char fill[128];
		struct prda_sys prda_sys;
	} sys_prda;
	union {
		char fill[256];
	} lib_prda;
	union {
		char fill[128];
	} usr_prda;
};
#define t_sys		sys_prda.prda_sys

/* Following structures used in the PR_ATTACHADDERPERM version of prctl
 * which is intended mainly for use by MPI library.  First argument to
 * to prctl specifies PR_ATTTACHADDRPERM, second argument points to
 * prattach_args_t input parameter, third argument points to
 * prattach_results_t for return values.
 */

struct prattach_args_t {
	__int64_t	local_vaddr;	/* local address for attach or "-1" */
	__int64_t	vaddr;		/* address to be attached */
	pid_t		pid;		/* process ID to be attached */
	__uint32_t	prots;		/* R/W/X protections requested */
	__uint32_t	flags;
};

/* prattach_args_t flags values */
#define	PRATTACH_ALIGN	0x1		/* align mapping offsets */

#define	PRATTACH_MASK	(PRATTACH_ALIGN)	/* allowable flags */

struct prattach_results_t {
	__int64_t	local_vaddr;	/* corresponds to original vaddr */
	__int64_t	region_base;	/* base address of mapped region */
	__int64_t	region_size;	/* size of mapped region in byes */
	__uint32_t	prots;		/* protections granted */
};

/* Thread control structures.
 */
typedef struct prthread {
	void		(*prt_entry)(void *, size_t);
	caddr_t		prt_arg;
	unsigned int	prt_flags;
	caddr_t		prt_stkptr;
	size_t		prt_stklen;
} prthread_t;

typedef struct {
	int	prs_priority;
	int	prs_policy;
} prsched_t;


#ifndef _KERNEL

/* prototypes for process control functions */
#include <stddef.h>
#include <sys/types.h>
int blockproc(pid_t);
int unblockproc(pid_t);
int setblockproccnt(pid_t, int);
int blockprocall(pid_t);
int unblockprocall(pid_t);
int setblockproccntall(pid_t, int);
ptrdiff_t prctl(unsigned, ...);
pid_t sproc(void (*)(void *), unsigned, ...);
pid_t sprocsp(void (*)(void *, size_t), unsigned, void *, caddr_t, size_t);
pid_t
pidsprocsp(void (*)(void *, size_t), unsigned, void *, caddr_t, size_t, pid_t);
pid_t nsproc(void (*)(void *, size_t), unsigned, prthread_t *, prsched_t *);

#endif /* !_KERNEL */
#endif /* (_LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS) */

#if defined(_LANGUAGE_ASSEMBLY)
#define PRDA		0x00200000
#define T_PID		0xe00
#define T_HINT		0xe04
#endif

/* values for prctl */
#define PR_MAXPROCS	1	/* maximum # procs per user */
#define PR_ISBLOCKED	2	/* return if pid is blocked */
#define PR_SETSTACKSIZE 3	/* set max stack size */
#define PR_GETSTACKSIZE 4	/* get max stack size */
#define PR_MAXPPROCS	5	/* max parallel processes */
#define PR_UNBLKONEXEC	6	/* unblock pid on exec/exit */
#define PR_SETEXITSIG	8	/* signal to be sent to share group on exit */
#define PR_RESIDENT	9	/* set process immune to swapout */
#define PR_ATTACHADDR	10	/* re-attach a region */
#define PR_DETACHADDR	11	/* detach a region */
#define PR_TERMCHILD	12	/* terminate child proc when parent exits */
#define PR_GETSHMASK	13	/* retrieve share mask */
#define PR_GETNSHARE	14	/* return # of members of share group */
#define PR_COREPID	15	/* if process dumps core, add pid to name */
#define	PR_ATTACHADDRPERM 16	/* re-attach a region, specify permissions */
/* 17 was used for PR_PTHREADEXIT in 6.2, 6.3, and 6.4 ... */
#define PR_SETABORTSIG	18	/* signal to be sent to share group on abort */
/* 19 is and has always been unused */
#define PR_INIT_THREADS	20	/* prep process for multi-threaded operation */
#define PR_THREAD_CTL	21	/* RESERVED, multi-threaded operations */
#define PR_LASTSHEXIT	22	/* RESERVED, new in 6.5.3: last sproc exit */

#define PR_THREAD_EXIT	1	/* toss a process thread, without prejudice */
#define PR_THREAD_BLOCK	2	/* block calling thread */
#define PR_THREAD_UNBLOCK 3	/* unblock named thread */
#define PR_THREAD_KILL	4	/* send signal to specified thread */
#define PR_THREAD_SCHED	5	/* change scheduling attributes for thread */

/*
 * sproc(2) sharing options
 */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) || defined(_LANGUAGE_ASSEMBLY)
#define PR_SPROC	0x00000001	/* doing an sproc(2) call */
#define PR_SFDS		0x00000002	/* share file descriptors */
#define PR_SDIR		0x00000004	/* share current/root directory */
#define PR_SUMASK	0x00000008	/* share umask value */
#define PR_SULIMIT	0x00000010	/* share ulimit value */
#define PR_SID		0x00000020	/* share uid/gid values */
#define PR_SADDR	0x00000040	/* share virtual address space */
#define PR_SALL		0x0000007f	/* share all sproc(2) options */
#define PR_THREADS	0x00000080	/* multi-threaded process */

/* sproc(2) and nsproc(2) flags */
#define PR_FLAGMASK	0xff000000
#define PR_BLOCK	0x01000000	/* caller blocks on sproc */
#define PR_NOLIBC	0x02000000	/* do not start libc arena */
#define PR_EVENT	0x04000000	/* mt events */
#endif

#if defined(_LANGUAGE_FORTRAN)
#define PR_SPROC	'00000001'x	/* doing an sproc(2) call */
#define PR_SFDS		'00000002'x	/* share file descriptors */
#define PR_SDIR		'00000004'x	/* share current/root directory */
#define PR_SUMASK	'00000008'x	/* share umask value */
#define PR_SULIMIT	'00000010'x	/* share ulimit value */
#define PR_SID		'00000020'x	/* share uid/gid values */
#define PR_SADDR	'00000040'x	/* share virtual address space */
#define PR_SALL		'0000007f'x	/* share it all */
#define PR_THREADS	'00000080'x	/* multi-threaded process */

/* sproc(2) and nsproc(2) flags */
#define PR_FLAGMASK	'ff000000'x
#define PR_BLOCK	'01000000'x	/* caller blocks on sproc */
#define PR_NOLIBC	'02000000'x	/* do not start libc arena */
#endif

/* blockproc[all](2), unblockproc[all](2), setblockproccnt[all](2) limits */
#define PR_MAXBLOCKCNT	 10000
#define PR_MINBLOCKCNT	-10000

/*
 * Flags for t_flags
 */
#define T_HOLD_VALID	0x1		/* Set if t_hold contains valid mask */
#define	T_HOLD_KSIG_O32	0x2		/* k_sigset_t signal position */

#ifdef __cplusplus
}
#endif

#endif /* __PRCTL_H__ */
