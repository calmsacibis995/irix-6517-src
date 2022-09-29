#ident	"IP22diags/pon/pon_addr.s:  $Revision: 1.3 $"

/*
 * pon_addr.s - power-on address uniqueness test
 *
 * This module contains the function pon_addr, which may be called from
 * power-on level 4 code.  It tests the address lines by walking 1's across
 * them
 */

#include "asm.h"
#include "early_regdef.h"
#include "ml.h"

	/* always called early with instructions uncached */
	AUTO_CACHE_BARRIERS_DISABLE

/* 
 * T30 = first address
 * T31 = memory size in bytes
 * T32 = address offset
 */
LEAF(pon_addr)			# address uniqueness test
	move	RA3,ra		# save our return address
 	move	T30,a0		# first address
	move	T31,a1		# size in bytes

#ifdef SEG1_BASE		/* With no hole and 64-bit test everything */
	li	v0,256*1024*1024# cannot test more than 256M
	ble	T31,v0,1f
	move	T31,v0

1:
#endif

	move	v0,zero		# test result
	li	T32,0x4		# start from bit 2 since bits 0/1 are not used
3:
	PTR_ADD	a0,T30,T32
	sw	a0,0(a0)
	sll	T32,1
	blt	T32,T31,3b

	li	T32,0x4		# start from bit 2
4:
	PTR_ADD	a0,T30,T32
	lwu	a2,0(a0)
	and	a0,0xffffffff	# drop high addr bits
	beq	a0,a2,5f
	move	a1,a0
	jal	pon_memerr
	li	v0,1		# test result error, so return 1
5:
	sll	T32,1
	blt	T32,T31,4b

	j	RA3

	END(pon_addr)

	AUTO_CACHE_BARRIERS_ENABLE
