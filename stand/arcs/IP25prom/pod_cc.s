/*
 * pod_cc.s -
 *	POD CPU board tests.
 */
#include <asm.h>

#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evdiag.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/sysctlr.h>
#include <sys/EVEREST/IP25addrs.h>
	
#include "ip25prom.h"
#include "prom_intr.h"
#include "prom_leds.h"
#include "pod.h"
#include "ml.h"

/*
 * For testing read/write registers, we build a table of the
 * addresses and value (double word) to write then read.
 * The table format is:
 *
 *	 	+---------+---------+
 *		| Address |  Value  |
 *		+---------+---------+
 * Byte          0       0 0	   1
 * Offset        0	 7 8	   5
 *
 * Address:	8-byte address of register to write/read
 *		8-byte valkue to write then read back and compare.
 */
#define CCT_ADDRESS	0		/* CC Test address */
#define CCT_VALUE	8		/* CC Test Value */
#define	CCT_SIZE	16		/* CC Test entry size */

/*
 * Macro:	CCT
 * Purpose:	CC chip test macro, used to define an entry in the
 *		Table of CC addresses/values to use.
 */
#define CCT(a, v)	.dword	a;  .dword	v;

	.rdata
ccLocalTest:
	CCT(EV_CEL,	0x000000000000007f)
	CCT(EV_CEL, 	0x000000000000002a)
	CCT(EV_CEL,	0x0000000000000055)
	CCT(EV_CEL,	0x0000000000000000)
	CCT(EV_IP0,	0xffffffffffffffff)
	CCT(EV_IP0,	0xaaaaaaaaaaaaaaaa)
	CCT(EV_IP0,	0x5555555555555555)
	CCT(EV_IP0,	0x0000000000000000)
	CCT(EV_IP1,	0xffffffffffffffff)
	CCT(EV_IP1,	0xaaaaaaaaaaaaaaaa)
	CCT(EV_IP1,	0x5555555555555555)
	CCT(EV_IP1,	0x0000000000000000)
	CCT(EV_IGRMASK,	0x000000000000ffff)
	CCT(EV_IGRMASK,	0x000000000000aaaa)
	CCT(EV_IGRMASK,	0x0000000000005555)
	CCT(EV_IGRMASK,	0x0000000000000000)
	CCT(EV_ILE,	0x0000000000000001)
	CCT(EV_ILE,	0x0000000000000000)
ccLocalTestEnd:

aConfigTest:
#if 0
	CCT(EV_A_URGENT_TIMEOUT,0x000000000000ffff)
	CCT(EV_A_URGENT_TIMEOUT,0x000000000000aaaa)
	CCT(EV_A_URGENT_TIMEOUT,0x0000000000005555)
	CCT(EV_A_URGENT_TIMEOUT,0x0000000000000000)
	CCT(EV_A_RSC_TIMEOUT,	0x00000000000fffff)
	CCT(EV_A_RSC_TIMEOUT,	0x00000000000aaaaa)
	CCT(EV_A_RSC_TIMEOUT,	0x0000000000055555)
	CCT(EV_A_RSC_TIMEOUT,	0x0000000000000000)
#endif
aConfigTestEnd:		

	.text
	.set	noreorder
/*
 * Function:	pod_check_cclocal
 * Purpose:	To verify the integrity of the CC chip local registers.
 * Parameters:	None
 * Returns:	0 - success
 *		!0 - failed
 */
LEAF(pod_check_cclocal)
	.set	noreorder
	/*
	 * Using the CCT table, check all of the requested
	 * read/write registers.
	 */
	dla	v0,ccLocalTest
	dla	v1,ccLocalTestEnd
1:	
	bgeu	v0,v1,2f
        nop
	ld	a0,CCT_ADDRESS(v0)	/* Pick up address */
	ld	t0,CCT_VALUE(v0)	/* And data */
	sd	t0,0(a0)		/* Bang */
	ld	t1,0(a0)		/* And back */
	bne	t0,t1,ccLocalFailed	/* Error --- */
	nop
	daddiu	v0,CCT_SIZE		/* Next entry */
	b	1b		
	nop				/* DELAY: */
	
2:	
	/*
	 * Success so far, continue testing those registers that can't
	 * be simply table driven.
	 */	
	dli	a0, EV_IP0		/* a0 = IP0 addr */
	li	a1, 1
	sd	a1 0(a0)		/* EV_IP0 = 1 */
	dli	a2, EV_CIPL0
	sd	zero, 0(a2)		/* Clear level 0 intr., prior 0 */
	ld	a3, 0(a0)		/* a3 = EV_IP0  */
#ifndef	SABLE
	bne	a3, zero, ccLocalFailed	/* Should have been cleared */
	nop
#endif

	dli	a0, EV_IP1		# a0 = IP1 addr
	li	a1, 1
	sd	a1 0(a0)		# EV_IP1 = 1
	dli	a2, EV_CIPL0
	li	a3, 64
	sd	a3, 0(a2)		# Clear level 0 intr., prior 64
	ld	a3, 0(a0)		# a3 = EV_IP1
#ifndef	SABLE
	bne	a3, zero, ccLocalFailed	/* Should have been cleared */
	nop
