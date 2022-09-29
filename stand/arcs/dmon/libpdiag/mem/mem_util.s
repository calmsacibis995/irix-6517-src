#ident	"$Id: mem_util.s,v 1.1 1994/07/21 00:58:52 davidl Exp $"
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

#include "pdiag.h"
#include "monitor.h"


/*
 * WriteMemW(u_init address, u_int word_count, u_int pattern)
 *
 *	Write the memory at the address with pattern.
 *
 * register usage:
 *	a0 - input address		a1 - word count
 *	a2 - pattern to write
 */
LEAF(WriteMemW)

	and	t7,a0,0x03		# check word boundary
	bne	t7,zero,11f

	sll	t7,a1,2			# get byte offset
	addu	a1,a0,t7		# get last address + 1
1:
	bgeu	a0,a1,done		# reach the last word yet?
	sw	a2,(a0)			# store pattern into the location
	addiu	a0,4			# next address
	b	1b
2:
	move	v0,zero
	j	ra
11:
	PRINT("\nAddress not at word boundary")
	li	v0,1
	j	ra

END(WriteMemW)



/*
 * CheckMemW(u_init address, u_int word_count, u_int pattern)
 *
 *	Check the memory at address with pattern.
 *
 * register usage:
 *	a0 - input address		a1 - word count
 *	a2 - pattern to compare
 */
LEAF(CheckMemW)

	and	t7,a0,0x03		# check word boundary
	bne	t7,zero,11f

	sll	t7,a1,2			# get byte offset
	addu	a1,a0,t7		# get last address + 1
1:
	bgeu	a0,a1,done		# reach the last word yet?
	lw	t7,(a0)			# read the word
	bne	a2,t7,12f		# branch if not compare
	addiu	a0,4			# next address
	b	1b
done:
	move	v0,zero
	j	ra
11:
	PRINT("\nAddress not at word boundary")
12:
	move	a1,a2			# expected value for error routine
	move	a2,t7			# actual value for error display
	li	v0,1
	j	ra

END(CheckMemW)



