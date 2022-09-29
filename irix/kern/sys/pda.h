/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1994, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_PDA_H__
#define __SYS_PDA_H__

#ident "$Revision: 3.299 $"

/*
 * sysmp(MP_STAT) structure
 */
struct pda_stat {
	int		p_cpuid;	/* processor ID */
	int		p_flags;	/* various flags */
};

/* values for p_flags */
#define PDAF_MASTER		0x0001
#define PDAF_CLOCK		0x0002
#define PDAF_ENABLED		0x0004	/* processor allowed to sched procs */
#define	PDAF_FASTCLOCK		0x0008	/* processor handles fastimer */
#define PDAF_ISOLATED		0x0010	/* processor is isolated */
#define PDAF_BROADCAST_OFF	0x0020	/* broadcast intr is not received */
#define PDAF_NONPREEMPTIVE	0x0040	/* processor is not preemptive */
#define PDAF_NOINTR		0x0080	/* processor disallows device interrupts */
					/* except for those specifically directed */
#define PDAF_ITHREADSOK		0x0100	/* ithreads have been enabled */
#define PDAF_DISABLE_CPU	0x0200  /* processor will be disabled on reboot*/

#define CNODEID_NONE (cnodeid_t)-1

#ifdef _KERNEL
#include <sys/immu.h>
#include <sys/timers.h>
#include <sys/pcb.h>
#include <sys/sema.h>
#include <sys/hwperftypes.h>

#ifdef EVEREST
#include <sys/EVEREST/everest.h>
#endif

#ifdef SN
#include <sys/SN/arch.h>
#include <sys/SN/intr_public.h>
#include <sys/SN/war.h>
#endif

#ifdef MP
#include <sys/callo.h>
#include <sys/calloinfo.h>
#endif
#ifdef	NUMA_BASE
struct	nodepda_s;	/* Defined in sys/nodepda.h */
#endif	/* NUMA_BASE */

#include <sys/q.h>

struct vmp_s;
typedef struct cpu_s {
	lock_t		c_lock;
	struct kthread	*c_threads;
#if MP
	cpuid_t		c_peekrotor;
	uchar_t		c_onq;
#endif
	uchar_t		c_restricted;
} cpu_t;

/*
 * The following allows other cpus to do things for you 
 */
typedef void (*cpuacfunc_t)(void *, void *, void *, void *);

typedef struct action_s {
	cpuacfunc_t	dofunc;
	void		*doarg0;
	void		*doarg1;
	void		*doarg2;
	void		*doarg3;
	struct action_s	*donext;
} action_t;

#ifdef MP
/* Following data structure carefully constructed to occupy one cacheline
 * for speedy cpuaction broadcasts.
 */
typedef struct actionlist_s {
	lock_t		actionlock;	/* lock for action list access */
	uint		cachelinefiller;	/* UNUSED */
	action_t	*firstaction;	/* first action_t in freelist */
	action_t	*todolist;	/* list of actions to do for others */
	action_t	*lasttodo;	/* last action to do for others */

	/* First two action blocks are in the same cacheline.
	 * This should pad this structure to 128 bytes.  Also, most of the time
	 * each cpu only utilizes one block so we handle most cpuactions with
	 * a single secondary cache miss.
	 */

	action_t	act_blocks[2];	
} actionlist_t;
#endif /* MP */

/*
 * The private data area for each processor
 * Always appears at the same virtual address in each processor
 * It is one page (4K), and we use the bottom 1024 bytes as a
 * boot/idle stack.
 */
/*
 * NOTE: when calling mutex/mrlock routines on pda synchronization
 * structures such as p_pcopy_lock,  make sure to pass k0 address;
 * priority inheritance code can reference this address from another
 * processor, so using private.p_pcopy_lock, for example, will result
 * in unpredictable behavior. Use pdaindr[cpuid()].pda->p_XXX instead.
 */
struct sthread_s;

/*
 * the pda itself
 */
