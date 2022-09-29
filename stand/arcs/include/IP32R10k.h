#ifndef	__SYS_R10K_H__
#define	__SYS_R10K_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.1 $"

#if defined(_LANGUAGE_C) 
#include <sys/types.h>			/* needed for cacheop_t */
#endif

/*
 * Coprocessor 0 registers
 * Some of these are r4000 specific.
 */
#ifdef _LANGUAGE_ASSEMBLY
#define	C0_INX		$0
#define	C0_RAND		$1
#define	C0_TLBLO_0	$2
#define	C0_TLBLO_1	$3
#define	C0_CTXT		$4
#define	C0_PGMASK	$5		/* page mask */
#define	C0_TLBWIRED	$6		/* # wired entries in tlb */
#define	C0_COUNT	$9		/* free-running counter */
#define	C0_BADVADDR	$8
#define	C0_TLBHI	$10
#define	C0_COMPARE	$11		/* counter comparison reg. */
#define	C0_SR		$12
#define	C0_CAUSE	$13
#define	C0_EPC		$14
#define	C0_PRID		$15		/* revision identifier */
#define	C0_CONFIG	$16		/* hardware configuration */
#define	C0_LLADDR	$17		/* load linked address */
#define	C0_WATCHLO	$18		/* watchpoint */
#define	C0_WATCHHI	$19		/* watchpoint */
#define	C0_EXTCTXT	$20		/* Extended context */
#define	C0_FMMASK	$21		/* Frame Mask */
#define	C0_BRDIAG	$22		/* Indices of tlb wired entries */
#define	C0_PRFCNT0	$25		/* performance counter 0 */
#define C0_PRFCNT1	$25		/* performance counter 1  */
#define	C0_PRFCRTL0	$25		/* performance control reg 0 */
#define C0_PRFCRTL1	$25		/* performance control reg 1  */
#define	C0_ECC		$26		/* S-cache ECC and primary parity */
#define	C0_CACHE_ERR	$27		/* cache error status */
#define	C0_TAGLO	$28		/* cache operations */
#define	C0_TAGHI	$29		/* cache operations */
#define	C0_ERROR_EPC	$30		/* ECC error prg. counter */
# else	/* ! _LANGUAGE_ASSEMBLY */
#define	C0_INX		0
#define	C0_RAND		1
#define	C0_TLBLO_0	2
#define	C0_TLBLO_1	3
#define	C0_CTXT		4
#define	C0_PGMASK	5		/* page mask */
#define	C0_TLBWIRED	6		/* # wired entries in tlb */
#define	C0_BADVADDR	8
#define	C0_COUNT	9		/* free-running counter */
#define	C0_TLBHI	10
#define	C0_COMPARE	11		/* counter comparison reg. */
#define	C0_SR		12
#define	C0_CAUSE	13
#define	C0_EPC		14
#define	C0_PRID		15		/* revision identifier */
#define	C0_CONFIG	16		/* hardware configuration */
#define	C0_LLADDR	17		/* load linked address */
#define	C0_WATCHLO	18		/* watchpoint */
#define	C0_WATCHHI	19		/* watchpoint */
#define	C0_EXTCTXT	20		/* Extended context */
#define	C0_FMMASK	21		/* Frame Mask */
#define	C0_BRDIAG	22		/* Indices of tlb wired entries */
#define C0_PRFCNT0      25             /* performance counter 0 */
#define C0_PRFCNT1      25             /* performance counter 1  */
#define C0_PRFCRTL0     25             /* performance control reg 0 */
#define C0_PRFCRTL1     25             /* performance control reg 1  */
#define	C0_ECC		26		/* S-cache ECC and primary parity */
#define	C0_CACHE_ERR	27		/* cache error status */
#define	C0_TAGLO	28		/* cache operations */
#define	C0_TAGHI	29		/* cache operations */
#define	C0_ERROR_EPC	30		/* ECC error prg. counter */
#endif	/* _LANGUAGE_ASSEMBLY */

/*
 * R10k Config Register defines.
 */
