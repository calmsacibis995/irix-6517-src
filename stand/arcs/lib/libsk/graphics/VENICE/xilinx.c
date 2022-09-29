/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************
 *
 * Module: usr/src/gfx/common/lib/libxilinx/xilinx.c
 *
 * $Revision: 1.1 $
 *
 * Description:
 *      Functions for loading/reading back xilinx LCA parts.  Requires that 
 *	the caller provide device specific functions (passed in as function
 *	pointer arguments) for wiggling & reading xilinx bits.
 *	
 *	NO file operations or printf calls are made from within this module,
 *	to prevent flash prom bloat from occuring.
 *
 **************************************************************************/

#include "xilinx.h"
#ifndef DEBUG
#define xprintf (void)
#else
#define xprintf ttyprintf
extern void ttyprintf(char *fmt, ...);
#endif

extern void sginap(int ticks);


/*
 * The xilinx configuration info fields are as follows:
 *
 * #frames, #bits/field, #bytes, #bits
 *
 * We compute byte count from the bit count and round up to the next byte
 * boundary.  The 3000 bit counts are 8 bits larger than those in the book,
 * because the XACT tool generates four extra dummy bits that show up in the
 * MCS file header's 24 bit length count.  We have to request 8 additional bits
 * because Intel MCS Hex format requires stuff to be encoded on byte boundaries.
 *
 * Similarly, the XC4005 only has 94998 bits in its config file, but the MCS
 * file encodes 95000 (to get us to a byte boundary).
 *
 * Technically, we should be able to only shift out the number of bits 
 * specified as the "24 bit length count" in the config header, but it's easier
 * to read the entire MCS file before looking at the header's bit length.  And,
 * it never hurts to shift out those extra bits (as long as there are no other
 * serially slaved xiliii).
 *
 */
config_info_t xilinx_info[] = {
    {197,        75,     1854,   14827 },       /* XC3020 */
    {241,        92,     2778,   22224 },       /* XC3030 */
    {285,       108,     3854,   30832 },       /* XC3042 */
    {329,       140,     5764,   46112 },       /* XC3064 */
    {373,       172,     8026,   64208 },       /* XC3090 */
    {356,       106,     4723,   37784 },       /* XC4002 */
    {428,       126,     6747,   53976 },       /* XC4003 */
    {500,       146,     9131,   73048 },       /* XC4004 */
    {572,       166,    11875,   95000 },       /* XC4005 */
    {644,       186,    14979,  119832 },       /* XC4006 */
    {716,       206,    18443,  147544 },       /* XC4008 */
    {788,       226,    22267,  178136 },       /* XC4010 */
};

/*
 * Function:	load_3000_serial()
 * 
 * Returns: 	
 * 
 *	XILINX_OKAY (1) 	if the part loaded successfully,
 *	XILINX_CONFIG_CLEAR_ERR	if the config mem clear failed,
 *	XILINX_NOT_DONE		if the DONE bit wasn't set.
 *
 * (Note that all HW specific functions are passed in as function parameters).
 *
 * Also note that for DG2, none of the status pins (INIT, DONE/PROG) are wired
 * up, so that we just have to blindly load the chip & hope for the best.  The
 * function pointers for the unconnected pins are no-ops.
 *
 * For 3000 series parts, we perform the following algorithm:
 *
 * 1. Set RESET pin low.
 *
 * 2. Force a high -> low transition on the DONE/PROG pin, then bring it back
 *    high again.
 *
 * 3. Set RESET pin high.
 *
 * 4. Wait for the part to clear configuration memory (you can either
 *    poll the INIT pin and wait for it to go high, or wait the maximum
 *    time for the largest xilinx part to clear itself, ~1msec.
 *
 * 5. Clock in all the bits from the config file (for each data bit, latch it
 *    to the register holding data for the serial data input pin, then force a 
 *    low -> high transition on the CCLK pin).
 *
 * 6. When all the bits have been shifted in, check DONE/PROG status; it should
 *    be HIGH.
 */

