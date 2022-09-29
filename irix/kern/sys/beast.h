/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1997, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SYS_BEAST_H__
#define __SYS_BEAST_H__

#include <sys/mips_addrspace.h>

/* The following base addresses are used in any compile mode when dealing
 * with the full 64-bit addresses.
 */
#define	KV0BASE		0x4000000000000000
#define	KPBASE		0x8000000000000000
#define	KPUNCACHED_BASE	0x9000000000000000
#define	KPCACHED_BASE	0xa800000000000000
#define	KV1BASE		0xc000000000000000

/*
 * Exception vectors
 */
#define SIZE_EXCVEC	0x400			/* Size (bytes) of an exc vec */
#define	R_VEC		(K1BASE+0x1fc00000)	/* reset vector */

#ifdef _LANGUAGE_ASSEMBLY
#define	UT_VEC		"not at a constant address. read C0_TrapBase register."
#define	KV0T_VEC	"not at a constant address. read C0_TrapBase register."
#define	KV1T_VEC	"not at a constant address. read C0_TrapBase register."
#define	E_VEC		"not at a constant address. read C0_TrapBase register."
#endif	/* _LANGUAGE_ASSEMBLY */

#ifdef _LANGUAGE_C

#include <sys/types.h>	/* needed for following extern */

extern __psunsigned_t	get_trapbase(void);
extern void		set_trapbase(__psunsigned_t);
extern void		set_kptbl(__psunsigned_t);

#define	UT_VEC		(get_trapbase()+0x000)	/* utlbmiss vector */
#define	KV0T_VEC	(get_trapbase()+0x400)	/* Kernel private tlbmiss */
#define	KV1T_VEC	(get_trapbase()+0x800)	/* Kernel global tlbmiss */
#define	E_VEC		(get_trapbase()+0xc00)	/* Gen. exception vector */

#endif	/* _LANGUAGE_C */

#ifdef MAPPED_KERNEL
#define	NKMAPENTRIES		1
#define	KMAP_INX		1
#else /* MAPPED_KERNEL */
#define	NKMAPENTRIES		0
#endif /* MAPPED_KERNEL */

#define NWIREDENTRIES		8
#define	TLBWIREDBASE		(1 + NKMAPENTRIES)

#define	NTLBENTRIES		128
#define	NTLBSETS		8
#define TLBRANDOMBASE		0
#define	NRANDOMENTRIES		NTLBENTRIES
#define	TLBFLUSH_NONPDA		TLBWIREDBASE	/* dont flush PDA + kernel*/
#define	TLBFLUSH_NONKERN 	(TLBWIREDBASE+TLBKSLOTS) /* all but PDA/UPG/KSTACK */
#define	TLBFLUSH_RANDOM		TLBRANDOMBASE	/* flush all random tlbs */

#define TLBLO_PFNMASK		0x000003FFFFFFFC00
#define	TLBLO_PFNSHIFT		10

#define	TLBLO_CACHMASK		0x380		/* cache coherency algorithm */
#define TLBLO_CACHSHIFT		7
#define TLBLO_UNCACHED		0x100		/* not cached */

#if _RUN_UNCACHED
#define TLBLO_NONCOHRNT		TLBLO_UNCACHED
#define	TLBLO_EXL		TLBLO_UNCACHED
#define TLBLO_EXLWR		TLBLO_UNCACHED
#else	/* _RUN_UNCACHED */
#define TLBLO_NONCOHRNT		0x180		/* Cacheable non-coherent */
#define	TLBLO_EXL		0x200		/* Exclusive */
#define TLBLO_EXLWR		0x280		/* Exclusive write */
#endif	/* _RUN_UNCACHED */
#define	TLBLO_UNCACHED_ACC	0x380		/* Uncached Accelerated */

#define	TLBLO_D			0x40		/* writeable */
#define	TLBLO_V			0x20		/* valid bit */
#define	TLBLO_G			0x10		/* global access bit */

/*
 * TLBLO system hint field.
 */

#define TLBLO_SHINTMASK  	0x00003C0000000000 /* system xface hint */
#define TLBLO_SHINTSHIFT	42
#define TLBLO_HWBITS		0x00003FFFFFFFFFFF


#if defined (PSEUDO_BEAST)
/* NOTE: TLBHI_VPNMASK and TLBHI_VPBSHIFT are NOT used to take the contents
 * 	 of TLBHI and extract the VPN.  Rather, they are used to take a
 *	 virtual address and extract the page number.
 */
#if _PAGESZ==4096
#define	TLBHI_VPNMASK		0xfffffffffffff000
#define	TLBHI_VPNSHIFT		12
#endif
#if _PAGESZ==16384
#define	TLBHI_VPNMASK		0xffffffffffffc000
#define	TLBHI_VPNSHIFT		14
#endif
#define	TLBHI_PIDMASK		0xff0
#define	TLBHI_PIDSHIFT		4
#define	TLBHI_NPID		255		/* 255 to fit in 8 bits */
#define	TLBHI_REGIONSHIFT	62
#ifdef _LANGUAGE_C
#define	TLBHI_REGIONMASK	(3L<<TLBHI_REGIONSHIFT)
#else
#define	TLBHI_REGIONMASK	(3<<TLBHI_REGIONSHIFT)
#endif

#define TLBSET_PROBE		0x8000000000000000
#define TLBSET_WIRED		0x4000000000000000
#define	TLBSET_MASK		0x7

#define ICACHE_NPID		255		

#define	SHIFTAMT_MASK		0xf



#endif /* PSEUDO_BEAST */


/*
 * Cache size constants
 */
#define	MINCACHE	(512 * 1024)	/* 512 KB */
#define	MAXCACHE	(32*1024*1024)	/* 32  MB */

#define	MAXPCACHESIZE	(64*1024)	/* 64  KB */
#if	_PAGESZ > MAXPCACHESIZE
#define CACHECOLORSIZE	1
#else
#define CACHECOLORSIZE	(MAXPCACHESIZE/NBPP)
#endif
#define CACHECOLORMASK	(CACHECOLORSIZE - 1)


/*
 * Status register
 */

#define	SR_CUMASK	0x70000000	/* coproc usable bits */
#define	SR_CU2		0x40000000	/* Coprocessor 2 usable */
#define	SR_CU1		0x20000000	/* Coprocessor 1 usable */
#define	SR_CU0		0x10000000	/* Coprocessor 0 usable */

#define	SR_FR		0x04000000	/* enable additional fp registers */
#define	SR_RE		0x02000000	/* reverse endian in user mode */

#define	SR_HWRESET	0x00400000	/* reverse endian in user mode */
#define	SR_TLBSHUTDN	0x00200000	/* reverse endian in user mode */

/* Diagnostic status bits */

#define	SR_SR		0x00100000	/* soft reset occured */
#define	SR_NMI		0x00080000	/* NMI bit */
#define SR_CE		0x00040000	/* Create ECC */

#define	SR_IMASK	0x0003ff00	/* Interrupt mask */
#define	SR_IMASK10	0x00000000	/* mask level 10 */
#define	SR_IMASK9	0x00010000	/* mask level 9 */
#define	SR_IMASK8	0x00030000	/* mask level 8 */
#define	SR_IMASK7	0x00038000	/* mask level 7 */
#define	SR_IMASK6	0x0003c000	/* mask level 6 */
#define	SR_IMASK5	0x0003e000	/* mask level 5 */
#define	SR_IMASK4	0x0003f000	/* mask level 4 */
#define	SR_IMASK3	0x0003f800	/* mask level 3 */
#define	SR_IMASK2	0x0003fc00	/* mask level 2 */
#define	SR_IMASK1	0x0003fe00	/* mask level 1 */
#define	SR_IMASK0	0x0003ff00	/* mask level 0 */

#define	SR_IBIT10	0x00020000	/* bit level 8 */
#define	SR_IBIT9	0x00010000	/* bit level 8 */
#define	SR_IBIT8	0x00008000	/* bit level 8 */
#define	SR_IBIT7	0x00004000	/* bit level 7 */
#define	SR_IBIT6	0x00002000	/* bit level 6 */
#define	SR_IBIT5	0x00001000	/* bit level 5 */
#define	SR_IBIT4	0x00000800	/* bit level 4 */
#define	SR_IBIT3	0x00000400	/* bit level 3 */
#define	SR_IBIT2	0x00000200	/* bit level 2 */
#define	SR_IBIT1	0x00000100	/* bit level 1 */

#define	SR_KSU_MSK	0x00000018	/* 2 bit mode: 00b=>k, 10b=>u */
#define	SR_KSU_USR	0x00000010	/* 2 bit mode: 00b=>k, 10b=>u */
#define	SR_KSU_KS	0x00000008	/* 0-->kernel 1-->supervisor */
#define	SR_ERL		0x00000004	/* Error level, 1=>cache error */
#define	SR_EXL		0x00000002	/* Exception level, 1=>exception */
#define	SR_IE		0x00000001	/* interrupt enable, 1=>enable */
#define	SR_IEC		SR_IE		/* compat with R3000 source */
#define	SR_PREVMODE	SR_KSU_MSK	/* previous kernel/user mode */
#define SR_PAGESIZE	0		/* No pagesize bits in SR */
#define SR_DM		0		/* No FP Debug Mode bits in SR */
#define SR_DEFAULT	0		/* Bits to preserve in SR */

#define SR_KERN_SET	SR_KADDR|SR_UXADDR /*Bits to set in SR for kernel mode*/
#define SR_KERN_USRKEEP	0		/* Bits to keep in SR from user mode*/

/*
 * SR_KADDR defines the desired state of the kernel address mode bit
 * in C0_SR, if such bits exist.  We could actually enable SR_KX when
 * compiled under 32-bit compilers, though there is no real reason to do so.
 */
#if defined (PSEUDO_BEAST)	
/*
 * hack so that spl is not 0.
 */
#define SR_KADDR	SR_CU0		/* kernel 32 bit addressing */
#else
#define SR_KADDR	0		/* kernel 32 bit addressing */
#endif
#define SR_UXADDR	0		/* no user ext. address/opcodes */
#define SR_UX		0
#define SR_KX		0
#define SR_DE		0

#define	SR_IMASKSHIFT	8


/*
 * Cause Register
 */
#define	CAUSE_BD	0x80000000	/* Branch delay slot */
#define	CAUSE_CEMASK	0x30000000	/* coprocessor error */
#define	CAUSE_CESHIFT	28

/* Interrupt pending bits */
#define	CAUSE_IP10	0x00020000	/* External level 10 pending */
#define	CAUSE_IP9	0x00010000	/* External level 9 pending */
#define	CAUSE_IP8	0x00010000	/* External level 8 pending */
#define	CAUSE_IP7	0x00004000	/* External level 7 pending */
#define	CAUSE_IP6	0x00002000	/* External level 6 pending */
#define	CAUSE_IP5	0x00001000	/* External level 5 pending */
#define	CAUSE_IP4	0x00000800	/* External level 4 pending */
#define	CAUSE_IP3	0x00000400	/* External level 3 pending */
#define	CAUSE_SW2	0x00000200	/* Software level 2 pending */
#define	CAUSE_SW1	0x00000100	/* Software level 1 pending */

#define	CAUSE_IPMASK	0x0000FF00	/* Pending interrupt mask */
#define	CAUSE_IPSHIFT	8

#define	CAUSE_EXCMASK	0x00000078	/* Cause code bits */
#define	CAUSE_EXCSHIFT	3

#define	CAUSE_FMT	"\20\40BD\36CE1\35CE0\20IP8\17IP7\16IP6\15IP5\14IP4\13IP3\12SW2\11SW1\1INT"

#define	setsoftclock()	siron(CAUSE_SW1)
#define	setsoftnet()	siron(CAUSE_SW2)
#define	acksoftclock()	siroff(CAUSE_SW1)
#define	acksoftnet()	siroff(CAUSE_SW2)

/* Cause register exception codes */

#define	EXC_CODE(x)	((x)<<3)

