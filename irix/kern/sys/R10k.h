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

#ident "$Revision: 1.23 $"

#if defined(_LANGUAGE_C) 
#include <sys/types.h>			/* needed for cacheop_t */
#endif

/*
 * R10k Config Register defines.
 */
#ifndef R4000
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
#endif /* !R4000 */

/*
 * IP32 kernels support both R5k/R10k. We need to make the
 * name of the R10k #define unique, if it's different from
 * the R4K version.
 */
#define	CONFIG_R10K_IC	0xe0000000	/* primary instruction cache size */
#define	CONFIG_R10K_IC_SHFT	29
#define	CONFIG_R10K_DC	0x1c000000 	/* primary data cache size */
#define	CONFIG_R10K_DC_SHFT	26
#define	CONFIG_R10K_SC	0x00380000 	/* Secondary cache clock ratio */
#define	CONFIG_R10K_SC_SHFT	19
#define	CONFIG_R10K_SS	0x00070000	/* secondary cache size */
#define	CONFIG_R10K_SS_SHFT	16
#define	CONFIG_R10K_SK	0x00004000 	/* Correbcting sec. data ECC err */
#define	CONFIG_R10K_SB	0x00002000	/* Secondary cache block size 16/32 */
#define CONFIG_R10K_SB_SHFT	13
#define	CONFIG_R10K_EC	0x00001e00 	/* System interface clock ratio */
#define	CONFIG_R10K_EC_SHFT	9
#define	CONFIG_R10K_PM	0x00000180	/* Max outstanding proc requests */
#define	CONFIG_R10K_PM_SHFT	7
#define	CONFIG_R10K_PE	0x00000040	/* Enable/Disable  eliminate req */
#define	CONFIG_R10K_CT	0x00000020	/* Target of coherence reqs */
#define	CONFIG_R10K_DN	0x00000018	/* Device number */
#define	CONFIG_R10K_DN_SHFT	3

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
#ifdef IP32
#define	CACHE_SLINE_SIZE	64	/* Secondary cache line size */
#else
#define	CACHE_SLINE_SIZE	128	/* Secondary cache line size */
#endif /* IP32 */
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
#define	CTP_STATEMOD_MASK	(0x7LL<<CTP_STATEMOD_SHFT)
#   define	CTP_STATEMOD_N	1	/* normal */
#   define	CTP_STATEMOD_I	2	/* inconsistent */
#   define	CTP_STATEMOD_R	4	/* refill */
#define	CTP_TAG_MASK		0x0000000fffffff00LL /* TAG */
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
 * | MRU |     | TAG[39:36] | TAG[35:18] |   | State | | Virt Idx | ECC |
 * +-----+-----+------------+------------+---+-------+-+----------+-----+
 *    6   6   3 3          3 3          1 1 1 1     1 0 0        0 0   0
 *    3   2   6 5          2 1          4 3 2 1     0 9 8        7 6   0       
 */

#define	CTS_MRU			0x8000000000000000LL
#define	CTS_TAG_MASK		0x0000000fffffc000LL
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
#ifndef R4000
#define	CACHERR_EE	CE_EE		/* PDcache: tag error on an inconsistent
						block
					   SYSAD: data error on a clean/dirty
						exclusive line */
#else	/* R4000 */
#define	CACHERR_R10K_EE	CE_EE		/* PDcache: tag error on an inconsistent
						block
					   SYSAD: data error on a clean/dirty
						exclusive line */
#endif	/* !R4000 */
#define	CACHERR_D	CE_D_MASK	/* data error, way1|way0 */
#define	CACHERR_TA	CE_TA_MASK	/* tag addr error, way1|way0 */
#define	CACHERR_TS	CE_TS_MASK	/* tag state error, way1|way0 */
#define	CACHERR_TM	CE_TM_MASK	/* PDcache: tag mode error, way1|way0 */
#define	CACHERR_SA	CE_SA		/* uncorrectable SysAd bus error */
#define	CACHERR_SC	CE_SC		/* uncorrectable SysCmd bus error */
#define	CACHERR_SR	CE_SR		/* uncorrectable SysResp bus error */
#ifndef R4000
#define	CACHERR_PIDX_MASK	CE_PINDX_MASK	/* Pcache virtual blk index */
#define	CACHERR_SIDX_MASK	CE_SINDX_MASK	/* Scache physical blk index */
#else	/* R4000 */
#define	CACHERR_R10K_PIDX_MASK	CE_PINDX_MASK	/* Pcache virtual blk index */
#define	CACHERR_R10K_SIDX_MASK	CE_SINDX_MASK	/* Scache physical blk index */
#endif	/* !R4000 */

