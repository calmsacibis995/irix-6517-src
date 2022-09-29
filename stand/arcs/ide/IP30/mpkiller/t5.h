/* |-----------------------------------------------------------|
 * | Copyright (c) 1992 MIPS Technology Inc.                   |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 *
 * $Id: t5.h,v 1.2 1996/11/15 01:49:32 kkn Exp $
 *
 * This header file is the complete set of general definitions for
 * assembly language programs for the t5 processor.
 *
 * SECTIONS and THEIR CONTENTS 
 * 1) General Registers (encompasses regdef.h).
 * 2) Instruction mnemonic aliases.
 * 3) Coproc 0 Registers with exception definitions listed by C0_Cause bits.
 * 4) Coproc 1 Registers
 * 5) Cache, TLB, & Address Space Constants
 * 6) cache instruction codes
 * 7) Code structure conventions (boot, handler, termination, new directives)
 * 8) System Model Constants
 * 9) Common Macros used in tests
 *
 * NOTE:
 * 1) halt() is specific to the SpeedRacer standalone mpkiller tests.
 */

/***********************************************************************
 ** SECTION 1                                                         **
 ** General Registers (encompasses regdef.h)                          **
 ***********************************************************************/
#define zero	$0	/* wired zero */
#define AT	$at	/* assembler temp */
#define v0	$2	/* return value */
#define v1	$3
#define a0	$4	/* argument registers */
#define a1	$5
#define a2	$6
#define a3	$7
#define t0	$8	/* caller saved */
#define t1	$9
#define t2	$10
#define t3	$11
#define t4	$12
#define t5	$13
#define t6	$14
#define t7	$15
#define s0	$16	/* callee saved */
#define s1	$17
#define s2	$18
#define s3	$19
#define s4	$20
#define s5	$21
#define s6	$22
#define s7	$23
#define t8	$24	/* code generator */
#define t9	$25
#define k0	$26	/* kernel temporary */
#define k1	$27
#define gp	$28	/* global pointer */
#define sp	$29	/* stack pointer */
#define s8	$30	/* one more callee saved */
#define ra	$31	/* return address */
/* #define fp	$30	* obsolete name for frame pointer */

#define r0	$0
#define r1	$1
#define r2	$2
#define r3	$3
#define r4	$4
#define r5	$5
#define r6	$6
#define r7	$7
#define r8	$8
#define r9	$9
#define r10	$10
#define r11	$11
#define r12	$12
#define r13	$13
#define r14	$14
#define r15	$15
#define r16	$16
#define r17	$17
#define r18	$18
#define r19	$19
#define r20	$20
#define r21	$21
#define r22	$22
#define r23	$23
#define r24	$24
#define r25	$25
#define r26	$26
#define r27	$27
#define r28	$28
#define r29	$29
#define r30	$30
#define r31	$31


/***********************************************************************
 ** SECTION 2     	                                              **
 ** Instruction mnemonic aliases                                      **
 ***********************************************************************/
#define jr	j
#define jalr	jal

#define sllv	sll
#define srlv	srl
#define srav	sra

#define dsllv	dsll
#define dsrlv	dsrl
#define dsrav	dsra

/* Removed by peltier because they cause too much confusion
#define dsll32	dsll
#define dsrl32	dsrl
#define dsra32	dsra
*/

#define tgeiu	tgeu
#define tgei	tge
#define tltiu	tltu
#define tlti	tlt
#define teqi	teq
#define tnei	tne

#if _ABIO32
/* if 3.19 */
#define pfetch	prefx
#endif

/***********************************************************************
 ** SECTION 3     	                                              **
 ** Coprocessor 0 Registers                                           **
 ***********************************************************************/
#define	C0_Index	$0
#define	C0_Random	$1
#define C0_EntryLo0	$2
#define C0_EntryLo1	$3
#define	C0_Context	$4
#define C0_PageMask	$5
#define C0_Wired	$6
#define	C0_BadVAddr	$8
#define C0_Count	$9
#define	C0_EntryHi	$10
#define	C0_Compare	$11
#define	C0_SR		$12
#define	C0_Cause	$13
#define	C0_EPC		$14
#define C0_PRId		$15
#define C0_Config	$16
#define C0_LLAddr	$17
#define C0_WatchLo	$18
#define C0_WatchHi	$19
#define C0_XContext	$20
#define C0_FrameMask	$21
#define C0_Diag		$22
#define C0_Perf		$25
#define C0_ECC          $26
#define C0_CacheErr	$27
#define C0_TagLo	$28
#define C0_TagHi	$29
#define C0_ErrorEPC	$30

/*------------------------------------------------------------------+
| C0_Index bits                                                     |
+------------------------------------------------------------------*/
#define Index_P			0x80000000
#define Index_IndexMsk		0x3f

/*------------------------------------------------------------------+
| C0_Random bits                                                    |
+------------------------------------------------------------------*/
#define Random_RdmMsk		0x3f

/*------------------------------------------------------------------+
| C0_EntryLo0 & C0_EntryLo1 bits                                    |
+------------------------------------------------------------------*/
#define EntryLo_UCMskHi		0xc0000000
#define EntryLo_PFN64MskHi	0x3
#define EntryLo_PFNMsk		0xffffffc0
#define EntryLo_PFNShf		6
#define EntryLo_CAMsk		0x38            /* cache algorithm */
#define EntryLo_CAShf  		3
#define EntryLo_UncachAcc 	(CA_UncachAcc << EntryLo_CAShf)
#define EntryLo_Uncached 	(CA_Uncached << EntryLo_CAShf)
#define EntryLo_CachNCoh 	(CA_CachNCoh << EntryLo_CAShf)
#define EntryLo_CachCohE 	(CA_CachCohE << EntryLo_CAShf)
#define EntryLo_CachCohS 	(CA_CachCohS << EntryLo_CAShf)
#define EntryLo_Dirty		1
#define EntryLo_NonDirty	0
#define EntryLo_DMsk		0x4             /* writeable */
#define EntryLo_DShf  		2
#define EntryLo_Valid		1 
#define EntryLo_Invalid		0
#define EntryLo_VMsk		0x2             /* valid bit */
#define EntryLo_VShf  		1
#define EntryLo_GMsk		0x1             /* global bit */
#define EntryLo_Global		1
#define EntryLo_NonGlobal	0

#define EntryLo_Concat32(pfn, ca, d, v, g)				\
  		((pfn << EntryLo_PFNShf) | (ca << EntryLo_CAShf) |	\
		 (d << EntryLo_DShf)     | (v << EntryLo_VShf)   | g)


/*------------------------------------------------------------------+
| C0_Context bits                                                   |
+------------------------------------------------------------------*/
#define Context_PTEBaseMsk	0xff800000
#define Context_PTEBaseShf	23
#define Context_BadVPN2Msk	0x007ffff0
#define Context_BadVPN2Shf	4

