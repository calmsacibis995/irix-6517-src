#include "t5.h"

#define	HEART_PIU_BASE	0xFFFFFFFFAFF00000
#define	HEART_IMR_BASE	0xFFFFFFFFAFF10000
#define	HEART_REG_BASE	0xFFFFFFFFB8000000
#define	BRIDGE_REG_BASE	0xFFFFFFFFBF000000

#define	IRIX		0xA80000002006E4D0

        .text
        .set noat
        .set noreorder

	.ent	Boot
	.globl	Boot
Boot:				# Address = 0xbfc00000.

/* configure T5 and jump to cacheable space */

	dla	t1,1f
	li   	t4, CA_CachNCoh
	j	t1
	  mtc0	t4, C0_Config
1:
	dla	t2, IRIX

	li	t4, 0x80
	mtc0	t4, C0_SR
	
	/* There must be a delay of at least 100 clocks after */
	/* each write to HeartRegBase reg */
	dla	t3, HEART_PIU_BASE
	
	dli	t4, 0x0920000000020800
	sd	t4, 0x000(t3)			/* heart_mode */
	
	/* hang out for a bit */
	mfc0	t5, C0_Count
	addiu	t6, t5, 100
2:	mfc0	t5, C0_Count
	ble 	t5, t6, 2b
	nop

	dli	t4, 0x0000000000000021
	sd	t4, 0x008(t3)			/* heart_sdram_mode */
	
	/* hang out for a bit */
	mfc0	t5, C0_Count
	addiu	t6, t5, 100
2:	mfc0	t5, C0_Count
	ble 	t5, t6, 2b
	nop

	dli	t4, 0x0000000000010021
	sd	t4, 0x008(t3)			/* heart_sdram_mode */
	
	/* hang out for a bit */
	mfc0	t5, C0_Count
	addiu	t6, t5, 100
2:	mfc0	t5, C0_Count
	ble 	t5, t6, 2b
	nop

	dli	t4, 0x8000000000000000
	sd	t4, 0x020(t3)			/* heart_memcfg(0) */
	
	/* hang out for a bit */
	mfc0	t5, C0_Count
	addiu	t6, t5, 100
2:	mfc0	t5, C0_Count
	ble 	t5, t6, 2b
	nop


	dla	t3, HEART_IMR_BASE
	dli	t4, 0x8FF8000000000000
	sd	t4, 0x000(t3)			/* heart_imr(0) */
	dli	t4, 0x1000000000000000
	sd	t4, 0x008(t3)			/* heart_imr(1) */
	dli	t4, 0x2000000000000000
	sd	t4, 0x010(t3)			/* heart_imr(2) */
	dli	t4, 0x4000000000000000
	sd	t4, 0x018(t3)			/* heart_imr(3) */

	dla	t3, HEART_REG_BASE
	dli	t4, 0xFFFFFFFFFFFFFFFF
	sd	t4, 0x068(t3)			/* heart_wid_err_mask */

	/* jam final values in bridge regs */
	dla	t3, BRIDGE_REG_BASE
	li	t4, 0x710C23FF
	sw	t4, 0x024(t3)			/* bridge_wid_control */
	li	t4, 0x3A080000
	sw	t4, 0x034(t3)			/* bridge_wid_int_upper */
	li	t4, 0x00000058
	sw	t4, 0x03C(t3)			/* bridge_wid_int_lower */
	li	t4, 0x00800000
	sw	t4, 0x084(t3)			/* bridge_dir_map */
	li	t4, 0x7FFFFF01
	sw	t4, 0x10C(t3)			/* bridge_int_enable */
	li	t4, 0x0000003A
	sw	t4, 0x12C(t3)			/* bridge_int_host_err */
	li	t4, 0x0000001F
	sw	t4, 0x134(t3)			/* bridge_int_addr(0) */
	

	mtc0	t2, C0_EPC
	mtc0	r0, C0_Cause
	eret				# eret to irix
	nop
END (Boot)

        .align      9       # Align to 1st BEV exc vector location (0xbfc00200)
LEAF (BEV_TLB_Refill)
BEV_TLB_Refill_ExcV: 
	dla	k0, TLB_ExcV
	jalr	k1, k0          	# Go to Normal Handler.
	xori	k1, 0x10		# indicator that BEV vector used.
END  (BEV_TLB_Refill)


        .align      7                   # Align to 0xbfc00280
LEAF (BEV_XTLB_Refill)
BEV_XTLB_Refill_ExcV: 
	dla	k0, XTLB_ExcV
	jalr 	k1, k0          	# Go to Normal Handler.
	xori	k1, 0x10		# indicator that BEV vector used.
END  (BEV_XTLB_Refill)


        .align      7                   # Align to 0xbfc00300
LEAF (BEV_CacheError)
BEV_CacheError_ExcV: 
	dla	k0, CacheError_ExcV
        jalr 	k1, k0          	# Go to Normal Handler.
        xori	k1, 0x10		# indicator that BEV vector used.
END  (BEV_CacheError)


        .align      7                   # Align to 0xbfc00380
