/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_TRAPLOG_H__
#define __SYS_TRAPLOG_H__


#if TRAPLOG_DEBUG && (_PAGESZ == 16384)

/* The following definitions determine the size and location of the traplog
 * buffer.  It must be in the PDA page (so area is permanently mapped in tlb)
 * and hence must also be addressable as a 16-bit negative offset from zero.
 * Otherwise the TRAPLOG macro will not work correctly in locore & utlbmiss.
 */

/*
 *	TLB_TRACE_ON
 *	
 *
 */
#define	TRAPLOG_BUFSTART	0xffffffffffffd040
#define	TRAPLOG_PTR		0xffffffffffffd000
#if defined (FULL_TRAPLOG_DEBUG)
#define	TRAPLOG_ENTRYSIZE	0x118
#define TRAPLOG_BUFEND		0xffffffffffffdfc0
#else  /* FULL_TRAPLOG_DEBUG */
#define TRAPLOG_BUFEND		0xffffffffffffdfc0
#define	TRAPLOG_ENTRYSIZE	0x40
#endif /* FULL_TRAPLOG_DEBUG */

#define	WTOB(b)	(b<<3)

#define	TRAPLOG_EPC		0
#define	TRAPLOG_BADVA		1
#define	TRAPLOG_C0_CAUSE	2
#define	TRAPLOG_C0_SR		3
#define	TRAPLOG_RA		4
#define	TRAPLOG_VPDA_CURUTHREAD	5
#define	TRAPLOG_SP		6
#define	TRAPLOG_CODE		7


#if defined (FULL_TRAPLOG_DEBUG)
#define	TRAPLOG_AT		8
#define	TRAPLOG_V0		9
#define	TRAPLOG_V1		10
#define	TRAPLOG_A0		11
#define	TRAPLOG_A1		12
#define	TRAPLOG_A2		13
#define	TRAPLOG_A3		14
#define	TRAPLOG_A4		15
#define	TRAPLOG_A5		16
#define	TRAPLOG_A6		17
#define	TRAPLOG_A7		18
#define	TRAPLOG_T0		19
#define	TRAPLOG_T1		20
#define	TRAPLOG_T2		21
#define	TRAPLOG_T3		22
#define	TRAPLOG_S0		23
#define	TRAPLOG_S1		24
#define	TRAPLOG_S2		25
#define	TRAPLOG_S3		26
#define	TRAPLOG_S4		27
#define	TRAPLOG_S5		28
#define	TRAPLOG_S6		29
#define	TRAPLOG_S7		30
#define	TRAPLOG_T8		31
#define	TRAPLOG_T9		32
#define TRAPLOG_GP		33
#define	TRAPLOG_FP		34

#define	TRAPLOG_STACK		35

#endif /* FULL_TRAPLOG_DEBUG */


#if defined (FULL_TRAPLOG_DEBUG)			
#define	TRAPLOG_SAVE_ALL_REGISTERS				\
								\
	sd	AT,WTOB(TRAPLOG_AT),(k1);			\
	sd	v0,WTOB(TRAPLOG_V0),(k1);			\
	sd	v1,WTOB(TRAPLOG_V1),(k1);			\
	sd	a0,WTOB(TRAPLOG_A0),(k1);			\
	sd	a1,WTOB(TRAPLOG_A1),(k1);			\
	sd	a2,WTOB(TRAPLOG_A2),(k1);			\
	sd	a3,WTOB(TRAPLOG_A3),(k1);			\
	sd	a4,WTOB(TRAPLOG_A4),(k1);			\
	sd	a5,WTOB(TRAPLOG_A5),(k1);			\
	sd	a6,WTOB(TRAPLOG_A6),(k1);			\
	sd	a7,WTOB(TRAPLOG_A7),(k1);			\
	sd	t0,WTOB(TRAPLOG_T0),(k1);			\
	sd	t1,WTOB(TRAPLOG_T1),(k1);			\
	sd	t2,WTOB(TRAPLOG_T2),(k1);			\
	sd	t3,WTOB(TRAPLOG_T3),(k1);			\
	sd	s0,WTOB(TRAPLOG_S0),(k1);			\
	sd	s1,WTOB(TRAPLOG_S1),(k1);			\
	sd	s2,WTOB(TRAPLOG_S2),(k1);			\
	sd	s3,WTOB(TRAPLOG_S3),(k1);			\
	sd	s4,WTOB(TRAPLOG_S4),(k1);			\
	sd	s5,WTOB(TRAPLOG_S5),(k1);			\
	sd	s6,WTOB(TRAPLOG_S6),(k1);			\
	sd	s7,WTOB(TRAPLOG_S7),(k1);			\
	sd	t8,WTOB(TRAPLOG_T8),(k1);			\
	sd	t9,WTOB(TRAPLOG_T9),(k1);			\
	sd	gp,WTOB(TRAPLOG_GP),(k1);			\
	sd	fp,WTOB(TRAPLOG_FP),(k1);