/* Hardware exception codes */
#define	EXC_INT		EXC_CODE(0)	/* interrupt */
#define	EXC_MOD		EXC_CODE(1)	/* TLB mod */
#define	EXC_RMISS	EXC_CODE(2)	/* Read TLB Miss */
#define	EXC_WMISS	EXC_CODE(3)	/* Write TLB Miss */
#define	EXC_RADE	EXC_CODE(4)	/* Read Address Error */
#define	EXC_WADE	EXC_CODE(5)	/* Write Address Error */
#define	EXC_IBE		EXC_CODE(6)	/* Instruction Bus Error */
#define	EXC_DBE		EXC_CODE(7)	/* Data Bus Error */
#define	EXC_SYSCALL	EXC_CODE(8)	/* SYSCALL */
#define	EXC_BREAK	EXC_CODE(9)	/* BREAKpoint */
#define	EXC_II		EXC_CODE(10)	/* Illegal Instruction */
#define	EXC_CPU		EXC_CODE(11)	/* CoProcessor Unusable */
#define	EXC_OV		EXC_CODE(12)	/* OVerflow */
#define	EXC_TRAP	EXC_CODE(13)	/* Trap exception */
#define	EXC_RSVD	EXC_CODE(14)	/* Unused */
#define	EXC_FPE		EXC_CODE(15)	/* Floating Point Exception */
#define	EXC_RSVD1	EXC_CODE(16)	/* Unused */

/* software exception codes */
#define	SEXC_SEGV	EXC_CODE(32)	/* Software detected seg viol */
#define	SEXC_RESCHED	EXC_CODE(33)	/* resched request */
#define	SEXC_PAGEIN	EXC_CODE(34)	/* page-in request */
#define	SEXC_CPU	EXC_CODE(35)	/* coprocessor unusable */
#define SEXC_BUS	EXC_CODE(36)	/* software detected bus error */
#define SEXC_KILL	EXC_CODE(37)	/* bad page in process (vfault) */
#define SEXC_WATCH	EXC_CODE(38)	/* watchpoint (vfault) */
#define SEXC_RSVD	EXC_CODE(39)	/* unused */
#define SEXC_RSVD1	EXC_CODE(40) 	/* unused */
#define	SEXC_UTINTR	EXC_CODE(41)	/* post-interrupt uthread processing */

/*
 * Coprocessor 0 operations
 */
#define	C0_READI  0x1		/* read ITLB entry addressed by C0_INDEX */
#define	C0_WRITEI 0x2		/* write ITLB entry addressed by C0_INDEX */
#define	C0_WRITER 0x6		/* write ITLB entry addressed by C0_RAND */
#define	C0_PROBE  0x8		/* probe for ITLB entry addressed by TLBHI */

/*
 * BEAST Cache Definitions
 * Cache sizes are in bytes
 */

#define	CACHE_ILINE_SIZE	128	/* Primary instruction line size */
#define	CACHE_ILINE_MASK	~(CACHE_ILINE_SIZE-1)
#define	CACHE_DLINE_SIZE	128	/* Primary data line size */
#define	CACHE_DLINE_MASK	~(CACHE_DLINE_SIZE-1)
#define	CACHE_SLINE_SIZE	128	/* Secondary cache line size */
#define	CACHE_SLINE_MASK	~(CACHE_SLINE_SIZE-1)


/*
 * Coprocessor 0 registers
 * Some of these are r4000 specific.
 */
#ifdef _LANGUAGE_ASSEMBLY
#define	C0_TLBSET	$0		/* Select set in set-associative tlb */
#define C0_TLBRAND	$1		/* Pseudo-Random counter for tlb repl*/
#define	C0_TLBLO	$2		/* Low half of tlb entry */
#define C0_KPS		$3		/* Kernel/Supervisor page size */
#define	C0_WORK0	$4		/* Uninterpreted temp. register */
#define	C0_UPS		$5		/* User page size 		*/
#define	C0_TRAPBASE	$6		/* Base addr of exception vectors */
#define C0_ICACHE	$7		/* Icache address space id	*/
#define	C0_BADVADDR	$8		/* Virtual address register 	*/
#define	C0_COUNT	$9		/* Free-running counter 	*/
#define	C0_TLBHI	$10		/* High half of tlb entry 	*/
#define	C0_COMPARE	$11		/* Timer Compare		*/
#define	C0_SR		$12		/* Status register 		*/
#define	C0_CAUSE	$13		/* Cause register 		*/
#define	C0_EPC		$14		/* Exception program counter 	*/
#define	C0_PRID		$15		/* Revision identifier 		*/
#define	C0_CONFIG	$16		/* Hardware configuration 	*/
#define	C0_WORK1	$17		/* Uninterpreted temp. register */
#define	C0_WORK2	$20		/* Uninterpreted temp. register */
#define	C0_WORK3	$21		/* Uninterpreted temp. register */
#define	C0_SERINTFC	$22		/* Serial Interface		*/
#define	C0_DIAG		$23		/* Diagnostic			*/
#define	C0_PERFCNT	$25		/* Performance Counters		*/
#define	C0_ECC		$26		/* S-cache ECC and primary parity */
#define	C0_CACHE_ERR	$27		/* cache error status */
#define	C0_TAGLO	$28		/* cache operations */