/*------------------------------------------------------------------+
| C0_PageMask bits                                                  |
+------------------------------------------------------------------*/
#define PageMask_Msk		0x01ffe000
#define PageMask_Shf		13

#define PageMask_4KMsk		0x00000000
#define PageMask_16KMsk		0x00006000
#define PageMask_64KMsk		0x0001e000
#define PageMask_256KMsk	0x0007e000
#define PageMask_1MMsk		0x001fe000
#define PageMask_4MMsk		0x007fe000
#define PageMask_16MMsk		0x01ffe000

#define TLB_Comp_Always		0x00001fff /* VA Bits always compared w TLB */

/*------------------------------------------------------------------+
| C0_Wired bits                                                     |
+------------------------------------------------------------------*/
#define Wired_Msk		0x3f

/*------------------------------------------------------------------+
| C0_BadVAddr bits                                                  |
+------------------------------------------------------------------*/

/*------------------------------------------------------------------+
| C0_Count bits                                                     |
+------------------------------------------------------------------*/

/*------------------------------------------------------------------+
| C0_EntryHi bits                                                   |
+------------------------------------------------------------------*/
#define EntryHi_RMskHi		0xc0000000
#define EntryHi_RShf		62
#define EntryHi_VPN2MskHi	0xfff
#define EntryHi_VPN2MskLo	0xffffe000
#define EntryHi_VPN2Shf		13
#define EntryHi_ASIDMsk		0xff


#define EntryHi_UserHi		0x00000000
#define EntryHi_SupervHi	0x40000000
#define EntryHi_KernelHi	0xc0000000

#define EntryHi_Concat32(vpn2, asid)	\
  		( ((vpn2 << EntryHi_VPN2Shf) & EntryHi_VPN2MskLo) | \
		  (asid & EntryHi_ASIDMsk) );
#define EntryHi_Concat64(R, vpn2, asid)					\
  		( (R << EntryHi_RShf) |					\
	  	  ((vpn2<<EntryHi_VPN2Shf) & 				\
		    ((EntryHi_VPN2MskHi<<32)|EntryHi_VPN2MskLo)) |	\
	  	  (asid & EntryHi_ASIDMsk) );
/*------------------------------------------------------------------+
| C0_Compare bits                                                   |
+------------------------------------------------------------------*/

/*------------------------------------------------------------------+
| C0_SR bits (Status)                                               |
+------------------------------------------------------------------*/
#define	SR_CUMsk	0xf0000000	/* coproc usable bits	*/
/* #define SR_CU3	0x80000000	 * Coprocessor 3 usable */
#define SR_XX		0x80000000	/* MipsIV mode enable */
#define SR_CU2		0x40000000	/* Coprocessor 2 usable */
#define SR_CU1		0x20000000	/* Coprocessor 1 usable */
#define SR_CU0		0x10000000	/* Coprocessor 0 usable */

#define SR_RP		0x08000000	/* RP bit */
#define SR_FR		0x04000000	/* FR bit */
#define SR_RE		0x02000000	/* reverse endian in user mode */

/* Cache control bits (Diags Status bits) */
#define SR_DSMsk	0x01ff0000	/* mask for DS bits */
#define	SR_BEV		0x00400000	/* Exceptions in boot region */
#define	SR_TS		0x00200000	/* TLB shutdown */
#define	SR_SR		0x00100000	/* soft reset */
#define SR_NMI          0x00080000      /* NMI */
#define	SR_CH		0x00040000	/* last cache load tag hit or miss */
#define	SR_CE		0x00020000	/* ECC register used to modify bits */
#define	SR_DE		0x00010000	/* parity or ECC to cause exceptions?*/

/* Interrupt enable bits for 8 conditions (0: disabled, 1: enabled) */
#define	SR_IMsk		0xff00		/* Interrupt mask */
#define	SR_IMsk8	0x0000		/* mask level 8 */
#define	SR_IMsk7	0x8000		/* mask level 7 */
#define	SR_IMsk6	0xc000		/* mask level 6 */
#define	SR_IMsk5	0xe000		/* mask level 5 */
#define	SR_IMsk4	0xf000		/* mask level 4 */
#define	SR_IMsk3	0xf800		/* mask level 3 */
#define	SR_IMsk2	0xfc00		/* mask level 2 */
#define	SR_IMsk1	0xfe00		/* mask level 1 */
#define	SR_IMsk0	0xff00		/* mask level 0 */

#define	SR_IM7		0x8000		/* Enable interrupt level 7 */
#define	SR_IM6		0x4000		/* Enable interrupt level 6 */
#define	SR_IM5		0x2000		/* Enable interrupt level 5 */
#define	SR_IM4		0x1000		/* Enable interrupt level 4 */
#define	SR_IM3		0x0800		/* Enable interrupt level 3 */
#define	SR_IM2		0x0400		/* Enable interrupt level 2 */
#define	SR_IM1		0x0200		/* Enable interrupt level 1 */
#define	SR_IM0		0x0100		/* Enable interrupt level 0 */

#define SR_KX		0x0080		/* kernel extended addressing */
#define SR_SX		0x0040		/* supervisor extended addressing*/
#define SR_UX		0x0020		/* user extended addressing*/
#define SR_KSUMsk	0x0018		/* mode - user,supervisor, or kernel */
#define SR_KSUUser	0x0010		/* user state */
#define SR_KSUSuperv	0x0008		/* supervisor state */
#define SR_KSUKernel	0x0000		/* kernel state */
#define SR_ERL		0x0004		/* error level */
#define SR_EXL		0x0002		/* exception level, 0:normal 1:except*/
#define SR_IE		0x0001		/* interrupts enable, 0:disable */

/*------------------------------------------------------------------+
| C0_Cause bits                                                     |
+------------------------------------------------------------------*/
#define	Cause_BD	0x80000000	/* Branch delay slot */
#define	Cause_CEMsk	0x30000000	/* coprocessor error */
#define	Cause_CEMskB	0xcfffffff	/* coprocessor error */
#define Cause_CE0	0x00000000
#define Cause_CE1	0x10000000
#define Cause_CE2	0x20000000
#define Cause_CE3	0x30000000
#define	Cause_CEShf	28

/* Interrupt pending bits */
#define	Cause_IPMsk	0xff00		/* Pending interrupt mask */
#define	Cause_IPShf	8
#define	Cause_IP7	0x8000		/* External level 7 pending */
#define	Cause_IP6	0x4000		/* External level 6 pending */
#define	Cause_IP5	0x2000		/* External level 5 pending */
#define	Cause_IP4	0x1000		/* External level 4 pending */
#define	Cause_IP3	0x0800		/* External level 3 pending */
#define	Cause_IP2	0x0400		/* External level 2 pending */
#define	Cause_SW1	0x0200		/* Software level 1 pending */
#define	Cause_SW0	0x0100		/* Software level 0 pending */

