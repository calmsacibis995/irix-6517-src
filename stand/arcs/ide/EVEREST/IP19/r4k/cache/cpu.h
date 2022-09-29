#ifndef __IDE_CPU_H__
#define __IDE_CPU_H__
#define __SYS_SBD_H__	/* HACK TO HUSH COMPILER WARNINGS */
/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
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

#ident "$Revision: 1.3 $"

/*
 * ide/EVEREST/IP19/r4k/cache/libcpu.h -- cpu-specific defines
 */

/*
 * Segment base addresses and sizes
 */
#define	K0BASE		0x80000000
#define	K0SIZE		0x20000000
#define	K1BASE		0xA0000000
#define	K1SIZE		0x20000000
#define	K2BASE		0xC0000000
#define	K2SIZE		0x20000000
/*
 * KPTE window is 2 megabytes long and is placed
 * 4 Meg from the top of kseg2, to leave space for upage). 
 * KPTE windown must be on a 2 Meg boundary.
 * (Note: this definition relies upon 2's complement arithmetic!)
 */
#define	KPTEBASE	(-0x400000)
#define	KPTESIZE	0x200000
#define	KUBASE		0
#define	KUSIZE		0x80000000

/*
 * Exception vectors
 */
#define	UT_VEC		K0BASE			/* utlbmiss vector */
#define	E_VEC		(K0BASE+0x80)		/* exception vector */
#define	R_VEC		(K1BASE+0x1fc00000)	/* reset vector */
/* for R4000 */
#define	UT_VEC_4000	K0BASE			/* utlbmiss vector */
#define	XT_VEC_4000	(K0BASE+0x80)		/* utlbmiss vector for xtlb */
#define	C_VEC_4000	(K0BASE+0x100)		/* exception vector */
#define	E_VEC_4000	(K0BASE+0x180)		/* exception vector */
#define	R_VEC_4000	(K1BASE+0x1fc00000)	/* reset vector for */

/*
 * Address conversion macros
 */
#define	K0_TO_K1(x)	((unsigned)(x)|0xA0000000)	/* kseg0 to kseg1 */
#define	K1_TO_K0(x)	((unsigned)(x)&0x9FFFFFFF)	/* kseg1 to kseg0 */
#define	K0_TO_PHYS(x)	((unsigned)(x)&0x1FFFFFFF)	/* kseg0 to physical */
#define	K1_TO_PHYS(x)	((unsigned)(x)&0x1FFFFFFF)	/* kseg1 to physical */
#define	PHYS_TO_K0(x)	((unsigned)(x)|0x80000000)	/* physical to kseg0 */
#define	PHYS_TO_K1(x)	((unsigned)(x)|0xA0000000)	/* physical to kseg1 */

/*
 * Address predicates
 */
#define	IS_KSEG0(x)	((unsigned)(x) >= K0BASE && (unsigned)(x) < K1BASE)
#define	IS_KSEG1(x)	((unsigned)(x) >= K1BASE && (unsigned)(x) < K2BASE)
#define	IS_KSEG2(x)	((unsigned)(x) >= K2BASE && (unsigned)(x) < KPTEBASE)
#define	IS_KPTESEG(x)	((unsigned)(x) >= KPTEBASE)
#define	IS_KUSEG(x)	((unsigned)(x) < K0BASE)

/*
 * Cache size constants
 */
#define	MINCACHE	+(4*1024)	/* leading plus for mas's benefit */
#define	MAXCACHE	+(64*1024)	/* leading plus for mas's benefit */

/*
 * TLB size constants
 */
#define	TLBWIREDBASE	0
#define	NWIREDENTRIES	8
#define	TLBRANDOMBASE	NWIREDENTRIES
#define	NRANDOMENTRIES	(NTLBENTRIES-NWIREDENTRIES)

#define NTLBENTRIES	64   	/* for 2000, 3000 */
#define NTLBENTRIES_4000 48	/* for 4000s */
#define NTLBENTRIES_6000 2048	/* for 6000s */

/* only for 6000 */
#define NPTAGENTRIES	2048 - 256    /* 2k - tlb and ptag lines */

/*
 * tlb entrylo format
 */
#ifndef LOCORE
union tlb_lo {
	unsigned tl_word;		/* efficient access */
	struct {
#ifdef MIPSEB
		unsigned tls_pfn:20;	/* physical page frame number */
		unsigned tls_n:1;	/* non-cacheable */
		unsigned tls_d:1;	/* dirty (actually writeable) */
		unsigned tls_v:1;	/* valid */
		unsigned tls_g:1;	/* match any pid */
		unsigned :8;
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned :8;
		unsigned tls_g:1;	/* match any pid */
		unsigned tls_v:1;	/* valid */
		unsigned tls_d:1;	/* dirty (actually writeable) */
		unsigned tls_n:1;	/* non-cacheable */
		unsigned tls_pfn:20;	/* physical page frame number */
#endif /* MIPSEL */
	} tl_struct;
	struct {
#ifdef MIPSEB
		unsigned :2;
		unsigned tls_pfn:24;	/* physical page frame number */
		unsigned tls_c:3;	/* cache algorithm */
		unsigned tls_d:1;	/* dirty (actually writeable) */
		unsigned tls_v:1;	/* valid */
		unsigned tls_g:1;	/* valid */
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned tls_g:1;	/* valid */
		unsigned tls_v:1;	/* valid */
		unsigned tls_d:1;	/* dirty (actually writeable) */
		unsigned tls_c:3;	/* cache algorithm */
		unsigned tls_pfn:24;	/* physical page frame number */
		unsigned :2;
#endif /* MIPSEL */
	} tl_struct_4000;
};

#define	tl_pfn		tl_struct.tls_pfn
#define	tl_n		tl_struct.tls_n
#define	tl_d		tl_struct.tls_d
#define	tl_v		tl_struct.tls_v
#define	tl_g		tl_struct.tls_g

#define tl_pfn_4000	tl_struct_4000.tls_pfn
#define tl_c_4000	tl_struct_4000.tls_c
#define tl_d_4000	tl_struct_4000.tls_d
#define tl_v_4000	tl_struct_4000.tls_v
#define tl_g_4000	tl_struct_4000.tls_g
#endif /* !LOCORE */

#define	TLBLO_PFNMASK	0xfffff000
#define	TLBLO_PFNSHIFT	12
#define	TLBLO_N		0x800		/* non-cacheable */
#define	TLBLO_D		0x400		/* writeable */
#define	TLBLO_V		0x200		/* valid bit */
#define	TLBLO_G		0x100		/* global access bit */

#define TLBLO_PFNMASK_4000	0x3fffffc0
#define TLBLO_PFNSHIFT_4000	6
#define TLBLO_C_4000		0x38	/* cache algorithm */
#define TLBLO_CSHIFT_4000	3
#define TLBLO_D_4000		0x4	/* writeable */
#define TLBLO_V_4000		0x2	/* valid bit */
#define TLBLO_G_4000		0x1     /* global */

#define	TLBLO_FMT	"\20\14N\13D\12V\11G"

#define	TLB_PFNMASK_R6000	0xfffffc00
#define TLB_PFNSHIFT_R6000	10	/* convert pfn to tlb_pfn */
#define	TLB_N_R6000		0x8	/* non-cacheable */
#define	TLB_D_R6000		0x4	/* writeable */
#define	TLB_V_R6000		0x2	/* valid bit */
#define	TLB_G_R6000		0x1	/* global access bit */

/*
 * TLB entryhi format
 */
#ifndef LOCORE
union tlb_hi {
	unsigned th_word;		/* efficient access */
	struct {
#ifdef MIPSEB
		unsigned ths_vpn:20;	/* virtual page number */
		unsigned ths_pid:6;
		unsigned :6;
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned :6;
		unsigned ths_pid:6;
		unsigned ths_vpn:20;	/* virtual page number */
#endif /* MIPSEL */
	} th_struct;
	struct {
#ifdef MIPSEB
		unsigned ths_vpn2:19;   /* virtual page number */
		unsigned ths_g:1;
		unsigned :4;
		unsigned ths_pid:8;
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned ths_pid:8;
		unsigned :4;
		unsigned ths_g:1;
		unsigned ths_vpn2:19;   /* virtual page number */
#endif /* MIPSEL */
	} th_struct_4000;
};

#define	th_vpn		th_struct.ths_vpn
#define	th_pid		th_struct.ths_pid

