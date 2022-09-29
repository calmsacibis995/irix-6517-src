
/******************************************************************************
 *
 *		CP0 Definitions
 *
 * $Header: /proj/irix6.5.7m/isms/stand/arcs/IP19prom/niblet/RCS/cp0_r4k.h,v 1.1 1992/06/17 18:21:40 stever Exp $
 *
 * Author	anoosh	Hosseini + DVT
 * Date started Wed July 10 15:37:59 PDT 1985
 * Purpose	provide a set of standard constants and macros for all avts
 * Revisions
 *
 * 	Copied from r3000 and ported to support R3000/R4000	a.h.  9/11/89
 *
 *	Only "ifdef" variable is now R4000,modified by Glenn Giacalone, 9/26/89
 *
 ******************************************************************************/


/*
 * Coprocessor 0 registers
 */

#define	C0_INX		$0
#define	C0_RAND		$1

/* R3000 */
#define	C0_TLBLO	$2

/* R4000 */
#define C0_ENLO0	$2
#define C0_ENLO1	$3

#define	C0_CTXT		$4

/* R4000 */
#define PAGEMASK	$5
#define C0_PAGEMASK	$5
#define WIRED		$6
#define C0_WIRED	$6
#define C0_ERROR	$7
#define	C0_BADVADDR	$8

/* R4000 */
#define C0_COUNT	$9

#define	C0_TLBHI	$10
#define	C0_ENHI		$10

#define COMPARE		$11
#define C0_COMPARE	$11

#define	C0_SR		$12
#define	C0_CAUSE	$13
#define	C0_EPC		$14
#define C0_PRID		$15

#define CONFIG		$16
#define C0_CONFIG	$16
#define C0_LLADDR	$17
#define C0_WATCH_LO	$18
#define C0_WATCH_HI	$19
#define XCONTEXT	$20
#define C0_XCONTEXT	$20

#define C0_ECC          $26
#define C0_CACHE_ERR	$27
#define C0_TAGLO	$28
#define C0_TAGHI	$29
#define ERROR_EPC	$30

/* Cause register exception codes */

#define	EXC_CODE(x)	((x)<<2)

/* Hardware exception codes */
#define	EXC_INT		EXC_CODE(0)	/* interrupt */
#define	EXC_MOD		EXC_CODE(1)	/* TLB mod */

#define	EXC_RMISS	EXC_CODE(2)	/* Read TLB Miss (load/I fetch) */
#define	EXC_TLBL	EXC_CODE(2)	/* Mnemonic compatible */

#define	EXC_WMISS	EXC_CODE(3)	/* Write TLB Miss (store) */
#define	EXC_TLBS	EXC_CODE(3)	/* Mnemonic compatible */

#define	EXC_RADE	EXC_CODE(4)	/* Read Address Error */
#define	EXC_ADEL	EXC_CODE(4)	/* Mnemonic compatible */

#define	EXC_WADE	EXC_CODE(5)	/* Write Address Error */
#define	EXC_ADES	EXC_CODE(5)	/* Mnemonic compatible */

#define	EXC_IBE		EXC_CODE(6)	/* Instruction Bus Error */
#define	EXC_DBE		EXC_CODE(7)	/* Data Bus Error */

#define	EXC_SYSCALL	EXC_CODE(8)	/* SYSCALL */
#define	EXC_SYS		EXC_CODE(8)	/* Mnemonic compatible */

#define	EXC_BREAK	EXC_CODE(9)	/* BREAKpoint */
#define	EXC_BP		EXC_CODE(9)	/* Mnemonic compatible */

#define	EXC_II		EXC_CODE(10)	/* Illegal Instruction */
#define	EXC_RI		EXC_CODE(10)	/* Mnemonic compatible */

#define	EXC_CPU		EXC_CODE(11)	/* CoProcessor Unusable */
#define	EXC_OV		EXC_CODE(12)	/* OVerflow */

#define	EXC_TRAP	EXC_CODE(13)	/* trap instruction */

#define EXC_VCEI	EXC_CODE(14)	/* instruction virtual coherency exception */

/* R4000 */
#define EXC_FPE		EXC_CODE(15)	/* floating point exception */
#define EXC_WATCH	EXC_CODE(23)	/* watch excecption */
#define EXC_VCED	EXC_CODE(31)	/* data virtual coherency exception */


