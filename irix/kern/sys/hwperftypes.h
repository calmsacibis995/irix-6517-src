/*
 * Copyright 1995 Silicon Graphics, Inc.
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

#ifndef __SYS_HWPERFTYPES_H__
#define __SYS_HWPERFTYPES_H__

#ident	"$Revision: 1.23 $"

/*
 * Type definitions for performance monitoring
 */
#include <sys/time.h>

#if defined (SN0)
#include <sys/SN/SN0/hubmd.h>
#include <sys/SN/SN0/hubio.h>
#if defined (_KERNEL)
#include <sys/SN/agent.h>
#endif
#endif /* SN0 */

/*
 * Most of this is R10k specific. This is being split up into machine
 * dependent and machine independent files.
 */
#define HWPERF_EVENTMAX		32	/* Max. number of events that can be
					 * counted together in the counters
					 */

#define	HWPERF_COUNTMAX		 4	/* maximum number of physical
					 * counters
					 */
struct hwperf_ctrlreg {
	ushort_t	hwp_ev  :11,	/* event to be counted */
			hwp_ie	:1,	/* overflow interrupt enable */
			hwp_mode:4;	/* user/kernel mode */
};

/*
 * R10000 performance monitoring
 */
#if defined (_KERNEL)
typedef struct cpu_mon {


	short		cm_flags;
	__uint32_t	cm_mixed_cpu_flags;
	__uint32_t	cm_mixed_cpu_started;	/* CPU architecture we       */
						/* started on		     */
	short		cm_sig;	             	/* signal to                 */
						/* deliver upon              */ 
						/* cntr overflow             */
	pid_t		cm_pgid;		/* process group this counter*/
						/* belongs to. used to signal*/
						/* CPU architecture change   */
	short		cm_perfcnt_arch_swtch_sig;
	cpuid_t		cm_counting_cpu;		
	int		cm_gen;			/* generation nr for         */
						/* specifiers                */
	mutex_t		cm_mutex;

	/*
	 * 	Counters
	 * 	========
	 */
	short		cm_num_counters;
	__uint64_t	cm_event_mask[HWPERF_COUNTMAX];
	short		cm_events    [HWPERF_COUNTMAX];	   /* # of events    */
							   /* monitor by cntr*/ 
	short		cm_evindx    [HWPERF_COUNTMAX];	   /* current events */
						           /* for counters   */
	short		cm_evmax     [HWPERF_COUNTMAX];	   /* max events for */
							   /* counters       */

	/*
	 *	Events
	 *	======
	 */	
	short		cm_num_events;			   /* total number   */
							   /* of events      */
	ushort 		cm_evspec    [HWPERF_EVENTMAX];    /* specifiers for */
							   /* counter        */
	uint		cm_preloadcnt[HWPERF_EVENTMAX];    /* preload values */
	uint		cm_savecnt   [HWPERF_EVENTMAX];    /* saved values   */
	__uint64_t 	cm_eventcnt  [HWPERF_EVENTMAX];    /* perf counters  */
} cpu_mon_t;

