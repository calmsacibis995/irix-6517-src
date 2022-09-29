/* excLib.h - exception library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01x,19mar95,dvs  removed #ifdef TRON - tron no longer supported.
01w,29jan94,gae  added ASMLANGUAGE around typedef.
01v,02dec93,pme  added Am29K family support
01u,15oct93,cd   added #ifndef _ASMLANGUAGE.
01t,11aug93,gae  vxsim hppa.
01s,20jun93,gae  vxsim.
01r,09jun93,hdn  added support for I80X86
01q,22sep92,rrr  added support for c++
01p,02aug92,jcf  removed excDeliverHook decl/added excShowInit.
01o,27jul92,rrr  added excDeliverHook decl
01n,04jul92,jcf  cleaned up.
01m,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01l,20jan92,yao  added ANSI function prototype for excInfoShow(), printExc().
		 added EXC_FAULT_TAB.
01k,09jan92,jwt  added ANSI function prototype for excJobAdd().
01j,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01i,02aug91,ajm  added MIPS support
01h,19jul91,gae  renamed architecture specific include file to be xx<arch>.h.
01g,22may91,wmd  made to include "CPU/excLib.h".
01f,29apr91,hdn  added TRON defines and macros.
01e,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01d,29sep90,del  added include i960/exc960Lib.h for I960 CPU_FAMILY
01c,01aug90,jcf  padded EXC_INFO to four byte boundary.
		 changed USHORT to UINT16.
01b,28apr89,mcl  added SPARC.
01a,28may88,dnw  written
*/

#ifndef __INCexcLibh
#define __INCexcLibh

#ifdef __cplusplus
extern "C" {
#endif

#if 	CPU_FAMILY==I960
#include "arch/i960/exci960lib.h"
#endif	/* CPU_FAMILY==I960 */

#if	CPU_FAMILY==MC680X0
#include "arch/mc68k/excmc68klib.h"
#endif	/* CPU_FAMILY==MC680X0 */

#if     CPU_FAMILY==MIPS
#include "arch/mips/excmipslib.h"
#endif	/* CPU_FAMILY==MIPS */

#if	CPU_FAMILY==SPARC
#include "arch/sparc/excsparclib.h"
#endif	/* CPU_FAMILY==SPARC */

#if	CPU_FAMILY==SIMSPARCSUNOS
#include "arch/simsparc/excsimsparclib.h"
#endif	/* CPU_FAMILY==SIMSPARCSUNOS */

#if	CPU_FAMILY==SIMHPPA
#include "arch/simhppa/excsimhppalib.h"
#endif	/* CPU_FAMILY==SIMHPPA */

#if     CPU_FAMILY==I80X86
#include "arch/i86/exci86lib.h"
#endif	/* CPU_FAMILY==I80X86 */

#if     CPU_FAMILY==AM29XXX
#include "arch/am29k/excam29klib.h"
#endif	/* CPU_FAMILY==AM29XXX */


#ifndef	_ASMLANGUAGE

typedef struct  excfaultTab
    {
    int faultType;		/* fault type */
    int subtype;		/* fault sub type */
    int signal;			/* signal */
    int code;			/* code */
    } EXC_FAULT_TAB;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)
extern STATUS 	excVecInit (void);
extern STATUS 	excShowInit (void);
extern STATUS 	excInit (void);
extern void 	excHookAdd (FUNCPTR excepHook);
extern void 	excTask (void);
extern STATUS 	excJobAdd (VOIDFUNCPTR func, int arg1, int arg2, int arg3,
			   int arg4, int arg5, int arg6);
#else
extern STATUS 	excVecInit ();
extern STATUS 	excShowInit ();
extern STATUS 	excInit ();
extern void 	excHookAdd ();
extern void 	excTask ();
extern STATUS 	excJobAdd ();
extern void 	printExc ();
#endif	/* __STDC__ */

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCexcLibh */
