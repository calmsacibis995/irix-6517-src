#ident	"IP30prom/csu_rprom.s:  $Revision: 1.7 $"

/*
 * csu_rprom.s -- rprom startup code
 */

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <ml.h>
#include <fault.h>
#include <sys/fpregdef.h>

#define RPROM_BEG_U16W		(TO_COMPAT_PHYS_MASK & FLASH_MEM_BASE)>>16
#define RPROM_END_U16W		(TO_COMPAT_PHYS_MASK & FTEXTADDR)>>16
#define PHYS_PC_U16W_MASK	(TO_COMPAT_PHYS_MASK)>>16
#define COMPAT_U32DW		(COMPAT_K0BASE)>>32

/*
 * BEGIN: rprom entry point table
 *
 * the following table is to process following conditions
 * Cold Reset:
 *		power/reset
 * 	
 * Soft Reset:
 *		cpu_hardreset		
 * NMI:
 *		nmi			ErrorEPC
 *
 * Boot Exceptions:
 *		various exceptions	EPC
 *
 * Except for Cold/Soft reset the NMI and boot exceptions handler
 * can either be rprom based or fprom based. If the source of
 * the exception was in rprom we will process them otherwise
 * forward the exception to the fprom.
 */
LEAF(start)
	.set	noat		# so NMI preserves AT
				/* 0x000 offset */
	j	rpromstart
	j	nmi		# for debugging SW NMIs
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x040 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x080 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x0c0 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x100 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x140 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x180 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x1c0 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x200 offset */
	j	_bev_utlbmiss	/* utlbmiss boot exception vector, 32 bits */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x240 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x280 offset */
	j	_bev_xutlbmiss	/* utlbmiss boot exception vector, 64 bits */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x2c0 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x300 offset */
	j	_bev_cacheerror
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x340 offset */
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
				/* 0x380 offset */
	j	_bev_general
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented
	j	_notimplemented

/*
 * END: rprom entry point table
 */

#ifdef IP30_RPROM
rpromstart:
	.set		noreorder
/*
 * check if NMI or cold/soft reset
 */
	mfc0		k0,C0_SR		# load SR from C0
	srl		k0,16			# shift SR & NMI bits to lo16
	andi		k0,(SR_SR|SR_NMI)>>16
	sub		k0,(SR_SR|SR_NMI)>>16
	beqz		k0,nmi
	nop					# BDSLOT

	.set		reorder
/*
 * this was a cold/soft reset not NMI go to real start
 */
	j		realstart

nmi:
/*
 * NMI and BEV handling is based on the ErrorEPC and EPC 
 * repectively which indicate the src of the exception.
 *
 * BEV processing:
 * If (src=rprom) use rprom handlers, jump to the bev_xxx handler
 * otherwise use fprom handlers, load bev offset to k0
 * and jump to FTEXTADDR + k0, where FTEXTADDR is fprom base
 *
 * NMI processing:
 *	Similar to regular exceptions, except we must debounce the NMI
 *	button (fprom expects this) and handle only saving k1 1x (usually).
 *
 *	We also really hope the master and slave are in the same address
 *	space.
 */
	.set	noat
	.set	noreorder
	li	k0,SR_PROMBASE|SR_DE	# ensure we are 64-bit, and sane
	mtc0	k0,C0_SR

	mfc0	k0,C0_LLADDR		# if LLADDR has magic, skip k1 save
	addi	k0,-0x00007d83
	beqz	k0,1f
	nop				# BDSLOT

	li	k0,0x00007d83
	mtc0	k0,C0_LLADDR

	dmfc0	k0,C0_ERROR_EPC		# Try to save this before a bounce.
	dmtc1	k0,$f2
	dmtc1	k1,$f1			# k1 is more important that FPU state
	.set	reorder
1:

	/* Spin for ~1 second to debounce NMIs. */
	LI	k0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	k1,0(k0)
	li	k0,HEART_COUNT_RATE	# 1 second
	daddu	k1,k0			# base + 1 second
1:	LI	k0,PHYS_TO_COMPATK1(HEART_COUNT)
	ld	k0,0(k0)
	dsub	k0,k1,k0
	bgtz	k0,1b

	/* Now we are set to see where we came from */

	.set		noreorder
	dmfc1		k1,$f2			# restore error epc
	mtc0		zero,C0_LLADDR		# clear LLADDR since it
						# retains its value across reset
	dmtc0		k1,C0_ERROR_EPC
	.set		reorder

	.set		noreorder
	dmfc0		k1,C0_ERROR_EPC		# saved error epc.
	.set		reorder
	dsrl		k1,32
	LI		k0,COMPAT_U32DW
	bne		k0,k1,1f		# COMPAT_Kx space?

	.set		noreorder
	dmfc0		k1,C0_ERROR_EPC		# saved error epc.
	.set		reorder
	srl		k1,16
	andi		k1,PHYS_PC_U16W_MASK

	sltiu		k0,k1,RPROM_BEG_U16W	# below Rprom ?
	bne		k0,zero,1f
	sltiu		k0,k1,RPROM_END_U16W	# above Rprom ?
	beq		k0,zero,1f		

						# ErrEPC is from Rprom

	dmfc1		k1,$f1			# restore k1
	dmtc1		zero,fa0		# flag for nmi
	j		bev_nmi

1:						# ErrEPC is from Fprom
	dmfc1		k1,$f1			# restore k1	
	LI		k0,FTEXTADDR+8		# bev_nmi hook
	jr		k0