/* software exception codes */
#define	SEXC_SEGV	EXC_CODE(16)	/* Software detected seg viol */
#define	SEXC_RESCHED	EXC_CODE(17)	/* resched request */
#define	SEXC_PAGEIN	EXC_CODE(18)	/* page-in request */
#define	SEXC_CPU	EXC_CODE(19)	/* coprocessor unusable */


/**********************************************
 * TLB LO entry 
 **********************************************/
#if R4000

#define	TLBLO_PFNMASK	0x3fffffc0
#define	TLBLO_DMASK	0x4		/* writeable */
#define	TLBLO_VMASK	0x2		/* valid bit */
#define	TLBLO_GMASK	0x1		/* global bit */

/* 
 * Position of PFN is address in ENLO is different
 * in ENLO and PA.  Therefore, there is a different
 * mask and an extra shift.
 */
#define	PFNMASK32		0xfffff000
#define	PFNMASK32_KSEG01	0x1ffff000
#define	PFNMASK64	0xffffff000
#define	PFNSHIFT	6

#define	TLBLO_PFNSHIFT	6
#define	TLBLO_CSHIFT	3
#define	TLBLO_DSHIFT	2
#define	TLBLO_VSHIFT	1

#define TLBLO_CACHEMASK	0x70		/* cache bits */

#define TLBLO_CACHE_NCOHRNT	 0
#define	TLBLO_CACHE_COHRNT_EX_RW 1
#define	TLBLO_CACHE_COHRNT_EX_WR 2
#define	TLBLO_CACHE_COHRNT_UP_WR 3
#define TLBLO_NONCACHEABLE	 4

#define	NTLBENTRIES	48

#else /* R3000 */

#define	TLBLO_PFNMASK	0xfffff000
#define	TLBLO_PFNSHIFT	12
#define	TLBLO_N		0x800		/* non-cacheable */
#define	TLBLO_D		0x400		/* writeable */
#define	TLBLO_V		0x200		/* valid bit */
#define	TLBLO_G		0x100		/* global access bit */
#define	NTLBENTRIES	64		/* WAG for now */

#endif /* R4000 */


/*
 * TLB size constants
 */
#define	TLBWIREDBASE	0		/* WAG for now */
#define	NWIREDENTRIES	8		/* WAG for now */
#define	TLBRANDOMBASE	NWIREDENTRIES
#define	NRANDOMENTRIES	(NTLBENTRIES-NWIREDENTRIES)


/**********************************************
 * TLB HI entry 
 **********************************************/

#if R4000

#define	TLBHI_RSHIFT	53
#define	TLBHI_VPNSHIFT	13

#define TLBHI_RMASK	0xe0000000000000
#define TLBHI_VPNMASK	0xffffffe000
#define	TLBHI_ASIDMASK	0xff
#define TLBHI_PID	0xff	/* Process ID (R3000 naming convention) */

/* region definitions */
#define TLBHI_USER	 0x00000000000000
#define TLBHI_SUPERVISOR 0x20000000000000
#define TLBHI_KERNEL	 0x40000000000000

#else /* R3000 */

#define	TLBHI_VPNMASK	0xfffff000
#define	TLBHI_VPNSHIFT	12
#define	TLBHI_PIDMASK	0xfc0
#define	TLBHI_PIDSHIFT	6
#define	TLBHI_NPID	64

#endif /* R4000 */

/******************************************************
 * TLB Index register
 ******************************************************/

#define	TLBINX_PROBE		0x80000000

#if R4000

#define	TLBINX_INXMASK		0x3f
#define	TLBINX_INXSHIFT		0

#else /* R3000 */

#define	TLBINX_INXMASK		0x3f00
#define	TLBINX_INXSHIFT		8

#endif /* R4000 */


/******************************************************
 * random register
 ******************************************************/

#if R4000

#define	TLBRAND_RANDMASK	0x3f
#define	TLBRAND_RANDSHIFT	0

#else /* R3000 */

#define	TLBRAND_RANDMASK	0x3f00
#define	TLBRAND_RANDSHIFT	8

#endif /* R4000 */

/******************************************************
 * Context  register
 ******************************************************/

#if R4000

/*** This is wrong and must be changed ****
 * #define	TLBCTXT_BASEMASK	0xffffffffffe00000
 */
#define	TLBCTXT_BASESHIFT	23
#define TLBCTXT_BADVPN2		0x7ffff0

#else /* R3000 */

#define	TLBCTXT_BASEMASK	0xffe00000
#define	TLBCTXT_BASESHIFT	21

