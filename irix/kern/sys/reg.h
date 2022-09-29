/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_REG_H__
#define __SYS_REG_H__
#ident	"$Revision: 3.30 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/*
 * Location of the users' stored
 * registers in exception frame.
 * Usage is u.u_ar0[XX] or USER_REG(x).
 */

#if (_MIPS_SIM == _ABIO32)
/* On -o32 kernels we need to reserve space for the C calling sequence
 * in case we're invoking intr(), tlbmiss(), etc.
 */
#define EF_ARGSAVE0	0		/* arg save for c calling seq */
#define EF_ARGSAVE1	1		/* arg save for c calling seq */
#define EF_ARGSAVE2	2		/* arg save for c calling seq */
#define EF_ARGSAVE3	3		/* arg save for c calling seq */
#define	EF_AT		4		/* r1:  assembler temporary */
#else
#define	EF_AT		0		/* r1:  assembler temporary */
#endif
#define	EF_V0		EF_AT+1		/* r2:  return value 0 */
#define	EF_V1		EF_AT+2		/* r3:  return value 1 */
#define	EF_A0		EF_AT+3		/* r4:  argument 0 */
#define	EF_A1		EF_AT+4		/* r5:  argument 1 */
#define	EF_A2		EF_AT+5		/* r6:  argument 2 */
#define	EF_A3		EF_AT+6		/* r7:  argument 3 */
#if (_MIPS_SIM == _ABIO32)
#define	EF_T0		EF_AT+7		/* r8:  caller saved 0 */
#define	EF_T1		EF_AT+8		/* r9:  caller saved 1 */
#define	EF_T2		EF_AT+9		/* r10: caller saved 2 */
#define	EF_T3		EF_AT+10	/* r11: caller saved 3 */
#define	EF_T4		EF_AT+11	/* r12: caller saved 4 */
#define	EF_T5		EF_AT+12	/* r13: caller saved 5 */
#define	EF_T6		EF_AT+13	/* r14: caller saved 6 */
#define	EF_T7		EF_AT+14	/* r15: caller saved 7 */
#endif
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
#define	EF_A4		EF_AT+7		/* r8:  argument 4 */
#define	EF_A5		EF_AT+8		/* r9:  argument 5 */
#define	EF_A6		EF_AT+9		/* r10: argument 6 */
#define	EF_A7		EF_AT+10	/* r11: argument 7 */
#define	EF_T0		EF_AT+11	/* r12: caller saved 0 */
#define	EF_T1		EF_AT+12	/* r13: caller saved 1 */
#define	EF_T2		EF_AT+13	/* r14: caller saved 2 */
#define	EF_T3		EF_AT+14	/* r15: caller saved 3 */
#endif
#define	EF_S0		EF_AT+15	/* r16: callee saved 0 */
#define	EF_S1		EF_AT+16	/* r17: callee saved 1 */
#define	EF_S2		EF_AT+17	/* r18: callee saved 2 */
#define	EF_S3		EF_AT+18	/* r19: callee saved 3 */
#define	EF_S4		EF_AT+19	/* r20: callee saved 4 */
#define	EF_S5		EF_AT+20	/* r21: callee saved 5 */
#define	EF_S6		EF_AT+21	/* r22: callee saved 6 */
#define	EF_S7		EF_AT+22	/* r23: callee saved 7 */
#define	EF_T8		EF_AT+23	/* r24: code generator 0 */
#define	EF_T9		EF_AT+24	/* r25: code generator 1 */
#define	EF_K0		EF_AT+25	/* r26: kernel temporary 0 */
#define	EF_K1		EF_AT+26	/* r27: kernel temporary 1 */
#define	EF_GP		EF_AT+27	/* r28: global pointer */
#define	EF_SP		EF_AT+28	/* r29: stack pointer */
#define	EF_FP		EF_AT+29	/* r30: frame pointer */
#define	EF_RA		EF_AT+30	/* r31: return address */
#define	EF_SR		EF_AT+31	/* status register */
#define	EF_MDLO		EF_AT+32	/* low mult result */
#define	EF_MDHI		EF_AT+33	/* high mult result */
#define	EF_BADVADDR	EF_AT+34	/* bad virtual address */
#define	EF_CAUSE	EF_AT+35	/* cause register */
#define	EF_EPC		EF_AT+36	/* program counter */
#define EF_FP4		EF_AT+37	/* FP temp register (TFP only) */
#define EF_CONFIG	EF_AT+38	/* TFP C0_CONFIG register (for SMM) */
#define EF_CEL		EF_AT+39	/* Everest Curr Exec Lvl */
#define EF_CRMMSK       EF_AT+39	/* MOOSEHEAD interrupt mask */
#define EF_ERROR_EPC	EF_AT+43	/* Error program counter */
#define EF_EXACT_EPC	EF_AT+44	/* Error program counter */
#if TFP
#endif

