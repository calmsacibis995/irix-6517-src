/* 
 * pod_cc.s -
 *	POD CPU board tests.
 */

#include <regdef.h> 
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "prom_leds.h"
#include "prom_intr.h"
#include "ip19prom.h"


#define A5

/* Constants for register testing: */
#define	BF_31TO0	0xffffffff
#define BA_31TO0	0xaaaaaaaa
#define B5_31TO0	0x55555555

#define BF_19TO0	0x000fffff
#define BA_19TO0	0x000aaaaa
#define B5_19TO0	0x00055555

#define BF_15TO0	0x0000ffff
#define BA_15TO0	0x0000aaaa
#define B5_15TO0	0x00005555

#define BF_10TO0	0x000007ff
#define BA_10TO0	0x000002aa
#define B5_10TO0	0x00000555

#define BF_7TO0		0x000000ff
#define BA_7TO0		0x000000aa
#define B5_7TO0		0x00000055

/* Read/write/compare macro-
 * 	addr = 		address to test (register)
 *	value = 	value to read/write (register)
 * 	fail_label = 	address to branch to if the comparison fails (label)
 * Register usage:
 * 	Trashes k0.
 */

#define RWCOMP(addr, value, fail_label)				\
	sd	value, 0(addr);					\
	ld	t0, 0(addr);					\
	nop;							\
	bne	value, t0, fail_label;				\
	nop;

#define CONF_RWCOMP(s, r, v, fail_label)		\
	EV_SET_CONFIG(s, r, v);				\
	EV_GET_CONFIG(s, r, t1);			\
	bne	v, t1, fail_label;			\
	nop

	.text
	.set	noreorder

/* a0 = address, a1 = value */
LEAF(ccregrwcomp)
	RWCOMP(a0, a1, ccreg_fail)
	j	ra
	nop
	END(ccregrwcomp)

/* a0 = address, a1 = value */
LEAF(ccconfig_rwcomp)
	RWCOMP(a0, a1, ccconfig_fail)
	j	ra
	nop
	END(ccconfig_rwcomp)

/* a0 = slot, a1 = regnum, a2 = value */
LEAF(achip_rwcomp)
	CONF_RWCOMP(a0, a1, a2, achip_fail)
	j	ra
	nop
	END(achip_rwcomp)

LEAF(pod_check_cclocal)
	move	v1, ra			# Save return address

	li	a0, EV_WGDST

	li	a1, BF_15TO0
	dsll	a1, a1, 16
	ori	a1, BF_15TO0		# now BF_31TO0

	dsll	a1, a1, 8		# Change to BF_39TO8
	jal	ccregrwcomp		# Write Fs
	nop
#ifdef A5

	li	a1, BA_15TO0
	dsll	a1, a1, 16
	ori	a1, BA_15TO0		# now BA_31TO0

	dsll	a1, a1, 8		# Change to BA_39TO8
	jal	ccregrwcomp		# Write As
	nop


	li	a1, B5_15TO0
	dsll	a1, a1, 16
	ori	a1, B5_15TO0		# now B5_31TO0

	dsll	a1, a1, 8		# Change to B5_39TO8
	jal	ccregrwcomp		# Write 5s
	nop
#endif
	li	a1, 0
	jal	ccregrwcomp		# Write 0s
	nop

	li	a0, EV_WGCNTRL
	li	a1, 3
	jal	ccregrwcomp		# Write 11
	nop

	li	a1, 1
#ifdef A5
	jal	ccregrwcomp		# Write 01
	nop

	li	a1, 2
	jal	ccregrwcomp		# Write 10
	nop
#endif
	li	a1, 0
	jal	ccregrwcomp		# Write 00
	nop

	li	a0, EV_CEL		# CEL is now 7 bits...
	li	a1, 0x7f
	jal	ccregrwcomp		# Write Fs
	nop
#ifdef A5
	li	a1, 0x2a
	jal	ccregrwcomp		# Write As
	nop
	li	a1, 0x55
	jal	ccregrwcomp		# Write 5s
	nop
#endif
	li	a1, 0
	jal	ccregrwcomp		# Write 0s
	nop

	li	a0, EV_IP0
	li	a1, BF_31TO0
	dsll	a1, a1, 16		# BF_47TO16
	ori	a1, BF_15TO0		# BF_47TO0
	dsll	a1, a1, 16		# BF_63TO16
	ori	a1, BF_15TO0		# BF_63TO0
	jal	ccregrwcomp		# Write Fs
	nop