typedef struct pda_s {
	/* This first cacheline contains items normally referenced by locore
	 * code.  Placing them in one cacheline makes operation more efficient.
	 * This cacheline contains most of the PDA variables referenced in
	 * the common exception path, especially interrupts and tlbmiss.
	 */
	union {
	    int	dbg[4];
	    struct {
		inst_t	*exceptnorm;	/* general exception handler */
		int	filler[2];
	    } common;
	} db;
	struct uthread_s *p_switchuthread; /* uthread that got kpreempted */
	struct uthread_s *p_curuthread;	/* current process thread */
	struct kthread	*p_curkthread;	/* current kernel thread */
	struct uli	*p_curuli;	/* current uli info if any */
	k_machreg_t	*p_intlastframe;/* last frame on interrupt stack */
	k_machreg_t	p_atsave;
	k_machreg_t	p_t0save;
	int		p_flags;	/* various flags */

#if EVEREST
	/* The following fields are placed in same cache-line as exceptnorm
	 * so we take at most one cache-miss in early locore exception
	 * handling (p_CEL_shadow & p_CEL_HW).
	 */
	unchar		p_CEL_shadow;		/* current CEL */
	unchar		p_CEL_hw;		/* CEL value in HW reg */
#else /* !EVEREST */
	unchar		p_alignpad3[2];
#endif /* !EVEREST */

	unchar		p_runrun;	/* switch on return to user */
	unchar		p_kstackflag;	/* kernel stack flag */

	/* Speed optimization when processing interrupts on idle stack.
	 * This flag is set in the idle() loop when we're just waiting for
	 * interrupts.  When an interrupt occurs we avoid the overhead of
	 * saving/restoring all of the registers and simply restart the
	 * idle loop through resumeidle().
	 * NOTE: Could almost always have this enabled.  Two exceptions
	 * are slave processes running cboot() which also runs on the idle
	 * stack, and master process handling console buffer I/O from
	 * idle.
	 */
	unchar		p_intr_resumeidle;
	unchar		p_pad_char[2];	/* THIS SPACE FOR RENT */
	unchar		p_idlstkdepth;	/* indication of idle stack depth */

	uint		p_cacheline_pad1[11];	/* skip to next cacheline */

	/* The following fields are in their own cacheline.  This was
	 * necessary in order to have efficient cpuvactions to an idle cpu.
	 * Apparently some fields in the preceeding cacheline are hammered
	 * by the idle loop, dramatically decreasing performance of the
	 * cpuvaction (by a factor of 200 or so) on large cpu configs.
	 *
	 * p_cpumask, p_acvec (p_va_XXXX), p_tlbflush_cnt, and p_delayacvec
	 * are all adjacent so they end up in same cacheline and speed
	 * operations like tlbsync() which need access to these fields.
	 *
	 * Since we have a limited number of events, give each a byte.
	 * This lets the value be update using normal "lb"/"sb" instead
	 * of atomic updates using LL/SC (big win for T5 with LL/SC war).
	 */
	char		p_va_panicspin;
	char		p_va_force_resched;
	char		p_va_reserved;
#ifdef	MC3_CFG_READ_WAR
	char		p_va_force_stop;
#else
	char		p_va_pad;	/* THIS SPACE FOR RENT */
#endif

	uint		p_tlbflush_cnt;	/* count of pending tlbflush vactions*/
	uint		p_delayacvec;	/* opcode vector for delay action */
	uint		p_pad_word;	/* THIS SPACE FOR RENT */

	cpumask_t	p_cpumask;	/* my processor ID mask */
#ifndef LARGE_CPU_COUNT
#ifdef EVEREST
 	char		p_cpumask_pad[8];	/* keep cacheline alignment*/
#else
 	char		p_cpumask_pad[12];
#endif
#endif /* LARGE_CPU_COUNT */
	uint64_t	p_tlbsync_asgen;

#if (MAXCPUS == 512)	/* cpumask is 8 words */
	uint		p_cacheline_pad2[10];	/* skip to next cacheline */
#elif (MAXCPUS == 256)	/* cpumask is 4 words */
	uint		p_cacheline_pad2[18];	/* skip to next cacheline */
#else			/* cpumask (+ pad) is 2 words */
	uint		p_cacheline_pad2[22];	/* skip to next cacheline */
#endif /* MAXCPUS */

#ifdef MP

	/* WARNING: this structure must immediately follow "db" and needs to
	 *	    modified if it changes size.
	 */

	/* For efficiency the p_actionlist should be cacheline aligned.  
	 * There are dependencies upon the "db" structure being first in the
	 * pda, so we pad out that cacheline makeing the p_actionlist second.
	 */
	actionlist_t	p_actionlist;	/* exactly one cacheline */
#endif /* MP */
#if _MIPS_SIM == _ABI64
	/* Two reasons to place causevec in the PDA:
	 *	- On SN0 systems the causevec will be local to the node
	 *	- On all 64-bit kernels you can quickly load it from (zero)
	 */
	int  (* const   p_causevec[32])(); /* two cachelines */
#endif /* _MIPS_SIM == _ABI64 */

	/* UTRACE and rtmon info should be in the same cacheline for
	 * efficiency, since they are often accessed together.
	 * See the #pragma after the declaration of pda_t.
	 */
	uint64_t	p_tstamp_mask;	/* Mask of timestamps in use */
	void		*p_tstamp_objp;	/* rtmon timestamp object */
	mrlock_t	p_tstamp_lock;	/* Lock on ptr. and all it contains;
					 * See NOTE about addressing pda
					 * synchronization variables.
					 */
	void		*p_tstamp_entries;	/* the array's base */
	void		*p_tstamp_ptr;		/* current point in the array*/
	void		*p_tstamp_last;		/* last element in the array */
	uint		p_tstamp_eobmode;	/* end-of-buffer mode */
	/* tstamp_align will be pushed to the _next_ cache line, the
	 * best we can do to avoid interference from following members;
	 * a fill pragma would have worked better.
	 */
	uint		p_tstamp_align;		

#if defined(SN0)
	/* On SN0 systems the nofault_pc array must be local to the node */
	int  (* const   p_nofault_pc[NF_NENTRIES])();
#endif
#if IP21
	unchar		p_dchip_err_hw;		/* cpu has D chip error logic*/
	unchar		p_dchip_err_bits;	/* D chip error bits */
#endif /* IP21 */
#ifdef	NUMA_BASE
	/* Having a pointer in the begining of PDA tends to increase 
	 * the chance of having this pointer in cache. (Yes something
	 * else gets pushed out). Doing this reduces the number of memory
	 * access to all nodepda variables to be one 
	 */
	struct nodepda_s *p_nodepda;	/* Pointer to Per node PDA */
#endif	/* NUMA_BASE */
	struct cpu_s	p_cpu;		/* scheduler specific processor info */
	cpuid_t		p_cpuid;	/* my processor ID */
	cnodeid_t	p_nodeid;	/* my node ID in compact-id-space */
	nasid_t		p_nasid;	/* my node ID in numa-as-id-space */
	unchar		p_asslevel;	/* level for production assertions */
	unchar		p_switching;	/* processor in swtch */
	unchar		p_schedflags;	/* scheduling flags mod by owner only */
	unchar		p_panicking;	/* The system is going to panic. */
	unchar		p_promlogging;	/* The system is doing promlogging. */
	struct kthread *p_nextthread;	/* next thread to run */
	k_machreg_t	*p_intstack;	/* base of interrupt stack */
	k_machreg_t	*p_bootlastframe;/* last frame on boot/idle stack */
	k_machreg_t	*p_bootstack;	/* base of boot/idle stack */

	/* fields for segment table manipulation */
	k_machreg_t	p_k1save;	/* save k1 */

	/* gfx wait flags; these are subcomponents of CPU wait */
        int             p_gfx_waitc;    /* waiting for gfx context swtch */
        int             p_gfx_waitf;    /* waiting for gfx fifo */
	lock_t		*p_curlock;	/* address of lock cpu is going after */
	lock_t		*p_lastlock;	/* addr of last lock locked */
	inst_t		*p_curlockcpc;	/* calling pc */

	/* scheduling values for the processor */

	int 		p_curpri;	/* current priority */
	int		p_cputype_word;	/* cpu rev */
	int		p_fputype_word;	/* fpu rev */
	int		p_nofault;	/* processor nofaults */
	caddr_t		p_kvfault;	/* processor kernel fault history */
	caddr_t		p_clrkvflt[8];
	struct tlbinfo	*p_tlbinfo;	/* tlb management info */
	struct icacheinfo *p_icacheinfo;/* icache pid management info */
	struct uthread_s *p_fpowner;	/* uthread owning fpu */
	unsigned int    p_hand;		/* a pseudo-random value */
	int		p_idletkn;	/* reasons of processor idle */
	int		p_lticks;	/* ticks left in time slice */
	int		p_vmeipl;	/* non-kmode VME interrupt level */
	unsigned	*p_prfptr;	/* ptr to profiler count table */
	short		p_prfswtch;	/* true if profiling switches */
	short		p_prfswtchcnt;	/* countdown to next swtch() profile */
	struct ksa	*ksaptr;	/* ptr to kernel system activities buf*/
	/*
	 * Local LED pattern maintainence.
	 */
	unsigned 	p_led_counter;
	unsigned	p_led_value;
#ifdef SN
	int		p_lastidle;	/* Used to update load on LEDs */
	int		p_lastvalue;	/* Last load */
	unchar		p_slice;	/* Physical position on node board */
	unchar		p_routertick;	/* How many seconds between reads */
	unchar		p_mdperf;	/* How many seconds between reads */
#if defined (SN0)
	unchar		p_sn00;		/* Are we an sn00? */
#endif /* SN0 */
	warbits_t	p_warbits;	/* Bitmap of enabled workarounds */
#endif /* SN */

	unsigned	p_dbgcntdown;	/* ticks between debugger checks */
	/*
	 * Special modifications lock.  Used to allow other processors
	 * to change p_flags or p_vmeipl (to distribute functionality).
	 */
	lock_t		p_special;

	/* for handling timein */
	int		p_timein;	/* set by timepoke */
#define PDA_TIMEIN		0x1
#define PDA_FTIMEIN		0x2
	int		fclock_freq;	/* freq of profiling clock */

	pde_t		p_pdalo;	/* tlblo entry for pda page */
	pde_t		p_ukstklo;	/* tlblo entry for ukstk */
#if TLBDEBUG
	pde_t		p_sv1lo;	/* tlblo entry for save of slot 1 */
	pde_t		p_sv1lo_1;	/* r4k: 2nd tlblo entry for slot 1 */
	uint		p_sv1hi;	/* tlbhi entry for save of slot 1 */
	pde_t		p_sv2lo;	/* tlblo entry for save of slot 2 */
	pde_t		p_sv2lo_1;	/* r4k: 2nd tlblo entry for slot 2 */
	uint		p_sv2hi;	/* tlbhi entry for save of slot 2 */
#endif

	/* delay calibration info */
	int 		decinsperloop;  /* deci-nanoseconds per 10 DELAY loop */

	uint		p_utlbmisses;	/* count user tlbmisses */
	uint		p_ktlbmisses;	/* count kernel tlbmisses */

	/* special utlbmiss handlers */
	int		p_utlbmissswtch; /* index of utlbmiss handler */
	inst_t	       *p_utlbmisshndlr; /* address of utlbmiss handler */

	unchar		p_idler;
#ifndef SN0
	/* special reserved virtual address lists for use in page_copy 
	 * and page_zero.  Should only be used by those routines. Using
	 * p_pcopy_lock to protect the pcopy fields, we can now allow
	 * kpreempt to occur during a page_copy/page_zero.
	 */
	char		p_pcopy_inuse[2];
	caddr_t		p_pcopy_pagelist[2];
	mutex_t		p_pcopy_lock;	/* protect p_pcopy fields; see NOTE about
					 * addressing pda synchronization variables.
					 */

	/* special reserved virtual address for use in cache operations */
	caddr_t		p_cacheop_pages;
	mutex_t		p_cacheop_pageslock; /* See NOTE about addressing pda
					      * synchronization variables.
					      */
#endif /* SN0 */
	uint		p_rtclock_rate;	/* R4k cycles/lbolt tick */
#if R4000
	char		*p_vcelog;	/* log of VCE exceptions (DEBUG) */
	int		p_vcelog_offset;/* offset into log of last entry*/
	int		p_vcecount;	/* count of VCE exceptions */
#endif
	char		*kstr_lfvecp;	/* kernel Streams buffer alloc area */
	char		*kstr_statsp;	/* kernel Streams statistic blocks */

	char		*knaptr;	/* kernel network statisics ptr */
	char		*nfsstat;	/* kernel nfs statisics ptr */
	char		*cfsstat;	/* kernel cachefs statisics ptr */
	int		p_prf_enabled;	/* count of kernel profiling users */

#if SN
	hub_intmasks_t	p_intmasks;	/* SN0 per-CPU interrupt masks */
#endif

#if IP30
	__uint64_t	p_clock_tick;	/* CTIME_IS_ABSOLUTE of next clock() */
	__uint64_t	p_fclock_tick;	/* " 		"    fastick_maint() */
	__uint64_t      p_next_intr;    /* "            "    of next intr */
	__uint32_t	p_clock_ticked;	/* launch clock() from intr */
	__uint32_t	p_fclock_ticked;/* launch fasttick_maint() from intr */
	__uint32_t	p_clock_slow;	/* count of missed clock interrupts*/
	__uint32_t	p_fclock_slow;	/* launch fasttick_maint() from intr */
#endif	/* IP30 */

#if MP
	__uint64_t	last_sched_intr_RTC;
	long long	counter_intr_over_sum;	/* sum of cycles over norm */
	uint		counter_intr_over_count;/* number of events */
	uint		counter_intr_over_max;	/* max cycles over norm */

	struct callout_info	p_calltodo;	/* callout queue */
#endif /* MP */

	/* The name of this CPU for reporting errors. */
	char 		*p_cpunamestr;

	/*
	 * Frame Scheduler
	 */
	void		*p_frs_objp;	/* frs pda-object */
	uint            p_frs_flags;	/* frs control */

	/*
	 * Secondary cache size- represents either sidcache_size,
	 * sicache_size, or sdcache_size. This is put into pda as 
	 * opposed to globals in order to support an MP mixture of 
	 * cpu boards with differing secondary cache sizes.
	 */
	int		p_scachesize;
#ifdef IP32
        /*
         * masks for CRIME interrupt mask register.
         */
	__uint64_t	p_splmasks[5];
	int		p_curspl;
#endif
	/*
	 * Non-panicking cpus save their registers here to make
	 * debugging easier.
	 */
	label_t		p_panicregs_tbl;
	int		p_panicregs_valid;
#if EXTKSTKSIZE == 1
	pde_t		p_stackext;	/* PTE for kernel stack extension */
	pde_t		p_bp_stackext;	/* PTE for backup kernel stack ext. */
	int		p_mapstackext;	/* Flag to indicate stk ext needs to be mapped */
#endif

#if defined(SN0)
	void		*p_bte_info;	/* CPU Specific BTE Info */
#endif

	cpu_mon_t	*p_cpu_mon; 	/* cpu's cpu_mon_t for event cntrs */
	cpu_mon_t	*p_active_cpu_mon; /* active user cpu_mon_t */
#ifdef R10000
	unchar		p_cacherr;	/* TRUE if cache error log pending */
#endif

	vertex_hdl_t	p_vertex;	/* hwgraph vertex representing this cpu */

	void *		pdinfo;		/* platform-dependent info */

	int		cpufreq;	/* cpu speed */
	int		cpufreq_cycles;
	int		ust_cpufreq;
#if IP27
	void *		p_elsc_portinfo; /* per cpu port to use for elsc printf */
#endif
#ifdef NUMA_BASE
        ushort          p_mem_tick_flags;          /* numa periodic op enable/disable */
        ushort          p_mem_tick_quiesce;        /* temporarily stop mem_tick */
        int             p_mem_tick_base_period;    /* numa periodic op base period */
        int             p_mem_tick_counter;        /* numa periodic op tick counter */
        int             p_mem_tick_seq;            /* seq of ticks within period */
        pfn_t           p_mem_tick_cpu_numpfns;    /* # of pfns scanned by this cpu */
        pfn_t           p_mem_tick_cpu_startpfn;   /* base pfn for list scanned by this cpu */

        __uint64_t      p_mem_tick_maxtime;        /* max time used by a memory tick */
        __uint64_t      p_mem_tick_mintime;        /* min time used by a memory tick */
        __uint64_t      p_mem_tick_lasttime;       /* time taken by the last tick */
        __uint64_t      p_mem_tick_avgtime;        /* cumulative average time */

        pfn_t           p_mem_tick_bounce_numpfns;     /* # of pfns scanned per mem_tick for bounce control */
        pfn_t           p_mem_tick_bounce_startpfn;    /* pfn index for the bounce control loop */
        pfn_t           p_mem_tick_bounce_accpfns;     /* number of pfns so far processed in period for bctrl */

        pfn_t           p_mem_tick_unpegging_numpfns;  /* # of pfns scanned per mem_tick for bounce control */
        pfn_t           p_mem_tick_unpegging_startpfn; /* pfn index for the unpegging loop */
        pfn_t           p_mem_tick_unpegging_accpfns;  /* number of pfns so far processed in period for uctrl */
#endif /* NUMA_BASE */
#ifdef SW_FAST_CACHE_SYNCH
	k_machreg_t	p_swinsave_tmp;	/* scratch for soft windows */
#endif
#ifdef _R5000_CVT_WAR
	double		p_fp0save;	/* save area for $f0 */
#endif
#ifdef JUMP_WAR
#if MP
#error	"This code is not mp safe - mutex locking needed to protect
#error	"the jump_war_pid and jump_war_uthreadid fields
#endif
	pid_t		p_jump_war_pid;
	int		p_jump_war_uthreadid;
#endif
#if defined (SN0)
	int		p_r10kcheck_cnt; /* check for status of progress */
	clkreg_t	p_nmi_rtc;	 /* rtc when nmi was sent        */
	clkreg_t	p_hung_rtc;	 /* rtc of cpu to which nmi was sent */
	int		p_progress_lck;  /* lock			*/
	cpuid_t		p_r10k_master_cpu;  /* cpuid of master who sent nmi */
	cpuid_t		p_r10k_ack;  	/* Did receiver see nmi? */
#endif
#if defined (R10000)
	char		p_r10kwar_bits;	
#endif /* R10000 */
#if defined (R10000) && defined (R10000_MFHI_WAR)
	uint		p_mfhi_brcnt;	/* count of war to branch */
	uint		p_mfhi_cnt;	/* count of war done	  */
	uint		p_mfhi_skip;	/* count of war skipped	*/
	machreg_t	p_mfhi_reg;
	machreg_t	p_mflo_reg;
	__psunsigned_t	p_mfhi_patch_buf;
#endif /*defined (R10000) && defined (R10000_MFHI_WAR)*/
	char		*dbastat;
#ifdef SN
	unchar		p_ioperf;	
#endif

	mrlock_t        **p_blaptr;     /* only required for CELL_CAPABLE, */
					/* but we try to keep the PDA the  */
					/* same size/layout                */
#ifdef SN
	__uint64_t      p_cerr_flags;
#endif

} pda_t;
/* Enforce cache line alignment on all types of build */
#pragma set field attribute pda_t p_tstamp_mask align=128
#pragma set field attribute pda_t p_tstamp_align align=128