LEAF (BEV_Others)
BEV_Others_ExcV: 
	dla	k0, Others_ExcV
        jalr 	k1, k0          	# Go to Normal Handler.
        xori	k1, 0x10		# indicator that BEV vector used.
END  (BEV_Others)

	.text
	.align	10		# Up to address 0xbfc00400
/*
 * If the vision law "ExtraInit=Init400" is set, runtime.ascii will cause this 
 * routine to be executed also as part of the boot sequence.
 */
LEAF(Init400)		
	mtc1	r0, fp0
	nop
	cvt.s.w	fp0, fp0
	add.s	fp1, fp0, fp0
	add.s	fp2, fp0, fp0
	mul.s	fp3, fp0, fp0
	add.s	fp4, fp0, fp0
	mul.s	fp5, fp0, fp0
	add.s	fp6, fp0, fp0
	mul.s	fp7, fp0, fp0
	mult	r0, r0
	li	r1, 0
	li	r2, 0
	li	r3, 0
	li	r4, 0
	li	r5, 0
	li	r6, 0
	li	r7, 0
	add.s	fp8, fp0, fp0
	mul.s	fp9, fp0, fp0
	add.s	fp10, fp0, fp0
	mul.s	fp11, fp0, fp0
	add.s	fp12, fp0, fp0
	mul.s	fp13, fp0, fp0
	add.s	fp14, fp0, fp0
	mul.s	fp15, fp0, fp0
	li	r7, 0
	li	r8, 0
	li	r9, 0
	li	r10, 0
	li	r11, 0
	li	r12, 0
	li	r13, 0
	li	r14, 0
	add.s	fp16, fp0, fp0
	mul.s	fp17, fp0, fp0
	add.s	fp18, fp0, fp0
	mul.s	fp19, fp0, fp0
	add.s	fp20, fp0, fp0
	mul.s	fp21, fp0, fp0
	add.s	fp22, fp0, fp0
	mul.s	fp23, fp0, fp0
	li	r15, 0
	li	r16, 0
	li	r17, 0
	li	r18, 0
	li	r19, 0
	li	r20, 0
	li	r21, 0
	li	r22, 0
	add.s	fp24, fp0, fp0
	mul.s	fp25, fp0, fp0
	add.s	fp26, fp0, fp0
	mul.s	fp27, fp0, fp0
	add.s	fp28, fp0, fp0
	mul.s	fp29, fp0, fp0
	add.s	fp30, fp0, fp0
	mul.s	fp31, fp0, fp0
	li	r23, 0
	li	r24, 0
	li	r25, 0
	li	r28, 0
	li	r29, 0
	li	r30, 0
	mtc0	r0, C0_PageMask
	mtc0	r0, C0_Cause
	jr	r31
END(Init400)		

	.align	9		# Up to address 0xbfc00600
LEAF(Init600)		
	mtc1	r0, fp0
	mtc1	r0, fp1
	mtc1	r0, fp2
	mtc1	r0, fp3
	mtc1	r0, fp4
	mtc1	r0, fp5
	mtc1	r0, fp6
	mtc1	r0, fp7
	mult	r0, r0
	li	r1, 0
	li	r2, 0
	li	r3, 0
	li	r4, 0
	li	r5, 0
	li	r6, 0
	li	r7, 0
	add.s	fp8, fp0, fp0
	mul.s	fp9, fp0, fp0
	add.s	fp10, fp0, fp0
	mul.s	fp11, fp0, fp0
	add.s	fp12, fp0, fp0
	mul.s	fp13, fp0, fp0
	add.s	fp14, fp0, fp0
	mul.s	fp15, fp0, fp0
	li	r7, 0
	li	r8, 0
	li	r9, 0
	li	r10, 0
	li	r11, 0
	li	r12, 0
	li	r13, 0
	li	r14, 0
	add.s	fp16, fp0, fp0
	mul.s	fp17, fp0, fp0
	add.s	fp18, fp0, fp0
	mul.s	fp19, fp0, fp0
	add.s	fp20, fp0, fp0
	mul.s	fp21, fp0, fp0
	add.s	fp22, fp0, fp0
	mul.s	fp23, fp0, fp0
	li	r15, 0
	li	r16, 0
	li	r17, 0
	li	r18, 0
	li	r19, 0
	li	r20, 0
	li	r21, 0
	li	r22, 0
	add.s	fp24, fp0, fp0
	mul.s	fp25, fp0, fp0
	add.s	fp26, fp0, fp0
	mul.s	fp27, fp0, fp0
	add.s	fp28, fp0, fp0
	mul.s	fp29, fp0, fp0
	add.s	fp30, fp0, fp0
	mul.s	fp31, fp0, fp0
	li	r23, 0
	li	r24, 0
	li	r25, 0
	li	r28, 0
	li	r29, 0
	li	r30, 0
	mtc0	r0, C0_PageMask
	mtc0	r0, C0_Cause
	jr	r31
END(Init600)		

	.align	11		# Up to address 0xbfc00800

LEAF(CustomBoot)		# 64 words alotted for code 
				# (runtime.ascii loaded here) 
	dla   	k1, IRIX
	mtc0	k1, C0_EPC
	nop
	j    	r31                 # Return Back.
	nop
END(CustomBoot)
