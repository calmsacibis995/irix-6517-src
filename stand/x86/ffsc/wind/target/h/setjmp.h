/* setjmp.h - setjmp/longjmp header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02h,27mar95,kkk  fixed multiple definition of jmp_buf & sigjmp_buf (SPR 4051)
02g,31mar94,cd   modified jmp_buf and sigjmp_buf structures for use
		 with 32/64 bit processors.
02g,12may94,ms   added macros for VxSim hppa
02f,22sep92,rrr  added support for c++
02e,31aug92,rrr  added setjmp
02d,10jul92,rrr  set it up to use the new signal code. One more pass is needed
                 to make it ANSI.
02c,09jul92,jwt  removed structure version of jmp_buf for SPARC - merge error.
02b,09jul92,rrr  fixed sparc from having two typedefs for jmp_buf
02a,04jul92,jcf  cleaned up.
01l,26may92,rrr  the tree shuffle
01k,09jan92,jwt  converted CPU==SPARC to CPU_FAMILY==SPARC.
01j,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01i,20jul91,jwt  modified jmp_buf for SPARC; added #ifndef _ASMLANGUAGE.
01h,02aug91,ajm  added defines and macros for MIPS architecture.
01g,29apr91,hdn  added defines and macros for TRON architecture.
01f,20apr91,del  added I960 specifics.
01e,19oct90,shl  changed IMPORT to extern for ANSI compatibility,
		 fixed wrong type definition for longjmp().
01d,05oct90,shl  added ANSI function prototypes.
		 added copyright notice.
                 made #endif ANSI style.
01c,25aug88,ecs  added SPARC version of jmp_buf.
01b,01jul88,rdc  changed order of stuff in jmp_buf.
01a,22jun88,dnw  written
*/

#ifndef __INCsetjmph
#define __INCsetjmph

#ifdef __cplusplus
extern "C" {
#endif

#include "regs.h"

typedef struct _jmp_buf
    {
    REG_SET reg;
    int extra[3];
    } jmp_buf[1];
typedef struct _sigjmp_buf
    {
    REG_SET regs;
    int extra[3];
    } sigjmp_buf[1];


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int	setjmp (jmp_buf __env);
extern void 	longjmp (jmp_buf __env, int __val);

extern int	sigsetjmp (sigjmp_buf __env);
extern void 	siglongjmp (sigjmp_buf __env, int __val);

#else

extern int	setjmp ();
extern void 	longjmp ();

extern int	sigsetjmp ();
extern void 	siglongjmp ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#if    CPU==SIMHPPA
extern  SETJMP();
extern  u_longjmp();
#define setjmp(buf) SETJMP((buf))
#define longjmp(buf, val) u_longjmp((buf),(val))
#endif /* CPU==SIMHPPA */

#endif /* __INCsetjmph */