/* Cause register exception code */
#define	Cause_ExcMsk	0x7c		/* Exception code bits */
#define	Cause_ExcShf	2

#define	Exc_Code(x)	((x)<<2)

/* Hardware exception codes */
#define	Exc_Int		Exc_Code(0)	/* interrupt */
#define	Exc_Mod		Exc_Code(1)	/* TLB mod */
#define	Exc_TLBL	Exc_Code(2)	/* Read TLB Miss (load/I fetch) */
#define	Exc_TLBS	Exc_Code(3)	/* Write TLB Misss (store) */
#define	Exc_AdEL	Exc_Code(4)	/* Read Address Error */
#define	Exc_AdES	Exc_Code(5)	/* Write Address Error */
#define	Exc_IBE		Exc_Code(6)	/* Instruction Bus Error */
#define	Exc_DBE		Exc_Code(7)	/* Data Bus Error */
#define	Exc_Sys		Exc_Code(8)	/* SysCall */
#define	Exc_Bp		Exc_Code(9)	/* Breakpoint */
#define	Exc_RI		Exc_Code(10)	/* Reserved Instruction */
#define	Exc_CpU		Exc_Code(11)	/* CoProcessor Unusable */
#define	Exc_Ov		Exc_Code(12)	/* Overflow */
#define	Exc_Tr		Exc_Code(13)	/* trap instruction */
/* #define Exc_VCEI	Exc_Code(14)	 * instr virtual coherency exception */
#define Exc_FPE		Exc_Code(15)	/* floating point exception */
#define Exc_Watch	Exc_Code(23)	/* watch excecption */
/* #define Exc_VCED	Exc_Code(31)	 * data virtual coherency exception */

/* Software exception codes */
#define	SExc_SegV	Exc_Code(16)	/* Software detected seg viol */
#define	SExc_Resched	Exc_Code(17)	/* resched request */
#define	SExc_PageIn	Exc_Code(18)	/* page-in request */
#define	SExc_CU		Exc_Code(19)	/* coprocessor unusable */

#define DecExc(x)	(1 << (x>>2))	/* Decode Excep Code to 1 of 32 bits */

/*------------------------------------------------------------------+
| C0_EPC bits                                                       |
+------------------------------------------------------------------*/

/*----------------------------------------------------------------------+
| C0_PRId bits                                                          |
| Hardwired value equals PRId_Value (rev1.1->0x900, rev1.2->0x912, etc.)|
+----------------------------------------------------------------------*/
#define PRId_ImpMsk	0xff00
#define PRId_ImpShf	0xff00
#define PRId_RevMsk	0x00ff

/*------------------------------------------------------------------+
| C0_Config bits                                                    |
+------------------------------------------------------------------*/
#define Config_ICMsk	 0xe0000000	/* IC  Icache size (value=3 : 32 kb) */
#define Config_DCMsk	 0x1c000000	/* DC  Dcache size (value=3 : 32 kb) */
#define Config_SSMsk	 0x001c0000	/* SS  scache size (0:512K - 5:16M)  */
#define Config_SCCorEn	 0x00004000	/* SK  enable 1-bit error correction */
#define Config_SCBlkSize 0x00002000	/* SB  SC block size 0:16, 1:32 wds  */
#define Config_MemEnd	 0x00008000	/* BE  Endian   0:Little, 1:Big      */
#define Config_SCCkDMsk	 0x00007000	/* SC  SCClkDiv Mask		     */
#define Config_SCCkDBy(x) ((x-1) << 12)	/* SC  SCClkDiv ratio - Divide by x  */
#define Config_SCkDMsk	 0x00000e00	/* EC  SysClkDiv Mask		     */
#define Config_SCkDBy(x)  ((x-1) << 9)	/* EC  SysClkDiv ratio - Divide by x */
#define Config_PRMMsk	 0x00000180	/* PM  PrcReqMax mask		     */
#define Config_PER	 0x00000040	/* PE  PrcElmReq	     	     */
#define Config_CPRT	 0x00000020	/* CT  CohPrcReqTar		     */
#define Config_DevNumMsk 0x00000018	/* DN  Device number mask            */
#define Config_K0Msk	 0x00000007	/* K0  Kseg0 Cache Coher Algorithm   */

/* #define Config_SFOB	 0x00010000	* SyncFlshOB (Removed on 7-6-94)     */

/* Redundant, unapproved versions */
#define Config_SBMsk	0x00800000	/* S-cache block size */
#define Config_SB16	0x00000000	/* S-cache block size 16 words*/
#define Config_SB32	0x00800000	/* S-cache block size 32 words*/
#define Config_BE	0x00008000	/* Endian: 0 - Little 1 - Big */

/*------------------------------------------------------------------+
| C0_LLAddr bits                                                     |
+------------------------------------------------------------------*/
#define LLAddr_PAddrMsk	0xfffffffff

/*------------------------------------------------------------------+
| C0_WatchLo & C0_WatchHi bits                                      |
+------------------------------------------------------------------*/
#define	Watch_PAddr0Msk	0xfffffff8
#define	Watch_R		0x2
#define	Watch_W		0x1
#define	Watch_PAddr1Msk	0xff

/*------------------------------------------------------------------+
| C0_XContext bits                                                  |
+------------------------------------------------------------------*/
#define XContext_PTEBaseMskHi	0xffffffe0
#define XContext_PTEBaseShf	37
#define XContext_RMskHi		0x18
#define XContext_RShf		35
#define XContext_BadVPN2MskHi	0x7
#define XContext_BadVPN2MskLo	0xfffffff0
#define XContext_BadVPN2Shf	4

/*------------------------------------------------------------------+
| C0_FrameMask bits                                                 |
+------------------------------------------------------------------*/

/*------------------------------------------------------------------+
| C0_Diag bits                                                      |
+------------------------------------------------------------------*/
/* See br_macros.h */

/*------------------------------------------------------------------+
| C0_Perf bits                                                      |
+------------------------------------------------------------------*/
/*
 * Until the compiler people add the 4 new instrs to do the mfc0 & mtc0
 * to C0_Perf, these macros may be used. 
 *
#define MFPC(reg,num)	.word	(0x4000c801 | (reg<<16) | (num<<1))
#define MFPS(reg,num)	.word	(0x4000c800 | (reg<<16) | (num<<1))
#define MTPC(reg,num)	.word	(0x4080c801 | (reg<<16) | (num<<1))
#define MTPS(reg,num)	.word	(0x4080c800 | (reg<<16) | (num<<1))
 */