load_3000_serial(
    int (*write_reset_pin)(int val),
    int (*write_doneProg_pin)(int val),
    int (*wait_config_mem_clear)(void),
    int (*clock_dataBit_in)(int val),
    int (*check_doneProg_status)(void),
    int xilinxType,
    unsigned char *buffer)
{
    int size;
    unsigned char *pBuf;
/*    unsigned int mask; */    /* long */
    unsigned long mask;    

    pBuf = buffer;

    (*write_reset_pin)(0);

    (*write_doneProg_pin)(1);
    (*write_doneProg_pin)(0);
    (*write_doneProg_pin)(1);

    (*write_reset_pin)(1);
    if ((*wait_config_mem_clear)() != XILINX_OKAY) {
xprintf("load_3000_serial returns error XILINX_CONFIG_CLEAR_ERR\n");
	return (XILINX_CONFIG_CLEAR_ERR);
    }

    /*
     * Now serially shift all those bits in.
     */
    size = xilinx_info[xilinxType].program_data_size_bytes;
    for (; size > 0; size--, pBuf++) {
	for (mask = 1; mask < 0x100; mask <<= 1) {
	    (*clock_dataBit_in)(((*pBuf & mask) ? 1 : 0));
	}
    }
    /*
     * Xilinx's PC based download tool by default sends an additional 25 clocks
     * of high data after all the config data have been clocked in.  So, we'll 
     * add a bunch of additional data clock cycles here too.  This is absolutely
     * required for the 4000 series parts.
     */
    for (size = 0; size < 25; size++) {
        (*clock_dataBit_in)(1);
    }
    if ((*check_doneProg_status)() == 0) {
xprintf("load_3000_serial returns error XILINX_LOAD_NOT_DONE\n");
	return (XILINX_LOAD_NOT_DONE);
    } else {
	return (XILINX_OKAY);
    }
}

load_3000_serial_byte(
    int (*write_reset_pin)(int val),
    int (*write_doneProg_pin)(int val),
    int (*wait_config_mem_clear)(void),
    int (*clock_dataByte_in)(int val),
    int (*check_doneProg_status)(void),
    int xilinxType,
    unsigned char *buffer)
{
    int size;
    int i;

    (*write_reset_pin)(0);

    (*write_doneProg_pin)(1);
    (*write_doneProg_pin)(0);
    (*write_doneProg_pin)(1);

    (*write_reset_pin)(1);
    if ((*wait_config_mem_clear)() != XILINX_OKAY) {
xprintf("load_3000_serial_byte returns XILINX_CONFIG_CLEAR_ERR\n");
	return (XILINX_CONFIG_CLEAR_ERR);
    }

    /*
     * Now serially shift all those bits in.
     */
    size = xilinx_info[xilinxType].program_data_size_bytes;
    for (i = 0; i < size; i++) {
	(*clock_dataByte_in)(buffer[i]/*, 8*/);
    }
    /*
     * Xilinx's PC based download tool by default sends an additional 25 clocks
     * of high data after all the config data have been clocked in.  So, we'll 
     * add a bunch of additional data clock cycles here too.  This is absolutely
     * required for the 4000 series parts.
     */
    for (size = 0; size < 4; size++) {
        (*clock_dataByte_in)(0xff/*, 8*/);
    }
xprintf("load_3000_serial_byte finished both loops\n");
    if ((*check_doneProg_status)() == 0) {
xprintf("load_3000_serial_byte returns XILINX_LOAD_NOT_DONE\n");
	return (XILINX_LOAD_NOT_DONE);
    } else {
xprintf("load_3000_serial_byte returns XILINX_OKAY\n");
	return (XILINX_OKAY);
    }
}

