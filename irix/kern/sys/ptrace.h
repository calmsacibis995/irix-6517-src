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
/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */
#ifndef __SYS_PTRACE_H__
#define __SYS_PTRACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 3.16 $"

#include <sys/types.h>

/*
 * ptrace.h -- definitions for use with ptrace() system call
 */

/*
 * ptrace request numbers
 */
#define	PTRC_SELFTRACE	0		/* enable tracing on this process */
#define	PTRC_RD_I	1		/* read user I space */
#define	PTRC_RD_D	2		/* read user D space */
#define	PTRC_RD_REG	3		/* read user registers */
#define	PTRC_WR_I	4		/* write user I space */
#define	PTRC_WR_D	5		/* write user D space */
#define	PTRC_WR_REG	6		/* write user registers */
#define	PTRC_CONTINUE	7		/* continue, possibly with signal */
#define	PTRC_TERMINATE	8		/* terminate traced process */
#define	PTRC_STEP	9		/* single step traced process */

/* special extentions to ptrace */
#define	PTRC_RD_GPRS	21		/* read all user gp registers */
#define	PTRC_RD_FPRS	22		/* read all user fp registers */
#define	PTRC_WR_GPRS	24		/* read all user gp registers */
#define	PTRC_WR_FPRS	25		/* read all user fp registers */

/*
 * register number definitions for PTRC_RD_REG's and PTRC_WR_REG's
 */
#define GPR_BASE	0			/* general purpose regs */
#define	NGP_REGS	32			/* number of gpr's */

#define FPR_BASE	(GPR_BASE+NGP_REGS)	/* fp regs */
#define	NFP_REGS	32			/* number of fp regs */

#define	SIG_BASE	(FPR_BASE+NFP_REGS)	/* sig handler addresses */
#define	NSIG_HNDLRS	32			/* number of signal handlers */

#define SPEC_BASE	(SIG_BASE+NSIG_HNDLRS)	/* base of spec purpose regs */
#define PC		SPEC_BASE+0		/* program counter */
#define	CAUSE		SPEC_BASE+1		/* cp0 cause register */
#define	BADVADDR	SPEC_BASE+2		/* bad virtual address */
#define MMHI		SPEC_BASE+3		/* multiply high */
#define MMLO		SPEC_BASE+4		/* multiply low */
#define FPC_CSR		SPEC_BASE+5		/* fp csr register */
#define FPC_EIR		SPEC_BASE+6		/* fp eir register */
#define NSPEC_REGS	7			/* number of spec registers */
#define NPTRC_REGS	(SPEC_BASE + NSPEC_REGS)

/* 
 * The registers above are made available to user debuggers 
 * (e.g. via core files). Additional special registers 
 * defined for use by symmon's rdebug follow.
 */
#define SR		SPEC_BASE+7		/* status register */
#define TLBHI		SPEC_BASE+8		/* tlb hi */
#define TLBLO		SPEC_BASE+9		/* tlb lo */
#define INX		SPEC_BASE+10		/* tlb index */
#define RAND		SPEC_BASE+11		/* tlb random */
#define CTXT		SPEC_BASE+12		/* context */
#define EXCTYPE		SPEC_BASE+13		/* exception type */
#if R4000 || R10000
#define TLBLO1		SPEC_BASE+14		/* tlb lo1 */
#define PGMSK		SPEC_BASE+15		/* page mask */
#define WIRED		SPEC_BASE+16		/* tlb wired */
#define COUNT		SPEC_BASE+17		/* count */
#define COMPARE		SPEC_BASE+18		/* compare */
#define LLADDR		SPEC_BASE+19		/* load linked address */
#define WATCHLO		SPEC_BASE+20		/* data watchpoint lo */
#define WATCHHI		SPEC_BASE+21		/* data watchpoint hi */
#define ECC		SPEC_BASE+22		/* ecc information */
#define CACHERR		SPEC_BASE+23		/* cache error */
#define TAGLO		SPEC_BASE+24		/* cache tag lo */
#define TAGHI		SPEC_BASE+25		/* cache tag hi */
#define ERREPC		SPEC_BASE+26		/* cache error epc */
#define EXTCTXT		SPEC_BASE+27		/* Extended context */
#endif /* R4000 || R10000 */

#ifndef _KERNEL
#if (_MIPS_SZPTR != _MIPS_SZINT)		/* 64 bit abi */
/* This violates the SVID API, but the prototype there
 * is useless for a 64 bit ABI.
 */
extern long ptrace(int, pid_t, void *, long);
#else
extern int ptrace(int, pid_t, int, int);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_PTRACE_H__ */
