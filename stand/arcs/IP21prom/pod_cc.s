/*
 * pod_cc.s -
 *	POD CPU board tests.
 */



#include "ml.h"
#include <sys/regdef.h> 
#include <asm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include "ip21prom.h"
#include "prom_leds.h"
#include "prom_intr.h"
#include "pod.h"
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evdiag.h>
#include <sys/EVEREST/IP21addrs.h>


#define RESET_CCJOIN   0x8040  /* value write to reset cc join reg */
#define INCR_CCJOIN    0xC040  /* value write to increment cc join reg */

#define WGBASE          0x8000000018300000
#define DEST_ADDR       0xf00000 	/* uncached,unmapped physical addr */
#define WGEMPTY_MASK    0x3f3f
#define WGTEST_PATTERN  0x01111111

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

#define RWCOMP(addr, value, fail_label)			\
	sd	value, 0(addr);				\
	ld	t0, 0(addr);				\
	bne	value, t0, fail_label;			\
	nop;

#ifdef SABLE

#define CONF_RWCOMP(s, r, v, fail_label)		\
	EV_SET_CONFIG(s, r, v);				\
	EV_GET_CONFIG(s, r, t1);			\
	nop
#else

#define CONF_RWCOMP(s, r, v, fail_label)		\
	and	v, 0xff; /* only 8 valid bits */	\
	EV_SET_CONFIG(s, r, v);				\
	EV_GET_CONFIG(s, r, t1);			\
	and	t1, 0xff; /* only 8 valid bits */	\
	bne	v, t1, fail_label;			\
	nop

#endif

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

	dli	a0, EV_WGDST

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

#ifndef SABLE
	dli	a0, EV_CEL		# CEL is now 7 bits...
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
#endif
	li	a1, 0
	jal	ccregrwcomp		# Write 0s
	nop

	dli	a0, EV_IP0
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

	dli	a0, EV_IP1
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

	dli	a0, EV_IP0		# a0 = IP0 addr
	li	a1, 1
	sd	a1 0(a0)		# EV_IP0 = 1
	dli	a2, EV_CIPL0
	sd	zero, 0(a2)		# Clear level 0 intr., prior 0
	nop
	ld	a3, 0(a0)		# a3 = EV_IP0
	nop
	bne	a3, zero, ccreg_fail	# Should have been cleared
	nop

	dli	a0, EV_IP1		# a0 = IP1 addr
	li	a1, 1
	sd	a1 0(a0)		# EV_IP1 = 1
	dli	a2, EV_CIPL0
	li	a3, 64
	sd	a3, 0(a2)		# Clear level 0 intr., prior 64
	nop
	ld	a3, 0(a0)		# a3 = EV_IP1
	nop
	bne	a3, zero, ccreg_fail	# Should have been cleared
	nop

	dli	a0, EV_IGRMASK
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

	dli	a0, EV_ILE
	li	a1, 0x01
	jal	ccregrwcomp		# Write 1
	nop

	li	a1, 0
	jal	ccregrwcomp		# Write 0
	nop
#ifdef ACHIP
	dli	a0, EV_ERTOIP
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
	dli	a2, EV_CERTOIP
	li	a1, BF_10TO0
	/* li	a0, EV_ERTOIP */
	sd	a1, 0(a2)		# Clear EV_ERTOIP
	ld	a3, 0(a0)		# a3 = EV_ERTOIP
	nop
	bne	a3, zero, ccreg_fail
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
	bne	k0, a2, ccconfig_fail
	and	k1, a3, a1
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
	EV_SET_PROCREG(t0, t1, EV_CMPREG0, a0)

	li	a0, 0x56			# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CMPREG1, a0)

	li	a0, 0x34			# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CMPREG2, a0)

	li	a0, 0x12			# Set MSB
	EV_SET_PROCREG(t0, t1, EV_CMPREG3, a0)

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
	EV_SET_PROCREG(t0, t1, EV_CMPREG0, a0)

	move	a0, zero				# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CMPREG1, a0)

	move	a0, zero				# Set next MS byte
	EV_SET_PROCREG(t0, t1, EV_CMPREG2, a0)

	move	a0, zero				# Set MSB
	EV_SET_PROCREG(t0, t1, EV_CMPREG3, a0)

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
	move	v1, ra			# Save return address

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


/*
 * pod_check_ccjoin
 *
 *    for each processor, set it's proc group mask, get the proc
 *    interrupt number, from there, calculate the associated
 *    cc join counter register addr:
 *    CC join counter addr = 0x41a000100 +(interrupt group number << 20)
 *    write 0x8000 to RESET the counter. read back the value from addr
 *    only take data bit<63:56>, compare it.
 *    write 0xC000 to INCREMENT the counter. read back the value from addr
 *    only take data bit<63:56>, compare it.
 *    Perform this test on 15 different interrupt groups
 */

LEAF(pod_check_ccjoin)
	move	s2, ra

        jal     set_cc_leds
	ori    	a0, zero, PLED_CCJOIN   # Load flash value (BD)

	dla	a0, join_test_msg
	jal	pod_puts
	nop

	#we don't want the slave processors running ccjoin
	#test all at the same time, it will change ccjoin
	#reg value, based on slot,slice number, we give it
	# different delay count.

        dli     a0, EV_SPNUM    # Get the processor SPNUM
        ld      a0, 0(a0)       # Load slot and processor number
        and     a0, EV_SLOTNUM_MASK | EV_PROCNUM_MASK
        sll     a0, 18

