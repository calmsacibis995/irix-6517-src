/* 
 * __infinity.s - libc module which contains read-only double-precision 
 *		 constant whose value is +Infinity.  This is needed for
 *		 the macro HUGE_VAL (limits.h, math.h) to work.
 *
 */

#if defined(_LIBCG0) || defined(_PIC)
	.rdata
#else
	.sdata
#endif
	.align 2
	.globl __infinity
__infinity:
	.word 0x7ff00000
	.word 0