#define EF_SIZE		sizeof(struct eframe_s)
/*
 * some instruction operations work much better by assuming an array of
 * registers
 */
#define	USER_REG(x)	(((k_machreg_t *)&curexceptionp->u_eframe)[(x)])

/*
 * We make everything the same size - even though a few of the registers
 * will never be larger than 32 bits
 * WARNING: some code still accesses this as an array!
 */
typedef struct eframe_s {
#if (_MIPS_SIM == _ABIO32)
	/* On -o32 kernels we need to reserve space for the C calling sequence
	 * in case we're invoking intr(), tlbmiss(), etc.
	 */
	k_machreg_t	ef_argsave0;
	k_machreg_t	ef_argsave1;
	k_machreg_t	ef_argsave2;
	k_machreg_t	ef_argsave3;
#endif
	k_machreg_t	ef_at;
	k_machreg_t	ef_v0;
	k_machreg_t	ef_v1;
	k_machreg_t	ef_a0;
	k_machreg_t	ef_a1;
	k_machreg_t	ef_a2;
	k_machreg_t	ef_a3;
#if (_MIPS_SIM == _ABIO32)
	k_machreg_t	ef_t0;
	k_machreg_t	ef_t1;
	k_machreg_t	ef_t2;
	k_machreg_t	ef_t3;
	k_machreg_t	ef_t4;
	k_machreg_t	ef_t5;
	k_machreg_t	ef_t6;
	k_machreg_t	ef_t7;
#endif
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
	k_machreg_t	ef_a4;
	k_machreg_t	ef_a5;
	k_machreg_t	ef_a6;
	k_machreg_t	ef_a7;
	k_machreg_t	ef_t0;
	k_machreg_t	ef_t1;
	k_machreg_t	ef_t2;
	k_machreg_t	ef_t3;
#define	ef_ta0	ef_a4
#define	ef_ta1	ef_a5
#define	ef_ta2	ef_a6
#define	ef_ta3	ef_a7
#endif
	k_machreg_t	ef_s0;
	k_machreg_t	ef_s1;
	k_machreg_t	ef_s2;
	k_machreg_t	ef_s3;
	k_machreg_t	ef_s4;
	k_machreg_t	ef_s5;
	k_machreg_t	ef_s6;
	k_machreg_t	ef_s7;
	k_machreg_t	ef_t8;
	k_machreg_t	ef_t9;
	k_machreg_t	ef_k0;
	k_machreg_t	ef_k1;
	k_machreg_t	ef_gp;
	k_machreg_t	ef_sp;
	k_machreg_t	ef_fp;
	k_machreg_t	ef_ra;
	k_machreg_t	ef_sr;
	k_machreg_t	ef_mdlo;
	k_machreg_t	ef_mdhi;
	k_machreg_t	ef_badvaddr;
	k_machreg_t	ef_cause;
	k_machreg_t	ef_epc;
	k_machreg_t	ef_fp4;		/* R8000 only */
	k_machreg_t	ef_config;
#if IP32
	__uint64_t	ef_crmmsk;	/* CRIME intmask at time of excep. */
#else
	__uint64_t	ef_cel;
#endif
	k_machreg_t	ef_cpuid;
	k_machreg_t	ef_buserr_info;
	k_machreg_t	ef_buserr_spl;
	k_machreg_t	ef_error_epc;	
	/*
	 * To facilitate compiler speculative code motion, we need to know
	 * the epc at exception time. The ef_epc may not always correctly
	 * reflect this, especially when a signal is pending.
	 */
	k_machreg_t	ef_exact_epc;	
} eframe_t;

#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */
#endif /* __SYS_REG_H__ */