/* target cache */

#define	CACH_PI			0x0	/* Primary instruction cache */
#define	CACH_PD			0x1	/* Primary data cache */
#define	CACH_S			0x3	/* Secondary cache */
#ifndef R4000
#define	CACH_SD			CACH_S	/* Secondary Data cache */
#endif	/* !R4000 */
#ifdef	R4000
#undef CACH_SI
#endif	/* R4000 */
#define	CACH_SI			CACH_S	/* Secondary Inst. cache */
#define CACH_BARRIER		0x14	/* Cache barrier operation */

/* Cache operations */

#ifndef R4000
#define	C_IINV			0x00	/* index invalidate (inst) */
#define	C_IWBINV		0x00	/* index writeback inval (d, 2ndary) */
#define	C_ILT			0x04	/* index load tag (all) */
#define	C_IST			0x08	/* index store tag (all) */
#define	C_HINV			0x10	/* hit invalidate (all) */
#define	C_HWBINV		0x14	/* hit writeback inv. (d, s) */
#endif	/* !R4000 */
#define	C_ILD			0x18	/* Index load data */
#define	C_ISD			0x1c	/* Index store data */
#ifndef R4000
#define	C_HWB			C_HWBINV /* hit writeback inv. (d, s) */
#endif	/* !R4000 */

#define PCACHE_WAYS		2
#define SCACHE_WAYS 		2

#define R10K_DCACHE_LINES	\
                (R10K_MAXPCACHESIZE / (CACHE_DLINE_SIZE * PCACHE_WAYS))
#define R10K_ICACHE_LINES	\
                (R10K_MAXPCACHESIZE / (CACHE_ILINE_SIZE * PCACHE_WAYS))

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


/*
 * physical address from cache index and tag.
 */

#define SCACHE_ERROR_ADDR(_cer, _tag) \
      (((_cer) & CE_SINDX_MASK) | (((_tag) & CTS_TAG_MASK) << 4))

#define PCACHE_ERROR_ADDR(_cer, _tag) \
      (((_cer) & CE_PINDX_MASK) | (((_tag) & CTP_TAG_MASK) << 4))

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
    __uint64_t		eccf_tag[2];	/* Current TAG  */
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
	__uint64_t	ecct_tag[2];	/* tag of affected line, each way */
	__uint64_t	ecct_errepc;
#if defined (EVEREST) || defined (SN0)
	__uint64_t	ecct_rtc;	/* clock - time of exception */
#endif /* EVEREST || SN0 */
	}		eccf_trace[ECCF_TRACE_CNT];
    uint		eccf_trace_idx;	/* NEXT index to log into */
    uint		eccf_putbuf_idx;/* NEXT index to read from */
    __uint64_t		ecct_recover_rtc;
    ushort		ecct_recover_icache_count;
    ushort		ecct_recover_dcache_count;
    ushort		ecct_recover_scache_count;
    ushort		ecct_recover_sie_count;
} eccframe_t;


#if defined (SN)

typedef unsigned short  __uint16_t;


#define IL_ENTRIES              (CACHE_ILINE_SIZE/sizeof(__uint32_t))
#define IL_ENTRIES              (CACHE_ILINE_SIZE/sizeof(__uint32_t))
#define ICACHE_ADDR(line, way)  (((line) * CACHE_ILINE_SIZE) + K0BASE + (way))

#define DL_ENTRIES              CACHE_DLINE_SIZE / sizeof (__uint32_t)
#define DCACHE_ADDR(line, way)  (((line) * CACHE_DLINE_SIZE) + K0BASE + (way))

#define SL_ENTRIES              (CACHE_SLINE_SIZE / (2 * sizeof (__uint64_t)))
#define SCACHE_ADDR(line, way)  (((line) * CACHE_SLINE_SIZE) + K0BASE + (way))

#define R10K_ICACHESIZE         0x008000
#define R10K_ICACHELINES        (R10K_ICACHESIZE / CACHE_ILINE_SIZE)
#define R10K_DCACHESIZE         0x008000
#define R10K_DCACHELINES        (R10K_DCACHESIZE / CACHE_DLINE_SIZE)
#define R10K_SCACHELINES        (sCacheSize()    / CACHE_SLINE_SIZE)