#define	CONFIG_IC	0xe0000000	/* primary instruction cache size */
#define	CONFIG_IC_SHFT	29
#define	CONFIG_DC	0x1c000000 	/* primary data cache size */
#define	CONFIG_DC_SHFT	26
#define	CONFIG_SC	0x00380000 	/* Secondary cache clock ratio */
#define	CONFIG_SC_SHFT	19
#define	CONFIG_SS	0x00070000	/* secondary cache size */
#define	CONFIG_SS_SHFT	16
#define	CONFIG_BE	0x00008000	/* Memory/kernel Endianness */
#define CONFIG_BE_SHFT	15
#define	CONFIG_SK	0x00004000 	/* Correbcting sec. data ECC err */
#define	CONFIG_SB	0x00002000	/* Secondary cache block size 16/32 */
#define CONFIG_SB_SHFT	13
#define	CONFIG_EC	0x00001e00 	/* System interface clock ratio */
#define	CONFIG_EC_SHFT	9
#define	CONFIG_PM	0x00000180	/* Max outstanding proc requests */
#define	CONFIG_PM_SHFT	7
#define	CONFIG_PE	0x00000040	/* Enable/Disable  eliminate req */
#define	CONFIG_CT	0x00000020	/* Target of coherence reqs */
#define	CONFIG_DN	0x00000018	/* Device number */
#define	CONFIG_DN_SHFT	3
#define	CONFIG_K0	0x00000007	/* K0SEG Coherency algorithm */

#define	CONFIG_UNCACHED		0x00000002	/* K0 is uncached */
#if _RUN_UNCACHED
#define	CONFIG_NONCOHRNT	CONFIG_UNCACHED
#define	CONFIG_COHRNT_EXL	CONFIG_UNCACHED
#define	CONFIG_COHRNT_EXLWR	CONFIG_UNCACHED
#else
#define	CONFIG_NONCOHRNT	0x00000003
#define	CONFIG_COHRNT_EXL	0x00000004
#define	CONFIG_COHRNT_EXLWR	0x00000005
#endif	/* _RUN_UNCACHED */
#define	CONFIG_UNCACHED_ACC	0x00000007	/* K0 is uncached accelerated */

#define	CONFIG_PCACHE_POW2_BASE	12	/* 2^12+# in config is primary size */
#define	CONFIG_SCACHE_POW2_BASE 19	/* 2^??+# in config is 2ndary size */

/*
 * R10k Cache Definitions
 * Cache sizes are in bytes
 */

#define	CACHE_ILINE_SIZE	64	/* Primary instruction line size */
#define	CACHE_ILINE_MASK	~(CACHE_ILINE_SIZE-1)
#define	CACHE_DLINE_SIZE	32	/* Primary data line size */
#define	CACHE_DLINE_MASK	~(CACHE_DLINE_SIZE-1)
#define	CACHE_SLINE_SIZE	128	/* Secondary cache line size */
#define	CACHE_SLINE_MASK	~(CACHE_SLINE_SIZE-1)
#define	CACHE_SLINE_SUBSIZE	16	/* quadword subsize */

/* 
 * Primary Cache Tag Definitions: (64 bits)
 *
 * For cache operations uing TAGLO/TAGHI register, bits 32-63 correspond
 * to TAGHI, 0-31 correspond to TAGLO.
 *
 * Primary Cache Tag:
 *
 *  +------+---+--------+--------+-----+---+---+----+------+-----+
 *  |St Mod|und|T[35:32]|T[31:12]|State|und|LRU|St P| Set  |Tag P|
 *  +------+---+--------+--------+-----+---+---+----+------+-----+
 *   6    6 6 3 3      3 3      0 0   0 0 0  0   0     0      0
 *   3    1 0 6 5      2 1      8 7   6 5 4  3   2     1      0
 *
 */

