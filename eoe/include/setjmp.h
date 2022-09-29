/*
 *
 * Copyright 1992, Silicon Graphics, Inc.
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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#ifndef __SETJMP_H__
#define __SETJMP_H__
#ident "$Revision: 1.36 $"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This is an ANSI/POSIX/XPG4 header.
 */
#include <standards.h>
#define _SIGJBLEN 128		/* (sizeof(ucontext_t) / sizeof (int))
				 * or sizeof(ucontext_t) / sizeof(__int64_t)
				 */
#define _IRIX4_SIGJBLEN 28	

#if defined(_BSD_SIGNALS) || defined(_BSD_COMPAT)
/*
 * BSD setjmp == sigsetjmp
 *    _setjmp == setjmp
 */
#define _JBLEN  _SIGJBLEN
#else
#define _JBLEN  28
#endif

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include <sgidefs.h>

#if (_MIPS_ISA == _MIPS_ISA_MIPS1) || (_MIPS_ISA == _MIPS_ISA_MIPS2)
typedef int jmp_buf[_JBLEN];
#endif

#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4) 
/* Really a machreg_t but this is an ANSI header */
typedef __uint64_t jmp_buf[_JBLEN];
#endif


/*
 * ANSI functions
 */
extern int 	setjmp(jmp_buf);
extern void 	longjmp(jmp_buf, int);
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 730))
#pragma unknown_control_flow (setjmp)
#endif

#if _POSIX90 && _NO_ANSIMODE
#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2) 
typedef int sigjmp_buf[_SIGJBLEN];
#endif

#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4) 
/* Really a machreg_t but this is an ANSI header */
typedef __uint64_t sigjmp_buf[_SIGJBLEN];
#endif

extern int 	sigsetjmp(sigjmp_buf, int);
extern void 	siglongjmp(sigjmp_buf, int);
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 730))
#pragma unknown_control_flow (sigsetjmp)
#endif
#endif	/* _POSIX90 */

#if _XOPEN4UX && _NO_ANSIMODE
extern int 	_setjmp(jmp_buf);
extern void 	_longjmp(jmp_buf, int);
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 730))
#pragma unknown_control_flow (_setjmp)
#endif
#endif

#if defined(_BSD_SIGNALS) || defined(_BSD_COMPAT)
/*
 * Note that due to above _JBLEN decl, jmpbuf == sigjmp_buf for BSD
 */
#define setjmp 	BSDsetjmp
#define longjmp	BSDlongjmp
extern int	BSDsetjmp(jmp_buf);
extern void	BSDlongjmp(jmp_buf, int);
#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 730))
#pragma unknown_control_flow (BSDsetjmp)
#endif
#endif /* _BSD_SIGNALS || _BSD_COMPAT */

#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */

#if _SGIAPI && _NO_ANSIMODE

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
/*
 * jmp_buf offsets
 */
#define	JB_ONSIGSTK	0	/* onsigstack flag */
#define	JB_SIGMASK	1	/* signal mask */
#define	JB_SP		2	/* stack pointer */
#define	JB_PC		3	/* program counter */
#define	JB_V0		4	/* longjmp retval */
#define	JB_S0		5	/* callee saved regs.... */
#define	JB_S1		6
#define	JB_S2		7
#define	JB_S3		8
#define	JB_S4		9
#define	JB_S5		10
#define	JB_S6		11
#define	JB_S7		12
#define	JB_S8		13	/* frame pointer */
#define	JB_F20		14	/* callee save regs */
#define	JB_F21		15
#define	JB_F22		16
#define	JB_F23		17
#define	JB_F24		18
#define	JB_F25		19
#define	JB_F26		20
#define	JB_F27		21
#define	JB_F28		22
#define	JB_F29		23
#define	JB_F30		24
#define	JB_F31		25
#define JB_FPC_CSR	26	/* fp control and status register */
#define	JB_MAGIC	27

#define	JB_FP		13	/* frame pointer -- obsolete */

#define	JBMAGIC		0xacedbade

#define	NJBREGS		_JBLEN

/* for JB_ONSIGSTK - no longer used */
#define	JB_ONSIGSTK_SVMASK	0x1		/* need to save/restore mask */
#define	JB_ONSIGSTK_ONSTACK	0x2		/* on sigstack */
#endif

#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
/*
 * jmp_buf offsets
 */
#define	JB_SP		0	/* stack pointer */
#define	JB_PC		1	/* program counter */
#define	JB_V0		2	/* longjmp retval */
#define	JB_S0		3	/* callee saved regs.... */
#define	JB_S1		4
#define	JB_S2		5
#define	JB_S3		6
#define	JB_S4		7
#define	JB_S5		8
#define	JB_S6		9
#define	JB_S7		10
#define	JB_S8		11	/* frame pointer */
#if (_MIPS_SIM == _MIPS_SIM_ABI64)
#define	JB_F24		12	/* callee saved fp regs */
#define	JB_F25		13
#define	JB_F26		14
#define	JB_F27		15
#define	JB_F28		16
#define	JB_F29		17
#define	JB_F30		18
#else  /* _MIPS_SIM_NABI32 */
#define	JB_F20		12	/* callee save regs */
#define	JB_F21		13
#define	JB_F22		14
#define	JB_F24		15
#define	JB_F26		16
#define	JB_F28		17
#define	JB_F30		18
#endif
#define	JB_F31		19
#define JB_FPC_CSR	20	/* fp control and status register */
#define JB_GP		21	/* gp for PIC is callee save */
#define	JB_MAGIC	22
#define JB_SPARE0	23
#define JB_SPARE1	24
#define JB_SPARE2	25
#define JB_SPARE3	26
#define JB_SPARE4	27

#define	JBMAGIC		0xacedbade

#endif /* _MIPS_SIM_ABI64 */

#endif /* _SGIAPI */

#ifdef __cplusplus
}
#endif
#endif /* !__SETJMP_H__ */
