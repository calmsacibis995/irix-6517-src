#include <sys/asm.h>
#include <sys/regdef.h>

	.set	reorder

LEAF(ULIatomic_tas)
1:
	move	t0, a1
	ll	v0, (a0)
	sc	t0, (a0)
	beq	t0, zero, 1b
	j	ra
	END(ULIatomic_tas)

LEAF(ULIatomic_tao)
1:
	move	t0, a1
	ll	v0, (a0)
	or	t0, v0
	sc	t0, (a0)
	beq	t0, zero, 1b
	j	ra
	END(ULIatomic_tao)

LEAF(ULIatomic_taadd)
1:
	move	t0, a1
	ll	v0, (a0)
	add	t0, v0
	sc	t0, (a0)
	beq	t0, zero, 1b
	j	ra
	END(ULIatomic_taadd)