#endif /* R4000 */


/******************************************************
 * XContext  register-  R4000 ONLY!!
 ******************************************************/

#define TLBXCTXT_BADVPN2	0x3ffffff8
#define TLBXCTXT_REGIONMASK	0xc0000000
#define TLBXCTXT_PTEBASEMASK	0xffffffff00000000
#define TLBXCTXT_PTEBASESHFT	32

/******************************************************
 * config  register
 ******************************************************/

#define CONFIG_E		0x00000001	/* endian: 0=> little */
#define	CONFIG_IC		0x00000004	/* instruction cache size */	
#define	CONFIG_DC		0x000000e0	/* data cache size */	
#define	CONFIG_SC		0x00000780	/* secondary cache size */	
#define CONFIG_IB		0x00000800	/* I block size */
#define CONFIG_DB		0x00000010	/* D block size */
#define CONFIG_SB		0x00c00000	/* second cache Block size */
#define CONFIG_SB4		0x00000000	/* second cache Block size 4 words */
#define CONFIG_SB8		0x00400000	/* second cache Block size 8 words */
#define CONFIG_SB16		0x00800000	/* second cache Block size 16 words */
#define CONFIG_SB32		0x00c00000	/* second cache Block size 32 words */
#define CONFIG_SW		0x00008000	/* second cache port width */
#define CONFIG_ST		0x00070000	/* second cache access time */
#define CONFIG_MW		0x00080000	/* memory port width  */
#define CONFIG_MT		0x00700000	/* memory port clock  */
#define CONFIG_KX		0x00800000	/* kernel 64 bit enable  */
#define CONFIG_KSEG0		0x00000007	/* mask for CohKseg0 bits */

#define	CONFIG_SBSHIFT	22


/******************************************************
 * Status register
 ******************************************************/

#define	SR_CUMASK	0xf0000000	/* coproc usable bits */

#define	SR_CU3		0x80000000	/* Coprocessor 3 usable */
#define	SR_CU2		0x40000000	/* Coprocessor 2 usable */
#define	SR_CU1		0x20000000	/* Coprocessor 1 usable */
#define	SR_CU0		0x10000000	/* Coprocessor 0 usable */

/* Cache control bits */

/* R4000 */
#define SR_DSMASK	0x01ff0000	/* mask for DS bits */
#define	SR_BEV		0x00400000	/* Exceptions in boot region */
#define	SR_TS		0x00200000	/* TLB shutdown */
#define	SR_SR		0x00100000	/* soft reset */
                                        /*  not used */
#define	SR_CH		0x00040000	/* last cache load tag hit or miss */
#define	SR_CE		0x00020000	/* ECC register used to modify bits */
#define	SR_DE		0x00010000	/* parity or ECC to cause exceptions?  */


/*
 * Interrupt enable bits
 * (NOTE: bits set to 1 enable the corresponding level interrupt)
 */
#define	SR_IMASK	0xff00	/* Interrupt mask */
#define	SR_IMASK8	0x0000	/* mask level 8 */
#define	SR_IMASK7	0x8000	/* mask level 7 */
#define	SR_IMASK6	0xc000	/* mask level 6 */
#define	SR_IMASK5	0xe000	/* mask level 5 */
#define	SR_IMASK4	0xf000	/* mask level 4 */
#define	SR_IMASK3	0xf800	/* mask level 3 */
#define	SR_IMASK2	0xfc00	/* mask level 2 */
#define	SR_IMASK1	0xfe00	/* mask level 1 */
#define	SR_IMASK0	0xff00	/* mask level 0 */

#define	SR_IBIT8	0x8000	/* bit level 8 */
#define	SR_IBIT7	0x4000	/* bit level 7 */
#define	SR_IBIT6	0x2000	/* bit level 6 */
#define	SR_IBIT5	0x1000	/* bit level 5 */
#define	SR_IBIT4	0x0800	/* bit level 4 */
#define	SR_IBIT3	0x0400	/* bit level 3 */
#define	SR_IBIT2	0x0200	/* bit level 2 */
#define	SR_IBIT1	0x0100	/* bit level 1 */

/* user space/kernel space stack- R3000 */

