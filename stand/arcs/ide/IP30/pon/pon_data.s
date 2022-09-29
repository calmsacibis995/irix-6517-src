#ident	"ide/IP30/pon/pon_data.s:  $Revision: 1.2 $"

/*
 * pon_data.s - walking 1s memory test for IP30
 *
 * this module contains the function pon_walkingbit, which may be called from
 * power-on level 4 code.  the purpose of this test is to make sure the data
 * lines going into the DIMMs are OK.  this test does not guarantee the
 * integrity of the DIMMs
 */

#include "asm.h"
#include "early_regdef.h"
#include "pon.h"

/* walking 1s data bus test */
LEAF(pon_walkingbit)	
	move	RA3,ra		# save return address
	move	T30,a0		# save starting address

	move	T34,zero	# clear bad DIMM mask
	move	T32,T30		# address to be tested
	li	T31,BAD_DIMM0

next_addr:
	li	T33,0x1		# initial data pattern

next_pattern:
	sd	T33,0(T32)	# write data
	not	T33
	sd	T33,16(T32)	# clobber data lines
	not	T33
	ld	a2,0(T32)	# read data
	beq	a2,T33,1f

	/* set bad DIMM mask to indicate which DIMM is at fault */
	or	T34,T31

	move	a0,T32		# address at fault
	move	a1,T33		# expected data
	jal	pon_memerr

1:
	dsll	T33,1		# walk the 1
	bne	T33,zero,next_pattern	# the 1 walks off the left end ?

	bne	T32,T30,addr_test_done	# both DIMMs done ?

	daddu	T32,T30,8	# get address for the next DIMM
	li	T31,BAD_DIMM1
	b	next_addr

addr_test_done:
	move	v0,T34
	j	RA3

	END(pon_walkingbit)
