#ifndef __SYS_SBD_H__
#define __SYS_SBD_H__

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

#ident	"$Revision: 1.2 $"

#if (defined(_STANDALONE) && (! defined(_IRIX5_1)))
#include <mips_addrspace.h>
#endif /* (defined(_STANDALONE) && (! defined(_IRIX5_1))) */

#if TFP
#include "sys/tfp.h"
#else	/* R3000 || R4000 */

#ifndef	_PAGESZ			/* for backward compatibility	*/
#define	_PAGESZ	4096		/* with old user programs	*/
#endif				/* and stand-alone		*/

/*
 * Chip definitions for R3000 and R4000-mipsII
 *
 *  constants for coprocessor 0
 */

/*
 * Segment base addresses and sizes
 */
#if (! (defined(_STANDALONE) && (! defined(_IRIX5_1))))
#define	KUBASE		0
#define	KUSIZE		0x80000000
#define	K0BASE		0x80000000
#define	K0SIZE		0x20000000
#define	K1BASE		0xA0000000
#define	K1SIZE		0x20000000
#define	K2BASE		0xC0000000
#define	K2SIZE		0x20000000
#endif /* (! (defined(_STANDALONE) && (! defined(_IRIX5_1)))) */

/*
 * Exception vectors
 */
#define SIZE_EXCVEC	0x80			/* Size (bytes) of an exc. vec */
#define	UT_VEC		K0BASE			/* utlbmiss vector */
#define	R_VEC		(K1BASE+0x1fc00000)	/* reset vector */

#if R3000
#define	E_VEC		(K0BASE+0x80)		/* exception vector */
#endif
#if R4000
#define	XUT_VEC		(K0BASE+0x80)		/* extended address tlbmiss */
#define	ECC_VEC		(K0BASE+0x100)		/* Ecc exception vector */
#define	E_VEC		(K0BASE+0x180)		/* Gen. exception vector */
#endif

/*
 * Address conversion macros
 */
#if (! (defined(_STANDALONE) && (! defined(_IRIX5_1))))
#ifdef _LANGUAGE_ASSEMBLY
#define	K0_TO_K1(x)	((x)|0xA0000000)	/* kseg0 to kseg1 */
#define	K1_TO_K0(x)	((x)&0x9FFFFFFF)	/* kseg1 to kseg0 */
#define	K0_TO_PHYS(x)	((x)&0x1FFFFFFF)	/* kseg0 to physical */
#define	K1_TO_PHYS(x)	((x)&0x1FFFFFFF)	/* kseg1 to physical */
#define	KDM_TO_PHYS(x)	((x)&0x1FFFFFFF)	/* DirectMapped
								 to physical */
#define	PHYS_TO_K0(x)	((x)|0x80000000)	/* physical to kseg0 */
#if !defined(EVEREST) || defined(STANDALONE)
#define	PHYS_TO_K1(x)	((x)|0xA0000000)	/* physical to kseg1 */
#endif
#else /* _LANGUAGE_C */

#if defined(EVEREST) && !defined(STANDALONE)
/*
 * Fake out uncached access.  On Everest, the only way to get to
 * memory is via coherent access (not K1SEG).  When copying out of
 * DMA'ed buffers, coherent references will work fine, since IO is
 * fully cache-coherent (and in fact, coherent references *must* be
 * used).  On Everest, the *only* code that can create true uncached
 * references is in the ml/EVEREST directory.  Such references are 
 * used only to do PIO operations.
 */
#define	K0_TO_K1(x)	((unsigned)(x)|0x80000000)
#define	PHYS_TO_K1(x)	((unsigned)(x)|0x80000000)
#else
#define	K0_TO_K1(x)	((unsigned)(x)|0xA0000000)	/* kseg0 to kseg1 */
#define	PHYS_TO_K1(x)	((unsigned)(x)|0xA0000000)	/* physical to kseg1 */
#endif /* EVEREST */

#define	K1_TO_K0(x)	((unsigned)(x)&0x9FFFFFFF)	/* kseg1 to kseg0 */
#define	K0_TO_PHYS(x)	((unsigned)(x)&0x1FFFFFFF)	/* kseg0 to physical */
#define	K1_TO_PHYS(x)	((unsigned)(x)&0x1FFFFFFF)	/* kseg1 to physical */
#define	KDM_TO_PHYS(x)	((unsigned)(x)&0x1FFFFFFF)	/* DirectMapped
								 to physical */
