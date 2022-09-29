/* asm.h - assembler definitions header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01n,19mar95,dvs  removed #ifdef TRON - tron no longer supported.
01m,18jun93,hdn  added support for I80X86
01l,02dec93,pme  added Am29K support 
01k,11aug93,gae  vxsim hppa.
01j,20jun93,gae  vxsim.
01i,22sep92,rrr  added support for c++
01h,04jul92,jcf  cleaned up.
01g,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01e,02aug91,ajm  added MIPS support
01d,19jul91,gae  renamed architecture specific include file to be xx<arch>.h.
01c,29apr91,hdn  added defines and macros for TRON architecture.
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,07may90,gae  written.
*/

#ifndef __INCasmh
#define __INCasmh

#ifdef __cplusplus
extern "C" {
#endif

#if	CPU_FAMILY==MC680X0
#include "arch/mc68k/asmmc68k.h"
#endif	/* MC680X0 */

#if	CPU_FAMILY==SPARC
#include "arch/sparc/asmsparc.h"
#endif	/* SPARC */

#if	CPU_FAMILY==SIMSPARCSUNOS
#include "arch/simsparc/asmsimsparc.h"
#endif	/* SIMSPARCSUNOS */

#if	CPU_FAMILY==SIMHPPA
#include "arch/simhppa/asmsimhppa.h"
#endif	/* SIMHPPA */

#if     CPU_FAMILY==MIPS
#include "arch/mips/asmmips.h"
#endif	/* MIPS */

#if     CPU_FAMILY==I80X86
#include "arch/i86/asmi86.h"
#endif	/* MIPS */

#if     CPU_FAMILY==AM29XXX
#include "arch/am29k/asmam29k.h"
#endif	/* AM29XXX */

#ifdef __cplusplus
}
#endif

#endif /* __INCasmh */
