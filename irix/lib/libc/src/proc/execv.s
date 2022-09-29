/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  _execv.s 1.1 */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"


#ifndef _LIBC_ABI
	.weakext	environ, _environ
	.globl	_environ
#if defined(_LIBCG0) || defined(_PIC)
	.data
#else
	.sdata
#endif /* _LIBCG0 || _PIC */
_environ:
	PTR_WORD 0
#endif /* !_LIBC_ABI */

LOCALSZ=	2		# save ra, gp
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)

	.text
	.weakext	execv, _execv

NESTED(_execv, FRAMESZ, ra)
        .mask   M_RA, RAOFF-FRAMESZ
	SETUP_GP
	PTR_SUBU sp,FRAMESZ
	SETUP_GP64(GPOFF,_execv)
	SAVE_GP(GPOFF)

	REG_S	ra,RAOFF(sp)
	PTR_L	a2,_environ
	jal	_execve
	REG_L	ra,RAOFF(sp)
	RESTORE_GP64
	PTR_ADDU sp,FRAMESZ
	j	ra
	END(_execv)			# _execv(file, argv)