/*	UTLBMISS_* defines the bitmask of utlbmiss handlers
 *	``installed'' in each pda.
 */
#define UTLBMISS_STD	0x0
#define UTLBMISS_COUNT	0x1
#define UTLBMISS_DEFER	0x2

#define	UTLBMISS_LPAGE	0x4
#define NUTLBMISS_HNDLRS	8	/* number of utlbmiss handlers */

struct utlbmiss_swtch {
	inst_t	*u_start;
	inst_t	*u_end;
};
extern struct utlbmiss_swtch utlbmiss_swtch[];

/*
 * Lock for putbuf.
 */
extern lock_t 	putbuflck;
extern int putbuf_lock(void);
extern int putbuf_trylock(void);
extern void putbuf_unlock(int);

#define PUTBUF_LOCK(_pl)	(_pl) = putbuf_lock()
#define PUTBUF_TRYLOCK(_pl)	((_pl) = putbuf_trylock())
#define PUTBUF_UNLOCK(_pl)	putbuf_unlock(_pl)

/*
 * Wait five seconds before breaking the putbuf lock.
 */
#define PUTBUF_LOCK_USECS	(USEC_PER_SEC * 5)

/*
 * Master processor's cpuid
 */
extern cpuid_t	master_procid;

#define	common_excnorm		db.common.exceptnorm