#define th_vpn2_4000	th_struct_4000.ths_vpn2
#define th_pid_4000	th_struct_4000.ths_pid
#define th_g_4000	th_struct_4000.ths_g
#endif /* !LOCORE */

#define	TLBHI_VPNMASK	0xfffff000
#define	TLBHI_VPNSHIFT	12
#define	TLBHI_PIDMASK	0xfc0
#define	TLBHI_PIDSHIFT	6
#define	TLBHI_NPID	64

#define TLBHI_VPN2MASK_4000	0xffffe000
#define TLBHI_VPN2SHIFT_4000	13
#define TLBHI_WHATENTRY_4000	0x1000
#define TLBHI_PIDMASK_4000	0xff
#define TLBHI_PIDSHIFT_4000	0
#define	TLBHI_NPID_4000		256

#define TLBPGMASK_MASK		0x01ffe000
#define TLBPGMASK_SHIFT		13
/* valid values for PageMask */
#define	PGMASK_4K		0x0
#define	PGMASK_16K		0x00006000
#define	PGMASK_64K		0x0001e000
#define	PGMASK_256K		0x0007e000
#define	PGMASK_1M		0x001fe000
#define	PGMASK_4M		0x007fe000
#define	PGMASK_16M		0x01ffe000

#define VPNMASK_R6000	0xffffc000
#define VPNSHIFT_R6000	14	/* convert virtual byte to page addr */
#define PHYS_TO_TLB_R6000(x)	(((x) >>VPNSHIFT_R6000) <<TLB_PFNSHIFT_R6000)
#define TLB_TO_PHYS_R6000(x)	(((x) >>TLB_PFNSHIFT_R6000) <<VPNSHIFT_R6000)
#define	PIDMASK_R6000		0xff
#define TLB_NPID_R6000	256

/*
 * TLB index register
 */
#ifndef LOCORE
union tlb_inx {
	unsigned ti_word;
	struct {
#ifdef MIPSEB
		unsigned tis_probe:1;	/* 1 => probe failure */
		unsigned :17;
		unsigned tis_inx:6;	/* tlb index for TLBWRITEI op */
		unsigned :8;
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned :8;
		unsigned tis_inx:6;	/* tlb index for TLBWRITEI op */
		unsigned :17;
		unsigned tis_probe:1;	/* 1 => probe failure */
#endif /* MIPSEL */
	} ti_struct;
	struct {
#ifdef MIPSEB
		unsigned tis_probe:1;	/* 1 => probe failure */
		unsigned :25;
		unsigned tis_inx:6;	/* tlb index for TLBWRITEI op */
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned tis_inx:6;	/* tlb index for TLBWRITEI op */
		unsigned :25;
		unsigned tis_probe:1;	/* 1 => probe failure */
#endif /* MIPSEL */
	} ti_struct_4000;
};

#define	ti_probe	ti_struct.tis_probe
#define	ti_inx		ti_struct.tis_inx

#define ti_probe_4000	ti_struct_4000.tis_probe
#define ti_inx_4000	ti_struct_4000.tis_inx
#endif /* !LOCORE */

#define	TLBINX_PROBE		0x80000000
#define	TLBINX_INXMASK		0x00003f00
#define	TLBINX_INXSHIFT		8

#define TLBINX_PROBE_4000	0x80000000
#define TLBINX_INXMASK_4000	0x0000003f
#define TLBINX_INXSHIFT_4000	0

/*
 * TLB random register
 */
#ifndef LOCORE
union tlb_rand {
	unsigned tr_word;
	struct {
#ifdef MIPSEB
		unsigned :18;
		unsigned trs_rand:6;	/* tlb index for TLBWRITER op */
		unsigned :8;
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned :8;
		unsigned trs_rand:6;	/* tlb index for TLBWRITER op */
		unsigned :18;
#endif /* MIPSEL */
	} tr_struct;
	struct {
#ifdef MIPSEB
		unsigned :26;
		unsigned trs_rand:6;	/* tlb index for TLBWRITER op */
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned trs_rand:6;	/* tlb index for TLBWRITER op */
		unsigned :26;
#endif /* MIPSEL */
	} tr_struct_4000;
};

#define	tr_rand		ti_struct.tis_rand
#define	tr_rand_4000	ti_struct_4000.tis_rand
#endif /* !LOCORE */