#define	SR_KUO		0x0020	/* old kernel/user, 0 => k, 1 => u */
#define	SR_IEO		0x0010	/* old interrupt enable, 1 => enable */
#define	SR_KUP		0x0008	/* prev kernel/user, 0 => k, 1 => u */
#define	SR_IEP		0x0004	/* prev interrupt enable, 1 => enable */
#define	SR_KUC		0x0002	/* cur kernel/user, 0 => k, 1 => u */
#define	SR_IEC		0x0001	/* cur interrupt enable, 1 => enable */

/* R4000 */
#define SR_IE		0x001	/* interrupts enable 0 => disable */
#define SR_EXL		0x002	/* exception level 0=> normal 1=> exception */
#define SR_ERL		0x004	/* error level */
#define SR_KSUMASK	0x018	/* mode user,supervisor, or kernel */
#define SR_USER		0x010	/* user state */
#define SR_SUPERVISOR	0x008	/* supervisor state */
#define SR_KERNEL	0x000	/* kernel state */
#define SR_KX		0x080	/* kernel extended addressing */
#define SR_SX		0x040	/* supervisor extended addressing*/
#define SR_UX		0x020	/* user extended addressing*/

#define SR_FR		0x04000000 /* FR bit */
#define SR_RE		0x02000000 /* reverse endian in user mode */

/* R3000 */
#define	SR_IMASKSHIFT	8
#define	SR_FMT		"\20\40BD\26TS\25PE\24CM\23PZ\22SwC\21IsC\20IM7\17IM6\16IM5\15IM4\14IM3\13IM2\12IM1\11IM0\6KUo\5IEo\4KUp\3IEp\2KUc\1IEc"


/***************************************************
 * Cause Register
 **************************************************/

#define	CAUSE_BD	0x80000000	/* Branch delay slot */
#define	CAUSE_CEMASK	0x30000000	/* coprocessor error */
#define	CAUSE_CEMASKB	0xcfffffff
#define CAUSE_CE0	0x00000000
#define CAUSE_CE1	0x10000000
#define CAUSE_CE2	0x20000000
#define CAUSE_CE3	0x30000000

#define	CAUSE_CESHIFT	28

/* Interrupt pending bits */
#define	CAUSE_IP8	0x8000	/* External level 8 pending */
#define	CAUSE_IP7	0x4000	/* External level 7 pending */
#define	CAUSE_IP6	0x2000	/* External level 6 pending */
#define	CAUSE_IP5	0x1000	/* External level 5 pending */
#define	CAUSE_IP4	0x0800	/* External level 4 pending */
#define	CAUSE_IP3	0x0400	/* External level 3 pending */
#define	CAUSE_SW2	0x0200	/* Software level 2 pending */
#define	CAUSE_SW1	0x0100	/* Software level 1 pending */

#define	CAUSE_IPMASK	0xFF00	/* Pending interrupt mask */
#define	CAUSE_IPSHIFT	8

#define	CAUSE_EXCMASK	0x007C	/* Cause code bits */
#define	CAUSE_EXCSHIFT	2

/**************************************************
 * ECC Register - R4000 Only
 **************************************************/

#define ECC_SHIFT       24


/**************************************************
 * Cache Err register- R4000 Only
 **************************************************/

#define CACHE_ERR_ER    0x80000000      /* mask bit positions */
#define CACHE_ERR_EC    0x40000000
#define CACHE_ERR_ED    0x20000000
#define CACHE_ERR_ET    0x10000000
#define CACHE_ERR_ES    0x08000000
#define CACHE_ERR_EE    0x04000000
#define CACHE_ERR_EB    0x02000000
#define CACHE_ERR_EI    0x01000000
#define CACHE_ERR_SIDX  0x003ffff8
#define CACHE_ERR_PIDX  0x00000007

/*************************************************
 * TagLo Register, R4000 Only
 *************************************************/

#define TAGLO_STAGLO       0xffffe000
#define TAGLO_STAGLO_SHIFT 0xd
#define TAGLO_SSTATE       0x00001c00
#define TAGLO_SSTATE_SHIFT 0xa
#define TAGLO_VINDEX       0x00000380
#define TAGLO_VINDEX_SHIFT 0x7
#define TAGLO_ECC          0x0000007f

#define TAGLO_PTAGLO       0xffffff00
#define TAGLO_PTAGLO_SHIFT 0x8
#define TAGLO_PSTATE       0x000000c0
#define TAGLO_PSTATE_SHIFT 0x6
#define TAGLO_P            0x00000001


/**************************************************
 * Page Mask register- R4000 Only!!
 **************************************************/


#define PM_MASK		0x01fff000	/* mask bit positions */
#define PM_SHIFT	0xc		/* shift amount for mask */

#define PM_4K		0x00000000
#define PM_16K		0x00006000
#define PM_64K		0x0001e000
#define PM_256K		0x0007e000
#define PM_1M		0x001fe000
#define PM_4M		0x007fe000
#define PM_16M		0x01ffe000


/**************************************************
 * Offset masks for all page sizes
 **************************************************/
#define	OFFSET4K	0x00000fff
#define	OFFSET16K	0x00003fff
#define	OFFSET64K	0x0000ffff
#define	OFFSET256K	0x0003ffff
#define	OFFSET1M	0x000fffff
#define	OFFSET4M	0x003fffff
#define	OFFSET16M	0x00ffffff

/**************************************************
 * TLB WIRED register
 **************************************************/

#define WIRED_MASK	0x3f

/**************************************************
 * WATCH register bits
 **************************************************/
#define	WATCH_READS	0x2
#define	WATCH_WRITES	0x1

/**************************************************
 * Coprocessor 0 operations
 **************************************************/

#define	C0_READI  0x1		/* read ITLB entry addressed by C0_INDEX */
#define	C0_WRITEI 0x2		/* write ITLB entry addressed by C0_INDEX */
#define	C0_WRITER 0x6		/* write ITLB entry addressed by C0_RAND */
#define	C0_PROBE  0x8		/* probe for ITLB entry addressed by TLBHI */
#define	C0_RFE	  0x10		/* restore for exception */

/******************************************************************************
 *    C O N S T A N T S
 ******************************************************************************/

/* entryhi register */
#define TLBHI_MASK (TLBHI_VPNMASK | TLBHI_PIDMASK)	/* Vaild bits */

/* entry lo register */
#define TLBLO_MASK (TLBLO_PFNMASK | TLBLO_N | TLBLO_D | TLBLO_G | TLBLO_V)

/* tlb index register */
#define TLBINDX_MASK (TLBINX_PROBE | TLBINX_INXMASK)


/* Context register */
#define CTXT_MASK TLBCTXT_BASEMASK

/* Some of the diags use unused bits of the cause register to pass information
 *   these bits are defined below
 */
#define DIAG_RETURN_EPC4	0x10000		/* Return to epc + 4 on exception */
#define DIAG_RETURN_EPC4_BD 0x20000	/* Return via epc + 4 if not in a 
					 *  branch delay slot */
#define DIAG_BUMP_EPC 0x40000	/* Increment expected epc each time thru */


/*******************************************************************************
 *   M  A C R O S
 ******************************************************************************/

#if R4000

#define ENTRY_HI_32(vpn, asid)	\
  .word		((vpn & 0x7ffff) << TLBHI_VPNSHIFT) | (asid & 0xff);


