
/*
 * ecc.s - diagnostic assembler subroutines for checking ECC logic
 */

#include "asm.h"
#include "regdef.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

/*
 * This file defines the following C-callable functions:
 *	int _eccsdlr(addr, offset, value)
 */

/* eccsdlr --
 *	write data using SDL ins
 *	a0 - address to be written to
 *	a1 - offset
 *	a2 - value to write
 *	This is a level 0 routine (C-callable).
 *
 *	long eccsdlr(addr,offset,value,expect);
 */
LEAF(eccsdlr)
	beq	zero,a1,1f
	li	t0,1
	beq	t0,a1,2f
	li	t0,2
	beq	t0,a1,3f
	li	t0,3
	beq	t0,a1,4f
	li	t0,4
	beq	t0,a1,5f
	li	t0,5
	beq	t0,a1,6f
	li	t0,6
	beq	t0,a1,7f
	li	t0,7
	beq	t0,a1,8f
	j	10f
1:
	sdr	a2,4(a0)
	sdl	a2,0(a0)
	j	10f
2:
	sdr	a2,4(a0)
	sdl	a2,1(a0)
	j	10f
3:
	sdr	a2,4(a0)
	sdl	a2,2(a0)
	j	10f
4:
	sdr	a2,4(a0)
	sdl	a2,3(a0)
	j	10f
5:
	sdr	a2,4(a0)
	sdl	a2,4(a0)
	j	10f
6:
	sdr	a2,4(a0)
	sdl	a2,5(a0)
	j	10f
7:
	sdr	a2,4(a0)
	sdl	a2,6(a0)
	j	10f
8:
	sdr	a2,4(a0)
	sdl	a2,7(a0)
	j	10f
10:
	ld	v0,0(a0)
	j	ra
	END(eccsdlr)


#ifdef	IP28

/* test_ucwrite --
 *	does uncached write (double word) in fast mode.
 *	After write, it does data compare; good compare
 *	returns 0 for success, and bad compare returns 1
 *	for failure.
 *
 *	Must be called on IP28 baseboard only.
 */

TESTFRAME=(2*SZREG)

NESTED(test_ucwrite,TESTFRAME,zero)
	/* Now must go to fast mode - save and restore calle saved registers
	 * around the call.
	 */
	PTR_SUBU sp,TESTFRAME
	REG_S	ra,0(sp)			# save return code
	REG_S	zero,8(sp)			# initialize rc

	li	a0,1
	jal	ip28_return_ucmem

        /* uncached write in fast mode */

	LI	a0,PHYS_TO_K1(0x0c00)		# misc scratch spot
	dli	a2,0x8877665544332211L
	sd	a2,0(a0)			# perform uncached store
	sync
	ld	a1,0(a0)			# get data back

	/* compare the data to see whether ucwrite was successful */
	beq	a1,a2,1f
	LI	a4,1				# bad compare, rc = 1
        REG_S   a4,8(sp)

1:
	/* return to slow mode */
	jal	ip28_enable_ucmem

	REG_L	ra,0(sp)
	REG_L   v0,8(sp)

	PTR_ADDU sp,TESTFRAME

	j	ra

	END(test_ucwrite)

#endif /* IP28 */