#define	R10K_CACHERR_PIC	0x0
#define	R10K_CACHERR_PDC	0x1
#define	R10K_CACHERR_SC		0x2
#define R10K_CACHERR_SYSAD	0x3


typedef union r10k_conf_u {
        __uint32_t val;
        struct {
                __uint32_t      ICSize          :3, /* <31:29>  */
                                DCSize          :3, /* <28:26>  */
                                rsv1            :4, /* <25:22>  */
                                SCClkDiv        :3, /* <21:19>  */
                                SCSize          :3, /* <18:16>  */
                                MemEnd          :1, /* <15>     */
                                SCCorEn         :1, /* <14>     */
                                SCBlkSize       :1, /* <13>     */
                                SySClKDiv       :4, /* <12:9>   */
                                PrcReqMax       :2, /* <8:7>    */
                                PrcElMReq       :1, /* <6>      */
                                CohPrcReqTar    :1, /* <5>      */
                                DevNr           :2, /* <4:3>    */
                                CacheAlg        :3; /* <2:0>    */
                } cnf;
} r10k_conf_t;


typedef union r10k_cacherr_syndrome {
	__uint32_t val;
	struct {
		__uint32_t	syndrome	: 9, /* <31:23> */
				rsv 		:23;
	} x;
} r10k_cacherr_syndrome_t;


typedef union r10k_cacherr {
	__uint32_t val;

	struct {
		__uint32_t	err		 : 2,/* <31:30> */
				rsv		 :30;/* <29:0>  */
	} error_type;

	
	/*
	 * primary instuction cache error. MIPS User's Manual Page 252
	 */
	struct { 
		__uint32_t 	errtype		 : 2,/* <31:30> */
				ew		 : 1,/* <29>	*/
				rsv1		 : 1,/* <28>	*/
				data_array_err	 : 2,/* <27:26> */
				tag_addr_err     : 2,/* <25:24> */
				tag_state_err    : 2,/* <23:22> */
				rsv2		 : 8,/* <21:14> */
				pidx		 : 8,/* <13:6>  */
				rsv3		 : 6;/* <5:0>	*/
	} pic;
				
	/*
	 * primary data cache error. MIPS User's Manual Page 253
	 */
	struct { 
		__uint32_t 	errtype		 : 2,/* <31:30> */
				ew		 : 1,/* <29>	*/
				ee		 : 1,/* <28>	*/
				data_array_err	 : 2,/* <27:26> */
				tag_addr_err     : 2,/* <25:24> */
				tag_state_err    : 2,/* <23:22> */
				tag_mod_array_err: 2,/* <21:20> */
				rsv1		 : 6,/* <19:14> */
				pidx		 :11,/* <13:3>  */
				rsv2		 : 3;/* <2:0>	*/
	} pdc;	

	/*
	 * secondary cache error. MIPS User's Manual Page 253
	 */
	struct { 
		__uint32_t 	errtype		 : 2,/* <31:30> */
				ew		 : 1,/* <29>	*/
				rsv1		 : 1,/* <28>	*/
				data_array_err	 : 2,/* <27:26> */
				tag_array_err    : 2,/* <25:24> */
				rsv2		 : 1,/* <23>	*/
				sidx		 :17,/* <22:6>  */
				rsv3		 : 6;/* <5:0>	*/
	} sc;
} r10k_cacherr_t;
			

typedef union il_tag_u {
        __uint64_t v;
        struct {
                __uint64_t      rsv4    :28,    /* TagHi [ 4:31] */
                                                /* TagHi [ 3: 0] */
                                tag     :28,    /* TagLo [31: 8] */
                                rsv3    : 1,    /* TagLo [ 7   ] */
                                state   : 1,    /* TagLo [ 6   ] */
                                rsv2    : 2,    /* TagLo [ 4: 2] */
                                lru     : 1,    /* TagLo [ 3   ] */
                                statepar: 1,    /* TagLo [ 2   ] */
                                rsv1    : 1,    /* TagLo [ 1   ] */
                                tagpar  : 1;    /* TagLo [ 0   ] */
                } t;
} il_tag_t;