#define	CTP_TAGPARITY_MASK	0x0000000000000001 /* Tag parity */
#define	CTP_TAGPARITY_SHFT	0
#define	CTP_SCW			0x0000000000000002 /* Secondary cache way */
#define	CTP_STATEPARITY_MASK	0x0000000000000004 /* State parity mask*/
#define CTP_STATEPARITY_SHFT	2		   /* State Parity shift */
#define	CTP_LRU			0x0000000000000008 /* LRU */
#define	CTP_STATE_MASK		0x00000000000000c0 /* state */
#define	CTP_STATE_SHFT		6
#   define	CTP_STATE_I	0	/* Invalid */
#   define	CTP_STATE_S	1	/* shared */
#   define	CTP_STATE_CE	2	/* clean exclusive */
#   define	CTP_STATE_DE	3	/* dirty exclusive */
#define	CTP_STATEMOD_SHFT	61
#define	CTP_STATEMOD_MASK	(0x7L<<CTP_STATEMOD_SHFT)
#   define	CTP_STATEMOD_N	1	/* normal */
#   define	CTP_STATEMOD_I	2	/* inconsistent */
#   define	CTP_STATEMOD_R	4	/* refill */
#define	CTP_TAG_MASK		0x0000000fffffff00 /* TAG */
#define	CTP_TAG_SHFT		8

/* Mask of bits that are valid for icache and dcache tags */

#define	CTP_ICACHE_TAG_MASK	(CTP_TAGPARITY_MASK+CTP_STATEPARITY_MASK+ \
				 CTP_STATEPARITY_MASK+CTP_LRU+CTP_STATE_I+\
				 CTP_TAG_MASK+CTP_STATEMOD_MASK)
#define	CTP_DCACHE_TAG_MASK	(CTP_TAGPARITY_MASK+CTP_STATEPARITY_MASK+ \
				 CTP_STATEPARITY_MASK+CTP_LRU+		  \
				 CTP_STATE_MASK+CTP_TAG_MASK+		  \
				 CTP_STATEMOD_MASK)

#define	CTP_ICACHE_TAGHI_MASK	0x0000000f
#define	CTP_ICACHE_TAGLO_MASK	0xffffff4d

#define	CTP_DCACHE_TAGHI_MASK	0xe000000f
#define	CTP_DCACHE_TAGLO_MASK	0xffffffcf

/*
 * Secondary Cache Tag Definitions: (64 bits)
 *
 * +-----+-----+------------+------------+---+-------+-+----------+-----+
 * | MRU |     | TAG[35:32] | TAG[31:18] |   | State | | Virt Idx | ECC |
 * +-----+-----+------------+------------+---+-------+-+---------+-----+
 *    6   6   3 3          3 3          1 1 1 1     1 0 0        0 0   0
 *    3   2   6 5          2 1          4 3 2 1     0 9 8        7 6   0       
 */

#define	CTS_MRU			0x8000000000000000
#define	CTS_TAG_MASK		0x0000000fffffc000
#define CTS_TAG_SHFT		14
#define	CTS_STATE_MASK		0x0000000000000c00
#define	CTS_STATE_SHFT		10
#   define	CTS_STATE_I	0	/* Invalid */
#   define	CTS_STATE_S	1	/* shared */
#   define	CTS_STATE_CE	2	/* clean exclusive */
#   define	CTS_STATE_DE	3	/* dirty exclusive */
#define	CTS_VIDX_MASK		0x0000000000000180
#define	CTS_VIDX_SHFT		7
#define CTS_ECC_MASK		0x000000000000007f

/* Mask of bits ahat are valid for scache tags */

#define	CTS_MASK		(CTS_MRU+CTS_TAG_MASK+CTS_STATE_MASK+ \
				 CTS_VIDX_MASK+CTS_ECC_MASK)
#define	CTS_TAGHI_MASK		0x8000000f
#define	CTS_TAGLO_MASK		0xffffcdff

/* Cache Error Register */

