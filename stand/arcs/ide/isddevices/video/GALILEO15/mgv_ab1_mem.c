/*
**               Copyright (C) 1989, Silicon Graphics, Inc.              
**                                                                      
**  These coded instructions, statements, and computer programs  contain 
**  unpublished  proprietary  information of Silicon Graphics, Inc., and 
**  are protected by Federal copyright law.  They  may  not be disclosed  
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc. 
*/

/*
 *      FileName:       mgv_ab1_mem.c
 *      $Revision: 1.3 $
 */

#include "mgv_diag.h"

/*
 * Forward References for Functions
 */
__uint32_t	mgv_AB1DramAddrBusTest(__uint32_t argc, char **argv);
__uint32_t	mgv_AB1DramDataBusTest(__uint32_t argc, char **argv);
__uint32_t	mgv_AB1DramPatrnTest(__uint32_t argc, char **argv);
__uint32_t	mgv_AB1DramPatrnSlowTest(__uint32_t argc, char **argv);
__uint32_t	_mgv_AB1GetArgs(__uint32_t argc, char **argv, char *r, \
			char *g, char *b, char *a);
void	_mgv_getChannelString(__uint32_t chanNum, char *chanStr);
__uint32_t	_mgv_AB1DramAddrBusTest(__uint32_t channel);
__uint32_t	_mgv_AB1DramDataBusTest(__uint32_t channel);
__uint32_t	_mgv_AB1DramPatrnTest(__uint32_t channel);
__uint32_t	_mgv_AB1DramPatrnSlowTest(__uint32_t channel);

/*
 * NAME: mgv_AB1DramAddrBusTest
 *
 * FUNCTION: AB1 DRAM Address Bus Tests (Top Level Function)
 *
 * INPUTS: -r -g -b -a
 *
 * OUTPUTS: error if any
 */
__uint32_t
mgv_AB1DramAddrBusTest(__uint32_t argc, char **argv)
{
    __uint32_t	errors = 0x0;
    char	red, grn, blu, alp;

    if (_mgv_AB1GetArgs(argc, argv, &red, &grn, &blu, &alp)) {
	msg_printf(SUM,
	 "Usage: mgv_abaddrbus [-r] [-g] [-b] [-a]\n");
	errors++;
    } else {
   	if (red) errors += _mgv_AB1DramAddrBusTest(AB1_RED_CHAN);
   	if (grn) errors += _mgv_AB1DramAddrBusTest(AB1_GRN_CHAN);
   	if (blu) errors += _mgv_AB1DramAddrBusTest(AB1_BLU_CHAN);
   	if (alp) errors += _mgv_AB1DramAddrBusTest(AB1_ALP_CHAN);
    }
    return(errors);
}

/*
 * NAME: mgv_AB1DramDataBusTest
 *
 * FUNCTION: AB1 DRAM Data Bus Tests (Top Level Function)
 *
 * INPUTS: -r -g -b -a
 *
 * OUTPUTS: error if any
 */
__uint32_t
mgv_AB1DramDataBusTest(__uint32_t argc, char **argv)
{
    __uint32_t	errors = 0x0;
    char	red, grn, blu, alp;

    if (_mgv_AB1GetArgs(argc, argv, &red, &grn, &blu, &alp)) {
	msg_printf(SUM,
	 "Usage: mgv_abdatabus [-r] [-g] [-b] [-a]\n");
	errors++;
    } else {
   	if (red) errors += _mgv_AB1DramDataBusTest(AB1_RED_CHAN);
   	if (grn) errors += _mgv_AB1DramDataBusTest(AB1_GRN_CHAN);
   	if (blu) errors += _mgv_AB1DramDataBusTest(AB1_BLU_CHAN);
   	if (alp) errors += _mgv_AB1DramDataBusTest(AB1_ALP_CHAN);
    }
    return(errors);
}

/*
 * NAME: mgv_AB1DramPatrnTest
 *
 * FUNCTION: AB1 DRAM Pattern Tests (Top Level Function)
 *
 * INPUTS: -r -g -b -a
 *
 * OUTPUTS: error if any
 */