#endif
	
	dli	a2, EV_CERTOIP
	dli	a1, EV_CERTOIP_MASK
	dli	a0, EV_ERTOIP
	sd	a1, 0(a2)		# Clear EV_ERTOIP
	ld	a3, 0(a0)		# a3 = EV_ERTOIP
	nop
	bne	a3, zero, ccLocalFailed
	nop

	j	ra			# Return address is in v1
	move	v0, zero		# Return value (BD)
	END(pod_check_cclocal)

LEAF(ccLocalFailed)
	LEDS(PLED_CCLFAILED_INITUART)
	jal	ccuart_init
	FLASH(FLED_CCLOCAL)
	END(ccLocalFailed)

LEAF(clock_fail)
	LEDS(PLED_NOCLOCK_INITUART)
	jal	ccuart_init 	        # Call the UART configuration code
	nop				# (BD)
	FLASH(FLED_CCCLOCK)
	END(clock_fail)

LEAF(pod_check_ccconfig)

	LEDS(PLED_CKCCCONFIG)

	move	v1, ra			# Save return address

	# Start the clock by sending the sync interrupt

	dli	a0, EV_SENDINT
	li	a1, SYNC_DEST		# Clock sync interrupt number
	sd	a1, 0(a0)

	# Get our slot and processor number.

	EV_GET_SPNUM(t0, t1)		# t0 = slot number
					# t1 = processor number

	# Is my board listed in the system configuration information?

	dli	a0, EV_SYSCONFIG
	ld	a1, 0(a0)		# a1 = system configuration bit vector
	li	a2, 1
	sll	a2, a2, t0		# 1 in position of board in bit vector
	sll	a3, a2, 16		# 1 in position of CPU board in vector
	and	k0, a1, a2
	and	k1, a3, a1
	bne	k0, a2, ccconfig_fail
        nop
	bne	k1, a3, ccconfig_fail
	nop

	# Make sure the clock is running

	dli	a0, EV_RTC
	ld	a1, 0(a0)		# a1 = time in nanoseconds
	nop

	dli	a0, EV_SYSCONFIG
	ld	a0, 0(a0)		# Give the poor guy a chance...

	dli	a0, EV_RTC
	ld	a2, 0(a0)		# a2 = time a little later 
	nop	
#ifndef SABLE
	beq	a1, a2, clock_fail	# if the time hasn't changed, fail
	nop
#endif

	# Test timer config registers

	
	li	a0, 0x78			# Set LSB
	EV_SET_PROCREG(t0, t1, EV_CFG_CMPREG0, a0)

	li	a0, 0x56			# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CFG_CMPREG1, a0)

	li	a0, 0x34			# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CFG_CMPREG2, a0)

	li	a0, 0x12			# Set MSB
	EV_SET_PROCREG(t0, t1, EV_CFG_CMPREG3, a0)

	dli	a0, EV_SYSCONFIG
	ld	a0, 0(a0)			# Stall until config write
						# completes...

	li	a0, 0x12345678			# Sign bit needs to be 0
	dli	a3, EV_RO_COMPARE
	ld	a1, 0(a3)			# Read back the test value
	nop
#ifndef SABLE
	bne 	a0, a1, ccconfig_fail
	nop
#endif

	move	a0, zero				# Set LSB
	EV_SET_PROCREG(t0, t1, EV_CFG_CMPREG0, a0)

	move	a0, zero				# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CFG_CMPREG1, a0)

	move	a0, zero				# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CFG_CMPREG2, a0)

	move	a0, zero				# Set MSB
	EV_SET_PROCREG(t0, t1, EV_CFG_CMPREG3, a0)

	dli	a3, EV_RO_COMPARE
	ld	a1, 0(a3)			# Read back the test value
	nop
#ifndef SABLE
	bne	a0, a1, ccconfig_fail
	nop
#endif
	j	v1				# Return address is in v1
	move	v0, zero			# Return value (BD)

END(pod_check_ccconfig)

LEAF(ccconfig_fail)
	jal	set_cc_leds		# Set the LEDs
	li	a0, PLED_CCCFAILED_INITUART	# (BD)
	jal	ccuart_init	        # Call the UART configuration code
	nop				# (BD)
	j	flash_cc_leds
	ori	a0, zero, FLED_CCCONFIG	# Load flash value (BD)
	END(ccconfig_fail)

LEAF(pod_check_achip)
	.set	noreorder
	move	v1, ra			# Save return address

	EV_GET_SPNUM(a0, a1)		# a0 = slot number
					# a1 = processor number
	dla	v0,aConfigTest
	dla	v1,aConfigTestEnd	
1:
	bgeu	v0,v1,2f
        nop
	ld	a1,CCT_ADDRESS(v0)
	ld	t0,CCT_VALUE(v0)
	EV_SET_CONFIG(a0, a1, t0)
	EV_GET_CONFIG(a0, a1, t1)
	add	v0,CCT_SIZE
	beq	t0,t1,1b
	nop
	/* 
	 * Fall through to here if we failed.
	 */
	FLASH(FLED_ACHIP)
2:	/* All OK */
	j	ra
	move	v0, zero		/* DELAY: return(0) */
END(pod_check_achip)
