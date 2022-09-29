/* sigCodes.h - defines for the code passed to signal handlers */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01d,19mar95,dvs  removed #ifdef TRON - tron no longer supported.
01c,09jun93,hdn  added support for I80X86
01b,22sep92,rrr  added support for c++
01a,20aug92,rrr  written.
*/

#ifndef __INCsigCodesh
#define __INCsigCodesh

#ifdef __cplusplus
extern "C" {
#endif

#if 	CPU_FAMILY==I960
#include "arch/i960/sigi960codes.h"
#endif	/* CPU_FAMILY==I960 */

#if 	CPU_FAMILY==MC680X0
#include "arch/mc68k/sigmc68kcodes.h"
#endif	/* CPU_FAMILY==MC680X0 */

#if     CPU_FAMILY==MIPS
#include "arch/mips/sigmipscodes.h"
#endif	/* CPU_FAMILY==MIPS */

#if	CPU_FAMILY==SPARC
#include "arch/sparc/sigsparccodes.h"
#endif	/* CPU_FAMILY==SPARC */

#if     CPU_FAMILY==I80X86
#include "arch/i86/sigi86codes.h"
#endif	/* CPU_FAMILY==I80X86 */

#ifdef __cplusplus
}
#endif

#endif /* __INCsigCodesh */