#define	PHYS_TO_K0(x)	((unsigned)(x)|0x80000000)	/* physical to kseg0 */
#endif

#ifdef STANDALONE
#ifdef IP5
/* Prom code which runs from B space ("back-door memory") needs this
 * macro when it gives dma addresses to controllers (such as esdi),
 * since the dma cannot access memory > 256m.
 */
#ifdef _LANGUAGE_ASSEMBLY
#define	KDM_TO_LOWPHYS(x)	((x)&0x0FFFFFFF)
#else
#define	KDM_TO_LOWPHYS(x)	((unsigned)(x)&0x0FFFFFFF)
#endif

#else	/* IP5 */
#define	KDM_TO_LOWPHYS(x)	(KDM_TO_PHYS(x))
#endif	/* IP5 */
#endif	/* STANDALONE */

/*
 * Address predicates
 */
#define	IS_KSEG0(x)	((unsigned)(x) >= K0BASE && (unsigned)(x) < K1BASE)
#define	IS_KSEG1(x)	((unsigned)(x) >= K1BASE && (unsigned)(x) < K2BASE)
#define	IS_KSEGDM(x)	((unsigned)(x) >= K0BASE && (unsigned)(x) < K2BASE)
#define	IS_KSEG2(x)	((unsigned)(x) >= K2BASE && (unsigned)(x) < KPTE_SHDUBASE)
#define	IS_KPTESEG(x)	((unsigned)(x) >= KPTE_SHDUBASE)
#define	IS_KUSEG(x)	((unsigned)(x) < K0BASE)
#endif /* (! (defined(_STANDALONE) && (! defined(_IRIX5_1)))) */


#if R3000
/*
 * Cache size constants
 */
#define	MINCACHE	(4*1024)
#define	MAXCACHE	(64*1024)
#endif

#if R4000
#define	MINCACHE	(128*1024)
#define	MAXCACHE	(4*1024*1024)
#define	R4K_MAXCACHELINESIZE	128		/* max r4k scache line size */
#define R4K_MAXPCACHESIZE	0x8000		/* max r4k primary cache size */
#if	NBPP > R4K_MAXPCACHESIZE
#define	CACHECOLORSIZE	1
#else
#define CACHECOLORSIZE	(R4K_MAXPCACHESIZE/NBPP)
#endif
#define CACHECOLORMASK	(CACHECOLORSIZE - 1)
#endif

/*
 * TLB size constants
 */

#if R3000
#define	NTLBENTRIES	64
#endif
#if R4000
#define	NTLBENTRIES	48
#endif

#if _PAGESZ == 4096
#define NWIREDENTRIES   8               /* WAG for now */
#endif
#if _PAGESZ == 16384
#define NWIREDENTRIES   6               /* WAG for now */
#endif
#if _PAGESZ == 65536
#define NWIREDENTRIES   4               /* WAG for now */
#endif
#define	TLBWIREDBASE	1
#define	TLBRANDOMBASE	NWIREDENTRIES
#define	NRANDOMENTRIES	(NTLBENTRIES-NWIREDENTRIES)

#define	TLBFLUSH_NONPDA	TLBWIREDBASE	/* flush all tlbs except PDA */
#define	TLBFLUSH_NONKERN TLBWIREDBASE+TLBKSLOTS /* all but PDA/UPG/KSTACK */
#define	TLBFLUSH_RANDOM	TLBRANDOMBASE	/* flush all random tlbs */

#define	TLBINX_PROBE	0x80000000

#if R3000
#define	TLBHI_VPNSHIFT		12
#define	TLBHI_VPNMASK		0xfffff000
#define	TLBHI_PIDMASK		0xfc0
#define	TLBHI_PIDSHIFT		6
#define	TLBHI_NPID		64

#define	TLBLO_PFNMASK		0xfffff000
#define	TLBLO_PFNSHIFT		12
#define	TLBLO_N			0x800		/* non-cacheable */
#define	TLBLO_D			0x400		/* writeable */
#define	TLBLO_V			0x200		/* valid bit */
#define	TLBLO_G			0x100		/* global access bit */

#define	TLBLO_FMT		"\20\14N\13D\12V\11G"

#define	TLBINX_INXMASK		0x00003f00
#define	TLBINX_INXSHIFT		8

#define	TLBRAND_RANDMASK	0x00003f00
#define	TLBRAND_RANDSHIFT	8

#define	TLBCTXT_BASEMASK	0xffe00000
#define	TLBCTXT_BASESHIFT	21

