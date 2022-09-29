
#ident	"SN0asm.s:  $Revision: 1.11 $"

/*
 * SN0asm.s - SN0 specific assembly language functions
 */

#include <ml.h>
#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/i8251uart.h>
#include <sys/SN/agent.h>
#include <sys/SN/addrs.h>

/*
 * wbflush() -- spin until write buffer empty
 */
LEAF(wbflush)
XLEAF(flushbus)
        LI      t0, LOCAL_HUB(MD_SLOTID_USTAT)
#if 0
	ld	zero,0(zero)	/* fixme: force an exception for now. XXX */
#endif
	j	ra
	END(wbflush)


/*
 * call_coroutine()
 * 	Switches stacks and calls a subroutine using the given stack
 * 	with the specified parameter.
 *
 * On entry:
 *	a0 -- The address of the subroutine to call.
 *	a1 -- The parameter to call the subroutine with.
 * 	a2 -- The stack to use for the subroutine.
 */

LEAF(call_coroutine)
	.set	reorder
	PTR_SUBU a2, 8			# Allocate enough space for ra and sp
	sreg	ra, BPREG(a2)		# Save the return address
	sreg	sp, 0(a2)		# Save the stack pointer

	move	t0, a0			# Move routine address to t0
	move	a0, a1			# Shift the parameter down
	move	sp, a2			# Change the stack pointer

	jal	t0			# Call the subroutine

	lreg	ra, BPREG(sp)		# Get the return address back
	lreg	sp, 0(sp)		# Restore the old stack pointer

	j	ra
	END(call_coroutine)

/* Primitive spl routines -- 
 * spl(int) : Set the Status register to given value
 *	    : return current setting
 */
LEAF(spl)
	.set	noreorder
	DMFC0(v0, C0_SR)
	LI	t0, SR_PAGESIZE
	or	a0, t0
	DMTC0(a0, C0_SR)
	nop
	j	ra
	nop
	.set	reorder
	END(spl)

/*
 * splerr() : Set SR to block errors - disable intrs
 */
LEAF(splerr)
	and	a0, zero		# disable everything
	j	spl			# Get current SR.
	END(splerr)

/*
 * Utility library routines for	manipulating the CC chip's LEDs and UART.
 */

#define         CMD     0x0
#define         DATA    0x8

	.set 	noreorder


/*
 * Routine ccuart_poll 
 *	Poll the hub UART to see if a charactre is available
 *
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- Nonzero if a character is available, zero otherwise
 */

LEAF(hubuart_poll) 
	LI	v1, LOCAL_HUB(MD_UREG0_0)
	ld	v0, CMD(v1) 		#   Get status bit
	nop
	j	ra
	andi	v0, I8251_RXRDY
	END(hubuart_poll)

/*
 * Routine hubuart_getc
 *	Reads a single character from the hub UART.
 *
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- The character read.  
 */

LEAF(hubuart_getc)
	LI	v1, LOCAL_HUB(MD_UREG0_0)
1:					# 
	ld	v0, CMD(v1) 		# DO 
	nop
	andi	v0, I8251_RXRDY			
	beqz	v0, 1b			# WHILE (not ready && 
	nop				# (BD)
	
	j	ra			# Return 
	ld	v0, DATA(v1)		# (BD) Read character from register
	END(hubuart_getc)


/*
 * Routine hubuart_putc
 * 	Writes a single character to the hub UART.
 *
 * Arguments:
 *	a0 -- ASCII value of character to be written
 * Returns:
 *	v0 -- If non-zero, putc succeeded.	
 */

LEAF(hubuart_putc)
	LI	a1, LOCAL_HUB(MD_UREG0_0)
#ifndef SABLE
	LI	v0, 1000000
1:					# DO
	ld	v1, CMD(a1)		#   Get status register
	nop
	andi	v1, I8251_TXRDY		# UNTIL ready to transmit 
	bne	v1, zero, 2f	 	# OR
	subu	v0, 1			# (BD) (--cnt == 0)
	bne	v0, zero, 1b
	nop
	j	ra			# paranoid return without storing
	nop
2:
#endif
	sd	a0, DATA(a1)		# (BD) Write char into xmit buf
	j	ra
	nop
	END(hubuart_putc)


/*
 * Routine hubuart_flush
 *	Flushes the input buffer of the UART.
 *
 * Arguments:
 *	None.
 * Returns:
 *	Nothing.
 */