/*
 * Function:	 load_4000_serial()
 * 
 * Returns: 	
 *
 *	XILINX_OKAY (1) 	if the part loaded successfully,
 *	XILINX_CONFIG_CLEAR_ERR	if the config mem clear failed,
 *	XILINX_FRAME_ERR	if an individual config frame load failed,
 *	XILINX_NOT_DONE		if the DONE bit wasn't set.
 *
 * (Note that all HW specific functions are passed in as function parameters).
 *
 * Also note that for DG2, none of the status pins (INIT, DONE/PROG) are wired
 * up, so that we just have to blindly load the chip & hope for the best.  The
 * function pointers for the unconnected pins are no-ops.  The code is written
 * such that if these status pins ever get wired up, you could supply real
 * functions and it ought to work.
 *
 * For 4000 series parts, we perform the following algorithm:
 *
 * 1. Force a high -> low transition on the PROGRAM pin, then bring it high
 *    again.
 *
 * 2. Wait for the part to clear configuration memory (you can either
 *    poll the INIT pin and wait for it to go high, or wait the maximum
 *    time for the largest xilinx part to clear itself, ~1msec.
 *
 * 3. For (i = 0; i < # frames; i ++)
 *        Clock in one frame's worth of bits from the config file:
 *	    (for each data bit, latch it to the register holding data 
 *	    for the serial data input pin, then force a low -> high 
 *	    transition on the CCLK pin).
 *	   Check the INIT pin's status; if it's low, return a framing error.
 *
 * 4. Clock in any remaining bits (the postamble).
 *
 * 5. When all the bits have been shifted in, check DONE status; it should
 *    be HIGH.
 */

load_4000_serial(
    int (*write_Program_pin)(int val),
    int (*wait_config_mem_clear)(void),
    int (*clock_dataBit_in)(int val),
    int (*check_frame_status)(void),
    int (*check_done_status)(void),
    int xilinxType,
    unsigned char *buffer)
{
    int size, i, j;
    unsigned char *pBuf;
/*    unsigned int mask; */     /* long */
    unsigned long mask;     /* long */

    pBuf = buffer;

    (*write_Program_pin)(1);
    (*write_Program_pin)(0);
    (*write_Program_pin)(1);

    if ((*wait_config_mem_clear)() != XILINX_OKAY) {
xprintf("load_4000_serial returns error XILINX_CONFIG_CLEAR_ERR\n");
	return (XILINX_CONFIG_CLEAR_ERR);
    }

    /*
     * Shift in the header, then load individual data frames.  Header is
     * 40 bits == 5 bytes.
     */
    size = xilinx_info[xilinxType].program_data_size_bytes;
    for (i = 0; i < 5; i++, size--, pBuf++) {
	for (mask = 1; mask < 0x100; mask <<= 1) {
	    (*clock_dataBit_in)(((*pBuf & mask) ? 1 : 0));
	}
    }
    mask = 1;
    for (i = 0; i < xilinx_info[xilinxType].frames; i++) {
	for (j = 0; j < xilinx_info[xilinxType].bits_per_frame; j++) {
	    (*clock_dataBit_in)(((*pBuf & mask) ? 1 : 0));
	    mask <<= 1;
	    if (mask == 0x100) {
		mask = 1;
		pBuf++;
		size--;
	    }
	}
	if ((*check_frame_status)() == 0) {
xprintf("load_4000_serial returns error XILINX_FRAME_ERR\n");
	    return(XILINX_FRAME_ERR);
	}
    }
    /*
     * Finally, serially shift in the remaining postamble bits.  Get to a
     * byte boundary first.
     */
    while (mask < 0x100) {
	(*clock_dataBit_in)(((*pBuf & mask) ? 1 : 0));
	mask <<= 1;
    }
    pBuf++;
    size--;
    for (; size > 0; size--, pBuf++) {
	for (mask = 1; mask < 0x100; mask <<= 1) {
	    (*clock_dataBit_in)(((*pBuf & mask) ? 1 : 0));
	}
    }
    /*
     * Xilinx's PC based download tool by default sends an additional 25 clocks
     * of high data after all the config data have been clocked in.  So, we'll 
     * add a bunch of additional data clock cycles here too.  This is absolutely
     * required for the 4000 series parts.
     */
    for (size = 0; size < 25; size++) {
        (*clock_dataBit_in)(1);
    }

    if ((*check_done_status)() == 0) {
xprintf("load_4000_serial returns error XILINX_LOAD_NOT_DONE\n");
	return (XILINX_LOAD_NOT_DONE);
    } else {
	return (XILINX_OKAY);
    }
}