#define	TLBCTXT_VPNMASK		0x001ffffc
#define	TLBCTXT_VPNSHIFT	2
#endif	/* R3000 */

#if R4000
#if _PAGESZ == 4096
#define	TLBHI_VPNSHIFT		12
#define	TLBHI_VPNMASK		0xffffe000
#endif
#if _PAGESZ == 16384
#define	TLBHI_VPNSHIFT		14
#define	TLBHI_VPNMASK		0xffff8000
#endif
#if _PAGESZ == 65536
#define	TLBHI_VPNSHIFT		16
#define	TLBHI_VPNMASK		0xfffe0000
#endif
#define	TLBHI_VPN2MASK		TLBHI_VPNMASK	/* As named in the arch. spec.*/
#define	TLBHI_VPN2SHIFT		(TLBHI_VPNSHIFT+1)
#define	TLBHI_PIDMASK		0xff
#define	TLBHI_PIDSHIFT		0
#define	TLBHI_NPID		255		/* 255 to fit in 8 bits */

#define	TLBLO_PFNMASK		0x3fffffc0
#define	TLBLO_PFNSHIFT		6
#define	TLBLO_CACHMASK		0x38		/* cache coherency algorithm */
#define TLBLO_CACHSHIFT		3
#define TLBLO_UNCACHED		0x10		/* not cached */
#if _RUN_UNCACHED
#define TLBLO_NONCOHRNT		TLBLO_UNCACHED
#else
#define TLBLO_NONCOHRNT		0x18		/* Cacheable non-coherent */
#endif
#define TLBLO_EXLWR		0x28		/* Exclusive write */
#define	TLBLO_D			0x4		/* writeable */
#define	TLBLO_V			0x2		/* valid bit */
#define	TLBLO_G			0x1		/* global access bit */

#define	TLBINX_INXMASK		0x3f
#define	TLBINX_INXSHIFT		0

#define	TLBRAND_RANDMASK	0x3f
#define	TLBRAND_RANDSHIFT	0

#define	TLBWIRED_WIREDMASK	0x3f

#define	TLBCTXT_BASEMASK	0xff800000
#define	TLBCTXT_BASESHIFT	23
#define TLBCTXT_BASEBITS	9

#define	TLBCTXT_VPNMASK		0x7ffff0
#define	TLBCTXT_VPNSHIFT	4

#define TLBPGMASK_4K		0x0
#define TLBPGMASK_16K		0x6000
#define TLBPGMASK_64K		0x1e000

#if	_PAGESZ == 4096
#define	TLBPGMASK_MASK		TLBPGMASK_4K
#endif
#if	_PAGESZ == 16384
#define	TLBPGMASK_MASK		TLBPGMASK_16K
#endif
#if	_PAGESZ == 65536
#define	TLBPGMASK_MASK		TLBPGMASK_64K
#endif

#endif	/* R4000 */

/*
 * Status register
 */
#define	SR_CUMASK	0xf0000000	/* coproc usable bits */

#define	SR_CU3		0x80000000	/* Coprocessor 3 usable */
#define	SR_CU2		0x40000000	/* Coprocessor 2 usable */
#define	SR_CU1		0x20000000	/* Coprocessor 1 usable */
#define	SR_CU0		0x10000000	/* Coprocessor 0 usable */

/* Diagnostic status bits */
#if R3000
#define	SR_PE		0x00100000	/* cache parity error */
#define	SR_CM		0x00080000	/* cache miss */
#define	SR_PZ		0x00040000	/* cache parity zero */
#define	SR_SWC		0x00020000	/* swap cache */
#define	SR_ISC		0x00010000	/* Isolate data cache */
#endif	/* R3000 */

#if R4000
#define	SR_SR		0x00100000	/* soft reset occured */
#define	SR_CH		0x00040000	/* Cache hit for last 'cache' op */
#define	SR_CE		0x00020000	/* Create ECC */
#define	SR_DE		0x00010000	/* ECC of parity does not cause error */
#endif	/* R4000 */

#define	SR_TS		0x00200000	/* TLB shutdown */
#define	SR_BEV		0x00400000	/* use boot exception vectors */

/*
 * Interrupt enable bits
 * (NOTE: bits set to 1 enable the corresponding level interrupt)
 */
#define	SR_IMASK	0x0000ff00	/* Interrupt mask */
#if IP32
/*
 * Moosehead has different status register bit assignments and
 * uses an external interrupt controller.  Only softints and 
 * count/compare go directly to SR, CRIME controls all other 
 * external interrupt sources in the system.
 */