LEAF(hubuart_flush)
	LI	v0, LOCAL_HUB(MD_UREG0_0)	
	ld	v1, CMD(v0)		# Read status bits
	and	v1, I8251_RXRDY		# IF character available
	beq	v1, zero, 1f		# THEN
	nop
	ld	zero, DATA(v0)		#   read character
	nop
1:					# ENDIF
	j	ra
	nop 
	END(hubuart_flush)

/* 
 * Routine hubuart_puthex
 *	Writes a 64-bit hexadecimal value to the hub UART.
 *
 * Arguments:
 *	a0 -- the 64-bit hex number to write.
 * Returns:
 *	v0 -- Success.  Non-zero indicates write successful.
 */

LEAF(hubuart_puthex)
	move    t9, ra
	LI	t8, 8 		# Number of digits to display (BD)
	LI	ta2, 0 
	move	ta1, a0		# Copy argument to ta1
2:
	dsrl	a0,ta1,30	# Rotate right 60 bits ($@!&*# assembler
	dsrl	a0,a0,30 	# 	won't assemble srl32 a0,ta1,28)
				# This isolates the rightmost nibble
	LA	ta3,hexdigit
	PTR_ADDU ta3,a0		# Calculate address of hex byte 
	jal     hubuart_putc	# Print it
	lb	a0, 0(ta3)	# (BD) Read byte	
	sub     t8,1		# Entire number printed?
	bne     t8,zero,2b	# 
	dsll	ta1,4		# (BD) Set up next nibble

	j       t9		# Yes - done
	nop
	END(hubuart_puthex)


/*
 * Routine hubuart_puts
 *	Writes a null-terminated string to the hub UART.
 *
 * Arguments:
 *	a0 -- the address of the first character of the string.
 * Returns:
 *	v0 -- If non-zero, puts (probably) succeeded.  If zero,
 *	      puts definitely failed. 
 */

LEAF(hubuart_puts)
	.set	noreorder
	move	t9, ra			# Save the return address
	move	t8, a0
1:
	lb	a0, 0(t8)		# Load the char to be printed
	nop				# 
        beq     a0, zero, 3f		# If char == zero, return
        nop				# (BD)
        jal     hubuart_putc		# 
        nop				# (BD)

        b       1b			# Loop back up
       	PTR_ADDU t8, 1			# (BD) Increment the string pointer
3:
        j       t9			# Return
        nop				# (BD)
	END(hubuart_puts)

	.data
hexdigit:
	.asciiz	"0123456789abcdef"

	.text

/*
 * long load_double_lo(uint*)
 *	This function uses the MIPS-3 ld instruction
 *	to read a 64-bit value.  Because of C's limitations,
 *	it only returns the least significant 32-bits.
 *
 * On entry:
 *	a0 -- The address to read from. 
 */
	.text
LEAF(load_double_lo)
	ld	v0,0(a0)
	j	ra
	END(load_double_lo)

/*
 * long load_double_hi(uint*)
 *	This function uses the MIPS-3 ld instruction
 *	to read a 64-bit value.  It then returns the 
 *	MSW.  It allows one to read the MSW in an
 *	endian-dependent way from C.
 *
 * On entry:
 *	a0 -- dword-aligned address to read from
 */
LEAF(load_double_hi)
	ld	v0,0(a0)
	dsrl	v0,32
	j	ra
	END(load_double_hi)


/*
 * void store_double_lo(uint*, uint)
 *	This function uses the MIPS-3 sd instruction
 *	to write a 32-bit value as a 64-bit transaction.
 *	This is useful for manipulating device registers in
 *	an endian-independent way.
 *
 * On entry:
 *	a0 -- The address to write to (better be dword-aligned)
 *	a1 -- The value to write.
 */
LEAF(store_double_lo)
	sd	a1,0(a0)
	j	ra
	END(store_double_lo)

/*
 * workaround ragnarok compiler interface problem
 */
LEAF(__dsrav)
	ld	a0,0(sp)
	ld	a1,8(sp)
	j	___dsrav
	END(__dsrav)

LEAF(__dsrlv)
	ld	a0,0(sp)
	ld	a1,8(sp)
	j	___dsrlv
	END(__dsrlv)

LEAF(__dsllv)
	ld	a0,0(sp)
	ld	a1,8(sp)
	j	___dsllv
	END(__dsllv)

/*
 * lo_printf is used in libkl and defined in ip27prom. If anyone else
 * is linking against libkl and also linking against libkl, give them
 * the real printf.
 */
LEAF(lo_printf)
	j	printf
	END(lo_printf)	
