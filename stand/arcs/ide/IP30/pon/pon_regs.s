#ident	"ide/IP30/pon/pon_regs.s:  $Revision: 1.18 $"

#include "sys/asm.h"
#include "early_regdef.h"
#include "sys/PCI/PCI_defs.h"
#include "sys/PCI/bridge.h"
#include "sys/PCI/ioc3.h"
#include "sys/ds1687clk.h"
#include "sys/cpu.h"
#include "sys/RACER/IP30nvram.h"
#include "sys/sbd.h"
#include <sys/mips_addrspace.h>

/* offset of the first nvram location into the ds1687 part */
#define NVRAM_OFFSET    NVRAM_REG_OFFSET
#define RTC_FAIL	1	

/*
 * Name:      pon_xbow.s - walking 1 xbow test for IP30
 *	     This tests only the data path between the processor
 *	     and the XBOW ASIC.
 * Input:     none
 * Output:    returns if successfull, displays error msg if failed
 *
 */

LEAF(pon_xbow)
	move	RA3,ra                  # save return address

	/*
	 * test the data path to the XBOW by walking a 1 through the
	 * link 0xe upper arbitration register, which is a 32 bit
	 * Read/Writable register
	 */

		                        # 32 bit RW-able register on xbow
	LI	T34,PHYS_TO_COMPATK1(XBOW_BASE|XB_LINK_ARB_UPPER(0xe))
	j	test_common

	END(pon_xbow)

/*
 * Name:     pon_bridge.s - walking 1 bridge test for IP30
 *	     This tests only the data path between the processor
 *	     and the BRIDGE ASIC.
 * Input:     none
 * Output:    returns if successfull, displays error msg if failed
 *
 *
 */
LEAF(pon_bridge)
	move	RA3,ra                  # save return address
	/*
	 * test the data path to the BRIDGE by walking a 1 through the
	 * even devices read response buffer register, which is a 32
	 * bit Read/Writable register
	 */
		                        # 32 bit RW-able register on bridge
	LI	T34,PHYS_TO_COMPATK1(BRIDGE_BASE|BRIDGE_EVEN_RESP)
	j	test_common

	END(pon_bridge)

/*
 * pon_ioc3 - short test of data paths to ioc3, assuming
 *	      that the CPU-HEART-XBOW-BRIDGE path is functional.
 */

LEAF(pon_ioc3)
	move    RA3,ra                  # save return address

	/* test data path to IOC3 by walking a 1 through serial port ring
	 * buffer upper base register, which is a 32 bit r/w register.
	 */
	LI	T34,PHYS_TO_COMPATK1(IOC3_PCI_DEVIO_BASE+IOC3_SBBR_H)
	j	test_common

	END(pon_ioc3)


LEAF(test_common)

        li      a1,0x1                  # set-up bit 0

1:                                      # for each bit pattern
        sw      a1,0(T34)               # store word
        lw      a2,0(T34)               # load word
        bne     a1,a2,2f                # call subroutine if data is bad

        sll     a1,1                    # shift logically to left
        bne     a1,zero,1b              # loop back to test the next bit

        sw      zero,0(T34)             # restore the reset value
        j       RA3                     # return (ra contains the ret address)

        /* Just wing it to try and get some message out.  Soft power is
         * not enabled yet, so just spin.  pon_initio uses T0x so it should
         * keep our syndrome.
         */
2:                                      # if an error msg needs to be displayed
        jal     pon_initio
        move    a0,T34                  # store in a0 failed add
        jal     pon_memerr              # this subroutine displays an err msg:
                                        # a0 - bad address
                                        # a1 - data expected from read
                                        # a2 - actual data obtained
                                        # 
        jal     pon_flash_led           # never returns

	END(test_common)

/*
 * pon_rtc - walk a bit through one dallas ram regiser.  Needs
 * to be done faily early as init_env() is done before pon_more(),
 * but we may want to pull it out a bit.
 */
 
LEAF(pon_rtc)
        move    RA2,ra
        move    T22,zero
        .set noreorder
        /* NOTE: T30 is used because all T20,T21,T22,T23 are already used */
        move    T30,fp                          # set new exception handler
        LA      fp,regs_hndlr
        .set reorder

        /* goto slow dallas mode */
        LI      T20,PHYS_TO_COMPATK1(IOC3_PCI_DEVIO_BASE+IOC3_SIO_CR)
        lw      T21,0(T20)                      # read value
        and     T33,T21,~SIO_SR_CMD_PULSE       # tweak it
        or      T33,0x8 << SIO_CR_CMD_PULSE_SHIFT;
        sw      T33,0(T20)                      # write back
        lw      zero,0(T20)                     # flushbus
        lw      zero,0(T20)                     # flushbus
        LI      T20,RTC_ADDR_PORT               # clock address
        LI      T21,RTC_DATA_PORT               # clock data
        li      T23,RTC_REG_NUM(RTC_CTRL_A)     # select ctrl A
        sb      T23,0(T20)
        li      T23,RTC_OSCILLATOR_ON		# bank 0
        sb      T23,0(T21)
        /* now walk a bit from the top down... */
        li      T34,0x80                        # walk a byte down to 0
1:      li      T31,RTC_REG_NUM(NVRAM_OFFSET)   # select nvram regiser
        sb      T31,0(T20)
        sb      T34,0(T21)                      # write new val
        li      T31,RTC_REG_NUM(NVRAM_OFFSET)   # select nvram regiser
        sb      T31,0(T20)
        lbu     T31,0(T21)
        beq     T31,T34,2f
        ori     T22,RTC_FAIL                    # diag failed
        b       3f                              # exit loop
2:      srl     T34,1                           # next bit.
        bnez    T34,1b
3:      LI      T20,PHYS_TO_COMPATK1(IOC3_PCI_DEVIO_BASE+IOC3_SIO_CR)
        sw      T33,0(T20)                      # restore setting.

        /* tests done, print messages for any errant paths
         */
        beqz    T22,regs_done
        LA      a0,reg_msg
        jal     pon_puts
        LA      a0,failed
        jal     pon_puts
	LA	a0,rtcfail_msg
	jal	pon_puts

        jal     pon_flash_led                   # spin and poll power switch

regs_done:
        .set noreorder                          # restore old exception hndlr
        move    fp,T30                          # similar to IP22's
        .set reorder
        move    ra,RA2
        j       ra

	END(pon_rtc)

LEAF(regs_hndlr)
        LA      a0,reg_msg
        jal     pon_puts
        LA      a0,reg_fail_exc_msg
        jal     pon_puts
        j       T30
        /* exception this early never returns */
        END(regs_hndlr)

        .data
rtcfail_msg:
        .asciiz "\tRTC path test \t\t           *FAILED*\r\n"
reg_msg:
        .asciiz "\r\nData path test\t\t\t    "
reg_fail_exc_msg:
        .asciiz "       *FAILED* with EXCEPTION:\r\n"
failed:
        .asciiz "       *FAILED*\r\n"

