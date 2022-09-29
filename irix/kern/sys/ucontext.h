/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * ucontext - used in /proc, sigsetjmp, signal handlers
 * NOTE: in 32 bit MIPS I mode this structure is 128 words long - the same as
 * a SIGJMPBUF
 * Another version is for the 64 bit ABI (MIPS 3/4)
 * The large version is always used by /proc
 *
 * This is an XPG header..
 * Since this is now included in sys/signal.h it has much more widespread
 * inclusion... in particular fp_csr is also the name of a register in regdef.h
 * so we carefully undefine it..
 */
#ifndef _SYS__UCONTEXT_H
#define _SYS__UCONTEXT_H
#ident "$Revision: 1.36 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <standards.h>
#include <sgidefs.h>

#if _SGIAPI && !defined(__SGI_NOUCONTEXT_COMPAT)
#define	fpregset	__fpregset
#define	fp_r		__fp_r
#define	fp_dregs	__fp_dregs
#define	fp_fregs	__fp_fregs
#define	fp_regs		__fp_regs
#define	fp_csr		__fp_csr
#define	fp_pad		__fp_pad
#define	gregs		__gregs
#define	fpregs		__fpregs
#define ucontext	__ucontext
#define sigaltstack	_sigaltstack
#endif

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)
#include <sys/types.h>

#if !defined(_SIGSET_T)
#define _SIGSET_T
typedef struct {                /* signal set type */
        __uint32_t __sigbits[4];
} sigset_t;
#endif

#if (_MIPS_SZLONG == 32)
typedef struct _sigaltstack {
	void	*ss_sp;
	size_t	ss_size;
	int	ss_flags;
} stack_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef struct _sigaltstack {
	void		*ss_sp;
	__uint32_t	ss_size;
	int		ss_flags;
} stack_t;
#endif

/*
 * SVR4 ABI version - 128 words long
 */
#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2 ||\
	 _MIPS_FPSET == 16) && !defined(_EXTENDED_CONTEXT)

typedef unsigned int greg_t;

#if _ABIAPI
#define NGREG	36
typedef greg_t gregset_t[NGREG];
#else
typedef greg_t gregset_t[36];
#endif	/* _ABIAPI */

typedef struct __fpregset {
	union {
		double		__fp_dregs[16];
		float		__fp_fregs[32];
		unsigned int	__fp_regs[32];
	} __fp_r;
	unsigned int 	__fp_csr;
	unsigned int	__fp_pad;
} fpregset_t;

typedef struct {
	gregset_t	__gregs;	/* general register set */
	fpregset_t 	__fpregs;	/* floating point register set */
} mcontext_t;

typedef struct __ucontext {
	unsigned long	uc_flags;
	struct __ucontext *uc_link;
	sigset_t   	uc_sigmask;
	stack_t 	uc_stack;
	mcontext_t 	uc_mcontext;
#if _NO_ABIAPI
	long		uc_filler[47];
	/* SGI specific */
	int		uc_triggersave;	/* state of graphic trigger */
#else
	unsigned long	uc_filler[48];
#endif
} ucontext_t;

#endif

/*
 * 64 bit version - for 64 bit applications and /proc
 * All but struct ucontext is a fixed size regardless of how the
 * app is compiled.
 * Struct must stay <= 128 64 bit words long (see setjmp.h)
 */
#if ((_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4) &&\
	(_MIPS_FPSET == 32)) || defined(_EXTENDED_CONTEXT)

typedef machreg_t greg_t;

#if _SGIAPI
/* In extended mode, include status register */
#define NGREG   37
typedef greg_t gregset_t[NGREG];
#else
typedef greg_t gregset_t[37];
#endif /* _SGIAPI */

typedef struct __fpregset {
	union {
		double		__fp_dregs[32];
#ifdef	_MIPSEB
		struct {
			__uint32_t	_fp_fill;
			float		_fp_fregs;
		} __fp_fregs[32];
#else
		struct {
			float		_fp_fregs;
			__uint32_t	_fp_fill;
		} __fp_fregs[32];
#endif
		machreg_t	__fp_regs[32];
	} __fp_r;
	__uint32_t 	__fp_csr;
	__uint32_t	__fp_pad;
} fpregset_t;

typedef struct {
	gregset_t	__gregs;	/* general register set */
	fpregset_t 	__fpregs;	/* floating point register set */
} mcontext_t;

typedef struct __ucontext {
	unsigned long	uc_flags;
	struct __ucontext *uc_link;
	sigset_t   	uc_sigmask;
	stack_t 	uc_stack;
	mcontext_t 	uc_mcontext;
	long		uc_filler[49];
} ucontext_t;

#endif /* _MIPS_ISA ... */

#if _SGIAPI
#define GETCONTEXT	0
#define SETCONTEXT	1
#endif

#endif /* _LANGUAGE_C */

#if _NO_XOPEN4 && _NO_XOPEN5
/* 
 * values for uc_flags
 * these are implementation dependent flags, that should be hidden
 * from the user interface, defining which elements of ucontext
 * are valid, and should be restored on call to setcontext
 */

#define	UC_SIGMASK	001	/* uc_sigmask */
#define	UC_STACK	002	/* uc_stack */
#define	UC_CPU		004	/* uc_mcontext.gregs? */
#define	UC_MAU		010	/* uc_mcontext.fpregs? */

#define UC_MCONTEXT	(UC_CPU|UC_MAU)