/*
 * there are three different HW performance monitor implementations
 * as of today
 *
 *	R10K Rev 2.X
 *	R10K Rev 3
 *	R12K Rev 2.2
 *
 * The following table lists the differences  :
 
Ev
  | cnt  R10000 Rev 2.x             R10000 Rev 3  | cnt       R12000
  |                                               |
--+-----------------------------------------------+-----------------------
0 |  0   Cycles                     same          | 0-3   same
  |                                               |
1 |  0   Issued instructions        same, except: | 0-3   Decoded Instructions
  |      sum of :                                 |       decoded on previous
  |      - int ops marked done                    |       cycle.
  |        in active list                         |
  |      - FP ops issued to an      - FP ops markd|
  |        FPU                        done an     |
  |                                   active list |
  |      - Load/store issued        - includes    |  
  |        to addr calculation        prefetch    |
  |        on prev cycle. not                     |
  |        including prefetch                     |
--+-----------------------------------------------+-------------------------
2 |  0   Issued loads               same, except: | 0-3   Decoded Loads
  |                                 includes      |       excluding pre-fetch
  |                                 prefetch      |       cache-op and sync
  |                                               |       instructions
  |                                               |
3 |  0   Issued stores              same          | 0-3   Decoded Stores
--+-----------------------------------------------+-------------------------
4 |  0   Isssued store              same          | 0-3   Missing Handle Table
  |      conditional                              |       Occupancy
  |                                               |
5 |  0   Failed store               same          | 0-3   same
  |           conditional                         |
--+-----------------------------------------------+-------------------------
6 |  0   Decoded branches           Predicted and | 0-3   same as R10000 Rev3,
  |      includes conditional       mispredicted  |       but handling multiple
  |      and unconditional          branches      |       FP cond braches
  |      branches, correctly        (branch       |       correctly
  |      and incorrectly               resolved)  |
  |      predicted branches                       |
  |      branch might be killed                   |
  |      due to exception or                      |
  |      prior misprediction                      |
  |                                multiple cond  |
  |                                cond branches  |
  |                                on FP code     |
  |                                counted in 1   |
  |                                cycle          |
  |                                               |
7 |  0  quadwords written          same           | 0-3   same
  |     back from L2 cache                        |
--+-----------------------------------------------+-------------------------
  |                                               |
8 |  0  Correctable L2 cache       same           | 0-3   same
  |     data array ECC error                      |
  |                                               |
9 |  0  Primay instruction         same           | 0-3   same
  |     cache misses                              |
--+-----------------------------------------------+-------------------------
  |                                               |
10|  0  Secondary instruction      same           | 0-3   same, except :
  |     cache misses                              |       counter is incremented
  |                                               |       after re-fill req is
  |                                               |       send to sys interface
  |                                               |
11|  0  Instructions               same           | 0-3   same
  |     mispredicted from                         |                
  |     L2 cache way table                        |
--+-----------------------------------------------+-------------------------
12|  0  External interventions     same           | 0-3   same
  |     except invalidate                         |
  |                                               |
13|  0  External invalidations     same           | 0-3   same
--+-----------------------------------------------+-------------------------
14|  0  Virtual coherency          Instructions   | 0-3  same as on R10000
  |     condition                  done on :      |      Rev 3
  |     leftover from R4000        - ALU1/2       |
  |     no use                     - FPU1/2       |
  |                                               |
15|  0  Graduated instructions     same           | 0-3  same
--+-----------------------------------------------+-------------------------
16|  1  Cycles                     same           | 0-3  Executed prefetch
  |     (same as event 0)                         |      instructions
  |                                               |
17|  1  Graduated instructions     same           | 0-3  Primary data cache
  |     (same as event 15)                        |      misses by prefetch
  |                                               |      instructions
--+-----------------------------------------------+-------------------------
18|  1  Grad load/prefetch/sync    same, execpt : | 0-3  same as R10000 Rev 3
  |     cache-op instructions      all graduated  |
  |     if a store graduates       loads are      |
  |     on a given cycle, all      counted        |
  |     loads, prefetches etc                     |
  |     which graduate on that                    |
  |     cycle are not counted                     |
  |                                               |
19|  1  Graduated stores           same           | 0-3  same
--+-----------------------------------------------+-------------------------
20|  1  Graduated store            same           | 0-3  same
  |      conditional                              |
  |                                               |
21|  1  Graduated FP               same           | 0-3  same
  |        instrcutions
--+-----------------------------------------------+-------------------------
22|  1  Quadwords written back     same           | 0-3  same
  |     from primary data cache                   |
  |                                               |
23|  1  TLB misses                 same           | 0-3  same
--+-----------------------------------------------+-------------------------
24|  1  Mispredicted                              | 0-3  same
  |     branches                                  |
  |                                               |
25|  1  Primary data cache         same           | 0-3  same
  |     misses                                    |
--+-----------------------------------------------+-------------------------
26|  1  Secondary data cache       same           | 0-3  same, except :
  |     misses                                    |      counter incremented
  |                                               |      after refill req is
  |                                               |      send to sys interface
  |                                               |
27|  1  Data misprediction         same           | 0-3
  |     from L2 cache way                         |
  |     predicition table                         |
--+-----------------------------------------------+-------------------------
28|  1  External intervention      same           | 0-3  State of external
  |     hits in L2 cache                          |      intervention hit in
  |                                               |      scache
  |                                               |      requires special
  |                                               |      setting, not currently
  |                                               |      supported by SW
  |                                               |
29|  1  External invalidation      same           | 0-3  State of external
  |     hits in L2 cache                          |      intervention hit in
  |                                               |      scache in conjunction
  |                                               |      with event 28
--+-----------------------------------------------+-------------------------
30|  1  Store/prefetch excl        same           | 0-3  same
  |     to clean block in scache                  |      
  |                                               |
31|  1  Store/prefetch excl        same           | 0-3  same
  |     to shared block in scache


*/

