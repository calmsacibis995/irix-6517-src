/* pon_io.s - register-based power-on diagnostic output primitives */

#include "asm.h"
#include "sys/sbd.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/cpu.h"
#include "sys/ns16550.h"
#include "early_regdef.h"

LEAF(pon_initio)
	/* set up bridge device(x) register for the IOC3 */
	LI	T00,PHYS_TO_COMPATK1(BRIDGE_BASE)

	li	T01,PCI_MAPPED_MEM_BASE(BRIDGE_IOC3_ID)>>20
	or	T01,BRIDGE_DEV_DEV_IO_MEM
	sw	T01,BRIDGE_DEVICE(BRIDGE_IOC3_ID)(T00)

#if defined(MFG_IO6)
	/*
	 * DBGPROM will use IO6 IOC3 devices.
	 * set up bridge device(x) register for the IO6 - Slot 10 (0xA)
	 */
	LI	T00,PHYS_TO_COMPATK1( MAIN_WIDGET(XBOW_PORT_A) )

	li	T01,PCI_MAPPED_MEM_BASE(BRIDGE_IOC3_ID)>>20
	or	T01,BRIDGE_DEV_DEV_IO_MEM
	sw	T01,BRIDGE_DEVICE(BRIDGE_IOC3_ID)(T00)
#endif	/* MFG_IO6 */



	/*
	 * set up, through PCI configuration space, to access IOC3 device
	 * address space through PCI memory space
	 */
	LI	T00,PHYS_TO_COMPATK1(IOC3_PCI_CFG_BASE)
	lw	T01,PCI_CFG_COMMAND(T00)
	or	T01,PCI_CMD_MEM_SPACE
	sw	T01,PCI_CFG_COMMAND(T00)

	/* IOC3 device address space within PCI memory space */
	li	T01,PCI_MAPPED_MEM_BASE(BRIDGE_IOC3_ID)
	sw	T01,PCI_CFG_BASE_ADDR_0(T00)


#if defined(MFG_IO6)
	/*
	 * DBGPROM will use IO6 IOC3 devices.
	 * set up, through PCI configuration space, to access IO6 - IOC3 device
	 * address space through PCI memory space
	 */
	LI	T00,PHYS_TO_COMPATK1( MAIN_WIDGET(XBOW_PORT_A)+BRIDGE_TYPE0_CFG_DEV(BRIDGE_IOC3_ID) )
	lw	T01,PCI_CFG_COMMAND(T00)
	or	T01,PCI_CMD_MEM_SPACE
	sw	T01,PCI_CFG_COMMAND(T00)

	/* IOC3 device address space within PCI memory space */
	li	T01,PCI_MAPPED_MEM_BASE(BRIDGE_IOC3_ID)
	sw	T01,PCI_CFG_BASE_ADDR_0(T00)
#endif	/* MFG_IO6 */



	/* unreset the superio and set the CMD_PULSE field */
	LI	T00,PHYS_TO_COMPATK1(IOC3_PCI_DEVIO_BASE)
	li	T01,0x4<<SIO_CR_CMD_PULSE_SHIFT
	sw	T01,IOC3_SIO_CR(T00)


	/* unreset the superio and set the CMD_PULSE field */
#if defined(MFG_IO6)
	/*
	 * DBGPROM will use IO6 IOC3 devices.
	 */
	LI	T00,PHYS_TO_COMPATK1( MAIN_WIDGET(XBOW_PORT_A)+BRIDGE_DEVIO2 )
	li	T01,0x4<<SIO_CR_CMD_PULSE_SHIFT
	sw	T01,IOC3_SIO_CR(T00)
#endif	/* MFG_IO6 */




	/*
	 * enable tristate generic PIO pins that are used as serial
	 * transceiver select for output
	 */
#ifdef EMULATION
	li	T01,(0x1<<2)|(0x1<<3)	/* generic PIO pins 2/3 */
	sw	T01,IOC3_GPCR_S(T00)	/* enable as output */
	sw	zero,IOC3_GPPR(2)(T00)	/* RS232 mode */
	sw	zero,IOC3_GPPR(3)(T00)	/* RS232 mode */