typedef union dl_tag_u {
        __uint64_t v;
        struct {
                __uint64_t      statemod: 3,    /* TagHi [31:29] */
                                rsv2    :25,    /* TagHi [ 4:28] */
                                                /* TagHi [ 3: 0] */
                                tag     :28,    /* TagLo [31: 8] */
                                state   : 2,    /* TagLo [ 7: 6] */
                                rsv1    : 2,    /* TagLo [ 4: 4] */
                                lru     : 1,    /* TagLo [ 3   ] */
                                statepar: 1,    /* TagLo [ 2   ] */
                                scway   : 1,    /* TagLo [ 1   ] */
                                tagpar  : 1;    /* TagLo [ 0   ] */
                } t;
} dl_tag_t;

typedef union sl_tag_u {
        __uint64_t v;
        struct {
                __uint64_t      mru     : 1,    /* TagHi [31   ] */
                                rsv3    :27,    /* TagHi [ 4:30] */
                                                /* TagHi [ 3: 0] */
                                tag     :22,    /* LagLo [31:14] */
                                rsv2    : 2,    /* TagLo [12:13] */
                                state   : 2,    /* TagLo [11:10] */
                                rsv1    : 1,    /* TagLo [ 9   ] */
                                virtind : 2,    /* TagLo [ 8: 7] */
                                ecc     : 7;    /* TagLo [ 6: 0] */
        } t;
} sl_tag_t;



typedef union sl_data_ecc_u {
        __uint16_t v;
        struct {
                __uint16_t      rsv     :6,
                                parity  :1,
                                ecc     :9;
        } t;
} sl_data_ecc_t;




typedef struct r10k_il_s {
        il_tag_t        il_tag;
        __uint64_t      il_data  [IL_ENTRIES];
        unsigned char   il_parity[IL_ENTRIES];
} r10k_il_t;

typedef struct r10k_dl_s {
        dl_tag_t        dl_tag;
        __uint32_t      dl_data  [DL_ENTRIES];
        unsigned char   dl_parity[DL_ENTRIES];

} r10k_dl_t;


typedef struct r10k_sl_s {
        sl_tag_t        sl_tag;
        __uint64_t      sl_data[SL_ENTRIES * 2];        /* 128 bit */
        sl_data_ecc_t   sl_ecc [SL_ENTRIES    ];
} r10k_sl_t;


/*
 * 	ECC exception frame for SN. It is located in the nodes UALIAS
 *	space. write access for remote nodes is disabled after the boot
 */
typedef struct sn_eccframe_s {

	__uint32_t	sn_eccf_errorepc;
	__uint32_t	sn_eccf_taghi;
	__uint32_t	sn_eccf_taglo;
	r10k_cacherr_t 	sn_eccf_cache_err;
	__uint32_t 	sn_eccf_fail_cpu;
	__uint32_t 	sn_eccf_hand_cpu;

	__uint64_t	sn_ecct_rtc;		/* clock - time of exception */

	/*
	 * the following counters are indexed by
 	 *	R10K_CACHERR_PIC   
         *      R10K_CACHERR_PDC   
         *  	R10K_CACHERR_SC    
         *	R10K_CACHERR_SYSAD 
	 */
	__uint32_t	sn_eccf_cnt        [4];	/* total counts		     */
	__uint32_t	sn_eccf_cnt_recov_i[4];	/* recovered by invalidating */
						/* cache line contents	     */
	__uint32_t	sn_eccf_cnt_recov_k[4]; /* recovered by killing      */
						/* application               */
					

	il_tag_t	sn_eccf_il_tag;
	dl_tag_t	sn_eccf_dl_tag;
	sl_tag_t	sn_eccf_sl_tag;
				

	pid_t		sn_eccf_pid;		/* PID causing this error    */
	__uint64_t	sn_eccf_addr;

	/*
	 * pointer to data which can be modified from the failing
	 * as well the handling CPU. Since our ECC frame is write-protected
	 * for remote notes, we need to use data in the PDA
	 */
	__uint64_t	*sn_eccf_flags;

} sn_eccframe_t;



#endif /* SN */

#define	ECCF_ADD(a,b)	(((a) + (b)) % ECCF_TRACE_CNT)

#define ECCF_RECOVERABLE_THRESHOLD(_ef, _eccf, _cer) \
                                   eccf_recoverable_threshold(_ef, _eccf, _cer)

