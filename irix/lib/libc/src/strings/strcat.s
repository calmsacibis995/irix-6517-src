/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*  strcat.s 1.1 */

/* This function is an assembly-code replacement for the libc function
 * strcat.
   
 * strcat and strcpy are very similar, but we waste about 40 words of
 * code when both are used, so that they can be independently replaced.

 * There are one caveat to consider: this function is written in
 * assembler code, and as such, cannot be merged using the U-code
 * loader. */

/* Craig Hansen - 3-September-86 */

#include <asm.h>
#include <regdef.h>

/* It turns out better to think of lwl/lwr and swl/swr as
   smaller-vs-bigger address rather than left-vs-right.
   Such a representation makes the code endian-independent. */

#ifdef MIPSEB
#    define LWS lwl
#    define LWB lwr
#    define SWS swl
#    define SWB swr
#else
#    define LWS lwr
#    define LWB lwl
#    define SWS swr
#    define SWB swl
#endif

.text	

LEAF(strcat)
.set noreorder
	## a0/ destination
	## a1/ source
	move	v0, a0		# a copy of destination address is returned
$findz: lb	t0,0(a0)
	nop
	bne	t0,0,$findz
	PTR_ADD	a0,1
	## go back over null byte
	PTR_ADD	a0,-1
	## start up first word
	## adjust pointers so that a0 points to next word
	## ta3 = a1 adjusted by same amount minus one
	## t0,t1,t2,t3 are filled with 4 consecutive bytes
	## ta0 is filled with the same 4 bytes in a single word
	lb	t0, 0(a1)
	ori	ta1, a0, 3	# get an early start
	beq	t0, 0, $doch0
	PTR_SUB	ta2, ta1, a0	# number of char in 1st word of dest - 1
	lb	t1, 1(a1)
	PTR_ADD	ta3, a1, ta2	# offset starting point for source string
	beq	t1, 0, $doch1
	nop
	lb	t2, 2(a1)
	nop
	beq	t2, 0, $doch2
	LWS	ta0, 0(a1)	# safe: always in same word as 0(a1)
	lb	t3, 3(a1)
	LWB	ta0, 3(a1)	# fill out word
	beq	t3, 0, $doch3
	SWS	ta0, 0(a0)	# store entire or part word
	PTR_ADDI a0, ta1, 1-4	# adjust destination ptr

	## inner loop
1:	lb	t0, 1(ta3)
	PTR_ADDI ta3, 4
	beq	t0, 0, $doch0
	PTR_ADDI a0, 4
	lb	t1, 1+1-4(ta3)
	nop
	beq	t1, 0, $doch1
	nop
	lb	t2, 2+1-4(ta3)
	nop
	beq	t2, 0, $doch2
	LWS	ta0, 0+1-4(ta3)
	lb	t3, 3+1-4(ta3)
	LWB	ta0, 3+1-4(ta3)
	bne	t3, 0, 1b
	sw	ta0, 0(a0)
	j	ra
	nop

	## store four bytes using swl/swr
$doch3:	j	ra
	SWB	ta0, 3(a0)
	## store up to three bytes, a byte at a time.
$doch2:	sb	t2, 2(a0)
$doch1:	sb	t1, 1(a0)
$doch0:	j	ra
	sb	t0, 0(a0)

.end strcat