#define	TLBRAND_RANDMASK	0x00003f00
#define	TLBRAND_RANDSHIFT	8

#define	TLBRAND_RANDMASK_4000	0x0000003f
#define	TLBRAND_RANDSHIFT_4000	0

/*
 * TLB context register
 */
#ifndef LOCORE
union tlb_ctxt {
	unsigned tc_word;		/* efficient access */
	struct {
#ifdef MIPSEB
		unsigned tcs_pteseg:11;	/* bits 21-31 of kernel pte window */
		unsigned tcs_vpn:19;	/* vpn of faulting ref (ro) */
		unsigned :2;
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned :2;
		unsigned tcs_vpn:19;	/* vpn of faulting ref (ro) */
		unsigned tcs_pteseg:11;	/* bits 22-31 of kernel pte window */
#endif /* MIPSEL */
	} tc_struct;
	struct {
#ifdef MIPSEB
		unsigned tcs_pteseg:11;	/* bits 21-31 of kernel pte window */
		unsigned tcs_vpn:19;	/* vpn of faulting ref (ro) */
		unsigned :2;
#endif /* MIPSEB */
#ifdef MIPSEL
		unsigned :2;
		unsigned tcs_vpn:19;	/* vpn of faulting ref (ro) */
		unsigned tcs_pteseg:11;	/* bits 22-31 of kernel pte window */
#endif /* MIPSEL */
	} tc_struct_4000;
};

#define	tc_pteseg	tc_struct.tcs_pteseg
#define	tc_vpn		tc_struct.tcs_vpn

#define	tc_pteseg_4000	tc_struct_4000.tcs_pteseg
#define	tc_vpn_4000	tc_struct_4000.tcs_vpn
#endif /* !LOCORE */

#define	TLBCTXT_BASEMASK	0xffe00000
#define	TLBCTXT_BASESHIFT	21

#define	TLBCTXT_VPNMASK		0x001ffffc
#define	TLBCTXT_VPNSHIFT	2

#define	TLBCTXT_BASEMASK_4000	0xff800000
#define	TLBCTXT_BASESHIFT_4000	23

#define	TLBCTXT_VPNMASK_4000	0x001ffff0
#define	TLBCTXT_VPNSHIFT_4000	4

/*
 * Status register
 */
#define	SR_CUMASK	0xf0000000	/* coproc usable bits */

#define	SR_CU3		0x80000000	/* Coprocessor 3 usable */
#define	SR_CU2		0x40000000	/* Coprocessor 2 usable */
#define	SR_CU1		0x20000000	/* Coprocessor 1 usable */
#define	SR_CU0		0x10000000	/* Coprocessor 0 usable */

#define SR_FR		0x04000000	/* enables additional FP registers */
#define SR_RE		0x02000000	/* reverse endian */
#define	SR_BEV		0x00400000	/* use boot exception vectors */

/* Cache control bits */
#define	SR_TS		0x00200000	/* TLB shutdown */
#define	SR_PE		0x00100000	/* cache parity error */
#define	SR_CM		0x00080000	/* cache miss */
#define SR_CM1		0x00100000	/* cache miss, side 1 (R6000) */
#define SR_CM0		0x00080000	/* cache miss, side 0 (R6000) */
#define	SR_PZ		0x00040000	/* cache parity zero */
#define	SR_SWC		0x00020000	/* swap cache */
#define	SR_ITP		0x00020000	/* invert tag parity (R6000) */
#define	SR_ISC		0x00010000	/* Isolate data cache */

#define SR_MM_MODE	0x00010000	/* lwl/swl/etc become scache/etc */
#define lcache		lwl
#define scache		swl
#define flush		lwr $0,
#define inval		swr $0,

/*
 * Interrupt enable bits
 * (NOTE: bits set to 1 enable the corresponding level interrupt)
 */
#define	SR_IMASK	0x0000ff00	/* Interrupt mask */
#define	SR_IMASK8	0x00000000	/* mask level 8 */
#define	SR_IMASK7	0x00008000	/* mask level 7 */
#define	SR_IMASK6	0x0000c000	/* mask level 6 */
#define	SR_IMASK5	0x0000e000	/* mask level 5 */
#define	SR_IMASK4	0x0000f000	/* mask level 4 */
#define	SR_IMASK3	0x0000f800	/* mask level 3 */
#define	SR_IMASK2	0x0000fc00	/* mask level 2 */
#define	SR_IMASK1	0x0000fe00	/* mask level 1 */
#define	SR_IMASK0	0x0000ff00	/* mask level 0 */