#define Perf_EventMsk	0x01e0
#define Perf_IE		0x0010
#define Perf_U		0x0008
#define Perf_S		0x0004
#define Perf_K		0x0002
#define Perf_EXL	0x0001

/*------------------------------------------------------------------+
| C0_ECC bits                                                       |
+------------------------------------------------------------------*/
#define ECC_ECCMsk	0xff

/*------------------------------------------------------------------+
| C0_CacheErr bits (based on System Design Using T5 - 7/25/94)      |
+------------------------------------------------------------------*/

#define CacheErr_Level     0xC0000000   /* Cache level and type */
#define CacheErr_PI_Level  0x00000000   /* Cache level and type */
#define CacheErr_PD_Level  0x40000000   /* Cache level and type */
#define CacheErr_SD_Level  0x80000000   /* Cache level and type */
#define CacheErr_SI_Level  0xC0000000   /* Cache level and type */

#define CacheErr_PI_EW     0x20000000   /* Error while holding previous error*/
#define CacheErr_PI_D      0x0c000000   /* Data array error (way1||way0)) */
#define CacheErr_PI_D_1    0x08000000   /* Data array error (way1) */
#define CacheErr_PI_D_0    0x04000000   /* Data array error (way0) */
#define CacheErr_PI_TA     0x03000000   /* Tag addr array error (way1||way0)*/
#define CacheErr_PI_TA_1   0x02000000   /* Tag address array error (way1) */
#define CacheErr_PI_TA_0   0x01000000   /* Tag address array error (way0) */
#define CacheErr_PI_TS     0x00c00000   /* Tag state array error (way1||way0)*/
#define CacheErr_PI_TS_1   0x00800000   /* Tag state array error (way1) */
#define CacheErr_PI_TS_0   0x00400000   /* Tag state array error (way0) */
#define CacheErr_PI_PIdx   0x00003fc0   /* Primary cache virtual word index */

#define CacheErr_PD_EW     0x20000000   /* Error while holding previous error*/
#define CacheErr_PD_EE     0x10000000   /* Error exclusive state */
#define CacheErr_PD_D      0x0c000000   /* Data array error (way1||way0)) */
#define CacheErr_PD_D_1    0x08000000   /* Data array error (way1) */
#define CacheErr_PD_D_0    0x04000000   /* Data array error (way0) */
#define CacheErr_PD_TA     0x03000000   /* Tag addr array error (way1||way0) */
#define CacheErr_PD_TA_1   0x02000000   /* Tag address array error (way1) */
#define CacheErr_PD_TA_0   0x01000000   /* Tag address array error (way0) */
#define CacheErr_PD_TS     0x00c00000   /* Tag state array error (way1||way0)*/
#define CacheErr_PD_TS_1   0x00800000   /* Tag state array error (way1) */
#define CacheErr_PD_TS_0   0x00400000   /* Tag state array error (way0) */
#define CacheErr_PD_TM     0x00300000   /* Tag mod array error (way1||way0) */
#define CacheErr_PD_TM_1   0x00200000   /* Tag mod array error (way1) */
#define CacheErr_PD_TM_0   0x00100000   /* Tag mod array error (way0) */
#define CacheErr_PD_PIdx   0x00003ff8   /* Primary cache virtual word index */

#define CacheErr_SD_EW     0x20000000   /* Error while holding previous error*/
#define CacheErr_SD_EE     0x10000000   /* Error exclusive state */
#define CacheErr_SD_D      0x0c000000   /* Uncorrectable data err(way1||way0)*/
#define CacheErr_SD_D_1    0x08000000   /* Uncorrectable data err (way1) */
#define CacheErr_SD_D_0    0x04000000   /* Uncorrectable data err (way0) */
#define CacheErr_SD_TA     0x03000000   /* Uncorrectable tag err (way1||way0)*/
#define CacheErr_SD_TA_1   0x02000000   /* Uncorrectable tag error (way1) */
#define CacheErr_SD_TA_0   0x01000000   /* Uncorrectable tag error (way0) */
#define CacheErr_SD_SIdx   0x007fffc0   /* S-cache physical double-word index*/
#define CacheErr_SD_PIdx   0x00000003   /* Primary cache virtual index */

#define CacheErr_SI_EW     0x20000000   /* Error while holding previous error*/
#define CacheErr_SI_EE     0x10000000   /* Error exclusive state */
#define CacheErr_SI_D      0x0c000000   /* Uncrctbl blk data resp(way1||way0)*/
#define CacheErr_SI_D_1    0x08000000   /* Uncrctbl blk data resp err (way1) */
#define CacheErr_SI_D_0    0x04000000   /* Uncrctbl blk data resp err (way0) */
#define CacheErr_SI_SA     0x02000000   /* Uncrctbl system address bus error */
#define CacheErr_SI_SC     0x01000000   /* Uncrctbl system command bus error */
#define CacheErr_SI_SR     0x00800000   /* Uncrctbl system response bus error*/
#define CacheErr_SI_SIdx   0x007FFFc0   /* S-cache phys quad-word index/protocol error type */

/*------------------------------------------------------------------+
| C0_TagLo bits (defines for P-Cache and S-Cache formats)           |
+------------------------------------------------------------------*/
#define TagLo_PTagLoMsk	0xffffff00
#define TagLo_PTagLoShf	8
#define TagLo_PStateMsk	0xc0
#define TagLo_PStateShf	6
#define TagLo_P		0x1

#define TagLo_STagLoMsk	0xffffe000
#define TagLo_STagLoShf	13
#define TagLo_SStateMsk	0x0c00
#define TagLo_SStateShf	10
#define TagLo_VIndexMsk	0x0380
#define TagLo_VIndexShf	7
#define TagLo_ECCMsk	0x007f

/*------------------------------------------------------------------+
| C0_TagHi bits  (defines for P-Cache and S-Cache formats)          |
+------------------------------------------------------------------*/

/*------------------------------------------------------------------+
| C0_ErrorEPC bits                                                  |
+------------------------------------------------------------------*/


/***********************************************************************
 ** SECTION 4     	                                              **
 ** Coprocessor 1 Registers                                           **
 ***********************************************************************/
/*  fp Data Registers  */
#define fp0	$f0
#define fp1	$f1
#define fp2	$f2
#define fp3	$f3
#define fp4	$f4
#define fp5	$f5
#define fp6	$f6
#define fp7	$f7
#define fp8	$f8
#define fp9	$f9
#define fp10	$f10
#define fp11	$f11
#define fp12	$f12
#define fp13	$f13
#define fp14	$f14
#define fp15	$f15
#define fp16	$f16
#define fp17	$f17
#define fp18	$f18
#define fp19	$f19
#define fp20	$f20
#define fp21	$f21
#define fp22	$f22
#define fp23	$f23
#define fp24	$f24
#define fp25	$f25
#define fp26	$f26
#define fp27	$f27
#define fp28	$f28
#define fp29	$f29
#define fp30	$f30
#define fp31	$f31