__uint32_t
mgv_AB1DramPatrnTest(__uint32_t argc, char **argv)
{
    __uint32_t	errors = 0x0;
    char	red, grn, blu, alp;

    if (_mgv_AB1GetArgs(argc, argv, &red, &grn, &blu, &alp)) {
	msg_printf(SUM,
	 "Usage: mgv_abpatrn [-r] [-g] [-b] [-a]\n");
	errors++;
    } else {
   	if (red) errors += _mgv_AB1DramPatrnTest(AB1_RED_CHAN);
   	if (grn) errors += _mgv_AB1DramPatrnTest(AB1_GRN_CHAN);
   	if (blu) errors += _mgv_AB1DramPatrnTest(AB1_BLU_CHAN);
   	if (alp) errors += _mgv_AB1DramPatrnTest(AB1_ALP_CHAN);
    }
    return(errors);
}

/*
 * NAME: mgv_AB1DramPatrnSlowTest
 *
 * FUNCTION: AB1 DRAM Pattern Slow Tests (Top Level Function)
 *
 * INPUTS: -r -g -b -a
 *
 * OUTPUTS: error if any
 */
__uint32_t
mgv_AB1DramPatrnSlowTest(__uint32_t argc, char **argv)
{
    __uint32_t	errors = 0x0;
    char	red, grn, blu, alp;

    if (_mgv_AB1GetArgs(argc, argv, &red, &grn, &blu, &alp)) {
	msg_printf(SUM,
	 "Usage: mgv_abpatrnslow [-r] [-g] [-b] [-a]\n");
	errors++;
    } else {
   	if (red) errors += _mgv_AB1DramPatrnSlowTest(AB1_RED_CHAN);
   	if (grn) errors += _mgv_AB1DramPatrnSlowTest(AB1_GRN_CHAN);
   	if (blu) errors += _mgv_AB1DramPatrnSlowTest(AB1_BLU_CHAN);
   	if (alp) errors += _mgv_AB1DramPatrnSlowTest(AB1_ALP_CHAN);
    }
    return(errors);
}

/*
 * NAME: _mgv_AB1GetArgs
 *
 * FUNCTION: process input arguments
 *
 * INPUTS: argc, argv and pointer to r,g,b,a
 *
 * OUTPUTS: 0 if no error, 1 if error
 */
__uint32_t
_mgv_AB1GetArgs(__uint32_t argc, char **argv, char *r, char *g, char *b, \
		char *a)
{
    __uint32_t	badarg = 0x0, inp = 0;

    /* assume nothing is selected */
    *r = *g = *b = *a = 0x0;

    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
		case 'r':
			*r = 0x1;
			inp++;
			break;
		case 'g':
			*g = 0x1;
			inp++;
			break;
		case 'b':
			*b = 0x1;
			inp++;
			break;
		case 'a':
			*a = 0x1;
			inp++;
			break;
		default: badarg++; break;
	}
	argc--; argv++;
    }

    if ( badarg || argc || !inp) {
	 return(1);
    } else {
	 return(0);
    }
}

/*
 * NAME: _mgv_getChannelString
 *
 * FUNCTION: gets the channel string from channel number
 *
 * INPUTS: channel -> R, G, B or A channel
 *
 * OUTPUTS: channel string
 */
void
_mgv_getChannelString(__uint32_t chanNum, char *chanStr)
{
    if (chanNum == AB1_RED_CHAN) {
	sprintf(chanStr, "Red Channel");
    } else if (chanNum == AB1_GRN_CHAN) {
	sprintf(chanStr, "Green Channel");
    } else if (chanNum == AB1_BLU_CHAN) {
	sprintf(chanStr, "Blue Channel");
    } else if (chanNum == AB1_ALP_CHAN) {
	sprintf(chanStr, "Alpha Channel");
    }
}

/*
 * NAME: _mgv_AB1DramAddrBusTest
 *
 * FUNCTION: This routine tests the specified AB1 DRAM address
 *           bus.  The data written is an incrementing data
 *           pattern.
 *
 * INPUTS: channel -> R, G, B or A channel
 *
 * OUTPUTS: errors
 */
