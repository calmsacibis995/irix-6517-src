#ifndef __SYS_RUNQ_H__
#define __SYS_RUNQ_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.92 $"

/*
 * Externally visible routines.
 */

# ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kthread.h>
#include <sys/sema.h>
/* initialize the runq - only call once from the master processor */
extern void		makeRunq(void);

struct kthread;
struct uthread_s;
struct proc_sched_s;

extern void	putrunq(struct kthread *, cpuid_t);
extern struct kthread	*getrunq(void);
extern int 	removerunq(struct kthread *);
extern int 	idlerunq(void);
extern void	kt_startrun(struct kthread *);
extern void	ut_endrun(struct uthread_s *);
extern void	kt_endtslice(struct kthread *);

/* called by other processors to make themselves known to the runq */
extern void	joinrunq(void);

/* called during clock handling to maintain time-slice accounting */
extern void	tschkRunq(struct uthread_s *);

/* called from fork to propogate scheduling info */
extern void	forkrunq(struct uthread_s *, struct uthread_s *);

/* called to propogate uthread scheduling info */
extern void	uthreadrunq(struct kthread *, int, struct proc_sched_s *);

/* called from uthread_attach to propogate scheduling info */
struct job_s;
extern void	uthread_attachrunq(struct kthread *, cpuid_t, struct job_s *);

/* called from exit for cleaning up if needed */
extern void	exitrunq(struct kthread *);

/* called from shared address space de-allocation */
struct shaddr_s;
extern void	endshaddrRunq(struct shaddr_s *);

/* called when a process leaves the share group */
extern void 	leaveshaddrRunq(struct uthread_s *);

#ifdef MP
typedef struct cpu_cookie {
            int must_run;
            cpuid_t cpu;
} cpu_cookie_t;

extern cpu_cookie_t	setmustrun(cpuid_t);
extern void		restoremustrun(cpu_cookie_t);
#else
typedef cpuid_t cpu_cookie_t;
#define setmustrun(x)		0
#define restoremustrun(x)
#endif

/* how many processors are available to this process? */
extern int		runprunq(struct kthread *);

/* insure that the given process runs in the kernel to check for an event */
extern void		evpostRunq(struct kthread *);

/* let the scheduler know that a processor has been re-enabled */
extern void		cpu_unrestrict(cpuid_t);

extern micro_t		basictomicro(basictime_t);

/* convert kthread priority to schedctl() RTPRI priority */
extern int		kt_rtpri(struct kthread *);

/* called to set scheduling parameters for a process */
extern int		setinfoRunq(struct kthread *, struct proc_sched_s *,
				    int, ...);
#define RQRTPRI		2	/* set real-time process priority */
#define RQSLICE		3	/* set process time slice */
#define RQPSETNEW	8	/* create a new processor set */
#define RQPSETEND	9	/* delete an existing processor set */
#define RQPSETADD	10	/* add processors to an existing set */
#define RQPSETDEL	11	/* delete processors from an existing set */
#define RQPSETPROC	12	/* set the processor set for a process */
#define RQPSETUNPROC	13	/* clear the processor set for a process */

#define RQF_GFX	0x1

#ifdef UT_STOP
/*
 * Only needed when both putrunq and uthread are available
 */
static __inline void
unstop(uthread_t *ut)
{
	ut->ut_whywhatstop = 0;
	ut->ut_flags &= ~UT_STOP;
	putrunq(UT_TO_KT(ut), CPU_NONE);
}
#endif

# endif

#ifdef USE_PTHREAD_RSA
extern void swtch_rsa_resume(struct uthread_s *ut);
#endif

#endif /* __SYS_RUNQ_H__ */