#else
#define	IOC3_PINS_OUTPUT_ENABLE		(GPCR_DIR_SERA_XCVR |	\
					 GPCR_DIR_SERB_XCVR |	\
					 GPCR_DIR_LED0 |	\
					 GPCR_DIR_LED1 |	\
					 GPCR_MLAN_EN)
	li	T01,IOC3_PINS_OUTPUT_ENABLE
	sw	T01,IOC3_GPCR_S(T00)	/* enable as output */

	/* RS232 mode for both UARTs, both LEDs off */
	sw	zero,IOC3_GPDR(T00)
#endif	/* EMULATION */

#define	STORE(reg,value)	li T01,value; sb T01,reg(T00)

#if defined(MFG_IO6)
	/*
	 * DBGPROM will use IO6 IOC3 devices.
	 */
	LI	T00,PHYS_TO_COMPATK1( MAIN_WIDGET(XBOW_PORT_A)+BRIDGE_DEVIO2+IOC3_SIO_UA_BASE )
#else
	LI	T00,MIPS_SER0_BASE
#endif /* MFG_IO6 */
	STORE(LINE_CNTRL_REG, LCR_DLAB)
	STORE(DIVISOR_LATCH_MSB_REG, SER_DIVISOR(9600,SER_XIN_CLK/SER_PREDIVISOR)>>8)
	STORE(DIVISOR_LATCH_LSB_REG, SER_DIVISOR(9600,SER_XIN_CLK/SER_PREDIVISOR)&0xff)
	STORE(SCRATCH_PAD_REG, SER_PREDIVISOR * 2)
	STORE(LINE_CNTRL_REG, LCR_8_BITS_CHAR|LCR_1_STOP_BITS)
	STORE(INTR_ENABLE_REG, 0x0)
	STORE(FIFO_CNTRL_REG, FCR_ENABLE_FIFO)
	STORE(FIFO_CNTRL_REG, FCR_ENABLE_FIFO|FCR_RCV_FIFO_RESET|FCR_RCV_FIFO_RESET)

	j	ra
	END(pon_initio)

LEAF(pon_puthex)
	li	T10,28		# Amount to shift to get the 1st digit
	b	print_hex

XLEAF(_pon_puthex64)		# no leading 0x on hex number.
	li	T10,60		# Amount to shift to get the 1st digit
	b	_print_hex

XLEAF(pon_puthex64)
	li	T10,60		# Amount to shift to get the 1st digit
	b	print_hex

_print_hex:
	move	T11,a0
	move	RA1,ra
	b	1f

print_hex:
	move	T11,a0
	move	RA1,ra

	/* print leading '0x' */
	li	a0,0x30		# Hex code for '0'
	jal	pon_putc
	li	a0,0x78		# Hex code for 'x'
	jal	pon_putc

1:
	dsrlv	a0,T11,T10	# Isolate digit 
	and	a0,0xf
	lb	a0,hexdigit(a0) # Convert it to hexidecimal
	jal	pon_putc	# Print it
	sub	T10,4		# Set up next nibble
	bge	T10,zero,1b

	j	RA1		# Yes - done
	END(pon_puthex)

	.data
hexdigit:
	.ascii  "0123456789abcdef"

	.text
LEAF(pon_puts)
	move	RA1,ra
	move	T10,a0
	b	2f		# jump to test for end-of-string
1:
	daddu	T10,1		# Bump to next character
	jal	pon_putc	# No - Display character
2:
	lb	a0,0(T10)	# Get character to display
	bne	a0,zero,1b	# End of string?

	j	RA1
	END(pon_puts)

/*
 * putc subroutine
 */
#if defined(EMULATION) || defined(SABLE)
#define DELAY_COUNT		1
#else
#define	DELAY_COUNT		0x8000
#endif

LEAF(pon_putc)
 	li	T01,DELAY_COUNT
#if defined(MFG_IO6)
	/*
	 * DBGPROM will use IO6 IOC3 devices.
	 */
	LI	T00,PHYS_TO_COMPATK1( MAIN_WIDGET(XBOW_PORT_A)+BRIDGE_DEVIO2+IOC3_SIO_UA_BASE )