#define	SR_IMASK8	0x00000000	/* mask level 8 */
#define	SR_IMASK7	0x00008000	/* mask level 7 */
#define	SR_IMASK6	0x00000400	/* mask level 6 */
#define	SR_IMASK5	0x00008400	/* mask level 5 */
#define	SR_IMASK4	0x00008400	/* mask level 4 */
#define	SR_IMASK3	0x00008400	/* mask level 3 */
#define	SR_IMASK2	0x00008400	/* mask level 2 */
#define	SR_IMASK1	0x00008600	/* mask level 1 */
#define	SR_IMASK0	0x00008700	/* mask level 0 */
#else
#define	SR_IMASK8	0x00000000	/* mask level 8 */
#define	SR_IMASK7	0x00008000	/* mask level 7 */
#define	SR_IMASK6	0x0000c000	/* mask level 6 */
#define	SR_IMASK5	0x0000e000	/* mask level 5 */
#define	SR_IMASK4	0x0000f000	/* mask level 4 */
#define	SR_IMASK3	0x0000f800	/* mask level 3 */
#define	SR_IMASK2	0x0000fc00	/* mask level 2 */
#define	SR_IMASK1	0x0000fe00	/* mask level 1 */
#define	SR_IMASK0	0x0000ff00	/* mask level 0 */
#endif

#define	SR_IBIT8	0x00008000	/* bit level 8 */
#define	SR_IBIT7	0x00004000	/* bit level 7 */
#define	SR_IBIT6	0x00002000	/* bit level 6 */
#define	SR_IBIT5	0x00001000	/* bit level 5 */
#define	SR_IBIT4	0x00000800	/* bit level 4 */
#define	SR_IBIT3	0x00000400	/* bit level 3 */
#define	SR_IBIT2	0x00000200	/* bit level 2 */
#define	SR_IBIT1	0x00000100	/* bit level 1 */

#if R3000
#define	SR_KUO		0x00000020	/* old kernel/user, 0=>k, 1=>u */
#define	SR_IEO		0x00000010	/* old interrupt enable, 1=>enable */
#define	SR_KUP		0x00000008	/* prev kernel/user, 0=>k, 1=>u */
#define	SR_IEP		0x00000004	/* prev interrupt enable, 1=>enable */
#define	SR_KUC		0x00000002	/* cur kernel/user, 0=>k, 1=>u */
#define	SR_IEC		0x00000001	/* cur interrupt enable, 1=>enable */
#define	SR_PREVMODE	SR_KUP		/* previous kernel/user mode */
#define SR_KADDR	0		/* No kernel addressing bit */
#endif	/* R3000 */

#if R4000
#define	SR_RP		0x08000000	/* enable reduced-power operation */
#define	SR_FR		0x04000000	/* enable additional fp registers */
#define	SR_RE		0x02000000	/* reverse endian in user mode */

#define	SR_KX		0x00000080	/* extended-addr TLB vec in kernel */
#define	SR_SX		0x00000040	/* xtended-addr TLB vec supervisor */
#define	SR_UX		0x00000020	/* xtended-addr TLB vec in user mode */
#define	SR_KSU_MSK	0x00000018	/* 2 bit mode: 00b=>k, 10b=>u */
#define	SR_KSU_USR	0x00000010	/* 2 bit mode: 00b=>k, 10b=>u */
#define	SR_KSU_KS	0x00000008	/* 0-->kernel 1-->supervisor */
#define	SR_ERL		0x00000004	/* Error level, 1=>cache error */
#define	SR_EXL		0x00000002	/* Exception level, 1=>exception */
#define	SR_IE		0x00000001	/* interrupt enable, 1=>enable */
#define	SR_IEC		SR_IE		/* compat with R3000 source */
#define	SR_PREVMODE	SR_KSU_MSK	/* previous kernel/user mode */
#define SR_DM		0		/* No FP Debug Mode bits in SR */

/* SR_KADDR defines the desired state of the kernel address mode bits
 * in C0_SR, if such bits exist.  We could actually enable SR_KX when
 * compiled under 32-bit compilers, though there is no real reason to do so.
 */
#ifdef _K64U64
#define SR_KADDR	SR_KX		/* use kernel 64 bit addressing */
#else
#define SR_KADDR	0		/* use kernel 32 bit addressing */
#endif
#endif	/* R4000 */

#define	SR_IMASKSHIFT	8

