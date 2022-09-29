#ifndef __ARCS_FAULT_H__
#define __ARCS_FAULT_H__

/*
 * arcs/include/fault.h - definitions for arcs standalone exception frame
 *
 * $Revision: 1.20 $
 */

#include <sgidefs.h>		/* For _MIPS_ABI, _MIPS_SZPTR, etc */

/*
 * For the time being keep everything at 4 bytes, but #defines
 * will make it easier to change.
 */
#undef R_SIZE
#undef P_SIZE

#define P_SIZE	(_MIPS_SZPTR/8)			/* 8 bits/byte */
#define R_SIZE	P_SIZE


#ifdef _LANGUAGE_ASSEMBLY
#define	K_MACHREG_SIZE	8
#endif	/* _LANGUAGE_ASSEMBLY */

/*
 * Exception types
 */
#define	EXCEPT_NORM	1
#define	EXCEPT_UTLB	2
#define	EXCEPT_BRKPT	3
#define EXCEPT_XUT	4
#define EXCEPT_ECC	5
#define EXCEPT_NMI	6

/*
 * Stack modes
 */
#define	MODE_INVALID	-1	/* pda was set up but stack isn't valid */
#define	MODE_NORMAL	0	/* executing on normal stack */
#define	MODE_FAULT	1	/* executing on fault stack */


/*
 * Exception frame
 */
#define	R_R0		0
#define	R_R1		1
#define	R_R2		2
#define	R_R3		3
#define	R_R4		4
#define	R_R5		5
#define	R_R6		6
#define	R_R7		7
#define	R_R8		8
#define	R_R9		9
#define	R_R10		10
#define	R_R11		11
#define	R_R12		12
#define	R_R13		13
#define	R_R14		14
#define	R_R15		15
#define	R_R16		16
#define	R_R17		17
#define	R_R18		18
#define	R_R19		19
#define	R_R20		20
#define	R_R21		21
#define	R_R22		22
#define	R_R23		23
#define	R_R24		24
#define	R_R25		25
#define	R_R26		26
#define	R_R27		27
#define	R_R28		28
#define	R_R29		29
#define	R_R30		30
#define	R_R31		31
/*
 * compiler names
 */
#define	R_ZERO		R_R0
#define	R_AT		R_R1
#define	R_V0		R_R2
#define	R_V1		R_R3
#define	R_A0		R_R4
#define	R_A1		R_R5
#define	R_A2		R_R6
#define	R_A3		R_R7
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
#define	R_T0		R_R8
#define	R_T1		R_R9
#define	R_T2		R_R10
#define	R_T3		R_R11
#define	R_T4		R_R12
#define	R_T5		R_R13
#define	R_T6		R_R14
#define	R_T7		R_R15
#elif (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
#define	R_A4		R_R8
#define	R_A5		R_R9
#define	R_A6		R_R10
#define	R_A7		R_R11
#define	R_T0		R_R12
#define	R_T1		R_R13
#define	R_T2		R_R14
#define	R_T3		R_R15
#endif
#define R_TA0		R_R12			/* portable 32/64 names */
#define R_TA1		R_R13
#define R_TA2		R_R14
#define R_TA3		R_R15
#define	R_S0		R_R16
#define	R_S1		R_R17
#define	R_S2		R_R18
#define	R_S3		R_R19
#define	R_S4		R_R20
#define	R_S5		R_R21
#define	R_S6		R_R22
#define	R_S7		R_R23
#define	R_T8		R_R24
#define	R_T9		R_R25
#define	R_K0		R_R26
#define	R_K1		R_R27
#define	R_GP		R_R28
#define	R_SP		R_R29
#define	R_FP		R_R30
#define R_S8		R_R30
#define	R_RA		R_R31

/*
 * floating point -- only saved if CU1 bit in status register is set
 */
