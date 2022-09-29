/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/* $Header: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/qwmultu.s,v 1.1 1993/10/21 23:15:03 vegas Exp $ */

#include <regdef.h>
#include <sys/asm.h>

/* Multiply 128-bit integers in X and Y to produced 256-bit product Z */

/* Calling sequence:
 * 
 * 	_qwmultu(unsigned long Z[8], 
 *		 unsigned long X[4], 
 *		 unsigned long Y[4])
 */

/* Multiplication scheme 
 * 	Hmn means hi( Xm * Yn), similarly with Lmn and lo.
 * 	Xm = X[m], similarly for Y and Z.
 *
 *					X0	X1	X2	X3
 *					Y0	Y1	Y2	Y3
 *					---------------------------
 *							H33	L33
 *						H23	L23
 *					H13	L13
 *				H03	L03
 *						H32	L32
 *					H22	L22
 *				H12	L12
 *			H02	L02
 *					H31	L31
 *				H21	L21
 *			H11	L11
 *		H01	L01
 *				H30	L30
 *			H20	L20
 *		H10	L10
 *	H00	L00
 *	-----------------------------------------------------------
 *	Z0	Z1	Z2	Z3	Z4	Z5	Z6	Z7
 *
 * All of the carries for Zn are accumulated in Zn-1. Computation
 * proceeds column by column from right to left.
 */

/* Register use: 
 * 
 *  $2	X3, X2, X1, X0
 *  $3	Y3, Y2, Y1, Y0
 *  $4	Z
 *  $5	X
 *  $6	Y
 *  $7	Z7, Z5, Z3, Z1
 *  $8	Z6, Z4, Z2, Z0
 *  $9	Lmn
 * $10	cy
 * $11	H23,H12,H01	The assignment of registers for Hmn is 
 * $12	H32,H21,H10	round robin in order of appearance in
 * $13	H13,H30,H00	the code. It only looks random. :-)
 * $14	H22,H02
 * $15	H31,H11
 * $24	H03,H20
 * $25	available, if needed
 */

#define	Z	$4	/* Z address */
#define Z0	$8	/* Most significant word of Z product */
#define Z1	$7
#define Z2	$8
#define Z3	$7
#define Z4	$8
#define Z5	$7
#define Z6	$8
#define Z7	$7	/* Least significant word of Z product */

#define	X	$5	/* X address */
#define X0	$2	/* Most significant word of X multiplier */
#define X1	$2
#define X2	$2
#define X3	$2	/* Least significant word of X multiplier */

#define Y	$6	/* Y address */
#define Y0	$3	/* Most significant word of Y multiplier */
#define Y1	$3
#define Y2	$3
#define Y3	$3	/* Least significant word of Y multiplier */

/* L33 and H33 aren't needed */
#define L32	$9	/* lo( X3 * Y2 ) */
#define H32	$12	/* hi( X3 * Y2 ) */
#define L31	$9	/* lo( X3 * Y1 ) */
#define H31	$15	/* hi( X3 * Y1 ) */
#define L30	$9	/* lo( X3 * Y0 ) */
#define H30	$13	/* hi( X3 * Y0 ) */
#define L23	$9	/* lo( X2 * Y3 ) */
#define H23	$11	/* hi( X2 * Y3 ) */
#define L22	$9	/* lo( X2 * Y2 ) */
#define H22	$14	/* hi( X2 * Y2 ) */
#define L21	$9	/* lo( X2 * Y1 ) */
#define H21	$12	/* hi( X2 * Y1 ) */
#define L20	$9	/* lo( X2 * Y0 ) */
#define H20	$24	/* hi( X2 * Y0 ) */
#define L13	$9	/* lo( X1 * Y3 ) */
#define H13	$13	/* hi( X1 * Y3 ) */
#define L12	$9	/* lo( X1 * Y2 ) */
#define H12	$11	/* hi( X1 * Y2 ) */
#define L11	$9	/* lo( X1 * Y1 ) */
#define H11	$15	/* hi( X1 * Y1 ) */
#define L10	$9	/* lo( X1 * Y0 ) */
#define H10	$12	/* hi( X1 * Y0 ) */
#define L03	$9	/* lo( X0 * Y3 ) */
#define H03	$24	/* hi( X0 * Y3 ) */
#define L02	$9	/* lo( X0 * Y2 ) */
#define H02	$14	/* hi( X0 * Y2 ) */
#define L01	$9	/* lo( X0 * Y1 ) */
#define H01	$11	/* hi( X0 * Y1 ) */
#define L00	$9	/* lo( X0 * Y0 ) */
#define H00	$13	/* hi( X0 * Y0 ) */