#if IP32
#define SR_CRIME_INT_OFF	0xfffffbff
#define SR_CRIME_INT_ON		0x00000400
#endif

#if R3000 || IP17 || IP20 || IP22 || IP32 || IPMHSIM
/*
 * The following value is used as a flag to indicate whether
 * a status register value is saved in the pda.  This is for
 * assertions that check for nested spsemahi calls.  This value
 * can be any bit that does not conflict with the mips processor
 * level nor (on the IP6 with the lio mask value).
 */
#define OSPL_SPDBG	0x00000040
#endif /* R3000 || IP17 || IP22 */

#if R3000
#define	SR_FMT		"\20\40CU3\37CU2\36CU1\35CU0\27BEV\26TS\25PE\24CM\23PZ\22SwC\21IsC\20IM7\17IM6\16IM5\15IM4\14IM3\13IM2\12IM1\11IM0\6KUo\5IEo\4KUp\3IEp\2KUc\1IEc"
#endif

/*
 * Cause Register
 */
#define	CAUSE_BD	0x80000000	/* Branch delay slot */
#define	CAUSE_CEMASK	0x30000000	/* coprocessor error */
#define	CAUSE_CESHIFT	28

/* Interrupt pending bits */
#define	CAUSE_IP8	0x00008000	/* External level 8 pending */
#define	CAUSE_IP7	0x00004000	/* External level 7 pending */
#define	CAUSE_IP6	0x00002000	/* External level 6 pending */
#define	CAUSE_IP5	0x00001000	/* External level 5 pending */
#define	CAUSE_IP4	0x00000800	/* External level 4 pending */
#define	CAUSE_IP3	0x00000400	/* External level 3 pending */
#define	CAUSE_SW2	0x00000200	/* Software level 2 pending */
#define	CAUSE_SW1	0x00000100	/* Software level 1 pending */

#define	CAUSE_IPMASK	0x0000FF00	/* Pending interrupt mask */
#define	CAUSE_IPSHIFT	8

#if R3000
#define	CAUSE_EXCMASK	0x0000003C	/* Cause code bits */
#endif
#if R4000
#define	CAUSE_EXCMASK	0x0000007C	/* Cause code bits */
#endif
#define	CAUSE_EXCSHIFT	2

#define	CAUSE_FMT	"\20\40BD\36CE1\35CE0\20IP8\17IP7\16IP6\15IP5\14IP4\13IP3\12SW2\11SW1\1INT"

#define	setsoftclock()	siron(CAUSE_SW1)
#define	setsoftnet()	siron(CAUSE_SW2)
#define	acksoftclock()	siroff(CAUSE_SW1)
#define	acksoftnet()	siroff(CAUSE_SW2)

/* Cause register exception codes */

#define	EXC_CODE(x)	((x)<<2)

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
#if R4000
#define	EXC_TRAP	EXC_CODE(13)	/* Trap exception */
#define	EXC_VCEI	EXC_CODE(14)	/* Virt. Coherency on Inst. fetch */
#define	EXC_FPE		EXC_CODE(15)	/* Floating Point Exception */
#define	EXC_WATCH	EXC_CODE(23)	/* Watchpoint reference */
#define	EXC_VCED	EXC_CODE(31)	/* Virt. Coherency on data read */
#endif

/* software exception codes */
#define	SEXC_SEGV	EXC_CODE(32)	/* Software detected seg viol */
#define	SEXC_RESCHED	EXC_CODE(33)	/* resched request */
#define	SEXC_PAGEIN	EXC_CODE(34)	/* page-in request */
#define	SEXC_CPU	EXC_CODE(35)	/* coprocessor unusable */
#define SEXC_BUS	EXC_CODE(36)	/* software detected bus error */
#define SEXC_KILL	EXC_CODE(37)	/* bad page in process (vfault) */
#define SEXC_WATCH	EXC_CODE(38)	/* watchpoint (vfault) */
#if R4000
#define SEXC_EOP	EXC_CODE(39)	/* end-of-page trouble */
#endif
#ifdef _MEM_PARITY_WAR
#define SEXC_ECC_EXCEPTION EXC_CODE(40) /* ECC/Parity error recovery */
#endif /* _MEM_PARITY_WAR */

/* C0_PRID Defines */
/*
 * coprocessor revision identifiers
 */