#define	SR_IBIT8	0x00008000	/* bit level 8 */
#define	SR_IBIT7	0x00004000	/* bit level 7 */
#define	SR_IBIT6	0x00002000	/* bit level 6 */
#define	SR_IBIT5	0x00001000	/* bit level 5 */
#define	SR_IBIT4	0x00000800	/* bit level 4 */
#define	SR_IBIT3	0x00000400	/* bit level 3 */
#define	SR_IBIT2	0x00000200	/* bit level 2 */
#define	SR_IBIT1	0x00000100	/* bit level 1 */

#define	SR_KUO		0x00000020	/* old kernel/user, 0 => k, 1 => u */
#define	SR_IEO		0x00000010	/* old interrupt enable, 1 => enable */
#define	SR_KUP		0x00000008	/* prev kernel/user, 0 => k, 1 => u */
#define	SR_IEP		0x00000004	/* prev interrupt enable, 1 => enable */
#define	SR_KUC		0x00000002	/* cur kernel/user, 0 => k, 1 => u */
#define	SR_IEC		0x00000001	/* cur interrupt enable, 1 => enable */

#define	SR_IMASKSHIFT	8

/* R4000 Diagnostic status bits - only the different or new bits */
#define SR_SR		0x00100000	/* Soft reset has occured */
#define SR_CH		0x00040000	/* Hit or miss indication of last 
					   CACHE Hit cache op or CACHE Create 
					   Dirty Exc cache op on secondary */
#define SR_CE		0x00020000	/* If set, the contents of the ECC 
					   register are used to set/modify 
					   the checks bit in the caches */
#define SR_DE		0x00010000	/* Specifies that cache parity or ECC 
					   errors do NOT cause exceptions */

#define SR_KX		0x00000080	/* Kernel mode is Mips III */
#define SR_SX		0x00000040	/* Supervisor mode is Mips III */
#define SR_UX		0x00000020	/* User mode is Mips III */
#define SR_KSU		0x00000018	/* Kernel/Supervisor/User mode */
#define SR_KSU_SHIFT	0x00000003	/* Kernel/Supervisor/User shift */
#define SR_KSU_KERN	0x00000000	/*    Kernel      */
#define SR_KSU_SUPR	0x00000008	/*    Supervisor  */
#define SR_KSU_USER	0x00000010	/*    User        */

#define SR_ERL		0x00000004	/* Error level,     0=normal, 1=err */
#define SR_EXL		0x00000002	/* Exception level, 0=normal, 1=exc */
#define SR_IE		0x00000001	/* Interrupt Enable 0=disable, 1=ena */

#define	SR_FMT		"\20\40BD\26TS\25PE\24CM\23PZ\22SwC\21IsC\20IM7\17IM6\16IM5\15IM4\14IM3\13IM2\12IM1\11IM0\6KUo\5IEo\4KUp\3IEp\2KUc\1IEc"

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

#define	CAUSE_EXCMASK	0x0000007C	/* Cause code bits */
#define	CAUSE_EXCSHIFT	2

#define	CAUSE_FMT	"\20\40BD\36CE1\35CE0\20IP8\17IP7\16IP6\15IP5\14IP4\13IP3\12SW2\11SW1\1INT"

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
#define EXC_TRAP	EXC_CODE(13)	/* TRAP */
#define EXC_DBL_NC	EXC_CODE(14)	/* DouBLeword Non-Cached access */
#define EXC_CHECK	EXC_CODE(15)	/* Machine Check */

/* software exception codes */
#define	SEXC_SEGV	EXC_CODE(16)	/* Software detected seg viol */
#define	SEXC_RESCHED	EXC_CODE(17)	/* resched request */
#define	SEXC_PAGEIN	EXC_CODE(18)	/* page-in request */
#define	SEXC_CPU	EXC_CODE(19)	/* coprocessor unusable */