#define	R_F0		32
#define	R_F1		33
#define	R_F2		34
#define	R_F3		35
#define	R_F4		36
#define	R_F5		37
#define	R_F6		38
#define	R_F7		39
#define	R_F8		40
#define	R_F9		41
#define	R_F10		42
#define	R_F11		43
#define	R_F12		44
#define	R_F13		45
#define	R_F14		46
#define	R_F15		47
#define	R_F16		48
#define	R_F17		49
#define	R_F18		50
#define	R_F19		51
#define	R_F20		52
#define	R_F21		53
#define	R_F22		54
#define	R_F23		55
#define	R_F24		56
#define	R_F25		57
#define	R_F26		58
#define	R_F27		59
#define	R_F28		60
#define	R_F29		61
#define	R_F30		62
#define	R_F31		63
#define	R_EPC		64
#define	R_MDHI		65
#define	R_MDLO		66
#define	R_SR		67
#define	R_CAUSE		68
#define	R_TLBHI		69
#define	R_TLBLO		70
#define	R_BADVADDR	71
#define	R_INX		72
#define	R_RAND		73
#define	R_CTXT		74
#define	R_EXCTYPE	75
#define	R_C1_EIR	76
#define	R_C1_SR		77

#if R4000 || R10000
/* R4k only cp0 registers */
#define	R_TLBLO0	70	/* same as R_TLBLO */
#define	R_TLBLO1	78
#define	R_PGMSK		79
#define	R_WIRED		80
#define	R_COUNT		81
#define	R_COMPARE	82
#define	R_LLADDR	83
#define	R_WATCHLO	84
#define	R_WATCHHI	85
#define	R_ECC		86
#define	R_CACHERR	87
#define	R_TAGLO		88
#define	R_TAGHI		89
#define	R_ERREPC	90
#define	R_CONFIG	91
#define	R_EXTCTXT	92
#endif	/* R4000 || R10000 */

#if TFP
/* TFP only cp0 registers */
#define R_TLBSET	78
#define R_UBASE		79
#define R_SHIFTAMT	80
#define R_TRAPBASE	81
#define R_BADPADDR	82
#define R_COUNT		83
#define R_PRID		84
#define R_CONFIG	85
#define R_WORK0		86
#define R_WORK1		87
#define R_PBASE		88
#define R_GBASE		89
#define R_WIRED		90
#define R_DCACHE	91
#define R_ICACHE	92
#define R_FCONFIG	93
#define R_FSR		94
#endif /* TFP */

#if defined(R4000) || defined(R10000)
#define	NREGS		93
#elif defined(TFP)
#define	NREGS		95
#endif

/* Macro to get Register index for use in regs array */
#define	REG(x)		(x)
#define	ROFF(x)		((x) * R_SIZE)

/*
 * libsk's csu.s allocates SKSTKSZ bytes for (fault+normal) stacks combined.
 * Of that total, EXSTKSZ bytes comprise the fault stack, required by 
 * exception-handling code
 *
 * NOTE: libsk/ml/faultasm.s loads (and restores) the processor-specific 
 * interrupt sp from private->fault_sp, so the pdas must be initialized
 * very early in the startup code of all standalones using the arcs
 * libsk fault-handling mechanism.
 */
#define	SKSTKSZ		0x10000		/* 64K total--both stacks */
#define	EXSTKSZ		0x1000		/* 4K of SKSTKSIZE is fault stack */

/* 
 * genpda.h defines a structure containing the set of global variables
 * used throughout the arcs code which must be unique for each 
 * processor.  An array of these structs--indexed by virtual 
 * processor-id--provides the duplicated storage necessary.  To 
 * avoid changing all files to explicitly reference the pda version, 
 * fault.h and genpda.h cooperate to change all instances of these 
 * variable names to full paths in the pda table and indexed by cpuid()
 * for MP machines, 0 for SP.
 * Files that require transparent access to the moved variables include
 * fault.h, which then includes genpda.h but with _USE_FULL_PATH defined.
 * This selects the #defines that substitute the pda_table[cpuid].varname
 * string for each occurrence of 'varname' throughout that file.
 * The handful of files requiring explicit access to the pdas include
 * genpda.h, which then includes fault.h with _USE_PPTR defined, which
 * defines the GPRIVATE and GPME macros for that file.  The idempotency
 * protection in the headers prevents multiple inclusion with inconsistent
 * defines.
 * Note, however, that any 'extern' declarations using the supposed type of
 * these pda variables results in a bewildering syntax error, but at least
 * this will prevent any *runtime* surprises to anyone who doesn't know 
 * about this stuff.
 */

 /*
  * - Files needing EXPLICIT access to the fields of a pda must include
  *   <genpda.h>, which defines the GPME() and GPRIVATE() macros.  It
  *   automatically includes everything in fault.h that doesn't conflict.
  * - Files needing IMPLICIT access to their pda (hidden references the
  *   pda fields instead of shared global copies with no source-changes
  *   required) must include <fault.h>, which defines the the full pda
  *   paths to the correct pda slot, and grabs relevant stuff from fault.h
  */ 

