#ident	"ide/IP30/pon/pon_heart.s:  $Revision: 1.10 $"

/*
 * pon_heart.s - walking 1 heart test for IP30
 *
 * Name:      pon_heart.s - walking 1 heart test for IP30
 * Function:  walking 1	heart test for IP30
 *	      NOTE: this tests only the	data path between the processor	
 *		       and the HEART ASIC.
 * Input:     none
 * Output:    returns if successfull, displays error msg if failed
 * 
 *
 */
#include <asm.h>	    
#include <early_regdef.h>  
#include <sys/cpu.h>
#include <sys/mips_addrspace.h> # needed for HEART_IM3 definition (PHYS_TO_K1)


LEAF(pon_heart)
	move	RA3,ra			# save return address

	/*
	 * test	the data path to the HEART by walking a	1 through the
	 * Interrupt Mask Register IM3 (mask for inexistant processor 3),
	 * which is a 64 bit Read/Writable register,
	 * after disabling globally all	interrupts.
	 */

					# 64 bit RW-able register on heart
	LI	T34,PHYS_TO_COMPATK1(HEART_IMR(3))
	li	a1,0x1			# set-up bit 0 

next_bit_pat:				# for each bit pattern
	sd	a1,0(T34)		# store	the double word
	ld	a2,0(T34)		# load the double word
	bne	a1,a2,disp_memerr	# call subroutine if data is bad

	dsll	a1,1			# shift logically to left 
	bne	a1,zero,next_bit_pat	# loop back to test the	next bit

	sd	zero,0(T34)		# restore the reset value
	j 	RA3			# return (ra contains the ret address)

	/* Just wing it to try and get some message out.  Soft power is
	 * not enabled yet, so just spin.  pon_initio uses T0x so it should
	 * keep our syndrome.
	 */
disp_memerr:				# if an	error msg needs	to be displayed
	jal	pon_initio
	move 	a0,T34 			# store	in a0 failed add
 	jal	pon_memerr		# this subroutine displays an err msg:
					 /*  a0 - bad address
					  *  a1 - data expected from read
					  *  a2 - actual data obtained
					  */
	jal     pon_flash_led           # never returns
	
	END(pon_heart)
