/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */

#ident "$Id: fp_class.s,v 1.11 1997/10/12 08:29:34 jwag Exp $"

.weakext fp_class_d, _fp_class_d
.weakext fp_class_f, _fp_class_f
#define fp_class_d _fp_class_d
#define fp_class_f _fp_class_f

#include "sys/regdef.h"
#include "sys/asm.h"
#include "fp_class.h"
#include "sys/softfp.h"

/*
 * fp_class_d(d)	
 * double d;
 */

	.text

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

LEAF(fp_class_d)

	# Get the double from f12

	mfc1	a2,$f13
	mfc1	a3,$f12

	# Break down the double into sign, exponent and fraction

	srl	a1,a2,DEXP_SHIFT
	move	a0,a1
	and	a1,DEXP_MASK
	and	a0,SIGNBIT>>DEXP_SHIFT
	and	a2,DFRAC_MASK

	# Check for infinities and Nan's

	bne	a1,DEXP_INF,4f
	bne	a2,zero,2f
	bne	a3,zero,2f
	bne	a0,zero,1f
	li	v0,FP_POS_INF
	j	ra

1:
	li	v0,FP_NEG_INF
	j	ra

2:	# Check to see if this is a signaling NaN

	and	a0,a2,DSNANBIT_MASK
	beq	a0,zero,3f
	li	v0,FP_SNAN
	j	ra

3:	li	v0,FP_QNAN
	j	ra

4:	# Check for zeroes and denorms

	bne	a1,zero,8f
	bne	a2,zero,6f
	bne	a3,zero,6f
	bne	a0,zero,5f
	li	v0,FP_POS_ZERO
	j	ra

5:	li	v0,FP_NEG_ZERO
	j	ra

6:	bne	a0,zero,7f
	li	v0,FP_POS_DENORM
	j	ra

7:	li	v0,FP_NEG_DENORM
	j	ra

8:	# It is just a normal number

	bne	a0,zero,9f
	li	v0,FP_POS_NORM
	j	ra

9:	li	v0,FP_NEG_NORM
	j	ra

END(fp_class_d)

/*
 * fp_class_f(f)	
 * float d;
 */

LEAF(fp_class_f)

	# Get the float from f12

	mfc1	a2,$f12

	# Break down the float into sign, exponent and fraction

	srl	a1,a2,SEXP_SHIFT
	move	a0,a1
	and	a1,SEXP_MASK
	and	a0,SIGNBIT>>SEXP_SHIFT
	and	a2,SFRAC_MASK

	# Check for infinities and Nan's

	bne	a1,SEXP_INF,4f
	bne	a2,zero,2f
	bne	a0,zero,1f
	li	v0,FP_POS_INF
	j	ra

1:
	li	v0,FP_NEG_INF
	j	ra

2:	# Check to see if this is a signaling NaN

	and	a0,a2,SSNANBIT_MASK
	beq	a0,zero,3f
	li	v0,FP_SNAN
	j	ra

3:	li	v0,FP_QNAN
	j	ra

4:
	# Check for zeroes and denorms

	bne	a1,zero,8f
	bne	a2,zero,6f
	bne	a0,zero,5f
	li	v0,FP_POS_ZERO
	j	ra

5:	li	v0,FP_NEG_ZERO
	j	ra

6:	bne	a0,zero,7f
	li	v0,FP_POS_DENORM
	j	ra

7:	li	v0,FP_NEG_DENORM
	j	ra

8:
	# It is just a normal number

	bne	a0,zero,9f
	li	v0,FP_POS_NORM
	j	ra

9:	li	v0,FP_NEG_NORM
	j	ra

END(fp_class_f)

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

LEAF(fp_class_d)

	# Get the double from f12

	dmfc1	a2,$f12

	# Break down the double into sign, exponent and fraction

	dsrl32	a1,a2,20
	and	a1,DEXP_MASK		# a1 = exponent, right justified
	move	a0,a2			# keep a copy of arg
	dsll	a2,12			# a2 = mantissa, left justified

	# Check for infinities and Nan's

	bne	a1,DEXP_INF,4f		# branch if exponent != 0x7ff
	bne	a2,zero,2f		# branch if mantissa != 0
	bltz	a0,1f			# branch if sign != 0
	li	v0,FP_POS_INF
	j	ra

1:
	li	v0,FP_NEG_INF
	j	ra

2:	# Check to see if this is a signaling NaN

	bltz	a2,3f			# branch if arg is a signaling NaN
	li	v0,FP_QNAN
	j	ra

3:	li	v0,FP_SNAN
	j	ra

4:
	# Check for zeroes and denorms

	bne	a1,zero,8f		# branch if exponent != 0
	bne	a2,zero,6f		# branch if mantissa != 0
	bltz	a0,5f			# branch if sign != 0
	li	v0,FP_POS_ZERO
	j	ra

5:	li	v0,FP_NEG_ZERO
	j	ra

6:	bltz	a0,7f			# branch if sign != 0
	li	v0,FP_POS_DENORM
	j	ra

7:	li	v0,FP_NEG_DENORM
	j	ra

8:
	# It is just a normal number

	bltz	a0,9f			# branch if sign != 0
	li	v0,FP_POS_NORM
	j	ra

9:	li	v0,FP_NEG_NORM
	j	ra

END(fp_class_d)

/*
 * fp_class_f(f)	
 * float d;
 */

LEAF(fp_class_f)

	# Get the float from f12

	dmfc1	a2,$f12

	# Break down the float into sign, exponent and fraction

	dsrl	a1,a2,SEXP_SHIFT	# shift exponent into rightmost bits
	and	a1,SEXP_MASK		# zero all but exponent bits
	dsll32	a0,a2,0			# shift sign into leftmost bit of a0
	dsll32	a2,9			# shift mantissa into leftmost bits of a2

	# Check for infinities and Nan's

	bne	a1,SEXP_INF,4f		# branch if exponent != 0xff
	bne	a2,zero,2f		# branch if mantissa != 0
	bltz	a0,1f			# branch if sign bit != 0
	li	v0,FP_POS_INF
	j	ra

1:
	li	v0,FP_NEG_INF
	j	ra

2:	# Check to see if this is a signaling NaN

	bltz	a2,3f			# branch if signaling nan bit is set
	li	v0,FP_QNAN
	j	ra

3:	li	v0,FP_SNAN
	j	ra

4:
	# Check for zeroes and denorms

	bne	a1,zero,8f		# branch if exponent != 0
	bne	a2,zero,6f		# branch if mantissa != 0
	bltz	a0,5f			# branch if sign != 0
	li	v0,FP_POS_ZERO
	j	ra

5:	li	v0,FP_NEG_ZERO
	j	ra

6:	bltz	a0,7f			# branch if sign != 0
	li	v0,FP_POS_DENORM
	j	ra

7:	li	v0,FP_NEG_DENORM
	j	ra

8:
	# It is just a normal number

	bltz	a0,9f			# branch if sign != 0
	li	v0,FP_POS_NORM
	j	ra

9:	li	v0,FP_NEG_NORM
	j	ra

END(fp_class_f)

#endif
