#ifndef __SYS_TFP_H__
#define __SYS_TFP_H__

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/* Copyright(C) 1986, MIPS Computer Systems */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.28 $"

#include <sys/mips_addrspace.h>

#if SABLE_RTL && !SABLE_CCUART_INT
/* When compiling for RTL simulation, don't enable CCUART interrupts
 * unless SABLE_CCUART_INT is set.
 */
#define IP21_NO_CCUART_INT 1
#endif

/*
 *  mips chip definitions
 *  constants for coprocessor 0
 */

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

/*
 * Cache size constants
 */
#define	MINCACHE	(2*1024*1024)
#define	MAXCACHE	(4*1024*1024)

#define	MAXPCACHESIZE	(16*1024)
#if	_PAGESZ > MAXPCACHESIZE
#define CACHECOLORSIZE	1
#else
#define CACHECOLORSIZE	(MAXPCACHESIZE/NBPP)
#endif
#define CACHECOLORMASK	(CACHECOLORSIZE - 1)

/*
 * TLB size constants
 */
#define	NTLBENTRIES	128
#define	NTLBSETS	3
/*
 * Wired entries are handled very differently for tfp.
 * We do not define NWIREDENTRIES since we do not have general purpose wired
 * registers.  TLBWIREDBASE and TLBRANDOMBASE are defined just to allow
 * them to be passed as ignored parameters to tlbwired routines.
 * NRANDOMENTRIES is defined so that the tlb_tick_actions routine can
 * view the entire TFP TLB as a collection of random entries - low level
 * invaltlb routine will not flush the entries which are wired.
 */
#define TLBRANDOMBASE	0	/* only defined as parm for flush/inval */
#define	NRANDOMENTRIES	NTLBENTRIES

#define	TLBFLUSH_NONPDA	1	/* flush all tlbs except PDA */
#define	TLBFLUSH_NONKERN 0	/* all but PDA/UPG/KSTACK */
#define	TLBFLUSH_RANDOM	0	/* flush all random tlbs */

#define	TLBINDEX(vaddr,pid)	((pnum(vaddr) ^ (pid)) & (NTLBENTRIES-1))

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

#define	TLBLO_PFNMASK		0xfffffff000
#define	TLBLO_PFNSHIFT		12
#define	TLBLO_CACHMASK		0xe00		/* cache coherency algorithm */
#define TLBLO_CACHSHIFT		9
#define TLBLO_UNC_PROCORD	0x0		/* uncached processor order */
#define TLBLO_UNC_SEQORD	0x400		/* uncached sequential order */
#define TLBLO_NONCOHRNT		0x600		/* Cacheable non-coherent */
#define TLBLO_COHRNT_EXL	0x800		/* coherent exclusive */
#define TLBLO_COHRNT_EXLWR	0xA00		/* coherent excl. on write */
#define	TLBLO_D			0x100		/* writeable */
#define	TLBLO_V			0x80		/* valid bit */
#define TLBLO_G			0		/* global bit--not used */


#define	TLBLO_UNCACHED		TLBLO_UNC_SEQORD

#define TLBSET_PROBE		0x8000000000000000
#define	TLBSET_MASK		0x3

#define ICACHE_NPID		255		/* TFP has ICACHE IASID */

#define	SHIFTAMT_MASK		0xf

#define	WIRED_MASK		0x7f
#define	WIRED_VMASK		0x80
#define	WIRED_SHIFT0		0
#define	WIRED_SHIFT1		8
#define	WIRED_SHIFT2		16
#define	WIRED_SHIFT3		24

#define	BADPADDR_PAMASK		0xffffffffff
#define	BADPADDR_SYNSHIFT	60
#define	BADPADDR_SYNMASK	(0xf<<BADPADDR_SYNSHIFT)

/*
 * Status register
 */
#define	SR_DM		0x10000000000		/* floating pt. debug mode */
#define	SR_KPSSHIFT	36
#define	SR_KPSMASK	(0xfLL<<SR_KPSSHIFT)	/* kernel page size */
#define	SR_UPSSHIFT	32
#define	SR_UPSMASK	(0xfLL<<SR_UPSSHIFT)	/* user page size */
#define	SR_CUMASK	0x30000000		/* coproc usable bits */
#define	SR_CU1		0x20000000		/* coprocessor 1 usable */
#define	SR_CU0		0x10000000		/* coprocessor 0 usable */
#define	SR_FR		0x04000000		/* enable additional fp regs */
#define	SR_RE		0x02000000		/* reverse endian in usr mode */

/* Page size selection
 */
