/*
 * ksys/uli.h
 *
 *      User Level Interrupt definitions
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */


#ifndef __KSYS_ULI_H
#define __KSYS_ULI_H
#ident "$Revision: 1.5 $"
#if _KERNEL
#include <sys/sema.h>
#include <sys/xlate.h>
#include <sys/uthread.h>
#include <sys/pcb.h>
#include <sys/reg.h>
#include <sys/uli.h>

#define MAX_ULIS 64

struct uli {
    /* Cache sensitive info. the following fields are in the
     * realtime critical path. The structure is allocated on
     * a cache line boundary.
     */
    label_t jmpbuf;	/* context save to return when done with ULI */

    /* pointers */
    caddr_t gp;		/* gp to run usercode */
    caddr_t sp;		/* ULI stack pointer to use */
    caddr_t pc;		/* ULI handler func */
    void *funcarg;	/* ULI handler argument */
    caddr_t saved_intstack;
    struct uthread_s *uli_saved_curuthread;
    struct uthread_s *uli_uthread; /* process thread to run to service uli */

    /* longs */
    k_machreg_t sr;	/* user status register */
#ifdef TFP
    k_machreg_t config;	/* user config register */
    k_machreg_t saved_config;	/* user config register */
#endif

    /* shorts */
    ushort_t saved_asids;	/* saved asids (tlbpid [, icachepid] */
    ushort_t new_asids;		/* new asids */

    /* chars */
    cpuid_t ulicpu;	/* cpu private tlbpid is allocated from */

    /* The following fields are not in the realtime critical path */
    struct uli *next, *prev; /* per thread linkage */
    int sig;		/* signal on return */
    int index;		/* index of this uli in global table */

    /* device dependent teardown func to disconnect interrupt */
    void (*teardown)(struct uli*);
    __psint_t teardownarg1;
    __psint_t teardownarg2;
    __psint_t teardownarg3;

    int uli_eframe_valid;
    eframe_t uli_eframe;

    int tstamp;		/* timestamp when ULI was called */

#ifdef EPRINTF
    caddr_t bunk;	/* to hold some tmp junk without recompiling */
#endif

    /* semaphores used to sleep waiting for ULI. This array is allocated
     * to the correct size for the desired number of semaphores and
     * hence must be the last member of the struct
     */
    int nsemas;
    struct ulisema {
	lock_t mutex;
	int count;
	sv_t sv;
    } sema[1];
};

/* size of the total ULI struct given the number of semaphores.
 */
#define ULI_SIZE(nsemas) ((size_t)&(((struct uli*)0)->sema[nsemas]))

/* mini eframe for ULI syscalls */
struct ULIeframe {
    k_machreg_t ef_argsave[4];

    k_machreg_t ef_return;
    k_machreg_t ef_a1;
    k_machreg_t ef_a2;
    k_machreg_t ef_a3;

    k_machreg_t ef_gp;
    k_machreg_t ef_sp;
    k_machreg_t ef_fp;
    k_machreg_t ef_ra;
    k_machreg_t ef_sr;
    k_machreg_t ef_epc;
#ifdef TFP
    k_machreg_t ef_config;
#endif
};

struct uliargs;
extern void	uli_return(int);
extern void	uli_init(void);
extern int	uli_register(struct uliargs *);
extern int	uli_unregister(int);
extern int	uli_callup(int);
extern void	uli_eret(struct uli*, k_machreg_t);
extern ushort_t	uli_getasids(void);
extern void	uli_setasids(ushort_t);
extern int	new_uli(struct uliargs *, struct uli **, int);
extern int	free_uli(struct uli *);
extern int	uli_index(struct uli *);
extern int	uli_sleep(int, int);
extern int	uli_wakeup_lock(int, int);
extern int	uli_wakeup(int, int);
extern int	vmeuli_change_ivec(int, int, int);
extern int	vmeuli_validate_vector(int, int, int);
extern void	uli_core(void);

#if _MIPS_SIM == _ABI64
extern int uliargs_to_irix5(void *, int, xlate_info_t *);
extern int irix5_to_uliargs(enum xlate_mode, void *, int, xlate_info_t *);
extern void uliargs32_to_uliargs(struct uliargs32 *, struct uliargs *);
extern void uliargs_to_uliargs32(struct uliargs *, struct uliargs32 *);
#endif

#endif /* _KERNEL */

#ifdef ULI_TSTAMP
#if _KERNEL
extern int uli_tstamps[];
extern int uli_saved_tstamps[];
extern void *ULI_register_ulits(int, void (*)(void*));
#endif

/* pre-allocate timestamp slots */
enum ulits_e {
    TS_TRIGGER,
    TS_GENEX,
    TS_EXCEPTION,
    TS_INTRNORM,
    TS_INTR,
    TS_DEVINTR,
    TS_CALLUP,
    TS_ERET,
    TS_FUI,
    TS_LOCALFUI,

    TS_MAX /* must be last */
};
#endif /* ULI_TSTAMP */

#endif /* __KSYS_ULI_H */
