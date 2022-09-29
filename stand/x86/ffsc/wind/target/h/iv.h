/* iv.h - interrupt vectors */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01q,19mar95,dvs  removed #ifdef TRON - tron no longer supported.
01p,02dec93,pme  added Am29K family support
01o,11aug93,gae  vxsim hppa.
01n,20jun93,gae  vxsim.
01m,09jun93,hdn  added support for I80X86
01l,22sep92,rrr  added support for c++
01k,04jul92,jcf  cleaned up.
01j,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01i,09jan92,jwt  converted CPU==SPARC to CPU_FAMILY==SPARC.
01h,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01g,02Aug91,ajm  included MIPS support
01f,19jul91,gae  renamed architecture specific include file to be xx<arch>.h.
01e,29apr91,hdn  added defines and macros for TRON architecture.
01d,25oct90,shl  fixed CPU_FAMILY logic so 68k and sparc won't clash when
		 compiling for sparc.
01c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01b,28sep90,del  added include i960/iv.h for I960 CPU_FAMILY.
01a,07aug89,gae  written.
*/

#ifndef __INCivh
#define __INCivh

#ifdef __cplusplus
extern "C" {
#endif

#if 	CPU_FAMILY==I960
#include "arch/i960/ivi960.h"
#endif	/* CPU_FAMILY==I960 */

#if 	CPU_FAMILY==MC680X0
#include "arch/mc68k/ivmc68k.h"
#endif	/* CPU_FAMILY==MC680X0 */

#if     CPU_FAMILY==MIPS
#include "arch/mips/ivmips.h"
#endif	/* CPU_FAMILY==MIPS */

#if	CPU_FAMILY==SIMHPPA
#include "arch/simhppa/ivsimhppa.h"
#endif	/* CPU_FAMILY==SIMHPPA */

#if	CPU_FAMILY==SIMSPARCSUNOS
#include "arch/simsparc/ivsimsparc.h"
#endif	/* CPU_FAMILY==SIMSPARCSUNOS */

#if	CPU_FAMILY==SPARC
#include "arch/sparc/ivsparc.h"
#endif	/* CPU_FAMILY==SPARC */

#if     CPU_FAMILY==I80X86
#include "arch/i86/ivi86.h"
#endif	/* CPU_FAMILY==I80X86 */

#if     CPU_FAMILY==AM29XXX
#include "arch/am29k/ivam29k.h"
#endif	/* CPU_FAMILY==AM29XXX */

#ifdef __cplusplus
}
#endif

#endif /* __INCivh */