#define	CE_TYPE_SHFT		30	/* Shift for type select */
#define	CE_TYPE_MASK		(3U<<CE_TYPE_SHFT)
#define	CE_TYPE_I		(0)	/* Instruction cache error */
#define	CE_TYPE_D		(1U<<CE_TYPE_SHFT)
#define	CE_TYPE_S		(2U<<CE_TYPE_SHFT)
#define	CE_TYPE_SIE		(3U<<CE_TYPE_SHFT)

#define	CE_EW			(1<<29)
#define	CE_EE			(1<<28)

/*
 * Cache Error - data array error/ uncorrectable response error, valid for
 * all cache error types.
 */
#define	CE_D_WAY1		(1<<27)
#define	CE_D_WAY0		(1<<26)
#define	CE_D_MASK		(CE_D_WAY1+CE_D_WAY0)

/*
 * Cache Error - Tag array error, valid for I-cache, D-cache and 
 * S-cache errors.
 */
#define	CE_TA_WAY1		(1<<25)
#define	CE_TA_WAY0		(1<<24)
#define	CE_TA_MASK		(CE_TA_WAY1+CE_TA_WAY0)

/*
 * Cache Error - Tag state array error, valid for I-cache and D-cache
 * errors.
 */
#define	CE_TS_WAY1		(1<<23)
#define	CE_TS_WAY0		(1<<22)
#define	CE_TS_MASK		(CE_TS_WAY1+CE_TS_WAY0)

/*
 * Cache Error - Tag mod array error bits, valid only for D-cache errors.
 */
#define	CE_TM_WAY1		(1<<21)
#define	CE_TM_WAY0		(1<<20)
#define	CE_TM_MASK		(CE_TM_WAY1+CE_TM_WAY0)

/*
 * Cache Error - System interface error defines.
 */
#define	CE_SA			(1<<25)
#define	CE_SC			(1<<24)
#define	CE_SR			(1<<23)

/* Cache Error - masks for cache indexes */

#define	CE_SINDX_MASK		(0x007fffc0)
#define	CE_PINDX_MASK		(0x00003ff8)

/*
 * C0_CACHE_ERR definitions.
 */
#define	CACHERR_SRC_MSK		CE_TYPE_MASK
#define	CACHERR_SRC_PI		CE_TYPE_I
#define	CACHERR_SRC_PD		CE_TYPE_D
#define	CACHERR_SRC_SD		CE_TYPE_S
#define	CACHERR_SRC_SYSAD	CE_TYPE_SIE

#define	CACHERR_EW	CE_EW		/* multiple errors from the same
					   source */
#define	CACHERR_EE	CE_EE		/* PDcache: tag error on an inconsistent
						block
					   SYSAD: data error on a clean/dirty
						exclusive line */
#define	CACHERR_D	CE_D_MASK	/* data error, way1|way0 */
#define	CACHERR_TA	CE_TA_MASK	/* tag addr error, way1|way0 */
#define	CACHERR_TS	CE_TS_MASK	/* tag state error, way1|way0 */
#define	CACHERR_TM	CE_TM_MASK	/* PDcache: tag mode error, way1|way0 */
#define	CACHERR_SA	CE_SA		/* uncorrectable SysAd bus error */
#define	CACHERR_SC	CE_SC		/* uncorrectable SysCmd bus error */
#define	CACHERR_SR	CE_SR		/* uncorrectable SysResp bus error */
#define	CACHERR_PIDX_MASK	CE_PINDX_MASK	/* Pcache virtual blk index */
#define	CACHERR_SIDX_MASK	CE_SINDX_MASK	/* Scache physical blk index */

/* target cache */

#define	CACH_PI			0x0	/* Primary instruction cache */
#define	CACH_PD			0x1	/* Primary data cache */
#define	CACH_S			0x3	/* Secondary cache */
#define	CACH_SD			CACH_S	/* Secondary Data cache */
#define	CACH_SI			CACH_S	/* Secondary Inst. cache */

/* Cache operations */

