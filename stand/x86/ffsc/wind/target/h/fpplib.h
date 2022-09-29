/* fppLib.h - floating-point coprocessor support library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01u.26may95,ms	 added prototypes for fppRegsToCtx and fppCtxToRegs.
01t,19mar95,dvs  removed #ifdef TRON - tron no longer supported.
01s,02dec93,pme  added Am29K family support
01r,11aug93,gae  vxsim hppa.
01q,20jun93,gae  vxsim.
01p,09jun93,hdn  added support for I80X86
01o,22sep92,rrr  added support for c++
01n,19sep92,jcf  ifdef _ASMLANGUAGE of taskLib.h
01m,19sep92,smb  added include of taskLib.h
01l,04jul92,jcf  cleaned up.
01k,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01j,20feb92,yao  changed ANSI function propotype declaration for
		 fppTaskRegs{S,G}et() and removed CPU_FAMILY conditionals.
01i,09jan92,jwt  converted CPU==SPARC to CPU_FAMILY==SPARC.
01h,01nov91,wmd  modified prototype of fpregs in fppTaskRegsGet() to be double.
01g,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01f,20aug91,ajm  added MIPS support.
01e,14aug91,del  added include for I960KB support.
01d,19jul91,gae  renamed architecture specific include file to be xx<arch>.h.
01c,29apr91,hdn  added defines and macros for TRON architecture.
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,07aug89,gae  written.
*/

#ifndef __INCfppLibh
#define __INCfppLibh

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE
#include "tasklib.h"
#endif	/* _ASMLANGUAGE */

#if     CPU_FAMILY==I960
#include "arch/i960/fppi960lib.h"
#endif	/* CPU_FAMILY==I960 */

#if     CPU_FAMILY==MC680X0
#include "arch/mc68k/fppmc68klib.h"
#endif	/* CPU==MC680X0 */

#if     CPU_FAMILY==MIPS
#include "arch/mips/fppmipslib.h"
#endif	/* MIPS */

#if	CPU_FAMILY==SPARC
#include "arch/sparc/fppsparclib.h"
#endif	/* CPU_FAMILY==SPARC */

#if	CPU_FAMILY==SIMSPARCSUNOS
#include "arch/simsparc/fppsimsparclib.h"
#endif	/* CPU_FAMILY==SIMSPARCSUNOS */

#if	CPU_FAMILY==SIMHPPA
#include "arch/simhppa/fppsimhppalib.h"
#endif	/* CPU_FAMILY==SIMHPPA */

#if     CPU_FAMILY==I80X86
#include "arch/i86/fppi86lib.h"
#endif	/* CPU_FAMILY==I80X86 */

#if     CPU_FAMILY==AM29XXX
#include "arch/am29k/fppam29klib.h"
#endif	/* CPU_FAMILY==AM29XXX */

/* function declarations */

#ifndef _ASMLANGUAGE

#if defined(__STDC__) || defined(__cplusplus)

extern void 	fppInit (void);
extern void	fppShowInit (void);
extern void 	fppTaskRegsShow (int task);
extern STATUS 	fppTaskRegsGet (int task, FPREG_SET *pFpRegSet);
extern STATUS 	fppTaskRegsSet (int task, FPREG_SET *pFpRegSet);
extern STATUS 	fppProbe (void);
extern void 	fppRestore (FP_CONTEXT *pFpContext);
extern void 	fppSave (FP_CONTEXT *pFpContext);
extern void	fppRegsToCtx (FPREG_SET *pFpRegSet, FP_CONTEXT *pFpContext);
extern void	fppCtxToRegs (FP_CONTEXT *pFpContext, FPREG_SET *pFpRegSet);

#else

extern void 	fppInit ();
extern void	fppShowInit ();
extern void 	fppTaskRegsShow ();
extern STATUS 	fppTaskRegsGet ();
extern STATUS 	fppTaskRegsSet ();
extern STATUS 	fppProbe ();
extern void 	fppRestore ();
extern void 	fppSave ();
extern void	fppRegsToCtx ();
extern void	fppCtxToRegs ();

#endif	/* __STDC__ */
#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCfppLibh */
