
#include <asm.h>
#include <regdef.h>

#ident "$Revision: 1.3 $"

/*
 * NOT general-purpose routines - called only by bcopy/memcpy to
 * do copies using floating-point registers, on machines where
 * this gives a performance benefit.
 *
 * These don't do the full copy - the start and finish are done
 * by bcopy/memcpy.
 *
 */

/*
 * void *_memcpy_fp(to, from, count);
 *	void *to, *from;
 *	unsigned long count;
 *
 * Both functions return "to"
 */

#define	to	a0
#define	from	a1
#define	count	a2
LEAF(_memcpy_fp)
	.set	noreorder
	.align	4
64:
	ldc1	$f4,0(from)
	ldc1	$f6,8(from)
	PTR_ADDU from,16		# advance from pointer now
	LONG_SUBU count,16		# Reduce count
	sdc1	$f4,0(to)
	sdc1	$f6,8(to)
	bne	count,a3,64b		# a3: residual bytes after _bcopy_fp
	PTR_ADDU to,16			# (BDSLOT) advance to pointer now
	.set	reorder
	j	ra
	END(_memcpy_fp)
