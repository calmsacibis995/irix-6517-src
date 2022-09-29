/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.2 $"

#include "ml/ml.h"

#define	M_EXCEPT	0xfbffffff /* all registers except k0 are saved. */

.extern VEC_trap

/*
 * VEC_dbe
 * Handles Data Bus Errors
 */
VECTOR(VEC_dbe, M_EXCEPT)
#ifdef MCCHIP || IPMHSIM
	SAVE_ERROR_STATE(t0,t1)
	clearbuserrm
#endif
#if IP32
	li	t0, CRM_ID|K1BASE
	ld	t1, CRM_CPU_ERROR_STAT-CRM_ID(t0)
	sd	t1, crm_cpu_error_stat
	ld	t1, CRM_CPU_ERROR_ADDR-CRM_ID(t0)
	sd	t1, crm_cpu_error_addr
	ld	t1, CRM_MEM_ERROR_STAT-CRM_ID(t0)
	sd	t1, crm_mem_error_stat
	ld	t1, CRM_MEM_ERROR_ADDR-CRM_ID(t0)
	sd	t1, crm_mem_error_addr
	ld	t1, CRM_VICE_ERROR_ADDR-CRM_ID(t0)
	sd	t1, crm_vice_error_addr
	clearbuserrm
#endif
#if HEART_CHIP
	/*
	 * do not clear the interrupts here since some of them
	 * are shared among all the processors and the interrupts need
	 * to be cleared by the processors themselves, clearing all of
	 * them here may create a race condition and causes other
	 * processors not to see interrupts intended for them
	 */

	/* get processor id */
	CLI	t0,PHYS_TO_COMPATK1(HEART_PRID)
	ld	t0,0(t0)

	/* get address of per processor interrupt mask register */
	CLI	t1,PHYS_TO_COMPATK1(HEART_IMR0)
	dsll	t2,t0,3			# processor id * 0x8
	daddu	t1,t2

	/* get mask bits for HEART exception and bus interrupt */
	li	t2,1
	daddu	t3,t0,HEART_INT_CPUPBERRSHFT
	dsllv	t2,t3
	or	t2,HEART_INT_EXC
	dsll32	t2,HEART_INT_L4SHIFT-32
	not	t2

	/*
	 * mask out the HEART exception and bus interrupt until we
	 * finish with the DBE exception, otherwise, we would get an
	 * interrupt right after we clear the SR_IE bit in VEC_trap
	 */
	ld	t3,0(t1)
	and	t3,t2
	sd	t3,0(t1)

	wbflushm
#endif	/* HEART_CHIP */

	CPUID_L	t0, VPDA_CPUID(zero)
	sd	t0, EF_CPUID(a0)
	
	/*
	 * TBD
	 * This needs to simulate the address evaluation of the
	 * instruction at EPC/BD
	 */
	j	VEC_trap
	END(VEC_dbe)