/* ``run-anywhere'' value for p_mustrun */
#define PDA_RUNANYWHERE		((cpuid_t)-1)

/* values for kstackflag */
#define PDA_CURUSRSTK		0 /* currently running on user stack */
#define PDA_CURKERSTK		1 /* currently running on kernel user stack */
#define PDA_CURINTSTK		2 /* currently running on interrupt stack */
#define PDA_CURIDLSTK		3 /* currently running on idle stack */
#define PDA_CURULISTK		4 /* currently running on ULI stack */

typedef struct pdaindr_s {
	int	CpuId;
	pda_t	*pda;
} pdaindr_t;

extern pdaindr_t	pdaindr[];
extern int		numcpus;	/* count of configured cpus */
extern int		numnodes;	/* count of configured nodes */
extern cpumask_t	maskcpus;	/* mask of configured cpus */
extern int		maxcpus;	/* max configured cpus */
extern int		maxnodes;	/* max configured nodes */
extern pda_t		*masterpda;	/* master processor's pda */
extern cpuid_t		clock_processor; /* processor that handles clock */
extern cpuid_t		fastclock_processor;
#ifndef _STANDALONE
#ifdef MP
extern cpuid_t		getcpuid(void);
#else
#define getcpuid()	0
#endif
#endif	/* _STANDALONE */