# else	/* ! _LANGUAGE_ASSEMBLY */

#define	C0_TLBSET	0		/* Select set in set-associative tlb */
#define C0_TLBRAND	1		/* Pseudo-Random counter for tlb repl*/
#define	C0_TLBLO	2		/* Low half of tlb entry */
#define C0_KPS		3		/* Kernel/Supervisor page size */
#define	C0_WORK0	4		/* Uninterpreted temp. register */
#define	C0_UPS		5		/* User page size 		*/
#define	C0_TRAPBASE	6		/* Base addr of exception vectors */
#define C0_ICACHASID	7		/* Icache address space id	*/
#define	C0_BADVADDR	8		/* Virtual address register 	*/
#define	C0_COUNT	9		/* Free-running counter 	*/
#define	C0_TLBHI	10		/* High half of tlb entry 	*/
#define	C0_COMPARE	11		/* Timer Compare		*/
#define	C0_SR		12		/* Status register 		*/
#define	C0_CAUSE	13		/* Cause register 		*/
#define	C0_EPC		14		/* Exception program counter 	*/
#define	C0_PRID		15		/* Revision identifier 		*/
#define	C0_CONFIG	16		/* Hardware configuration 	*/
#define	C0_WORK1	17		/* Uninterpreted temp. register */
#define	C0_WORK2	20		/* Uninterpreted temp. register */
#define	C0_WORK3	21		/* Uninterpreted temp. register */
#define	C0_SERINTFC	22		/* Serial Interface		*/
#define	C0_DIAG		23		/* Diagnostic			*/
#define	C0_PERFCNT	25		/* Performance Counters		*/
#define	C0_ECC		26		/* S-cache ECC and primary parity */
#define	C0_CACHE_ERR	27		/* cache error status */
#define	C0_TAGLO	28		/* cache operations */

#endif	/* _LANGUAGE_ASSEMBLY */

#define TLB_PSIZE_SHFT		4
#define TLB_PSIZE_MASK		0xf
#define TLB_PSIZE_SET_SHFT(_set)	((_set) * TLB_PSIZE_SHFT)

#define TLB_PSIZE_SET_VAL(_set, _val)	((_val) << TLB_PSIZE_SET_SHFT(_set))

/*
 * Page size values (per tlbset)to be programmed into the KPS and UPS regs 
 */
#define TLB_PSIZE_4k		0
#define TLB_PSIZE_16k		1
#define TLB_PSIZE_64k		2
#define TLB_PSIZE_256k		3
#define TLB_PSIZE_1m		4
#define TLB_PSIZE_4m		5
#define TLB_PSIZE_16m		6


#define DEFAULT_PAGE_SIZE	(TLB_PSIZE_SET_VAL(7, TLB_PSIZE_16k) |  \
				 TLB_PSIZE_SET_VAL(6, TLB_PSIZE_16k) |  \
				 TLB_PSIZE_SET_VAL(5, TLB_PSIZE_16k) |  \
				 TLB_PSIZE_SET_VAL(4, TLB_PSIZE_16k) |  \
				 TLB_PSIZE_SET_VAL(3, TLB_PSIZE_16k) |  \
				 TLB_PSIZE_SET_VAL(2, TLB_PSIZE_16k) |  \
				 TLB_PSIZE_SET_VAL(1, TLB_PSIZE_16k) |  \
				 TLB_PSIZE_SET_VAL(0, TLB_PSIZE_16k))

/* target cache */

