
#ident	"lib/libsk/ml/EVERESTasm.s:  $Revision: 1.12 $"

/*
 * EVERESTasm.s - EVEREST specific assembly language functions
 */

#include <ml.h>
#include <asm.h>
#include <regdef.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/evmp.h>
#include <sys/i8251uart.h>

#ifdef TFP
#include <sys/EVEREST/IP21.h>
#include <sys/EVEREST/IP21addrs.h>
#elif IP19
#include <sys/EVEREST/IP19addrs.h>
#elif IP25
#include <sys/EVEREST/IP25addrs.h>
#endif

/*
 * wbflush() -- spin until write buffer empty
 */
LEAF(wbflush)
XLEAF(flushbus)
	LI	t0,EV_SPNUM
	ld	zero,0(t0)
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
#if IP21
	or	t0, SR_IE
#elif _MIPS_SIM == _ABI64
	or	t0, SR_KX
#endif
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
	.set	noat


/*
 * Routine ccuart_poll 
 *	Poll the CC UART to see if a charactre is available
 *
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- Nonzero if a character is available, zero otherwise
 */

LEAF(ccuart_poll) 
	LI	v1, EV_UART_BASE
	ld	v0, CMD(v1) 		#   Get status bit
	nop
	j	ra
	andi	v0, I8251_RXRDY
	END(ccuart_poll)

/*
 * Routine ccuart_getc
 *	Reads a single character from the CC UART.
 *
 * Arguments:
 * 	None.
 * Returns:
 *	v0 -- The character read.  
 */

LEAF(ccuart_getc)
	LI	v1, EV_UART_BASE
1:					# 
	ld	v0, CMD(v1) 		# DO 
	nop
	andi	v0, I8251_RXRDY			
	beqz	v0, 1b			# WHILE (not ready && 
	nop				# (BD)
	
	j	ra			# Return 
	ld	v0, DATA(v1)		# (BD) Read character from register
	END(ccuart_getc)


/*
 * Routine ccuart_putc
 * 	Writes a single character to the CC UART.
 *
 * Arguments:
 *	a0 -- ASCII value of character to be written
 * Returns:
 *	v0 -- If non-zero, putc succeeded.	
 */

LEAF(ccuart_putc)
	LI	a1, EV_UART_BASE
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
	END(ccuart_putc)


/*
 * Routine ccuart_flush
 *	Flushes the input buffer of the UART.
 *
 * Arguments:
 *	None.
 * Returns:
 *	Nothing.
 */

LEAF(ccuart_flush)
	LI	v0, EV_UART_BASE	
	ld	v1, CMD(v0)		# Read status bits
	and	v1, I8251_RXRDY		# IF character available
	beq	v1, zero, 1f		# THEN
	nop
	ld	zero, DATA(v0)		#   read character
	nop
1:					# ENDIF
	j	ra
	nop 
	END(ccuart_flush)

/* 
 * Routine ccuart_puthex
 *	Writes a 64-bit hexadecimal value to the CC UART.
 *
 * Arguments:
 *	a0 -- the 64-bit hex number to write.
 * Returns:
 *	v0 -- Success.  Non-zero indicates write successful.
 */

LEAF(ccuart_puthex)
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
	jal     ccuart_putc	# Print it
	lb	a0, 0(ta3)	# (BD) Read byte	
	sub     t8,1		# Entire number printed?
	bne     t8,zero,2b	# 
	dsll	ta1,4		# (BD) Set up next nibble

	j       t9		# Yes - done
	nop
	END(ccuart_puthex)


/*
 * Routine ccuart_puts
 *	Writes a null-terminated string to the CC UART.
 *
 * Arguments:
 *	a0 -- the address of the first character of the string.
 * Returns:
 *	v0 -- If non-zero, puts (probably) succeeded.  If zero,
 *	      puts definitely failed. 
 */

LEAF(ccuart_puts)
	.set	noreorder
	move	t9, ra			# Save the return address
	move	t8, a0
1:
	lb	a0, 0(t8)		# Load the char to be printed
	nop				# 
        beq     a0, zero, 3f		# If char == zero, return
        nop				# (BD)
        jal     ccuart_putc		# 
        nop				# (BD)

        b       1b			# Loop back up
       	PTR_ADDU t8, 1			# (BD) Increment the string pointer
3:
        j       t9			# Return
        nop				# (BD)
	END(ccuart_puts)

	.data
hexdigit:
	.asciiz	"0123456789abcdef"

	.text
/*
 * Function:	get_mpconf
 * Purpose:	To recover the mpconf structure for this CPU.
 * Returns:	Pointer to MPconf for this CPU or 0.
 * Notes:	This function ONLY KILLS T registers, + v0 and v1.
 */