#ifdef _KERNEL
#ifdef _LANGUAGE_C
typedef union rev_id {
	unsigned int	ri_uint;
	struct {
#ifdef MIPSEB
		unsigned int	Ri_fill:16,
				Ri_imp:8,		/* implementation id */
				Ri_majrev:4,		/* major revision */
				Ri_minrev:4;		/* minor revision */
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned int	Ri_minrev:4,		/* minor revision */
				Ri_majrev:4,		/* major revision */
				Ri_imp:8,		/* implementation id */
				Ri_fill:16;
#endif /* MIPSEL */
	} Ri;
} rev_id_t;
#define	ri_imp		Ri.Ri_imp
#define	ri_majrev	Ri.Ri_majrev
#define	ri_minrev	Ri.Ri_minrev
#endif /* _LANGUAGE_C */
#endif /* _KERNEL */

#define	C0_IMPMASK	0xff00
#define	C0_IMPSHIFT	8
#define C0_REVMASK	0xff
#define C0_MAJREVMASK	0xf0
#define	C0_MAJREVSHIFT	4
#define C0_MINREVMASK	0xf
#define C0_MINREVSHIFT	0

#define C0_IMP_UNDEFINED 0x29
#define	C0_IMP_RM5271 	0x28
#define	C0_IMP_R5000 	0x23
#define	C0_IMP_TRITON 	C0_IMP_R5000
#define	C0_IMP_R4650 	0x22 
#define	C0_IMP_R4700 	0x21
#define	C0_IMP_R4600 	0x20
#define	C0_IMP_R8000 	0x10
#define	C0_IMP_R6000A 	0x06
#define	C0_IMP_R4400 	0x04
#define C0_MAJREVMIN_R4400 0x04
#define	C0_IMP_R4000 	0x04
#define	C0_IMP_R6000 	0x03
#define	C0_IMP_R3000A 	0x02
#define C0_MAJREVMIN_R3000A 0x03
#define	C0_IMP_R3000 	0x02
#define C0_MAJREVMIN_R3000 0x02
#define	C0_IMP_R2000A 	0x02
#define C0_MAJREVMIN_R2000A 0x01
#define	C0_IMP_R2000 	0x01

#define C0_MAKE_REVID(x,y,z) (((x) << C0_IMPSHIFT) | \
			      ((y) << C0_MAJREVSHIFT) | \
			      ((z) << C0_MINREVSHIFT))

/*
 * Coprocessor 0 operations
 */
#define	C0_READI  0x1		/* read ITLB entry addressed by C0_INDEX */
#define	C0_WRITEI 0x2		/* write ITLB entry addressed by C0_INDEX */
#define	C0_WRITER 0x6		/* write ITLB entry addressed by C0_RAND */
#define	C0_PROBE  0x8		/* probe for ITLB entry addressed by TLBHI */
#define	C0_RFE	  0x10		/* restore for exception */
#define C0_WAIT   0x20		/* wait for interrupt */

#if R4000
/*
 * 'cache' instruction definitions
 */

/* Target cache */
#define	CACH_PI		0x0	/* specifies primary inst. cache */
#define	CACH_PD		0x1	/* primary data cache */
#define	CACH_SI		0x2	/* secondary instruction cache */
#define	CACH_SD		0x3	/* secondary data cache */

/* Cache operations */
#define	C_IINV		0x0	/* index invalidate (inst, 2nd inst) */
#define	C_IWBINV	0x0	/* index writeback inval (d, sd) */
#define	C_ILT		0x4	/* index load tag (all) */
#define	C_IST		0x8	/* index store tag (all) */
#define	C_CDX		0xc	/* create dirty exclusive (d, sd) */
#define	C_HINV		0x10	/* hit invalidate (all) */
#define	C_HWBINV	0x14	/* hit writeback inv. (d, sd) */
#define	C_FILL		0x14	/* fill (i) */
#define	C_HWB		0x18	/* hit writeback (i, d, sd) */
#define	C_HSV		0x1c	/* hit set virt. (si, sd) */
#ifdef TRITON
#define C_INVALL	0x0	/* Triton invalidate all (s) */
#define C_INVPAGE	0x14	/* Triton invalidate page (s) */
#endif /* TRITON */

/*
 * C0_CONFIG register definitions
 */
#define	CONFIG_CM	0x80000000	/* 1 == Master-Checker enabled */
#define	CONFIG_EC	0x70000000	/* System Clock ratio */
#define	CONFIG_EP	0x0f000000	/* Transmit Data Pattern */
#define	CONFIG_SB	0x00c00000	/* Secondary cache block size */