#if (_USE_PPTR != 0xcad)
#define _USE_FULL_PATH	0xfeed

#include <genpda.h>

#if defined(_LANGUAGE_C)
/*
 * these defines convert existing references to the global variables
 * into gpda references.  Note that nofault's lack of leading
 * underscore prevents access via GPRIVATE; they must all be converted 
 * into set_nofault() and clear_nofault() function calls anyway.
 */

/* For UP machines, assume cpuid 0 in machine dependent code where
 * -D$(CPUBOARD) is defined.
 */
#if IP20 || IP22 || IP26 || IP28 || IP32
#define _cpuid()	0
#else
#define	_cpuid()	cpuid()
#endif

#define	_epc_save	GEN_PDA_TAB(_cpuid()).gdata.epc_save
#define	_at_save	GEN_PDA_TAB(_cpuid()).gdata.at_save
#define	_v0_save	GEN_PDA_TAB(_cpuid()).gdata.v0_save
#define	_badvaddr_save	GEN_PDA_TAB(_cpuid()).gdata.badvaddr_save
#define	_cause_save	GEN_PDA_TAB(_cpuid()).gdata.cause_save
#define	_sp_save	GEN_PDA_TAB(_cpuid()).gdata.sp_save
#define	_sr_save	GEN_PDA_TAB(_cpuid()).gdata.sr_save
#if R4000 || R10000
#define	_error_epc_save	GEN_PDA_TAB(_cpuid()).gdata.error_epc_save
#define	_cache_err_save	GEN_PDA_TAB(_cpuid()).gdata.cache_err_save
#endif
#define	_exc_save	GEN_PDA_TAB(_cpuid()).gdata.exc_save
#define	_stack_mode	GEN_PDA_TAB(_cpuid()).gdata.stack_mode
#define	_mode_save	GEN_PDA_TAB(_cpuid()).gdata.mode_save
#define	_my_virtid	GEN_PDA_TAB(_cpuid()).gdata.my_virtid
#define	_my_physid	GEN_PDA_TAB(_cpuid()).gdata.my_physid
#define	_notfirst	(GEN_PDA_TAB(_cpuid()).gdata.notfirst)
#define	_first_epc	GEN_PDA_TAB(_cpuid()).gdata.first_epc
/*
 * note that the fields below here are pointers, which may not 
 * continue to match the length of unsigned longs. 
 */
#define	_regs		GEN_PDA_TAB(_cpuid()).gdata.regs
#define	nofault		GEN_PDA_TAB(_cpuid()).gdata.pda_nofault
#define	_specific_pda	GEN_PDA_TAB(_cpuid()).gdata.specific_pda

#endif /* !_LANGUAGE_ASSEMBLY */

#undef _USE_FULL_PATH
#endif /* _USE_PPTR != 0xcad */

/*
 * the startup code in libsk/ml/csu.s builds vid 0's normal and exception
 * stacks, and saves those SP addrs in (global variables) initial_stack 
 * and _fault_sp, resp.  Those values are copied into vid 0's pda_initial_sp
 * and pda_fault_sp PDA fields by fault.c:setup_genpdas(). 
 */
#ifdef	_LANGUAGE_C
extern unsigned long	*initial_stack;	/* temp. storage for bootmaster's sp */
extern unsigned long	*_fault_sp;	/* stack used during exceptions */
#endif

/* This stuff is used with the InstallHandler/RemoveHandler routines*/
/*
 * Fault table indexing.
 */
#define	FLT_TBLSHIFT		8
#define	FLT_TBLMSK		0xff

#define	FLT_TBLPTR(x)		((x) & (FLT_TBLMSK << FLT_TBLSHIFT))
#define	FLT_TBLINDEX(x)		((x) & FLT_TBLMSK)

#define	FLT_TBL			(1 << FLT_TBLSHIFT)
#define	INT_TBL			(2 << FLT_TBLSHIFT)
#define	IMSK_TBL		(3 << FLT_TBLSHIFT)
#define	ALLFLT_TBL		(4 << FLT_TBLSHIFT)
#define CACHEERR_TBL		(5 << FLT_TBLSHIFT)

/*
 * Exception code table.  If index is 0, FaultHandler() looks at IntTbl[].
 */