#define	SR_UPS_4K	0x0000000000000000
#define SR_UPS_8K	0x0000000100000000
#define SR_UPS_16K	0x0000000200000000
#define SR_UPS_64K	0x0000000300000000
#define SR_UPS_1M	0x0000000400000000
#define SR_UPS_4M	0x0000000500000000
#define SR_UPS_16M	0x0000000600000000
#define	SR_KPS_4K	0x0000000000000000
#define SR_KPS_8K	0x0000001000000000
#define SR_KPS_16K	0x0000002000000000
#define SR_KPS_64K	0x0000003000000000
#define SR_KPS_1M	0x0000004000000000
#define SR_KPS_4M	0x0000005000000000
#define SR_KPS_16M	0x0000006000000000

#if _PAGESZ==4096
#define	SR_PAGESIZE	(SR_UPS_4K|SR_KPS_4K)
#endif
#if _PAGESZ==16384
#define	SR_PAGESIZE	(SR_UPS_16K|SR_KPS_16K)
#endif

/*
 * Bits in the SR that must be preserved.
 * SR_DEFAULT		default kernel mode (i.e. initial value)
 * SR_KERN_USRKEEP	SR bits which should be preserved from user (i.e. DM)
 * SR_KERN_SET		SR bits which should always be set in kernel mode
 */
#define SR_DEFAULT	(SR_PAGESIZE|SR_CU1)
#define SR_KERN_USRKEEP	(SR_DM|SR_FR)
#define SR_KERN_SET	(SR_PAGESIZE|SR_CU1)

/*
 * Interrupt enable bits
 * bits set to 1 enable the corresponding level interrupt
 */
#define	SR_IMASK	0x0007ff00	/* Interrupt mask */
#define	SR_IMASK11	0x00000000	/* mask level 11 */
#define	SR_IMASK10	0x00040000	/* mask level 10 */
#define	SR_IMASK9	0x00060000	/* mask level 9 */
#define	SR_IMASK8	0x00070000	/* mask level 8 */
#define	SR_IMASK7	0x00078000	/* mask level 7 */
#define	SR_IMASK6	0x0007c000	/* mask level 6 */
#define	SR_IMASK5	0x0007e000	/* mask level 5 */
#define	SR_IMASK4	0x0007f000	/* mask level 4 */
#define	SR_IMASK3	0x0007f800	/* mask level 3 */
#define	SR_IMASK2	0x0007fc00	/* mask level 2 */
#define	SR_IMASK1	0x0007fe00	/* mask level 1 */
#define	SR_IMASK0	0x0007ff00	/* mask level 0 */
#if IP21_NO_CCUART_INT
#undef	SR_IMASK
#undef	SR_IMASK4
#undef	SR_IMASK3
#undef	SR_IMASK2
#undef	SR_IMASK1
#undef	SR_IMASK0
#define	SR_IMASK	0x0007ef00	/* Interrupt mask */
#define	SR_IMASK4	0x0007e000	/* mask level 4 */
#define	SR_IMASK3	0x0007e800	/* mask level 3 */
#define	SR_IMASK2	0x0007ec00	/* mask level 2 */
#define	SR_IMASK1	0x0007ee00	/* mask level 1 */
#define	SR_IMASK0	0x0007ef00	/* mask level 0 */
#endif	/* IP21_NO_CCUART_INT */

#define	SR_IBIT11	0x00040000	/* bit level 11 */
#define	SR_IBIT10	0x00020000	/* bit level 10 */
#define	SR_IBIT9	0x00010000	/* bit level 9 */
#define	SR_IBIT8	0x00008000	/* bit level 8 */
#define	SR_IBIT7	0x00004000	/* bit level 7 */
#define	SR_IBIT6	0x00002000	/* bit level 6 */
#define	SR_IBIT5	0x00001000	/* bit level 5 */
#define	SR_IBIT4	0x00000800	/* bit level 4 */
#define	SR_IBIT3	0x00000400	/* bit level 3 */
#define	SR_IBIT2	0x00000200	/* bit level 2 */
#define	SR_IBIT1	0x00000100	/* bit level 1 */

#define	SR_XX		0x00000040	/* SGI-extended opcodes in user mode */
#define	SR_UX		0x00000020	/* mipsIII opcodes in user mode */
#define	SR_KU		0x00000010	/* execution mode: 0=>k, 1=>u */
#define	SR_PREVMODE	SR_KU		/* previous kernel/user mode */
#define	SR_KSU_MSK	SR_KU
#define	SR_KSU_USR	SR_KU
#define	SR_EXL		0x00000002	/* Exception level, 1=>exception */
#define	SR_IE		0x00000001	/* interrupt enable, 1=>enable */
#define	SR_IEC		SR_IE		/* compat with R3000 source */
#define SR_UXADDR	SR_UX		/* user ext. addressing and opcodes */
#define	SR_IMASKSHIFT	8
#define SR_KADDR	0		/* No kernel address mode bit */
#define SR_KX		0		/* Not defined for TFP */