#define CHECK_COMPATK(label)					\
	.set		noreorder;				\
	DMFC0		(k1,C0_EPC);				\
	.set		reorder;				\
	dsrl		k1,32;					\
	LI		k0,COMPAT_U32DW;			\
	bne		k0,k1,label

#define CHECK_RPROM(label,bev_fct)				\
	.set		noreorder;				\
	DMFC0		(k1,C0_EPC);				\
	.set		reorder;				\
	srl		k1,16;					\
	andi		k1,PHYS_PC_U16W_MASK;			\
	sltiu		k0,k1,RPROM_BEG_U16W;			\
	bne		k0,zero,label;				\
	sltiu		k0,k1,RPROM_END_U16W;			\
	beq		k0,zero,label;				\
	j		bev_fct
	
_bev_utlbmiss:
	CHECK_COMPATK(1f)
	CHECK_RPROM(1f,bev_utlbmiss)

1:	LI		k0,0x200
	b		10f

_bev_xutlbmiss:
	CHECK_COMPATK(1f)
	CHECK_RPROM(1f,bev_xutlbmiss)

1:	LI		k0,0x280
	b		10f
		
_bev_cacheerror:
	CHECK_COMPATK(1f)
	CHECK_RPROM(1f,bev_cacheerror)

1:	LI		k0,0x340
	b		10f
		
_bev_general:
	CHECK_COMPATK(1f)
	CHECK_RPROM(1f,bev_general)

1:	LI		k0,0x380
	b		10f
		
_notimplemented:
	CHECK_COMPATK(1f)
	CHECK_RPROM(1f,notimplemented)

1:	LI		k0,0x4
	b		10f


/*
 * forward the bev to the fprom
 */

/*
 * src of exception not (FLASH_MEM_BASE <= a0 < FTEXTADDR)
 * so let fprom handle it, could be fprom, dprom, and
 * possibly the flash critical code relocated to bss (for all Xproms
 * but if take an exception in flash critical code we're toast)
 */
10:
	LI		k1,FTEXTADDR
	PTR_ADDU	k0,k1
	jr		k0
		
	END(start)

LEAF(reloc_to_fprom)
	LI		k0,FTEXTADDR
	jr		k0

	END(reloc_to_fprom)

/*
 * RPROM system exception handlers:
 * very simple for all, we really cant do much,
 * XXX may put out some LED sequence
 * just dump the registers and go into power spin
 * k0, $f12 (a.k.a. fa0), $f13 (fa1), and $f14 (fa2), are used
 *
 * 	k0	- scratch
 * 	fa0	- EXCEPT_XXX
 *	fa1	- C0_CAUSE
 *	fa2	- C0_EPC
 * NOTE: use use the pon_nmi_handler to dump all the registers
 * the fa0 contains non-zero value and thats how the handler
 * differentiates system exceptions from nmi (fa0==0).
 */

	.set		noreorder
	.set		noat

/*
 * each exception vector will exec off of memory
 * it has to fit under 32 inst's and use jr, no j or b
 * 'cuz it has to jump from sysmem to prom > 256 MB
 * XXX dont know if all the nop's are necessary but
 * this is based on _j_exceptnorm code.
 */

LEAF(rprom_exceptutlb)
	LA		k1,rprom_exceptcommon
	li		k0,EXCEPT_UTLB
	nop
	jr		k1
	nop
	END(rprom_exceptutlb)

LEAF(rprom_exceptnorm)
	LA		k1,rprom_exceptcommon
	li		k0,EXCEPT_NORM
	nop
	jr		k1
	nop
	END(rprom_exceptnorm)

LEAF(rprom_exceptxut)
	LA		k1,rprom_exceptcommon
	li		k0,EXCEPT_XUT
	nop
	jr		k1
	nop
	END(rprom_exceptxut)

LEAF(rprom_exceptecc)
	LA		k1,rprom_exceptcommon
	li		k0,EXCEPT_ECC
	nop
	jr		k1
	nop
	END(rprom_exceptecc)

	.set		reorder

/*
 * system exception handling back in Rprom, we should have
 * jumped to this common code from the except vector,
 * case the needed regs and goto pon_nmi_handler to 
 * have the registers dumped and to powerspin
 */ 

LEAF(rprom_exceptcommon)
	mtc1		k0,fa0			# non-zero, nmi is zero
	.set    	noreorder
	mfc0    	k0,C0_CAUSE
	.set    	reorder
	mtc1		k0,fa1
	.set    	noreorder
	DMFC0(k0,C0_EPC)
	.set    	reorder
	dmtc1		k0,fa2

	j		pon_nmi_handler
	END(rprom_exceptcommon)

	.set		at


#else /* !IP30_RPROM */

/*
 * this is for the current NULL RPROM which will go away
 * once we throw switch on to go to 3 segments for RPROM
 */

rpromstart:
	move		k0,zero
	b		1f

_bev_utlbmiss:
	LI		k0,0x200
	b		1f

_bev_xutlbmiss:
	LI		k0,0x280
	b		1f
		
_bev_cacheerror:
	LI		k0,0x340
	b		1f
		
_bev_general:
	LI		k0,0x380
	b		1f
		
_notimplemented:
	LI		k0,0x4
	b		1f

1:
	LI		k1,FTEXTADDR
	PTR_ADDU	k0,k1
	jr		k0
		
	END(start)

#endif /* !IP30_RPROM */

/*
 * FTEXTADDR and sflash.h fprom address consistancy check
 */
#include <sys/RACER/sflash.h>

#if FTEXTADDR != SFLASH_FTEXT_ADDR
#include "error -- FTEXTADDR != SFLASH_FTEXT_ADDR, Makefile and sflash.h conflict"
#endif
