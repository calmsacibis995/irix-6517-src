/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#ident "$Revision: 1.2 $"

#include <regdef.h>
#include <sys/asm.h>

	PICOPT

LOCALSZ=	5		# save gp, ra
FRAMESZ=	(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK
RAOFF=		FRAMESZ-(1*SZREG)
GPOFF=		FRAMESZ-(2*SZREG)

.text

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

	/* perform 64-bit integer multiply, and store 128 bits of result */

 #	void
 #	__dwmultu(__uint64_t z[2],	/* 128b product */
 #		 __uint64_t y,		/* 64b multiplier */
 #		 __uint64_t x)		/* 64b multiplier */

	.globl  __dwmultu
	.ent	__dwmultu

__dwmultu:
	.frame	sp,0,ra
	lw	ta1, 16(sp)	# fetch x1 = HI(x)
	lw	ta0, 20(sp)	# fetch x0 = LO(x)
	multu	ta0, a3		# x0 * y0
	## 10 cycle interlock
	mflo	t0		# lo(x0 * y0)
	mfhi	t1		# hi(x0 * y0)
	not	ta2, t1
	## 1 nop
	multu	ta1, a3		# x1 * y0
	## 10 cycle interlock
	mflo	ta3		# lo(x1 * y0)
	mfhi	t2		# hi(x1 * y0)
	sltu	ta2, ta2, ta3	# carry(hi(x0 * y0) + lo(x1 * y0))
	addu	t1, ta3		# hi(x0 * y0) + lo(x1 * y0)
	multu	ta0, a2		# x0 * y1
	addu	t2, ta2		# hi(x1 * y0) + carry
	not	ta2, t1
	## 8 cycle interlock
	mflo	ta3		# lo(x0 * y1)
	mfhi	t3		# hi(x0 * y1)
	sltu	ta2, ta2, ta3	# carry((hi(x0 * y0) + lo(x1 * y0)) + lo(x0 * y1))
	addu	t1, ta3		# hi(x0 * y0) + lo(x1 * y0) + lo(x0 * y1)
	multu	ta1, a2		# x1 * y1
	addu	t2, ta2		# hi(x1 * y0) + carry + carry
	not	ta3, t2
	sltu	ta3, ta3, t3	# carry(hi(x1 * y0) + hi(x0 * y1))
	addu	t2, t3		# hi(x1 * y0) + hi(x0 * y1))
	not	ta2, t2
	## 5 cycle interlock
	mfhi	t3		# hi(x1 * y1)
	addu	t3, ta3		# hi(x1 * y1) + carry(hi(x1 * y0) + hi(x0 * y1))
	mflo	ta3		# lo(x1 * y1)
	sltu	ta2, ta2, ta3	# carry((hi(x1 * y0) + hi(x0 * y1)) + lo(x1 * y1))
	addu	t3, ta2
	addu	t2, ta3		# hi(x1 * y0) + hi(x0 * y1) + lo(x1 * y1)
	sw	t3, 0(a0)	# store z
	sw	t2, 4(a0)
	sw	t1, 8(a0)
	sw	t0, 12(a0)
	j	ra
	.end 	__dwmultu

#define RMSW v0
#define AMSW t0
#define ALSW t1
#define BLSW t2
#define	DEXP t3
#define	ND t3
#define PBUFF t6


 #	typedef  union {
 #		__uint32_t	i[2];
 #		__uint64_t	ll;
 #	} _ullval;

 #	void _get_first_digit(&frac.i[0], &frac.i[1], &dexp)
 #	_ullval	frac;
 #	int	dexp;
 #  
 #
 #	/* find first non-zero digit */
 #
 #	while( !(frac.i[0] >> 28) )
 #	{
 #		frac.ll = frac.ll * 10;
 #		dexp--;
 #	}

.globl _get_first_digit
.ent _get_first_digit

_get_first_digit:
	.frame	sp, 0, ra
	lw	AMSW, 0(a0)	# fetch frac, msw
	lw	ALSW, 0(a1)	# fetch frac, lsw
	lw	DEXP, 0(a2)	# fetch dexp
	li	BLSW, 10	# set b = 10
	srl	t4, AMSW, 28
	bne	t4, 0, ret	# branch if letfmost 4 bits of frac != 0
top:
	multu	ALSW, BLSW	# multiply frac by 10
	mflo	ALSW
	mfhi	RMSW
	multu	AMSW, BLSW
	mflo	t4
	addu	AMSW, RMSW, t4

	addiu	DEXP, -1	# decrement dexp
	srl	t4, AMSW, 28
	beq	t4, 0, top	# branch if letfmost 4 bits of frac == 0

	sw	AMSW, 0(a0)	# store frac, msw
	sw	ALSW, 0(a1)	# store frac, lsw
	sw	DEXP, 0(a2)	# store dexp
ret:
	j	ra
.end _get_first_digit

 #	void _get_digits(&p_buffer, &frac.i[0], &frac.i[1], &nd)
 # 	char	*p_buffer;
 # 	_ullval	frac;
 # 	int	nd;
 #
 #
 #	/* produce digits - always at least one */
 #
 #	do
 #	{
 #		*buffer++ = (char)('0' + (frac.i[0]>>28));
 #		frac.i[0] &= (1<<28) - 1;
 #		frac.ll = frac.ll * 10;
 #	} while ( --nd > 0 );

.globl _get_digits
.ent _get_digits

_get_digits:
	.frame	sp, 0, ra
	lw	PBUFF, 0(a0)	# load pointer to buffer
	lw	AMSW, 0(a1)	# load frac, msw
	lw	ALSW, 0(a2)	# load frac, lsw
	lw	ND, 0(a3)	# load nd
	li	BLSW, 10	# set b = 10
	li	t4, "0"		# set t5 = '0'
top2:
	srl	t5, AMSW, 28
	addu	t5, t4
	sb	t5, 0(PBUFF)	# *p_buffer = (frac.i[0] >> 28) + '0'
	addiu	PBUFF, 1	# p_buffer++
	sll	AMSW, 4		# shift off leftmost 4 bits of frac
	srl	AMSW, 4
	multu	ALSW, BLSW	# multiply frac by 10
	mflo	ALSW
	mfhi	RMSW
	multu	AMSW, BLSW
	mflo	t5
	addu	AMSW, RMSW, t5

	addiu	ND, -1		# decrement nd
	bgtz	ND, top2	# branch if nd > 0 

ret2:
	sw	PBUFF, 0(a0)	# update pointer to buffer
	sw	AMSW, 0(a1)	# store frac, msw
	sw	ALSW, 0(a2)	# store frac, lsw
	sw	ND, 0(a3)	# store nd
	j	ra
.end _get_digits

 #  	void _gen_digits(&buffer, &frac.ll, &nd)
 #  	char	*buffer;
 #  	_ullval	frac;
 #  	int	nd;
 #
 #
 #	do
 #	{
 #		if ( frac.ll == 0L )
 #			break;
 #
 #		frac.ll = frac.ll * 10; /* generate digit */
 #		*buffer++ = (char)('0' + (frac.ll>>60)); /* store it */
 #		frac.ll &= (1ULL<<60) - 1; /* zero first 4 bits */
 #
 #	} while ( --nd > 0 )
 

.globl _gen_digits
.ent _gen_digits
_gen_digits:
	.frame	sp, 0, ra
	lw	PBUFF, 0(a0)	# load pointer to buffer
	lw	AMSW, 0(a1)	# load frac, msw
	lw	ALSW, 4(a1)	# load frac, lsw
	lw	ND, 0(a2)	# load nd
	li	BLSW, 10	# set b = 10
	li	t4, "0"		# set t4 = '0'
top3:
	or	t5, AMSW, ALSW
	beq	t5, 0, ret3	# branch if frac.ll == 0L
	multu	ALSW, BLSW	# multiply frac by 10
	mflo	ALSW
	mfhi	RMSW
	multu	AMSW, BLSW
	mflo	t5
	addu	AMSW, RMSW, t5

	srl	t5, AMSW, 28
	addu	t5, t4
	sb	t5, 0(PBUFF)	# *buffer = (frac.i[0] >> 28) + '0'
	addiu	PBUFF, 1	# increment p_buffer
	sll	AMSW, 4		# clear leftmost 4 bits of frac
	srl	AMSW, 4
	addiu	ND, -1		# decrement nd
	bgtz	ND, top3	# branch if nd > 0

ret3:
	sw	PBUFF, 0(a0)	# update pointer to buffer
	sw	AMSW, 0(a1)	# store frac, msw
	sw	ALSW, 4(a1)	# store frac, lsw
	sw	ND, 0(a2)	# store nd
	j	ra
.end _gen_digits

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

	/* perform 64-bit integer multiply, and store 128 bits of result */
	/* This replaces dwmultuc.c for mips3/4 targets */

 #	void
 #	__dwmultu(__uint64_t z[2],     /* 128b product */
 #		  __uint64_t y,        /* 64b multiplier */
 #		  __uint64_t x)        /* 64b multiplier */

	.text
LEAF(__dwmultu)
	dmultu a1, a2
	mflo a1
	mfhi a2
	sd a1, 8(a0)
	sd a2, 0(a0)
	j	ra
END(__dwmultu)

#endif

