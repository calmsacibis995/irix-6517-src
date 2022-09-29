/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: hwperfmacros.h
 */

#ifndef __SYS_HWPERFMACROS_H__
#define __SYS_HWPERFMACROS_H__

#ident	"$Revision: 1.39 $"

/*
 * Some commands access the event counters via syssgi.
 * For entry via syssgi, there is only one syssgi value, 
 * and the actual operation on the counters is specified 
 * through the following sub-commands:
 */
/*
 * Most of this is non-R10k specific. This is being split up into machine
 * dependent and machine independent files.
 */

#include <sys/sbd.h>
#ifndef _KERNEL
#define IS_R12000() (0)
#endif
#define HWPERF_PROFENABLE	1	/* acquire counters for profiling */
#define HWPERF_ENSYSCNTRS	2	/* grab counters globally */
#define HWPERF_GET_CPUCNTRS	4	/* read cpu's private counters */
#define HWPERF_PROF_CTR_SUSP	5	/* suspend counters for caller... */
#define HWPERF_PROF_CTR_CONT	6	/* until continue request */
#define HWPERF_RELSYSCNTRS	8	/* release system counters */
#define HWPERF_GET_SYSCNTRS	16 	/* read all the counters together */
#define HWPERF_GET_SYSEVCTRL	32 	/* get global counter state */
#define HWPERF_SET_SYSEVCTRL	64 	/* set global counter state */

#if defined (SN0)
#define HWPERF_ERRCNT_ENABLE	256
#define HWPERF_ERRCNT_DISABLE	257
#define HWPERF_ERRCNT_GET	258

/* These codes are now all obsolete and are just here for compatibilty
   reasons */
#define MDPERF_ENABLE		259
#define MDPERF_DISABLE		260
#define MDPERF_GET_CTRL		261
#define MDPERF_GET_COUNT	262
#define MDPERF_GET_NODE_COUNT	263

/* These are the new md_perf control codes */
#define MDPERF_NODE_ENABLE	264
#define MDPERF_NODE_DISABLE	265
#define MDPERF_NODE_GET_CTRL	266
#define MDPERF_NODE_GET_COUNT	267

#define IOPERF_ENABLE		300
#define IOPERF_DISABLE		301
#define IOPERF_GET_COUNT	302
#define IOPERF_GET_CTRL		303

#endif

/*
 * To extract the appropriate fields given a specifier
 */

#define	PDA_IS_R12000		((private.p_cputype_word >> C0_IMPSHIFT) \
							== C0_IMP_R12000)

#define HWPERF_EVMASK 		0x1E0	/* Mask for the event bits */
#define HWPERF_MODEMASK  	0xF   	/* Mask for the count mode bits */
#define HWPERF_EVSHIFT 		5 

/* Event corresponding to the event specifier */
#define HWPERF_EVENTSEL(spec) (((spec) & HWPERF_EVMASK) >> HWPERF_EVSHIFT)

#define HWPERF_CONDMASK		0x1C00		/* conditional count mask */
#define HWPERF_CONDSHIFT	10
#define HWPERF_SPECIALMASK	0x1E000		/* Special bits at end of mask */
#define HWPERF_SPECIALSHIFT	13

/* Conditional count corresponding to conditional specifier */

#define HWPERF_CONDSEL(spec) (((spec) & HWPERF_CONDMASK) >> HWPERF_CONDSHIFT) 
#define HWPERF_ISCONDCOUNT(spec) (((((spec) & HWPERF_SPECIALMASK) >> HWPERF_SPECIALSHIFT) & 0x1)?1:0)
#define HWPERF_ISINVCONDCOUNT(spec) (((((spec) & HWPERF_SPECIALMASK) >> HWPERF_SPECIALSHIFT) & 0x2)?1:0)
#define HWPERF_ISONESHOT(spec) (((((spec) & HWPERF_SPECIALMASK) >> HWPERF_SPECIALSHIFT) & 0x4)?1:0)
#define HWPERF_ISL1L2(spec) (((((spec) & HWPERF_SPECIALMASK) >> HWPERF_SPECIALSHIFT) & 0x4)?1:0)
#define HWPERF_ISOPENTRIG(spec) (((((spec) & HWPERF_SPECIALMASK) >> HWPERF_SPECIALSHIFT) & 0x8)?1:0)

/* First 16 virtual counters correspond to the 
 * hardware counter 0.
 * Next 16 corr. to hardware counter 1
 */

#define HWPERF_CNT1BASE 16