#define	CONFIG_SS	0x00200000	/* Split scache: 0 == I&D combined */
#define	CONFIG_SW	0x00100000	/* scache port: 0==128, 1==64 */
#define	CONFIG_EW	0x000c0000	/* System Port width: 0==64, 1==32 */
#define	CONFIG_SC	0x00020000	/* 0 -> 2nd cache present */
#define	CONFIG_SM	0x00010000	/* 0 -> Dirty Shared Coherency enabled*/
#define	CONFIG_BE	0x00008000	/* Endian-ness: 1 --> BE */
#define	CONFIG_EM	0x00004000	/* 1 -> ECC mode, 0 -> parity */
#define	CONFIG_EB	0x00002000	/* Block order:1->sequent,0->subblock */

#define	CONFIG_IC	0x00000e00	/* Primary Icache size */
#define	CONFIG_DC	0x000001c0	/* Primary Dcache size */
#define	CONFIG_IB	0x00000020	/* Icache block size */
#define	CONFIG_DB	0x00000010	/* Dcache block size */
#define	CONFIG_CU	0x00000008	/* Update on Store-conditional */
#define	CONFIG_K0	0x00000007	/* K0SEG Coherency algorithm */

#ifdef TRITON
#define CONFIG_TR_SS	0x00300000	/* Triton SS (2nd cache size) */
#define CONFIG_TR_SC	CONFIG_SC	/* Triton SC (2nd cache present) */
#define CONFIG_TR_SE    0x00001000	/* Triton SE (2nd cache enabled) (R/W) */
#endif /* TRITON */

#define	CONFIG_UNCACHED		0x00000002	/* K0 is uncached */
#if _RUN_UNCACHED
#define	CONFIG_NONCOHRNT	CONFIG_UNCACHED
#define	CONFIG_COHRNT_EXLWR	CONFIG_UNCACHED
#else
#define	CONFIG_NONCOHRNT	0x00000003
#define	CONFIG_COHRNT_EXLWR	0x00000005
#endif
#define	CONFIG_SB_SHFT	22		/* shift SB to bit position 0 */
#define	CONFIG_IC_SHFT	9		/* shift IC to bit position 0 */
#define	CONFIG_DC_SHFT	6		/* shift DC to bit position 0 */
#define	CONFIG_BE_SHFT	15		/* shift BE to bit position 0 */
#define CONFIG_IB_SHFT	5		/* shift IB to bit position 0 */
#define CONFIG_DB_SHFT	4		/* shift DB to bit position 0 */
#ifdef TRITON
#define CONFIG_TR_SS_SHFT 20		/* shift TR_SS to bit position 0 */
#endif /* TRITON */

/*
 * C0_TAGLO definitions for setting/getting cache states and physaddr bits
 */
#define SADDRMASK  	0xFFFFE000	/* 31..13 -> scache paddr bits 35..17 */
#define SVINDEXMASK	0x00000380	/* 9..7: prim virt index bits 14..12 */
#define SSTATEMASK	0x00001c00	/* bits 12..10 hold scache line state */
#define SINVALID	0x00000000	/* invalid --> 000 == state 0 */
#define SCLEANEXCL	0x00001000	/* clean exclusive --> 100 == state 4 */
#define SDIRTYEXCL	0x00001400	/* dirty exclusive --> 101 == state 5 */
#define SECC_MASK	0x0000007f	/* low 7 bits are ecc for the tag */
#define SADDR_SHIFT	4		/* shift STagLo (31..13) to 35..17 */

#define PADDRMASK	0xFFFFFF00	/* PTagLo31..8->prim paddr bits35..12 */
#define PADDR_SHIFT	4		/* roll bits 35..12 down to 31..8 */
#define PSTATEMASK	0x00C0		/* bits 7..6 hold primary line state */
#define PINVALID	0x0000		/* invalid --> 000 == state 0 */
#define PCLEANEXCL	0x0080		/* clean exclusive --> 10 == state 2 */
#define PDIRTYEXCL	0x00C0		/* dirty exclusive --> 11 == state 3 */
#define PPARITY_MASK	0x0001		/* low bit is parity bit (even). */

/*
 * C0_CACHE_ERR definitions.
 */