LEAF(get_mpconf)
	.set	noreorder
	
	LI	t0,EV_SPNUM
	ld	t0,0(t0)
	and	t0,EV_SPNUM_MASK
	
	LI	t2,MPCONF_SIZE*EV_MAX_CPUS+MPCONF_ADDR
	LI	t1,MPCONF_ADDR

	lui	v0,(MPCONF_MAGIC >> 16)
	ori	v0,(MPCONF_MAGIC & 0xffff)

1:
	lw	v1,MP_MAGICOFF(t1)	/* Magic # */
	bne	v1,v0,2f
	nop
	lbu	v1,MP_PHYSID(t1)
	beq	v1,t0,3f
	nop
2:	
	PTR_ADD	t1,MPCONF_SIZE
	bltu	t1,t2,1b
	nop
	move	t1,zero			/* Not found */
3:
	move	v0,t1
	j	ra
	nop

	.set	reorder
	END(get_mpconf)


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
#if DS32_WAR
	dsrl	v0,16	
	dsrl	v0,16
#else
	dsrl	v0,32
#endif
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
 * Handy routines to access > 32bit address without doing TLB mapping.
 * Borrowed from IP19prom/libasm.s
 */
/*
        int u64lw(int bloc, int offset)
        a0 = bloc
        a1 = offset
        *v0 := *(bloc <<8 + offset)
	NOTE: KX bit in status register should be set before calling this.
*/
							
LEAF(u64lw)
        .set    noreorder
#if IP19
	li	v0, 0x00100000		# Start of processor local stuff
	li	v1, 0xfff00000		# Mask off bottom 20 bits of bloc
	and	v1, v1, a0
	bne	v0, v1, 1f		# Address covered by proc. local stuff?
	lui	v1, 0x0ff0		# Load address of hidden RAM (BD)
	or	a0, v1, a0		# Change address to 40'h0ff0000000)
1:
#endif
        dsll	a0, 8                   # a0 := Bits 39-8 of the address (BD)
        daddu   a0, a1                  # a0 := Full physical address

	# dli doesn't work in PROM so load start of uncached kphysseg space
	lui	a1, 0x9000		# a1 = 0x90000000
	dsll	a1, a1, 16		# a1 = 0x900000000000
	dsll	a1, a1, 16		# a1 = 0x9000000000000000

        or      a0, a1, a0              # Uncached address of word
        lw      v0, 0(a0)               # Do load.
        j       ra
        nop				# (BD)
        .set    reorder
END(u64lw)


/*
        int u64sw(int bloc, int offset, int data)
        a0 = bloc
        a1 = offset
	a2 = data
	NOTE: KX bit in status register should be set before calling this.
*/

LEAF(u64sw)
        .set    noreorder
#if IP19
	li	v0, 0x00100000		# Start of processor local stuff
	li	v1, 0xfff00000		# Mask off bottom 20 bits of bloc
	and	v1, v1, a0		
	bne	v0, v1, 1f		# Address covered by proc. local stuff?
	lui	v1, 0x0ff0		# Load address of hidden RAM (BD)
	or	a0, v1, a0		# Change address to 40'h0ff0000000)
1:
#endif
        dsll    a0, 8                   # a0 := Bits 39-8 of the address
        daddu   a0, a1                  # a0 := Full physical address

	# dli doesn't work in PROM so load start of uncached kphysseg space
	lui	a1, 0x9000		# a1 = 0x90000000
	dsll	a1, a1, 16		# a1 = 0x900000000000
	dsll	a1, a1, 16		# a1 = 0x9000000000000000

        or      a0, a1, a0              # Uncached address of word
        sw      a2, 0(a0)               # Do store
        j       ra
        nop
        .set    reorder
END(u64sw)

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
 * uint load_lwin_half_store_mult(int window, int offset, char *dest,
				  int numtocopy)
 * Copy multiple bytes from little IO window.
 * Returns checksum.
 * For fast IO4prom copy.
 */
LEAF(load_lwin_half_store_mult)
	.set    noreorder
	move    v0, zero        # init checksum
	daddi   a0, 0x10        # Convert to physical offset
	dsll    a0, 30          # Shift it up
	dadd    a0, a1          # Merge in the offset
	lui     ta0, 0x9000
	dsll    ta0, 32         # Build the uncached xkphys address
	dadd    a0, ta0         # Make LWIN into a real address
1:
	lhu     t0, 0(a0)       # Perform the actual read
	addu    v0, t0
	daddi   a0, 2           # next short
	sh      t0, 0(a2)       # Perform the actual write
	addi    a3, -2          # decrement count
	bgt     a3, 0, 1b       # loop if not done
	daddi   a2, 2           # (BD) next short

	j       ra              # Return
	nop
	END(load_lwin_half_store_mult)

#ifdef TFP
LEAF(read_revision)
	LI	v0, EV_IP21_REV
	ld	v0, 0(v0)
	j	ra
	END(read_revision)

LEAF(write_vcr)
	LI	v0, EV_VOLTAGE_CTRL
	sd	a0,0(v0)
	j	ra
	END(write_vcr)
#endif