/*  FP Control Registers  */
#define C1_IR	$0
#define C1_SR	$31


/*  FP Condition Code bits */
#define cc0     $fcc0
#define cc1     $fcc1
#define cc2     $fcc2
#define cc3     $fcc3
#define cc4     $fcc4
#define cc5     $fcc5
#define cc6     $fcc6
#define cc7     $fcc7


/* Floating Point Constants */

#define C1_IR_ImpMsk	0x0000ff00
#define C1_IR_RevMsk	0x000000ff

#define FP_RN 		0
#define FP_RZ 		1
#define FP_RP 		2
#define FP_RM 		3

#define FP_RMODE	0x3
#define FP_STKY		0x7c
#define FP_ENABLE 	0xf80
#define FP_EXC 		0x3f000

#define FP_STKY_I	0x4
#define FP_STKY_U	0x8
#define FP_STKY_O	0x10
#define FP_STKY_Z	0x20
#define FP_STKY_V	0x40
#define FP_EN_I	 	0x80
#define FP_EN_U	 	0x100
#define FP_EN_O 	0x200
#define FP_EN_Z 	0x400
#define FP_EN_V 	0x800
#define FP_EXC_I 	0x1000
#define FP_EXC_U 	0x2000
#define FP_EXC_O 	0x4000
#define FP_EXC_Z 	0x8000
#define FP_EXC_V 	0x10000
#define FP_EXC_E 	0x20000
#define FP_EXC_VZOUI 	0x1F000
#define FP_EXC_ALL 	0x3F000

#define FP_CondMsk 	0xfe800000
#define FP_COND 	0x800000
#define FP_CC0 		0x800000
#define FP_CC1 		0x2000000
#define FP_CC2 		0x4000000
#define FP_CC3 		0x8000000
#define FP_CC4 		0x10000000
#define FP_CC5 		0x20000000
#define FP_CC6 		0x40000000
#define FP_CC7 		0x80000000
#define FP_SLEAZE 	0x1000000

		
/*floating point constants for min/max/NaN/infinity   */

#define	MAXSINGLE		0x1.fffffeH0xfe
#define	MINSINGLE		0x0.000002H0x0
#define	MINNORMSINGLE		0x1H0x1
#define	MAXDENORMSINGLE		0x0.fffffeH0x0

#define	DMAXSINGLE		0x1.fffffeH127
#define	DMINSINGLE		0x1.0H-150
#define	DMINNORMSINGLE		0x1H-126
#define	DMAXDENORMSINGLE	0x1.fffffcH-128

#define	MAXDOUBLE		0x1.fffffffffffffH0x7fe
#define	MINDOUBLE		0x0.0000000000001H0x0
#define	MINNORMDOUBLE		0x1H0x1
#define	MAXDENORMDOUBLE		0x0.fffffffffffffH0x0

#define	MAXEXTENDED		0x1.fffffffffffffffeH0x7ffe
#define	MINEXTENDED		0x0.0000000000000002H0x0
#define	MINNORMEXTENDED		0x1H0x1
#define	MAXDENORMEXTENDED	0x0.fffffffffffffffeH0x0

#define SNANS			0x1.fffffeH0xff
#define QNANS			0x1.7ffffeH0xff

#define SNAND			0x1.fffffffffffffH0x7ff
#define QNAND			0x1.7ffffffffffffH0x7ff

#define SNANE			0x1.fffffffffffffffeH0x7fff
#define QNANE			0x1.7ffffffffffffffeH0x7fff

#define INFINITYS		0x1.0H0xff
#define INFINITYD		0x1.0H0x7ff
#define INFINITYE		0x1.0H0x7fff

#define MAXINT 			0x7fffffff
#define MININT 			0x80000000

#define MAXLONG			0xfffffffffffff
#define MINLONG			0xfff0000000000000

/***********************************************************************
 ** SECTION 5                             	                      **
 ** Cache, TLB, & Address Space Constants                             **
 ***********************************************************************/

/*------------------------------------------------------------------+
| Scache and Pcache defines                                         |
+------------------------------------------------------------------*/
#define M_EXCEPT        	0xb80100f3
#define TAG_MASK_S_PIdx         0x00000180     # Bits 8..7 of TagLo(S_Cache)
#define SC_VIndex_Mask          0x00003000     # Bits 13..12 of VA
#define SC_Shift_17             0x11
#define SC_Shift_18             0x12
#define SC_Shift_19             0x13
#define SC_Shift_20             0x14
#define SC_Shift_21             0x15
#define SC_Shift_22             0x16
#define SC_Shift_23             0x17
#define SC_Shift_14             0xE
#define SC_Shift_13             0xD
#define PA_Shift_12             0xC
#define SC_Shift_12             0xC
#define PA_Shift_11             0xB
#define PA_Shift_8              0x8
#define SC_Shift_7              0x7
#define SC_OFFSET		0x0001FFFF	# offset for secondary
#define PA_MASK                 0x7FFFFFFF
#define TAG_MASK_P              0x7FFFF000
#define TAG_MASK_S              0x7FFE0000	# bits 35..17
#define Set_Zero                0xFFFFFFFE
#define Set_One                 0x00000001

/* Masks and defines for the primary */
#define  TagLo_P_Parity_Mask    0x00000001	# Bit 0 of TagLo (TP)
#define  TagLo_P_Set_Mask       0x00000002	# Bit 1 of TagLo (Set)
#define  TagLo_P_SParity_Mask   0x00000004	# Bit 2 of TagLo (State Parity)
#define  TagLo_P_LRU_Mask       0x00000008	# Bit 3 of TagLo (LRU)
#define  TagLo_P_PState_Mask    0x000000C0      # Bits 7..6 of TagLo (PState)
#define  TagLo_P_PTag_Mask      0xFFFFFF00	# Bits 31..8 of TagLo (PTag0)
#define  TagLo_P_Set_W0         0x00000000	# Refilled from Secondary Way0
#define  TagLo_P_Set_W1         0x00000002	# Refilled from Secondary Way1
#define  TagLo_P_LRU_W0         0x00000000 	# Way 0 is Least Recently Used 
#define  TagLo_P_LRU_W1         0x00000008	# Way 1 is  Reacently Used
#define  TagHi_P_PTag_Mask      0x0000000F	# Bits 3..1 of TagHi (PTag1)
#define  TagHi_P_SMod_Mask      0xE0000000	# Bits 31..29 of TagHi(StateMod)
#define  TagHi_P_PMod_Neither_Refill  0x20000000 # Neither Refill or Written 
#define  TagHi_P_PMod_Written         0x40000000 # Written
#define  TagHi_P_PMod_Refill          0x80000000 # Refill
#define  TagLo_PState_Invalid         0x00000000 # Invalid   
#define  TagLo_PState_Shared          0x00000040 # Shared
#define  TagLo_PState_Clean_Exclusive 0x00000080 # Clean Exclusive
#define  TagLo_PState_Dirty_Exclusive 0x000000C0 # Dirty Exclusive
#define  TagLo_PIState_Valid          0x00000040 # Dirty Exclusive