/* R4000 only exception codes */
#define EXC_VCEI	EXC_CODE(14)	/* Virtual Coherency Exception Instr */ 
#define EXC_FPE		EXC_CODE(15)	/* Floating Point Exception */
#define EXC_WATCH	EXC_CODE(23)	/* Watch Exception */
#define EXC_VCED	EXC_CODE(31)	/* Virtual Coherency Exception Data */ 


#define C0_ERROR_IEXT	0x00080000	/* ignore external parity err */
#define C0_ERROR_IRF	0x00040000	/* ignore register parity err */
#define C0_ERROR_IDB	0x00020000	/* ignore data bus parity err */
#define C0_ERROR_IIB	0x00010000	/* ignore instr bus parity err */
#define C0_ERROR_ECCA	0x00000080	/* enable cache coherency (R6000A) */
#define C0_ERROR_DCCA	0x00000070	/* default cache coherency (R6000A) */
#define C0_ERROR_CMASK	(C0_ERROR_ECCA | C0_ERROR_DCCA)
#define C0_ERROR_IMASK	(C0_ERROR_IEXT|C0_ERROR_IRF|C0_ERROR_IDB|C0_ERROR_IIB)
#define C0_ERROR_EXT	0x00000008	/* external parity err */
#define C0_ERROR_RF	0x00000004	/* register parity err */
#define C0_ERROR_DB	0x00000002	/* data bus parity err */
#define C0_ERROR_IB	0x00000001	/* instr bus parity err */
#define C0_ERROR_MASK	(C0_ERROR_EXT|C0_ERROR_RF|C0_ERROR_DB|C0_ERROR_IB)


/***************************/
/*** new R4000 registers ***/
/***************************/
/*
 * Config register
 */
#define CONFIG_MCHECK	0x80000000	/* MasterChecker mode if set */
#define CONFIG_CLKRAT	0x70000000	/* System Clock Ratio
					   0= /2, 1= /3, 2= /4 */
#define CONFIG_DATA_PAT	0x0f000000	/* Xmit data pattern */
#define CONFIG_SB	0x00c00000	/* Secondary cache block  (read-write)
					   size. 00=32 bytes, 01=64 bytes
					   10=128 bytes. */
#define CONFIG_SB_SHIFT 0x00000016
#define CONFIG_SPLIT	0x00200000	/* Split Secondary cache if set */
#define CONFIG_SW	0x00100000	/* Secondary cache port (read-only)
					   port width.  0=64 bits, 1=128 bits */
#define CONFIG_SPW	0x000c0000	/* System port width */
#define CONFIG_NOSC	0x00020000	/* Secondary cache present
					   0=Secondary, 1=NO Secondary */
#define CONFIG_DSHARE	0x00010000	/* Dirty Shared mode disabled if set */
#define CONFIG_BE	0x00008000	/* Big Endian CPU if set */
#define CONFIG_ECC	0x00004000	/* ECC enabled if set */
#define CONFIG_BO	0x00002000	/* Block ordering
					   0=sub-block, 1=sequential */
#define CONFIG_IC	0x00000e00	/* Instruction cache size (read-only)
					   Size=2^(12+IC) bytes */
#define CONFIG_IC_SHIFT	0x00000009
#define CONFIG_DC	0x000001C0	/* Data cache size        (read-only)
					   Size=2^(12+DC) bytes */
#define CONFIG_DC_SHIFT	0000000006
#define CONFIG_IB	0x00000020	/* Instruction cache      (read-write)
					   block size.  0=16 bytes, 1=32 bytes */
#define CONFIG_DB	0x00000010	/* Data cache block size  (read-write)
					   0=16 bytes, 1=32 bytes */
#define CONFIG_UPSC	0x00000008	/* Update on Store Conditional disabled
					   if set */

#define CONFIG_K0C	0x00000007	/* KSeg0 coherency algorithm */

#define CONFIG_RWBITS	0x0000003F	/* Mask for read-write bits */

                                           
/*
 * TagLo and TagHi defines
 */
#define TAG_PTAG_MASK           0xffffff00      /* Primary Tag */
#define TAG_PTAG_SHIFT          0x00000008
#define TAG_PSTATE_MASK         0x000000c0      /* Primary Cache State */
#define TAG_PSTATE_SHIFT        0x00000006
#define TAG_PARITY_MASK         0x00000001      /* Primary Tag Parity */
#define TAG_PARITY_SHIFT        0x00000000