8:      addi    a0, -1
        nop                     # Don't want the branch in the
        nop                     # same quad as the target for
        nop                     
        bgtz    a0, 8b
        nop


        dli     s7, EV_IGRMASK
        ld      t2, 0(s7)    /* save it for later */
	move	s5, zero     
	li	t3, 16		/* perform test on 15 interrupt groups */
10:
	subu	t3, 1
	dli	t0, 1
	dsll	t0, t0, t3	/* calculate interrupt group mask value */
        sd      t0, 0(s7)
        dli     t1, EV_SYNC_SIGNAL /* get cc join register base address */
        dsll    t0, t3, 20
        dadd    t0, t1		/* cc join register address for this interrupt group */

        li      a2, RESET_CCJOIN
        sd      a2, 0(t0)	/* write 0x8040 to cc join reg */
	
	# need some delay  
        li      t1, 0x3ff
1:
        sub     t1, 1
	nop
	nop
	nop
        bgtz    t1, 1b
        nop

        ld      t1, 0(t0)	/* read from cc join reg */
	dsrl	t1, 56
	move	a3, t1
        bne     t1, zero, 99f	/* should read back as zero for RESET */
        nop              

98:
	# Now, test cc join increment functionality
        li      a2, INCR_CCJOIN /* write 0xc040 to cc join reg */
        sd      a2, 0(t0)       

        # need some delay
        li      t1, 0x3ff 
1:
        sub     t1, 1
	nop
	nop
	nop
        bgtz    t1, 1b
        nop

        ld      a3, 0(t0)	/* read from cc join reg */
	dsrl	a3, 56
        li      t1, 1
        bne     t1, a3, 97f	/* cc join reg read value should be 1 */
	nop

	move	v0, zero
	subu	t3, 1 
	bgtz 	t3, 10b
	nop
        b	5f
        nop

97:
        dla     a0, join_incr_fail
        jal     pod_puts
        nop
        move    a0, t3          
        jal     pod_puthex
        nop
        dla     a0, crlf
        jal     pod_puts
        nop
#ifdef DEBUG
	# dump cc join reg address and cc join reg value
        dla     a0, ccreg
        jal     pod_puts
        nop
        move    a0, t0 
        jal     pod_puthex
        nop
	dla	a0, ccreg_value
	jal	pod_puts
	nop
	move	a0, a3
	jal	pod_puthex
        nop
        dla     a0, crlf
        jal     pod_puts
        nop
        dla     a0, crlf
        jal     pod_puts
        nop
#endif	
	daddiu	s5, 1		/* cc join incr failed */

	bgtz 	t3, 10b
	nop
	b 	5f
	nop

99:
	move	s6, t0		/* save cc join register addr */
        dla     a0, join_reset_fail
        jal     pod_puts
        nop
        move    a0, t3          
        jal     pod_puthex
        nop
        # dump cc join reg address and cc join reg value
        dla     a0, ccreg
        jal     pod_puts
        nop
        move    a0, t0
        jal     pod_puthex
        nop

        dla     a0, ccreg_value
        jal     pod_puts
        nop
        move    a0, a3
        jal     pod_puthex
        nop
        dla     a0, crlf
        jal     pod_puts
        nop
	daddiu	s5, 1		/* cc join reset failed counter */

	move	t0, s6		/* restore cc join register addr to t0 */
	b 	98b		/* after reset test, do incr test */
	nop

5:
        beqz    s5, 8f		/* error count of cc join test */
        nop

        sd      t2, 0(s7)    /* restore */ 
	dli	v0, EVDIAG_CCJOIN
        j       s2
	nop

8:
	dla	a0, join_test_pass
	jal	pod_puts
	nop
        sd      t2, 0(s7)    /* restore */
	dli	v0, 0
	j	s2
	nop

	END(pod_check_ccjoin)

	.data
join_test_msg:  .asciiz "Checking CC Join...                             ..."
join_test_pass: .asciiz "passed. \r\n"
join_reset_fail: .asciiz "\nCC join RESET fail at PROC INTR GRP "
join_incr_fail: .asciiz "CC join INCR fail at PRC INTR GRP "
ccreg_value:	.asciiz " value is "
ccreg:	.asciiz "  address is "
join_test_fail: .asciiz "...failed\n"
	.text

LEAF(pod_init_wg)
	dli	t0, EV_SPNUM	     	# Load the SPNUM addr 
	ld      t0, 0(t0)
	srl	t0, EV_CCREVNUM_SHFT
	andi	t0, 0x00ff
	bne	t0, 3, 1f		# don't init wg on rev 3 bbcc 
	nop
	j	ra
	li	v0, 0
1:
	move	a7, ra
        jal     set_cc_leds
	li    	a0, PLED_WG  

        dli     t1, WGTESTMEM_ADDR
        dli     t0, EV_WGDST
        sd      t1, 0(t0)       # point WRGDST to target memory

	li	t1, 128
	dli	a3, WGTEST_PATTERN # into the write gatherer
	dli	t2, WGBASE

wgloop:	addiu	t1, -1
	sw	a3, 0(t2)	
	bgtz	t1, wgloop
	nop

	dli	t2, WGBASE
	dli	t3, EV_WGSTATUS
	sw	zero, 0x20(t2)		# flush write gatherer buffer
	sw	zero, 0x28(t2)  	# Sync write gatherer buffer

floop:	ld	a1, 0(t3)		# read write gatherer buffer status
	andi	a1, WGEMPTY_MASK  	# check WGA and WGB word count
	bnez	a1, floop
	sw	zero, 0x28(t2)  	# Sync the write gatherer buffer

        j       a7
	li	v0, 0			# BDSLOT
END(pod_init_wg)
