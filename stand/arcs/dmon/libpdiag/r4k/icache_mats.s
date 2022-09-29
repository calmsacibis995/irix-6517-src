#ident	"$Id: icache_mats.s,v 1.1 1994/07/21 01:25:28 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/regdef.h"
#include "sys/asm.h"
#include "sys/sbd.h"
#include "monitor.h"

 ##############################################################################
 #									      #
 #	Aina test on memory		      				      #
 #	A 0 is returned upon success.					      #
 #									      #
 #	Functional Description:						      #
 #									      #
 #      Tests memory or cache for addressing faults.			      #
 #                                                                            #
 #      Register Used:                                                        #
 #              a0 - input of first address                                   #
 #              a1 - input of last address                                    #
 #              a3 - temp register                                            #
 #              s1 - saved first address                                      #
 #              s2 - saved last address                                       #
 #              s7 - saved return address                                     #
 #                                                                            #
 #	User Information:						      #
 #                                                                            #
 #              The input address determine whether the test is performed     #
 #      upon memory (Kseg1) or cache (Kseg0). In case of failure return,      #
 #      a0 = failing address, a1 = expected pattern, a2 = actual pattern.     #
 #									      #
 ##############################################################################

#define K0BASE_ADDR      0x88000000
#define K1BASE_ADDR      0xa8000000


LEAF(Icache_mats)

	move	s7, ra			#save the return address

        #move    s1,a0                   # save first address
        #move    s2,a1                   # save last address + 4
	jal	size_2nd_cache
	move	t9, v0

        jal     cache_init             # init all the caches

        jal     icache_size

	li	s1, K0BASE_ADDR
	li	s2, K0BASE_ADDR
        addu	s2,v0                   # last address + 4

	/*
	 * Start of aina test
	 */

	/*----------------------------------------------------------------+
	| Address-In-Address;  Ascending Stores;  Ascending Check         |
	+----------------------------------------------------------------*/
	/* TAG MEMORY */

	move	a0, s1			# get starting address.
	move	t1, s2
	li	t0, 0xa0000000
	or	a0, t0
	or	t1, t0
1:
	sw	a0, 0(a0)		# write address-in-address

	addiu	a0,a0,4			# increment to next address
	bltu	a0, t1, 1b		# reach the end ?


	/* Check for a secondary cache */
	jal	noSCache	
	nop	
	bne	v0, zero, 2f		# branch if no Scache


	/*
	 * PRIME the SECONDARY CACHE
	 */
	jal	scache_ln_size
	move	a0, s1
1:
	lw	t0, 0(a0)	# read in K0 space, forces cache reads
	addu	a0, a0, v0		# go to next line
	bltu	a0, s2, 1b
		
	/*
	 * LOAD the ICACHE
	 */
	jal	icache_ln_size
	move	t0, s1
2:
        .set noreorder
        cache   CACH_PI | C_FILL, 0(t0)            # fill I cache from memory
        nop
        .set reorder
        addu   t0,t0,v0                # next word
        blt     t0,s2,2b                # reach last address yet?

	/* 
	 * SCRUB MEMORY
	 * Now that the data is supposed to be in the Icache scrub the
	 * data in the secondary and memory
	 */
	li	t0, 0xa0000000
	move	t1, s2
	or	t1, t0
        move    a0, s1                  # first address
	or	a0, t0
        li      a1, 0xdeadbeef		# write pattern
1:
        sw      a1, 0(a0)	        # write to location.
        addiu   a0,a0,4
        bltu    a0, t1, 1b              # check for last location.

	/*
	 * Invalidate the dcache this ensures when reading from the
	 * dcache that the data is loaded in from the secondary cache
	 */
	jal	dcache_size
	move	a0, v0
	jal	dcache_ln_size
	move	a1, v0
	jal	invalidate_dcache

	/* 
	 * WRITEBACK the ICACHE
	 */
	move	t0, s1			# starting address
	move	t4, s2
	li	t3, K0BASE_ADDR
	or	t0, t3
	or	t4, t3
2:

 	/* Write back the data to memory/secondary */
        .set noreorder
        cache   CACH_PI | C_HWBINV, 0(t0)
        nop
        .set reorder
        addiu   t0,t0,4                 # next word
        blt     t0,t4,2b                # reach last address yet?


	/*
	 * The writeback does not dirty the Secondary if one exists
	 * so we must dirty the cache to get a hit in the secondary
	 */
/*
	jal	noSCache
	bne	v0, zero, 1f

	move	a0, s1			# Starting address
	#move	a1, t9			# Size of secondary cache
	li	a1, 0x100000
	jal	scache_ln_size
	addu	t1, a1, a0		# Determine last address
	li	t3, K0BASE_ADDR
	or	a0, t3
	or	t1, t3

1:
	.set	noreorder
	cache	Create_Dirty_Exc_SD, 0(a0)
	nop
	.set	reorder
	addu	a0, a0, v0
	bltu	a0, t1, 1b
*/
	

	move	a0, s1			# get starting address.
	li	a1, K1BASE_ADDR