#if NUMA_BASE

extern cnodeid_t	getcnodeid(void);
#define cnodeid()	(private.p_nodeid)

extern int mem_tick_enabled;
extern int mem_tick_base_period;
extern void mem_tick(cpuid_t cpuid);

#define MEM_TICK()                                                                   \
        {                                                                            \
                if (private.p_mem_tick_flags && --private.p_mem_tick_counter <= 0) { \
                        private.p_mem_tick_counter =                                 \
                                private.p_mem_tick_base_period;                      \
                        mem_tick(cpuid());                                           \
                }                                                                    \
        }

#else /* !NUMA_BASE */

#define MAX_COMPACT_NODES 1
#define CPUS_PER_NODE	maxcpus
#define cnodetocpu(cnode) 0
#define CNODE_NUM_CPUS(cnode)	   CPUS_PER_NODE
#define getcnodeid()	0
#define cnodeid()	0
#define cputocnode(cpu)	0
#define get_cpu_cnode(cpu)	   cputocnode(cpu)
#define cputoslice(_cpu)	(_cpu)
#define cnode_slice_to_cpuid(_cnode, _slice)	(_slice)
#define CNODE_TO_CPU_BASE(cnode)   ((cnodeid_t)0)
#define COMPACT_TO_NASID_NODEID(c) ((nasid_t)(c))
#define NASID_TO_COMPACT_NODEID(n) ((cnodeid_t)(n))
#define MEM_TICK()

