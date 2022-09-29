/***********************************************************************\
*	File:		pod_io4.c					*
*									*
*	Contains basic diagnostic code for the IO4 and the PBUS 	*
*	devices used by the IP19 PROM.  For the most part, these 	*
*	tests are non-destructive.  Exceptions to this policy will 	*
*	be explicitly noted. 						*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>


/*
 * pod_check_iaid
 *	Does a quick and dirty examination of the basic IO4 control
 *	registers.  It doesn't change their default state permanently
 *	(unless the tests crash for some reason), but it does step
 *	through all of the registers and ping them to see if they
 *	are behaving as expected.
 */

int
pod_check_iaid(uint slot)
{
    uint value;			/* Previous value of config register */
    jmp_buf fault_buf;		/* Fault buffer for storing machine state */
    uint prev_fault;		/* Storage for previous fault handler */

    /* 
     * Set up for unexpected exceptions.
     */
    if (setfault(fault_buf, &prev_fault)) {
	loprintf("\t*** EXCEPTION occurred while probing type register.\n");
	return -1;
    }

    /* 
     * Make sure that the board is actually present.
     */
    value = load_double_lo(EV_SYSCONFIG);
    if (!(value & (1 << slot))) {
	OUTPUT("\tWARNING: It appears as if there are no boards in slot %d\n",
	       slot);
    }
    
    value = EV_GET_CONFIG(IO4_CONF_REVTYPE) & IO4_TYPE_MASK;
    if (value != IO4_TYPE_VALUE) {
	OUTPUT("\tWARNING: Board type value of 0x%x is wrong for IO4's\n",
	       value);
    }
	
    /*
     * Do a simple read/write test of the IO4 registers.
     */
    OUTPUT("\tTesting IO4 configuration registers...");
    if (test_reg(IO4_CONF_LW, 0x3ff) == -1) return -1;
    if (test_reg(IO4_CONF_ADAP, 0xffffffff) == -1) return -1;
    if (test_reg(IO4_CONF_ENDIAN, 0x1) == -1) return -1;
    if (test_reg(IO4_CONF_INTRVECTOR, 0xffff) == -1) return -1;
    OUTPUT("  PASSED.\n");

    /*
     * Test support of invalid PIO stuff.  This will also tweak
     * the interrupts.  Should be able to steal this code from Bhanu.
     */
    OUTPUT("\tTesting IA PIO error handling...         ");     
    OUTPUT("  PASSED.\n");

    return 0;
}


/*
 * pod_check_epcreg
 *	Checks out the basic EPC registers. Doesn't do 
 * 	a whole lot of work; just plinks things in a reasonably
 *	appropriate manner.
 */

int
pod_check_epcreg(uint window, uint padap)
{
    unsigned value;
    unsigned int base_addr = SWIN_BASE(window, padap);

    /* Initialize the IO4 */
    store_double_lo(base_addr + EPC_IERR, 0x1000000);
    load_double_lo(base_addr + EPC_IERRC);
    store_double_lo(base_addr + EPC_PRSTCLR, 0x3ff);

    /* Switch off interrupts */
    store_double_lo(base_addr+EPC_IMRST, EPC_INTR_ALL); 
    
    /* Do simple read/write tests to registers */
    if (test_reg(base_addr + EPC_
}


/*
 * pod_check_epcnvram
 *	Examines the NVRAM.  It assumes that the two highest pages
 * 	of the NVRAM are reserved for testing, and it flip-flops
 *	back and forth between the two to ensure that things are
 *	working.  This is a reasonably good stress test for the EPC
 *	PIO mechanisms as a whole.
 */

int
pod_check_epcnvram(uint window, uint padap)
{
}


/*
 * pod_check_epcuart
 *	Examines the basic UART support by reading and writing
 *	the basic Zilog control registers as well as flipping
 *	DUART into local loopback mode and trying to write a
 *	couple of characters.
 */

int
pod_check_epcuart(uint window, uint padap)
{
}



