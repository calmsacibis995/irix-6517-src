#include <regdef.h>
#include <asm.h>

LEAF(ckpt_test_then_add)
	.set		noreorder
	LONG_LL		v0,(a0)
	LONG_ADDU	t0,v0,a1
	LONG_SC		t0,(a0)
	beqz		t0,ckpt_test_then_add
	nop
	j		ra
	nop
	.end	ckpt_test_then_add

LEAF(ckpt_add_then_test)
	.set		noreorder
	LONG_LL		v0,(a0)
	nop				# picie bug
	LONG_ADDU	v0,a1
	move		t0,v0
	LONG_SC		t0,(a0)
	beqz		t0,ckpt_add_then_test
	nop
	j		ra
	nop
	.end	ckpt_add_then_test

LEAF(ckpt_spinlock)
        .set    noreorder
        LONG_LL v0,(a0)
        move    t0,a1
        LONG_SC t0,(a0)
        beqzl   t0,ckpt_spinlock
        nop
        j       ra
        nop
        .end    ckpt_spinlock