#define HWPERF_CNTEN_U  0x8  /* user mode counting */
#define HWPERF_CNTEN_S  0x4  /* supervisor mode    */
#define HWPERF_CNTEN_K  0x2  /* kernel mode        */
#define HWPERF_CNTEN_E  0x1  /* exception level    */
#define HWPERF_CNTEN_A  0xA  /* all "real" modes   */
#define HWPERF_CNTEN_IE 0x10  /* enable overflow interrupt */

#define HWPERF_MINEVENT 0  /* Minimum event number for a counter */
#define HWPERF_MAXEVENT 15

#define HWPERF_OVERFLOW_THRESH	0x80000000

/*
 * Following are the various kinds of events that can be counted in the T5
 * performance counters.
 */

#define HWPERF_C0PRFCNT0_CYCLES   0  /* Cycles */
#define HWPERF_C0PRFCNT0_IINSTR   1  /* Issued instructions */
#define HWPERF_C0PRFCNT0_ILD      2  /* Issued loads */
#define HWPERF_C0PRFCNT0_IST      3  /* Issued stores */
#define HWPERF_C0PRFCNT0_ISC      4  /* Issued store conditionals */
#define HWPERF_C0PRFCNT0_FAILSC   5  /* Failed store conditionals */
#define HWPERF_C0PRFCNT0_DECBR    6  /* Decoded branches */
#define HWPERF_C0PRFCNT0_QWWSC    7  /* Quadwords written back to scache */
#define HWPERF_C0PRFCNT0_SCDAECC  8  /* Correctable scache data array errors */
#define HWPERF_C0PRFCNT0_PICMISS  9  /* Primary instruction cache misses */
#define HWPERF_C0PRFCNT0_SICMISS  10 /* Secondary instruction cache misses */
#define HWPERF_C0PRFCNT0_IMISPRED 11 /* Instruction misprediction from scache
					way predication table */
#define HWPERF_C0PRFCNT0_EXTINT   12 /* External interventions */
#define HWPERF_C0PRFCNT0_EXTINV   13 /* External invalidates */
#define HWPERF_C0PRFCNT0_VCOH     14 /* Virtual coherency conditon */
#define HWPERF_C0PRFCNT0_GINSTR   15 /* Graduated instructions */


#define HWPERF_C0PRFCNT1_CYCLES   0  /* Cycles */
#define HWPERF_C0PRFCNT1_GINSTR   1  /* Graduated instructions */
#define HWPERF_C0PRFCNT1_GLD      2  /* Graduated loads */
#define HWPERF_C0PRFCNT1_GST      3  /* Graduated stores */
#define HWPERF_C0PRFCNT1_GSC      4  /* Graduated store conditionals */
#define HWPERF_C0PRFCNT1_GFINSTR  5  /* Graduated floating point instructions */
#define HWPERF_C0PRFCNT1_QWWPC    6  /* Quadwords written back from primary
					data cache */
#define HWPERF_C0PRFCNT1_TLBMISS  7  /* Translation lookaside buffer misses */
#define HWPERF_C0PRFCNT1_BRMISS   8  /* Mispredicted branches */
#define HWPERF_C0PRFCNT1_PDCMISS  9  /* Primary data cache misses */
#define HWPERF_C0PRFCNT1_SDCMISS  10 /* Secondary data cache misses */
#define HWPERF_C0PRFCNT1_DMISPRED 11 /* Data misprediction from scache
					way predication table */
#define HWPERF_C0PRFCNT1_EXTINTHIT 12 /* External intervention hits in scache */
#define HWPERF_C0PRFCNT1_EXTINVHIT 13 /* External invalidate hits in scache */
#define HWPERF_C0PRFCNT1_SPEXCLEAN 14 /* Store/prefetch exclusive to clean
					 block in secondary cache */
#define HWPERF_C0PRFCNT1_SPEXSHR   15 /* Store/prefetch exclusive to shared
					 block in secondary cache */

/* 
 * Following are the various kinds of events tat can be counted in the R12000
 * performance counters.
 */

#define HWPERF_PRFCNT_CYCLES	0	/* Cycles */
#define HWPERF_PRFCNT_DINSTR	1	/* Decoded intsructions */
#define HWPERF_PRFCNT_DLD	2	/* Decoded loads */
#define HWPERF_PRFCNT_DST	3	/* Decoded stores */
#define HWPERF_PRFCNT_MTHO	4	/* Miss Handling Table occupancy */
#define HWPERF_PRFCNT_FAILSC	5	/* Failed store conditionals */
#define HWPERF_PRFCNT_RESCB	6	/* Resolved conditional branches */
#define HWPERF_PRFCNT_QWWSC	7	/* Quadwords written back to scache */
#define HWPERF_PRFCNT_SCDAECC	8	/* Correctable scache data array errors */  
#define HWPERF_PRFCNT_PICMISS	9	/* Primary instruction cache misses */           
#define HWPERF_PRFCNT_SICMISS	10	/* Secondary instruction cache misses */         
#define HWPERF_PRFCNT_IMISPRED	11	/* Instruction misprediction from scache
					   way predication table */