3:
	lw	a2, 0(a0)	# ascending check address-in-address
	bne	a1, a2, aina_err

	addiu	a0,a0,4			# increment to next address
	addiu	a1,a1,4			# increment to next address
	bltu	a0,s2,3b		# reach the end ?

	move	v0, zero
	j s7

	/*----------------------------------------------------------------+
	| ~Address-In-Address;  Ascending Stores; Descending Check        |
	+----------------------------------------------------------------*/

	move	a0,s1			# get starting address.
1:
	not	a1, a0
	sw	a1, (a0)		# write ~address-in-address

	addiu	a0,a0,4			# increment to next address
	bltu	a0, s2, 1b		# reach the end ?

	addiu	a0,s2,-4		# get last address
2:
	not	a1, a0
	lw	a2, (a0)		# descending check ~address-in-address
	bne	a1,a2,aina_err

	addiu	a0,a0,-4			# decrement the address.
	bgeu	a0, s1, 2b		# reach the beginning ?

	/*----------------------------------------------------------------+
	| Address-In-Address; Descending Stores;  Ascending Check         |
	+----------------------------------------------------------------*/

	addiu	a0,s2,-4		# get last address
1:
	sw	a0,(a0) 		# write address-in-address

	addiu	a0,a0,-4  		# decrement the address.
	bgeu	a0, s1, 1b		# reach the beginning ?

	move	a0, s1			# get starting address.
2:
	move	a1,a0			# get the expected value
	lw	a2, (a0)		# ascending check address-in-address
	bne	a1, a2, aina_err

	addiu	a0,a0,4			# increment to next address
	bltu	a0,s2, 2b		# reach the end ?

	/*----------------------------------------------------------------+
	|~Address-In-Address; Descending Stores; Descending Check         |
	+----------------------------------------------------------------*/

	addiu	a0,s2,-4		# get last address
1:
	not	a1, a0
	sw	a1, (a0)		# write ~address-in-address

	addiu	a0,a0,-4		# decrement the address.
	bgeu	a0, s1, 1b		# reach the beginning ?

	addiu	a0,s2,-4		# get last address
2:
	not	a1, a0
	lw	a2, (a0)		# ascending check address-in-address
	bne	a1, a2, aina_err

	addiu	a0,a0,-4		# decrement the address.
	bgeu	a0, s1, 2b		# reach the beginning ?

	move	v0, zero
	j	s7			# return to calling routine

aina_err:
	li	a3, 2
	jal	cache_err
	li	v0, 1			# return failure
	j	s7

END(ainatest)

/*
LEAF(rich)
	.set	noreorder
	move	s3,ra
	move	s5, a0
	li	s4, 0x20
1:
	lw	t0, (s5)
	PUTHEX(t0)
	PRINT("\n")
	addi	s4, -1
	bne	s4, zero, 1b
	addu	s5, 4
	j 	s3
	nop
	.set	reorder
END(rich)
LEAF(readitag)
	.set	noreorder
	move	s3, ra
	move	s4, a0
	li	s5, 0x20
1:
	cache   Index_Load_Tag_I,0(s4)
	nop
	mfc0	t1, C0_TAGLO
	nop
	PUTHEX(t1)
	PRINT("\n")
	PUTHEX(s5)
	PRINT("\n")
	addi	s5, -1
	beq	s5, zero, 2f
	addu	s4, 0x10
	b	1b
	nop
2:
	j	s3
	nop
	.set	reorder
END(readitag)
LEAF(readdtag)
	.set	noreorder
	move	s3, ra
	move	s4, a0
	li	s5, 0x20
1:
	cache   Index_Load_Tag_D,0(s4)
	nop
	mfc0	t1, C0_TAGLO
	nop
	PUTHEX(t1)
	PRINT("\n")
	PUTHEX(s5)
	PRINT("\n")
	addi	s5, -1
	beq	s5, zero, 2f
	addu	s4, 0x10
	b	1b
	nop
2:
	j	s3
	nop
	.set	reorder
END(readdtag)
LEAF(readsctag)
	.set	noreorder
	move	s3, ra
	move	s4, a0
	li	s5, 0x20
1:
	cache   Index_Load_Tag_SD,0(s4)
	nop
	mfc0	t1, C0_TAGLO
	nop
	PUTHEX(t1)
	PRINT("\n")
	PUTHEX(s5)
	PRINT("\n")
	addi	s5, -1
	beq	s5, zero, 2f
	addu	s4, 0x10
	b	1b
	nop
2:
	j	s3
	nop
	.set	reorder
END(readsctag)
*/