#define TAG_STAG_MASK           0xffffe000      /* Secondary Tag */
#define TAG_STAG_SHIFT          0x0000000d
#define TAG_SSTATE_MASK         0x00001c00      /* Secondary Cache State */
#define TAG_SSTATE_SHIFT        0x0000000a
#define TAG_VINDEX_MASK         0x00000380      /* Secondary Virtual Index */
#define TAG_VINDEX_SHIFT        0x00000007
#define TAG_ECC_MASK            0x0000007f      /* Secondary Tag ECC */
#define TAG_ECC_SHIFT           0x00000000
#define TAG_STAG_SIZE			19	/* Secondar Tag Width in bits */

/*
 * Watch mask
 */
#define WATCHLO_MASK		0xfffffff8
#define WATCHHI_MASK		0x0000000f
#define WATCH_READ		0x00000002
#define WATCH_WRITE		0x00000001
#define WATCH_BITMASK		0x00000007

/*
 * Cache Err mask
 */
#define CACHEERR_TYPE		0x80000000	/* reference type: 
						   0=Instr, 1=Data */
#define CACHEERR_LEVEL		0x40000000	/* cache level:
						   0=Primary, 1=Secondary */
#define CACHEERR_DATA		0x20000000	/* data field:
						   0=No error, 1=Error */
#define CACHEERR_TAG		0x10000000	/* tag field:
						   0=No error, 1=Error */
#define CACHEERR_REQ		0x08000000	/* request type:
						   0=Internal, 1=External */
#define CACHEERR_BUS		0x04000000	/* error on bus:
						   0=No, 1=Yes */
#define CACHEERR_BOTH		0x02000000	/* Data & Instruction error:
						   0=No, 1=Yes */
#define CACHEERR_ECC		0x01000000	/* ECC error :
						   0=No, 1=Yes */
#define CACHEERR_SIDX		0x003ffff8	/* PADDR(21..3) */
#define CACHEERR_SIDX_SHIFT		 3	/* PADDR(21..3) */
#define CACHEERR_PIDX		0x00000003	/* VADDR(14..12) */
#define CACHEERR_PIDX_SHIFT		12	/* VADDR(14..12) */

/*
 * R4000 Cache operations
 *
 *
 * 	CACHE	op, offset(base)
 *
 *	31	26 25		21 20		16 15			0
 *	-----------------------------------------------------------------
 *	| CACHE   | 	base      |     OP        |	offset          |
 *      -----------------------------------------------------------------
 *
 *     Bits 17..16
 *    Code	Name		Cache
 *	0	 I		Primary Instruction
 *	1        D		Primary Data
 *	2	 SI             Secondary Instruction
 *	3        SD             Secondary Data (combined I/D).
 *
 *     Bits 20..18
 *    Code    Caches		Name
 *	0	I,SI		Index Invalidate
 *	0	D,SD		Index Writeback Invalidate
 *	1     	all		Index Load Tag
 *	2     	all		Index Store Tag
 *	3      	SD	        Create Dirty Exclusive
 *	3      	D               Create Dirty  Exclusive
 *	4      	all	        Hit Invalidate
 *	5      	D,SD            Hit Writeback Invalidate
 *	5      	I		Fill
 *	6      	D,SD            Hit Writeback
 *	6      	I               Hit Writeback
 *	7      	SI,SD           Hit Set Virtual
 *
 *	OP(20..16)			Code	      20..18 17..16
 *      ----------------------          ----          -------------
 */