#define	CACH_PI			0x0	/* Primary instruction cache */
#define	CACH_PD			0x1	/* Primary data cache */
#define	CACH_S			0x3	/* Secondary cache */
#define CACH_BARRIER		0x14	/* Cache barrier operation */

/* Cache operations */

#define	C_IINV			0x00	/* index invalidate (i) */
#define	C_IWBINV		0x00	/* index writeback inval (s) */
#define	C_ILT			0x04	/* index load tag (all) */
#define	C_IST			0x08	/* index store tag (all) */
#define	C_HINV			0x10	/* hit invalidate (s) */
#define	C_HWBINV		0x14	/* hit writeback inv. (s) */
#define	C_HWB			C_HWBINV /* hit writeback inv. (s) */

#define PICACHE_SETS		8
#define PICACH_SIZE_SHFT	3
#define PDCACHE_SETS		8
#define PDCACH_SIZE_SHFT	3
#define SCACHE_SETS		2
#define SCACH_SIZE_SHFT		1

/*
 * BEAST Configuration - These define the values used 
 * during the reset cycle.
 */

#define	CONFIG_K0_SHFT		0
#define	CONFIG_K0_MASK		(0x7 << CONFIG_K0_SHFT)
#define	CONFIG_K0(_B)	 	((_B) << CONFIG_K0_SHFT)

#define	CONFIG_DN_SHFT		3
#define	CONFIG_DN_MASK		(0x1 << CONFIG_DN_SHFT)

#define	CONFIG_PD_SHFT		4
#define	CONFIG_PD_MASK		(0x1 << CONFIG_PD_SHFT)
#define	CONFIG_PD(_B)		((_B) << CONFIG_PD_SHFT)

#define	CONFIG_CBM_SHFT		5
#define	CONFIG_CBM_MASK		(0x1 << CONFIG_CBM_SHFT)
#define	CONFIG_CBM(_B)		((_B) << CONFIG_CBM_SHFT)

#define	CONFIG_DM_SHFT		6
#define	CONFIG_DM_MASK		(0x1 << CONFIG_DM_SHFT)
#define	CONFIG_DM(_B)		((_B) << CONFIG_DM_SHFT)

#define	CONFIG_BE_SHFT		7
#define	CONFIG_BE_MASK		(0x1 << CONFIG_BE_SHFT)
#define	CONFIG_BE(_B)		((_B) << CONFIG_BE_SHFT)

#define	CONFIG_EW_SHFT		8
#define	CONFIG_EW_MASK		(0x1 << CONFIG_EW_SHFT)
#define	CONFIG_EW(_B)		((_B) << CONFIG_EW_SHFT)

#define	CONFIG_EP_SHFT		9
#define	CONFIG_EP_MASK		(0x1 << CONFIG_EP_SHFT)
#define	CONFIG_EP(_B)		((_B) << CONFIG_EP_SHFT)

#define	CONFIG_ES_SHFT		10
#define	CONFIG_ES_MASK		(0x7 << CONFIG_ES_SHFT)
#define	CONFIG_ES(_B)		((_B) << CONFIG_ES_SHFT)

#define	CONFIG_ER_SHFT		13
#define	CONFIG_ER_MASK		(0x1 << CONFIG_ER_SHFT)
#define	CONFIG_ER(_B)		((_B) << CONFIG_ER_SHFT)

#define	CONFIG_ED_SHFT		14
#define	CONFIG_ED_MASK		(0x1 << CONFIG_ED_SHFT)
#define	CONFIG_ED(_B)		((_B) << CONFIG_ED_SHFT)

#define	CONFIG_EL_SHFT		15
#define	CONFIG_EL_MASK		(0xF << CONFIG_ES_SHFT)
#define	CONFIG_EL(_B)		((_B) << CONFIG_ES_SHFT)

#define	CONFIG_ET_SHFT		19
#define	CONFIG_ET_MASK		(0x7 << CONFIG_ET_SHFT)
#define	CONFIG_ET(_B)		((_B) << CONFIG_ET_SHFT)

#define	CONFIG_EI_SHFT		21
#define	CONFIG_EI_MASK		(0x1 << CONFIG_EI_SHFT)
#define	CONFIG_EI(_B)		((_B) << CONFIG_EI_SHFT)

