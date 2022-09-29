/*
 * pon_data.s - walking 0/1's memory test for IP22
 *
 * This module contains the function pon_walkingbit, which may be called from
 * power-on level 4 code.  It touches all of the drams in a bank.
 */

#ident "$Revision: 1.2 $"

#include "asm.h"
#include "early_regdef.h"
#include "pon.h"
#include "ml.h"

/* 
 * a0 = base address of memory bank
 * a1 = memory bank size in bytes
 */

/*
 * T30 = base address in the bank
 * T31 = middle address in the bank, first address in the 2nd subbank
 * T32 = address of the location being tested with the walking 0/1's
 * T33 = walking 1's pattern, xor with T34 to form either walking 0/1's
 * T34 = background value
 * sp = bad SIMM code
 */

#ifdef	LOOP_ON_BAD_MEMORY
#define	LOOP_INSTR	j	bad_memory
#else
#define	LOOP_INSTR
#endif	/* LOOP_ON_BAD_MEMORY */

#define	RETURN_ON_BAD_MEMORY
#ifdef	RETURN_ON_BAD_MEMORY
#define	RETURN_INSTR	j	RA3
#else
#define	RETURN_INSTR
#endif	/* RETURN_ON_BAD_MEMORY */

/* clobber the data lines before reading */
#define	verify_background_before(reg,offset)	\
	lw	zero,-8(RA3);			\
	PTR_ADDU	a0,reg,offset;		\
	lw	a2,0(a0);			\
	beq	a2,T34,1f;			\
	move	a1,T34;				\
	jal	pon_memerr;			\
	LOOP_INSTR;				\
	or	sp,BAD_SIMM0>>(offset>>2);	\
	move	v0,sp;				\
	sw	T34,offset(reg);		\
	RETURN_INSTR;				\
1:

/* clobber the data lines before reading */
#define	verify_background_after(reg,offset)	\
	lw	zero,-8(RA3);			\
	PTR_ADDU	a0,reg,offset;		\
	beq	a0,T32,1f;			\
	lw	a2,0(a0);			\
	beq	a2,T34,1f;			\
	move	a1,T34;				\
	jal	pon_memerr;			\
	LOOP_INSTR;				\
	or	sp,BAD_SIMM0>>(offset>>2);	\
	move	v0,sp;				\
	sw	T34,offset(reg);		\
	RETURN_INSTR;				\
1:


/* walking 0/1 data bus test */
LEAF(pon_walkingbit)	
	AUTO_CACHE_BARRIERS_DISABLE		# uses $sp as scratch
	move	RA3,ra		# save return addr
	move	T30,a0		# save starting addr
	srl	T31,a1,1
	PTR_ADDU	T31,T30	# first addr in second subbank
	move	sp,zero		# clear bad SIMM mask

	/* put a background value on the 8 locations we are going to test */
	move	T34,zero	# first background is all 0'a

background:
	sw	T34,0(T30)	# put in background
	sw	T34,4(T30)
	sw	T34,8(T30)
	sw	T34,12(T30)
	sw	T34,0(T31)
	sw	T34,4(T31)
	sw	T34,8(T31)
	sw	T34,12(T31)

	/* make sure the background is correct */
	verify_background_before(T31,12)
	verify_background_before(T31,8)
	verify_background_before(T31,4)
	verify_background_before(T31,0)
	verify_background_before(T30,12)
	verify_background_before(T30,8)
	verify_background_before(T30,4)
	verify_background_before(T30,0)

	/* walk a 0/1 through the current memory location */
	PTR_ADDU T32,T31,12	# start from the highest of the 8 locations

target:
	li	T33,1		# always walk a 1 in this register

pattern:
	xor	a1,T33,T34	# this generate walking 0/1's
	sw	a1,0(T32)

	verify_background_after(T31,12)
	verify_background_after(T31,8)
	verify_background_after(T31,4)
	verify_background_after(T31,0)
	verify_background_after(T30,12)
	verify_background_after(T30,8)
	verify_background_after(T30,4)
	verify_background_after(T30,0)

	/* check the target location */
	lw	a2,0(T32)
	xor	a1,T33,T34	# regenerate the walking 0/1's pattern
	beq	a2,a1,next_pattern
	move	a0,T32
	jal	pon_memerr
	LOOP_INSTR

	/* update the bad SIMM code if we think there's a data problem */
	srl	v0,T32,2
	and	v0,0x3
	li	v1,BAD_SIMM0
	srl	v1,v1,v0
	or	sp,v1
	move	v0,sp
	RETURN_INSTR

next_pattern:
	sll	T33,1
	bne	T33,zero,pattern

next_target:
	sw	T34,0(T32)
	beq	T32,T30,next_background
	beq	T32,T31,1f
	PTR_SUBU	T32,4
	b	target
1:
	PTR_ADDU	T32,T30,12
	b	target

next_background:
	bne	T34,zero,test_done
	li	T34,0xffffffff		# second background is all 1's
	b	background

test_done:
	move	v0,sp		# return bad SIMM code
	j	RA3

#ifdef	LOOP_ON_BAD_MEMORY
/* loop on sequence of instructions that caused the problem */
bad_memory:
	sw	T34,0(T30)	# write background
	sw	T34,4(T30)
	sw	T34,8(T30)
	sw	T34,12(T30)
	sw	T34,0(T31)
	sw	T34,4(T31)
	sw	T34,8(T31)
	sw	T34,12(T31)

	xor	a1,T33,T34	# this generate either the walking 1 or 0
	sw	a1,0(T32)

	lw	zero,0(T30)	# read background
	lw	zero,4(T30)
	lw	zero,8(T30)
	lw	zero,12(T30)
	lw	zero,0(T31)
	lw	zero,4(T31)
	lw	zero,8(T31)
	lw	zero,12(T31)

	b	bad_memory
#endif	/* LOOP_ON_BAD_MEMORY */
	
	AUTO_CACHE_BARRIERS_ENABLE

	END(pon_walkingbit)