__uint32_t
_mgv_AB1DramAddrBusTest(__uint32_t channel)
{
    __uint32_t i, loop, row, column, rcv;
    __uint32_t index, addr_range;
    __uint32_t *dataptr, databuf[16];
    __uint32_t trycnt, errCode;
    __uint32_t errors = 0;
    char RGBChannel[20];

    /* Fetch the ascii string for the channel being tested */
    _mgv_getChannelString(channel, RGBChannel);

    /* select the error code */
    if (channel == AB1_RED_CHAN) {
	errCode = AB1_DRAM_ADDR_BUS_RED_CHAN_TEST;
    } else if (channel == AB1_GRN_CHAN) {
	errCode = AB1_DRAM_ADDR_BUS_GRN_CHAN_TEST;
    } else if (channel == AB1_BLU_CHAN) {
	errCode = AB1_DRAM_ADDR_BUS_BLU_CHAN_TEST;
    } else if (channel == AB1_ALP_CHAN) {
	errCode = AB1_DRAM_ADDR_BUS_ALP_CHAN_TEST;
    }

    /* Print message as to what test is being executed */
    msg_printf(SUM, "IMPV:- %s\n", mgv_err[errCode].TestStr);

    /* Copy data over to different buffer */
    bcopy((char*)mgv_ab1_basebuf, (char*)databuf, sizeof(mgv_ab1_basebuf));
    dataptr = &databuf[0];

    /* Select the desired AB1 channel, also diagnostic mode  */
    AB_W1 (mgvbase, AB_SYSCON, channel);

    /* Write the data to the selected AB1 DRAM */
    for (addr_range = 0x0000; addr_range < AB1_ADDR_SIZE;) {
	/* Every 4096 bytes, print a debug message */
	if ((addr_range & 0xFFF) == 0) {
		busy (1);
		msg_printf(DBG, "\n%s AB1 DRAM Address Bus Test Writing \
			%08X Address region\n\n" , RGBChannel, addr_range);
	}
	/*
	 * Each one of the following loops, writes 4
	 * words to AB1 memory
	 */
	for (loop = 0; loop < 0x4; loop++, addr_range++) {
	    /* Set the addres in the registers */
	    row = addr_range >> 7;
	    column = addr_range & 0x7F;

	    /* (8 LSBs of row address ) */
	    AB_W1(mgvbase, AB_HIGH_ADDR, row >> 1);
	    /* MSB of row, 7 Bits of column */
	    AB_W1(mgvbase, AB_LOW_ADDR, (((row & 0x1) << 7) | 
					(column & 0x7F)));

	    /* Write to the AB1 external dRam, 4 times */
	    for (i = 0; i < 0x4; i++) {
		AB_W4(mgvbase, AB_EXT_MEM, *dataptr);
		msg_printf(DBG, "Address = 0x%08X data sent = 0x%08X\n", 
				addr_range, *dataptr);
		dataptr++;
	    }
	}

	/* Increment the data values and write some more */
	dataptr = &databuf[0];
	for (index = 0; index < 16; index++) {
	    *(dataptr + index) += 0x00010001;
	}
    }

    /* Wait here for a second, for some reason */
    busy(1);

    /* ReadBack and verify */
    msg_printf(DBG, "%s AB1 DRAM Address Bus Test Readback & Verify\n", 
						RGBChannel);

    /* Reinitialize data buffer */
    bcopy((char*)mgv_ab1_basebuf, (char*)databuf,sizeof(mgv_ab1_basebuf));
    dataptr = &databuf[0];

    /* Read back the AB1 DRAM and verify */
    for (addr_range = 0x0000; addr_range < AB1_ADDR_SIZE;) {
	/* Every 4096 bytes, print a debug message */
	if ((addr_range & 0xFFF) == 0) {
	    busy (1);
	    msg_printf(DBG, "%s AB1 DRAM Address Bus Test Reading \
			%08X Address region\n" , RGBChannel, addr_range);
	}

	/*
         * Each one of the following loops, reads 4
         * words from AB1 memory
         */
	for (loop = 0; loop < 0x4; loop++, addr_range++) {

	    /* Setup the address */
	    row = addr_range >> 7;
	    column = addr_range & 0x7F;

	    /*
	     * HACK!!! try at least 5 times to read the 1st word
	     * This is trying to fix a crosstalk problem happened
	     * in read back from AB Dram.
	     */
	    for (trycnt = 0; trycnt < AB1_MAX_TRYCNT; trycnt++) {

	    	/* (8 LSBs of row address ) */
	    	AB_W1 (mgvbase, AB_HIGH_ADDR, row >> 1);
	    	/* MSB of row, 7 Bits of column */
	    	AB_W1 (mgvbase, AB_LOW_ADDR, 
			(((row & 0x1) << 7) | (column & 0x7F)));

	    	/* Read the first word and verify it */
	    	AB_R4 (mgvbase, AB_EXT_MEM, rcv);

	    	/*
	     	 * If the word is correct, break from
	     	 * the current for loop and continue with
	    	 * the other three words
	    	 */
	    	if (rcv == *dataptr) break;
	    }

	    /* If retries were exhausted, the test failed */
	    if (trycnt == AB1_MAX_TRYCNT) {
	        errors++;
		msg_printf(ERR, "Maximum Retry Time Out\n");
		msg_printf(DBG, "\tFailed Addrs = 0x%08X, Expected 0x%x, \
			Actual 0x%x\n", addr_range, *dataptr, rcv);
		msg_printf(DBG, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
	    }

	    /* Proceed to next valid data */
	    ++dataptr;

	    /* Read and verify the next three words */
	    for (i = 1; i < 4; i++) {

		/* Perform the read */
		AB_R4 (mgvbase, AB_EXT_MEM, rcv);

		/* If the data is not good, print error message */
		if (rcv != *dataptr) {
		    errors++;
		    msg_printf(ERR, "\tFailed Addrs = 0x%08X, Expected 0x%x, \
			Actual 0x%x\n", addr_range, *dataptr, rcv);
		    msg_printf(ERR, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
		} 

		/* Proceed to next valid data */
		++dataptr;
	    }
	}

	/*
	 * Update to next set of expected values 
	 * and read some more
	 */
	dataptr = &databuf[0];
	for (index = 0; index < 16; index++)
		*(dataptr + index) += 0x00010001;

    }
    busy(0);

    return(_mgv_reportPassOrFail((&mgv_err[errCode]) ,errors));
}

/*
 * NAME: _mgv_AB1DramDataBusTest
 *
 * FUNCTION: This routine tests the DRAM databus of the 
 *           specified channel using a walking ones and zeros.
 *
 * INPUTS: channel -> R, G, B or A channel
 *
 * OUTPUTS: errors
 */
__uint32_t
_mgv_AB1DramDataBusTest(__uint32_t channel)
{

    __uint32_t i, *dataptr, rcv;
    __uint32_t row, column, addr_range;
    __uint32_t loop = 0;
    __uint32_t trycnt, errCode;
    __uint32_t errors = 0;
    char RGBChannel[20];

    /* Fetch the ascii string for the channel being tested */
    _mgv_getChannelString(channel, RGBChannel);

    /* select the error code */
    if (channel == AB1_RED_CHAN) {
	errCode = AB1_DRAM_DATA_BUS_RED_CHAN_TEST;
    } else if (channel == AB1_GRN_CHAN) {
	errCode = AB1_DRAM_DATA_BUS_GRN_CHAN_TEST;
    } else if (channel == AB1_BLU_CHAN) {
	errCode = AB1_DRAM_DATA_BUS_BLU_CHAN_TEST;
    } else if (channel == AB1_ALP_CHAN) {
	errCode = AB1_DRAM_DATA_BUS_ALP_CHAN_TEST;
    }

    /* Print message as to what test is being executed */
    msg_printf(SUM, "IMPV:- %s\n", mgv_err[errCode].TestStr);

    dataptr = &mgv_walk_one[0];

    /* Select the desired AB1 channel, also diagnostic mode  */
    AB_W1 (mgvbase, AB_SYSCON, channel);

    /*
     * Each one of the following loops, writes 4
     * words to AB1 memory
     */
    dataptr = &mgv_walk_one[0];
    for (addr_range = 0; addr_range < 0x10; ++addr_range, loop += 4) {

	/* Set the address in the registers */
	row = addr_range >> 7;
	column = addr_range & 0x7F;

	/* (8 LSBs of row address ) */
	AB_W1(mgvbase, AB_HIGH_ADDR, row >> 1);
	/* MSB of row, 7 Bits of column */
	AB_W1(mgvbase, AB_LOW_ADDR, (((row & 0x1) << 7) | (column & 0x7F)));

	dataptr = &mgv_walk_one[loop & 0xF];

	/* Write to the AB1 external dRam  */
	for (i = 0; i < 0x4; i++) {
	    msg_printf(DBG, "%s AB1 DRAM Data Bus Test Address = 0x%08X \
		data sent = 0x%08X\n" ,RGBChannel, addr_range, *dataptr);
	    AB_W4(mgvbase, AB_EXT_MEM, *dataptr);
	    dataptr++;
	}
    }

    /* Wait here for a second */
    busy(1);
    loop = 0;

    /* ReadBack and verify */
    msg_printf(DBG, "%s AB1 DRAM Data Bus Test ReadBack&verify\n", RGBChannel);

    /* Read back the AB1 DRAM and verify */
    for (addr_range = 0; addr_range < 0x10; ++addr_range, loop += 4) {

	/* Setup the address */
        row = addr_range >> 7;
        column = addr_range & 0x7F;

	dataptr = &mgv_walk_one[loop & 0xF];

	/*
	 * HACK!!! try at least 5 times to read the 1st word 
	 * This is trying to fix a crosstalk problem happened
	 * in read back from AB Dram.
	 */
        for (trycnt = 0; trycnt < AB1_MAX_TRYCNT; trycnt++) {

            /* (8 LSBs of row address ) */
	    AB_W1 (mgvbase, AB_HIGH_ADDR, row >> 1);
	    /* MSB of row, 7 Bits of column */
	    AB_W1 (mgvbase, AB_LOW_ADDR, (((row & 0x1) << 7) | (column & 0x7F)));
	    AB_R4 (mgvbase, AB_EXT_MEM, rcv);
	    if (rcv == *dataptr) break;
        }

	/* If retries were exhausted, the test failed */
        if (trycnt == AB1_MAX_TRYCNT) {
	    errors++;
	    msg_printf(ERR, "Maximum Retry Time Out\n");
	    msg_printf(SUM, "If retries were exhausted, the test failed \n");
	    msg_printf(ERR, "Failed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
	    msg_printf(ERR, "Failed bits 0x%x\n", *dataptr ^ rcv);
        }

	dataptr++;

	/* Read and verify the next three words */
        for (i = 1; i < 4; i++) {

	    /* Perform the read */
	    AB_R4 (mgvbase, AB_EXT_MEM, rcv) ;

            msg_printf(DBG,"%s Ab1_Dram_DataBus_Test Addrs= 0x%x Exp=0x%x \
		Rcv=0x%x\n" ,RGBChannel, addr_range, *dataptr, rcv);

	    /* If the data is not good, print error message */
	    if (rcv != *dataptr) {
	    	errors++;
	    	msg_printf(ERR, "Failed Addrs = 0x%08X, Expected 0x%x, Actual 0x%x\n", addr_range, *dataptr, rcv);
	    	msg_printf(ERR, "Failed bits 0x%x\n", *dataptr ^ rcv);
	    } else {
	    	++dataptr;
	    }
	}
    }

    busy(0);

    return(_mgv_reportPassOrFail((&mgv_err[errCode]) ,errors));
}

/*
 * NAME: _mgv_AB1DramPatrnTest
 *
 * FUNCTION: This test performs a data pattern test on each
 *           memory cell.  
 *
 *           The test is looped three times to make sure that
 *           each memory cell can hold a 0 or a 1.
 *
 * INPUTS: channel -> channel to be tested
 *
 * OUTPUTS: errors
 */
__uint32_t
_mgv_AB1DramPatrnTest(__uint32_t channel)
{
    __uint32_t loop;
    __uint32_t i, rcv, *dataptr;
    __uint32_t row, column, addr_range;
    __uint32_t trycnt, errCode;
    __uint32_t errors = 0;
    char RGBChannel[20];

    /* Fetch the ascii string for the channel being tested */
    _mgv_getChannelString(channel, RGBChannel);

    /* select the error code */
    if (channel == AB1_RED_CHAN) {
	errCode = AB1_DRAM_PATRN_RED_CHAN_TEST;
    } else if (channel == AB1_GRN_CHAN) {
	errCode = AB1_DRAM_PATRN_GRN_CHAN_TEST;
    } else if (channel == AB1_BLU_CHAN) {
	errCode = AB1_DRAM_PATRN_BLU_CHAN_TEST;
    } else if (channel == AB1_ALP_CHAN) {
	errCode = AB1_DRAM_PATRN_ALP_CHAN_TEST;
    }

    /* Print message as to what test is being executed */
    msg_printf(SUM, "IMPV:- %s\n", mgv_err[errCode].TestStr);

    /* Select the desired AB1 channel, also diagnostic mode  */
    AB_W1 (mgvbase, AB_SYSCON, channel);

    /* Loop through the data array patterns */
    for (loop = 0; loop < 3; loop++) {
	/* Write this series of patterns into memory */
       	for (addr_range = 0; addr_range < AB1_ADDR_SIZE;) {
	    /* Every 4096 bytes, wait awhile */
	    if ((addr_range & 0xFFF) == 0) {
		busy (1);
		msg_printf(DBG, "%s AB1 Pattern Test  Writing 0x%08X \
			Addr region\n" , RGBChannel, addr_range);
	    }
	    /* Setup the next 16 byte address range */
            row = addr_range >> 7;
            column = addr_range & 0x7F;
	
	    /* (8 LSBs of row address ) */
            AB_W1(mgvbase, AB_HIGH_ADDR, row >> 1); 
	    /* MSB of row, 7 Bits of column */
            AB_W1(mgvbase, AB_LOW_ADDR, (((row & 0x1) << 7) | (column & 0x7F)));
	
	    /* Reset the data ptr */
       	    dataptr = &mgv_patrn[loop];

            /* Write to the AB1 external dRam  */
            for (i = 0; i < 0x4; i++) {
		msg_printf(DBG, "Address = 0x%08X data sent = 0x%08X\n", 
				addr_range, *dataptr);
                AB_W4(mgvbase, AB_EXT_MEM, *dataptr);
		dataptr++;
	    }

	    /* Increment the address range */
	    ++addr_range;
        }

	busy(1);

        /* ReadBack and verify */
	msg_printf(DBG, "%s AB1 Pattern Test Readback & Verify\n", RGBChannel);

	/* Initialize the starting address and read and verify memory */
        for (addr_range = 0; addr_range < AB1_ADDR_SIZE;) {

	    /* After every 4096 bytes, wait awhile */
	    if ((addr_range & 0xFFF) == 0) {
		busy (1);
		msg_printf(DBG, "%s AB1 Pattern Test  Reading 0x%08X Address \
			region\n" , RGBChannel, addr_range);
	    }

	    /* Init the address */
            row = (addr_range) >> 7;
            column = (addr_range) & 0x7F;

	    /* Setup the data pattern */
	    dataptr = &mgv_patrn[loop];  

	    /*
	     * HACK!!! try at least 5 times to read the 1st word 
	     * This is trying to fix a crosstalk problem happened
	     * in read back from AB Dram.
	     */
            for (trycnt = 0; trycnt < AB1_MAX_TRYCNT; trycnt++) {
             	/* (8 LSBs of row address ) */
                AB_W1(mgvbase, AB_HIGH_ADDR, row >> 1);
                /* MSB of row, 7 Bits of column */
                AB_W1(mgvbase, AB_LOW_ADDR, (((row & 0x1) << 7) | (column & 0x7F)));
                AB_R4(mgvbase, AB_EXT_MEM, rcv);
                if (rcv == *dataptr) break;
            }
            if (trycnt == AB1_MAX_TRYCNT) {
		errors++;
	    	msg_printf(ERR, "Maximum Retry Time Out\n");
		msg_printf(DBG, "\tFailed Addrs = 0x%08X, Expected 0x%x, \
			Actual 0x%x\n", addr_range, *dataptr, rcv);
		msg_printf(DBG, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
	    }
            ++dataptr;

	    /* Verify the rest of the 16 bytes */
            for (i = 1; i < 4; i++) {
            	AB_R4(mgvbase, AB_EXT_MEM, rcv) ;
		if (rcv != *dataptr) {
		    errors++;
		    msg_printf(ERR, "\tFailed Addrs = 0x%08X, Expected 0x%x, \
			Actual 0x%x\n", addr_range, *dataptr, rcv);
		    msg_printf(ERR, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
		} 
                ++dataptr;
            }
	    ++addr_range;
       	}
    }
    busy (0);

    return(_mgv_reportPassOrFail((&mgv_err[errCode]) ,errors));
}

/*
 * NAME: _mgv_AB1DramPatrnSlowTest
 *
 * FUNCTION: This test performs a data pattern test on each
 *           memory cell.  
 *
 *           The test is looped three times to make sure that
 *           each memory cell can hold a 0 or a 1.
 *           The difference between this test and the previous one
 *           is this test writes 16 bytes and reads them back.  The
 *           previous one writes the whole memory then reads them
 *           back.
 *
 * INPUTS: channel -> channel to be tested
 *
 * OUTPUTS: errors
 */
__uint32_t
_mgv_AB1DramPatrnSlowTest(__uint32_t channel)
{
    __uint32_t loop;
    __uint32_t i, rcv, *dataptr;
    __uint32_t row, column, addr_range;
    __uint32_t trycnt, errCode;
    __uint32_t errors = 0;
    char RGBChannel[20];

    /* Fetch the ascii string for the channel being tested */
    _mgv_getChannelString(channel, RGBChannel);

    /* select the error code */
    if (channel == AB1_RED_CHAN) {
	errCode = AB1_DRAM_PATRN_SLOW_RED_CHAN_TEST;
    } else if (channel == AB1_GRN_CHAN) {
	errCode = AB1_DRAM_PATRN_SLOW_GRN_CHAN_TEST;
    } else if (channel == AB1_BLU_CHAN) {
	errCode = AB1_DRAM_PATRN_SLOW_BLU_CHAN_TEST;
    } else if (channel == AB1_ALP_CHAN) {
	errCode = AB1_DRAM_PATRN_SLOW_ALP_CHAN_TEST;
    }
    /* Print message as to what test is being executed */
    msg_printf(SUM, "IMPV:- %s\n", mgv_err[errCode].TestStr);

    /* Select the desired AB1 channel, also diagnostic mode  */
    AB_W1 (mgvbase, AB_SYSCON, channel);

    /* Loop through all the data patterns in the array */
    for (loop = 0; loop < 3; loop++) {
	/* Write this series of patterns into memory */
        for (addr_range = 0; addr_range < AB1_ADDR_SIZE;) {
	    /* Every 4096 bytes, wait awhile */
	    if ((addr_range & 0xFFF) == 0) {
		busy (1);
		msg_printf(DBG, "%s AB1 Section Pattern Test  Writing 0x%08X \
			Addr region\n" , RGBChannel, addr_range);
	    }

	    /* Setup the next address */
            row = addr_range >> 7;
            column = addr_range & 0x7F;
	
	    /* (8 LSBs of row address ) */
            AB_W1 (mgvbase, AB_HIGH_ADDR, row >> 1); 
	    /* MSB of row, 7 Bits of column */
            AB_W1 (mgvbase, AB_LOW_ADDR, (((row & 0x1) << 7) | (column & 0x7F)));
	
	    /* Reset the data ptr */
       	    dataptr = &mgv_patrn[loop];

            /* Write to the AB1 external dRam  */
            for (i = 0; i < 0x4; i++) {
		msg_printf(DBG, "Address = 0x%08X data sent = 0x%08X\n", 
				addr_range, *dataptr);
                AB_W4 (mgvbase, AB_EXT_MEM, *dataptr);
		dataptr++;
	    }

	    busy(1);

	    /* ReadBack and verify */
	    msg_printf(DBG, "%s AB1 Section Pattern Test Readback & Verify\n", \
			RGBChannel);
	    /* Reset the data ptr */
	    dataptr = &mgv_patrn[loop];  

	    /*
	     * HACK!!! Try at least 5 time to read 1st word
	     * This is trying to fix a crosstalk problem happened
	     * in read back from AB Dram.
	    */
            /* ReadBack and verify */
            for (trycnt = 0; trycnt < AB1_MAX_TRYCNT; trycnt++) {
            	/* (8 LSBs of row address ) */
            	AB_W1 (mgvbase, AB_HIGH_ADDR, row >> 1);
           	/* MSB of row, 7 Bits of column */
           	AB_W1(mgvbase, AB_LOW_ADDR, (((row & 0x1) << 7) | (column & 0x7F)));
           	AB_R4 (mgvbase, AB_EXT_MEM, rcv);
           	if (rcv == *dataptr) break;
            }
            if (trycnt == AB1_MAX_TRYCNT) {
	    	msg_printf(ERR, "Maximum Retry Time Out\n");
		errors++;
        	msg_printf(DBG, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
            }
            ++dataptr;

	    /* Read and verify the rest of the 16 byte group */
            for (i = 1; i < 4; i++) {
                AB_R4(mgvbase, AB_EXT_MEM, rcv) ;
		if (rcv != *dataptr) {
		    errors++;
		    msg_printf(ERR, "\tFailed Addrs = 0x%08X, Expected 0x%x, \
			Actual 0x%x\n", addr_range, *dataptr, rcv);
		    msg_printf(ERR, "\tFailed bits 0x%x\n", *dataptr ^ rcv);
		}
                ++dataptr;
            }
	    ++addr_range;
        }
    }
    busy (0);

    return(_mgv_reportPassOrFail((&mgv_err[errCode]) ,errors));
}

