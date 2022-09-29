/*
 * bus_tags.s -
 */

#include <regdef.h>
#include <asm.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>

	.set	reorder

/* Calculate the size of the bus tags. */
LEAF(calc_bustag_size)

	move	t8, ra			# save return address

	.set	noreorder
	mfc0	t1, C0_CONFIG		# read config register
	.set	reorder

	/* read_scachesize */
	jal	read_scachesize

        # Figure out the scache block size
        and	t1, t1, CONFIG_SB	# Pull out the bits
        srl	t1, CONFIG_SB_SHFT
        addu	t1, 4			# Turn 0,1,2,3 into 4,5,6,7

	srl	v1, v0, t1		# Divide scache size by line size.
	sll	v1, 3			# Four bytes per doubleword

	/* Caclulate the size of bus tag RAM 
	 * 2^18 = 256k is the smallest possible cache.
	 * It requires 24 bits of bustags.
	 * There's a tag for each scache line.
	 */

	move	v0, zero
	addu	v0, v0, v1

	j	t8

	END(calc_bustag_size)

/*
 * return the mask for bus tag testing
 */
LEAF(get_bustag_mask)
	move	t8, ra			# save return address

	/* read_scachesize */
	jal	read_scachesize

        move    t0, zero
1:     
        sll     t0, 1
        srl     v0, 19
        ori     t0, 1
        bnez    v0, 1b

        not     t0              # t0 = mask for bus tag testing
        li      t2, 0x00ffffff  # 24 bits of 1s
        and     t0, t2          # Proper mask

	move	v0, zero
	addu	v0, v0, t0

	j	t8

	END(get_bustag_mask)

/*
 * return the size of the secondary cache as stored in the CPU EAROM
 */
LEAF(read_scachesize)
        dli     t0, EV_CACHE_SZ_LOC
        ld      t0, 0(t0)       # Read the CPU board EAROM
        li      v0, 1
        beq     t0, v0, scsz_ret
        move    v0, zero        # A cache size of 1 means no scache
        li      v0, 1
        sll     v0, v0, t0      # Shift 1 left by a0 (cache size = 2^a0)
scsz_ret:
        j       ra
        END(read_scachesize)