#define Index_Invalidate_I               0x0         /* 0       0 */
#define Index_Writeback_Inv_D            0x1         /* 0       1 */
#define Index_Invalidate_SI              0x2         /* 0       2 */
#define Index_Writeback_Inv_SD           0x3         /* 0       3 */
#define Index_Load_Tag_I                 0x4         /* 1       0 */
#define Index_Load_Tag_D                 0x5         /* 1       1 */
#define Index_Load_Tag_SI                0x6         /* 1       2 */
#define Index_Load_Tag_SD                0x7         /* 1       3 */
#define Index_Store_Tag_I                0x8         /* 2       0 */
#define Index_Store_Tag_D                0x9         /* 2       1 */
#define Index_Store_Tag_SI               0xA         /* 2       2 */
#define Index_Store_Tag_SD               0xB         /* 2       3 */
#define Create_Dirty_Exc_D               0xD         /* 3       1 */
#define Create_Dirty_Exc_SD              0xF         /* 3       3 */
#define Hit_Invalidate_I                 0x10        /* 4       0 */
#define Hit_Invalidate_D                 0x11        /* 4       1 */
#define Hit_Invalidate_SI                0x12        /* 4       2 */
#define Hit_Invalidate_SD                0x13        /* 4       3 */
#define Hit_Writeback_Inv_D              0x15        /* 5       1 */
#define Hit_Writeback_Inv_SD             0x17        /* 5       3 */
#define Fill_I                           0x14        /* 5       0 */
#define Hit_Writeback_D                  0x19        /* 6       1 */
#define Hit_Writeback_SD                 0x1B        /* 6       3 */
#define Hit_Writeback_I                  0x18        /* 6       0 */
#define Hit_Set_Virtual_SI               0x1E        /* 7       2 */
#define Hit_Set_Virtual_SD               0x1F        /* 7       3 */


/*
 * Coprocessor 0 registers
 */
#define	C0_INX		$0		/* tlb index */
#define	C0_RAND		$1		/* tlb random */
#define	C0_TLBLO	$2		/* tlb entry low */

#define	C0_CTXT		$4		/* tlb context */

#define C0_PIDMASK	$6		/* Mips2 */
#define C0_ERROR	$7		/* Mips2 */
#define	C0_BADVADDR	$8		/* bad virtual address */

#define	C0_TLBHI	$10		/* tlb entry hi */
#define C0_PID		$10		/* Mips2 */

#define	C0_SR		$12		/* status register */
#define	C0_CAUSE	$13		/* exception cause */
#define	C0_EPC		$14		/* exception pc */
#define	C0_PRID		$15		/* revision identifier */

/*** r4000 only registers ***/
#define	C0_TLBLO0	$2		/* tlb entry low for even VPN */
#define	C0_TLBLO1	$3		/* tlb entry low for even VPN */
#define	C0_PAGEMASK	$5		/* tlb page mask */
#define	C0_WIRED	$6		/* No. of wired tlb entries */
#define	C0_COUNT	$9		/* Timer Count */
#define	C0_COMPARE	$11		/* Timer Compare */
#define	C0_CONFIG	$16		/* Configuration register */
#define	C0_LLADDR	$17		/* Load Linked address register */
#define	C0_WATCHLO	$18		/* Lower half of Watch register */
#define	C0_WATCHHI	$19		/* Upper half of Watch register */
#define	C0_XCONTEXT	$20		/* Context register for MIPS3 address */
#define	C0_ECC		$26		/* Scache ECC and Pcache parity */
#define	C0_CACHEERR	$27		/* Cache error and status register */
#define	C0_TAGLO	$28		/* Lower half of cache tag register */
#define	C0_TAGHI	$29		/* Upper half of cache tag register */
#define	C0_ERROREPC	$30		/* Error exception program counter */


/*
 * Coprocessor 0 operations
 */
#define	C0_READI  0x1		/* read ITLB entry addressed by C0_INDEX */
#define	C0_WRITEI 0x2		/* write ITLB entry addressed by C0_INDEX */
#define	C0_WRITER 0x6		/* write ITLB entry addressed by C0_RAND */
#define	C0_PROBE  0x8		/* probe for ITLB entry addressed by TLBHI */
#define	C0_RFE	  0x10		/* restore for exception */

/*
 * Flags for the nofault handler. 0 means no fault is expected.
 */
#define	NF_BADADDR	1	/* badaddr, wbadaddr */
#define	NF_COPYIO	2	/* copyin, copyout */
#define	NF_ADDUPC	3	/* addupc */
#define	NF_FSUMEM	4	/* fubyte, subyte, fuword, suword */
#define	NF_USERACC	5	/* useracc */
#define	NF_SOFTFP	6	/* softfp */
#define	NF_REVID	7	/* revision ids */
#define	NF_NENTRIES	8

/*
 * Chip interrupt vector
 */
#define	NC0VECS		8
#ifndef LOCORE
#ifdef KERNEL
extern int (*c0vec_tbl[])();
#endif
#endif /* !LOCORE */

#endif /* __IDE_CPU_H__ */

