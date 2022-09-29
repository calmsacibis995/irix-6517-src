#include <regdef.h>

#ifdef _MIPSEL
#	define RLSW	v0
#	define RMSW	v1
#	define ALSW	a0
#	define AMSW	a1
#	define BLSW	a2
#	define BMSW	a3
#else /* _MIPSEB */
#	define RMSW	v0
#	define RLSW	v1
#	define AMSW	a0
#	define ALSW	a1
#	define BMSW	a2
#	define BLSW	a3
#endif

.text

/* long long __ll_mul(long long a, long long b) */

.globl __ll_mul
.ent __ll_mul
__ll_mul:
	.frame	sp, 0, ra
	sw	AMSW, 0(sp)
	sw	ALSW, 4(sp)
	sw	BMSW, 8(sp)
	sw	BLSW, 12(sp)
	ld	t7, 8(sp)
	ld	t6, 0(sp)
	dmultu	t6, t7
	mflo	RMSW
	dsll	RLSW, RMSW, 32
	dsra	RLSW, RLSW, 32
	dsra	RMSW, RMSW, 32
	j	ra
.end __ll_mul

/* long long __ll_div(long long a, long long b) */

.globl __ll_div
.ent __ll_div
__ll_div:
	.frame	sp, 0, ra
	sw	AMSW, 0(sp)
	sw	ALSW, 4(sp)
	sw	BMSW, 8(sp)
	sw	BLSW, 12(sp)
	ld	t7, 8(sp)
	ld	t6, 0(sp)
	ddiv	RMSW, t6, t7
	dsll	RLSW, RMSW, 32
	dsra	RLSW, RLSW, 32
	dsra	RMSW, RMSW, 32
	j	ra
.end __ll_div

/* unsigned long long __ull_div(unsigned long long a, unsigned long long b) */

.globl __ull_div
.ent __ull_div
__ull_div:
	.frame	sp, 0, ra
	sw	AMSW, 0(sp)
	sw	ALSW, 4(sp)
	sw	BMSW, 8(sp)
	sw	BLSW, 12(sp)
	ld	t7, 8(sp)
	ld	t6, 0(sp)
	ddivu	RMSW, t6, t7
	dsll	RLSW, RMSW, 32
	dsra	RLSW, RLSW, 32
	dsra	RMSW, RMSW, 32
	j	ra
.end __ull_div

/* long long __ll_rem(long long a, long long b) */

.globl __ll_rem
.ent __ll_rem
__ll_rem:
	.frame	sp, 0, ra
	sw	AMSW, 0(sp)
	sw	ALSW, 4(sp)
	sw	BMSW, 8(sp)
	sw	BLSW, 12(sp)
	ld	t7, 8(sp)
	ld	t6, 0(sp)
	drem	RMSW, t6, t7
	dsll	RLSW, RMSW, 32
	dsra	RLSW, RLSW, 32
	dsra	RMSW, RMSW, 32
	j	ra
.end __ll_rem

/* unsigned long long __ull_rem(unsigned long long a, unsigned long long b) */

.globl __ull_rem
.ent __ull_rem
__ull_rem:
	.frame	sp, 0, ra
	sw	AMSW, 0(sp)
	sw	ALSW, 4(sp)
	sw	BMSW, 8(sp)
	sw	BLSW, 12(sp)
	ld	t7, 8(sp)
	ld	t6, 0(sp)
	dremu	RMSW, t6, t7
	dsll	RLSW, RMSW, 32
	dsra	RLSW, RLSW, 32
	dsra	RMSW, RMSW, 32
	j	ra
.end __ull_rem

/* long long __ll_lshift(long long a, long long b) */

.globl __ll_lshift
.ent __ll_lshift
__ll_lshift:
	.frame	sp, 0, ra
	sw	AMSW, 0(sp)
	sw	ALSW, 4(sp)
	sw	BMSW, 8(sp)
	sw	BLSW, 12(sp)
	ld	t7, 8(sp)
	ld	t6, 0(sp)
	mfhi	zero
	dsll	RMSW, t6, t7
	dsll	RLSW, RMSW, 32
	dsra	RLSW, RLSW, 32
	dsra	RMSW, RMSW, 32
	j	ra
.end __ll_lshift

/* long long __ll_rshift(long long a, long long b) */

.globl __ll_rshift
.ent __ll_rshift
__ll_rshift:
	.frame	sp, 0, ra
	sw	AMSW, 0(sp)
	sw	ALSW, 4(sp)
	sw	BMSW, 8(sp)
	sw	BLSW, 12(sp)
	ld	t7, 8(sp)
	ld	t6, 0(sp)
	mfhi	zero
	dsra	RMSW, t6, t7
	dsll	RLSW, RMSW, 32
	dsra	RLSW, RLSW, 32
	dsra	RMSW, RMSW, 32
	j	ra
.end __ll_rshift

/* unsigned long long __ull_rshift(unsigned long long a, unsigned long long b) */

.globl __ull_rshift
.ent __ull_rshift
__ull_rshift:
	.frame	sp, 0, ra
	sw	AMSW, 0(sp)
	sw	ALSW, 4(sp)
	sw	BMSW, 8(sp)
	sw	BLSW, 12(sp)
	ld	t7, 8(sp)
	ld	t6, 0(sp)
	mfhi	zero
	dsrl	RMSW, t6, t7
	dsll	RLSW, RMSW, 32
	dsra	RLSW, RLSW, 32
	dsra	RMSW, RMSW, 32
	j	ra
.end __ull_rshift