#define ENTRY_LO_32(pfn, c, d, v, g)	\
  .word		((pfn << TLBLO_PFNSHIFT) | (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g)

/*
 * MT_ENHI
 *	Create an Entry Hi >>> for now an m4 macro in r4000.m4!!
 */

/*
 * MT_ENLO0:
 *	Create an Entry Lo from the label, c, d, v, and g given for a 32b address.
 */
#define	MT_ENLO0(label, c, d, v, g)						\
		la	r2, label;						\
		li	r3, PFNMASK64;						\
		and	r2, r2, r3;						\
		srl	r2, r2, PFNSHIFT;					\
	        ori	r2, r2, (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g; 	\
                mtc0	r2, C0_ENLO0


/*
 * MT_ENLO0_32:
 *	Create an Entry Lo from the label, c, d, v, and g given for a 32b address.
 *	KSeg0/1 are detected and if in those regions the upper 3 bits of the
 *	address are masked.
 */
#define	MT_ENLO0_32(label, c, d, v, g)						\
		la	r2, label;						\
                lui	r3, 0xc000;						\
                and	r3, r3, r2;						\
                lui	r4, 0x8000;						\
                beq	r3, r4, 1f;						\
                ori	r3, r0, 0x1fff;						\
                ori	r3, r3, 0xe000;						\
      1:        sll	r3, r3, 16;						\
                ori	r3, r3, 0xf000;						\
		and	r2, r2, r3;						\
		srl	r2, r2, PFNSHIFT;					\
	        ori	r2, r2, (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g; 	\
                mtc0	r2, C0_ENLO0



/*
 * MT_ENLO0_64:
 *	Create an Entry Lo from the label, c, d, v, and g given for a 64b address.
 *      assumes that the dla returns an umapped kseg0 address. i.e. we can assume the 
 *      lower portion is indeed a physical address and the PFN can be extracted
 ****   Notice that this macro extracts just the MipsII compatibility bits.
 */
#define	MT_ENLO0_64(label, c, d, v, g)						\
		dla	r2, label;						\
                li	r3, 0x3ffff000;				        \
		and	r2, r2, r3;						\
		srl	r2, r2, PFNSHIFT;					\
	        ori	r2, r2, (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g; 	\
                mtc0	r2, C0_ENLO0

/*
 * MT_ENLO1_64:
 *	Create an Entry Lo from the label, c, d, v, and g given for a 64b address.
 *      assumes that the dla returns an umapped kseg0 address. i.e. we can assume the 
 *      lower portion is indeed a physical address and the PFN can be extracted
 */
#define	MT_ENLO1_64(label, c, d, v, g)						\
		dla	r2, label;						\
                li	r3, 0x3ffff000;				        \
		and	r2, r2, r3;						\
		srl	r2, r2, PFNSHIFT;					\
	        ori	r2, r2, (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g; 	\
                mtc0	r2, C0_ENLO1


/*
 * MT_ENLO0_32LI:
 *	Create an Entry Lo from the address, c, d, v, and g given for a 32b address.
 *	KSeg0/1 are detected and if in those regions the upper 3 bits of the
 *	address are masked.
 */
#define	MT_ENLO0_32LI(address, c, d, v, g)						\
		li	r2, address;						\
                lui	r3, 0xc000;						\
                and	r3, r3, r2;						\
                lui	r4, 0x8000;						\
                beq	r3, r4, 1f;						\
                ori	r3, r0, 0x1fff;						\
                ori	r3, r3, 0xe000;						\
      1:        sll	r3, r3, 16;						\
                ori	r3, r3, 0xf000;						\
		and	r2, r2, r3;						\
		srl	r2, r2, PFNSHIFT;					\
	        ori	r2, r2, (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g; 	\
                mtc0	r2, C0_ENLO0


/**************************************************************************************
 *
 * MT_ENLO1:
 *	Create an Entry Lo from the label, c, d, v, and g given for a 32b address.
 *
 **************************************************************************************/
#define	MT_ENLO1(label, c, d, v, g)						\
		la	r2, label;						\
		li	r3, PFNMASK64;						\
		and	r2, r2, r3;						\
		srl	r2, r2, PFNSHIFT;					\
		ori	r2, r2, (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g; 	\
                mtc0	r2, C0_ENLO1


/*
 * MT_ENLO1_32:
 *	Create an Entry Lo from the label, c, d, v, and g given for a 32b address.
 *	KSeg0/1 are detected and if in those regions the upper 3 bits of the
 *	address are masked.
 */
#define	MT_ENLO1_32(label, c, d, v, g)						\
		la	r2, label;						\
                lui	r3, 0xc000;						\
                and	r3, r3, r2;						\
                lui	r4, 0x8000;						\
                beq	r3, r4, 1f;						\
                ori	r3, r0, 0x1fff;						\
                ori	r3, r3, 0xe000;						\
      1:        sll	r3, r3, 16;						\
                ori	r3, r3, 0xf000;						\
		and	r2, r2, r3;						\
		srl	r2, r2, PFNSHIFT;					\
	        ori	r2, r2, (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g; 	\
                mtc0	r2, C0_ENLO1


/*
 * MT_ENLO1_32LI:
 *	Create an Entry Lo from the label, c, d, v, and g given for a 32b address.
 *	KSeg0/1 are detected and if in those regions the upper 3 bits of the
 *	address are masked.
 */
#define	MT_ENLO1_32LI(address, c, d, v, g)						\
		li	r2, address;						\
                lui	r3, 0xc000;						\
                and	r3, r3, r2;						\
                lui	r4, 0x8000;						\
                beq	r3, r4, 1f;						\
                ori	r3, r0, 0x1fff;						\
                ori	r3, r3, 0xe000;						\
      1:        sll	r3, r3, 16;						\
                ori	r3, r3, 0xf000;						\
		and	r2, r2, r3;						\
		srl	r2, r2, PFNSHIFT;					\
	        ori	r2, r2, (c << TLBLO_CSHIFT) | (d << TLBLO_DSHIFT) | (v << TLBLO_VSHIFT) | g; 	\
                mtc0	r2, C0_ENLO1


/**************************************************************************************
 *
 * MT_PMASK loads a PageMask into r2 and moves it to the Page Mask register
 *
 **************************************************************************************/
#define MT_PMASK(pmask)								\
                li	r2, pmask;						\
                mtc0	r2, C0_PAGEMASK


/**************************************************************************************
 *
 * MT_INDEX loads an index into r2 and then moves it to the INDEX register in CP0
 *
 **************************************************************************************/
#define MT_INDEX(index)								\
                ori	r2, r0, index;						\
                mtc0	r2, C0_INX

/*************************************************************
 *
 * MT_ENHIb_32 loads a Mips 1/2 enhi into the C0_ENHI register
 *
 *************************************************************/
#define MT_ENHI_32(vpn, asid)	\
  li	r2, ((vpn & 0x7ffff) << TLBHI_VPNSHIFT) | (asid & 0xff); \
  mtc0	r2, C0_ENHI

/*************************************************************
 *
 * MT_ENHI_64 loads a Mips 3 enhi into the C0_ENHI register
 *    masks to VSIZE of 40
 *
 *************************************************************/
#define MT_ENHI_64(vpn, asid, R)	\
  li	r2, (vpn & 0x7ffffff);  \
  dsll  r2, r2, TLBHI_VPNSHIFT; \
  li    r1, (asid & 0xff);      \
  or    r2, r2, r1;             \
  li    r1, (R & 0x3);          \
  dsll  r1,r1, 62;              \
  or    r2, r1, r2;             \
  mtc0	r2, C0_ENHI


/**************************************************************************************
 *
 * MAKE_VA is a m4 macro in r4000.m4
 *
 **************************************************************************************/

#else /* R3000 */

#define TLB_ENTRY_HI( vpn, pid )\
  (((vpn)<<TLBHI_VPNSHIFT) | ((pid)<<TLBHI_PIDSHIFT)) 

#define TLB_ENTRY_LO( pfn, n, d, v, g )\
   (((pfn)<<TLBLO_PFNSHIFT) | (n) | (d) | (v) | (g) )

#define TLB_ENTRY( vpn, pid, pfn ) \
 (((vpn)<<TLBHI_VPNSHIFT)|((pid)<<TLBHI_PIDSHIFT)),((pfn)<<TLBLO_PFNSHIFT) 

#define TLBLO_NDVGSHIFT 8

#define TLBLO(pfn,ndvg) (((pfn)<<TLBLO_PFNSHIFT) | ((ndvg)<<TLBLO_NDVGSHIFT))

#define TLBHI( vpn, pid ) (((vpn)<<TLBHI_VPNSHIFT) | ((pid)<<TLBHI_PIDSHIFT)) 

#define TLB_STOREI( entry, lo, hi ) 	\
	addi	r30,r0,entry;		\
	sll	r30,TLBINX_INXSHIFT;	\
	mtc0	r30,C0_INX;		\
	li	r29,lo;			\
	mtc0	r29,C0_TLBLO;		\
	li	r30,hi;			\
	mtc0	r30,C0_TLBHI;		\
	nop;				\
	c0	C0_WRITEI

#define TLB_STORER( lo, hi ) \
	mtc0	r30,C0_INX;		\
	li	r30,lo;			\
	mtc0	r30,C0_TLBLO;		\
	li	r30,hi;			\
	mtc0	r30,C0_TLBHI;		\
	nop;				\
	c0	C0_WRITER

#endif

/*
 * Segment base addresses and sizes
 */
#define	K0BASE		0x80000000
#define	K0SIZE		0x20000000
#define	K1BASE		0xA0000000
#define	K1SIZE		0x20000000
#define	K2BASE		0xC0000000
#define	K2SIZE		0x20000000
#define	KPTEBASE	(-0x200000)	/* pte window is at top of kseg2 */
#define	KPTESIZE	0x200000
#define	KUBASE		0
#define	KUSIZE		0x80000000

/*
 * Exception vectors
 */
#define	UT_VEC		K0BASE			/* utlbmiss vector */
#define	E_VEC		(K0BASE+0x80)		/* exception vector */
#define	R_VEC		(K1BASE+0x1fc00000)	/* reset vector */

/*
 * Segment masks for creating PA's in 32b mode:
 */
#define KSEG0_MASK	0x1fffffff
#define KSEG1_MASK	0x1fffffff