#define	TRAPLOG_RESTORE_ALL_REGISTERS				\
	ld	AT,WTOB(TRAPLOG_AT),(k1);			\
	ld	v0,WTOB(TRAPLOG_V0),(k1);			\
	ld	v1,WTOB(TRAPLOG_V1),(k1);			\
	ld	a0,WTOB(TRAPLOG_A0),(k1);			\
	ld	a1,WTOB(TRAPLOG_A1),(k1);			\
	ld	a2,WTOB(TRAPLOG_A2),(k1);			\
	ld	a3,WTOB(TRAPLOG_A3),(k1);			\
	ld	a4,WTOB(TRAPLOG_A4),(k1);			\
	ld	a5,WTOB(TRAPLOG_A5),(k1);			\
	ld	a6,WTOB(TRAPLOG_A6),(k1);			\
	ld	a7,WTOB(TRAPLOG_A7),(k1);			\
	ld	t0,WTOB(TRAPLOG_T0),(k1);			\
	ld	t1,WTOB(TRAPLOG_T1),(k1);			\
	ld	t2,WTOB(TRAPLOG_T2),(k1);			\
	ld	t3,WTOB(TRAPLOG_T3),(k1);			\
	ld	s0,WTOB(TRAPLOG_S0),(k1);			\
	ld	s1,WTOB(TRAPLOG_S1),(k1);			\
	ld	s2,WTOB(TRAPLOG_S2),(k1);			\
	ld	s3,WTOB(TRAPLOG_S3),(k1);			\
	ld	s4,WTOB(TRAPLOG_S4),(k1);			\
	ld	s5,WTOB(TRAPLOG_S5),(k1);			\
	ld	s6,WTOB(TRAPLOG_S6),(k1);			\
	ld	s7,WTOB(TRAPLOG_S7),(k1);			\
	ld	t8,WTOB(TRAPLOG_T8),(k1);			\
	ld	t9,WTOB(TRAPLOG_T9),(k1);			\
	ld	gp,WTOB(TRAPLOG_GP),(k1);			\
	ld	fp,WTOB(TRAPLOG_FP),(k1);			\
								\
	ld 	ra,WTOB(TRAPLOG_RA)(k1);			\
	ld	sp,WTOB(TRAPLOG_SP)(k1);			

#if defined (TRAPLOG_SAVE_STACK_SZ)
#define	TRAPLOG_SAVE_STACK					\
	lreg    sp,VPDA_LBOOTSTACK(zero);			\
	LA      gp,_gp;   					\
	ld	a0,WTOB(TRAPLOG_SP)(k1);			\
	dli	a1,WTOB(TRAPLOG_STACK);				\
	add	a1,k1,a1;					\
	jal	traplog_save_stack;				\
	ld      k1,TRAPLOG_PTR;    				\
	lreg    sp,WTOB(TRAPLOG_SP)(k1);			\
	lreg	gp,WTOB(TRAPLOG_GP),(k1);
#else  /* TRAPLOG_SAVE_STACK_SZ */
#define	TRAPLOG_SAVE_STACK
#endif /* TRAPLOG_STACK_TRACE_SZ */
							
	
#else  /* FULL_TRAPLOG_DEBUG */
#define	TRAPLOG_SAVE_STACK
#define TRAPLOG_SAVE_ALL_REGISTERS   
#define TRAPLOG_RESTORE_ALL_REGISTERS
#endif /* FULL_TRAPLOG_DEBUG */

/*
	DMFC0(k0, C0_CAUSE); 				\
	andi	k0,CAUSE_EXCMASK;			\
	dli	k1,EXC_BREAK;				\
	bne	k0,k1,1f;				\
*/