#define cy	$10	/* carry */

/* Timing analysis:
 *  
 * The timing is largely determined by the following sequence of
 * instructions.
 * 
 *					(previous mflo/mfhi)
 *	instructions before multu 	(minimum of 2 cycles needed)
 *	multu
 *	instructions after multu 	(these together
 *	interlock cycles		are at least 10 cycles)
 *	mflo
 *	mfhi
 * 
 * There are 16 such sequences of instructions:
 *                                                                 total
 * 		   Cycles
 * before          3  2  2  2  2  2  2  2  2  2  4  2  2  4  2  2   37
 * multu           1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1   16
 * instructions    1  2  3  8  8  4  8  8  8  4 10  8  8 10  7 10  107
 * interlock       9  8  7  2  2  6  2  2  2  6  0  2  2  0  3  0   53
 * mflo            1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1   16
 * mfhi            1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1   16
 * after                                                        7    7
 * 
 * total          16 15 15 15 15 15 15 15 15 15 17 15 15 17 15 22  252
 * 
 * 	16 multiplies * 15 cycles                                  240
 * 
 * overhead                                                         12
 * percent overhead                                                4.8%
 */

LEAF(_qwmultu)
	.frame	sp, 0, ra
	lw	X3,12(X)
	lw	Y3,12(Y)
	## nop
	multu	X3,Y3
	lw	X2,8(X)
	## 9 cycle interlock
	mflo	Z7		# Z7 = L33
	mfhi	Z6		# Z6 = H33
	sw	Z7,28(Z)
	not	Z5,Z6		# Z5 = carry(Z6+L23)
/* -------------------- end of column 7 */
	multu	X2,Y3
	lw	X3,12(X)
	lw	Y2,8(Y)
	## 8 cycle interlock
	mflo	L23
	mfhi	H23
	sltu	Z5,Z5,L23
	addu	Z6,L23

	multu	X3,Y2
	lw	X1,4(X)
	lw	Y3,12(Y)
	not	cy,Z6		# cy = carry(Z6+L32)
	## 7 cycle interlock
	mflo	L32
	mfhi	H32
	sltu	cy,cy,L32
	add	Z5,cy

	multu	X1,Y3
	addu	Z6,L32
	sw	Z6,24(Z)
/* -------------------- end of column 6 */
	not	Z4,Z5		# Z4 = carry(Z5+H23)
	sltu	Z4,Z4,H23
	addu	Z5,H23

	lw	X2,8(X)
	lw	Y2,8(Y)
	not	cy,Z5		# cy = carry(Z5+L13)
	## 2 cycle interlock
	mflo	L13
	mfhi	H13
	sltu	cy,cy,L13
	add	Z4,cy

	multu	X2,Y2
	addu	Z5,L13

	not	cy,Z5		# cy = carry(Z5+H32)
	sltu	cy,cy,H32
	add	Z4,cy
	addu	Z5,H32

	lw	X3,12(X)
	lw	Y1,4(Y)
	not	cy,Z5		# cy = carry(Z5+L22)
	## 2 cycle interlock
	mflo	L22
	mfhi	H22
	sltu	cy,cy,L22
	add	Z4,cy

	multu	X3,Y1
	addu	Z5,L22

	lw	X0,0(X)
	lw	Y3,12(Y)
	not	cy,Z5		# cy = carry(Z5+L31)
	## 6 cycle interlock
	mflo	L31
	mfhi	H31
	sltu	cy,cy,L31
	add	Z4,cy

	multu	X0,Y3
	addu	Z5,L31
	sw	Z5,20(Z)