/*
 * Cause Register
 */
#define	CAUSE_BD	0x8000000000000000 /* Branch delay slot */
#define	CAUSE_CEMASK	0x10000000	/* coprocessor error */
#define	CAUSE_CESHIFT	28
#define	CAUSE_NMI	0x08000000	/* Non-maskable interrupt occurred */
#define	CAUSE_BE	0x04000000	/* Bus error pending */
#define	CAUSE_VCI	0x02000000	/* Coproc Virtual Coherency interrupt */
#define	CAUSE_FPI	0x01000000	/* Floating pt. interrupt */

/* Interrupt pending bits */
#define	CAUSE_IP11	0x00040000	/* External level 11 pending */
#define	CAUSE_IP10	0x00020000	/* External level 10 pending */
#define	CAUSE_IP9	0x00010000	/* External level 9 pending */
#define	CAUSE_IP8	0x00008000	/* External level 8 pending */
#define	CAUSE_IP7	0x00004000	/* External level 7 pending */
#define	CAUSE_IP6	0x00002000	/* External level 6 pending */
#define	CAUSE_IP5	0x00001000	/* External level 5 pending */
#define	CAUSE_IP4	0x00000800	/* External level 4 pending */
#define	CAUSE_IP3	0x00000400	/* External level 3 pending */
#define	CAUSE_SW2	0x00000200	/* Software level 2 pending */
#define	CAUSE_SW1	0x00000100	/* Software level 1 pending */

#define	CAUSE_IPMASK	0x0007FF00	/* Pending interrupt mask */
#define	CAUSE_IPSHIFT	8

#define	CAUSE_EXCMASK	0x000000f8	/* Cause code bit mask */
#define	CAUSE_EXCSHIFT	3

#define CAUSE_BERRINTR	CAUSE_BE

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
#define	EXC_TLBX	EXC_CODE(30)	/* Duplicate entry in tlb */

/* software exception codes */
#define	SEXC_SEGV	EXC_CODE(32)	/* Software detected seg viol */
#define	SEXC_RESCHED	EXC_CODE(33)	/* resched request */
#define	SEXC_PAGEIN	EXC_CODE(34)	/* page-in request */
#define	SEXC_CPU	EXC_CODE(35)	/* coprocessor unusable */
#define SEXC_BUS	EXC_CODE(36)	/* software detected bus error */
#define SEXC_KILL	EXC_CODE(37)	/* bad page in process (vfault) */
#define SEXC_WATCH	EXC_CODE(38)	/* watchpoint (vfault) */
#define SEXC_UTINTR     EXC_CODE(41)    /* post-interrupt uthread processing */


#define	TRAPBASE_BMASK		0xfffffffff000	/* Base addr of exc. vectors */
#define	TRAPBASE_CACHSHIFT	59
#define	TRAPBASE_CACHMASK	(0x7<<TRAPBASE_CACHSHIFT)	/* cache algo */
#define	TRAPBASE_REGSHIFT	62
#define	TRAPBASE_REGMASK	(0x3<<TRAPBASE_REGSHIFT)	/* region */

#define	ICACHE_ASIDSHIFT	40
#define	ICACHE_ASIDMASK		(0xff<<ICACHE_ASIDSHIFT)
#define ICACHE_NASID		256
#define ICACHE_SIZE		16384		/* Size of icache in bytes */
#define ICACHE_LINESIZE		32		/* icache line size */
#define ICACHE_LINEMASK		31		/* mask for above */

#define DCACHE_HIT		(1<<63)		/* last cache probe hit */
#define	DCACHE_VMASK		(0xf<<55)	/* valid bits, 1/32-bit word */
#define	DCACHE_TAGMASK		0xfffffff000	/* Physical tag */
#define	DCACHE_E		0x800		/* cache line in excl. state */
#define	DCACHE_LINESIZE		32		/* dcache line size */
#define DCACHE_LINEMASK		31		/* mask for above */

#define	CONFIG_SMM		0x400000000	/* sequential memory model */
#define	CONFIG_ICE		0x200000000	/* inhibit count during exc. */
#define	CONFIG_PM		0x100000000	/* parity mode, 1 - odd par. */
#define	CONFIG_BE		0x00008000	/* 1 = big endian memory */
#define	CONFIG_IC		0x00000e00	/* Primary Icache size */
#define	CONFIG_DC		0x000001c0	/* Primary Dcache size */
#define	CONFIG_IB		0x00000020	/* Icache block size */
#define	CONFIG_DB		0x00000010	/* Dcache block size */
#define	CONFIG_IC_SHFT		9		/* shift IC to bit position 0 */
#define	CONFIG_DC_SHFT		6		/* shift DC to bit position 0 */
#define CONFIG_BE_SHFT		15              /* shift BE to bit position 0 */