#define TRAPLOG(code)					\
							\
	ld	k1,TRAPLOG_PTR; 			\
	beqz	k1, 1f;					\
	nop;						\
							\
	DMFC0(k0, C0_EPC); 				\
	sd	k0, WTOB(TRAPLOG_EPC)(k1);		\
	DMFC0(k0, C0_BADVADDR); 			\
        sd      k0, WTOB(TRAPLOG_BADVA)(k1) ; 		\
	DMFC0(k0, C0_CAUSE); 				\
	sd	k0, WTOB(TRAPLOG_C0_CAUSE)(k1);		\
	DMFC0(k0, C0_SR); 				\
	sd	k0, WTOB(TRAPLOG_C0_SR)(k1);		\
	sd	ra, WTOB(TRAPLOG_RA)(k1); 		\
	ld	k0, VPDA_CURUTHREAD(zero);		\
	sd	k0, WTOB(TRAPLOG_VPDA_CURUTHREAD)(k1);	\
	sd	sp, WTOB(TRAPLOG_SP)(k1); 		\
	li	k0, code; 				\
	sd	k0, WTOB(TRAPLOG_CODE)(k1); 		\
							\
	TRAPLOG_SAVE_ALL_REGISTERS			\
	TRAPLOG_SAVE_STACK				\
	TRAPLOG_RESTORE_ALL_REGISTERS			\
							\
	daddiu	k1, TRAPLOG_ENTRYSIZE;			\
	LI	k0, TRAPLOG_BUFEND; 			\
	slt	k0, k1, k0; 				\
	bnez	k0, 2f; 				\
	nop; 						\
	LI	k1, TRAPLOG_BUFSTART; 			\
2:	sd	k1, TRAPLOG_PTR; 			\
1:

#ifndef TFP

#if defined (TLB_TRACE_ON)

#define TRAPLOG_UTLBMISS(code)	\
	sd	k1,TRAPLOG_PTR+24;	\
	ld	k1,TRAPLOG_PTR; \
	beqz	k1, 1f;	\
	sd	k0,TRAPLOG_PTR+16;	\
	DMFC0(k0, C0_EPC); \
	sd	k0, 0(k1); \
	DMFC0(k0, C0_BADVADDR); \
	sd	k0, 8(k1); \
	DMFC0(k0, C0_CAUSE); \
	sd	k0, 0x10(k1); \
	DMFC0(k0, C0_SR); \
	sd	k0, 0x18(k1); \
	sd	ra, 0x20(k1); \
	ld	k0, VPDA_CURUTHREAD(zero); \
	sd	k0, 0x28(k1); \
	sd	sp, 0x30(k1); \
	li	k0, code; \
	sd	k0, 0x38(k1); \
	daddiu	k1, TRAPLOG_ENTRYSIZE;	\
	LI	k0, TRAPLOG_BUFEND; \
	slt	k0, k1, k0; \
	bnez	k0, 2f; \
	nop; 		\
	LI	k1, TRAPLOG_BUFSTART; \
2:	sd	k1, TRAPLOG_PTR; \
	ld	k0, TRAPLOG_PTR+16; \
1:	ld	k1, TRAPLOG_PTR+24

#else /* TLB_TRACE_ON */
#define TRAPLOG_UTLBMISS(code)
#endif /* TLB_TRACE_ON */
	
#endif /* !TFP */

#else
#define TRAPLOG(code)
#define TRAPLOG_UTLBMISS(code)
#endif	/* TRAPLOG_DEBUG && (_PAGESZ == 16384) */


#ifdef _EXCEPTION_TIMER_CHECK

#define SETUP_TIMER_CHECK_DATA()		\
	.data					;\
EXPORT(exception_timer_save)			;\
	.repeat (((5*SZREG)+7)/8)		;\
	.dword		0			;\
	.endr					;\
EXPORT(exception_timer_eframe)			;\
	.repeat	(EF_SIZE/8)			;\
	.dword		0			;\
	.endr					;\
	.text

/* macro called at exception time */
#define DO_EXCEPTION_TIMER_CHECK()		\
	la	k0,exception_timer_save		;\
	sreg	AT,(0*SZREG)(k0)		;\
	sreg	k1,(1*SZREG)(k0)		;\
	sreg	t0,(2*SZREG)(k0)		;\
	MFC0(k1,C0_CAUSE)			;\
	NOP_0_4					;\
	sreg	k1,(3*SZREG)(k0)		;\
	MFC0(k1,C0_COUNT)			;\
	NOP_0_4					;\
	sreg	k1,(4*SZREG)(k0)		;\
	.set	at				;\
	la	t0,exception_timer_enable	;\
	lw	t0,0(t0)			;\
	beqz	t0,4f				;\
	nop					;\
	la	t0,exception_timer_bypass	;\
	lw	t0,0(t0)			;\
	beqz	t0,4f				;\
	nop					;\
	la	t0,r4k_compare_shadow		;\
	lw	t0,0(t0)			;\
	subu	k1,t0				;\
	subu	k1,200000000	/* 200 MHZ (about 1 second) */		;\
	bltz	k1,4f		/* --> not too late */			;\
	nop					;\