#define	HWPERF_CPU_R10K_REV2X		0x0001
#define	HWPERF_CPU_R10K_REV3		0x0002
#define HWPERF_CPU_R12K_REV22		0x0004

/* Some useful macros on the event mask in the cpu monitoring info */
#define IS_HWPERF_EVENT_MASK_SET(_mask,_e)	(_mask & (1ull << _e))

#define HWPERF_FREE		0x0001
#define HWPERF_RSVD		0x0002
#define HWPERF_PROC		0x0004
#define HWPERF_SYS		0x0008
#define HWPERF_PROFILING	0x0010

#define HWPERF_MAXWAIT		50000000	/* 50 mils */

/* flags for cm_flags */

#define HWPERF_CM_PROC		0x0001
#define HWPERF_CM_SYS		0x0002
#define HWPERF_CM_CPU		0x0004
#define HWPERF_CM_ENABLED	0x0008
#define HWPERF_CM_EN		0x000f

#define HWPERF_CM_VALID		0x0010
#define HWPERF_CM_PSAVE		0x0020
#define HWPERF_CM_LOST		0x0040
#define HWPERF_CM_PROFILING	0x0080 	/* using counters for profiling */
#define HWPERF_CM_SUSPENDED	0x0100

typedef struct perf_mon {
	cpu_mon_t *pm_cpu_mon;    /* cpu performance monitoring counters */
} perf_mon_t;

struct eframe_s;
struct hwperf_profevctrarg;
struct hwperf_profevctraux;

#endif /* _KERNEL */

/*
 * Several of these structures include one another. Hopefully, we can 
 * change the interface at some point, and clean this up.
 */
/*
 * structure used to retrieve the counts 
 */
typedef struct {
	__uint64_t hwp_evctr[HWPERF_EVENTMAX];
} hwperf_cntr_t;


/*
 * The following is used to set/retrieve the information
 * on which events are being tracked.
 */

typedef union {
	short			hwperf_spec;
	struct hwperf_ctrlreg	hwperf_creg;
} hwperf_ctrl_t;

typedef struct {
	hwperf_ctrl_t 	hwp_evctrl[HWPERF_EVENTMAX];
} hwperf_eventctrl_t;

typedef struct hwperf_profevctrarg {
	hwperf_eventctrl_t	hwp_evctrargs;
	int			hwp_ovflw_freq[HWPERF_EVENTMAX];
	int			hwp_ovflw_sig; /* SIGUSR1,2 */
} hwperf_profevctrarg_t;

typedef struct hwperf_profevctraux {
	int			hwp_aux_gen;
	hwperf_cntr_t		hwp_aux_cntr;
} hwperf_profevctraux_t;