#define HWPERF_PRFCNT_EXTINT	12	/* External interventions */
#define HWPERF_PRFCNT_EXTINV	13	/* External invalidates */
#define HWPERF_PRFCNT_ALUFPUFP	14	/* ALU/FPU forward progress */
#define HWPERF_PRFCNT_GINSTR	15	/* Graduated instructions */
#define HWPERF_PRFCNT_EXPREI	16	/* Executed Prefetch Instructions */
#define HWPERF_PRFCNT_PDCMISSPI	17	/* Primary data cache misses by prefetch
					   instructions */
#define HWPERF_PRFCNT_GLD	18	/* Graduated loads */
#define HWPERF_PRFCNT_GST	19	/* Graduated stores */ 
#define HWPERF_PRFCNT_GSC	20	/* Graduated store conditionals */
#define HWPERF_PRFCNT_GFINSTR	21	/* Graduated floating point instructions */
#define HWPERF_PRFCNT_QWWPC	22	/* Quadwords written back from primary
					   data cache */ 
#define HWPERF_PRFCNT_TLBMISS	23	/* Translation lookaside buffer misses */
#define HWPERF_PRFCNT_BRMISS	24	/* Mispredicted branches */
#define HWPERF_PRFCNT_PDCMISS	25	/* Primary data cache misses */
#define HWPERF_PRFCNT_SDCMISS	26	/* Secondary data cache misses */
#define HWPERF_PRFCNT_DMISPRED	27	/* Data misprediction from scache
					   way predication table */
#define HWPERF_PRFCNT_EXTINTHIT	28	/* State of External intervention hits in scache */ 
#define HWPERF_PRFCNT_EXTINVHIT 29	/* State of External invalidate hits in scache */ 
#define HWPERF_PRFCNT_NHTAM	30	/* Miss Handling Table entries accessing memory */
#define HWPERF_PRFCNT_SPEX	31	/* Store/prefetch exclusive to
					   block in secondary cache */ 
					   
					   
					   
#if defined (_KERNEL) 

#ifdef R10000

#if defined (SN0)
#define MULTIPLEX_MDPERF_COUNTERS()					\
	if (private.p_mdperf)	 					\
		md_perf_multiplex(cnodeid());

#define MULTIPLEX_IOPERF_COUNTERS()					\
        if (private.p_ioperf)						\
	        io_perf_multiplex(cnodeid());

#else	/* !SN0 */
#define MULTIPLEX_MDPERF_COUNTERS()	
#define MULTIPLEX_IOPERF_COUNTERS()					
#endif	/* SN0 */

#define MULTIPLEX_HWPERF_COUNTERS()    					\
	if (private.p_active_cpu_mon) 					\
		hwperf_multiplex_events(private.p_active_cpu_mon);	\
	MULTIPLEX_MDPERF_COUNTERS();					\
	MULTIPLEX_IOPERF_COUNTERS()

#define	START_HWPERF_COUNTERS(_ut)					\
	if ((_ut)->ut_cpumon &&						\
	    (_ut)->ut_cpumon->cm_flags & HWPERF_CM_ENABLED)		\
		hwperf_setup_counters((_ut)->ut_cpumon)

#define	SUSPEND_HWPERF_COUNTERS(_ut)					\
	if ((_ut)->ut_cpumon &&						\
	    (_ut)->ut_cpumon == private.p_active_cpu_mon) {		\
		r1nk_perf_stop_counters((_ut)->ut_cpumon);		\
		r1nk_perf_update_counters((_ut)->ut_cpumon);		\
		private.p_active_cpu_mon = NULL;			\
	}
#define	STOP_HWPERF_COUNTERS(_ut)					\
	if ((_ut)->ut_cpumon->cm_flags & HWPERF_CM_EN)			\
		hwperf_disable_counters((_ut)->ut_cpumon)

#define	ADD_HWPERF_COUNTERS(_source_mon, _targ_mon)			\
	if ((_source_mon)->cm_flags & HWPERF_CM_VALID)			\
		hwperf_add_counters(_source_mon, _targ_mon)

#define	USING_HWPERF_COUNTERS(_p)					\
	((_p)->p_proxy.prxy_cpumon)