/* ARGSUSED */
int
load_4000_serial_byte(
    int (*write_Program_pin)(int val),
    int (*wait_config_mem_clear)(void),
    int (*clock_dataByte_in)(int val, int count),
    int (*check_frame_status)(void),
    int (*check_done_status)(void),
    int xilinxType,
    unsigned char *buffer)
{
    int size, i;

    (*write_Program_pin)(1);
    (*write_Program_pin)(0);
    (*write_Program_pin)(1);

    if ((*wait_config_mem_clear)() != XILINX_OKAY) {
xprintf("load_4000_serial_byte returns error XILINX_CONFIG_CLEAR_ERR\n");
	return (XILINX_CONFIG_CLEAR_ERR);
    }

    /*
     * Shift in the header, then load individual data frames.  Header is
     * 40 bits == 5 bytes.
     */
    size = xilinx_info[xilinxType].program_data_size_bytes;
    for (i = 0; i < size; i++)
	(*clock_dataByte_in)(buffer[i], 8);

    /*
     * Xilinx's PC based download tool by default sends an additional 25 clocks
     * of high data after all the config data have been clocked in.  So, we'll 
     * add a bunch of additional data clock cycles here too.  This is absolutely
     * required for the 4000 series parts.
     */
    for (size = 0; size < 4; size++)
        (*clock_dataByte_in)(0xff, 8);

    if ((*check_done_status)() == 0) {
xprintf("load_4000_serial_byte returns error XILINX_LOAD_NOT_DONE\n");
	return (XILINX_LOAD_NOT_DONE);
    } else {
	return (XILINX_OKAY);
    }
}

/*
 * Function:	read_3000_serial()
 * 
 * Returns: 	
 * 
 *	XILINX_OKAY (1) 	if the part read back successfully.
 *
 * The 3000 series do not provide a 'readback in progess' status indicator,
 * so the only way we will know if the readback is done is by reading the
 * proper number of bits from the configuration memory.  If we don't read 
 * all the bits, the Xilinx will be hung in a funky readback state, and won't
 * perform any of its normal operational functions.
 *
 * (Note that all HW specific functions are passed in as function parameters).
 *
 * For 3000 series parts, we perform the following readback algorithm:
 *
 * 1. Force a high -> low transition on the M0/RTRIG (Read Trigger) pin.
 *
 * 2. Clock in all the bits from the config memory (each data bit is presented
 *    on the RDATA pin whenever a low -> high transition occurs on CCLK) into
 *    an array passed in by the caller.  Readback does not include the preamble,
 *    but we'll read a full configuration file's worth of bits anyway, and
 *    throw away the last 40 or so bits, because they'll be garbage.
 *
 *    The 'low' condition of the serial clock must be maintained for at least
 *    .5 microseconds.  It's the responsibility of the caller to guarantee this.
 */

int
read_3000_serial(
    int (*write_rtrigger_pin)(int val),
    int (*clock_dataBit_out)(void),
    unsigned char (*read_dataBit)(void),
    int xilinxType,
    unsigned char *buffer)
{
    int size, shiftCount;
    unsigned char *pBuf;

    pBuf = buffer;

    /*
     * Arm the readback.  The trigger signal must go high a minimum of 250 ns
     * before you can start clocking data out of the Xilinx.
     */
    (*write_rtrigger_pin)(0);
    (*write_rtrigger_pin)(1);
    sginap(2);

    /*
     * Serially shift all those configuration memory bits out.
     */
    size = xilinx_info[xilinxType].program_data_size_bytes;
    for (; size > 0; size--, pBuf++) {
	*pBuf = 0;
	for (shiftCount = 0; shiftCount < 8; shiftCount++) {
	    (*clock_dataBit_out)();
	    *pBuf |= ((*read_dataBit)() << shiftCount);
	}
    }
    return (XILINX_OKAY);
}