typedef struct hwperf_eventctrlaux {
	int			hwp_aux_sig;
	int			hwp_aux_freq[HWPERF_EVENTMAX];
} hwperf_eventctrlaux_t;
	
typedef struct hwperf_profevctrargex {
	hwperf_profevctrarg_t	*hwp_args;
	hwperf_profevctraux_t	*hwp_aux;
} hwperf_profevctrargex_t;

typedef struct hwperf_getctrlex_arg {
	hwperf_eventctrl_t	*hwp_evctrl;
	hwperf_eventctrlaux_t	*hwp_evctrlaux;
} hwperf_getctrlex_arg_t;

#if defined (SN0)
/* The MD interface section. */


/* sets */
#define MD_PERF_MEM_CYCLES       0       /* memory activity breakdown */
#define MD_PERF_OUTMSG_CLASS     1       /* outgoing message classification */
#define MD_PERF_INVAL_CLASS      2       /* outgoing intervention/invalidation */
#define MD_PERF_INMSG_CLASS      3       /* incoming message classification */
#define MD_PERF_DIR_STATE        4       /* directory state for read classif. */
#define MD_PERF_LOCAL_REQ        5       /* requests by local processors */

/* mpc_select bits */
#define MD_PERF_SEL_ALL          ((1 << MD_PERF_SETS) - 1)
#define MD_PERF_SEL_MEM_CYCLES   (1 << MD_PERF_MEM_CYCLES)
#define MD_PERF_SEL_OUTMSG_CLASS (1 << MD_PERF_OUTMSG_CLASS)
#define MD_PERF_SEL_INVAL_CLASS  (1 << MD_PERF_INVAL_CLASS)
#define MD_PERF_SEL_INMSG_CLASS  (1 << MD_PERF_INMSG_CLASS)
#define MD_PERF_SEL_DIR_STATE    (1 << MD_PERF_DIR_STATE)
#define MD_PERF_SEL_LOCAL_REQ    (1 << MD_PERF_LOCAL_REQ)

typedef struct md_perf_control {
  uint ctrl_bits;
} md_perf_control_t;

typedef struct md_perf_reg {
    __uint64_t  mpr_overflow : 1,       /* did the counter overflow */
                mpr_value    :63;       /* cumulative value */
} md_perf_reg_t;

/* Return value of syssgi get_count interface */
typedef struct md_perf_values {
    md_perf_reg_t mpv_count[MD_PERF_SETS][MD_PERF_COUNTERS];
    __int64_t    mpv_timestamp[MD_PERF_SETS];
} md_perf_values_t;

/* set 0 counters -- memory */
#define MD_PERF_S0_IDLE          0       /* idle cycles */
#define MD_PERF_S0_BLOCKED       1       /* blocked (no output queue space) */
#define MD_PERF_S0_REFRESH       2       /* refresh cycles counter */
#define MD_PERF_S0_DIRECTORY     3       /* directory-only busy cycles */
#define MD_PERF_S0_MEMORY        4       /* memory/directory busy cycles */
#define MD_PERF_S0_MISC          5       /* miscellaneous busy cycles */


/* set 1 counters -- outgoing messages */
#define MD_PERF_S1_II            0       /* intervention/invalidate counter */
#define MD_PERF_S1_BOFF_II       1       /* backoff intervention/invalidate */
#define MD_PERF_S1_WBACK_ACK     2       /* writeback acknowledgement counter */
#define MD_PERF_S1_XSU_RESP      3       /* exclusive/shared/uncached response */
#define MD_PERF_S1_NACK          4       /* negative acknowledgement counter */
#define MD_PERF_S1_OTHER         5       /* other outgoing replies */

/* set 2 counters -- outgoing intervention/invalidation */
#define MD_PERF_S2_INVAL         0       /* invalidation counter */
#define MD_PERF_S2_IV_REMOVE     1       /* intervention remove counter */
#define MD_PERF_S2_IV_RXUNC      2       /* interv. read-exclusive uncached */
#define MD_PERF_S2_IV_RSUNC      3       /* interv. read-shared uncached ctr. */
#define MD_PERF_S2_IV_RXCL       4       /* interv. read-exclusive counter */
#define MD_PERF_S2_IV_RSHD       5       /* interv. read-shared counter */