/* 
 * UC_ALL specifies the default context
 */

#define UC_ALL		(UC_SIGMASK|UC_STACK|UC_MCONTEXT)
#endif

#if _SGIAPI || _ABIAPI
#define	CTX_R0		0
#define	CTX_AT		1
#define	CTX_V0		2
#define	CTX_V1		3
#define	CTX_A0		4
#define	CTX_A1		5
#define	CTX_A2		6
#define	CTX_A3		7
#if (_MIPS_SIM == _ABIO32)
#define CTX_T0          8
#define CTX_T1          9
#define CTX_T2          10
#define CTX_T3          11
#define CTX_T4          12
#define CTX_T5          13
#define CTX_T6          14
#define CTX_T7          15
#elif (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
#define CTX_A4          8
#define CTX_A5          9
#define CTX_A6          10
#define CTX_A7          11
#define CTX_T0          12
#define CTX_T1          13
#define CTX_T2          14
#define CTX_T3          15
#endif
#define	CTX_S0		16
#define	CTX_S1		17
#define	CTX_S2		18
#define	CTX_S3		19
#define	CTX_S4		20
#define	CTX_S5		21
#define	CTX_S6		22
#define	CTX_S7		23
#define	CTX_T8		24
#define	CTX_T9		25
#define	CTX_K0		26
#define	CTX_K1		27
#define	CTX_GP		28
#define	CTX_SP		29
#define	CTX_S8		30
#define	CTX_RA		31
#define	CTX_MDLO	32
#define	CTX_MDHI	33
#define	CTX_CAUSE	34
#define	CTX_EPC		35
#define CTX_SR		36

/* defines for PS ABI compatibility */
#define	CXT_R0		CTX_R0
#define	CXT_AT		CTX_AT
#define	CXT_V0		CTX_V0
#define	CXT_V1		CTX_V1
#define	CXT_A0		CTX_A0
#define	CXT_A1		CTX_A1
#define	CXT_A2		CTX_A2
#define	CXT_A3		CTX_A3
#define	CXT_T0		CTX_T0
#define	CXT_T1		CTX_T1
#define	CXT_T2		CTX_T2
#define	CXT_T3		CTX_T3
#define	CXT_T4		CTX_T4
#define	CXT_T5		CTX_T5
#define	CXT_T6		CTX_T6
#define	CXT_T7		CTX_T7
#define	CXT_S0		CTX_S0
#define	CXT_S1		CTX_S1
#define	CXT_S2		CTX_S2
#define	CXT_S3		CTX_S3
#define	CXT_S4		CTX_S4
#define	CXT_S5		CTX_S5
#define	CXT_S6		CTX_S6
#define	CXT_S7		CTX_S7
#define	CXT_T8		CTX_T8
#define	CXT_T9		CTX_T9
#define	CXT_K0		CTX_K0
#define	CXT_K1		CTX_K1
#define	CXT_GP		CTX_GP
#define	CXT_SP		CTX_SP
#define	CXT_S8		CTX_S8
#define	CXT_RA		CTX_RA
#define	CXT_MDLO	CTX_MDLO
#define	CXT_MDHI	CTX_MDHI
#define	CXT_CAUSE	CTX_CAUSE
#define	CXT_EPC		CTX_EPC
#define CXT_SR		CTX_SR

#if ((_MIPS_SIM == _ABI64) || (_MIPS_SIM == _ABIN32))
#define CTX_FV0		0     /* return registers - caller saved */
#define CTX_FV1		2
#define CTX_FA0		12    /* argument registers - caller saved */
#define CTX_FA1		13
#define CTX_FA2		14
#define CTX_FA3		15
#define CTX_FA4		16
#define CTX_FA5		17
#define CTX_FA6		18
#define CTX_FA7		19
#define CTX_FT0		4     /* caller saved */
#define CTX_FT1		5
#define CTX_FT2		6
#define CTX_FT3		7
#define CTX_FT4		8
#define CTX_FT5		9
#define CTX_FT6		10
#define CTX_FT7		11
#if (_MIPS_SIM == _ABI64)
#define CTX_FT8		20
#define CTX_FT9		21
#define CTX_FT10	22
#define CTX_FT11	23
#define CTX_FT12	1
#define CTX_FT13	3
#define CTX_FS0		24    /* callee saved */
#define CTX_FS1		25
#define CTX_FS2		26
#define CTX_FS3		27
#define CTX_FS4		28
#define CTX_FS5		29
#define CTX_FS6		30
#define CTX_FS7		31
#endif /* (_MIPS_SIM == _ABI64) */
#if (_MIPS_SIM == _ABIN32)
#define CTX_FT8		21
#define CTX_FT9		23
#define CTX_FT10	25
#define CTX_FT11	27
#define CTX_FT12	29
#define CTX_FT13	31
#define CTX_FT14	1
#define CTX_FT15	3
#define CTX_FS0		20    /* callee saved */
#define CTX_FS1		22
#define CTX_FS2		24
#define CTX_FS3		26
#define CTX_FS4		28
#define CTX_FS5		30
#endif /* (_MIPS_SIM == _ABIN32) */

#endif /* ((_MIPS_SIM == _ABI64) || (_MIPS_SIM == _ABIN32)) */

#endif /* _SGIAPI || _ABIAPI */


#ifdef __cplusplus
}
#endif
#endif /* ! _SYS__UCONTEXT_H */
