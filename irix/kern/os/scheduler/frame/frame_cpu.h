/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __FRAME_CPU_H__
#define __FRAME_CPU_H__

#include <sys/groupintr.h>
#include <sys/frs.h>

#if defined(EVEREST)
#define FRS_MAX_CPUS EV_MAX_CPUS
#endif

#if defined(SN0)
#define FRS_MAX_CPUS MAXCPUS
#endif

#if defined(IP30)
#define FRS_MAX_CPUS MAXCPU
#endif

/*
 * Per cpu frame scheduler parent object.
 * Used to acquire a reference to the frame
 * scheduler managing a cpu for handling
 * scheduler interrupts and process dispatching.
 * One of these objects is allocated per cpu
 * at boot time.
 */

typedef struct frs_pda {
        lock_t		   lock;      /* protect resv and frs */
        int                resv;      /* temporarily reserve cpu */
        struct frs_fsched* frs;       /* frs object linked to this cpu */
        __uint64_t         basecc;    /* base counter for CC intr */
	int		   x[32];     /* cache line padding */
} frs_pda_t;

#define frs_cpu_pdalock(p)         mutex_spinlock(&(p)->lock)
#define frs_cpu_pdaunlock(p, s)    mutex_spinunlock(&(p)->lock, (s))

/*
 * Interrupt Source Descriptor
 */
typedef struct intrdesc {
	int           source;          /* Interrupt source */
	intrgroup_t*  intrgroup;       /* Interrupt Group */
	union { 
		uint period;           /* Initial Global Minor Frame Period */
		uint pipenum;          /* Graphic Pipe when intr is VSYNC */
		uint driverid;         /* Driver Id when intr is DRIVER */
		uint value;            /* generic value */
	} q;
} intrdesc_t;

#define iget_intrsource(idesc)   ((idesc)->source)
#define iget_intrgroup(idesc)    ((idesc)->intrgroup)
#define iget_period(idesc)       ((idesc)->q.period)
#define iget_pipenum(idesc)      ((idesc)->q.pipenum)
#define iget_driverid(idesc)     ((idesc)->q.driverid)

#if IP30
/*
 * Octane does not have interrupt groups, nor does it need them
 * since the FRS can only run on cpu 1.
 *
 * Instead, we fake them with the macros below.
 */
#define intrgroup_alloc()	(&fake_igroup)
#define intrgroup_join(i, c)	(*i = c)
#define intrgroup_unjoin(i, c)	(*i &= ~c)
#define intrgroup_free(i)	{}
#define intrgroup_get_cpubv(i)	1
#define intrgroup_get_groupid(i) 1

#define sendgroupintr(i)						\
{									\
	if (cpuid() == 0) {						\
		cpuaction(1, (cpuacfunc_t) frs_fsched_eventintr, A_NOW);\
	} else if (private.p_frs_flags)	{				\
		frs_fsched_eventintr();					\
	}								\
}
#endif /* IP30 */

#define curfrs() (((frs_pda_t*) private.p_frs_objp)->frs)

/*
 * Function Protos
 */

struct frs_fsched;

extern void frs_cpu_frspda_print(cpuid_t cpu);
extern int frs_cpu_verifyvalid(cpuid_t cpu);
extern int frs_cpu_verifynotclock(cpuid_t cpu);
extern int frs_cpu_reservepda(cpuid_t cpu);
extern void frs_cpu_unreservepda(cpuid_t cpu);
extern int frs_cpu_linkpda(cpuid_t cpu, struct frs_fsched* frs);
extern struct frs_fsched* frs_cpu_unlinkpda(cpuid_t cpu);
extern struct frs_fsched* frs_cpu_getfrs(cpuid_t cpu);
extern int frs_cpu_isolate(cpuid_t cpu);
extern int frs_cpu_unisolate(cpuid_t cpu);

extern void frs_cpu_resetcputimer(uint cc);
extern void frs_cpu_stopcputimer(void);
extern void frs_cpu_resetcctimer(__uint64_t new_time);
extern void frs_cpu_stopcctimer(void);
extern __uint64_t frs_cpu_getcctimer(void);
extern void frs_cpu_stopproftimer(void);
extern void frs_cpu_resched(void);

extern int frs_cpu_eventintr_validate(struct frs_fsched*,
				      frs_fsched_info_t*,
				      frs_intr_info_t*);
			       
extern void frs_cpu_eventintr_arm(struct frs_fsched*);
extern void frs_cpu_eventintr_fire(struct frs_fsched*);
extern void frs_cpu_eventintr_reset(cpuid_t cpu, intrdesc_t* idesc);
extern void frs_cpu_eventintr_clear(struct frs_fsched*);
extern void frs_cpu_eventintr_clearwait(struct frs_fsched*);
extern void frs_cpu_resetallintrs(cpuid_t cpu);

extern void frs_handle_termination(void);

#endif /* __FRAME_CPU_H__ */
