#include <asm.h>
#include <early_regdef.h>
#include <sys/sbd.h>
#include <sys/cpu.h>

#define	_CPUCTRL0		(CPUCTRL0^0x4)
#define	_EEROM			(0x1fa00034)



	.set noreorder

#define ENDIAN_MASK     0x8400	/* mask for the 2 endian bits in the EEROM */

/* switch from little/big endian to big/little endian */
LEAF(switch_endian)
	move	RA3,ra

	jal	eerom_read		/* read register 0 where the 2 endian */
	move	a0,zero			/* bits are located */

	xor	a1,v0,ENDIAN_MASK	/* switch the 2 endian bits */
	jal	eerom_write		/* write to the EEROM */
	move	a0,zero

	LI	v0,PHYS_TO_K1(_CPUCTRL0)	/* reset for the new EEROM */
	li	v1,CPUCTRL0_WR_ST 		/* bits to take effect */
	sw	v1,0(v0)

	j	RA3			/* never reached */
	nop
	END(switch_endian)



/* turn on the chip select to the EEROM */
LEAF(eerom_cs_on)
	LI	T00,PHYS_TO_K1(_EEROM)

	move	T01,zero
	sw	T01,0(T00)		# clear serial clock, CS, etc.
	or	T01,EEROM_CS
	sw	T01,0(T00)		# turn chip select on
	or	T01,EEROM_SCK
	sw	T01,0(T00)		# set serial clock

	j	ra
	nop
	END(eerom_cs_on)



/* turn off the chip select to the EEROM */
LEAF(eerom_cs_off)
	LI	T00,PHYS_TO_K1(_EEROM)
	lw	T01,0(T00)		# read eerom register
	nop

	and	T01,~EEROM_SCK
	sw	T01,0(T00)		# clear serial clock
	and	T01,~EEROM_CS
	sw	T01,0(T00)		# turn chip select off
	or	T01,EEROM_SCK
	sw	T01,0(T00)		# set serial clock

	j	ra
	nop
	END(eerom_cs_off)



#define	COMMAND_SIZE	11

/*
 * send the specified command to the EEROM
 *
 * a0 = command
 * a1 = register number
 */
LEAF(eerom_cmd)
	LI	T00,PHYS_TO_K1(_EEROM)
	lw	T01,0(T00)		# read eerom register

	li	T02,COMMAND_SIZE	# command bits + address bits = 11
	sll	a1,16-COMMAND_SIZE	# data in a0 is left justified in the
	or	a0,a1			# lower half word

1:
	and	a1,a0,0x8000		# msb first
	and	T01,~EEROM_SO
	beq	a1,zero,2f
	nop
	or	T01,EEROM_SO

2:
	sw	T01,0(T00)
	and	T01,~EEROM_SCK
	sw	T01,0(T00)
	or	T01,EEROM_SCK
	sw	T01,0(T00)

	sub	T02,1
	bne	T02,zero,1b
	sll	a0,1

	and	T01,~EEROM_SO
	sw	T01,0(T00)

	j	ra
	nop
	END(eerom_cmd)



#define	EEROM_REG_SIZE	16

/*
 * read the specified 16 bits register from the EEROM
 *
 * a0 = register number
 *
 * v0 returns read data
 */
LEAF(eerom_read)
	move	RA1,ra
	move	T10,a0

	jal	eerom_cs_on
	nop

	move	a1,T10			# register number
	jal	eerom_cmd
	li	a0,SER_READ

	LI	a0,PHYS_TO_K1(_EEROM)
	lw	a1,0(a0)
	li	a2,EEROM_REG_SIZE
	move	T10,zero

1:
	and	a1,~EEROM_SCK
	sw	a1,0(a0)
	or	a1,EEROM_SCK
	sw	a1,0(a0)

	sll	T10,1
	lw	a1,0(a0)
	nop
	and	a3,a1,EEROM_SI
	beq	a3,zero,2f
	nop
	or	T10,1

2:
	sub	a2,1
	bne	a2,zero,1b
	nop

	jal	eerom_cs_off
	nop

	j	RA1
	move	v0,T10
	END(eerom_read)



/*
 * write to the specified 16 bits register in the EEROM 
 *
 * a0 = register number
 * a1 = data
 */
LEAF(eerom_write)
	move	RA2,ra
	move	T20,a0
	move	T21,a1

	jal	eerom_cs_on
	nop

	li	a0,SER_WEN		# enable the EEROM for writing
	jal	eerom_cmd
	move	a1,zero

	jal	eerom_cs_off
	nop

	jal	eerom_cs_on
	nop

	move	a1,T20			# register number
	jal	eerom_cmd
	li	a0,SER_WRITE

	LI	a0,PHYS_TO_K1(_EEROM)
	lw	a1,0(a0)
	li	a2,EEROM_REG_SIZE

1:
	and	a3,T21,0x8000
	and	a1,~EEROM_SO
	beq	a3,zero,2f
	nop
	or	a1,EEROM_SO

2:
	sw	a1,0(a0)
	and	a1,~EEROM_SCK
	sw	a1,0(a0)
	or	a1,EEROM_SCK
	sw	a1,0(a0)

	sub	a2,1
	bne	a2,zero,1b
	sll	T21,1

	jal	eerom_cs_off
	nop

	jal	eerom_hold
	nop

	jal	eerom_cs_on
	nop

	li	a0,SER_WDS		# disable the EEROM for writing
	jal	eerom_cmd
	move	a1,zero

	jal	eerom_cs_off
	nop

	j	RA2
	nop
	END(eerom_write)



#define	LED_FLASH_COUNT	100000

/*
 * wait for EEROM to finish writing. flash the LED every ~1 second and don't
 * return if write fails
 */
LEAF(eerom_hold)
	move	RA1,ra

	jal	eerom_cs_on
	nop

	LI	T10,PHYS_TO_K1(_EEROM)
	LI	T11,PHYS_TO_K1(HPC3_WRITE1)
	li	a1,LED_FLASH_COUNT
#ifdef IP24
	li	a2,KBD_MS_RESET|PAR_RESET|VIDEO_RESET|ISDN_RESET
#else
	li	a2,EISA_RESET|KBD_MS_RESET|PAR_RESET
#endif

1:
	lw	a0,0(T10)
	sub	a1,1
	and	a0,EEROM_SI
	bne	a0,zero,2f
	nop
	bne	a1,zero,1b
	nop
#ifdef IP24
	xor	a2,LED_GREEN_OFF|LED_RED_OFF	# off/amber
#else
	xor	a2,LED_AMBER			# amber/off
#endif
	li	a1,LED_FLASH_COUNT
	b	1b
	sw	a2,0(T11)

2:
	jal     eerom_cs_off
	nop

	j	RA1
	nop
	END(eerom_hold)

	.set reorder