EXPORT(exception_timer_panic)			;\
	la	t0,exception_timer_bypass	;\
	sw	zero,0(t0)			;\
	la	k1,exception_timer_eframe	;\
	.set	noat				;\
	lreg	AT,(0*SZREG)(k0)		;\
	sreg	AT,EF_AT(k1)	# saved in ecc springboard		;\
	SAVEAREGS(k1)				;\
	SAVESREGS(k1)				;\
	sreg	gp,EF_GP(k1)			;\
	sreg	sp,EF_SP(k1)			;\
	sreg	ra,EF_RA(k1)			;\
	lreg	t0,(2*SZREG)(k0)		;\
						;\
	mfc0	a0,C0_SR			;\
	mfc0	a1,C0_CAUSE			;\
	DMFC0(a2,C0_EPC)			;\
	DMFC0(a3,C0_BADVADDR)			;\
	sreg	a0,EF_SR(k1)			;\
	sreg	a1,EF_CAUSE(k1)			;\
	sreg	a2,EF_EPC(k1)			;\
	sreg	a3,EF_BADVADDR(k1)		;\
						;\
	jal	tfi_save	# save tregs, v0,v1, mdlo,mdhi		;\
	nop					;\
						;\
	lreg	ra,EF_RA(k1)			;\
	lreg	k1,(0*SZREG)(k0)		;\
	.set	at				;\
	LA	a0,2f				;\
	j	stk1				;\
	nop					;\
	.data					;\
2:	.asciiz	"Exception loop at 0x%x sp:0x%x k1:0x%x\12\15"		;\
	.text					;\
	.set	noat				;\
4:						;\
	lreg	t0,(2*SZREG)(k0)		;\
	lreg	AT,(0*SZREG)(k0)

#endif /* _EXCEPTION_TIMER_CHECK */


#ifdef	R4000_LOG_EXC
#if (! SP) || (! R4000)
	<< error: R4000_LOG_EXC allowed only if SP R4000 or equivalent>>
#endif
	
#define	R4000_LOG_SIZE	256
#define	R4000_LOG_INC		16
#define R4000_EXC_LOG_ADDR 0xa0000300	

#define DO_R4000_LOG_EXC()						\
	ABS(exc_log,R4000_EXC_LOG_ADDR)					;\
									;\
	lui	k1, (R4000_EXC_LOG_ADDR >> 16)				;\
	ori	k1, (R4000_EXC_LOG_ADDR & 0xFFFF)			;\
	sreg	t0, 8(k1)		         			;\
	move	t0, k1							;\
	lw	k1, 0(t0)						;\
	nop								;\
	bne	k1, zero, 1f		                          	;\
	nop								;\
	addi	k1, t0, R4000_LOG_INC	                             	;\
	sw	k1, 0(t0)						;\
	nop								;\
1:	sw	k0, 0(k1)		                 		;\
	nop								;\
	mfc0	k0, C0_BADVADDR		                             	;\
	NOP_0_4								;\
	sw	k0, 4(k1)		                     		;\
	nop								;\
	mfc0	k0, C0_EPC						;\
	NOP_0_4								;\
	sw	k0, 8(k1)						;\
	sw	ra, 12(k1)						;\
	lw	k0, 0(k1)		                      		;\
									;\
	addi	k1, R4000_LOG_INC		           		;\
	sw	k1, 0(t0)						;\
	sub	k1, t0			                           	;\
	sub	k1, R4000_LOG_SIZE - R4000_LOG_INC			;\
	bltz	k1, 1f			                 		;\
	addi	k1, t0,R4000_LOG_INC					;\
	sw	k1, 0(t0)		                       		;\
	nop								;\
1:	lreg	t0, 8(t0)		             			;\
									;\
	MFC0(k0,C0_CAUSE)
#endif	/* R4000_LOG_EXC */

#endif	/* __SYS_TRAPLOG_H__ */
