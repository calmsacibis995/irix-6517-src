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

#ident "$Revision: 1.1 $"

/*
 * Register offsets.
 */
#define	R0		1
#define	R1		2
#define	R2		3
#define	R3		4
#define	R4		5
#define	R5		6
#define	R6		7
#define	R7		8
#define	R8		9
#define	R9		10
#define	R10		11
#define	R11		12
#define	R12		13
#define	R13		14
#define	R14		15
#define	R15		16
#define	R16		17
#define	R17		18
#define	R18		19
#define	R19		20
#define	R20		21
#define	R21		22
#define	R22		23
#define	R23		24
#define	R24		25
#define	R25		26
#define	R26		27
#define	R27		28
#define	R28		29
#define	GP		R28
#define	R29		30
#define	SP		R29
#define	R30		31
#define	FP		R30
#define	R31		32
#define	HI		33
#define	LO		34

/* R2000/R3000 */
#define	TLBHI		35
#define	TLBLO		36
#define	CONTEXT		37
#define	INDEX		38

/* R6000 */
#define PID		35
#define C0ERROR		36
#define SBCERR		37
#define SBCINT		38

#define	SR		39
#define	CAUSE		40
#define	EPC		41
#define	BADVADDR	42

/* R6000 */
#define SBCIMASK	43

#ifdef R4000
/* Additional registers for R4000 */

#define TLBLO0                  36
#define TLBLO1                  43
#define PAGEMASK                44
#define WIRED                   45
#define COUNT                   46
#define COMPARE                 47
#define CONFIG                  48
#define LLADDR                  49
#define WATCHLO                 50
#define WATCHHI                 51
#define ECC                     52
#define CACHEERR                53
#define TAGLO                   54
#define TAGHI                   55
#define ERROREPC                56
#define XCONTEXT                57

#define N_REGS                  57      /* number of registers */
#else R4000
#define	N_REGS		43		/* number of registers */
#endif R4000

#define	R_OFF(x)	((x) * 4)

#define	R_AT		R_OFF(R1)
#define	R_V0		R_OFF(R2)
#define	R_V1		R_OFF(R3)
#define	R_A0		R_OFF(R4)
#define	R_A1		R_OFF(R5)
#define	R_A2		R_OFF(R6)
#define	R_A3		R_OFF(R7)
#define	R_T0		R_OFF(R8)
#define	R_T1		R_OFF(R9)
#define	R_T2		R_OFF(R10)
#define	R_T3		R_OFF(R11)
#define	R_T4		R_OFF(R12)
#define	R_T5		R_OFF(R13)
#define	R_T6		R_OFF(R14)
#define	R_T7		R_OFF(R15)
#define	R_S0		R_OFF(R16)
#define	R_S1		R_OFF(R17)
#define	R_S2		R_OFF(R18)
#define	R_S3		R_OFF(R19)
#define	R_S4		R_OFF(R20)
#define	R_S5		R_OFF(R21)
#define	R_S6		R_OFF(R22)
#define	R_S7		R_OFF(R23)
#define	R_T8		R_OFF(R24)
#define	R_T9		R_OFF(R25)
#define R_K0		R_OFF(R26)
#define R_K1		R_OFF(R27)
#define R_GP		R_OFF(R28)
#define R_SP		R_OFF(R29)
#define	R_FP		R_OFF(R30)
#define	R_RA		R_OFF(R31)

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

#ifndef R4000
#define	FLT_RSV1		(FLT_TBL | 13)	/* reserved */
#define	FLT_RSV2		(FLT_TBL | 14)	/* reserved */
#define	FLT_RSV3		(FLT_TBL | 15)	/* reserved */

/* R6000 only */
#define	FLT_TRAP		(FLT_TBL | 13)	/* trap */
#define	FLT_DBLNC		(FLT_TBL | 14)	/* dbl wd non-cacheable rd/wr */
#define	FLT_MCHECK		(FLT_TBL | 15)	/* machine check */

#define	N_FLT			16		/* number of entries */

#else R4000
/* R4000 exceptions */
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


/*
 * R4000 Cache Error Exception table. R4000 provided a special vector for
 * cache errors.
 */
#define CACHEERR_SYSAD	(CACHEERR_TBL | 0) 	/* SysAD bus parity err */
#define CACHEERR_PCACHE	(CACHEERR_TBL | 1)	/* p-cache error */
#define CACHEERR_SCACHE (CACHEERR_TBL | 2)	/* s-cache error */

#define N_CACHEERR	3			/* number of entries */
#endif R4000

/*
 * Interrupt table.  If cause is 0 (ext intr), FaultHandler() looks at IsrTbl[].
 * This table should eventually go away...
 */
