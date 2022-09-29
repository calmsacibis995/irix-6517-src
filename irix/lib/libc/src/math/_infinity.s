/* 
 * _infinity.s - copy of __infinity.s retained for compatibility.
 *
 */

#if defined(_LIBCG0) || defined(_PIC)
	.rdata
#else
	.sdata
#endif
	.align 2
	.globl _infinity
_infinity:
	.word 0x7ff00000
	.word 0