/* Masks and defines for the Secondary */
#define  TagLo_SC_ECC_Mask    	0x0000007F	# Bits 6..0 TagLo (ECC)
#define  TagLo_SC_VIndex_Mask 	0x00000180	# Bits 8..7 of TagLo (Vindex)
#define  TagLo_SC_SState_Mask 	0x00001C00	# Bits 12..10 of TagLo (SState)
#define  TagLo_SC_STag_Mask   	0xFFFFE000	# Bits 31..13 of TagLo (STag0)
#define  TagHi_SC_STag_Mask   	0x0000000F	# Bits 3..0 of TagHi (STag1)
#define  TagHi_SC_MRU_Mask   	0x80000000	# Bits 31 of TagHi (MRU)
#define  TagHi_SC_MRU_W0        0x00000000	# Way 0 is Most Recently Used
#define  TagHi_SC_MRU_W1        0x80000000	# Way 1 is Most Recently Used
#define  TagLo_SCState_Invalid  0x00000000      # Invalid   
#define  TagLo_SCState_Shared   0x00000400 	# Shared
#define  TagLo_SCState_Clean_Exclusive  0x00000800	# Clean Exclusive
#define  TagLo_SCState_Dirty_Exclusive  0x00000C00	# Dirty Exclusive

/* Cache Algorithms */
#define CA_Uncached 	2	
#define CA_CachNCoh 	3
#define CA_CachCohE 	4
#define CA_CachCohS 	5
#define CA_UncachAcc 	7

/* TLB size Constants */
#define NTLBEntries	64
#define TLBWiredBase    0
#define NWiredEntries   8
#define TLBRandomBase   NWiredEntries
#define NRandomEntries  (NTLBEntries-NWiredEntries)

/*
 * Position of PFN is different in ENLO and PA.  Therefore, 
 * there is a different mask and an extra shift.
 */
#define PFNMask32               0xfffff000
#define PFNMask32_Kseg01        0x1ffff000
#define PFNMask64       	0xffffff000
#define PFNShf	  6	/* PhAd bits 37..12 shift to/from EntryLo bits 31..6 */

/*------------------------------------------------------------------+
| Masks for offset within a page for all page sizes                 |
+------------------------------------------------------------------*/
#define Offset4K  	0x00000fff
#define Offset16K 	0x00003fff
#define Offset64K 	0x0000ffff
#define Offset256K	0x0003ffff
#define Offset1M  	0x000fffff
#define Offset4M  	0x003fffff
#define Offset16M 	0x00ffffff

/*------------------------------------------------------------------+
| Kernel mode address spaces and conversion                         |
+------------------------------------------------------------------*/
#define	K0Base		0x80000000
#define	K0Size		0x20000000
#define	K1Base		0xa0000000
#define	K1Size		0x20000000
#define	K2Base		0xc0000000
#define	K2Size		0x20000000
#define	KPTEBase	(-0x200000)	/* pte window is at top of kseg2 */
#define	KPTESize	0x200000
#define	K3Base		0xe0000000
#define	K3Size		0x20000000
#define	KUBase		0
#define	KUSize		0x80000000
#define KSegMsk		0xe0000000	/* Use to read or clear kseg */

#define K0_to_K1(x)	((x) | 0xa0000000)	/* kseg0 to kseg1        */
#define K1_to_K0(x)	((x) & 0x9fffffff)	/* kseg1 to kseg0        */
#define K0_to_Phys(x)	((x) & 0x1fffffff)	/* kseg0 to physical     */
#define K1_to_Phys(x)	((x) & 0x1fffffff)	/* kseg1 to physical     */
#define Phys_to_K0(x)	((x) | 0x80000000)	/* physical to kseg0     */
#define Phys_to_K1(x)	((x) | 0xa0000000)	/* physical to kseg1     */

/* 64-bit addressing */
#define Xkphys_CachNonCoh(x)    ((x & 0x00ffffffffffffff) | 0x9800000000000000)
#define Xkphys_CachCohExc(x)    ((x & 0x00ffffffffffffff) | 0xa000000000000000)
#define Xkphys_CachCohExcWr(x)  ((x & 0x00ffffffffffffff) | 0xa800000000000000)
#define Xkphys_UnCach(x)        Xkphys_UnCach0(x)
#define Xkphys_UnCach0(x)       ((x & 0x00ffffffffffffff) | 0x9000000000000000)
#define Xkphys_UnCach1(x)       ((x & 0x00ffffffffffffff) | 0x9200000000000000)
#define Xkphys_UnCach2(x)       ((x & 0x00ffffffffffffff) | 0x9400000000000000)
#define Xkphys_UnCach3(x)       ((x & 0x00ffffffffffffff) | 0x9600000000000000)
#define Xkphys_UnCachAcc(x)     Xkphys_UnCachAcc0(x)
#define Xkphys_UnCachAcc0(x)    ((x & 0x00ffffffffffffff) | 0xb800000000000000)
#define Xkphys_UnCachAcc1(x)    ((x & 0x00ffffffffffffff) | 0xba00000000000000)
#define Xkphys_UnCachAcc2(x)    ((x & 0x00ffffffffffffff) | 0xbc00000000000000)
#define Xkphys_UnCachAcc3(x)    ((x & 0x00ffffffffffffff) | 0xbe00000000000000)

/* 64-bit addressing - reg verstion */
#define Xkphys_CachNonCoh_Reg(X)\
        dli     k0, 0x000000ffffffffff;\
        and     X,  k0;\
        dli     k0, 0x9800000000000000;\
        or      X,  k0

/*------------------------------------------------------------------+
| Exception Vectors                                                 |
+------------------------------------------------------------------*/
#define	Reset_ExcV	0xbfc00000		/* Reset excep vector */
#define	TLB_ExcV	K0Base			/* TLB refill excep vector */
#define	XTLB_ExcV	(K0Base | 0x80)		/* XTLB refill excep vector */
#define	CacheError_ExcV	(K1Base | 0x100)	/* Cache Error excep vector */
#define	Others_ExcV	(K0Base | 0x180)	/* Others excep vector */


/***********************************************************************
 ** SECTION 6                             	                      **
 ** cache instruction codes                                           **
 ***********************************************************************/