#endif /* NUMA_BASE */

extern int		sendintr(cpuid_t, unchar);

#define PDASZ		1		/* # pages of pda */

/* cpu isn't on (wasn't on) any processor */
#define CPU_NONE (cpuid_t)-1
#define CPU_QUEUE(cpu)	(&pdaindr[cpu].pda->p_cpu)

#define	getpda()	((pda_t *) PDAADDR)
#define	private		(*((pda_t *) PDAADDR))
#ifdef MP
#define	cpuid()		((cpuid_t)(private.p_cpuid))
#else
#define cpuid()		0
#endif
#define	cpumask()	(private.p_cpumask)

#ifdef MP
extern int cpu_enabled(cpuid_t);
#define cpu_restricted(cpu) (pdaindr[(cpu)].pda->p_cpu.c_restricted)
#else
#define	cpu_enabled(_c)	1
#define	cpu_restricted(_c)	(0)
#endif

#ifdef MP
#define ON_MP(X)	if (maxcpus > 1) {X;}
#define IS_MP		(maxcpus > 1)
#else
#define ON_MP(X)
#define IS_MP		0
#endif

/*
 * The following allows other cpus to do things for you 
 */

extern void actioninit(void);
extern void cpuaction(cpuid_t, cpuacfunc_t, int , ...);
extern void nested_cpuaction(cpuid_t, cpuacfunc_t, void *,void *,void *,void*);
extern void doactions(void);
extern void doacvec(void);
extern void doacvec_splhi(void);
extern void da_flush_tlb(void);
extern cpumask_t kvfaultcheck(int);