/* set 3 counters -- incoming messages */
#define MD_PERF_S3_MISC          0       /* incoming misc. requests counter */
#define MD_PERF_S3_READ          1       /* incoming read requests counter */
#define MD_PERF_S3_WRITE         2       /* incoming write replies counter */
#define MD_PERF_S3_REVISION      3       /* incoming revision replies counter */
#define MD_PERF_S3_FOP_HITS      4       /* fetch-and-op cache hits counter */
#define MD_PERF_S3_FOP_MISSES    5       /* fetch-and-op cache miss counter */

/* set 4 counters -- directory state */
#define MD_PERF_S4_UNOWNED       0       /* unowned counter */
#define MD_PERF_S4_POISONED      1       /* poisoned counter */
#define MD_PERF_S4_SHARED        2       /* shared counter */
#define MD_PERF_S4_XCL_REQ       3       /* exclusive by requestor counter */
#define MD_PERF_S4_XCL_OTHER     4       /* exclusive by other counter */
#define MD_PERF_S4_BUSY          5       /* busy */

/* set 5 counters -- local requests */
#define MD_PERF_S5_CPU0_READS    0       /* reads by processor 0 */
#define MD_PERF_S5_CPU0_WBACKS   1       /* writebacks by processor 0 */
#define MD_PERF_S5_CPU1_READS    2       /* reads by processor 1 */
#define MD_PERF_S5_CPU1_WBACKS   3       /* writebacks by processor 1 */
#define MD_PERF_S5_IO_READS      4       /* reads by IO */
#define MD_PERF_S5_IO_WRITES     5       /* write invalidates by IO */


/* THe II Interface section */

/* The bits for II tests */

/* IPPRO_C selections */


#define IO_PERF_MICRO_TO_LLP                    0
#define IO_PERF_DATA_DWORDS_FROM_LLP            1
#define IO_PERF_DWORDS_TO_LEGONET               2
#define IO_PERF_DATA_DWORDS_FROM_LEGONET        3
#define IO_PERF_11_TO_14_CRBS_USED              4
#define IO_PERF_5_OR_LESS_CRBS_USED             5
#define IO_PERF_3_PARTIAL_CACHE_USED            6
#define IO_PERF_1_PARTIAL_CACHE_USED            7
#define IO_PERF_RETIRED_AFTER_CRBS              8
#define IO_PERF_EJECTED_CACHE_ENTRY     	9
#define IO_PERF_CRB_ENTRY_ALLOCATED             10

/* IPPRC_1 */
#define IO_PERF_PACKETS_FROM_LLP         	16
#define IO_PERF_DATA_PACKETS_TO_LLP      	17
#define IO_PERF_DWORDS_FROM_LEGONET      	18
#define IO_PERF_DATA_DWORDS_TO_LEGONET   	19
#define IO_PERF_CLOCKS_15_CRBS           	20
#define IO_PERF_CLOCKS_6_TO_10_CRBS_USED 	21
#define IO_PERF_CLOCKS_4_PARTIAL_CACHES  	22
#define IO_PERF_CLOCKS_2_PARTIAL_CACHES  	23
#define IO_PERF_RETIRED_BEFORE_CRBS      	24
#define IO_PERF_PARTIAL_XTALK_WRITES     	25
#define IO_PERF_CRB_ALLOCATIONS          	26