#define	INT_SWTRP0		(INT_TBL | 0)	/* software trap 0 */
#define	INT_SWTRP1		(INT_TBL | 1)	/* software trap 1 */
#define	INT_BAD			(INT_TBL | 2)	/* bad entry for handlers */
#define	INT_DUART		(INT_TBL | 3)	/* DUARTs */
#define INT_SCC			(INT_TBL | 3)	/* RB31xx: SCC */
#define	INT_TIMER0		(INT_TBL | 4)	/* timer 0 */
#define	INT_COPROC		(INT_TBL | 5)	/* coprocessor */
#define	INT_TIMER1		(INT_TBL | 6)	/* timer 1 */
#define	INT_DBUSER		(INT_TBL | 7)	/* data bus error */

/* RC62x0 only */
#define INT_SBC			(INT_TBL | 2)	/* sbc external interrupt */
#define INT_EXT			INT_SBC
#define	INT_FP			(INT_TBL | 3)	/* fpu coprocessor exception */

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

/*
 * IMR Maskable interrupt table.  First 17 defines are for the M120 and 
 * next 16 defines are for the M1000, M2000, RB31xx (Genesis) etc.
 */

/*
 * M120/M180 machines
 */
#define	IMSK_IRQ3		(IMSK_TBL | 0)	/* 0  IRQ 3 */
#define	IMSK_IRQ4		(IMSK_TBL | 1)	/* 1  IRQ 4 */
#define	IMSK_IRQ5		(IMSK_TBL | 2)	/* 2  IRQ 5 */
#define	IMSK_IRQ6		(IMSK_TBL | 3)	/* 3  IRQ 6 */
#define	IMSK_IRQ7		(IMSK_TBL | 4)	/* 4  IRQ 7 */
#define	IMSK_IRQ13		(IMSK_TBL | 5)	/* 5  IRQ 13 */
#define	IMSK_IRQ14		(IMSK_TBL | 6)	/* 6  IRQ 14 */
#define	IMSK_IRQ15		(IMSK_TBL | 7)	/* 7  IRQ 15 */
#define	IMSK_IRQ11		(IMSK_TBL | 8)	/* 8  IRQ 11 */
#define	IMSK_IRQ10		(IMSK_TBL | 9)	/* 9  IRQ 10 */
#define	IMSK_IRQ9		(IMSK_TBL | 10)	/* 10 IRQ 9 */
#define	IMSK_RSV		(IMSK_TBL | 11)	/* 11 not used */
#define	IMSK_9516		(IMSK_TBL | 12)	/* 12 9516 */
#define	IMSK_SCSI		(IMSK_TBL | 13)	/* 13 SCSI */
#define	IMSK_ENET		(IMSK_TBL | 14)	/* 14 Ethernet */
#define	IMSK_FBUF		(IMSK_TBL | 15)	/* 15 frame buffer */
#define	N_IMSK			16		/* number of entries */

/*
 * M1000, M2000 and RB31xx (Genesis) machines
 */
#define	IMSK_VMEIPL1		(IMSK_TBL | 1)	/* 1  VME IPL 1 */
#define	IMSK_VMEIPL2		(IMSK_TBL | 2)	/* 2  VME IPL 2 */
#define	IMSK_VMEIPL3		(IMSK_TBL | 3)	/* 3  VME IPL 3 */
#define	IMSK_VMEIPL4		(IMSK_TBL | 4)	/* 4  VME IPL 4 */
#define	IMSK_VMEIPL5		(IMSK_TBL | 5)	/* 5  VME IPL 5 */
#define	IMSK_VMEIPL6		(IMSK_TBL | 6)	/* 6  VME IPL 6 */
#define	IMSK_VMEIPL7		(IMSK_TBL | 7)	/* 7  VME IPL 7 */
#define IMSK_DMAERROR		(IMSK_TBL | 8)	/* 8 dma error */
#define IMSK_ZEROHALF		(IMSK_TBL | 9)	/* 9 dma error */
#define IMSK_BUFERROR		(IMSK_TBL | 10)	/* 8 dma error */
#define IMSK_LNCINTR		(IMSK_TBL | 11)	/* 8 dma error */
#define IMSK_SCSIINT		(IMSK_TBL | 12)	/* 8 dma error */
#if (M2000 || RB3125)
#undef N_IMSK
#define N_IMSK			12		/* number of imsk entries */
#endif (M2000 || RB3125)

#define	IMSK_VMEVEC(x)		(IMSK_TBL | (x))/* VME vector */
#define	N_VME_IMSK_R3200	0x100		/* number of vme intr vectors */
#define	N_VME_IMSK		256		/* number of vem intr vectors */

#ifdef	LANGUAGE_C
extern int UnexpectHandler();
extern u_int DisableInt();
extern u_int EnableInt();
extern void InterruptInit();
#endif	LANGUAGE_C