#define	C_IINV			0x00	/* index invalidate (inst) */
#define	C_IWBINV		0x00	/* index writeback inval (d, 2ndary) */
#define	C_ILT			0x04	/* index load tag (all) */
#define	C_IST			0x08	/* index store tag (all) */
#define	C_HINV			0x10	/* hit invalidate (all) */
#define	C_HWBINV		0x14	/* hit writeback inv. (d, s) */
#define	C_BARRIER		0x14	/* cache barrier      (i only)*/
#define	C_ILD			0x18	/* Index load data */
#define	C_ISD			0x1c	/* Index store data */
#define	C_HWB			C_HWBINV /* hit writeback inv. (d, s) */

#if defined(_LANGUAGE_C) 

typedef struct cacheop_s {
    __uint64_t		cop_address;	/* address for operation */
    __uint32_t		cop_operation;	/* operation */
    __uint32_t		cop_taghi;	/* tag hi value */
    __uint32_t		cop_taglo;	/* tag lo value */
    __uint32_t		cop_ecc;	/* ecc register value */
} cacheop_t;

#endif

/*
 * Diagnostic Register (CP0 register 22)
 */
#define	DR_BP_MOD_SHF		16
#define	DR_BP_MOD_MASK		(0x3 << DR_BP_MOD_SHF)
#define	DR_BP_2BITS_COUNTER	(0x0 << DR_BP_MOD_SHF)
#define	DR_BP_NONE_TAKEN	(0x1 << DR_BP_MOD_SHF)
#define	DR_BP_ALL_TAKEN		(0x2 << DR_BP_MOD_SHF)
#define	DR_BP_BACK_TAKEN	(0x3 << DR_BP_MOD_SHF)

#if defined(_LANGUAGE_ASSEMBLY)

/*
 * Macros: ICACHE, DCACHE, SCACHE
 * Purpose: Form "cache" assmebly language instructions
 * Paramters: op - Operation from cache operations above.
 * 	      va/pa - virtual/physical address, in the form "offset(reg)"
 */
#define	ICACHE(op, va)	cache op+CACH_PI, va
#define	DCACHE(op, va)	cache op+CACH_PD, va
#define	SCACHE(op, pa)	cache op+CACH_S, pa

/*
 * Macro: CACHE_BARRIER
 * Purpose: Form cache barrier assembler instruction
 * Parameters: none
 */
#define	CACHE_BARRIER	cache CACH_BARRIER,0(zero)

#endif /* defined(_LANGUAGE_ASSEMBLEY) */

/*
 * IP25 ECC frame has the following layout:
 *
 *       +--------------------------------------------------+
 *       |						    |
 *	 | CACHE_STACK_SIZE - per CPU stack      	    |
 *	 |						    |
 *	 +--------------------------------------------------+
 *	 | ECCF - ecc handler extended frame 		    |
 *	 +--------------------------------------------------+
 *       | EFRAME - standard EFRAME 			    |
 *       +--------------------------------------------------+
 *
 * Total size is: ECCF_SIZE
 */

#define	ECCF_STACK_SIZE	4096		/* Includes stack */
#define	ECCF_TRACE_CNT	16		/* cache error regs to trace */

#if defined(_LANGUAGE_C) 

typedef struct eccframe_s {
    __uint64_t		eccf_errorEPC;	/* Current error EPC */
    __uint64_t		eccf_tag;	/* Current TAG  */
    __uint32_t		eccf_cache_err; /* Current cache error register */
    __uint32_t		eccf_taglo;	/* Current taglo value */
    __uint32_t		eccf_taghi;	/* Current taghi value */
    unsigned short	eccf_ecc;	/* Current ECC value */ 
    unsigned char	eccf_status;	/* current status - see below */
    uint		eccf_icount;	/* icache error count */
    uint		eccf_dcount;	/* dcache error count */
    uint		eccf_scount;	/* scache error count */
    uint		eccf_sicount;	/* system interface count */
    struct {
	__uint32_t	ecct_cer;
	__uint64_t	ecct_tag;	/* tag of affected line */
	__uint64_t	ecct_errepc;
#if EVEREST
	__uint64_t	ecct_rtc;	/* clock - time of exception */
#endif
	}		eccf_trace[ECCF_TRACE_CNT];
    uint		eccf_trace_idx;	/* NEXT index to log into */
    uint		eccf_putbuf_idx;/* NEXT index to read from */
} eccframe_t;