/* -------------------- end of column 5 */
	not	Z3,Z4		# Z3 = carry(Z4+H13)
	sltu	Z3,Z3,H13
	addu	Z4,H13

	lw	X1,4(X)
	lw	Y2,8(Y)
	not	cy,Z4		# cy = carry(Z4+L03)
	## 2 cycle interlock
	mflo	L03
	mfhi	H03
	sltu	cy,cy,L03
	add	Z3,cy

	multu	X1,Y2
	addu	Z4,L03

	not	cy,Z4		# cy = carry(Z4+H22)
	sltu	cy,cy,H22
	add	Z3,cy
	addu	Z4,H22

	lw	X2,8(X)
	lw	Y1,4(Y)
	not	cy,Z4		# cy = carry(Z4+L12)
	## 2 cycle interlock
	mflo	L12
	mfhi	H12
	sltu	cy,cy,L12
	add	Z3,cy

	multu	X2,Y1
	addu	Z4,L12

	not	cy,Z4		# cy = carry(Z4+H31)
	sltu	cy,cy,H31
	add	Z3,cy
	addu	Z4,H31

	lw	X3,12(X)
	lw	Y0,0(Y)
	not	cy,Z4		# cy = carry(Z4+L21)
	## 2 cycle interlock
	mflo	L21
	mfhi	H21
	sltu	cy,cy,L21
	add	Z3,cy

	multu	X3,Y0
	addu	Z4,L21

	lw	X0,0(X)
	lw	Y2,8(Y)
	not	cy,Z4		# cy = carry(Z4+L30)
	## 6 cycle interlock
	mflo	L30
	mfhi	H30
	sltu	cy,cy,L30
	add	Z3,cy
	addu	Z4,L30
	sw	Z4,16(Z)
/* -------------------- end of column 4 */
	multu	X0,Y2
	not	Z2,Z3		# Z2 = carry(Z3+H03)
	sltu	Z2,Z2,H03
	addu	Z3,H03

	not	cy,Z3		# cy = carry(Z3+H12)
	sltu	cy,cy,H12
	add	Z2,cy
	addu	Z3,H12

	lw	X1,4(X)
	lw	Y1,4(Y)
	not	cy,Z3		# cy = carry(Z3+L02)
	## 0 cycle interlock
	mflo	L02
	mfhi	H02
	sltu	cy,cy,L02
	add	Z2,cy

	multu	X1,Y1
	addu	Z3,L02

	not	cy,Z3		# cy = carry(Z3+H21)
	sltu	cy,cy,H21
	add	Z2,cy
	addu	Z3,H21

	lw	X2,8(X)
	lw	Y0,0(Y)
	not	cy,Z3		# cy = carry(Z3+L11)
	## 2 cycle interlock
	mflo	L11
	mfhi	H11
	sltu	cy,cy,L11
	add	Z2,cy

	multu	X2,Y0
	addu	Z3,L11

	not	cy,Z3		# cy = carry(Z3+H30)
	sltu	cy,cy,H30
	add	Z2,cy
	addu	Z3,H30

	lw	X0,0(X)
	lw	Y1,4(Y)
	not	cy,Z3		# cy = carry(Z3+L20)
	## 2 cycle interlock
	mflo	L20
	mfhi	H20
	sltu	cy,cy,L20
	add	Z2,cy

	addu	Z3,L20
	sw	Z3,12(Z)
/* -------------------- end of column 3 */
	multu	X0,Y1
	not	Z1,Z2		# Z1 = carry(Z2+H02)
	sltu	Z1,Z1,H02
	addu	Z2,H02

	not	cy,Z2		# cy = carry(Z2+H11)
	sltu	cy,cy,H11
	add	Z1,cy
	addu	Z2,H11

	lw	X1,4(X)
	lw	Y0,0(Y)
	not	cy,Z2		# cy = carry(Z2+L01)
	## 0 cycle interlock
	mflo	L01
	mfhi	H01
	sltu	cy,cy,L01
	add	Z1,cy

	multu	X1,Y0
	addu	Z2,L01

	not	cy,Z2		# cy = carry(Z2+H20)
	sltu	cy,cy,H20
	add	Z1,cy
	addu	Z2,H20

	lw	X0,0(X)
	not	cy,Z2		# cy = carry(Z2+L10)
	## 3 cycle interlock
	mflo	L10
	mfhi	H10
	sltu	cy,cy,L10
	add	Z1,cy

	multu	X0,Y0
	addu	Z2,L10
	sw	Z2,8(Z)
/* -------------------- end of column 2 */
	not	Z0,Z1		# Z0 = carry(Z1+H01)
	sltu	Z0,Z0,H01
	addu	Z1,H01

	not	cy,Z1		# cy = carry(Z1+H10)
	sltu	cy,cy,H10
	add	Z0,cy
	addu	Z1,H10

	not	cy,Z1		# cy = carry(Z1+L00)
	## 0 cycle interlock
	mflo	L00
	mfhi	H00
	sltu	cy,cy,L00
	add	Z0,cy
	addu	Z1,L00
	sw	Z1,4(Z)
/* -------------------- end of column 1 */
	addu	Z0,H00
	sw	Z0,0(Z)
/* -------------------- end of column 0 */
	j	ra
	END(_qwmultu)
