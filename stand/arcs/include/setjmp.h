#ifndef __SETJMP_H__
#define __SETJMP_H__

#define _JBLEN	13

#if	defined(_LANGUAGE_C)

#include <sys/types.h>

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2) 
typedef int		jmp_buf[_JBLEN];
typedef int *		jmpbufptr;
#endif

#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4) 
typedef __uint64_t	jmp_buf[_JBLEN];
typedef __uint64_t *	jmpbufptr;
#endif

extern int		setjmp(jmp_buf);
extern int		longjmp(jmp_buf,int);
#endif

#undef JB_PC
#undef JB_SP
#undef JB_FP
#undef JB_S0
#undef JB_S1
#undef JB_S2
#undef JB_S3
#undef JB_S4
#undef JB_S5
#undef JB_S6
#undef JB_S7
#undef JB_S8
#undef JB_T9


#define JB_PC		0
#define JB_SP		1
#define JB_FP		2
#define JB_S0		3
#define JB_S1		4
#define JB_S2		5
#define JB_S3		6
#define JB_S4		7
#define JB_S5		8
#define JB_S6		9
#define JB_S7		10
#define JB_S8		11
#define JB_T9		12
#endif