#else
	LI	T00,MIPS_SER0_BASE
#endif	/* MFG_IO6 */

1:
	lb	T02,LINE_STATUS_REG(T00)

	and	T02,LSR_XMIT_BUF_EMPTY	# transmitter ready ?
	bne	T02,zero,9f		# if not empty, delay
 	sub	T01,1			# decrement the count
 	beq	T01,zero,2f		# spin till it be 0
	b	1b			# just spin till ready
2:
 	j	ra			# no output, just return
9:
	sb	a0,XMIT_BUF_REG(T00)

 	j	ra			# and return
	END(pon_putc)

LEAF(pon_memerr)
	/*  a0 - bad address
	 *  a1 - data expected from read
	 *  a2 - actual data obtained
	 */
	move	RA2,ra
	move	T20,a0		# save bad address
	move	T21,a1		# save expected value
	move	T22,a2		# save actual value

	jal	pon_amber_led	# tell user diags failed

	dla	a0,failed
	jal	pon_puts

	dla	a0,address
	jal	pon_puts
	move	a0,T20		# address where error occured
	jal	pon_puthex64

	dla	a0,expected
	jal	pon_puts
	move	a0,T21		# data expected
	jal	pon_puthex64

	dla	a0,received
	jal	pon_puts
	move	a0,T22		# actual value read
	jal	pon_puthex64

	dla	a0,crlf		# new line
	jal	pon_puts

	j	RA2
	END(pon_memerr)


#if EMULATION
#define	WAITCOUNT	2000
#else
#define	WAITCOUNT	200000
#endif	/* EMULATION */
LEAF(pon_flash_led)
#if EMULATION || SABLE
	LI	a0,PHYS_TO_COMPATK1(HEART_MODE)
	ld	v0,0(a0)
	dli	v1,~HM_TRIG_SRC_SEL_MSK
	and	v0,v1
	dli	v1,HM_TRIG_REG_BIT
	or	v0,v1
	sd	v0,0(a0)

	LI	a0,PHYS_TO_COMPATK1(HEART_TRIGGER)
	li	v0,0x1		/* set pin initially */

	.set	noreorder
1:
	sd	v0,0(a0)
	li	a1,WAITCOUNT

2:	bne	a1,zero,2b
	sub	a1,1

	b	1b
	xor	v0,1		/* switch pin state */
	.set	reorder
#else
	LI	a3,IOC3_PCI_DEVIO_K1PTR	# IOC3 base
	sw	zero,IOC3_GPPR(0)(a3)	# clear green led
	sw	zero,IOC3_GPPR(1)(a3)	# clear amber led
	li	a4,1			# initial amber value
	move	a5,zero			# counter
1:
	and	a6,a5,0x1ffff		# flash every so often
	bnez	a6,2f
	sw	a4,IOC3_GPPR(1)(a3)	# write led
	xor	a4,1			# change amber/off

	/* Poll for power interrupt */
2:	LI	a0,PHYS_TO_COMPATK1(BRIDGE_BASE|BRIDGE_INT_STATUS)
	lw	a0,0(a0)
	andi	a0,PWR_SWITCH_INTR
	daddiu	a5,1			# inc flash counter
	beqz	a0,1b			# no press, loop forever
	j	bev_poweroff		# turn the power off
#endif	/* EMULATION || SABLE */
	END(pon_flash_led)

/* modify LED state according to a0 */
LEAF(pon_amber_led)
#if !(EMULATION || SABLE)
	LI	a0,IOC3_PCI_DEVIO_K1PTR	# IOC3 base
	sw	zero,IOC3_GPPR(0)(a0)	# clear green led
	li	v0,1
	sw	v0,IOC3_GPPR(1)(a0)	# set amber led
#endif
	j	ra
	END(pon_amber_led)

	.data
address:	.asciiz	"Address: "
expected:	.asciiz	", Expected: "
received:	.asciiz	", Received: "
failed:		.asciiz "       *FAILED*\r\n"

EXPORT(crlf)
		.asciiz	"\r\n"
