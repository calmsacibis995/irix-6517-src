/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Id: ckpt_sbrk.s,v 1.1 1995/08/29 01:30:04 beck Exp $"

	.weakext	sbrk, _sbrk
#ifndef  _LIBC_ABI
	.weakext	brk, _brk
#endif /* _LIBC_ABI */

#include <sys/regdef.h>
#include <sys/asm.h>
#include <sys.s>
#include "sys/syscall.h"

	.globl	_end
	.globl	_minbrk
	.globl	_curbrk

#if defined(_LIBCG0) || defined(_PIC)
      .data
#else
      .sdata
#endif /* _LIBCG0 || _PIC */

_minbrk:PTR_WORD	_end
_curbrk:PTR_WORD	_end

FRAMESZ= 16
	.text

LEAF(setbrk)
	PTR_S	a0,_minbrk
	PTR_S	a0,_curbrk
	j	ra
	END(setbrk)

LEAF(_sbrk)
	SETUP_GP
	USE_ALT_CP(t0)
	SETUP_GP64(t0,_sbrk)
	PTR_L	v1,_curbrk
	beq	a0,zero,noincr
	PTR_ADDU	a0,v1
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	PTR_SUBU sp, FRAMESZ
	PTR_S	t0,0(sp)
#endif
	.set	noreorder
	li	v0,SYS_brk
	syscall
	.set	reorder
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	PTR_L	t0,0(sp)
	PTR_ADDU sp, FRAMESZ
#endif
	bne	a3,zero,err
	move	v0,v1			# return previous curbrk
	PTR_S	a0,_curbrk		# update to new curbrk
	j	ra

noincr:
	move	v0,v1			# return previous curbrk
	RESTORE_GP64
	j	ra

err:
	JCERROR
	END(_sbrk)


#ifndef  _LIBC_ABI
LEAF(_brk)
	SETUP_GP
	USE_ALT_CP(t0)
	SETUP_GP64(t0,_brk)
	PTR_L	v0,_minbrk
	bgeu	a0,v0,1f
	move	a0,v0
1:
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	PTR_SUBU sp, FRAMESZ
	PTR_S	t0, 0(sp)
#endif
	.set	noreorder
	li	v0,SYS_brk
	syscall
	.set	reorder
#if defined(_PIC) && (_MIPS_SIM != _MIPS_SIM_ABI32)
	PTR_L	t0, 0(sp)
	PTR_ADDU sp, FRAMESZ
#endif
	bne	a3,zero,err
	PTR_S	a0,_curbrk
	move	v0,zero
	j	ra
	END(_brk)
#endif /* _LIBC_ABI */
