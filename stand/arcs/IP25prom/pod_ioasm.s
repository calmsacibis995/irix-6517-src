/***********************************************************************\
*	File:		pod_ioasm.s					*
*									*
*	Contains very low-level I/O routines for assembly language.	*
*	These routines examine the contents of the BSR and switch 	*
*	to the appropriate device-specific I/O routine.			*
*									*
\***********************************************************************/

#include <asm.h>

#include <sys/regdef.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <sys/mips_addrspace.h>

#include <sys/EVEREST/IP25.h>
#include <sys/EVEREST/epc.h>

#include "ip25prom.h"
#include "prom_intr.h"
#include "pod_failure.h"

	.text
	.set 	noreorder

LEAF(pod_puthex)
	DMFBR(a1, BR_BSR)			# Read the BSR
	andi	a1, BSR_EPCUART			# Check to for EPC UART
	bnez	a1, 1f
	nop
	j	ccuart_puthex			# Call SysCtlr UART code
	nop
1:	
	ori	a1, zero, CHN_A			# (BD) Write to channel A
	j	epcuart_puthex			# Call EPC UART code
	nop
	END(pod_puthex)

LEAF(pod_puthex32)
	move	a3,ra				/* save way home */
	move	ta3,a0				/* Save Value */
	li	ta2,32				/* Shift amount */
1:
	sub	ta2,4
	jal	getHexChar
	srl	a0,ta3,ta2
	jal	pod_putc
	move	a0,v0
	bnez	ta2,1b
	nop
	j	a3
	END(pod_puthex32)

LEAF(pod_puthex64)
	move	a3,ra				/* save way home */
	move	ta3,a0				/* Save Value */
	li	ta2,64				/* Shift amount */
1:
	sub	ta2,4
	jal	getHexChar
	dsrl	a0,ta3,ta2
	jal	pod_putc
	move	a0,v0
	bnez	ta2,1b
	nop
	j	a3
	END(pod_puthex64)

LEAF(pod_puts)
	DMFBR(a1, BR_BSR)			# Read the BSR
	and	a1, BSR_EPCUART			# Check for EPC UART use
	bnez	a1, 1f
	nop					# (BD)
	j	ccuart_puts
	nop
1:	j	epcuart_puts
	ori	a1, zero, CHN_A			# (BD) Write to channel A	
	END(pod_puts)

LEAF(pod_getc)
	DMFBR(a1, BR_BSR)			# Read the BSR
	andi	a1, BSR_EPCUART
	bnez	a1, 1f
	nop					# (BD)
	j	ccuart_getc
	nop
1:	j	epcuart_getc
	ori 	a0, zero, CHN_A			# (BD) Read from channel A	
	END(pod_getc)

LEAF(pod_putc)
	DMFBR(a1, BR_BSR)			# Read the BSR
	andi	a1, BSR_EPCUART			# Check for which UART to use
	bnez	a1, 1f
	nop					# (BD)
	j	ccuart_putc
	nop
1:	j	epcuart_putc
	ori	a1, zero, CHN_A			# (BD) Write to channel A	
	END(pod_putc)

LEAF(pod_flush)
	DMFBR(a0, BR_BSR)			# Read the BSR
	andi	a0, BSR_EPCUART			# Check for which UART to use
	bnez	a0, 1f				# Jump if not CCUART
	nop					# (BD)
	j	ccuart_flush
	nop
1:	j	epcuart_flush
	ori	a0, zero, CHN_A			# (BD) Flush channel A	
	END(pod_flush)

LEAF(pod_poll)
	DMFBR(a1, BR_BSR)			# Read the BSR
	andi	a1, BSR_EPCUART
	bnez	a1, 1f
	nop					# (BD)
	j	ccuart_poll	
	nop
1:	j	epcuart_poll
	ori	a0, zero, CHN_A			# (BD) Poll channel A	
	END(pod_poll)
