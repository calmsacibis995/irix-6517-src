/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_SN1_IP33_H__
#define __SYS_SN_SN1_IP33_H__

#if !_LANGUAGE_ASSEMBLY

#define CAUSE_BERRINTR 		CAUSE_IP7

#endif /* !_LANGUAGE_ASSEMBLY */

#if _LANGUAGE_ASSEMBLY

/*
 * KL_GET_CPUNUM (similar to EV_GET_SPNUM for EVEREST platform) reads
 * the processor number of the calling processor.  The proc parameters
 * must be a register.
 */
#define KL_GET_CPUNUM(proc) 				\
	dli	proc, LOCAL_HUB(0); 			\
	ld	proc, PI_CPU_NUM(proc)

#endif /* _LANGUAGE_ASSEMBLY */

#define SCACHE_LINESIZE	128
#define SCACHE_LINEMASK	(SCACHE_LINESIZE - 1)

/*
 * R10000 status register interrupt bit mask usage for IP27.
 */
#define SRB_SWTIMO	CAUSE_SW1	/* 0x00100 */
#define SRB_NET		CAUSE_SW2	/* 0x00200 */
#define SRB_DEV0	CAUSE_IP3	/* 0x00400 */
#define SRB_DEV1	CAUSE_IP4	/* 0x00800 */
#define SRB_TIMOCLK	CAUSE_IP5	/* 0x01000 */
#define SRB_PROFCLK	CAUSE_IP6	/* 0x02000 */
#define SRB_ERR		CAUSE_IP7	/* 0x04000 */
#define SRB_SCHEDCLK	CAUSE_IP8	/* 0x08000 */
#define SRB_UNKNOWN	CAUSE_IP9	/* 0x10000 */
#define SRB_FPINTR	CAUSE_IP10	/* 0x20000 */

#define SPLHI_DISABLED_INTS	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_SCHEDCLK|SRB_DEV0
#define SPLPROF_DISABLED_INTS	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_SCHEDCLK|SRB_DEV0|SRB_PROFCLK
#define SPL7_DISABLED_INTS	SRB_SWTIMO|SRB_NET|SRB_TIMOCLK|SRB_SCHEDCLK|SRB_DEV0|SRB_PROFCLK|SRB_DEV1|SRB_ERR | SRB_FPINTR

#define SR_HI_MASK	(SRB_DEV1 | SRB_PROFCLK | SRB_ERR)
#define SR_PROF_MASK	(SRB_DEV1 | SRB_ERR)
#if defined (PSEUDO_BEAST)
#define SR_ALL_MASK	(SR_CU0)
#else
#define SR_ALL_MASK	(0)
#endif
#define SR_IBIT_HI	SRB_DEV0
#define SR_IBIT_PROF	SRB_PROFCLK

#define SRB_SWTIMO_IDX		0
#define SRB_NET_IDX		1
#define SRB_DEV0_IDX		2
#define SRB_DEV1_IDX		3
#define SRB_TIMOCLK_IDX		4
#define SRB_PROFCLK_IDX		5
#define SRB_ERR_IDX		6
#define SRB_SCHEDCLK_IDX	7

#define NUM_CAUSE_INTRS		8


#include <sys/SN/addrs.h>

#endif /* __SYS_SN_SN1_IP33_H__ */
