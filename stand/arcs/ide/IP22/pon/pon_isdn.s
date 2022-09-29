#ident	"IP22diags/pon/pon_addr.s:  $Revision: 1.5 $"

#include <asm.h>
#include <early_regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>

#define  ISAC_VERS_MASK	0x7f
#define  ISAC_VERS	0x60
#define  ISAC_VERS_REG	0x1fbd94ab
#define  ISAC_CIR0_REG	0x1fbd94c7
#define  HSCX_STAR1_REG	0x1fbd9087
#define  HSCX_STAR2_REG	0x1fbd9187

/*
 * Test the ISDN chip set by looking at power on values.
 */
LEAF(pon_isdn)
	move	RA3,ra			# save return address

	IMPORT(_hpc3_write1,4)
	lw	T32, _hpc3_write1
	ori	T32, ISDN_RESET
	sw	T32, _hpc3_write1
	LI	T34, PHYS_TO_K1(HPC3_WRITE1);
	sw	T32, 0(T34)		# Turn off reset

/*	li	T34, PHYS_TO_K1(ISAC_VERS_REG)
	lbu	T34, 0(T34)
	andi	T34, T34, ISAC_VERS_MASK
	li	T32, ISAC_VERS
	bne	T34, T32, error_exit  */

	LI	T34, PHYS_TO_K1(ISAC_CIR0_REG)
	lbu	T34, 0(T34)
	li	T32, 0x7c
	bne	T34, T32, error_exit

	LI	T34, PHYS_TO_K1(HSCX_STAR1_REG)
	lbu	T34, 0(T34)
	li	T32, 0x48
	bne	T34, T32, error_exit

	LI	T34, PHYS_TO_K1(HSCX_STAR2_REG)
	lbu	T34, 0(T34)
	li	T32, 0x48
	bne	T34, T32, error_exit

	# Do I have to restore registers?

	li	v0, 0			# Test passed
	j	RA3

error_exit:
	li	v0, 1			# Error
	j	RA3

END(pon_isdn)