#define	ECCF_ADD(a,b)	(((a) + (b)) % ECCF_TRACE_CNT)

#endif

#define	ECCF_STATUS_NORMAL	0
#define	ECCF_STATUS_ACTIVE	1
#define	ECCF_STATUS_PANIC	2


/*
 * R10000 Configuration Cycle - These define the SYSAD values used 
 * during the reset cycle.
 */

#define	R10000_KSEG0CA_SHFT	0
#define	R10000_KSEG0CA_MASK	(7 << R10000_KSEG0CA_SHFT)
#define	R10000_KSEG0CA(_B) 	((_B) << R10000_KSEG0CA_SHFT)

#define	R10000_DEVNUM_SHFT	3
#define	R10000_DEVNUM_MASK	(0x3 << R10000_DEVNUM_SHFT)
#define	R10000_DEVNUM(_B)	((_B) << R10000_DEVNUM_SHFT)

#define	R10000_CRPT_SHFT	5
#define	R10000_CRPT_MASK	(1<<R10000_CRPT_SHFT)
#define	R10000_CPRT(_B)		((_B)<<R10000_CRPT_SHFT)

#define	R10000_PER_SHFT		6
#define	R10000_PER_MASK		(1 << R10000_PER_SHFT)
#define	R10000_PER(_B)		((_B) << R10000_PER_SHFT)

#define	R10000_PRM_SHFT		7
#define	R10000_PRM_MASK		(3 << R10000_PRM_SHFT)
#define	R10000_PRM(_B)		((_B) << R10000_PRM_SHFT)

#define	R10000_SCD_SHFT		9
#define	R10000_SCD_MASK		(0xf << R10000_SCD_SHFT)
#define	R10000_SCD(_B)		((_B) << R10000_SCD_SHFT)

#define	R10000_SCBS_SHFT	13
#define	R10000_SCBS_MASK	(1<<R10000_SCBS_SHFT)
#define	R10000_SCBS(_B)		(((_B)) << R10000_SCBS_SHFT)

#define	R10000_SCCE_SHFT	14
#define	R10000_SCCE_MASK	(1 << R10000_SCCE_SHFT)
#define	R10000_SCCE(_B)		((_B) << R10000_SCCE_SHFT)

#define	R10000_ME_SHFT		15
#define	R10000_ME_MASK		(1 << R10000_ME_SHFT)
#define	R10000_ME(_B)		((_B) << R10000_ME_SHFT)

#define	R10000_SCS_SHFT		16
#define	R10000_SCS_MASK		(0x7 << R10000_SCS_SHFT)
#define	R10000_SCS(_B)		((_B) << R10000_SCS_SHFT)

#define	R10000_SCCD_SHFT	19
#define	R10000_SCCD_MASK	(0x7 << R10000_SCCD_SHFT)
#define	R10000_SCCD(_B)		((_B) << R10000_SCCD_SHFT)

#define	R10000_SCCT_SHFT	25
#define	R10000_SCCT_MASK	(0xf << R10000_SCCT_SHFT)
#define	R10000_SCCT(_B)		((_B) << R10000_SCCT_SHFT)

#define	R10000_ODSC_SHFT	26
#define R10000_ODSC_MASK	(1 << R10000_ODSC_SHFT)
#define	R10000_ODSC(_B)		((_B) << R10000_ODSC_SHFT)

#define	R10000_ODSYS_SHFT	30
#define	R10000_ODSYS_MASK	(1 << R10000_ODSYS_SHFT)
#define	R10000_ODSYS(_B)	((_B) << R10000_ODSYS_SHFT)

#define	R10000_CTM_SHFT		31
#define	R10000_CTM_MASK		(1 << R10000_CTM_SHFT)
#define	R10000_CTM(_B)		((_B) << R10000_CTM_SHFT)

#endif /* __SYS_R10K_H__ */