#ifdef A5
	li	a1, BA_31TO0
	dsll	a1, a1, 16		# BA_47TO16
	ori	a1, BA_15TO0		# BA_47TO0
	dsll	a1, a1, 16		# BA_63TO16
	ori	a1, BA_15TO0		# BA_63TO0
	jal	ccregrwcomp		# Write As
	nop

	li	a1, B5_31TO0
	dsll	a1, a1, 16		# B5_47TO16
	ori	a1, B5_15TO0		# B5_47TO0
	dsll	a1, a1, 16		# B5_63TO16
	ori	a1, B5_15TO0		# B5_63TO0
	jal	ccregrwcomp		# Write 5s
	nop
#endif
	li	a1, 0
	jal	ccregrwcomp		# Write 0s
	nop

	li	a0, EV_IP1
	li	a1, BF_31TO0
	dsll	a1, a1, 16		# BF_47TO16
	ori	a1, BF_15TO0		# BF_47TO0
	dsll	a1, a1, 16		# BF_63TO16
	ori	a1, BF_15TO0		# BF_63TO0
	jal	ccregrwcomp		# Write Fs
	nop

#ifdef A5
	li	a1, BA_31TO0
	dsll	a1, a1, 16		# BA_47TO16
	ori	a1, BA_15TO0		# BA_47TO0
	dsll	a1, a1, 16		# BA_63TO16
	ori	a1, BA_15TO0		# BA_63TO0
	jal	ccregrwcomp		# Write As
	nop

	li	a1, B5_31TO0
	dsll	a1, a1, 16		# B5_47TO16
	ori	a1, B5_15TO0		# B5_47TO0
	dsll	a1, a1, 16		# B5_63TO16
	ori	a1, B5_15TO0		# B5_63TO0
	jal	ccregrwcomp		# Write 5s
	nop
#endif
	li	a1, 0
	jal	ccregrwcomp		# Write 0s
	nop

	li	a0, EV_IP0		# a0 = IP0 addr
	li	a1, 1
	sd	a1 0(a0)		# EV_IP0 = 1
	li	a2, EV_CIPL0
	sd	zero, 0(a2)		# Clear level 0 intr., prior 0
	nop
	ld	a3, 0(a0)		# a3 = EV_IP0
	nop
	bne	a3, zero, ccreg_fail	# Should have been cleared
	nop

	li	a0, EV_IP1		# a0 = IP1 addr
	li	a1, 1
	sd	a1 0(a0)		# EV_IP1 = 1
	li	a2, EV_CIPL0
	li	a3, 64
	sd	a3, 0(a2)		# Clear level 0 intr., prior 64
	nop
	ld	a3, 0(a0)		# a3 = EV_IP1
	nop
	bne	a3, zero, ccreg_fail	# Should have been cleared
	nop

	li	a0, EV_IGRMASK
	li	a1, BF_15TO0
	jal	ccregrwcomp		# Write Fs
	nop
#ifdef A5
	li	a1, BA_15TO0
	jal	ccregrwcomp		# Write As
	nop
	li	a1, B5_15TO0
	jal	ccregrwcomp		# Write 5s
	nop
#endif
	li	a1, 0
	jal	ccregrwcomp		# Write 0s
	nop

	li	a0, EV_ILE
	li	a1, 0x01
	jal	ccregrwcomp		# Write 1
	nop
	li	a1, 0x08
	jal	ccregrwcomp		# Write 8
	nop
	li	a1, 0x10
	jal	ccregrwcomp		# Write 16
	nop

	li	a1, 0
	jal	ccregrwcomp		# Write 0
	nop
#ifdef ACHIP
	li	a0, EV_ERTOIP
	li	a1, 0
	jal	ccregrwcomp		# Write 0s
	nop
#ifdef A5
	li	a1, BA_10TO0
	jal	ccregrwcomp		# Write As
	nop

	li	a1, B5_10TO0
	jal	ccregrwcomp		# Write 5s
	nop
#endif
	li	a1, BF_10TO0
	jal	ccregrwcomp		# Write Fs
	nop
#endif /* ACHIP */
	li	a2, EV_CERTOIP
	li	a1, BF_10TO0
	/* li	a0, EV_ERTOIP */
	sd	a1, 0(a2)		# Clear EV_ERTOIP
	ld	a3, 0(a0)		# a3 = EV_ERTOIP
	nop
	bne	a3, zero, ccreg_fail
	nop

	li	a0, EV_ECCSB_DIS
	li	a1, 1
	jal	ccregrwcomp		# Write 1
	nop

	li	a1, 0
	jal	ccregrwcomp		# Write 0
	nop

	j	v1			# Return address is in v1
	move	v0, zero		# Return value (BD)
	END(pod_check_cclocal)

LEAF(ccreg_fail)
#ifdef NEWTSIM
	FAIL
#endif
	jal	set_cc_leds		# Set the LEDs
	li	a0, PLED_CCLFAILED_INITUART	# (BD)
	jal	ccuart_init        	# Call the UART configuration code
	nop				# (BD)
	j	flash_cc_leds
	ori	a0, zero, FLED_CCLOCAL	# Load flash value (BD)
	END(ccreg_fail)