/* IPPR0 Selection bits */
#define IO_PERF_SEL_MICRO_TO_LLP		(1 << IO_PERF_MICRO_TO_LLP)      
#define IO_PERF_SEL_DATA_DWORDS_FROM_LLP	(1 << IO_PERF_DATA_DWORDS_FROM_LLP)     
#define IO_PERF_SEL_DWORDS_TO_LEGONET		(1 << IO_PERF_DWORDS_TO_LEGONET)      
#define IO_PERF_SEL_DATA_DWORDS_FROM_LEGONET  	(1 << IO_PERF_DATA_DWORDS_FROM_LEGONET)
#define IO_PERF_SEL_11_TO_14_CRBS_USED        	(1 << IO_PERF_11_TO_14_CRBS_USED)     
#define IO_PERF_SEL_5_OR_LESS_CRBS_USED       	(1 << IO_PERF_5_OR_LESS_CRBS_USED)  
#define IO_PERF_SEL_3_PARTIAL_CACHE_USED      	(1 << IO_PERF_3_PARTIAL_CACHE_USED)
#define IO_PERF_SEL_1_PARTIAL_CACHE_USED      	(1 << IO_PERF_1_PARTIAL_CACHE_USED)    
#define IO_PERF_SEL_RETIRED_AFTER_CRBS        	(1 << IO_PERF_RETIRED_AFTER_CRBS)      
#define IO_PERF_SEL_EJECTED_CACHE_ENTRY 	(1 << IO_PERF_EJECTED_CACHE_ENTRY)     
#define IO_PERF_SEL_CRB_ENTRY_ALLOCATED       	(1 << IO_PERF_CRB_ENTRY_ALLOCATED)

/* IPPR1 Selection Bits */
#define IO_PERF_SEL_PACKETS_FROM_LLP        	(1 << IO_PERF_PACKETS_FROM_LLP)         
#define IO_PERF_SEL_DATA_PACKETS_TO_LLP     	(1 << IO_PERF_DATA_PACKETS_TO_LLP)      
#define IO_PERF_SEL_DWORDS_FROM_LEGONET     	(1 << IO_PERF_DWORDS_FROM_LEGONET)      
#define IO_PERF_SEL_DATA_DWORDS_TO_LEGONET  	(1 << IO_PERF_DATA_DWORDS_TO_LEGONET)   
#define IO_PERF_SEL_CLOCKS_15_CRBS          	(1 << IO_PERF_CLOCKS_15_CRBS)           
#define IO_PERF_SEL_CLOCKS_6_TO_10_CRBS_USED	(1 << IO_PERF_CLOCKS_6_TO_10_CRBS_USED) 
#define IO_PERF_SEL_CLOCKS_4_PARTIAL_CACHES 	(1 << IO_PERF_CLOCKS_4_PARTIAL_CACHES)  
#define IO_PERF_SEL_CLOCKS_2_PARTIAL_CACHES 	(1 << IO_PERF_CLOCKS_2_PARTIAL_CACHES)  
#define IO_PERF_SEL_RETIRED_BEFORE_CRBS     	(1 << IO_PERF_RETIRED_BEFORE_CRBS)      
#define IO_PERF_SEL_PARTIAL_XTALK_WRITES    	(1 << IO_PERF_PARTIAL_XTALK_WRITES)     
#define IO_PERF_SEL_CRB_ALLOCATIONS            	(1 << IO_PERF_CRB_ALLOCATIONS)         

#define IO_PERF_SEL_IPPR0		 	(0x7FF)
#define IO_PERF_SEL_IPPR1		 	(0x7FF0000)
#define IO_PERF_SEL_ALL				(IO_PERF_SEL_IPPR1 | IO_PERF_SEL_IPPR0)

/* The ioperf tests will not accepts illegal bits being set */
#define IO_PERF_CTRLBITS_INVALID(_bits)		(_bits & ~(IO_PERF_SEL_ALL))

typedef struct io_perf_control {
  uint ctrl_bits;
} io_perf_control_t;


/* Return value of syssgi get_count interface */
typedef struct io_perf_values {
	__int64_t iopv_count[IO_PERF_SETS];
	__int64_t iopv_timestamp[IO_PERF_SETS];
} io_perf_values_t;

#endif /* SN0 */

#endif
