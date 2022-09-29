#include <sys/asm.h>
#include <sys/regdef.h>

/*
 * void sigorset(k_sigset_t *s1, k_sigset_t *s2);
 *
 * Atomically ORs s2 to s1.  Loops until successful.
 *
 * Returns: nothing.
 */
LEAF(__ksigorset)
	.set	noreorder
	# a0 - contains address of s1
	# a1 - contains address of s2

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	/* Can't use lld/scd in this case, so do things word-wise. */
	lw	t2, 0(a1)
	lw	t3, 4(a1)

1:	ll	t0, 0(a0)
	or	t4, t0, t2
	sc	t4, 0(a0)
	beql	t4, 0, 1b
	nop

2:	ll	t1, 4(a0)
	or	t5, t1, t3
	sc	t5, 4(a0)
	beql	t5, 0, 2b
	nop
#else
        /* #if (_MIPS_SIM == _MIPS_SIM_ABI64) or N32 */
	ld	t1, 0(a1)

3:	lld	t0, 0(a0)
	or	t2, t0, t1
	scd	t2, 0(a0)
	beql	t2, 0, 3b
	nop
#endif

	j	ra
	nop
	.set	reorder
	END(__ksigorset)

/*
 * void sigdiffset(k_sigset_t *s1, k_sigset_t *s2);
 *
 * Atomically ANDs ~s2 to s1.  Loops until successful.
 *
 * Returns: nothing.
 */
LEAF(__ksigdiffset)
	.set	noreorder
	# a0 - contains address of s1
	# a1 - contains address of s2

#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	/* Can't use lld/scd in this case, so do things word-wise. */
	lw	t2, 0(a1)
	lw	t3, 4(a1)
	not	t2, t2
	not	t3, t3

1:	ll	t0, 0(a0)
	and	t4, t0, t2
	sc	t4, 0(a0)
	beql	t4, 0, 1b
	nop

2:	ll	t1, 4(a0)
	and	t5, t1, t3
	sc	t5, 4(a0)
	beql	t5, 0, 2b
	nop
#else
	/* #if (_MIPS_SIM == _MIPS_SIM_ABI64) or N32 */
	ld	t1, 0(a1)
	not	t1, t1

3:	lld	t0, 0(a0)
	and	t2, t0, t1
	scd	t2, 0(a0)
	beql	t2, 0, 3b
	nop
#endif

	j	ra
	nop
	.set	reorder
	END(__ksigdiffset)