/* C0_PRID Defines */
#define	C0_IMPMASK	0xff00
#define	C0_IMPSHIFT	8
#define C0_REVMASK	0xff
#define C0_MAJREVMASK	0xf0
#define	C0_MAJREVSHIFT	4
#define C0_MINREVMASK	0xf

/*
 * Coprocessor 0 operations
 *
 * TLB operations operate on TLB addressed by TLBset and VAddr
 */
#define	C0_COM		0x01000000	/* COP0 COM functions */
#define	C0_READ		C0_COM | 0x01	/* read TLB entry for TLBset & VAddr */
#define C0_WRITE	C0_COM | 0x02	/* write TLBentry for TLBset & VAddr */
#define C0_PROBE	C0_COM | 0x08	/* probe for TLB entry for VAddr */

/*****************************************/
/* There should disappear -- only here to let kernel compile.
 * Not really valid for TFP
 */
#define	C0_WRITEI	C0_WRITE
#define C0_WRITER	C0_WRITE
#define	C0_READI	C0_READ
/*****************************************/
/*
 * Coprocessor 0 registers
 * Some of these are r4000 specific.
 */
#ifdef _LANGUAGE_ASSEMBLY
#define	C0_TLBSET	$0		/* Select set in set-associative tlb */
#define	C0_TLBLO	$2		/* Low half of tlb entry */
#define	C0_UBASE	$4		/* Base of user page tables */
#define	C0_SHIFTAMT	$5		/* Shift amount for tlb refill */
#define	C0_TRAPBASE	$6		/* Base addr of exception vectors */
#define	C0_BADPADDR	$7		/* Bad Physical address */
#define	C0_BADVADDR	$8		/* Virtual address register */
#define	C0_COUNT	$9		/* Free-running counter */
#define	C0_TLBHI	$10		/* High half of tlb entry */
#define	C0_SR		$12		/* Status register */
#define	C0_CAUSE	$13		/* Cause register */
#define	C0_EPC		$14		/* Exception program counter */
#define	C0_PRID		$15		/* Revision identifier */
#define	C0_CONFIG	$16		/* Hardware configuration */
#define	C0_WORK0	$18		/* Uninterpreted temp. register */
#define	C0_WORK1	$19		/* Uninterpreted temp. register */
#define	C0_PBASE	$20		/* Base of kernel private page tables */
#define	C0_GBASE	$21		/* Base of kernel global page tables */
#define	C0_WIRED	$24		/* Indices of tlb wired entries */
#define	C0_DCACHE	$28		/* Dcache control register */
#define	C0_ICACHE	$29		/* Icache control register */

# else	/* ! _LANGUAGE_ASSEMBLY */
#define	C0_TLBSET	0		/* Select set in set-associative tlb */
#define	C0_TLBLO	2		/* Low half of tlb entry */
#define	C0_UBASE	4		/* Base of user page tables */
#define	C0_SHIFTAMT	5		/* Shift amount for tlb refill */
#define	C0_TRAPBASE	6		/* Base addr of exception vectors */
#define	C0_BADPADDR	7		/* Bad Physical address */
#define	C0_BADVADDR	8		/* Virtual address register */
#define	C0_COUNT	9		/* Free-running counter */
#define	C0_TLBHI	10		/* High half of tlb entry */
#define	C0_SR		12		/* Status register */
#define	C0_CAUSE	13		/* Cause register */
#define	C0_EPC		14		/* Exception program counter */
#define	C0_PRID		15		/* Revision identifier */
#define	C0_CONFIG	16		/* Hardware configuration */
#define	C0_WORK0	18		/* Uninterpreted temp. register */
#define	C0_WORK1	19		/* Uninterpreted temp. register */
#define	C0_PBASE	20		/* Base of kernel private page tables */
#define	C0_GBASE	21		/* Base of kernel global page tables */
#define	C0_WIRED	24		/* Indices of tlb wired entries */
#define	C0_DCACHE	28		/* Dcache control register */
#define	C0_ICACHE	29		/* Icache control register */
#endif	/* _LANGUAGE_ASSEMBLY */

#define	C0_CEL		C0_WORK0
#define	C0_KPTBL	C0_GBASE
#define	C0_KPTEBASE	C0_PBASE

#endif /* __SYS_TFP_H__ */