#define	CACHERR_ER		0x80000000	/* 0: inst ref, 1: data ref */
#define	CACHERR_EC		0x40000000	/* 0: primary, 1: secondary */
#define	CACHERR_ED		0x20000000	/* 1: data error */
#define	CACHERR_ET		0x10000000	/* 1: tag error */
#define	CACHERR_ES		0x08000000	/* 1: external ref, e.g. snoop*/
#define	CACHERR_EE		0x04000000	/* error on SysAD bus */
#define	CACHERR_EB		0x02000000	/* complicated, see spec. */
#define	CACHERR_EI		0x01000000	/* complicated, see spec. */
#define	CACHERR_SIDX_MASK	0x003ffff8	/* secondary cache index */
#define	CACHERR_PIDX_MASK	0x00000007	/* primary cache index */
#define CACHERR_PIDX_SHIFT	12		/* bits 2..0 are paddr14..12 */

/* R4000 family supports hardware watchpoints:
 *   C0_WATCHLO:
 *     bits 31..3 are bits 31..3 of physaddr to watch
 *     bit 2:  reserved; must be written as 0.
 *     bit 1:  when set causes a watchpoint trap on load accesses to paddr.
 *     bit 0:  when set traps on stores to paddr;
 *   C0_WATCHHI
 *     bits 31..4 are reserved and must be written as zeros.
 *     bits 3..0 are bits 35..32 of the physaddr to watch
 */
#define WATCHLO_WTRAP           0x00000001
#define WATCHLO_RTRAP           0x00000002
#define WATCHLO_ADDRMASK        0xfffffff8
#define WATCHLO_VALIDMASK       0xfffffffb
#define WATCHHI_VALIDMASK       0x0000000f

#endif	/* R4000 */

/*
 * Coprocessor 0 registers
 * Some of these are r4000 specific.
 */
#ifdef _LANGUAGE_ASSEMBLY
#define	C0_INX		$0
#define	C0_RAND		$1
#define	C0_TLBLO	$2
#define	C0_CTXT		$4
#define	C0_BADVADDR	$8
#define	C0_TLBHI	$10
#define	C0_SR		$12
#define	C0_CAUSE	$13
#define	C0_EPC		$14
#define	C0_PRID		$15		/* revision identifier */

#if R4000
#define	C0_TLBLO_0	$2
#define	C0_TLBLO_1	$3
#define	C0_PGMASK	$5		/* page mask */
#define	C0_TLBWIRED	$6		/* # wired entries in tlb */
#define	C0_COUNT	$9		/* free-running counter */
#define	C0_COMPARE	$11		/* counter comparison reg. */
#define	C0_CONFIG	$16		/* hardware configuration */
#define	C0_LLADDR	$17		/* load linked address */
#define	C0_WATCHLO	$18		/* watchpoint */
#define	C0_WATCHHI	$19		/* watchpoint */
#define	C0_EXTCTXT	$20		/* Extended context */
#define	C0_ECC		$26		/* S-cache ECC and primary parity */
#define	C0_CACHE_ERR	$27		/* cache error status */
#define	C0_TAGLO	$28		/* cache operations */
#define	C0_TAGHI	$29		/* cache operations */
#define	C0_ERROR_EPC	$30		/* ECC error prg. counter */
#endif	/* R4000 */

# else	/* ! _LANGUAGE_ASSEMBLY */
#define	C0_INX		0
#define	C0_RAND		1
#define	C0_TLBLO	2
#define	C0_CTXT		4
#define	C0_BADVADDR	8
#define	C0_TLBHI	10
#define	C0_SR		12
#define	C0_CAUSE	13
#define	C0_EPC		14
#define	C0_PRID		15		/* revision identifier */

#if R4000
#define	C0_TLBLO_0	2
#define	C0_TLBLO_1	3
#define	C0_PGMASK	5		/* page mask */
#define	C0_TLBWIRED	6		/* # wired entries in tlb */
#define	C0_COUNT	9		/* free-running counter */
#define	C0_COMPARE	11		/* counter comparison reg. */
#define	C0_CONFIG	16		/* hardware configuration */
#define	C0_LLADDR	17		/* load linked address */
#define	C0_WATCHLO	18		/* watchpoint */
#define	C0_WATCHHI	19		/* watchpoint */
#define	C0_ECC		26		/* S-cache ECC and primary parity */
#define	C0_CACHE_ERR	27		/* cache error status */
#define	C0_TAGLO	28		/* cache operations */
#define	C0_TAGHI	29		/* cache operations */
#define	C0_ERROR_EPC	30		/* ECC error prg. counter */
#endif	/* R4000 */

#endif	/* _LANGUAGE_ASSEMBLY */

#endif	/* R3000 || R4000 */

#endif /* __SYS_SBD_H__ */
