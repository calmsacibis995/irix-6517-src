#include "ml.h"
#include <sys/sbd.h>
#include <regdef.h>
#include <asm.h>

/* int callarcsprom(param, func) */
#define ARCS_BUF_SIZE	6*SZREG
	.comm	CallArcsPromBuf,	ARCS_BUF_SIZE
LEAF(CallArcsProm)
	.set reorder
	LA	v1,CallArcsPromBuf
	PTR_S	ra,SZREG(v1)
	PTR_S	sp,2*SZREG(v1)
	PTR_S	a0,3*SZREG(v1)
	PTR_S	a1,4*SZREG(v1)
	LA	ra,2f
#if _K64PROM32
	# change sp, pc, and ra so that they are all in 32-bit K1 space
	jal	flush_cache
	LA	v1,CallArcsPromBuf
	PTR_L	a0,3*SZREG(v1)
	PTR_L	a1,4*SZREG(v1)

	LA	v1,1f
	or	v1,COMPAT_K1BASE
	j	v1
1:
	LA	ra,2f
	or	ra,COMPAT_K1BASE
	or	sp,COMPAT_K1BASE
	or	a1,COMPAT_K1BASE
	#now go 32-bit
	.set noreorder
	MFC0	(v0,C0_SR)
        NOP_1_4
	and	v0,~SR_KX
        NOP_1_2
	MTC0	(v0,C0_SR)
        NOP_1_4
#endif
	.set reorder
	j	a1

2:
#if _K64PROM32
	#go back to 64-bit
	.set noreorder
	MFC0	(v1,C0_SR)
	NOP_1_4
	ori	v1,SR_KX
	NOP_1_2
	MTC0	(v1,C0_SR)
	NOP_1_4
	.set reorder
#endif
	# restore pc and sp to 64-bit
	LA	v1,CallArcsPromBuf
	PTR_L	ra,SZREG(v1)
	PTR_L	sp,2*SZREG(v1)
	j	ra
	# pc now restored to 64-bit address
	END(CallArcsProm)
