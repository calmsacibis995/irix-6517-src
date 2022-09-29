
#ident "$Revision: 1.3 $"

#include "sys/regdef.h"
#include "sys/asm.h"

/*
 * NOT general-purpose routines - called only by bzero/memset to
 * do copies using floating-point registers, on machines where
 * this gives a performance benefit.
 *
 * These don't do the full copy - the start and finish are done
 * by bzero/memset.
 *
 * void * _memset_fp( void *dst, int c, size_t n)
 * Set block of memory to contents of c, and return dst.
 *
 */

LEAF(_memset_fp)
#define c a1
#define n a2
	.set	noreorder

#if (SZREG == 8)
	sd	c,0(a0)		# store the first 8 bytes of "c"
#endif
#if (SZREG == 4)
	sw	c,0(a0)		# store the first 8 bytes of "c"
	sw	c,4(a0)		# store the first 8 bytes of "c"
#endif


	PTR_ADDIU t2,a0,0		# t2 = dst
	PTR_ADDIU t3,a0,32		# t3 = dst + 32
	l.d	$f4,(a0)		# load 8 bytes of "c" into fp register
	PTR_ADDU a0,n			# a0 = dst address at end of block move

	.align	4
32:
	s.d	$f4,0(t2)
	s.d	$f4,8(t2)
	s.d	$f4,16(t2)
	s.d	$f4,24(t2)
	beq	t3,a0,ret
	PTR_ADDIU t2,t3,32		# BDSLOT

	s.d	$f4,0(t3)
	s.d	$f4,8(t3)
	s.d	$f4,16(t3)
	s.d	$f4,24(t3)
	bne	t2,a0,32b
	PTR_ADDIU t3,t2,32		# BDSLOT

	.set	reorder
ret:
	j	ra
	END(_memset_fp)