#define HWPERF_FORK_COUNTERS(_pt, _ct)					\
	if ((_pt)->ut_cpumon)						\
		hwperf_fork_counters(_pt, _ct)

#else /* !R10000 */

#define	SUSPEND_HWPERF_COUNTERS(_cmp)
#define	START_HWPERF_COUNTERS(_cmp)
#define	STOP_HWPERF_COUNTERS(_cmp)
#define MULTIPLEX_HWPERF_COUNTERS()
#define USING_HWPERF_COUNTERS(_pp)	0
#define HWPERF_FORK_COUNTERS(_pp, _cp)	
#endif /* !R10000 */

/*
 * 	machine dependant functions	(ml/r10kperf.c)
 * 	===========================
 */
#ifndef __SYS_REG_H__
#include <sys/reg.h>		/* declaration for cpu_mon_t */
#endif

void r1nk_perf_start_counters    (cpu_mon_t *);
void r1nk_perf_update_counters   (cpu_mon_t *);
int  r1nk_perf_init_counters     (cpu_mon_t *,hwperf_profevctrarg_t *,
                                             hwperf_profevctraux_t *);
void r1nk_perf_stop_counters     (cpu_mon_t *);
void r1nk_perf_intr              (eframe_t *);
void r1nk_perf_intr_action       (eframe_t *, cpu_mon_t *, int);


struct proc;
struct uthread_s;
struct eframe_s;
extern void hwperf_init(void);
extern void hwperf_intr(struct eframe_s *);
extern int  hwperf_enable_counters(cpu_mon_t *, hwperf_profevctrarg_t *,
				   hwperf_profevctraux_t *, int, int *);
extern int  hwperf_disable_counters(cpu_mon_t *);
extern int  hwperf_change_sys_control(hwperf_profevctrarg_t *, int, int *);
extern int  hwperf_change_control(cpu_mon_t *, 
				  hwperf_profevctrarg_t *, int, int *);
extern int  hwperf_sys_control_info(hwperf_eventctrl_t *,
				    hwperf_eventctrlaux_t *, int *);
extern int  hwperf_control_info(cpu_mon_t *, hwperf_eventctrl_t *,
				hwperf_eventctrlaux_t *, int *);
extern int  hwperf_get_sys_counters(hwperf_cntr_t *, int *);
extern int  hwperf_get_counters(cpu_mon_t *, hwperf_cntr_t *, int *);
extern int  hwperf_get_cpu_counters(int, hwperf_cntr_t *, int *);
extern void hwperf_plus_counters(cpu_mon_t *, hwperf_cntr_t *, int *);

extern void hwperf_fork_counters(struct uthread_s *, struct uthread_s *);
extern void hwperf_add_counters(struct cpu_mon *, struct cpu_mon *);
extern int  hwperf_parent_accumulates(cpu_mon_t *, int *);
extern void hwperf_multiplex_events(cpu_mon_t *);

extern int  hwperf_enable_sys_counters(hwperf_profevctrarg_t *,
				       hwperf_profevctraux_t *, int, int *);
extern int  hwperf_disable_sys_counters(void);
extern int  hwperf_get_access(cpu_mon_t *, int);
extern void hwperf_check_release(cpu_mon_t *);
extern void hwperf_release_access(cpu_mon_t *);
extern void hwperf_read_counters(cpu_mon_t *);
extern void hwperf_proc_counters(void);
extern void hwperf_initiate_startup(cpu_mon_t *);
extern void hwperf_setup_counters(cpu_mon_t *cmp);
extern void hwperf_copy_cmp(cpu_mon_t *, cpu_mon_t *);
extern int  hwperf_arch_nosupport(void);
extern void hwperf_arch_fatal(void);
extern int  hwperf_overflowed_intr(void);

extern cpu_mon_t *cpumon_alloc(struct uthread_s *);
extern void hwperf_cpu_monitor_info_free(cpu_mon_t *);


#if defined (R10000)


/* Interface routines to read and write the hardware
 * performance monitor dat & control registers.
 */
extern int  r1nkperf_data_register_get   (int);
extern int  r1nkperf_control_register_get(int);
extern void r1nkperf_data_register_set   (int,int);
extern void r1nkperf_control_register_set(int,int);
extern int  r10k_perf_overflow_intr	 (void);              
extern int  r12k_perf_overflow_intr	 (void);              

#else
#define hwperf_overflowed_intr()	0
#endif /* R10000*/
#endif /* _KERNEL */

#endif /* __SYS_HWPERFMACROS_H__ */