LEAF(clock_fail)
	jal	set_cc_leds		# Set the LEDs
	li	a0, PLED_NOCLOCK_INITUART	# (BD)
	jal	ccuart_init 	        # Call the UART configuration code
	nop				# (BD)
	j	flash_cc_leds
	ori	a0, zero, FLED_CCCLOCK	# Load flash value (BD)
	END(clock_fail)

LEAF(pod_check_ccconfig)

	move	v1, ra			# Save return address

	# Start the clock by sending the sync interrupt

	li	a0, EV_SENDINT
	li	a1, SYNC_DEST		# Clock sync interrupt number
	sd	a1, 0(a0)

	# Get our slot and processor number.

	EV_GET_SPNUM(t0, t1)		# t0 = slot number
					# t1 = processor number

	# Is my board listed in the system configuration information?

	li	a0, EV_SYSCONFIG
	ld	a1, 0(a0)		# a1 = system configuration bit vector
	li	a2, 1
	sll	a2, a2, t0		# 1 in position of board in bit vector
	sll	a3, a2, 16		# 1 in position of CPU board in vector
	and	k0, a1, a2
	bne	k0, a2, ccconfig_fail
	and	k1, a3, a1
	bne	k1, a3, ccconfig_fail
	nop

	# Make sure the clock is running

	li	a0, EV_RTC
	ld	a1, 0(a0)		# a1 = time in nanoseconds
	nop

	li	a0, EV_SYSCONFIG
	ld	a0, 0(a0)		# Give the poor guy a chance...

	li	a0, EV_RTC
	ld	a2, 0(a0)		# a2 = time a little later 
	nop	
	beq	a1, a2, clock_fail	# if the time hasn't changed, fail
	nop

	# Test timer config registers

	li	a0, 0x78			# Set LSB
	EV_SET_PROCREG(t0, t1, EV_CMPREG0, a0)

	li	a0, 0x56			# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CMPREG1, a0)

	li	a0, 0x34			# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CMPREG2, a0)

	li	a0, 0x12			# Set MSB
	EV_SET_PROCREG(t0, t1, EV_CMPREG3, a0)

	li	a0, EV_SYSCONFIG
	ld	a0, 0(a0)			# Stall until config write
						# completes...

	li	a0, 0x12345678			# Sign bit needs to be 0

	li	a3, EV_RO_COMPARE
	ld	a1, 0(a3)			# Read back the test value
	nop
	bne 	a0, a1, ccconfig_fail
	nop

	move	a0, r0				# Set LSB
	EV_SET_PROCREG(t0, t1, EV_CMPREG0, a0)

	move	a0, r0				# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CMPREG1, a0)

	move	a0, r0				# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CMPREG2, a0)

	move	a0, r0				# Set MSB
	EV_SET_PROCREG(t0, t1, EV_CMPREG3, a0)

	li	a3, EV_RO_COMPARE
	ld	a1, 0(a3)			# Read back the test value
	nop
	bne	a0, a1, ccconfig_fail
	nop
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
	move	v1, ra				# Save return address

	EV_GET_SPNUM(a0, a1)		# a0 = slot number
					# a1 = processor number

	li	a1, EV_A_URGENT_TIMEOUT
	jal	achip_rwcomp		# Write Fs
	ori	a2, zero, BF_15TO0	# (BD)

	li	a1, EV_A_URGENT_TIMEOUT
	jal	achip_rwcomp		# Write As
	ori	a2, zero, BA_15TO0	# (BD)

	li	a1, EV_A_URGENT_TIMEOUT
	jal	achip_rwcomp		# Write 5s
	ori	a2, zero, B5_15TO0	# (BD)

	li	a1, EV_A_URGENT_TIMEOUT
	jal	achip_rwcomp		# Write 0s
	move	a2, zero		# (BD)

	li	a1, EV_A_RSC_TIMEOUT
	li	a2, BF_19TO0
	jal	achip_rwcomp		# Write Fs
	nop

	li	a1, EV_A_RSC_TIMEOUT
	li	a2, BA_19TO0
	jal	achip_rwcomp		# Write As
	nop

	li	a1, EV_A_RSC_TIMEOUT
	li	a2, B5_19TO0
	jal	achip_rwcomp		# Write 5s
	nop

	li	a1, EV_A_RSC_TIMEOUT
	jal	achip_rwcomp		# Write 0s
	move	a2, zero		# (BD)

	j	v1
	move	v0, zero		# Return value = 0 (success) (BD)
END(pod_check_achip)
	
LEAF(achip_fail)
	j	flash_cc_leds
	ori	a0, zero, FLED_ACHIP	# Load flash value (BD)
	END(achip_fail)