#define	FLT_BAD			(FLT_TBL | 0)	/* bad entry for handlers */
#define	FLT_TLBMOD		(FLT_TBL | 1)	/* TLB modification */
#define	FLT_TLBLDMISS		(FLT_TBL | 2)	/* TLB miss on load */
#define	FLT_TLBSTMISS		(FLT_TBL | 3)	/* TLB miss on store */
#define	FLT_LDADDRER		(FLT_TBL | 4)	/* address error on load */
#define	FLT_STADDRER		(FLT_TBL | 5)	/* address error on store */
#define	FLT_IBUSER		(FLT_TBL | 6)	/* bus error on I-fetch */
#define	FLT_DBUSER		(FLT_TBL | 7)	/* data bus error */
#define	FLT_SYSCALL		(FLT_TBL | 8)	/* system call */
#define	FLT_BRKPT		(FLT_TBL | 9)	/* breakpoint */
#define	FLT_RSVINST		(FLT_TBL | 10)	/* reserved instruction */
#define	FLT_COPROC		(FLT_TBL | 11)	/* coprocessor unusable */
#define	FLT_OVRFL		(FLT_TBL | 12)	/* arithmetic overflow */
#define FLT_TRAP                (FLT_TBL | 13)  /* trap exception */
#define FLT_VCEI                (FLT_TBL | 14)  /* Virtual Coherency Exc Ins */
#define FLT_FPE                 (FLT_TBL | 15)  /* floating point exception */

#define FLT_RSV1                (FLT_TBL | 16)  /* reserved */
#define FLT_RSV2                (FLT_TBL | 17)  /* reserved */
#define FLT_RSV3                (FLT_TBL | 18)  /* reserved */
#define FLT_RSV4                (FLT_TBL | 19)  /* reserved */
#define FLT_RSV5                (FLT_TBL | 20)  /* reserved */
#define FLT_RSV6                (FLT_TBL | 21)  /* reserved */
#define FLT_RSV7                (FLT_TBL | 22)  /* reserved */

#define FLT_WATCH               (FLT_TBL | 23)  /* Watch exception */

#define FLT_RSV8                (FLT_TBL | 24)  /* reserved */
#define FLT_RSV9                (FLT_TBL | 25)  /* reserved */
#define FLT_RSV10               (FLT_TBL | 26)  /* reserved */
#define FLT_RSV11               (FLT_TBL | 27)  /* reserved */
#define FLT_RSV12               (FLT_TBL | 28)  /* reserved */
#define FLT_RSV13               (FLT_TBL | 29)  /* reserved */
#define FLT_RSV14               (FLT_TBL | 30)  /* reserved */

#define FLT_VCED                (FLT_TBL | 31)  /* Virtual Coherency Exc Data */

#define N_FLT                   32              /* number of entries */


/* Index to cache error table */
#define SYSAD_PARERR	0
#define SCACHE_PARERR	1
#define PCACHE_PARERR	2

/*
 * R4000 Cache Error Exception table. R4000 provided a special vector for
 * cache errors.
 */
#define CACHEERR_SYSAD	(CACHEERR_TBL | 0) 	/* SysAD bus parity err */
#define CACHEERR_PCACHE	(CACHEERR_TBL | 1)	/* p-cache error */
#define CACHEERR_SCACHE (CACHEERR_TBL | 2)	/* s-cache error */

#define N_CACHEERR	3			/* number of entries */

/*
 * New Interrupt table. Uses level names instead of actual interrupting
 * devices. 
 */
#define	INT_SWTRP0		(INT_TBL | 0)	/* software trap 0 */
#define	INT_SWTRP1		(INT_TBL | 1)	/* software trap 1 */
#define	INT_Level0		(INT_TBL | 2)	/* IRQ 0 Interrupt */
#define	INT_Level1		(INT_TBL | 3)	/* IRQ 1 Interrupt */
#define	INT_Level2		(INT_TBL | 4)	/* IRQ 2 Interrupt */
#define	INT_Level3		(INT_TBL | 5)	/* IRQ 3 Interrupt */
#define	INT_Level4		(INT_TBL | 6)	/* IRQ 4 Interrupt */
#define	INT_Level5		(INT_TBL | 7)	/* IRQ 5 Interrupt */
#define	N_INT			8		/* number of entries */
#endif /* __ARCS_FAULT_H__ */