/*------------------------------------------------------------------+
| Cache Ops							    |
|	See /user/bobn/syst5/t5/extint/CacheOp.doc for more info    |
|								    |
|   CACHE	op, offset(base)				    |
|								    |
|	31	26 25	    21 20     16 15                   0	    |
|	--------------------------------------------------------    |
|	| CACHE   |   base    |   OP    |	offset         |    |
|       --------------------------------------------------------    |
|								    |
|      The Following will define the OP fields of the various	    |
|      cache ops based on the table below.			    |
|								    |
|	Bits 20..18						    |
|	  code	caches		name				    |
|	  ----	------		----				    |
|	  0  	I		Index Invalidate		    |
|	  0	D,S		Index Writeback Invalidate	    |
|	  1	I,D,S		Index load Tag			    |
|	  2	I,D,S		Index Store Tag			    |
|	  4	I,D,S		Hit invalidate			    |
|	  5	I               FILL                    	    |
|	  5	D,S             Hit Writeback Invalidate	    |
|	  5	I		Fill				    |
|	  6	I,D,S           Index load Data			    |
|	  7	I,D,S           Index store Data		    |
|								    |
|	Bits 17..16						    |
|	  code	name		cache				    |
|	  ----	----		-----				    |
|	  0	I		primary instruction		    |
|	  1	D		primary data			    |
|	  3	S	      	secondary                     	    |
+------------------------------------------------------------------*/
/*	MNEMONIC              		Code       20..18 17..16   */
/*      ---------------------           ----       ------ ------   */
#define Index_Invalidate_I		0x0      /*   0      0	*/
#define Index_Writeback_Inv_D		0x1      /*   0      1	*/
#define Index_Writeback_Inv_S		0x3      /*   0      3	*/
#define Index_Load_Tag_I		0x4      /*   1      0	*/
#define Index_Load_Tag_D		0x5      /*   1      1	*/
#define Index_Load_Tag_S		0x7      /*   1      3	*/
#define Index_Store_Tag_I		0x8      /*   2      0	*/
#define Index_Store_Tag_D		0x9      /*   2      1	*/
#define Index_Store_Tag_S		0xb      /*   2      3	*/
#define Hit_Invalidate_I		0x10     /*   4      0	*/
#define Hit_Invalidate_D		0x11     /*   4      1	*/
#define Hit_Invalidate_S		0x13     /*   4      3	*/
#define Fill_I				0x14     /*   5      0	*/
#define Hit_Writeback_Inv_D		0x15     /*   5      1	*/
#define Hit_Writeback_Inv_S		0x17     /*   5      3	*/
#define Index_Load_Data_I		0x18     /*   6      0	*/
#define Index_Load_Data_D		0x19     /*   6      1	*/
#define Index_Load_Data_S		0x1b     /*   6      3	*/
#define Index_Store_Data_I		0x1c     /*   7      0	*/
#define Index_Store_Data_D		0x1d     /*   7      1	*/
#define Index_Store_Data_S		0x1f     /*   7      3	*/

/***********************************************************************
 ** SECTION 7                             	                      **
 ** Code structure conventions (boot, handler, termination, etc.)     **
 ***********************************************************************/

/*------------------------------------------------------------------+
| Boot and handler constant addresses.                              |
+------------------------------------------------------------------*/
#define	Krnl_Data_Table   	0xa800000000000400
#define	TLB_HndlrV_Offs		0x00
#define	XTLB_HndlrV_Offs	0x08
#define	CacheError_HndlrV_Offs	0x10
#define	Others_HndlrV_Offs	0x18
#define	OK_Exceptions_SKIP_Offs	0x20
#define	OK_Exceptions_NOSK_Offs	0x24

/* Bits 11:10 of the Stack Address always equals the processor number in
 * MP simulations.  255 word capacity per stack
#define	Proc0_Stack		0x800013fc
#define	Proc1_Stack		0x800017fc
#define	Proc2_Stack		0x80001bfc
#define	Proc3_Stack		0x80001ffc
 */

#define StartAddr		0x80002000
#define StartAddr64		0xa800000020002000

/*------------------------------------------------------------------+
| All tests end by executing the Halt() macro which is a break      |
| instruction with a termination code placed in the 20 code bits.   |
| 								    |
| Status code is in k1						    |
| Use 64-bit addressing for Heart				    |
+------------------------------------------------------------------*/
#define PASS 0
#define FAIL 1
#define HUNG 2
/*
 * IP30 testing
 */
#define	halt(code) \
	li	v1,code; \
	beq	v1,zero,1f; \
	move	v0, zero; \
	dla	v0,1f; \
1:	j	Exit_MPK; \
	nop;

#define P_INVALID	0
#define P_SHARED	1
#define P_CEX		2
#define P_DEX		3
#define S_INVALID	0
#define S_CEX		4
#define S_DEX		5
#define S_SHARED	6

#define temp1		$1
#define temp2		$2
#define temp3		$3

/* The following general register aliases are used in some CP0 diags: */
#define ExpCacheErr     $18
#define ExpErrorEPC     $19
#define	GoodRet		$20
#define	BadRet		$21
#define	ExpStat		$22
#define	ExpCause	$23
#define	ExpEPC		$24
#define	ExpBadVA	$25
#define	ExpVect		$26
#define pline_size      $27
#define scache_mask     $28

/***********************************************************************
 ** SECTION 8                             	                      **
 ** System Model Constants and memory map                             **
 ***********************************************************************/

/* The following special locations are defined in eccError.terse.  
 * By writing to them you can cause ECC errors to occur in data, 
 * tag, and check bits.
 */

/* Important! These values must be changed if they are changed in 
 * rtl/scache/eccError.terse and vise-versa.
 */

/* Defines for the T5 system model 9-16-93 */

#define EAREGSET	0xbf000000
#define PCLKCOUNT	0		/* offset from top of structure */
#define SYSCLKCOUNT	8		/* offset from top of structure */
#define INSTRCOUNT	0x10		/* offset from top of structure */
#define BOOTMODES	0x18		/* offset from top of structure */
#define PROTOCOLERR	0x20		/* offset from top of structure */
#define CMD		0x28		/* offset from top of structure */
#define STATREG		CMD
#define SYSAD		0x30		/* offset from top of structure */
#define SYSCMD		SYSAD  + (8*32) /* offset from top of structure */
#define STATERESP	SYSCMD + (8*32)	/* offset from top of structure */
#define CDATARESP	STATERESP + 8