/*
 * Function:	read_4000_serial()
 * 
 * Returns: 	
 * 
 *	XILINX_OKAY 		if the part read back successfully.
 *	XILINX_RDBK_NOT_DONE 	if the read back failed.
 *
 * (Note that all HW specific functions are passed in as function parameters).
 *
 * For 4000 series parts, we perform the following readback algorithm:
 *
 * 1. Force a high -> low transition on the RDBK.TRIG (Read Trigger) pin.
 *
 * 2. Clock in all the bits from the config memory (each data bit is presented
 *    on the RDBK.DATA pin whenever a low -> high transition occurs on CCLK) 
 *    into an array passed in by the caller.  Readback does not include the 
 *    preamble, but we'll read a full configuration file's worth of bits anyway,
 *    and throw away the last 40 or so bits, because they'll be garbage.
 *
 *    The 'low' condition of the serial clock must be maintained for at least
 *    .5 microseconds.  It's the responsibility of the caller to guarantee this.
 */

int
read_4000_serial(
    int (*write_rtrigger_pin)(int val),
    int (*clock_dataBit_out)(void),
    unsigned char (*read_dataBit)(void),
    int (*check_read_status)(void),
    int xilinxType,
    unsigned char *buffer)
{
    int size, shiftCount;
    unsigned char *pBuf;

    pBuf = buffer;

    /*
     * Arm the readback.  The trigger signal must go high a minimum of 250 ns
     * before you can start clocking data out of the Xilinx.
     */
    (*write_rtrigger_pin)(1);
    (*write_rtrigger_pin)(0);
    /*
     * If readback abort was enabled, the write_rtrigger_pin() function would
     * have forced a high -> low -> high transition; the high -> low transition
     * causes the part to go into readback abort state.  The Xilinx manual says
     * "After an aborted readback, additional clocks (up-to-one readback clock
     * per configuration frame) may be required to re-initialize the control
     * logic".  Since we seem to have problems with readbacks (unless it is
     * the first readback after the configuration memory has been loaded), we 
     * will ALWAYS issue a bunch of clocks here.
     */
    for (size = 0; size < xilinx_info[xilinxType].frames; size++) {
	(*clock_dataBit_out)();
    }
    /*
     * Do it again (xilinx funkiness? 25 extra clocks?  voodoo?)
     */
    for (size = 0; size < xilinx_info[xilinxType].frames; size++) {
	(*clock_dataBit_out)();
    }
    (*write_rtrigger_pin)(1);
    sginap(2);

    /*
     * Serially shift all those configuration memory bits out.  The 4000 series
     * also shifts out a start bit (low) and an 11 bit crc signature before
     * the readback sequence is completed, so we'll ask for an additional two
     * bytes.
     */
    size = xilinx_info[xilinxType].program_data_size_bytes + 2;
    for (; size > 0; size--, pBuf++) {
	*pBuf = 0;
	for (shiftCount = 0; shiftCount < 8; shiftCount++) {
	    (*clock_dataBit_out)();
	    *pBuf |= ((*read_dataBit)() << shiftCount);
	}
    }
    /*
     * RDBK.RIP is an active low signal (goes low after all the configuration
     * memory bits have been clocked out).
     */
    if ((*check_read_status)() == 1) {
xprintf("read_4000_serial returns error XILINX_RDBK_NOT_DONE\n");
	return (XILINX_RDBK_NOT_DONE);
    } else {
	return (XILINX_OKAY);
    }
}
