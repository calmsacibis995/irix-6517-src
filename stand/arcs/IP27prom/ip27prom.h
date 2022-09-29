/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	_IP27PROM_H_
#define	_IP27PROM_H_

#define BIT_RANGE(hi, lo) \
	((0xffffffffffffffff >> (63 - (hi))) & (0xffffffffffffffff << (lo)))

#define GET_FIELD(var, fname) \
	((var) >> fname##_SHFT & fname##_MASK >> fname##_SHFT)

#define SET_FIELD(var, fname, fval) \
	((var) = (var) & ~fname##_MASK | (__uint64_t) (fval) << fname##_SHFT)

#define DEFAULT_SEGMENT		"io6prom"

#if _LANGUAGE_ASSEMBLY
#define ROUND8(x)		(((x) + 7) & ~7)
#define ROUND1K(x)		(((x) + 0x3ff) & ~0x3ff)
#define ROUND4K(x)		(((x) + 0xfff) & ~0xfff)
#else
#define ROUND8(x)		(((__uint64_t) (x) + 7) & ~7)
#define ROUND1K(x)		(((__uint64_t) (x) + 0x3ff) & ~0x3ff)
#define ROUND4K(x)		(((__uint64_t) (x) + 0xfff) & ~0xfff)
#endif /* _LANGUAGE_ASSEMBLY */

#ifdef _LANGUAGE_C

extern char			_ftext[], _fbss[], _end[];
extern int 			verbose;

#define TEXT_LENGTH		((__psunsigned_t) _fbss - \
				 (__psunsigned_t) _ftext)

#define BSS_LENGTH		((__psunsigned_t) _end - \
				 (__psunsigned_t) _fbss)

#define TOTAL_LENGTH		(TEXT_LENGTH + BSS_LENGTH)

#define MIN_BANK_SIZE		(32 * 0x100000)
#define MIN_BANK_STRING		"32"

#endif /* _LANGUAGE_C */

/*
 * Use TRACE in code to help with RTL debugging.
 * Search the RTL logs for writes to PI_CPU_NUM.
 */

#if _LANGUAGE_ASSEMBLY
#   define	DMFC1(_v, _r)	dmfc1	_v,_r
#   define	DMTC1(_v, _r)	dmtc1	_v,_r

#   define	MFC1(_v, _r)	mfc1	_v,_r
#   define	MTC1(_v, _r)	mtc1	_v,_r

#   define	MESSAGE(r, s)	\
	 dla	    r, 99f; .data ;99: .asciiz s; .text

# if defined(RTL) || defined(GATE)
#   define	TRACE(x)	dli k0, LOCAL_HUB(0);			\
				or k1, zero, x; \
				sd k1, PI_CPU_NUM(k0)
# else
#   define	TRACE(x)
# endif
#endif /* _LANGUAGE_ASSEMBLY */

#if _LANGUAGE_C

#if defined(RTL) || defined(GATE)
# define TRACE(x)		(SD(LOCAL_HUB(PI_CC_MASK),		\
				    0xee00000000000000 |		\
				    (ulong) TRACE_FILE_NO << 48 |	\
				    (ulong) __LINE__ << 32),		\
				 SD(LOCAL_HUB(PI_CC_MASK), (x)))
#else
# define TRACE(x)
#endif

/*
 * The Hub only supports word and double-word reads from the IP27 PROM.
 * The following macros assist in loading chars and shorts.
 *
 * LBYTE and LBYTEU load a byte, with or without sign extension
 * LHALF and LHALFU load a half-word, with or without sign extension
 *
 * Warning: These macros evaluate their arguments more than once.
 */