#define	A_NOW		0x0001	/* perform this action NOW */
#define A_QUEUE		0x0002	/* perform this action later (at disp time) */
#define	DOACTION	0xab	/* argument for sendintr() to do actions */
#if (defined(EVEREST) || defined(SN0)) && !defined(_NO_SPLTLB)
#define	DOTLBACTION	0xbb	/* sendintr() arg for random TLB flush */
#else
#define	DOTLBACTION	DOACTION
#endif

#if defined(DEBUG) || defined(ISOLATE_DEBUG)
/* Opcodes for the cpu action vector */
/* WARNING if you remove one of these opcodes then you also need to change 
 * the definition of the next opcode to reuse the value of the one removed
 * read the code in cpuvaction to understand
 */
extern void cpuvaction(register cpuid_t, register uint);
#define	SEND_VA_PANICSPIN(x)	{ cpuvaction(x, VA_PANICSPIN); }
#define	SEND_VA_FORCE_RESCHED(x) 	\
	{ASSERT(x != private.p_cpuid); cpuvaction(x, VA_FORCE_RESCHED);}
#define	CPUVACTION_RESCHED(x)	{ cpuvaction(x, VA_FORCE_RESCHED); }
#ifdef MC3_CFG_READ_WAR
#define	SEND_VA_FORCE_STOP(x)	\
	{ASSERT(x != private.p_cpuid); cpuvaction(x, VA_FORCE_STOP);}
#endif
#define	VA_PANICSPIN		0x1	/* highest priority vaction */
#define	VA_FORCE_RESCHED	0x4
#define	VA_TLBFLUSH_RAND	0x8
#ifdef MC3_CFG_READ_WAR
#define	VA_FORCE_STOP		0x10
#define	VA_LAST_OP		VA_FORCE_STOP
#else
#define	VA_LAST_OP		VA_TLBFLUSH_RAND
#endif
#else /* !DEBUG && !ISOLATE_DEBUG */
#define	CPU_VACTION(x)		\
		if (pdaindr[x].CpuId == private.p_cpuid)	\
			doacvec_splhi();			\
		else						\
			sendintr(x, DOACTION);

#define	SEND_VA_PANICSPIN(x)		\
	{pdaindr[x].pda->p_va_panicspin = 1; sendintr(x, DOACTION);}
#define	SEND_VA_FORCE_RESCHED(x)	\
	{pdaindr[x].pda->p_va_force_resched = 1; sendintr(x, DOACTION);}