#define CERR_RECOVER_TIME	60000000	/* 1 minute */
#define CERR_RECOVER_COUNT      100

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

#if defined (_LANGUAGE_C)


struct sc_data {
	__uint64_t	sc_data[2];
	ushort		sc_cb;
	ushort		sc_ecc;
	ushort		sc_syn;
};
struct pic_data {
	__uint64_t	pic_data;
	ushort		pic_cb;
	ushort		pic_ecc;
	ushort		pic_syn;
};

struct pdc_data {
	__uint32_t	pdc_data;
	ushort		pdc_cb;
	ushort		pdc_ecc;
	ushort		pdc_syn;
};

#define SCACHE_LINE_FRAGMENTS	8
#define PICACHE_LINE_FRAGMENTS	16
#define PDCACHE_LINE_FRAGMENTS	8

typedef struct t5_cache_line {
	__uint64_t	c_addr;
	__uint64_t	c_tag;
	union {
		struct sc_data  sc_bits[SCACHE_LINE_FRAGMENTS];
		struct pic_data pic_bits[PICACHE_LINE_FRAGMENTS];
		struct pdc_data pdc_bits[PDCACHE_LINE_FRAGMENTS];
	} c_data;
	unsigned char	c_state;
	unsigned char	c_way;
	unsigned char	c_type;
	uint		c_idx;
} t5_cache_line_t;

void ecc_scache_dump(void (*prf)(char *, ...), int);
void ecc_scache_line_dump(__psunsigned_t, int, void (*prf)(char *, ...));
void ecc_picache_dump(void (*prf)(char *, ...), int);
void ecc_picache_line_dump(__psunsigned_t, int, void (*prf)(char *, ...));
void ecc_pdcache_dump(void (*prf)(char *, ...), int);
void ecc_pdcache_line_dump(__psunsigned_t, int, void (*prf)(char *, ...));
int ecc_pdcache_line_check_syndrome(__psunsigned_t k0addr, int way);
int ecc_picache_line_check_syndrome(__psunsigned_t k0addr, int way);
int ecc_scache_line_check_syndrome(__psunsigned_t k0addr, int way);

void	ecc_cacheop_get(__uint32_t, __psunsigned_t, cacheop_t *);
int	ecc_bitcount(__uint64_t word);
#endif /* _LANGUAGE_C */

/*
 * Secondary Cache ECC Matrix for data array
 */

#define E9_8H 0xFF03FFE306FF0721LL
#define E9_8L 0x62D0B0D000202080LL

#define E9_7H 0xFFFF030FFF0703B2LL
#define E9_7L 0x100848C802909020LL

#define E9_6H 0x83FFFF030503FF5CLL
#define E9_6L 0x808404C438484850LL

#define E9_5H 0x42808012FCFBFC3FLL
#define E9_5L 0xFD6202228504C408LL

#define E9_4H 0x214043FF808084FFLL
#define E9_4L 0xFF210101FFC20284LL

#define E9_3H 0x10232081444046BFLL
#define E9_3L 0xFC3FDF3F4C010142LL

#define E9_2H 0x0A12121823202100LL
#define E9_2L 0xBAFFC0A0D0FFFFC1LL

#define E9_1H 0x0409094413121008LL
#define E9_1L 0x4DC0E0FFE0C0FFFFLL

#define E9_0H 0x010404200B0D0B47LL
#define E9_0L 0x04E0FF60C3FFC0FFLL


/*
 * Secondary cache ECC array for tag
 */ 

#define E7_6W_ST 0x0A8F888
#define E7_5W_ST 0x114FF04
#define E7_4W_ST 0x2620F42
#define E7_3W_ST 0x29184F0
#define E7_2W_ST 0x10A40FF
#define E7_1W_ST 0x245222F
#define E7_0W_ST 0x1FF1111


#define SCDATA_ECC_BITS 9
#define SCDATA_BITS 64
#define SC_ECC_PARITY_MASK	0x200

#define SCTAG_ECC_BITS 7
#define SCTAG_BITS  26		/* 22 bits for stag, 2 each for state and 
				 * pidx
				 */
#define SCTAG_MASK	((1 << SCTAG_BITS) - 1)

/*
 * R10k workaround bits.
 */
#define R10K_MFHI_WAR_DISABLE		0x1
#endif /* __SYS_R10K_H__ */