#define LBYTE(caddr) \
	(char) ((*(int *) ((__psunsigned_t) (caddr) & ~3) <<		\
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

#define LBYTEU(caddr) \
	(u_char) ((*(uint *) ((__psunsigned_t) (caddr) & ~3) <<		\
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 24)

#define LHALF(caddr) \
	(short) ((*(int *) ((__psunsigned_t) (caddr) & ~3) <<		\
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 16)

#define LHALFU(caddr) \
	(ushort_t) ((*(uint *) ((__psunsigned_t) (caddr) & ~3) <<	\
	  ((__psunsigned_t) (caddr) & 3) * 8) >> 16)

#endif /* _LANGUAGE_C */

#if defined(_LANGUAGE_ASSEMBLY)

/*
 * Define floating point registers to hold special values since there
 * is no memory available on startup.
 *
 * NOTE:  See initializeCPU when adding another register.
 */

#define	BR_BSR			$f0	/* Boot status reg. (see BSR_xxx) */
#define	BR_NOFAULT		$f1
#define	BR_ASM_HANDLER		$f2
#define	BR_LIBC_DEVICE		$f3
#define	BR_IOC3UART_BASE	$f4
#define	BR_ELSC_SPACE		$f5	/* ELSC internal data space */

/*
 * Warning: the rest of the fpregs are currently overwritten by
 * saveGprs when an exception occurs.
 */

#define BR_ERR_PI_EINT_PEND	$f14
#define BR_ERR_PI_ESTS0_A	$f15
#define BR_ERR_PI_ESTS0_B	$f16
#define BR_ERR_PI_ESTS1_A	$f17
#define BR_ERR_PI_ESTS1_B	$f18

#define BR_ERR_MD_DIR_ERROR	$f19
#define BR_ERR_MD_MEM_ERROR	$f20
#define BR_ERR_MD_PROTO_ERROR	$f21
#define BR_ERR_MD_MISC_ERROR	$f22

/*
 * Macros to move values to or from a boot register (BR_XXXX)
 */

#define	DMFBR(to, from)	DMFC1(to, from)
#define	DMTBR(from, to)	DMTC1(from, to)

#endif /* defined(_LANGUAGE_ASSEMBLY) */

/*
 * Boot Status Register
 */

#define	BSR_CPUNUM		(1 <<  0)	/* CPU number bit           */
#define BSR_DEX			(1 <<  1)	/* dirty excl. cache mode   */
#define BSR_UNC			(1 <<  2)	/* uncached memory mode     */
#define	BSR_PODDEX		(1 <<  3)	/* Stop in DEX POD mode     */
#define	BSR_PODMEM		(1 <<  4)	/* Stop in cached POD mode  */
#define	BSR_NVRAM		(1 <<  5)	/* NVRAM is legitimate 	    */
#define	BSR_SYSCTLR		(1 <<  6)	/* sys ctrller passed arb   */
#define BSR_MANUMODE		(1 <<  7)	/* running with CC UART     */
#define BSR_NODIAG		(1 <<  8)	/* no diagnostics 	    */
#define	BSR_ABDICATE		(1 <<  9)	/* Already been master 	    */
#define	BSR_MEMTESTS		(1 << 10)	/* Memory tests requested   */
#define BSR_INITCONFIG		(1 << 11)	/* KLCONFIG initialized     */
#define BSR_UNUSED4		(1 << 12)
#define BSR_GDASYMS		(1 << 13)	/* Use GDA symtab as well   */
#define BSR_KREGS		(1 << 14)	/* display kernel nmi regs  */
#define BSR_UNUSED5		(1 << 15)
#define BSR_VERBOSE_SHFT	16
#define BSR_VERBOSE		(1 << BSR_VERBOSE_SHFT)	  /* Printf verbose */
#define BSR_DIPSAVAIL		(1 << 17)
#define BSR_KDEBUG		(1 << 18)
#define BSR_POWERON		(1 << 19)	/* Reset was due to pwr-on  */
#define BSR_LOCALPOD		(1 << 20)	/* Stop in local POD mode   */
#define BSR_GLOBALPOD		(1 << 21)	/* Stop in global POD mode  */
#define BSR_ALLPREM		(1 << 22)	/* All nodes premium DIMMs  */
#define BSR_DBGNOT0		(1 << 23)	/* Non-zero Debug switches  */
#define BSR_COARSEMODE		(1 << 24)	/* Using coarse region size */
#define BSR_PART		(1 << 25)       /* Partitioned machine      */
#define BSR_NETCONFIG           (1 << 26)       /* SN0net fully configured  */

#define BSR_DIPSHFT		48		/* DIP switches (virt+phys) */
#define BSR_DIPMASK		(0xffffUL << BSR_DIPSHFT)

#if 1
#define	PROM_SR			SR_CU1|SR_FR|SR_KX|SR_BEV
#else
#define	PROM_SR			SR_CU1|SR_FR|SR_KX|SR_BEV|SR_IE
#endif

#define PROM_SR_INTR		(SRB_ERR)

/*
 * If you try to run a memory test on memory lower than this address,
 * the prom will print a warning.
 */

#define LOMEM_STRUCT_END	0x4000

/*
 * By calling the ioc3uart I/O routines directly the user can select between
 * channel A and channel B.
 */

/* XXX fixme XXX */
#define CHN_A	(/* IOC3_DUART1 + */0x8)
#define CHN_B	(/* IOC3_DUART1 + */0x0)

#if _LANGUAGE_C

#   define	LB(x)		(*(volatile char  *)(x))
#   define	LH(x)		(*(volatile short *)(x))
#   define	LW(x)		(*(volatile int	  *)(x))
#   define	LD(x)		(*(volatile long  *)(x))

#   define	LBU(x)		(*(volatile unsigned char  *) (x))
#   define	LHU(x)		(*(volatile unsigned short *) (x))
#   define	LWU(x)		(*(volatile unsigned int   *) (x))
#   define	LDU(x)		(*(volatile unsigned long  *) (x))

#   define	SB(x,v)		(LBU(x) = (unsigned char ) (v))
#   define	SH(x,v)		(LHU(x) = (unsigned short) (v))
#   define	SW(x,v)		(LWU(x) = (unsigned int	 ) (v))
#   define	SD(x,v)		(LDU(x) = (unsigned long ) (v))

#   define	LD_HI(x)	(LDU(x) >> 32)
#   define	LD_LO(x)	(LDU(x) & 0xffffffff)

#   define	SD_HI(x,v)	SD(x, (unsigned long) (v) & 0xffffffff00000000)
#   define	SD_LO(x,v)	SD(x, (unsigned long) (v) & 0x00000000ffffffff)

#endif /* _LANGUAGE_C */

/*
 * PROMDATA defines a memory area residing only within the cache
 *          for use during boot (initialized as dirty exclusive).
 *
 * POD_STACK defines a subarea of PROMDATA for the cache stack.
 * POD_ELSC defines a subarea of PROMDATA for ELSC driver internal data.
 */

#define PROMDATA_VADDR		0xa800000000100000
#define PROMDATA_PADDR		0x0000000000100000
#define PROMDATA_SIZE		0x8000		/* Same as D-cache size */

#define	POD_STACKVADDR		(PROMDATA_VADDR + 0x0)
#define	POD_STACKSIZE		0x4000

#define POD_ELSCVADDR		(PROMDATA_VADDR + 0x4000)
#define POD_ELSCSIZE		0x400

#define POD_ERRORVADDR		(PROMDATA_VADDR + 0x4400)
#define POD_ERRORSIZE		0x200

#define SET_PROGRESS_FAST	1		/* Using 0 slows boot LEDs */

#ifdef _LANGUAGE_ASSEMBLY

#if SET_PROGRESS_FAST || defined(SABLE) || defined(RTL) || defined(GATE)
#define SET_PROGRESS_DELAY()
#else
#define SET_PROGRESS_DELAY()						\
	dli	k0, 100000;						\
99:	bnez	k0, 99b;						\
	 dsubu	k0, 1
#endif

#define	SET_PROGRESS_SHADOW(s)						\
	dli	k0, LOCAL_HUB(0);					\
	ld	k1, PI_CPU_NUM(k0);					\
	dsll	k1, 3;							\
	dli	k0, LOCAL_HUB(PI_RT_COMPARE_A);				\
	daddu	k0, k1;							\
	or	k1, zero, (s);						\
	sd	k1, 0(k0)

#define SET_PROGRESS(s)							\
	dli	k0, LOCAL_HUB(0);					\
	ld	k1, PI_CPU_NUM(k0);					\
	dsll	k1, 3;							\
	dli	k0, LOCAL_HUB(PI_RT_COMPARE_A);				\
	daddu	k0, k1;							\
	or	k1, zero, (s);						\
	sd	k1, 0(k0);						\
	HUB_LED_SET(s);							\
	SET_PROGRESS_DELAY()

#endif /* _LANGUAGE_ASSEMBLY */

#ifdef _LANGUAGE_C

#if SET_PROGRESS_FAST || defined(SABLE) || defined(RTL) || defined(GATE)
#define SET_PROGRESS_DELAY()
#else
#define SET_PROGRESS_DELAY()	rtc_sleep(400000);
#endif

#define SET_PROGRESS(s)							\
	{								\
	    SD(LOCAL_HUB(PI_RT_COMPARE_A) +				\
	       LD(LOCAL_HUB(PI_CPU_NUM)), (s));				\
	    hub_led_set(s);						\
	    SET_PROGRESS_DELAY();					\
	}

#define SET_PROGRESS_OTHER(s)						\
	{								\
	    SD(LOCAL_HUB(PI_RT_COMPARE_A) +				\
		(LD(LOCAL_HUB(PI_CPU_NUM)) ? 0 : 1), (s));		\
	    hub_led_set(s);						\
	    SET_PROGRESS_DELAY();					\
	}

#endif /*  _LANGUAGE_C */

#ifdef _LANGUAGE_C
/* Routines from main.c */
extern	int	copy_prom_to_memory(int);
extern	void	read_dip_switches(uchar_t *);
extern	int	run_discover(int);
extern	int	distribute_tables(void);
extern	int	distribute_nasids(void);
extern	int	set_coarse_mode(void);
extern	int	load_execute_segment(char *, int, int);
#endif /* _LANGUAGE_C */

/* Verion from which partitioning supported */

#define	PARTITION_PVERS		6

/*
 * ADVERT are advertisement masks for information in NI_SCRATCH_REG1.
 */

#define ADVERT_OBJECTS_SHFT	0	/* Valid before global arb. */
#define ADVERT_OBJECTS_MASK	0xffffULL
#define ADVERT_NASID_SHFT	0	/* Valid after global arb. */
#define ADVERT_NASID_MASK	0xffffULL
#define ADVERT_NASID_NONE	0xffffULL
#define ADVERT_CONSOLE_SHFT	16
#define ADVERT_CONSOLE_MASK	(1ULL << ADVERT_CONSOLE_SHFT)
#define ADVERT_BARRIER_SHFT	17
#define ADVERT_BARRIER_MASK	(1ULL << ADVERT_BARRIER_SHFT)
#define ADVERT_LOCALPOD_SHFT	18
#define ADVERT_LOCALPOD_MASK	(1ULL << ADVERT_LOCALPOD_SHFT)
#define ADVERT_GLOBALPOD_SHFT	19
#define ADVERT_GLOBALPOD_MASK	(1ULL << ADVERT_GLOBALPOD_SHFT)
#define ADVERT_ROUTERWAR_SHFT	20
#define ADVERT_ROUTERWAR_MASK	(1ULL << ADVERT_ROUTERWAR_SHFT)
#define ADVERT_GMASTER_SHFT	21
#define ADVERT_GMASTER_MASK	(1ULL << ADVERT_GMASTER_SHFT)
#define ADVERT_DISCDONE_SHFT	22
#define ADVERT_DISCDONE_MASK	(1ULL << ADVERT_DISCDONE_SHFT)
#define ADVERT_POWERON_SHFT	23
#define ADVERT_POWERON_MASK	(1ULL << ADVERT_POWERON_SHFT)
#define ADVERT_PROMVERS_SHFT	24	/* Valid before global arb. */
#define ADVERT_PROMVERS_MASK	(0xffULL << ADVERT_PROMVERS_SHFT)
#define ADVERT_PROMREV_SHFT	32	/* Valid before global arb. */
#define ADVERT_PROMREV_MASK	(0xffULL << ADVERT_PROMREV_SHFT)
#define ADVERT_BARRLINE_SHFT	24	/* Valid after global arb. */
#define ADVERT_BARRLINE_MASK	(0xffffULL << ADVERT_BARRLINE_SHFT)
#define ADVERT_SLOTNUM_SHFT	40
#define ADVERT_SLOTNUM_MASK	(0xfULL << ADVERT_SLOTNUM_SHFT)
#define ADVERT_CPUMASK_SHFT     44
#define ADVERT_CPUMASK_MASK     (0xfULL << ADVERT_CPUMASK_SHFT)

#define ADVERT_PREMDIR_SHFT     48
#define ADVERT_PREMDIR_MASK     (1ULL << ADVERT_PREMDIR_SHFT)
#define ADVERT_COARSE_SHFT      49
#define ADVERT_COARSE_MASK      (1ULL << ADVERT_COARSE_SHFT)
#define ADVERT_SN00_SHFT        50
#define ADVERT_SN00_MASK        (1ULL << ADVERT_SN00_SHFT)
#define ADVERT_BOOTED_SHFT      51
#define ADVERT_BOOTED_MASK      (0x1ULL << ADVERT_BOOTED_SHFT)
#define ADVERT_PARTPROM_SHFT    52
#define ADVERT_PARTPROM_MASK    (0x1ULL << ADVERT_PARTPROM_SHFT)
#define ADVERT_EXCP_SHFT        53
#define ADVERT_EXCP_MASK        (0x1ULL << ADVERT_EXCP_SHFT)
#define ADVERT_UNUSED_SHFT      54
#define ADVERT_UNUSED_MASK      (0x1ULL << ADVERT_UNUSED_SHFT)
#define ADVERT_MODPROM_SHFT     55      /* ELSC down, used PROM module # */
#define ADVERT_MODPROM_MASK     (1ULL << ADVERT_MODPROM_SHFT)
#define ADVERT_MODULE_SHFT      56
#define ADVERT_MODULE_MASK      (0xffULL << ADVERT_MODULE_SHFT)

/*
 * Valid only in pvers >= PARTITION_PVERS
 * The partitionid is advertized in the NI_SCRATCH_REG0. The HUB NIC is
 * 48 bits
 */

#define	ADVERT_HUB_NIC_SHFT	0
#define	ADVERT_HUB_NIC_MASK	(0xffffffffffffULL << ADVERT_HUB_NIC_SHFT)
#define	ADVERT_PARTITION_SHFT	48	/* Valid after rtr tbl dist. */
#define	ADVERT_PARTITION_MASK	(0xffULL << ADVERT_PARTITION_SHFT)

#define MAX_ALT_REGS		1

#define NMI_KERNEL_REGS		1
#define NMI_KREGS_ADDR		(COMPAT_K1BASE | IP27_NMI_KREGS_OFFSET)

#define DIP_SWITCH(num)		(get_BSR() >> BSR_DIPSHFT - 1 + (num) & 1UL)
#define	DIP_SWITCH_CLEAR(num)	(set_BSR(get_BSR() & ~(1UL << BSR_DIPSHFT - 1 + (num))))
#define	DIP_SWITCH_SET(num)	(set_BSR(get_BSR() | (1UL << BSR_DIPSHFT - 1 + (num))))

#define DIP_NUM_DIAG_MODE1	1	/* Physical switches (1 through 8) */
#define DIP_NUM_DIAG_MODE0	2
#define DIP_NUM_VERBOSE		3
#define DIP_NUM_DIAG_STOP1	4
#define DIP_NUM_DIAG_STOP0	5
#define DIP_NUM_DEFAULT_ENV	6
#define DIP_NUM_BYPASS_IO6	7
#define DIP_NUM_ABDICATE	8

#define DIP_NUM_NODISABLE	9	/* Virtual switches (9 through 16) */
#define DIP_NUM_UNUSED10	10
#define DIP_NUM_DEFAULT_CONS	11
#define DIP_NUM_ROUTER_OVEN	12
#define DIP_NUM_ERROR_SHOW	13
#define DIP_NUM_IGNORE_AUTOBOOT	14
#define DIP_NUM_MIDPLANE_DIAG	15
#define DIP_NUM_METAROUTER_DIAG	16

#define	DIP_SWITCHES()		(get_BSR() >> BSR_DIPSHFT & \
				 BSR_DIPMASK >> BSR_DIPSHFT)

#define DIP_DIAG_MODE()		(DIP_SWITCH(DIP_NUM_DIAG_MODE1) << 1 | \
				 DIP_SWITCH(DIP_NUM_DIAG_MODE0))
#define DIP_VERBOSE()		DIP_SWITCH(DIP_NUM_VERBOSE)
#define DIP_DIAG_STOP()		(DIP_SWITCH(DIP_NUM_DIAG_STOP1) << 1 | \
				 DIP_SWITCH(DIP_NUM_DIAG_STOP0))
#define DIP_DEFAULT_ENV()	DIP_SWITCH(DIP_NUM_DEFAULT_ENV)
#define DIP_BYPASS_IO6()	DIP_SWITCH(DIP_NUM_BYPASS_IO6)
#define DIP_ABDICATE()		DIP_SWITCH(DIP_NUM_ABDICATE)
#define DIP_NODISABLE()		DIP_SWITCH(DIP_NUM_NODISABLE)
#define	DIP_DEFAULT_CONS()	DIP_SWITCH(DIP_NUM_DEFAULT_CONS)
#define	DIP_ROUTER_OVEN()	DIP_SWITCH(DIP_NUM_ROUTER_OVEN)
#define	DIP_ERROR_SHOW()	DIP_SWITCH(DIP_NUM_ERROR_SHOW)
#define	DIP_IGNORE_AUTOBOOT()	DIP_SWITCH(DIP_NUM_IGNORE_AUTOBOOT)
#define DIP_MIDPLANE_DIAG()	DIP_SWITCH(DIP_NUM_MIDPLANE_DIAG)
#define DIP_METAROUTER_DIAG()	DIP_SWITCH(DIP_NUM_METAROUTER_DIAG)

/* These are used for the disable masks after memory tests */
#define	DISABLE_BIT_SHFT	0
#define	DISABLE_BIT_MASK	(1 << DISABLE_BIT_SHFT)
#define	DISABLE_BANK_SHFT	1

#define	SET_DISABLE_BIT(reg)		\
			(reg | DISABLE_BIT_MASK)

#define	GET_DISABLE_BIT(reg)		\
			(reg & DISABLE_BIT_MASK)

#define	SET_DISABLE_BANK(reg, bank)	\
			(reg | ((1 << bank) << DISABLE_BANK_SHFT))

#define	GET_DISABLE_BANK(reg, bank)	\
			(reg & ((1 << bank) << DISABLE_BANK_SHFT))

#define	PARTITION_YES	0
#define	PARTITION_INVAL	1

#endif /* _IP27PROM_H_ */
