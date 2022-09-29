/***********************************************************************\
*	File:		pod_ioasm.s					*
*									*
*	Contains very low-level I/O routines for assembly language.	*
*	These routines examine the contents of the BSR and switch 	*
*	to the appropriate device-specific I/O routine.			*
*									*
\***********************************************************************/

#include "ml.h"
#include <sys/regdef.h>
#include <asm.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/epc.h>
#include "ip21prom.h"

	.text
	.set 	noreorder

LEAF(pod_puthex)
	GET_BSR(a1)				# Read the BSR
	andi	a1, BS_USE_EPCUART		# Check to for EPC UART
	bnez	a1, 1f
	nop
	j	ccuart_puthex			# Call SysCtlr UART code
	nop
1:	j	epcuart_puthex			# Call EPC UART code
	ori	a1, zero, CHN_A			# (BD) Write to channel A	
	END(pod_puthex)

LEAF(pod_puts)
	GET_BSR(a1)				# Read the BSR
	and	a1, BS_USE_EPCUART		# Check for EPC UART use
	bnez	a1, 1f
	nop					# (BD)
	j	ccuart_puts
	nop
1:	j	epcuart_puts
	ori	a1, zero, CHN_A			# (BD) Write to channel A	
	END(pod_puts)

LEAF(pod_getc)
	GET_BSR(a1)				# Read the BSR
	andi	a1, BS_USE_EPCUART
	bnez	a1, 1f
	nop					# (BD)
	j	ccuart_getc
	nop
1:	j	epcuart_getc
	ori 	a0, zero, CHN_A			# (BD) Read from channel A	
	END(pod_getc)

LEAF(pod_putc)
	GET_BSR(a1)				# Read the BSR
	andi	a1, BS_USE_EPCUART		# Check for which UART to use
	bnez	a1, 1f
	nop					# (BD)
	j	ccuart_putc
	nop
1:	j	epcuart_putc
	ori	a1, zero, CHN_A			# (BD) Write to channel A	
	END(pod_putc)

LEAF(pod_flush)
	GET_BSR(a0)				# Read the BSR
	andi	a0, BS_USE_EPCUART		# Check for which UART to use
	bnez	a0, 1f				# Jump if not CCUART
	nop					# (BD)
	j	ccuart_flush
	nop
1:	j	epcuart_flush
	ori	a0, zero, CHN_A			# (BD) Flush channel A	
	END(pod_flush)

LEAF(pod_poll)
	GET_BSR(a1)				# Read the BSR
	andi	a1, BS_USE_EPCUART
	bnez	a1, 1f
	nop					# (BD)
	j	ccuart_poll	
	nop
1:	j	epcuart_poll
	ori	a0, zero, CHN_A			# (BD) Poll channel A	
	END(pod_poll)
