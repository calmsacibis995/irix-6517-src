/*	this file exists to dummy out the functions used by kernel
	source files so they can be used unmodified in the standalone
	programs and proms.

	Otherwise we have a lot of ifdefs; the memory usage for
	these calls isn't very significant.

	One common entrypoint so we don't use any more memory than
	we have to.
*/

#include	"ml.h"
#include	"sys/asm.h"
#include	"sys/regdef.h"

LEAF(dummy_func)

/** spl functions */
XLEAF(spl0)
XLEAF(spl5)
XLEAF(splimp)
XLEAF(spl7)
XLEAF(splhi)
XLEAF(splx)

/* semaphore functions */	/* may want to ifdef !IP5 */
XLEAF(initnsema)
XLEAF(makesname)
/* XLEAF(freesplock) Defined in libsk/ml/spinlock.c */
XLEAF(freesema)
XLEAF(psema)
XLEAF(vsema)
XLEAF(cpsema)
XLEAF(svsema)
XLEAF(svpsema)
XLEAF(spsema)
XLEAF(timeout)
XLEAF(untimeout)

/* dma function never used in standalone; only kernel */
XLEAF(dma_mapbp)
/* preempt function never used in standalone; only kernel */
XLEAF(preemptchk)
XLEAF(init_sema)
XLEAF(init_spinlock)

	j ra
END(dummy_func)

LEAF(suser)		/* always suser in standalone */
	li v0, 1
	j ra
END(suser)

#if 0			/* physio not currently needed */
/* routines that call physio should never be called in standalone */
	.data	
	.align	0
$$4:
	.ascii	"physio called\X00"
	.text	
	.align	2
	.ent physio
EXPORT(physio)
	subu	sp, 24
	sw	ra, 20(sp)
	.mask	0x80000000, -4
	.frame	sp, 24, ra
	.loc	2 1
	la	a0, $$4
	jal	panic
	lw	ra, 20(sp)
	addu	sp, 24
	j	ra
END(physio)
#endif /* 0 */

/* cvsema always returns 0 in standalone, since we are single-threaded */
	.ent	cvsema 2
EXPORT(cvsema)
	.frame	sp, 0, ra
#if 0
/* What is this trying to do ? */
	sw	a0, 0(sp)
	sw	a1, 4(sp)
#endif
	move	v0, zero
	j	ra
END(cvsema)