/* External Agent Commands */
#define CMDSHIFT	3
#define RESETCMD	(1 << CMDSHIFT)
#define INTERRUPTCMD 	(2 << CMDSHIFT)
#define NMICMD		(3 << CMDSHIFT)
#define ECCERRORCMD	(4 << CMDSHIFT)
#define SYSCMDPARCMD	(5 << CMDSHIFT)
#define SYSRESPPARCMD	(6 << CMDSHIFT)
#define RANDOMCMD	(7 << CMDSHIFT)
#define RDRDYCMD	(8 << CMDSHIFT)
#define WRRDYCMD	(9 << CMDSHIFT)
#define ARBCMD		(0xa << CMDSHIFT)
#define INTERVENTIONCMD	(0xb << CMDSHIFT)
#define INVALIDATECMD	(0xc << CMDSHIFT)
#define RESPSTATECMD	(0xd << CMDSHIFT)
#define CAPTURECMD	(0xe << CMDSHIFT)
#define ALLOCRQSTCMD	(0xf << CMDSHIFT)
#define MCAPTURECMD	(0x1f << CMDSHIFT)
#define NONTESTCMD	(0x1d << CMDSHIFT)
#define RNDMEVENTCMD	(0x1c << CMDSHIFT)
#define MEMLATECMD	(0x1b << CMDSHIFT)
#define RNDMMEVENTCMD	(0x1a << CMDSHIFT)
#define RNDMMIEVENTCMD	(0x19 << CMDSHIFT)
#define RNDMMDEVENTCMD	(0x18 << CMDSHIFT)

/* External Agent Command Register Bits */
#define SWEEP		2
#define EXPCTDATA	4
#define STATUSMASK	(0x1f << CMDSHIFT)
#define TRIGGERSHIFT	8
#define TRIGGERMASK	(0x7 << TRIGGERSHIFT)
#define IVATRIGGER	0
#define INSTRTRIGGER	(1 << TRIGGERSHIFT)
#define CLKTRIGGER	(2 << TRIGGERSHIFT)
#define IVACLKTRIGGER	(3 << TRIGGERSHIFT)
#define INSTRCLKTRIGGER	(4 << TRIGGERSHIFT)
#define STARTSHIFT	11
#define START		(1 << STARTSHIFT)
#define CLKDLYSHIFT	16
#define CLKDLYMASK	(16 << CLKDLYSHIFT)

#define ICOUNTSHIFT		32
#define TARGETSHIFT		48
#define	INTERRUPTWESHIFT	51
#define	INTERRUPTDATASHIFT	56
#define ISSUEDEVNTSHIFT		56
#define EVNTRESPSHIFT		49
#define EASTATSTORE		0x800f0000
#define EVENTMASK		0x7f
#define CLEAREVENTCNTMASK	0x801fffffffffffff

/* External Agent State_Resp Register Bits */
#define SYSSTATEMASK		0x7
#define SYSRESPMASK		0x78
#define SYSRESPSHIFT		3

/* SysCmd Bus Field */
#define	REQNUMSHIFT		8
#define	REQNUM			(0x7 << REQNUMSHIFT)

/* SysResponse Bus Field */
#define SYSRESP_ACK		0x0
#define SYSRESP_ERR		0x1
#define SYSRESP_NACK		0x2
#define SYSRESP_REQNUMSHIFT	2


/* External Agent Bootmode register defines */
#define DEVNUMMASK      0x18
#define ENDIANMASK      0x8000
#define ENDIANSHIFT     15
#define	SCKDIVMASK	0x380000
#define	SCKDIVSHIFT	19
#define BLKSIZEMASK     0x2000
#define BLKSIZESHIFT	13
#define SYSCLKDIVMASK	0x1e00
#define SYSCLKDIVSHIFT	9
#define REQMAXMASK	0x180
#define REQMAXSHIFT	7


/* The following set up cache err masks for each test: */

#define SYSDATA_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ED | CacheErr_EE)
#define DATA_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ED)
#define DATACHK_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ED)
#define TAG_CE 		(CacheErr_ER | CacheErr_EC | CacheErr_ET)
#define TAGCHK_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ET)
#define INST_CE 	(CacheErr_EC | CacheErr_ED)
#define INSTCHK_CE 	(CacheErr_EC | CacheErr_ED)
#define INSTTAG_CE 	(CacheErr_EC | CacheErr_ET)
#define INSTTAGCHK_CE 	(CacheErr_EC | CacheErr_ET)
#define DATAINTFC_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ED)
#define DATACHKINTFC_CE (CacheErr_ER | CacheErr_EC | CacheErr_ED)
#define TAGINTFC_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ET)
#define TAGCHKINTFC_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ET)
#define CACHEERR_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ED)
#define DATATAG_CE 	(CacheErr_ER | CacheErr_EC | CacheErr_ET)
#define JLD_CE 		(CacheErr_EC | CacheErr_ED | CacheErr_EB)

/*
 * SysCmd's that are r4000 responses to Snoops and Interventions
 */
#define	EOD_R_INV	0x100
#define	EOD_R_CEX	0x104
#define	EOD_R_DEX	0x105
#define	EOD_R_SH	0x106
#define	EOD_R_DSH	0x107
#define UNUSED		0xbeefcafe

/*
 * SysCmd's that are r4000 responses to Snoops and Interventions with errors
 */
#define	EOD_R_INV_ERR	0x120
#define	EOD_R_CEX_ERR	0x124
#define	EOD_R_DEX_ERR	0x125
#define	EOD_R_SH_ERR	0x126
#define	EOD_R_DSH_ERR	0x127

/*
 * Data identifiers
 */
#define	DW_R_INV	0x180
#define	DW_R_CEX	0x184
#define	DW_R_DEX	0x185
#define	DW_R_SH		0x186
#define	DW_R_DSH	0x187


/*
 * SysCmd invalidate decode
 */
#define SYSCMD_INV      0x080
#define SYSCMD_PINV     0x088
#define SYSCMD_UPD      0x0a0
#define SYSCMD_PUPD     0x0a8

/*
 * Partial word decodings for SysCmd
 */
#define ONEBYTE         0
#define TWOBYTES        1
#define THREEBYTES      2
#define FOURBYTES       3
#define FIVEBYTES       4
#define SIXBYTES        5
#define SEVENBYTES      6
#define EIGHTBYTES      7

/*
System model memory map:

Address range			Description
00000000000 - 00017ffffff	RAM
00018800000 - 00018800fff      *Disk
00018801000 - 00018801fff      *Clock
0001b000000 - 0001b000fff      *SIO
0001e000800 - 0001e000803      *SBE
0001e001000 - 0001e00100f      *Timer
0001e010000 - 0001e0100ff      *NVRAM
0001e080000 - 0001e08001f      *CPU Board
0001f000000 - 0001f0fffff	Sys Model Registers for uniprocessor
0001f100000 - 0001fbfffff	Sys Model Registers for additional processors
0001fc00000 - 0001fc7ffff	RAM (Writable Boot PROM space)
0001ff00000 - 0001ff000ff      *ID PROM
0001ff80000 - fffffffffff	RAM
*/

/***********************************************************************
 ** SECTION 9                                                         **
 ** Common Macros used in tests                                       **
 ***********************************************************************/

/* end */
