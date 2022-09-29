/* dsmLib.h - disassembler library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01r,19mar95,dvs  removed #ifdef TRON - tron no longer supported.
01q,02dec93,pad  added Am29k family support.
01p,11aug93,gae  vxsim hppa.
01o,20jun93,gae  vxsim.
01n,09jun93,hdn  added support for I80X86
01m,22sep92,rrr  added support for c++
01l,04jul92,jcf  cleaned up.
01k,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01j,09jan92,jwt  converted CPU==SPARC to CPU_FAMILY==SPARC.
01i,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01h,10seo91,wmd  fixed typo in comments.
01g,02aug91,ajm  added MIPS support
01f,19jul91,gae  renamed architecture specific include file to be xx<arch>.h.
01e,29apr91,hdn  added defines and macros for TRON architecture.
01d,25oct90,shl  fixed CPU_FAMILY logic so 68k and sparc won't clash when
		 compiling for sparc.
01c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01b,28sep90,del  include i960/dsmLib.h.
01a,07aug89,gae  written.
*/

#ifndef __INCdsmLibh
#define __INCdsmLibh

#ifdef __cplusplus
extern "C" {
#endif

#if 	CPU_FAMILY==I960
#include "arch/i960/dsmi960lib.h"
#endif	/* CPU_FAMILY==I960 */

#if	CPU_FAMILY==MC680X0
#include "arch/mc68k/dsmmc68klib.h"
#endif	/* CPU_FAMILY==MC680X0 */

#if     CPU_FAMILY==MIPS
#include "arch/mips/dsmmipslib.h"
#endif	/* CPU_FAMILY==MIPS */

#if	CPU_FAMILY==SPARC
#include "arch/sparc/dsmsparclib.h"
#endif	/* CPU_FAMILY==SPARC */

#if	CPU_FAMILY==SIMSPARCSUNOS
#include "arch/simsparc/dsmsimsparclib.h"
#endif	/* CPU_FAMILY==SIMSPARCSUNOS */

#if	CPU_FAMILY==SIMHPPA
#include "arch/simhppa/dsmsimhppalib.h"
#endif	/* CPU_FAMILY==SIMHPPA */

#if     CPU_FAMILY==I80X86
#include "arch/i86/dsmi86lib.h"
#endif	/* CPU_FAMILY==I80X86 */

#if     CPU_FAMILY==AM29XXX
#include "arch/am29k/dsmam29klib.h"
#endif	/* CPU_FAMILY==AM29XXX */

#ifdef __cplusplus
}
#endif

#endif /* __INCdsmLibh */
