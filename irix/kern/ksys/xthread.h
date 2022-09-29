/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident	"$Revision: 1.18 $"

#ifndef	__XTHREAD_H__
#define	__XTHREAD_H__

#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/resource.h>

/*
 * XTHREAD_LATENCY measures the thread latency overhead for interrupts.
 *
 */

#ifdef ITHREAD_LATENCY
/*
 * Interrupt Threads
 * Currently, just xthreads.  This is where we'll do int specific stuff.
 */
typedef struct intr_latstats {
	__uint64_t	ls_sum;
	__uint64_t	ls_cnt;
	uint		ls_istamp;	/* interrupt timestamp */
	uint		ls_min;
	uint		ls_max;
	uint		ls_10us[10];
	uint		ls_100us[10];
	uint		ls_long;
} xt_latency_t;

#endif /* ITHREAD_LATENCY */

/*
 * Interrupt Threads
 * These can run most low level kernel code:
 * - device drivers
 * - file system code
 * - networking
 * but they are optimized for use by interrupt handlers
 *
 * Locking:
 *	kt_lock(&xt->xt_ev) is used when putting xthread on runq
 *
 * They are always statically bound to a kthread
 *
 * xthreads support being scheduled on a processor set. If need
 * to be run on one particular processor, use setmustrun.
 * Both pset and mustrun are supported by the underlying kthread.
 */
#define KT_TO_XT(kt)	(ASSERT(KT_ISXTHREAD(kt)), (xthread_t *)(kt))
#define XT_TO_KT(xt)	((kthread_t *)(xt))

#define IT_NAMELEN	16

typedef void (xt_func_t)(void *, void *);

typedef struct xthread_s {
	struct kthread xt_ev;		/* execution vehicle */
	xt_func_t *xt_func;		/* interrupt handler function */
	void *xt_arg;			/* argument to interrupt function */
	void *xt_arg2;			/* argument to interrupt function */
	caddr_t xt_sp;			/* stack pointer for restart */
	char xt_name[IT_NAMELEN];	/* name */
	struct cred *xt_cred;		/* running credential */
#if	CELL
	void	*xt_info;		/* info about the intr being handled */
	uint64_t xt_mesg_ticks;		/* ktimer accum at mesg boundry */
#endif /* CELL */
	struct xthread_s *xt_next;	/* list of xthreads */
	struct xthread_s *xt_prev;	/* list of xthreads */
#ifdef ITHREAD_LATENCY
	xt_latency_t *xt_latstats;
#endif /* ITHREAD_LATENCY */
} xthread_t;

#define XT_TO_KT(xt)	((kthread_t *)(xt))

/* values for xthread_create flags argument */
	/* None currently defined */

/* This data structure gets appended to each of the hardware dependent
 * interrupt vector structures.  It gives the extra info needed to
 * register/unregister interrupt threads, and to execute them.
 */
typedef struct thd_int_s {
	int		thd_flags;	/* State info */
	sema_t		thd_isync;	/* Sync xthread to event */
	xthread_t	*thd_xthread;	/* Execution vehicle for interrupt */
	short		thd_pri;	/* Interrupt thread priority */
#ifdef ITHREAD_LATENCY /* Don't want extra indirection through thd_xthread. */
	xt_latency_t	*thd_latstats;	/* Latency statistics */
#endif /* ITHREAD_LATENCY */
} thd_int_t;

/* Definitions for thd_flags fields. */
#define THD_HWDEP	0x000000ff	/* Bits for each hw type to use */
#define THD_ISTHREAD	0x00000100	/* This interrupt is threaded */
#define THD_REG		0x00000200	/* Interrupt has been registered */
#define THD_INIT	0x00000400	/* Ithread has initialized itself */
#define THD_EXIST	0x00000800	/* An xthread exists for this int */
#define THD_OK		0x00001000	/* Okay to run as thread */
#define THD_EXITING	0x00002000	/* Old thread is exiting */

typedef void (xpt_func_t)(void *);

/* Info structure for xthread periodic timeouts */
typedef struct xpt {
	thd_int_t	tip;
	xpt_func_t	*xpt_func;
	void *		xpt_arg;
	__int64_t	xpt_period;
	__int64_t	xpt_time;
	cpuid_t		xpt_timeout_cpu;
	cpuid_t		xpt_thread_cpu;
} xpt_t;
#define xpt_flags	tip.thd_flags
#define xpt_isync	tip.thd_isync
#define xpt_xthread	tip.thd_xthread

/*
 * interface functions
 */
extern void xthread_exit(void);
extern void *xthread_create(char *, caddr_t, uint_t, 
		uint_t, uint_t, uint_t, xt_func_t, void *);
extern void xthread_setup(char *,int, thd_int_t *, xt_func_t *, void *);
extern void xthread_set_func(xthread_t *, xt_func_t *, void *);
#if defined(CLOCK_CTIME_IS_ABSOLUTE)
extern void xthread_periodic_timeout(char *, int, __int64_t, cpuid_t,
						xpt_t *, xpt_func_t *, void *);
extern void xthread_periodic_timeout_stop(xpt_t *);
#endif /* CLOCK_CTIME_IS_ABSOLUTE */

#ifdef ITHREAD_LATENCY
extern void xthread_zero_latstats(xt_latency_t *);
extern void xthread_print_latstats(xt_latency_t *);
extern void xthread_update_latstats(xt_latency_t *);
extern void xthread_set_istamp(xt_latency_t *);
extern xt_latency_t *xthread_get_latstats(xthread_t *);
#endif /* ITHREAD_LATENCY */

/* context switch routines */
extern void xtresume(xthread_t *, kthread_t *);
extern void restartxthread(kthread_t *, caddr_t, kthread_t *);
extern void loopxthread(kthread_t *, caddr_t);
extern void xthread_prologue(kthread_t *);
extern void xthread_loop_prologue(kthread_t *);
extern void xthread_epilogue(kthread_t *);
extern void xtdestroy(xthread_t *);

extern xthread_t *icvsema(sema_t *, int pri, xt_func_t *, void *, void *);

#endif /* __XTHREAD_H__ */