#define	CONFIG_EWL_SHFT		22
#define	CONFIG_EWL_MASK		(0x7 << CONFIG_EWL_SHFT)
#define	CONFIG_EWL(_B)		((_B) << CONFIG_EWL_SHFT)

#define	CONFIG_ECI_SHFT		25
#define	CONFIG_ECI_MASK		(0x1 << CONFIG_ECI_SHFT)
#define	CONFIG_ECI(_B)		((_B) << CONFIG_ECI_SHFT)

#define	CONFIG_ESYNC_SHFT	26
#define	CONFIG_ESYNC_MASK	(0x1 << CONFIG_ESYNC_SHFT)
#define	CONFIG_ESYNC(_B)		((_B) << CONFIG_ESYNC_SHFT)

#define	CONFIG_WM_SHFT		27
#define	CONFIG_WM_MASK		(0x7 << CONFIG_WM_SHFT)
#define	CONFIG_WM(_B)		((_B) << CONFIG_WM_SHFT)

#define	CONFIG_EM_SHFT		30
#define	CONFIG_EM_MASK		(0x3 << CONFIG_EM_SHFT)
#define	CONFIG_EM(_B)		((_B) << CONFIG_EM_SHFT)

#define	CONFIG_SI_SHFT		32
#define	CONFIG_SI_MASK		(0x7 << CONFIG_SI_SHFT)
#define	CONFIG_SI(_B)		((_B) << CONFIG_SI_SHFT)

#define	CONFIG_SS_SHFT		35
#define	CONFIG_SS_MASK		(0x3 << CONFIG_SS_SHFT)
#define	CONFIG_SS(_B)		((_B) << CONFIG_SS_SHFT)

#define	CONFIG_SM_SHFT		37
#define	CONFIG_SM_MASK		(0x1F << CONFIG_SM_SHFT)
#define	CONFIG_SM(_B)		((_B) << CONFIG_SM_SHFT)

#define	CONFIG_SC_SHFT		42
#define	CONFIG_SC_MASK		(0x1 << CONFIG_SC_SHFT)
#define	CONFIG_SC(_B)		((_B) << CONFIG_SC_SHFT)

#define	CONFIG_SR_SHFT		43
#define	CONFIG_SR_MASK		(0x1 << CONFIG_SR_SHFT)
#define	CONFIG_SR(_B)		((_B) << CONFIG_SR_SHFT)

#define	CONFIG_SH_SHFT		44
#define	CONFIG_SH_MASK		(0xF << CONFIG_SH_SHFT)
#define	CONFIG_SH(_B)		((_B) << CONFIG_SH_SHFT)

#define	CONFIG_VS_SHFT		48
#define	CONFIG_VS_MASK		(0x3 << CONFIG_VS_SHFT)
#define	CONFIG_VS(_B)		((_B) << CONFIG_VS_SHFT)

#define	CONFIG_VR_SHFT		50
#define	CONFIG_VR_MASK		(0x1 << CONFIG_VR_SHFT)
#define	CONFIG_VR(_B)		((_B) << CONFIG_VR_SHFT)

#define	CONFIG_DC_SHFT		58
#define	CONFIG_DC_MASK		(0x7 << CONFIG_DC_SHFT)
#define	CONFIG_DC(_B)		((_B) << CONFIG_DC_SHFT)

#define	CONFIG_IC_SHFT		61
#define	CONFIG_IC_MASK		(0x7 << CONFIG_IC_SHFT)
#define	CONFIG_IC(_B)		((_B) << CONFIG_IC_SHFT)

#if _PAGESZ == 4096
#define SYS_PGSIZE_BITS	   TLB_PSIZE_4k
#elif _PAGESZ == 16384 
#define SYS_PGSIZE_BITS	   	   TLB_PSIZE_16k
#else
#error <SYS_PGSIZE_BITS need to be set>
#endif

/*
 * software definitions and function declarations
 */
#if defined(_LANGUAGE_C) 

typedef struct cacheop_s {
    __uint64_t		cop_address;	/* address for operation */
    __uint64_t		cop_tag;	/* tag hi value */
    __uint32_t		cop_operation;	/* operation */
} cacheop_t;

#endif

#endif /* __SYS_BEAST_H__ */

