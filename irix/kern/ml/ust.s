#include "ml/ml.h"
#include "sys/mace.h"

#if !EVEREST && !SN0
#define UST_HI(x) 0(x)
#define UST_LO(x) 4(x)
#define UST_LAST_CTR(x) 8(x)
#endif

/*
 * update_ust_i is the same as update_ust but it blocks interrupts.
 * (In many cases interrupts will already be blocked; callers should
 * then call update_ust directly).
 */

USTLOCALSZ=	1
USTFRM=	FRAMESZ((NARGSAVE+USTLOCALSZ)*SZREG)
USTRAOFF=	USTFRM-(1*SZREG)
USTLCLOFF=	USTFRM-(2*SZREG)

#if EVEREST
LEAF(update_ust)
	ld	v0, EV_RTC
	LA	t0, ust_val
	sd	v0, 0(t0)
	j	ra
        END(update_ust)
#elif IP30
LEAF(update_ust)
	CLI	t0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	v0,0(t0)
	LA	t1, ust_val
	sd	v0,0(t1)
	j	ra
	END(update_ust)
#elif IP32
LEAF(update_ust)
	CLI	t0,PHYS_TO_COMPATK1(MACE_UST)
	ld	v0,0(t0)		# get current value of counter
	dsll32	v0,0			# strip off the high order bits
	dsrl32	v0,0			# which may or may not be valid
	LA	t0, ust_val
        lw      v1, UST_LAST_CTR(t0)	# v1 = ust_last_ctr
        lw      t2, UST_LO(t0)		# t2 = ust_lo
        lw      t1, UST_HI(t0)		# t1 = ust_hi
        sw      v0, UST_LAST_CTR(t0)	# store into ust_last_ctr
        subu    ta1, v0, v1		# ta1 = delta since last check
        addu    v0, t2, ta1		# add to low bits
        sltu    ta0, v0, t2		# 1 if low bits overflow
        sw      v0, UST_LO(t0)
        addu    t1, t1, ta0		# add overflow to high bits
        sw      t1, UST_HI(t0)
	j	ra
	END(update_ust)
#elif SN
LEAF(update_ust)
	ld	v0, LOCAL_HUB_ADDR(PI_RT_COUNT)
	LA	t0, ust_val
	sd	v0, 0(t0)
	j	ra
	END(update_ust)
#elif R4000 || R10000 || IP26
NESTED(update_ust,USTFRM,zero)
	PTR_SUBU sp, sp, USTFRM
	REG_S	ra, USTRAOFF(sp)
        jal     get_r4k_counter
	LA	t0, ust_val
        lw      v1, UST_LAST_CTR(t0)	# v1 = ust_last_ctr
        lw      t2, UST_LO(t0)		# t2 = ust_lo
        lw      t1, UST_HI(t0)		# t1 = ust_hi
        sw      v0, UST_LAST_CTR(t0)	# store into ust_last_ctr
        subu    ta1, v0, v1		# ta1 = delta since last check
        addu    v0, t2, ta1		# add to low bits
        sltu    ta0, v0, t2		# 1 if low bits overflow
        sw      v0, UST_LO(t0)
        addu    t1, t1, ta0		# add overflow to high bits
        sw      t1, UST_HI(t0)
	REG_L	ra, USTRAOFF(sp)
	PTR_ADDU sp, sp, USTFRM
	j       ra
        END(update_ust)
#endif

	.sdata
/*
 * DO NOT change the order of these or insert any values in between
 * them. We use offsets from ust_val to reference these values.
 */
EXPORT(ust_val)
#if EVEREST || SN0
	.dword	0
#else
        .word   0		# high bits of UST
        .word   0		# low bits of UST
ust_last_ctr:
        .word   0		# ust_last_ctr
#endif