#define	CPUVACTION_RESCHED(x)	\
	{pdaindr[x].pda->p_va_force_resched = 1; CPU_VACTION(x)}
#ifdef MC3_CFG_READ_WAR
#define	SEND_VA_FORCE_STOP(x)	\
	{pdaindr[x].pda->p_va_force_stop = 1; sendintr(x, DOACTION);}
#endif
#endif /* !DEBUG && !ISOLATE_DEBUG */

/* Opcodes for the cpu delay action vector */
#define	DA_ICACHE_FLUSH		0x1
#define	DA_TLB_FLUSH		0x2
#define	DA_LAST_OP		DA_TLB_FLUSH

struct utas_s;
extern void	utlbmiss_resume_nopin(struct utas_s *);
extern void	utlbmiss_resume(struct utas_s *);
extern void	utlbmiss_reset(void);
extern void	wirepda(pda_t *);
extern inst_t	*get_except_norm(void);

extern cpumask_t	allclr_cpumask;

extern void check_delay_tlbflush(int);
/* flags for check_delay_tlbflush() */
#define ENTRANCE	0
#define EXIT	1

#ifdef MP
#define CHECK_DELAY_TLBFLUSH(flag)	{\
	if (private.p_flags & PDAF_ISOLATED) \
		check_delay_tlbflush(flag); \
	}
#define CHECK_DELAY_TLBFLUSH_INTR(flag, s) {\
	if ((flag) == ENTRANCE) { \
		if ((private.p_flags & (PDAF_ISOLATED|PDAF_BROADCAST_OFF)) == \
					(PDAF_ISOLATED|PDAF_BROADCAST_OFF)) { \
			check_delay_tlbflush(flag); \
			s = 1; \
		} else \
			s = 0; \
	} else { \
		if (s && (private.p_flags & PDAF_ISOLATED)) \
			check_delay_tlbflush(flag); \
	} \
}
#else
#define CHECK_DELAY_TLBFLUSH(x)
#define CHECK_DELAY_TLBFLUSH_INTR(x)
#endif

/* values for tlb tracing - enabled with TLB_TRACE */
#if TLB_TRACE
extern struct ktrace *tlb_trace_buf;
#define KTRACE_TLBENTER(op, v0, v1, v2, v3) \
	ktrace_enter(tlb_trace_buf, \
		(void *)__return_address, \
		(void *)(__psunsigned_t)lbolt, \
		(void *)(__psunsigned_t)cpuid(), \
		(void *)(__psunsigned_t)private.p_flags, \
		(void *)(__psunsigned_t)op, \
		(void *)(__psunsigned_t)v0, \
		(void *)(__psunsigned_t)v1, \
		(void *)(__psunsigned_t)v2, \
		(void *)(__psunsigned_t)v3, \
		(void *)0 , (void *) 0, (void *) 0, \
		(void *)0 , (void *)0 , (void *) 0, (void *) 0)

#define TLB_TRACE_TLBFLUSH	0
#define TLB_TRACE_TLBSYNC	1
#define TLB_TRACE_TLBSYNC_DEL	2
#define TLB_TRACE_CLEAN_AGE	3
#define TLB_TRACE_CHECK_DEL	4
#define TLB_TRACE_FLUSH_DEL	5
#define TLB_TRACE_TLBSYNC1	6

#else
#define KTRACE_TLBENTER(op, v0, v1, v2, v3)
#endif /* TLB_TRACE */

#if IP19 || IP21 || IP25 || IP27 || IP28 || IP30 

extern int	nmi_maxcpus;	/* max configured cpus during nmi*/
#define MAX_NUMBER_OF_CPUS()	max(maxcpus,nmi_maxcpus)	

#else

#define MAX_NUMBER_OF_CPUS()	maxcpus

#endif	/* IP19 || IP21 || IP25 || IP27 || IP28 || IP30  */

#define	cpuvertex()		(private.p_vertex)
#define cpuid_to_vertex(cpuid) (pdaindr[cpuid].pda->p_vertex)
#define cpu_allows_intr(cpuid) (!(pdaindr[cpuid].pda->p_flags & PDAF_NOINTR))
extern void enter_panic_mode(void);
extern void exit_panic_mode(void);
extern int in_panic_mode(void);

extern void enter_dump_mode(void);
extern void exit_dump_mode(void);
extern int in_dump_mode(void);
extern void enter_promlog_mode(void);
extern void exit_promlog_mode(void);
extern int in_promlog_mode(void);


#endif /* _KERNEL */

#endif /* __SYS_PDA_H__ */
